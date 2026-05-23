#include "gate_server.h"
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
#include <iostream>

#include "base.pb.h"
#include "internal.pb.h"

namespace farm {

// 心跳检测间隔（秒）
static constexpr int HEARTBEAT_INTERVAL = 5;

GateServer::GateServer(const std::string& ip, uint16_t port)
    : ip_(ip)
    , port_(port)
    , base_(nullptr)
    , listener_(nullptr)
    , heartbeat_timer_(nullptr)
    , running_(false)
    , game_port_(0)
{
}

GateServer::~GateServer() {
    stop();
}

bool GateServer::start() {
    base_ = event_base_new();
    if (!base_) {
        std::cerr << "[GateServer] Failed to create event_base" << std::endl;
        return false;
    }

    // 绑定地址
    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port_);
    if (ip_.empty() || ip_ == "0.0.0.0") {
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        inet_pton(AF_INET, ip_.c_str(), &sin.sin_addr);
    }

    // 创建监听器
    listener_ = evconnlistener_new_bind(
        base_, on_accept, this,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
        128, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin));

    if (!listener_) {
        std::cerr << "[GateServer] Failed to create listener on " << ip_ << ":" << port_ << std::endl;
        event_base_free(base_);
        base_ = nullptr;
        return false;
    }

    // 创建心跳定时器
    struct timeval tv;
    tv.tv_sec = HEARTBEAT_INTERVAL;
    tv.tv_usec = 0;
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    evtimer_add(heartbeat_timer_, &tv);

    running_ = true;
    std::cout << "[GateServer] Listening on " << ip_ << ":" << port_ << std::endl;

    // 连接到 Game Server（如果配置了）
    if (!game_ip_.empty() && game_port_ > 0) {
        game_conn_ = std::make_unique<GameConnection>(base_, "gate-1");
        game_conn_->set_message_callback(
            [this](uint32_t msg_id, const std::vector<uint8_t>& payload) {
                handle_game_message(msg_id, payload);
            });
        game_conn_->connect(game_ip_, game_port_);
    }

    // 进入事件循环（阻塞）
    event_base_dispatch(base_);

    running_ = false;
    return true;
}

void GateServer::stop() {
    running_ = false;
    if (game_conn_) {
        game_conn_->disconnect();
        game_conn_.reset();
    }
    if (heartbeat_timer_) {
        event_free(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
    if (listener_) {
        evconnlistener_free(listener_);
        listener_ = nullptr;
    }
    if (base_) {
        event_base_loopexit(base_, nullptr);
        event_base_free(base_);
        base_ = nullptr;
    }
}

void GateServer::set_game_server(const std::string& ip, uint16_t port) {
    game_ip_ = ip;
    game_port_ = port;
}

void GateServer::on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                           struct sockaddr* addr, int len, void* ctx) {
    auto* server = static_cast<GateServer*>(ctx);
    server->handle_accept(fd, addr);
}

void GateServer::handle_accept(evutil_socket_t fd, struct sockaddr* addr) {
    // 创建 bufferevent
    struct bufferevent* bev = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        std::cerr << "[GateServer] Failed to create bufferevent for fd=" << fd << std::endl;
        evutil_closesocket(fd);
        return;
    }

    // 创建 Session
    auto session = session_mgr_.add_session(fd, bev);

    // 设置回调
    bufferevent_setcb(bev, on_read, nullptr, on_event, this);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    // 打印客户端地址
    char ip_str[INET_ADDRSTRLEN] = {0};
    if (addr->sa_family == AF_INET) {
        auto* sin = reinterpret_cast<struct sockaddr_in*>(addr);
        inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));
        std::cout << "[GateServer] New connection from " << ip_str << ":" << ntohs(sin->sin_port)
                  << " fd=" << fd << std::endl;
    }
}

void GateServer::on_read(struct bufferevent* bev, void* ctx) {
    auto* server = static_cast<GateServer*>(ctx);
    evutil_socket_t fd = bufferevent_getfd(bev);
    auto session = server->session_mgr_.find_by_fd(fd);
    if (!session) {
        return;
    }
    server->handle_read(session);
}

void GateServer::handle_read(std::shared_ptr<Session> session) {
    struct bufferevent* bev = session->bev();
    struct evbuffer* input = bufferevent_get_input(bev);

    // 读取所有可用数据到 Session 缓冲区
    char buf[4096];
    while (true) {
        ev_ssize_t n = evbuffer_remove(input, buf, sizeof(buf));
        if (n <= 0) break;
        session->append_read_data(reinterpret_cast<const uint8_t*>(buf), n);
    }

    // 尝试解析消息
    auto& read_buf = session->read_buffer();
    while (!read_buf.empty()) {
        ParsedMessage msg;
        size_t consumed = 0;
        if (MessageParser::try_parse(read_buf.data(), read_buf.size(), msg, consumed)) {
            session->consume_read_data(consumed);
            route_message(session, msg.msg_id, msg.payload);
        } else {
            break;  // 数据不完整，等待更多数据
        }
    }
}

void GateServer::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* server = static_cast<GateServer*>(ctx);
    evutil_socket_t fd = bufferevent_getfd(bev);
    auto session = server->session_mgr_.find_by_fd(fd);

    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        if (session) {
            server->handle_disconnect(session);
        }
    }
}

void GateServer::handle_disconnect(std::shared_ptr<Session> session) {
    evutil_socket_t fd = session->fd();
    std::cout << "[GateServer] Connection closed fd=" << fd << std::endl;

    // 通知 Game 玩家离开
    if (session->state() == SessionState::LOGGED_IN && session->player_id() != 0) {
        if (game_conn_ && game_conn_->is_identified()) {
            farm::PlayerLeave leave;
            leave.set_player_id(session->player_id());
            std::string payload;
            leave.SerializeToString(&payload);
            game_conn_->send(MSG_ID_PLAYER_LEAVE, payload);
            std::cout << "[GateServer] Notified Game: player_id=" << session->player_id() << " left" << std::endl;
        }
        player_to_game_.erase(session->player_id());
    }

    session_mgr_.remove_session(fd);
}

void GateServer::on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* server = static_cast<GateServer*>(ctx);
    server->check_heartbeat();
}

void GateServer::check_heartbeat() {
    auto sessions = session_mgr_.get_all_sessions();
    time_t now = std::time(nullptr);

    for (auto& session : sessions) {
        if (session->state() == SessionState::DISCONNECTED) continue;

        if (now - session->last_heartbeat() > HEARTBEAT_TIMEOUT) {
            std::cout << "[GateServer] Heartbeat timeout fd=" << session->fd() << std::endl;
            handle_disconnect(session);
        }
    }
}

void GateServer::route_message(std::shared_ptr<Session> session, uint32_t msg_id,
                               const std::vector<uint8_t>& payload) {
    // 心跳消息直接处理
    if (msg_id == MSG_ID_HEARTBEAT) {
        handle_heartbeat(session, payload);
        return;
    }

    // 登录消息处理
    if (msg_id == MSG_ID_LOGIN_REQ) {
        handle_login(session, payload);
        return;
    }

    // 游戏逻辑消息（4000+）转发到 Game
    if (msg_id >= 4000) {
        forward_to_game(session, msg_id, payload);
        return;
    }

    std::cout << "[GateServer] Unknown msg_id=" << msg_id << " from fd=" << session->fd() << std::endl;
}

void GateServer::handle_heartbeat(std::shared_ptr<Session> session, const std::vector<uint8_t>& payload) {
    session->update_heartbeat();

    // 解析心跳消息
    farm::Heartbeat hb;
    if (!payload.empty()) {
        hb.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    // 回复心跳
    farm::Heartbeat resp;
    resp.set_timestamp(static_cast<uint64_t>(std::time(nullptr)));
    std::string resp_data;
    resp.SerializeToString(&resp_data);

    auto packed = MessageParser::pack(MSG_ID_HEARTBEAT_RESP, resp_data);
    bufferevent_write(session->bev(), packed.data(), packed.size());
}

void GateServer::handle_login(std::shared_ptr<Session> session, const std::vector<uint8_t>& payload) {
    farm::LoginReq req;
    if (!payload.empty()) {
        req.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    std::cout << "[GateServer] Login request from fd=" << session->fd()
              << " token=" << req.token() << std::endl;

    // 简单验证（目前直接通过）
    session->set_state(SessionState::LOGGED_IN);
    uint64_t player_id = static_cast<uint64_t>(session->fd());  // 临时用 fd 作为 player_id
    session_mgr_.bind_player_id(session->fd(), player_id);

    // 返回登录响应
    farm::LoginResp resp;
    resp.set_code(0);
    resp.set_msg("login success");
    std::string resp_data;
    resp.SerializeToString(&resp_data);

    auto packed = MessageParser::pack(MSG_ID_LOGIN_RESP, resp_data);
    bufferevent_write(session->bev(), packed.data(), packed.size());

    // 通知 Game 玩家上线
    if (game_conn_ && game_conn_->is_identified()) {
        farm::PlayerJoin join;
        join.set_player_id(player_id);
        std::string join_payload;
        join.SerializeToString(&join_payload);
        game_conn_->send(MSG_ID_PLAYER_JOIN, join_payload);
        std::cout << "[GateServer] Notified Game: player_id=" << player_id << " joined" << std::endl;
    }
}

// ===========================================
// Game 连接相关
// ===========================================

void GateServer::handle_game_message(uint32_t msg_id, const std::vector<uint8_t>& payload) {
    switch (msg_id) {
        case MSG_ID_GATE_IDENTIFY_RESP:
            handle_game_identify_resp(payload);
            break;
        case MSG_ID_INTERN_HEARTBEAT_RESP:
            handle_game_heartbeat_resp(payload);
            break;
        case MSG_ID_PLAYER_JOIN_RESP:
            handle_player_join_resp(payload);
            break;
        case MSG_ID_GAME_MSG:
            handle_game_msg(payload);
            break;
        default:
            std::cout << "[GateServer] Unknown game msg_id=" << msg_id << std::endl;
            break;
    }
}

void GateServer::handle_game_identify_resp(const std::vector<uint8_t>& payload) {
    farm::GateIdentifyResp resp;
    if (!payload.empty()) {
        resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    if (resp.code() == 0) {
        std::cout << "[GateServer] Game identified successfully" << std::endl;
        // GameConnection 内部已设置状态为 IDENTIFIED
    } else {
        std::cout << "[GateServer] Game identification failed: " << resp.msg() << std::endl;
    }
}

void GateServer::handle_game_heartbeat_resp(const std::vector<uint8_t>& payload) {
    // 心跳响应，GameConnection 内部已处理
}

void GateServer::handle_player_join_resp(const std::vector<uint8_t>& payload) {
    farm::PlayerJoinResp resp;
    if (!payload.empty()) {
        resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    if (resp.code() == 0) {
        std::cout << "[GateServer] Player " << resp.player_id() << " joined Game successfully" << std::endl;
        // 注册玩家路由
        player_to_game_[resp.player_id()] = game_conn_.get();
    } else {
        std::cout << "[GateServer] Player " << resp.player_id() << " join Game failed: " << resp.msg() << std::endl;
    }
}

void GateServer::handle_game_msg(const std::vector<uint8_t>& payload) {
    farm::GameMessage game_msg;
    if (!payload.empty()) {
        game_msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    uint64_t player_id = game_msg.player_id();
    uint32_t msg_id = game_msg.msg_id();
    const std::string& inner_payload = game_msg.payload();

    // 查找客户端 Session
    auto session = session_mgr_.find_by_player_id(player_id);
    if (!session) {
        std::cout << "[GateServer] Warning: player_id=" << player_id
                  << " not found, discarding GAME_MSG" << std::endl;
        return;
    }

    // 直接用原始 msg_id + payload 发送给客户端
    auto packed = MessageParser::pack(msg_id, inner_payload);
    bufferevent_write(session->bev(), packed.data(), packed.size());
}

void GateServer::forward_to_game(std::shared_ptr<Session> session, uint32_t msg_id,
                                  const std::vector<uint8_t>& payload) {
    // 检查 Game 连接
    if (!game_conn_ || !game_conn_->is_identified()) {
        std::cout << "[GateServer] Warning: Game not connected, discarding msg_id="
                  << msg_id << " from player_id=" << session->player_id() << std::endl;
        return;
    }

    // 打包为 ClientMessage
    farm::ClientMessage client_msg;
    client_msg.set_player_id(session->player_id());
    client_msg.set_msg_id(msg_id);
    client_msg.set_payload(payload.data(), payload.size());

    std::string packed_payload;
    client_msg.SerializeToString(&packed_payload);

    game_conn_->send(MSG_ID_CLIENT_MSG, packed_payload);
}

}  // namespace farm
