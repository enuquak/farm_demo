#include "team_connection.h"
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

TeamConnection::TeamConnection(struct event_base* base, const std::string& gate_id)
    : base_(base)
    , gate_id_(gate_id)
    , team_port_(0)
    , state_(TeamConnState::DISCONNECTED)
    , bev_(nullptr)
    , heartbeat_timer_(nullptr)
    , reconnect_timer_(nullptr)
    , last_heartbeat_(0)
{
}

TeamConnection::~TeamConnection() {
    disconnect();
    stop_heartbeat();
    stop_reconnect();
}

bool TeamConnection::connect(const std::string& ip, uint16_t port) {
    team_ip_ = ip;
    team_port_ = port;

    // 创建 socket
    evutil_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        SPDLOG_ERROR("[TeamConnection]Failed to create socket");
        return false;
    }

    // 设置非阻塞
    evutil_make_socket_nonblocking(fd);

    // 创建 bufferevent
    bev_ = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev_) {
        SPDLOG_ERROR("[TeamConnection]Failed to create bufferevent");
        evutil_closesocket(fd);
        return false;
    }

    // 设置回调
    bufferevent_setcb(bev_, on_read, nullptr, on_event, this);
    bufferevent_enable(bev_, EV_READ | EV_WRITE);

    // 连接到 Team Server
    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &sin.sin_addr);

    state_ = TeamConnState::CONNECTING;

    if (bufferevent_socket_connect(bev_,
            reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin)) < 0) {
        SPDLOG_ERROR("[TeamConnection]Failed to connect to {}:{}", ip, port);
        disconnect();
        return false;
    }

    SPDLOG_INFO("[TeamConnection]Connecting to {}:{}", ip, port);
    return true;
}

void TeamConnection::disconnect() {
    state_ = TeamConnState::DISCONNECTED;
    if (bev_) {
        bufferevent_free(bev_);
        bev_ = nullptr;
    }
    read_buffer_.clear();
}

bool TeamConnection::send(uint32_t msg_id, std::string_view payload) {
    return send(msg_id, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
}

bool TeamConnection::send(uint32_t msg_id, const uint8_t* payload, size_t len) {
    if (!bev_ || state_ == TeamConnState::DISCONNECTED) {
        return false;
    }

    auto packed = MessageParser::pack(msg_id, payload, len);
    bufferevent_write(bev_, packed.data(), packed.size());
    return true;
}

void TeamConnection::start_heartbeat() {
    if (heartbeat_timer_) return;

    struct timeval tv;
    tv.tv_sec = 5;  // 5 秒间隔
    tv.tv_usec = 0;
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    evtimer_add(heartbeat_timer_, &tv);
}

void TeamConnection::stop_heartbeat() {
    if (heartbeat_timer_) {
        event_free(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
}

void TeamConnection::start_reconnect() {
    if (reconnect_timer_) return;

    struct timeval tv;
    tv.tv_sec = 5;  // 5 秒间隔
    tv.tv_usec = 0;
    reconnect_timer_ = event_new(base_, -1, EV_PERSIST, on_reconnect_timer, this);
    evtimer_add(reconnect_timer_, &tv);
    SPDLOG_INFO("[TeamConnection]Reconnect timer started (5s interval)");
}

void TeamConnection::stop_reconnect() {
    if (reconnect_timer_) {
        event_free(reconnect_timer_);
        reconnect_timer_ = nullptr;
    }
}

// ===========================================
// libevent 回调
// ===========================================

void TeamConnection::on_read(struct bufferevent* bev, void* ctx) {
    auto* conn = static_cast<TeamConnection*>(ctx);
    conn->handle_read();
}

void TeamConnection::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* conn = static_cast<TeamConnection*>(ctx);

    if (events & BEV_EVENT_CONNECTED) {
        conn->handle_connect_success();
    } else if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        conn->handle_disconnect();
    }
}

void TeamConnection::on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* conn = static_cast<TeamConnection*>(ctx);
    conn->send_heartbeat();
}

void TeamConnection::on_reconnect_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* conn = static_cast<TeamConnection*>(ctx);
    conn->try_reconnect();
}

// ===========================================
// 内部处理
// ===========================================

void TeamConnection::handle_read() {
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

void TeamConnection::handle_connect_success() {
    SPDLOG_INFO("[TeamConnection]Connected to {}:{}", team_ip_, team_port_);

    // 发送身份标识
    send_identify();

    // 启动心跳
    start_heartbeat();

    // 停止重连定时器
    stop_reconnect();
}

void TeamConnection::handle_disconnect() {
    SPDLOG_INFO("[TeamConnection]Disconnected from {}:{}", team_ip_, team_port_);

    state_ = TeamConnState::DISCONNECTED;
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

void TeamConnection::send_identify() {
    farm::GateIdentify identify;
    identify.set_gate_id(gate_id_);
    identify.set_address("127.0.0.1:8080");  // Gate 自身地址

    std::string payload;
    identify.SerializeToString(&payload);

    send(MSG_ID_TEAM_SERVICE_IDENTIFY, payload);
    SPDLOG_INFO("[TeamConnection]Sent TEAM_SERVICE_IDENTIFY (gate_id={})", gate_id_);
}

void TeamConnection::send_heartbeat() {
    if (state_ != TeamConnState::IDENTIFIED) return;

    farm::InternHeartbeat heartbeat;
    heartbeat.set_timestamp(static_cast<uint64_t>(std::time(nullptr)));

    std::string payload;
    heartbeat.SerializeToString(&payload);

    send(MSG_ID_TEAM_SERVICE_HEARTBEAT, payload);
}

void TeamConnection::try_reconnect() {
    if (state_ != TeamConnState::DISCONNECTED) return;

    SPDLOG_INFO("[TeamConnection]Trying to reconnect to {}:{}", team_ip_, team_port_);
    connect(team_ip_, team_port_);
}

}  // namespace farm
