#include "game_server.h"
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

#include "internal.pb.h"

namespace farm {

GameServer::GameServer(const std::string& ip, uint16_t port)
    : ip_(ip)
    , port_(port)
    , base_(nullptr)
    , listener_(nullptr)
    , heartbeat_timer_(nullptr)
    , running_(false)
{
}

GameServer::~GameServer() {
    stop();
}

bool GameServer::start() {
    base_ = event_base_new();
    if (!base_) {
        std::cerr << "[GameServer] Failed to create event_base" << std::endl;
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
        std::cerr << "[GameServer] Failed to create listener on "
                  << ip_ << ":" << port_ << std::endl;
        event_base_free(base_);
        base_ = nullptr;
        return false;
    }

    // 创建心跳定时器
    struct timeval tv;
    tv.tv_sec = HEARTBEAT_TIMEOUT;
    tv.tv_usec = 0;
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    evtimer_add(heartbeat_timer_, &tv);

    running_ = true;
    std::cout << "[GameServer] Listening on " << ip_ << ":" << port_ << std::endl;

    // 进入事件循环（阻塞）
    event_base_dispatch(base_);

    running_ = false;
    return true;
}

void GameServer::stop() {
    running_ = false;
    if (heartbeat_timer_) {
        event_free(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
    if (listener_) {
        evconnlistener_free(listener_);
        listener_ = nullptr;
    }
    // 清理所有 Gate 会话
    gate_sessions_.clear();
    if (base_) {
        event_base_loopexit(base_, nullptr);
        event_base_free(base_);
        base_ = nullptr;
    }
}

void GameServer::register_handler(uint32_t msg_id, MessageCallback callback) {
    msg_handler_.register_handler(msg_id, std::move(callback));
}

// ===========================================
// libevent 回调
// ===========================================

void GameServer::on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                           struct sockaddr* addr, int len, void* ctx) {
    auto* server = static_cast<GameServer*>(ctx);
    server->handle_accept(fd, addr);
}

void GameServer::handle_accept(evutil_socket_t fd, struct sockaddr* addr) {
    // 创建 bufferevent
    struct bufferevent* bev = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        std::cerr << "[GameServer] Failed to create bufferevent for fd=" << fd << std::endl;
        evutil_closesocket(fd);
        return;
    }

    // 创建 GateSession
    auto session = std::make_shared<GateSession>(fd, bev);
    gate_sessions_[fd] = session;

    // 设置回调
    bufferevent_setcb(bev, on_read, nullptr, on_event, this);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    // 打印客户端地址
    char ip_str[INET_ADDRSTRLEN] = {0};
    if (addr->sa_family == AF_INET) {
        auto* sin = reinterpret_cast<struct sockaddr_in*>(addr);
        inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));
        std::cout << "[GameServer] New Gate connection from "
                  << ip_str << ":" << ntohs(sin->sin_port)
                  << " fd=" << fd << std::endl;
    }
}

void GameServer::on_read(struct bufferevent* bev, void* ctx) {
    auto* server = static_cast<GameServer*>(ctx);
    evutil_socket_t fd = bufferevent_getfd(bev);
    auto it = server->gate_sessions_.find(fd);
    if (it == server->gate_sessions_.end()) {
        return;
    }
    server->handle_read(it->second);
}

void GameServer::handle_read(std::shared_ptr<GateSession> session) {
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
            route_internal_message(session, msg.msg_id, msg.payload);
        } else {
            break;  // 数据不完整，等待更多数据
        }
    }
}

void GameServer::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* server = static_cast<GameServer*>(ctx);
    evutil_socket_t fd = bufferevent_getfd(bev);
    auto it = server->gate_sessions_.find(fd);

    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        if (it != server->gate_sessions_.end()) {
            server->handle_disconnect(it->second);
        }
    }
}

void GameServer::handle_disconnect(std::shared_ptr<GateSession> session) {
    evutil_socket_t fd = session->fd();
    std::string gate_id = session->gate_id();
    std::cout << "[GameServer] Gate disconnected fd=" << fd
              << " gate_id=" << gate_id << std::endl;

    // 清理该 Gate 关联的所有玩家
    player_mgr_.remove_players_by_gate(session.get());

    // 移除会话
    gate_sessions_.erase(fd);
}

void GameServer::on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* server = static_cast<GameServer*>(ctx);
    server->check_heartbeat();
}

void GameServer::check_heartbeat() {
    time_t now = std::time(nullptr);
    std::vector<evutil_socket_t> timeout_fds;

    for (auto& kv : gate_sessions_) {
        auto& session = kv.second;
        if (session->state() == GateSessionState::DISCONNECTED) continue;

        if (now - session->last_heartbeat() > HEARTBEAT_TIMEOUT) {
            std::cout << "[GameServer] Heartbeat timeout fd=" << session->fd()
                      << " gate_id=" << session->gate_id() << std::endl;
            timeout_fds.push_back(kv.first);
        }
    }

    for (evutil_socket_t fd : timeout_fds) {
        auto it = gate_sessions_.find(fd);
        if (it != gate_sessions_.end()) {
            handle_disconnect(it->second);
        }
    }
}

// ===========================================
// 内部消息路由
// ===========================================

void GameServer::route_internal_message(std::shared_ptr<GateSession> session,
                                        uint32_t msg_id,
                                        const std::vector<uint8_t>& payload) {
    // 身份识别前只允许 GATE_IDENTIFY 消息
    if (session->state() == GateSessionState::CONNECTED) {
        if (msg_id == MSG_ID_GATE_IDENTIFY) {
            handle_gate_identify(session, payload);
        } else {
            std::cout << "[GameServer] Ignoring msg_id=" << msg_id
                      << " from unidentified Gate fd=" << session->fd() << std::endl;
        }
        return;
    }

    // 已识别的 Gate 处理所有内部消息
    switch (msg_id) {
        case MSG_ID_INTERN_HEARTBEAT:
            handle_intern_heartbeat(session, payload);
            break;
        case MSG_ID_PLAYER_JOIN:
            handle_player_join(session, payload);
            break;
        case MSG_ID_PLAYER_LEAVE:
            handle_player_leave(session, payload);
            break;
        case MSG_ID_CLIENT_MSG:
            handle_client_msg(session, payload);
            break;
        default:
            std::cout << "[GameServer] Unknown internal msg_id=" << msg_id
                      << " from fd=" << session->fd() << std::endl;
            break;
    }
}

// ===========================================
// 具体消息处理
// ===========================================

void GameServer::handle_gate_identify(std::shared_ptr<GateSession> session,
                                      const std::vector<uint8_t>& payload) {
    farm::GateIdentify req;
    if (!payload.empty()) {
        req.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    session->set_gate_id(req.gate_id());
    session->set_state(GateSessionState::IDENTIFIED);
    session->update_heartbeat();

    std::cout << "[GameServer] Gate identified: gate_id=" << req.gate_id()
              << " address=" << req.address()
              << " fd=" << session->fd() << std::endl;

    // 回复 GATE_IDENTIFY_RESP
    farm::GateIdentifyResp resp;
    resp.set_code(0);
    resp.set_msg("identified");
    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_gate(session, MSG_ID_GATE_IDENTIFY_RESP, resp_data);
}

void GameServer::handle_intern_heartbeat(std::shared_ptr<GateSession> session,
                                         const std::vector<uint8_t>& payload) {
    session->update_heartbeat();

    // 解析心跳消息
    farm::InternHeartbeat hb;
    if (!payload.empty()) {
        hb.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    // 回复心跳
    farm::InternHeartbeatResp resp;
    resp.set_timestamp(static_cast<uint64_t>(std::time(nullptr)));
    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_gate(session, MSG_ID_INTERN_HEARTBEAT_RESP, resp_data);
}

void GameServer::handle_player_join(std::shared_ptr<GateSession> session,
                                    const std::vector<uint8_t>& payload) {
    farm::PlayerJoin req;
    if (!payload.empty()) {
        req.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    uint64_t player_id = req.player_id();
    std::cout << "[GameServer] Player join: player_id=" << player_id
              << " from gate_id=" << session->gate_id() << std::endl;

    farm::PlayerJoinResp resp;
    resp.set_player_id(player_id);

    bool added = player_mgr_.add_player(player_id, session.get());
    if (added) {
        resp.set_code(0);
        resp.set_msg("joined");
        std::cout << "[GameServer] Player " << player_id << " added to PlayerManager" << std::endl;
    } else {
        resp.set_code(1);
        resp.set_msg("already joined");
        std::cout << "[GameServer] Player " << player_id << " already exists" << std::endl;
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_gate(session, MSG_ID_PLAYER_JOIN_RESP, resp_data);
}

void GameServer::handle_player_leave(std::shared_ptr<GateSession> session,
                                     const std::vector<uint8_t>& payload) {
    farm::PlayerLeave req;
    if (!payload.empty()) {
        req.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    uint64_t player_id = req.player_id();
    std::cout << "[GameServer] Player leave: player_id=" << player_id
              << " from gate_id=" << session->gate_id() << std::endl;

    player_mgr_.remove_player(player_id);
}

void GameServer::handle_client_msg(std::shared_ptr<GateSession> session,
                                   const std::vector<uint8_t>& payload) {
    farm::ClientMessage req;
    if (!payload.empty()) {
        req.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    uint64_t player_id = req.player_id();
    uint32_t msg_id = req.msg_id();
    const std::string& inner_payload = req.payload();

    // 检查玩家是否存在
    Player* player = player_mgr_.get_player(player_id);
    if (!player) {
        std::cout << "[GameServer] Warning: player_id=" << player_id
                  << " not found, discarding msg_id=" << msg_id << std::endl;
        return;
    }

    // 分发到业务 handler
    msg_handler_.dispatch(msg_id, player_id,
                          reinterpret_cast<const uint8_t*>(inner_payload.data()),
                          inner_payload.size());
}

// ===========================================
// 发送消息辅助
// ===========================================

void GameServer::send_to_gate(std::shared_ptr<GateSession> session,
                              uint32_t msg_id, const std::string& payload) {
    auto packed = MessageParser::pack(msg_id, payload);
    bufferevent_write(session->bev(), packed.data(), packed.size());
}

void GameServer::send_game_msg(uint64_t player_id, uint32_t msg_id,
                               const uint8_t* payload, size_t payload_len) {
    // 查找玩家对应的 GateSession
    Player* player = player_mgr_.get_player(player_id);
    if (!player || !player->gate_session()) {
        std::cout << "[GameServer] Cannot send GAME_MSG: player_id=" << player_id
                  << " has no Gate session" << std::endl;
        return;
    }

    // 打包为 GameMessage
    farm::GameMessage game_msg;
    game_msg.set_player_id(player_id);
    game_msg.set_msg_id(msg_id);
    game_msg.set_payload(payload, payload_len);

    std::string resp_data;
    game_msg.SerializeToString(&resp_data);

    // 查找对应的 GateSession
    GateSession* gate = player->gate_session();
    auto it = gate_sessions_.find(gate->fd());
    if (it != gate_sessions_.end()) {
        send_to_gate(it->second, MSG_ID_GAME_MSG, resp_data);
    }
}

}  // namespace farm
