#include "gate_server.h"
#include "message_parser.h"
#include "internal_msg_ids.h"
#include "msg_ids.h"
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
#include <iostream>
#include <fstream>
#include <sstream>

#include "base.pb.h"
#include "internal.pb.h"
#include "account.pb.h"

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

    // 连接到所有配置的 Game Server
    if (!game_server_configs_.empty()) {
        for (const auto& config : game_server_configs_) {
            auto conn = std::make_unique<GameConnection>(base_, "gate-1");
            uint32_t server_id = config.server_id;
            conn->set_message_callback(
                [this, server_id](uint32_t msg_id, const std::vector<uint8_t>& payload) {
                    handle_game_message(server_id, msg_id, payload);
                });
            conn->connect(config.ip, config.port);
            game_conns_[server_id] = std::move(conn);
            std::cout << "[GateServer] Connecting to Game Server " << server_id
                      << " at " << config.ip << ":" << config.port << std::endl;
        }
    }

    // 进入事件循环（阻塞）
    event_base_dispatch(base_);

    running_ = false;
    return true;
}

void GateServer::stop() {
    running_ = false;
    // 断开所有 Game 连接
    for (auto& kv : game_conns_) {
        kv.second->disconnect();
    }
    game_conns_.clear();
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
    // 向后兼容：添加为 server_id=1 的配置
    GameServerConfig config;
    config.server_id = 1;
    config.ip = ip;
    config.port = port;
    game_server_configs_.push_back(config);
}

bool GateServer::load_game_servers_config(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "[GateServer] Failed to open config file: " << config_file << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // 简单的 JSON 解析（不依赖外部库）
    // 查找 "game_servers" 数组
    size_t array_start = content.find("[");
    size_t array_end = content.find("]");
    if (array_start == std::string::npos || array_end == std::string::npos) {
        std::cerr << "[GateServer] Invalid config format: missing array" << std::endl;
        return false;
    }

    std::string array_content = content.substr(array_start + 1, array_end - array_start - 1);

    // 解析每个 server 对象
    size_t pos = 0;
    while (pos < array_content.size()) {
        // 查找 server_id
        size_t id_pos = array_content.find("\"server_id\"", pos);
        if (id_pos == std::string::npos) break;

        size_t id_colon = array_content.find(":", id_pos);
        size_t id_end = array_content.find(",", id_colon);
        if (id_end == std::string::npos) id_end = array_content.find("}", id_colon);
        std::string id_str = array_content.substr(id_colon + 1, id_end - id_colon - 1);
        // 去除空格
        id_str.erase(std::remove_if(id_str.begin(), id_str.end(), ::isspace), id_str.end());
        uint32_t server_id = static_cast<uint32_t>(std::stoul(id_str));

        // 查找 ip
        size_t ip_pos = array_content.find("\"ip\"", id_end);
        if (ip_pos == std::string::npos) break;

        size_t ip_colon = array_content.find(":", ip_pos);
        size_t ip_quote_start = array_content.find("\"", ip_colon);
        size_t ip_quote_end = array_content.find("\"", ip_quote_start + 1);
        std::string ip = array_content.substr(ip_quote_start + 1, ip_quote_end - ip_quote_start - 1);

        // 查找 port
        size_t port_pos = array_content.find("\"port\"", ip_quote_end);
        if (port_pos == std::string::npos) break;

        size_t port_colon = array_content.find(":", port_pos);
        size_t port_end = array_content.find(",", port_colon);
        if (port_end == std::string::npos) port_end = array_content.find("}", port_colon);
        std::string port_str = array_content.substr(port_colon + 1, port_end - port_colon - 1);
        port_str.erase(std::remove_if(port_str.begin(), port_str.end(), ::isspace), port_str.end());
        uint16_t port = static_cast<uint16_t>(std::stoul(port_str));

        GameServerConfig config;
        config.server_id = server_id;
        config.ip = ip;
        config.port = port;
        game_server_configs_.push_back(config);

        std::cout << "[GateServer] Loaded game server config: server_id=" << server_id
                  << " ip=" << ip << " port=" << port << std::endl;

        pos = port_end;
    }

    return true;
}

GameConnection* GateServer::get_game_connection(uint32_t server_id) {
    auto it = game_conns_.find(server_id);
    if (it != game_conns_.end() && it->second->is_identified()) {
        return it->second.get();
    }
    return nullptr;
}

GameConnection* GateServer::get_any_game_connection() {
    if (game_conns_.empty()) {
        return nullptr;
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

    return nullptr;
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
        auto it = player_to_server_.find(session->player_id());
        if (it != player_to_server_.end()) {
            GameConnection* conn = get_game_connection(it->second);
            if (conn) {
                farm::PlayerLeave leave;
                leave.set_player_id(session->player_id());
                std::string payload;
                leave.SerializeToString(&payload);
                conn->send(MSG_ID_PLAYER_LEAVE, payload);
                std::cout << "[GateServer] Notified Game " << it->second
                          << ": player_id=" << session->player_id() << " left" << std::endl;
            }
            player_to_server_.erase(it);
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
        if (!payload.empty()) {
            player_msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
        }
        uint32_t server_id = player_msg.server_id();
        forward_player_msg_to_game(session, server_id, msg_id, payload);
        return;
    }

    // 内部消息（3000+）转发到 Game
    if (msg_id >= 3000) {
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
    GameConnection* conn = get_any_game_connection();
    if (conn) {
        farm::PlayerJoin join;
        join.set_player_id(player_id);
        std::string join_payload;
        join.SerializeToString(&join_payload);
        conn->send(MSG_ID_PLAYER_JOIN, join_payload);
        std::cout << "[GateServer] Notified Game: player_id=" << player_id << " joined" << std::endl;
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
            std::cout << "[GateServer] Unknown game msg_id=" << msg_id
                      << " from server_id=" << server_id << std::endl;
            break;
    }
}

void GateServer::handle_game_identify_resp(uint32_t server_id, const std::vector<uint8_t>& payload) {
    farm::GateIdentifyResp resp;
    if (!payload.empty()) {
        resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    if (resp.code() == 0) {
        std::cout << "[GateServer] Game Server " << server_id << " identified successfully" << std::endl;
        // 设置 GameConnection 状态为 IDENTIFIED
        auto it = game_conns_.find(server_id);
        if (it != game_conns_.end()) {
            it->second->set_state(GameConnState::IDENTIFIED);
        }
    } else {
        std::cout << "[GateServer] Game Server " << server_id << " identification failed: " << resp.msg() << std::endl;
    }
}

void GateServer::handle_game_heartbeat_resp(uint32_t server_id, const std::vector<uint8_t>& payload) {
    // 心跳响应，GameConnection 内部已处理
}

void GateServer::handle_player_join_resp(uint32_t server_id, const std::vector<uint8_t>& payload) {
    farm::PlayerJoinResp resp;
    if (!payload.empty()) {
        resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    if (resp.code() == 0) {
        std::cout << "[GateServer] Player " << resp.player_id() << " joined Game Server " << server_id << " successfully" << std::endl;
        // 注册玩家路由
        player_to_server_[resp.player_id()] = server_id;
    } else {
        std::cout << "[GateServer] Player " << resp.player_id() << " join Game Server " << server_id << " failed: " << resp.msg() << std::endl;
    }
}

void GateServer::handle_game_msg(uint32_t server_id, const std::vector<uint8_t>& payload) {
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

void GateServer::handle_account_msg_resp(uint32_t server_id, const std::vector<uint8_t>& payload) {
    farm::AccountMessageResp resp;
    if (!payload.empty()) {
        resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    const std::string& account_id = resp.account_id();
    uint32_t msg_id = resp.msg_id();
    const std::string& inner_payload = resp.payload();

    // 查找客户端 Session（通过 account_id）
    auto session = session_mgr_.find_by_account_id(account_id);
    if (!session) {
        std::cout << "[GateServer] Warning: account_id=" << account_id
                  << " not found, discarding ACCOUNT_MSG_RESP" << std::endl;
        return;
    }

    // 发送给客户端
    auto packed = MessageParser::pack(msg_id, inner_payload);
    bufferevent_write(session->bev(), packed.data(), packed.size());
}

void GateServer::forward_to_game(std::shared_ptr<Session> session, uint32_t msg_id,
                                  const std::vector<uint8_t>& payload) {
    // 获取任意 Game 连接
    GameConnection* conn = get_any_game_connection();
    if (!conn) {
        std::cout << "[GateServer] Warning: No Game connected, discarding msg_id="
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

    conn->send(MSG_ID_CLIENT_MSG, packed_payload);
}

void GateServer::forward_account_msg_to_game(std::shared_ptr<Session> session, uint32_t msg_id,
                                              const std::vector<uint8_t>& payload) {
    // 解析 AccountMsg
    farm::AccountMsg account_msg;
    if (!payload.empty()) {
        account_msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    const std::string& account_id = account_msg.account_id();
    uint32_t inner_msg_id = account_msg.msg_id();
    const std::string& inner_payload = account_msg.payload();

    // 绑定 account_id 到 session（用于响应路由）
    session->set_account_id(account_id);

    // 获取任意 Game 连接
    GameConnection* conn = get_any_game_connection();
    if (!conn) {
        std::cout << "[GateServer] Warning: No Game connected, discarding AccountMsg from account_id="
                  << account_id << std::endl;
        return;
    }

    // 打包为 AccountMessage（内部协议）
    farm::AccountMessage internal_msg;
    internal_msg.set_account_id(account_id);
    internal_msg.set_msg_id(inner_msg_id);
    internal_msg.set_payload(inner_payload);

    std::string packed_payload;
    internal_msg.SerializeToString(&packed_payload);

    conn->send(MSG_ID_ACCOUNT_MSG, packed_payload);
}

void GateServer::forward_player_msg_to_game(std::shared_ptr<Session> session, uint32_t server_id,
                                             uint32_t msg_id, const std::vector<uint8_t>& payload) {
    // 获取指定 server_id 的 Game 连接
    GameConnection* conn = get_game_connection(server_id);
    if (!conn) {
        std::cout << "[GateServer] Warning: Game Server " << server_id
                  << " not connected, discarding PlayerMsg from player_id=" << session->player_id() << std::endl;
        return;
    }

    // 打包为 ClientMessage
    farm::ClientMessage client_msg;
    client_msg.set_player_id(session->player_id());
    client_msg.set_msg_id(msg_id);
    client_msg.set_payload(payload.data(), payload.size());

    std::string packed_payload;
    client_msg.SerializeToString(&packed_payload);

    conn->send(MSG_ID_CLIENT_MSG, packed_payload);
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
                std::cerr << "[GateServer] Failed to parse MSG_ID_SHUTDOWN" << std::endl;
            }
            break;
        }
        case MSG_ID_SHUTDOWN_RESP: {
            AdminShutdownResp resp;
            if (AdminShutdownResp::deserialize(payload_str, resp)) {
                handle_shutdown_resp(resp);
            } else {
                std::cerr << "[GateServer] Failed to parse MSG_ID_SHUTDOWN_RESP" << std::endl;
            }
            break;
        }
        default:
            std::cout << "[GateServer] Unknown admin msg_id=" << msg_id << std::endl;
            break;
    }
}

void GateServer::handle_shutdown(const AdminShutdownMsg& msg) {
    std::cout << "[GateServer] Received shutdown request: reason=" << msg.reason
              << ", timeout_ms=" << msg.timeout_ms << std::endl;

    // 停止接受新连接
    if (listener_) {
        evconnlistener_disable(listener_);
        std::cout << "[GateServer] Stopped accepting new connections" << std::endl;
    }

    // 向所有 Game Server 发送 MSG_ID_SHUTDOWN
    AdminShutdownMsg forward_msg;
    forward_msg.reason = msg.reason;
    forward_msg.timeout_ms = msg.timeout_ms;
    std::string forward_payload = forward_msg.serialize();

    for (auto& kv : game_conns_) {
        if (kv.second->is_identified()) {
            kv.second->send(MSG_ID_SHUTDOWN, forward_payload);
            std::cout << "[GateServer] Forwarded shutdown to Game Server " << kv.first << std::endl;
        }
    }

    // 等待 Game Server 响应（简化实现：直接退出）
    // 实际实现应该等待 MSG_ID_SHUTDOWN_RESP 或超时
    std::cout << "[GateServer] Shutdown initiated, stopping server..." << std::endl;

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
    std::cout << "[GateServer] Received shutdown response: code=" << resp.code
              << ", msg=" << resp.msg << std::endl;

    // 如果所有 Game Server 都响应了就绪，可以安全退出
    if (resp.code == 0) {
        std::cout << "[GateServer] Game Server ready to shutdown" << std::endl;
    }
}

}  // namespace farm
