#include "gate_server.h"
#include "message_parser.h"
#include "internal_msg_ids.h"
#include "message_ids.h"
#include "admin_msg_ids.h"

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

#include "base.pb.h"
#include "internal.pb.h"
#include "account.pb.h"
#include "log_macros.h"

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
    , next_game_index_(0)
{
}

GateServer::~GateServer() {
    stop();
}

bool GateServer::start() {
    base_ = event_base_new();
    if (!base_) {
        SPDLOG_ERROR("[Gate]Failed to create event_base");
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
        SPDLOG_ERROR("[Gate]Failed to create listener on {}:{}", ip_, port_);
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
    SPDLOG_INFO("[Gate]Listening on {}:{}", ip_, port_);

    // 连接到所有配置的 Game Server
    if (!game_server_configs_.empty()) {
        std::lock_guard<std::mutex> lock(game_conns_mutex_);
        for (const auto& config : game_server_configs_) {
            auto conn = std::make_unique<GameConnection>(base_, "gate-1");
            uint32_t server_id = config.server_id;
            conn->set_message_callback(
                [this, server_id](uint32_t msg_id, const std::vector<uint8_t>& payload) {
                    handle_game_message(server_id, msg_id, payload);
                });
            conn->connect(config.ip, config.port);
            game_conns_[server_id] = std::move(conn);
            SPDLOG_INFO("[Gate]Connecting to Game Server {} at {}:{}", server_id, config.ip, config.port);
        }
    }

    // 连接到 Chat Server（如果配置了）
    if (!chat_server_ip_.empty() && chat_server_port_ > 0) {
        chat_conn_ = std::make_unique<ChatConnection>(base_, "gate_1");
        chat_conn_->set_message_callback([this](uint32_t msg_id, const std::vector<uint8_t>& payload) {
            handle_chat_message(msg_id, payload);
        });
        chat_conn_->connect(chat_server_ip_, chat_server_port_);
        SPDLOG_INFO("[Gate]ChatServer connection configured: {}:{}", chat_server_ip_, chat_server_port_);
    }

    // 连接到 Team Server（如果配置了）
    if (!team_server_ip_.empty() && team_server_port_ > 0) {
        team_conn_ = std::make_unique<TeamConnection>(base_, "gate_1");
        team_conn_->set_message_callback([this](uint32_t msg_id, const std::vector<uint8_t>& payload) {
            handle_team_message(msg_id, payload);
        });
        team_conn_->connect(team_server_ip_, team_server_port_);
        SPDLOG_INFO("[Gate]TeamServer connection configured: {}:{}", team_server_ip_, team_server_port_);
    }

    // 进入事件循环（阻塞）
    event_base_dispatch(base_);

    running_ = false;
    return true;
}

void GateServer::stop() {
    running_ = false;
    // 断开所有 Game 连接
    {
        std::lock_guard<std::mutex> lock(game_conns_mutex_);
        for (auto& [server_id, conn] : game_conns_) {
            conn->disconnect();
        }
        game_conns_.clear();
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

void GateServer::dispatch_to_event_loop(std::function<void()> func) {
    if (!base_) return;

    // 包装为 heap 分配的 std::function，由 libevent 回调释放
    auto* func_ptr = new std::function<void()>(std::move(func));

    struct timeval tv = {0, 0};  // 立即触发
    event_base_once(base_, -1, EV_TIMEOUT, on_dispatch_callback, func_ptr, &tv);
}

void GateServer::on_dispatch_callback(evutil_socket_t fd, short events, void* ctx) {
    auto* func = static_cast<std::function<void()>*>(ctx);
    (*func)();
    delete func;
}

void GateServer::add_game_server(uint32_t server_id, const std::string& ip, uint16_t port) {
    std::lock_guard<std::mutex> lock(game_conns_mutex_);

    // 如果已存在，先断开旧连接
    auto it = game_conns_.find(server_id);
    if (it != game_conns_.end()) {
        SPDLOG_INFO("[Gate]Replacing existing Game Server {} connection", server_id);
        it->second->disconnect();
        game_conns_.erase(it);
    }

    // 如果 base_ 已初始化，直接连接
    if (base_) {
        auto conn = std::make_unique<GameConnection>(base_, "gate-1");
        conn->set_message_callback(
            [this, server_id](uint32_t msg_id, const std::vector<uint8_t>& payload) {
                handle_game_message(server_id, msg_id, payload);
            });
        conn->connect(ip, port);
        game_conns_[server_id] = std::move(conn);
        SPDLOG_INFO("[Gate]Connecting to Game Server {} at {}:{}", server_id, ip, port);
    } else {
        // base_ 未初始化，存储配置供 start() 使用
        GameServerConfig config;
        config.server_id = server_id;
        config.ip = ip;
        config.port = port;
        game_server_configs_.push_back(config);
    }
}

void GateServer::remove_game_server(uint32_t server_id) {
    std::lock_guard<std::mutex> lock(game_conns_mutex_);

    auto it = game_conns_.find(server_id);
    if (it == game_conns_.end()) {
        SPDLOG_WARN("[Gate]Game Server {} not found for removal", server_id);
        return;
    }

    // 断开连接
    it->second->disconnect();
    game_conns_.erase(it);

    // 清理该 server_id 的玩家路由
    for (auto it_route = player_to_server_.begin(); it_route != player_to_server_.end(); ) {
        if (it_route->second == server_id) {
            SPDLOG_INFO("[Gate]Removed player route: player_id={} server_id={}",
                        it_route->first, it_route->second);
            it_route = player_to_server_.erase(it_route);
        } else {
            ++it_route;
        }
    }

    SPDLOG_INFO("[Gate]Removed Game Server {}", server_id);
}

std::optional<GameConnection*> GateServer::get_game_connection(uint32_t server_id) {
    std::lock_guard<std::mutex> lock(game_conns_mutex_);
    auto it = game_conns_.find(server_id);
    if (it != game_conns_.end() && it->second->is_identified()) {
        return it->second.get();
    }
    return std::nullopt;
}

std::optional<GameConnection*> GateServer::get_any_game_connection() {
    std::lock_guard<std::mutex> lock(game_conns_mutex_);
    if (game_conns_.empty()) {
        return std::nullopt;
    }

    // 轮询选择
    size_t count = 0;
    size_t size = game_conns_.size();
    while (count < size) {
        size_t index = next_game_index_ % size;
        next_game_index_++;

        auto it = game_conns_.begin();
        std::advance(it, index);
        if (it->second->is_identified()) {
            return it->second.get();
        }
        count++;
    }

    return std::nullopt;
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
        SPDLOG_ERROR("[Gate]Failed to create bufferevent for fd={}", fd);
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
        SPDLOG_INFO("[Gate]New connection from {}:{} fd={}", ip_str, ntohs(sin->sin_port), fd);
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
    SPDLOG_INFO("[Gate]Connection closed fd={}", fd);

    // 通知 Game 玩家离开
    {
        std::lock_guard<std::mutex> lock(game_conns_mutex_);
        if (session->state() == SessionState::LOGGED_IN && session->player_id() != 0) {
            auto it = player_to_server_.find(session->player_id());
            if (it != player_to_server_.end()) {
                auto conn_it = game_conns_.find(it->second);
                if (conn_it != game_conns_.end() && conn_it->second->is_identified()) {
                    farm::PlayerLeave leave;
                    leave.set_player_id(session->player_id());
                    std::string payload;
                    leave.SerializeToString(&payload);
                    conn_it->second->send(MSG_ID_PLAYER_LEAVE, payload);
                    SPDLOG_INFO("[Gate]Notified Game {}: player_id={} left", it->second, session->player_id());
                }
                player_to_server_.erase(it);
            }
        }
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

        if (now - session->last_heartbeat() > GATE_HEARTBEAT_TIMEOUT) {
            SPDLOG_INFO("[Gate]Heartbeat timeout fd={}", session->fd());
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

    // 管理消息（5000-5999）
    if (msg_id >= 5000 && msg_id < 6000) {
        handle_admin_message(msg_id, payload);
        return;
    }

    // 账号消息（1000-1999）
    if (msg_id >= 1000 && msg_id < 2000) {
        forward_account_msg_to_game(session, msg_id, payload);
        return;
    }

    // 玩家消息（2000-2999）
    if (msg_id >= 2000 && msg_id < 3000) {
        // 解析 PlayerMsg 获取 server_id
        farm::PlayerMsg player_msg;
        if (!payload.empty() && !player_msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
            SPDLOG_ERROR("[Gate]Failed to parse PlayerMsg from fd={}", session->fd());
            return;
        }
        uint32_t server_id = player_msg.server_id();
        forward_player_msg_to_game(session, server_id, msg_id, payload);
        return;
    }

    // Chat 消息（6000-6999）
    if (msg_id >= 6000 && msg_id < 7000) {
        forward_to_chat(session, msg_id, payload);
        return;
    }

    // Team 消息（7001-7099）
    if (msg_id >= 7001 && msg_id <= 7099) {
        forward_to_team(session, msg_id, payload);
        return;
    }

    // 内部消息（3000+）转发到 Game
    if (msg_id >= 3000) {
        forward_to_game(session, msg_id, payload);
        return;
    }

    SPDLOG_INFO("[Gate]Unknown msg_id={} from fd={}", msg_id, session->fd());
}

void GateServer::handle_heartbeat(std::shared_ptr<Session> session, const std::vector<uint8_t>& payload) {
    session->update_heartbeat();

    // 解析心跳消息
    farm::Heartbeat hb;
    if (!payload.empty() && !hb.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Gate]Failed to parse Heartbeat");
        return;
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
    if (!payload.empty() && !req.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Gate]Failed to parse LoginReq");
        return;
    }

    SPDLOG_INFO("[Gate]Login request from fd={} token={}", session->fd(), req.token());

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
    auto conn = get_any_game_connection();
    if (conn) {
        farm::PlayerJoin join;
        join.set_player_id(player_id);
        std::string join_payload;
        join.SerializeToString(&join_payload);
        conn.value()->send(MSG_ID_PLAYER_JOIN, join_payload);
        SPDLOG_INFO("[Gate]Notified Game: player_id={} joined", player_id);
    }
}

// ===========================================
// Game 连接相关
// ===========================================

void GateServer::handle_game_message(uint32_t server_id, uint32_t msg_id, const std::vector<uint8_t>& payload) {
    switch (msg_id) {
        case MSG_ID_GATE_IDENTIFY_RESP:
            handle_game_identify_resp(server_id, payload);
            break;
        case MSG_ID_INTERN_HEARTBEAT_RESP:
            handle_game_heartbeat_resp(server_id, payload);
            break;
        case MSG_ID_PLAYER_JOIN_RESP:
            handle_player_join_resp(server_id, payload);
            break;
        case MSG_ID_GAME_MSG:
            handle_game_msg(server_id, payload);
            break;
        case MSG_ID_ACCOUNT_MSG_RESP:
            handle_account_msg_resp(server_id, payload);
            break;
        default:
            SPDLOG_INFO("[Gate]Unknown game msg_id={} from server_id={}", msg_id, server_id);
            break;
    }
}

void GateServer::handle_game_identify_resp(uint32_t server_id, const std::vector<uint8_t>& payload) {
    farm::GateIdentifyResp resp;
    if (!payload.empty() && !resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Gate]Failed to parse GateIdentifyResp from server_id={}", server_id);
        return;
    }

    std::lock_guard<std::mutex> lock(game_conns_mutex_);
    if (resp.code() == 0) {
        SPDLOG_INFO("[Gate]Game Server {} identified successfully", server_id);
        // 设置 GameConnection 状态为 IDENTIFIED
        auto it = game_conns_.find(server_id);
        if (it != game_conns_.end()) {
            it->second->set_state(GameConnState::IDENTIFIED);
        }
    } else {
        SPDLOG_ERROR("[Gate]Game Server {} identification failed: {}", server_id, resp.msg());
    }
}

void GateServer::handle_game_heartbeat_resp(uint32_t server_id, const std::vector<uint8_t>& payload) {
    // 心跳响应，GameConnection 内部已处理
}

void GateServer::handle_player_join_resp(uint32_t server_id, const std::vector<uint8_t>& payload) {
    farm::PlayerJoinResp resp;
    if (!payload.empty() && !resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Gate]Failed to parse PlayerJoinResp from server_id={}", server_id);
        return;
    }

    if (resp.code() == 0) {
        SPDLOG_INFO("[Gate]Player {} joined Game Server {} successfully", resp.player_id(), server_id);
        // 注册玩家路由
        std::lock_guard<std::mutex> lock(game_conns_mutex_);
        player_to_server_[resp.player_id()] = server_id;
    } else {
        SPDLOG_ERROR("[Gate]Player {} join Game Server {} failed: {}", resp.player_id(), server_id, resp.msg());
    }
}

void GateServer::handle_game_msg(uint32_t server_id, const std::vector<uint8_t>& payload) {
    farm::GameMessage game_msg;
    if (!payload.empty() && !game_msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Gate]Failed to parse GameMessage from server_id={}", server_id);
        return;
    }

    uint64_t player_id = game_msg.player_id();
    uint32_t msg_id = game_msg.msg_id();
    const std::string& inner_payload = game_msg.payload();

    // 查找客户端 Session
    auto session = session_mgr_.find_by_player_id(player_id);
    if (!session) {
        SPDLOG_ERROR("[Gate]Warning: player_id={} not found, discarding GAME_MSG", player_id);
        return;
    }

    // 直接用原始 msg_id + payload 发送给客户端
    auto packed = MessageParser::pack(msg_id, inner_payload);
    bufferevent_write(session->bev(), packed.data(), packed.size());
}

void GateServer::handle_account_msg_resp(uint32_t server_id, const std::vector<uint8_t>& payload) {
    farm::AccountMessageResp resp;
    if (!payload.empty() && !resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Gate]Failed to parse AccountMessageResp from server_id={}", server_id);
        return;
    }

    const std::string& account_id = resp.account_id();
    uint32_t msg_id = resp.msg_id();
    const std::string& inner_payload = resp.payload();

    // 查找客户端 Session（通过 account_id）
    auto session = session_mgr_.find_by_account_id(account_id);
    if (!session) {
        SPDLOG_ERROR("[Gate]Warning: account_id={} not found, discarding ACCOUNT_MSG_RESP", account_id);
        return;
    }

    // 发送给客户端
    auto packed = MessageParser::pack(msg_id, inner_payload);
    bufferevent_write(session->bev(), packed.data(), packed.size());
}

void GateServer::forward_to_game(std::shared_ptr<Session> session, uint32_t msg_id,
                                  const std::vector<uint8_t>& payload) {
    // 获取任意 Game 连接
    auto conn = get_any_game_connection();
    if (!conn) {
        SPDLOG_ERROR("[Gate]Warning: No Game connected, discarding msg_id={} from player_id={}", msg_id, session->player_id());
        return;
    }

    // 打包为 ClientMessage
    farm::ClientMessage client_msg;
    client_msg.set_player_id(session->player_id());
    client_msg.set_msg_id(msg_id);
    client_msg.set_payload(payload.data(), payload.size());

    std::string packed_payload;
    client_msg.SerializeToString(&packed_payload);

    conn.value()->send(MSG_ID_CLIENT_MSG, packed_payload);
}

void GateServer::forward_account_msg_to_game(std::shared_ptr<Session> session, uint32_t msg_id,
                                              const std::vector<uint8_t>& payload) {
    // 解析 AccountMsg
    farm::AccountMsg account_msg;
    if (!payload.empty() && !account_msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Gate]Failed to parse AccountMsg");
        return;
    }

    const std::string& account_id = account_msg.account_id();
    uint32_t inner_msg_id = account_msg.msg_id();
    const std::string& inner_payload = account_msg.payload();

    // 绑定 account_id 到 session（用于响应路由）
    session->set_account_id(account_id);

    // 获取任意 Game 连接
    auto conn = get_any_game_connection();
    if (!conn) {
        SPDLOG_ERROR("[Gate]Warning: No Game connected, discarding AccountMsg from account_id={}", account_id);
        return;
    }

    // 打包为 AccountMessage（内部协议）
    farm::AccountMessage internal_msg;
    internal_msg.set_account_id(account_id);
    internal_msg.set_msg_id(inner_msg_id);
    internal_msg.set_payload(inner_payload);

    std::string packed_payload;
    internal_msg.SerializeToString(&packed_payload);

    conn.value()->send(MSG_ID_ACCOUNT_MSG, packed_payload);
}

void GateServer::forward_player_msg_to_game(std::shared_ptr<Session> session, uint32_t server_id,
                                             uint32_t msg_id, const std::vector<uint8_t>& payload) {
    // 获取指定 server_id 的 Game 连接
    auto conn = get_game_connection(server_id);
    if (!conn) {
        SPDLOG_ERROR("[Gate]Warning: Game Server {} not connected, discarding PlayerMsg from player_id={}", server_id, session->player_id());
        return;
    }

    // 打包为 ClientMessage
    farm::ClientMessage client_msg;
    client_msg.set_player_id(session->player_id());
    client_msg.set_msg_id(msg_id);
    client_msg.set_payload(payload.data(), payload.size());

    std::string packed_payload;
    client_msg.SerializeToString(&packed_payload);

    conn.value()->send(MSG_ID_CLIENT_MSG, packed_payload);
}

// ===========================================
// Chat 连接相关
// ===========================================

void GateServer::set_chat_server(const std::string& ip, uint16_t port) {
    chat_server_ip_ = ip;
    chat_server_port_ = port;
}

void GateServer::forward_to_chat(std::shared_ptr<Session> session, uint32_t msg_id,
                                  const std::vector<uint8_t>& payload) {
    if (!chat_conn_ || !chat_conn_->is_identified()) {
        SPDLOG_WARN("[Gate]ChatServer not connected, dropping msg_id={}", msg_id);
        return;
    }
    // Parse PlayerMsg to get player_id
    farm::PlayerMsg player_msg;
    if (!payload.empty() && !player_msg.ParseFromArray(payload.data(),
            static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Gate]Failed to parse PlayerMsg for chat from fd={}", session->fd());
        return;
    }
    uint64_t player_id = player_msg.player_id();
    // Wrap as ClientMessage for ChatServer
    farm::ClientMessage client_msg;
    client_msg.set_player_id(player_id);
    client_msg.set_msg_id(msg_id);
    client_msg.set_payload(player_msg.payload());
    std::string wrapped;
    client_msg.SerializeToString(&wrapped);
    chat_conn_->send(MSG_ID_CLIENT_MSG, wrapped);
}

void GateServer::handle_chat_message(uint32_t msg_id, const std::vector<uint8_t>& payload) {
    // Handle identify response from ChatServer
    if (msg_id == MSG_ID_GATE_IDENTIFY_RESP) {
        farm::GateIdentifyResp resp;
        if (!payload.empty() && !resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
            SPDLOG_ERROR("[Gate]Failed to parse GateIdentifyResp from ChatServer");
            return;
        }
        if (resp.code() == 0) {
            SPDLOG_INFO("[Gate]ChatServer identified successfully");
            if (chat_conn_) {
                chat_conn_->set_state(ChatConnState::IDENTIFIED);
            }
        } else {
            SPDLOG_ERROR("[Gate]ChatServer identification failed: {}", resp.msg());
        }
        return;
    }

    // Handle heartbeat response from ChatServer
    if (msg_id == MSG_ID_INTERN_HEARTBEAT_RESP) {
        // Heartbeat response, ChatConnection internal already handled
        return;
    }

    // ChatServer sends back GameMessage format
    if (msg_id == MSG_ID_GAME_MSG) {
        farm::GameMessage game_msg;
        if (!game_msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
            SPDLOG_ERROR("[Gate]Failed to parse GameMessage from ChatServer");
            return;
        }
        uint64_t player_id = game_msg.player_id();
        uint32_t inner_msg_id = game_msg.msg_id();
        const auto& inner_payload = game_msg.payload();
        // Find the player's session and forward
        auto session = session_mgr_.find_by_player_id(player_id);
        if (!session) {
            SPDLOG_DEBUG("[Gate]Player {} not found for chat response", player_id);
            return;
        }
        // Forward to the player's client
        auto packed = MessageParser::pack(inner_msg_id, inner_payload);
        bufferevent_write(session->bev(), packed.data(), packed.size());
        SPDLOG_DEBUG("[Gate]Forwarding chat msg_id={} to player={}", inner_msg_id, player_id);
        return;
    }

    SPDLOG_INFO("[Gate]Unknown chat msg_id={}", msg_id);
}

// ===========================================
// Team 连接相关
// ===========================================

void GateServer::add_team_server(const std::string& ip, uint16_t port) {
    team_server_ip_ = ip;
    team_server_port_ = port;

    // 如果 base_ 已初始化，直接连接
    if (base_) {
        team_conn_ = std::make_unique<TeamConnection>(base_, "gate_1");
        team_conn_->set_message_callback([this](uint32_t msg_id, const std::vector<uint8_t>& payload) {
            handle_team_message(msg_id, payload);
        });
        team_conn_->connect(ip, port);
        SPDLOG_INFO("[Gate]TeamServer connection configured: {}:{}", ip, port);
    }
}

void GateServer::forward_to_team(std::shared_ptr<Session> session, uint32_t msg_id,
                                 const std::vector<uint8_t>& payload) {
    if (!team_conn_ || !team_conn_->is_identified()) {
        SPDLOG_WARN("[Gate]TeamServer not connected, dropping msg_id={}", msg_id);
        return;
    }
    // Parse PlayerMsg to get player_id
    farm::PlayerMsg player_msg;
    if (!payload.empty() && !player_msg.ParseFromArray(payload.data(),
            static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Gate]Failed to parse PlayerMsg for team from fd={}", session->fd());
        return;
    }
    uint64_t player_id = player_msg.player_id();
    // Wrap as ClientMessage for TeamServer
    farm::ClientMessage client_msg;
    client_msg.set_player_id(player_id);
    client_msg.set_msg_id(msg_id);
    client_msg.set_payload(player_msg.payload());
    std::string wrapped;
    client_msg.SerializeToString(&wrapped);
    team_conn_->send(MSG_ID_TEAM_CLIENT_MSG, wrapped);
}

void GateServer::handle_team_message(uint32_t msg_id, const std::vector<uint8_t>& payload) {
    // Handle identify response from TeamServer
    if (msg_id == MSG_ID_TEAM_SERVICE_IDENTIFY_RESP) {
        farm::GateIdentifyResp resp;
        if (!payload.empty() && !resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
            SPDLOG_ERROR("[Gate]Failed to parse GateIdentifyResp from TeamServer");
            return;
        }
        if (resp.code() == 0) {
            SPDLOG_INFO("[Gate]TeamServer identified successfully");
            if (team_conn_) {
                team_conn_->set_state(TeamConnState::IDENTIFIED);
            }
        } else {
            SPDLOG_ERROR("[Gate]TeamServer identification failed: {}", resp.msg());
        }
        return;
    }

    // Handle heartbeat response from TeamServer
    if (msg_id == MSG_ID_TEAM_SERVICE_HEARTBEAT_RESP) {
        // Heartbeat response, TeamConnection internal already handled
        return;
    }

    // TeamServer sends back GameMessage format
    if (msg_id == MSG_ID_TEAM_SERVICE_MSG) {
        farm::GameMessage game_msg;
        if (!game_msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
            SPDLOG_ERROR("[Gate]Failed to parse GameMessage from TeamServer");
            return;
        }
        uint64_t player_id = game_msg.player_id();
        uint32_t inner_msg_id = game_msg.msg_id();
        const auto& inner_payload = game_msg.payload();
        // Find the player's session and forward
        auto session = session_mgr_.find_by_player_id(player_id);
        if (!session) {
            SPDLOG_DEBUG("[Gate]Player {} not found for team response", player_id);
            return;
        }
        // Forward to the player's client
        auto packed = MessageParser::pack(inner_msg_id, inner_payload);
        bufferevent_write(session->bev(), packed.data(), packed.size());
        SPDLOG_DEBUG("[Gate]Forwarding team msg_id={} to player={}", inner_msg_id, player_id);
        return;
    }

    SPDLOG_INFO("[Gate]Unknown team msg_id={}", msg_id);
}

// ===========================================
// 管理消息处理
// ===========================================

void GateServer::handle_admin_message(uint32_t msg_id, const std::vector<uint8_t>& payload) {
    std::string payload_str(payload.begin(), payload.end());

    switch (msg_id) {
        case MSG_ID_SHUTDOWN: {
            AdminShutdownMsg msg;
            if (AdminShutdownMsg::deserialize(payload_str, msg)) {
                handle_shutdown(msg);
            } else {
                SPDLOG_ERROR("[Gate]Failed to parse MSG_ID_SHUTDOWN");
            }
            break;
        }
        case MSG_ID_SHUTDOWN_RESP: {
            AdminShutdownResp resp;
            if (AdminShutdownResp::deserialize(payload_str, resp)) {
                handle_shutdown_resp(resp);
            } else {
                SPDLOG_ERROR("[Gate]Failed to parse MSG_ID_SHUTDOWN_RESP");
            }
            break;
        }
        default:
            SPDLOG_INFO("[Gate]Unknown admin msg_id={}", msg_id);
            break;
    }
}

void GateServer::handle_shutdown(const AdminShutdownMsg& msg) {
    SPDLOG_INFO("[Gate]Received shutdown request: reason={}, timeout_ms={}", msg.reason, msg.timeout_ms);

    // 停止接受新连接
    if (listener_) {
        evconnlistener_disable(listener_);
        SPDLOG_INFO("[Gate]Stopped accepting new connections");
    }

    // 向所有 Game Server 发送 MSG_ID_SHUTDOWN
    AdminShutdownMsg forward_msg;
    forward_msg.reason = msg.reason;
    forward_msg.timeout_ms = msg.timeout_ms;
    std::string forward_payload = forward_msg.serialize();

    {
        std::lock_guard<std::mutex> lock(game_conns_mutex_);
        for (auto& [server_id, conn] : game_conns_) {
            if (conn->is_identified()) {
                conn->send(MSG_ID_SHUTDOWN, forward_payload);
                SPDLOG_INFO("[Gate]Forwarded shutdown to Game Server {}", server_id);
            }
        }
    }

    // 等待 Game Server 响应（简化实现：直接退出）
    // 实际实现应该等待 MSG_ID_SHUTDOWN_RESP 或超时
    SPDLOG_INFO("[Gate]Shutdown initiated, stopping server...");

    // 发送响应给调用方
    AdminShutdownResp resp;
    resp.code = 0;
    resp.msg = "GateServer shutting down";
    std::string resp_payload = resp.serialize();

    // 广播给所有客户端（可选）
    // 这里简化处理，直接停止服务器
    stop();
}

void GateServer::handle_shutdown_resp(const AdminShutdownResp& resp) {
    SPDLOG_INFO("[Gate]Received shutdown response: code={}, msg={}", resp.code, resp.msg);

    // 如果所有 Game Server 都响应了就绪，可以安全退出
    if (resp.code == 0) {
        SPDLOG_INFO("[Gate]Game Server ready to shutdown");
    }
}

}  // namespace farm
