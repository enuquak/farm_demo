#include "game_server.h"
#include "internal_msg_ids.h"
#include "dbmgr_msg_ids.h"
#include "msg_ids.h"
#include "player_id_generator.h"

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
#include <tuple>

#include "internal.pb.h"
#include "account.pb.h"
#include "player.pb.h"
#include "dbmgr.pb.h"

namespace farm {

GameServer::GameServer(const std::string& ip, uint16_t port,
                       const std::vector<DBMgrConfig>& dbmgr_configs)
    : ip_(ip)
    , port_(port)
    , base_(nullptr)
    , listener_(nullptr)
    , heartbeat_timer_(nullptr)
    , running_(false)
    , dbmgr_configs_(dbmgr_configs)
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

    // 初始化 DBMgr 连接管理器
    if (!dbmgr_configs_.empty()) {
        if (!dbmgr_mgr_.init(base_, dbmgr_configs_)) {
            std::cerr << "[GameServer] Failed to init DBMgrConnectionManager" << std::endl;
        } else {
            std::cout << "[GameServer] DBMgr connections initialized ("
                      << dbmgr_configs_.size() << " DBMgrs)" << std::endl;
            // 设置 PlayerManager 的 DBMgr 连接管理器
            player_mgr_.set_dbmgr_manager(&dbmgr_mgr_);
        }
    } else {
        std::cout << "[GameServer] No DBMgr configs, skipping DBMgr connections" << std::endl;
    }

    // 进入事件循环（阻塞）
    event_base_dispatch(base_);

    running_ = false;
    return true;
}

void GameServer::stop() {
    running_ = false;
    // Shutdown DBMgr connections first
    dbmgr_mgr_.shutdown();
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

    // 清理该 Gate 关联的所有玩家（并保存数据）
    player_mgr_.remove_players_by_gate_with_save(session.get());

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
        case MSG_ID_ACCOUNT_MSG:
            handle_account_msg(session, payload);
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

    // 创建回调函数，用于数据加载完成后发送响应
    auto callback = [this, session, player_id](uint64_t pid, bool success, const std::string& msg) {
        handle_player_join_callback(session, pid, success, msg);
    };

    bool added = player_mgr_.add_player_with_data_load(player_id, session.get(), std::move(callback));
    if (!added) {
        // 玩家已存在，直接回复失败
        farm::PlayerJoinResp resp;
        resp.set_player_id(player_id);
        resp.set_code(1);
        resp.set_msg("already joined");

        std::string resp_data;
        resp.SerializeToString(&resp_data);
        send_to_gate(session, MSG_ID_PLAYER_JOIN_RESP, resp_data);

        std::cout << "[GameServer] Player " << player_id << " already exists" << std::endl;
    }
    // 注意：成功添加的情况下，响应会在 handle_player_join_callback 中发送
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

    // 保存数据并移除玩家
    player_mgr_.remove_player_with_save(player_id);
}

void GameServer::handle_player_join_callback(std::shared_ptr<GateSession> session,
                                             uint64_t player_id, bool success,
                                             const std::string& msg) {
    std::cout << "[GameServer] Player join callback: player_id=" << player_id
              << " success=" << success << " msg=" << msg << std::endl;

    farm::PlayerJoinResp resp;
    resp.set_player_id(player_id);

    if (success) {
        resp.set_code(0);
        resp.set_msg("joined");
        std::cout << "[GameServer] Player " << player_id << " joined successfully" << std::endl;
    } else {
        resp.set_code(1);
        resp.set_msg(msg);
        std::cerr << "[GameServer] Player " << player_id << " join failed: " << msg << std::endl;
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_gate(session, MSG_ID_PLAYER_JOIN_RESP, resp_data);
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

    // 处理 EnterGameReq
    if (msg_id == MSG_ID_ENTER_GAME_REQ) {
        handle_enter_game_req(session, player_id, inner_payload);
        return;
    }

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

void GameServer::handle_enter_game_req(std::shared_ptr<GateSession> session,
                                        uint64_t player_id, const std::string& payload) {
    farm::EnterGameReq req;
    if (!payload.empty()) {
        req.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    uint64_t req_player_id = req.player_id();
    uint32_t server_id = req.server_id();

    std::cout << "[GameServer] EnterGameReq: player_id=" << req_player_id
              << " server_id=" << server_id << std::endl;

    // 检查 DBMgr 是否可用
    if (!dbmgr_mgr_.is_connected(0)) {
        std::cerr << "[GameServer] DBMgr not available for EnterGameReq, player_id=" << req_player_id << std::endl;

        // 回复失败
        farm::GameMessage game_msg;
        game_msg.set_player_id(req_player_id);
        game_msg.set_msg_id(MSG_ID_ENTER_GAME_RESP);

        farm::EnterGameResp resp;
        resp.set_code(-1);
        resp.set_msg("DBMgr not available");

        std::string resp_data;
        resp.SerializeToString(&resp_data);
        game_msg.set_payload(resp_data);

        std::string final_resp;
        game_msg.SerializeToString(&final_resp);

        send_to_gate(session, MSG_ID_GAME_MSG, final_resp);
        return;
    }

    // 使用 PlayerManager 创建玩家并加载数据
    auto callback = [this, session, req_player_id](uint64_t pid, bool success, const std::string& msg) {
        // 构造响应
        farm::GameMessage game_msg;
        game_msg.set_player_id(req_player_id);
        game_msg.set_msg_id(MSG_ID_ENTER_GAME_RESP);

        farm::EnterGameResp resp;
        if (success) {
            // 获取玩家数据
            Player* player = player_mgr_.get_player(pid);
            if (player) {
                const PlayerBizData& data = player->player_data();
                farm::PlayerData player_data;
                player_data.set_player_id(pid);
                player_data.set_role_name(data.role_name);
                player_data.set_level(data.level);
                player_data.set_exp(data.experience);
                player_data.set_pos_x(data.pos_x);
                player_data.set_pos_y(data.pos_y);
                player_data.set_pos_z(data.pos_z);
                player_data.set_scene_id(data.scene_id);
                *resp.mutable_player_data() = player_data;
                resp.set_code(0);
                resp.set_msg("success");
            } else {
                resp.set_code(-1);
                resp.set_msg("Player not found after data load");
            }
        } else {
            resp.set_code(-1);
            resp.set_msg(msg);
        }

        std::string resp_data;
        resp.SerializeToString(&resp_data);
        game_msg.set_payload(resp_data);

        std::string final_resp;
        game_msg.SerializeToString(&final_resp);

        send_to_gate(session, MSG_ID_GAME_MSG, final_resp);
    };

    bool added = player_mgr_.add_player_with_data_load(req_player_id, session.get(), std::move(callback));
    if (!added) {
        // 玩家已存在，直接回复失败
        farm::GameMessage game_msg;
        game_msg.set_player_id(req_player_id);
        game_msg.set_msg_id(MSG_ID_ENTER_GAME_RESP);

        farm::EnterGameResp resp;
        resp.set_code(1);
        resp.set_msg("Player already in game");

        std::string resp_data;
        resp.SerializeToString(&resp_data);
        game_msg.set_payload(resp_data);

        std::string final_resp;
        game_msg.SerializeToString(&final_resp);

        send_to_gate(session, MSG_ID_GAME_MSG, final_resp);
    }
}

void GameServer::handle_account_msg(std::shared_ptr<GateSession> session,
                                    const std::vector<uint8_t>& payload) {
    farm::AccountMessage req;
    if (!payload.empty()) {
        req.ParseFromArray(payload.data(), static_cast<int>(payload.size()));
    }

    const std::string& account_id = req.account_id();
    uint32_t msg_id = req.msg_id();
    const std::string& inner_payload = req.payload();

    std::cout << "[GameServer] Account message: account_id=" << account_id
              << " msg_id=" << msg_id << std::endl;

    // 根据 msg_id 处理不同的账号消息
    if (msg_id == MSG_ID_QUERY_ROLES_REQ) {
        // 查询角色列表
        dbmgr_mgr_.send_account_data_req(account_id,
            [this, session, account_id](int32_t code, const std::vector<std::tuple<uint32_t, uint64_t, std::string>>& roles) {
                // 构造响应
                farm::AccountMessageResp resp;
                resp.set_account_id(account_id);
                resp.set_msg_id(MSG_ID_QUERY_ROLES_RESP);

                // 构造 QueryRolesResp
                farm::QueryRolesResp roles_resp;
                roles_resp.set_code(code);
                if (code == 0) {
                    for (const auto& role : roles) {
                        auto* r = roles_resp.add_roles();
                        r->set_server_id(std::get<0>(role));
                        r->set_player_id(std::get<1>(role));
                        r->set_role_name(std::get<2>(role));
                    }
                } else {
                    roles_resp.set_msg("Failed to query roles");
                }

                std::string resp_data;
                roles_resp.SerializeToString(&resp_data);
                resp.set_payload(resp_data);

                std::string final_resp;
                resp.SerializeToString(&final_resp);

                send_to_gate(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
            });
    } else if (msg_id == MSG_ID_CREATE_ROLE_REQ) {
        // 创建角色
        farm::CreateRoleReq create_req;
        if (!inner_payload.empty()) {
            create_req.ParseFromArray(inner_payload.data(), static_cast<int>(inner_payload.size()));
        }

        uint32_t server_id = create_req.server_id();
        const std::string& role_name = create_req.role_name();

        // 生成 player_id
        uint64_t player_id = player_id_gen_.generate(server_id);

        dbmgr_mgr_.send_account_set_req(account_id,
            server_id, player_id, role_name,
            [this, session, account_id, player_id](int32_t code, const std::string& msg) {
                // 构造响应
                farm::AccountMessageResp resp;
                resp.set_account_id(account_id);
                resp.set_msg_id(MSG_ID_CREATE_ROLE_RESP);

                // 构造 CreateRoleResp
                farm::CreateRoleResp create_resp;
                create_resp.set_code(code);
                create_resp.set_msg(msg);
                if (code == 0) {
                    create_resp.set_player_id(player_id);
                }

                std::string resp_data;
                create_resp.SerializeToString(&resp_data);
                resp.set_payload(resp_data);

                std::string final_resp;
                resp.SerializeToString(&final_resp);

                send_to_gate(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
            });
    } else if (msg_id == MSG_ID_ACCOUNT_DATA_REQ) {
        // 查询账号数据（内部使用）
        dbmgr_mgr_.send_account_data_req(account_id,
            [this, session, account_id](int32_t code, const std::vector<std::tuple<uint32_t, uint64_t, std::string>>& roles) {
                // 构造响应
                farm::AccountMessageResp resp;
                resp.set_account_id(account_id);
                resp.set_msg_id(MSG_ID_ACCOUNT_DATA_RESP);

                // 构造 AccountDataResp
                farm::AccountDataResp data_resp;
                data_resp.set_code(code);
                if (code == 0) {
                    for (const auto& role : roles) {
                        auto* r = data_resp.add_roles();
                        r->set_server_id(std::get<0>(role));
                        r->set_player_id(std::get<1>(role));
                        r->set_role_name(std::get<2>(role));
                    }
                } else {
                    data_resp.set_msg("Failed to get account data");
                }

                std::string resp_data;
                data_resp.SerializeToString(&resp_data);
                resp.set_payload(resp_data);

                std::string final_resp;
                resp.SerializeToString(&final_resp);

                send_to_gate(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
            });
    } else if (msg_id == MSG_ID_ACCOUNT_SET_REQ) {
        // 设置账号数据（内部使用）
        farm::AccountSetReq set_req;
        if (!inner_payload.empty()) {
            set_req.ParseFromArray(inner_payload.data(), static_cast<int>(inner_payload.size()));
        }

        const auto& new_role = set_req.new_role();
        dbmgr_mgr_.send_account_set_req(account_id,
            new_role.server_id(), new_role.player_id(), new_role.role_name(),
            [this, session, account_id](int32_t code, const std::string& msg) {
                // 构造响应
                farm::AccountMessageResp resp;
                resp.set_account_id(account_id);
                resp.set_msg_id(MSG_ID_ACCOUNT_SET_RESP);

                // 构造 AccountSetResp
                farm::AccountSetResp set_resp;
                set_resp.set_code(code);
                set_resp.set_msg(msg);

                std::string resp_data;
                set_resp.SerializeToString(&resp_data);
                resp.set_payload(resp_data);

                std::string final_resp;
                resp.SerializeToString(&final_resp);

                send_to_gate(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
            });
    } else if (msg_id == MSG_ID_ENTER_GAME_REQ) {
        // 进入游戏（通过账号消息通道）
        farm::EnterGameReq enter_req;
        if (!inner_payload.empty()) {
            enter_req.ParseFromArray(inner_payload.data(), static_cast<int>(inner_payload.size()));
        }

        uint64_t req_player_id = enter_req.player_id();
        uint32_t server_id = enter_req.server_id();

        std::cout << "[GameServer] EnterGameReq (account): player_id=" << req_player_id
                  << " server_id=" << server_id << std::endl;

        // 检查 DBMgr 是否可用
        if (!dbmgr_mgr_.is_connected(0)) {
            std::cerr << "[GameServer] DBMgr not available for EnterGameReq (account), player_id=" << req_player_id << std::endl;

            // 回复失败
            farm::AccountMessageResp resp;
            resp.set_account_id(account_id);
            resp.set_msg_id(MSG_ID_ENTER_GAME_RESP);

            farm::EnterGameResp enter_resp;
            enter_resp.set_code(-1);
            enter_resp.set_msg("DBMgr not available");

            std::string resp_data;
            enter_resp.SerializeToString(&resp_data);
            resp.set_payload(resp_data);

            std::string final_resp;
            resp.SerializeToString(&final_resp);

            send_to_gate(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
            return;
        }

        // 使用 PlayerManager 创建玩家并加载数据
        auto callback = [this, session, account_id, req_player_id](uint64_t pid, bool success, const std::string& msg) {
            // 构造响应
            farm::AccountMessageResp resp;
            resp.set_account_id(account_id);
            resp.set_msg_id(MSG_ID_ENTER_GAME_RESP);

            farm::EnterGameResp enter_resp;
            if (success) {
                // 获取玩家数据
                Player* player = player_mgr_.get_player(pid);
                if (player) {
                    const PlayerBizData& data = player->player_data();
                    farm::PlayerData player_data;
                    player_data.set_player_id(pid);
                    player_data.set_role_name(data.role_name);
                    player_data.set_level(data.level);
                    player_data.set_exp(data.experience);
                    player_data.set_pos_x(data.pos_x);
                    player_data.set_pos_y(data.pos_y);
                    player_data.set_pos_z(data.pos_z);
                    player_data.set_scene_id(data.scene_id);
                    *enter_resp.mutable_player_data() = player_data;
                    enter_resp.set_code(0);
                    enter_resp.set_msg("success");
                } else {
                    enter_resp.set_code(-1);
                    enter_resp.set_msg("Player not found after data load");
                }
            } else {
                enter_resp.set_code(-1);
                enter_resp.set_msg(msg);
            }

            std::string resp_data;
            enter_resp.SerializeToString(&resp_data);
            resp.set_payload(resp_data);

            std::string final_resp;
            resp.SerializeToString(&final_resp);

            send_to_gate(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
        };

        bool added = player_mgr_.add_player_with_data_load(req_player_id, session.get(), std::move(callback));
        if (!added) {
            // 玩家已存在，直接回复失败
            farm::AccountMessageResp resp;
            resp.set_account_id(account_id);
            resp.set_msg_id(MSG_ID_ENTER_GAME_RESP);

            farm::EnterGameResp enter_resp;
            enter_resp.set_code(1);
            enter_resp.set_msg("Player already in game");

            std::string resp_data;
            enter_resp.SerializeToString(&resp_data);
            resp.set_payload(resp_data);

            std::string final_resp;
            resp.SerializeToString(&final_resp);

            send_to_gate(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
        }
    } else {
        std::cout << "[GameServer] Unknown account msg_id=" << msg_id << std::endl;
    }
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
