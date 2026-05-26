#include "dbmgr_server.h"
#include "internal_msg_ids.h"
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

#include "dbmgr.pb.h"
#include "account.pb.h"

namespace farm {

DbMgrServer::DbMgrServer(uint32_t index, const std::string& ip, uint16_t port, const std::string& data_dir)
    : index_(index)
    , ip_(ip)
    , port_(port)
    , data_dir_(data_dir)
    , base_(nullptr)
    , listener_(nullptr)
    , heartbeat_timer_(nullptr)
    , running_(false)
    , data_mgr_(data_dir)
{
}

DbMgrServer::~DbMgrServer() {
    stop();
}

bool DbMgrServer::start() {
    // 初始化数据目录
    if (!data_mgr_.init()) {
        std::cerr << "[DbMgrServer] Failed to initialize data directory" << std::endl;
        return false;
    }

    base_ = event_base_new();
    if (!base_) {
        std::cerr << "[DbMgrServer] Failed to create event_base" << std::endl;
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
        std::cerr << "[DbMgrServer] Failed to create listener on "
                  << ip_ << ":" << port_ << std::endl;
        event_base_free(base_);
        base_ = nullptr;
        return false;
    }

    // 创建心跳定时器（每 5 秒触发一次）
    struct timeval tv;
    tv.tv_sec = HEARTBEAT_INTERVAL;
    tv.tv_usec = 0;
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    evtimer_add(heartbeat_timer_, &tv);

    running_ = true;
    std::cout << "[DbMgrServer] DBMgr index=" << index_
              << " listening on " << ip_ << ":" << port_
              << " data_dir=" << data_dir_ << std::endl;

    // 进入事件循环（阻塞）
    event_base_dispatch(base_);

    running_ = false;
    return true;
}

void DbMgrServer::stop() {
    running_ = false;
    if (heartbeat_timer_) {
        event_free(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
    if (listener_) {
        evconnlistener_free(listener_);
        listener_ = nullptr;
    }
    // 清理所有 Game 会话
    game_sessions_.clear();
    if (base_) {
        event_base_loopexit(base_, nullptr);
        event_base_free(base_);
        base_ = nullptr;
    }
}

// ===========================================
// libevent 回调
// ===========================================

void DbMgrServer::on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                            struct sockaddr* addr, int len, void* ctx) {
    auto* server = static_cast<DbMgrServer*>(ctx);
    server->handle_accept(fd, addr);
}

void DbMgrServer::handle_accept(evutil_socket_t fd, struct sockaddr* addr) {
    // 创建 bufferevent
    struct bufferevent* bev = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        std::cerr << "[DbMgrServer] Failed to create bufferevent for fd=" << fd << std::endl;
        evutil_closesocket(fd);
        return;
    }

    // 创建 GameSession
    auto session = std::make_shared<GameSession>(fd, bev);
    game_sessions_[fd] = session;

    // 设置回调
    bufferevent_setcb(bev, on_read, nullptr, on_event, this);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    // 打印客户端地址
    char ip_str[INET_ADDRSTRLEN] = {0};
    if (addr->sa_family == AF_INET) {
        auto* sin = reinterpret_cast<struct sockaddr_in*>(addr);
        inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));
        std::cout << "[DbMgrServer] New Game connection from "
                  << ip_str << ":" << ntohs(sin->sin_port)
                  << " fd=" << fd << std::endl;
    }

    // 发送 DBMgrIdentify 消息
    farm::DBMgrIdentify identify;
    identify.set_index(index_);
    identify.set_address(ip_ + ":" + std::to_string(port_));
    std::string identify_data;
    identify.SerializeToString(&identify_data);

    send_to_game(session, MSG_ID_DBMGR_IDENTIFY, identify_data);
    std::cout << "[DbMgrServer] Sent DBMgrIdentify to fd=" << fd
              << " index=" << index_ << std::endl;
}

void DbMgrServer::on_read(struct bufferevent* bev, void* ctx) {
    auto* server = static_cast<DbMgrServer*>(ctx);
    evutil_socket_t fd = bufferevent_getfd(bev);
    auto it = server->game_sessions_.find(fd);
    if (it == server->game_sessions_.end()) {
        return;
    }
    server->handle_read(it->second);
}

void DbMgrServer::handle_read(std::shared_ptr<GameSession> session) {
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

void DbMgrServer::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* server = static_cast<DbMgrServer*>(ctx);
    evutil_socket_t fd = bufferevent_getfd(bev);
    auto it = server->game_sessions_.find(fd);

    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        if (it != server->game_sessions_.end()) {
            server->handle_disconnect(it->second);
        }
    }
}

void DbMgrServer::handle_disconnect(std::shared_ptr<GameSession> session) {
    evutil_socket_t fd = session->fd();
    std::cout << "[DbMgrServer] Game disconnected fd=" << fd << std::endl;

    // 移除会话
    game_sessions_.erase(fd);
}

void DbMgrServer::on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* server = static_cast<DbMgrServer*>(ctx);
    server->check_heartbeat();
}

void DbMgrServer::check_heartbeat() {
    time_t now = std::time(nullptr);
    std::vector<evutil_socket_t> timeout_fds;

    for (auto& kv : game_sessions_) {
        auto& session = kv.second;
        if (session->state() == GameSessionState::DISCONNECTED) continue;

        // 只对已识别的连接发送心跳
        if (session->state() == GameSessionState::IDENTIFIED) {
            send_heartbeat(session);
        }

        // 检查心跳超时
        if (now - session->last_heartbeat() > HEARTBEAT_TIMEOUT) {
            std::cout << "[DbMgrServer] Heartbeat timeout fd=" << session->fd() << std::endl;
            timeout_fds.push_back(kv.first);
        }
    }

    for (evutil_socket_t fd : timeout_fds) {
        auto it = game_sessions_.find(fd);
        if (it != game_sessions_.end()) {
            handle_disconnect(it->second);
        }
    }
}

void DbMgrServer::send_heartbeat(std::shared_ptr<GameSession> session) {
    farm::DBMgrHeartbeat hb;
    hb.set_timestamp(static_cast<uint64_t>(std::time(nullptr)));
    std::string hb_data;
    hb.SerializeToString(&hb_data);

    send_to_game(session, MSG_ID_DBMGR_HEARTBEAT, hb_data);
}

// ===========================================
// 消息路由
// ===========================================

void DbMgrServer::route_message(std::shared_ptr<GameSession> session,
                                uint32_t msg_id,
                                const std::vector<uint8_t>& payload) {
    // 管理消息（5000-5999）可以在任何状态下处理
    if (msg_id >= 5000 && msg_id < 6000) {
        handle_admin_message(session, msg_id, payload);
        return;
    }

    // 身份识别前只允许 DBMgr_IDENTIFY_RESP 消息
    if (session->state() == GameSessionState::CONNECTED) {
        if (msg_id == MSG_ID_DBMGR_IDENTIFY_RESP) {
            handle_dbmgr_identify_resp(session, payload);
        } else {
            std::cout << "[DbMgrServer] Ignoring msg_id=" << msg_id
                      << " from unidentified Game fd=" << session->fd() << std::endl;
        }
        return;
    }

    // 已识别的 Game 处理所有消息
    switch (msg_id) {
        case MSG_ID_DBMGR_HEARTBEAT_RESP:
            handle_dbmgr_heartbeat_resp(session, payload);
            break;
        case MSG_ID_PLAYER_DATA_REQ:
            handle_player_data_req(session, payload);
            break;
        case MSG_ID_ACCOUNT_DATA_REQ:
            handle_account_data_req(session, payload);
            break;
        case MSG_ID_ACCOUNT_SET_REQ:
            handle_account_set_req(session, payload);
            break;
        default:
            std::cout << "[DbMgrServer] Unknown msg_id=" << msg_id
                      << " from fd=" << session->fd() << std::endl;
            break;
    }
}

// ===========================================
// 具体消息处理
// ===========================================

void DbMgrServer::handle_dbmgr_identify_resp(std::shared_ptr<GameSession> session,
                                              const std::vector<uint8_t>& payload) {
    farm::DBMgrIdentifyResp resp;
    if (!payload.empty()) {
        resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    if (resp.code() == 0) {
        session->set_state(GameSessionState::IDENTIFIED);
        session->update_heartbeat();
        std::cout << "[DbMgrServer] Game identified successfully fd=" << session->fd() << std::endl;
    } else {
        std::cerr << "[DbMgrServer] Game identify failed fd=" << session->fd()
                  << " code=" << resp.code() << " msg=" << resp.msg() << std::endl;
    }
}

void DbMgrServer::handle_dbmgr_heartbeat_resp(std::shared_ptr<GameSession> session,
                                               const std::vector<uint8_t>& payload) {
    session->update_heartbeat();
}

void DbMgrServer::handle_player_data_req(std::shared_ptr<GameSession> session,
                                          const std::vector<uint8_t>& payload) {
    farm::PlayerDataReq req;
    if (!payload.empty()) {
        req.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    uint64_t request_id = req.request_id();
    uint64_t player_id = req.player_id();
    PlayerDataOp op = req.op();
    const std::string& key = req.key();
    const std::string& value = req.value();

    std::cout << "[DbMgrServer] PlayerDataReq request_id=" << request_id
              << " player_id=" << player_id
              << " op=" << static_cast<int>(op)
              << " key=" << key << std::endl;

    // 构造响应
    farm::PlayerDataResp resp;
    resp.set_request_id(request_id);

    std::vector<uint8_t> result_value;
    DataResult result;

    switch (op) {
        case PlayerDataOp::GET_ALL:
            result = data_mgr_.get_all(player_id, result_value);
            break;
        case PlayerDataOp::GET:
            result = data_mgr_.get(player_id, key, result_value);
            break;
        case PlayerDataOp::SET_ALL:
            result = data_mgr_.set_all(player_id, std::vector<uint8_t>(value.begin(), value.end()));
            break;
        case PlayerDataOp::SET:
            result = data_mgr_.set(player_id, key, std::vector<uint8_t>(value.begin(), value.end()));
            break;
        case PlayerDataOp::DEL:
            result = data_mgr_.del(player_id, key);
            break;
        default:
            std::cerr << "[DbMgrServer] Unknown op=" << static_cast<int>(op) << std::endl;
            result = DataResult::PARSE_ERROR;
            break;
    }

    resp.set_code(static_cast<int32_t>(result));
    if (!result_value.empty()) {
        resp.set_value(result_value.data(), result_value.size());
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_game(session, MSG_ID_PLAYER_DATA_RESP, resp_data);

    std::cout << "[DbMgrServer] PlayerDataResp request_id=" << request_id
              << " code=" << static_cast<int32_t>(result)
              << " value_size=" << result_value.size() << std::endl;
}

void DbMgrServer::handle_account_data_req(std::shared_ptr<GameSession> session,
                                           const std::vector<uint8_t>& payload) {
    farm::AccountDataReq req;
    if (!payload.empty()) {
        req.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    const std::string& account_id = req.account_id();

    std::cout << "[DbMgrServer] AccountDataReq account_id=" << account_id << std::endl;

    // 构造响应
    farm::AccountDataResp resp;

    std::vector<AccountRole> roles;
    AccountResult result = data_mgr_.get_account(account_id, roles);

    resp.set_code(static_cast<int32_t>(result));
    if (result == AccountResult::SUCCESS) {
        for (const auto& role : roles) {
            auto* role_info = resp.add_roles();
            role_info->set_server_id(role.server_id);
            role_info->set_player_id(role.player_id);
            role_info->set_role_name(role.role_name);
        }
    } else {
        resp.set_msg("Failed to get account data");
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_game(session, MSG_ID_ACCOUNT_DATA_RESP, resp_data);

    std::cout << "[DbMgrServer] AccountDataResp account_id=" << account_id
              << " code=" << static_cast<int32_t>(result)
              << " roles_count=" << roles.size() << std::endl;
}

void DbMgrServer::handle_account_set_req(std::shared_ptr<GameSession> session,
                                          const std::vector<uint8_t>& payload) {
    farm::AccountSetReq req;
    if (!payload.empty()) {
        req.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    const std::string& account_id = req.account_id();
    const auto& new_role = req.new_role();

    std::cout << "[DbMgrServer] AccountSetReq account_id=" << account_id
              << " server_id=" << new_role.server_id()
              << " player_id=" << new_role.player_id()
              << " role_name=" << new_role.role_name() << std::endl;

    // 构造响应
    farm::AccountSetResp resp;

    AccountRole role;
    role.server_id = new_role.server_id();
    role.player_id = new_role.player_id();
    role.role_name = new_role.role_name();

    AccountResult result = data_mgr_.set_account(account_id, role);

    resp.set_code(static_cast<int32_t>(result));
    if (result == AccountResult::ROLE_ALREADY_EXISTS) {
        resp.set_msg("角色已存在");
    } else if (result != AccountResult::SUCCESS) {
        resp.set_msg("Failed to set account data");
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_game(session, MSG_ID_ACCOUNT_SET_RESP, resp_data);

    std::cout << "[DbMgrServer] AccountSetResp account_id=" << account_id
              << " code=" << static_cast<int32_t>(result) << std::endl;
}

// ===========================================
// 发送消息辅助
// ===========================================

void DbMgrServer::send_to_game(std::shared_ptr<GameSession> session,
                               uint32_t msg_id, const std::string& payload) {
    auto packed = MessageParser::pack(msg_id, payload);
    bufferevent_write(session->bev(), packed.data(), packed.size());
}

// ===========================================
// 管理消息处理
// ===========================================

void DbMgrServer::handle_admin_message(std::shared_ptr<GameSession> session,
                                       uint32_t msg_id, const std::vector<uint8_t>& payload) {
    std::string payload_str(payload.begin(), payload.end());

    switch (msg_id) {
        case MSG_ID_SHUTDOWN: {
            AdminShutdownMsg msg;
            if (AdminShutdownMsg::deserialize(payload_str, msg)) {
                handle_shutdown(session, msg);
            } else {
                std::cerr << "[DbMgrServer] Failed to parse MSG_ID_SHUTDOWN" << std::endl;
            }
            break;
        }
        case MSG_ID_SHUTDOWN_RESP: {
            AdminShutdownResp resp;
            if (AdminShutdownResp::deserialize(payload_str, resp)) {
                handle_shutdown_resp(session, resp);
            } else {
                std::cerr << "[DbMgrServer] Failed to parse MSG_ID_SHUTDOWN_RESP" << std::endl;
            }
            break;
        }
        default:
            std::cout << "[DbMgrServer] Unknown admin msg_id=" << msg_id << std::endl;
            break;
    }
}

void DbMgrServer::handle_shutdown(std::shared_ptr<GameSession> session, const AdminShutdownMsg& msg) {
    std::cout << "[DbMgrServer] Received shutdown request: reason=" << msg.reason
              << ", timeout_ms=" << msg.timeout_ms << std::endl;

    // 停止接受新连接
    if (listener_) {
        evconnlistener_disable(listener_);
        std::cout << "[DbMgrServer] Stopped accepting new connections" << std::endl;
    }

    // DataManager 基于文件系统，每次写入都会直接写入文件，无需额外 flush
    std::cout << "[DbMgrServer] Data is already persisted to disk" << std::endl;

    // 发送响应给 Game
    AdminShutdownResp resp;
    resp.code = 0;
    resp.msg = "DBMgr shutting down";
    std::string resp_payload = resp.serialize();

    send_to_game(session, MSG_ID_SHUTDOWN_RESP, resp_payload);
    std::cout << "[DbMgrServer] Sent shutdown response to Game" << std::endl;

    // 退出服务器
    std::cout << "[DbMgrServer] Shutdown initiated, stopping server..." << std::endl;
    stop();
}

void DbMgrServer::handle_shutdown_resp(std::shared_ptr<GameSession> session, const AdminShutdownResp& resp) {
    std::cout << "[DbMgrServer] Received shutdown response: code=" << resp.code
              << ", msg=" << resp.msg << std::endl;
}

}  // namespace farm
