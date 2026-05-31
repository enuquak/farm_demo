#include "game_connection.h"
#include "message_parser.h"
#include "internal_msg_ids.h"

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/event.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include <cstring>

#include "internal.pb.h"
#include "log_macros.h"

namespace farm {

GameConnection::GameConnection(struct event_base* base, const std::string& gate_id)
    : base_(base)
    , gate_id_(gate_id)
    , game_port_(0)
    , state_(GameConnState::DISCONNECTED)
    , bev_(nullptr)
    , heartbeat_timer_(nullptr)
    , reconnect_timer_(nullptr)
    , last_heartbeat_(0)
{
}

GameConnection::~GameConnection() {
    disconnect();
    stop_heartbeat();
    stop_reconnect();
}

bool GameConnection::connect(const std::string& ip, uint16_t port) {
    game_ip_ = ip;
    game_port_ = port;

    // 创建 socket
    evutil_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        SPDLOG_ERROR("[GameConnection]Failed to create socket");
        return false;
    }

    // 设置非阻塞
    evutil_make_socket_nonblocking(fd);

    // 创建 bufferevent
    bev_ = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev_) {
        SPDLOG_ERROR("[GameConnection]Failed to create bufferevent");
        evutil_closesocket(fd);
        return false;
    }

    // 设置回调
    bufferevent_setcb(bev_, on_read, nullptr, on_event, this);
    bufferevent_enable(bev_, EV_READ | EV_WRITE);

    // 连接到 Game Server
    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &sin.sin_addr);

    state_ = GameConnState::CONNECTING;

    if (bufferevent_socket_connect(bev_,
            reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin)) < 0) {
        SPDLOG_ERROR("[GameConnection]Failed to connect to {}:{}", ip, port);
        disconnect();
        return false;
    }

    SPDLOG_INFO("[GameConnection]Connecting to {}:{}", ip, port);
    return true;
}

void GameConnection::disconnect() {
    state_ = GameConnState::DISCONNECTED;
    if (bev_) {
        bufferevent_free(bev_);
        bev_ = nullptr;
    }
    read_buffer_.clear();
}

bool GameConnection::send(uint32_t msg_id, std::string_view payload) {
    return send(msg_id, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
}

bool GameConnection::send(uint32_t msg_id, const uint8_t* payload, size_t len) {
    if (!bev_ || state_ == GameConnState::DISCONNECTED) {
        return false;
    }

    auto packed = MessageParser::pack(msg_id, payload, len);
    bufferevent_write(bev_, packed.data(), packed.size());
    return true;
}

void GameConnection::start_heartbeat() {
    if (heartbeat_timer_) return;

    struct timeval tv;
    tv.tv_sec = 5;  // 5 秒间隔
    tv.tv_usec = 0;
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    evtimer_add(heartbeat_timer_, &tv);
}

void GameConnection::stop_heartbeat() {
    if (heartbeat_timer_) {
        event_free(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
}

void GameConnection::start_reconnect() {
    if (reconnect_timer_) return;

    struct timeval tv;
    tv.tv_sec = 5;  // 5 秒间隔
    tv.tv_usec = 0;
    reconnect_timer_ = event_new(base_, -1, EV_PERSIST, on_reconnect_timer, this);
    evtimer_add(reconnect_timer_, &tv);
    SPDLOG_INFO("[GameConnection]Reconnect timer started (5s interval)");
}

void GameConnection::stop_reconnect() {
    if (reconnect_timer_) {
        event_free(reconnect_timer_);
        reconnect_timer_ = nullptr;
    }
}

// ===========================================
// libevent 回调
// ===========================================

void GameConnection::on_read(struct bufferevent* bev, void* ctx) {
    auto* conn = static_cast<GameConnection*>(ctx);
    conn->handle_read();
}

void GameConnection::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* conn = static_cast<GameConnection*>(ctx);

    if (events & BEV_EVENT_CONNECTED) {
        conn->handle_connect_success();
    } else if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        conn->handle_disconnect();
    }
}

void GameConnection::on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* conn = static_cast<GameConnection*>(ctx);
    conn->send_heartbeat();
}

void GameConnection::on_reconnect_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* conn = static_cast<GameConnection*>(ctx);
    conn->try_reconnect();
}

// ===========================================
// 内部处理
// ===========================================

void GameConnection::handle_read() {
    struct evbuffer* input = bufferevent_get_input(bev_);

    // 读取所有可用数据到缓冲区
    char buf[4096];
    while (true) {
        ev_ssize_t n = evbuffer_remove(input, buf, sizeof(buf));
        if (n <= 0) break;
        read_buffer_.insert(read_buffer_.end(),
                            reinterpret_cast<const uint8_t*>(buf),
                            reinterpret_cast<const uint8_t*>(buf) + n);
    }

    // 尝试解析消息
    while (!read_buffer_.empty()) {
        ParsedMessage msg;
        size_t consumed = 0;
        if (MessageParser::try_parse(read_buffer_.data(), read_buffer_.size(), msg, consumed)) {
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + consumed);

            // 更新心跳时间
            last_heartbeat_ = std::time(nullptr);

            // 调用消息回调
            if (msg_callback_) {
                msg_callback_(msg.msg_id, msg.payload);
            }
        } else {
            break;  // 数据不完整
        }
    }
}

void GameConnection::handle_connect_success() {
    SPDLOG_INFO("[GameConnection]Connected to {}:{}", game_ip_, game_port_);

    // 发送身份标识
    send_identify();

    // 启动心跳
    start_heartbeat();

    // 停止重连定时器
    stop_reconnect();
}

void GameConnection::handle_disconnect() {
    SPDLOG_INFO("[GameConnection]Disconnected from {}:{}", game_ip_, game_port_);

    state_ = GameConnState::DISCONNECTED;
    if (bev_) {
        bufferevent_free(bev_);
        bev_ = nullptr;
    }
    read_buffer_.clear();

    // 停止心跳
    stop_heartbeat();

    // 启动重连
    start_reconnect();
}

void GameConnection::send_identify() {
    farm::GateIdentify identify;
    identify.set_gate_id(gate_id_);
    identify.set_address("127.0.0.1:8080");  // Gate 自身地址

    std::string payload;
    identify.SerializeToString(&payload);

    send(MSG_ID_GATE_IDENTIFY, payload);
    SPDLOG_INFO("[GameConnection]Sent GATE_IDENTIFY (gate_id={})", gate_id_);
}

void GameConnection::send_heartbeat() {
    if (state_ != GameConnState::IDENTIFIED) return;

    farm::InternHeartbeat heartbeat;
    heartbeat.set_timestamp(static_cast<uint64_t>(std::time(nullptr)));

    std::string payload;
    heartbeat.SerializeToString(&payload);

    send(MSG_ID_INTERN_HEARTBEAT, payload);
}

void GameConnection::try_reconnect() {
    if (state_ != GameConnState::DISCONNECTED) return;

    SPDLOG_INFO("[GameConnection]Trying to reconnect to {}:{}", game_ip_, game_port_);
    connect(game_ip_, game_port_);
}

}  // namespace farm
