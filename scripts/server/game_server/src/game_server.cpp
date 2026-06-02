#include "game_server.h"
#include "game_scene_manager.h"
#include "game_clock.h"
#include "admin_handler.h"
#include "internal_msg_ids.h"
#include "message_ids.h"
#include "admin_msg_ids.h"
#include "game_constants.h"

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
#include <cmath>
#include <algorithm>
#include <ctime>
#include <tuple>

#include "internal.pb.h"
#include "account.pb.h"
#include "player.pb.h"
#include "dbmgr.pb.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>

namespace farm {

GameServer::GameServer(const std::string& ip, uint16_t port,
                       const std::vector<DBMgrConfig>& dbmgr_configs,
                       const std::string& redis_uri,
                       uint32_t server_id,
                       uint16_t gm_http_port,
                       const std::string& gm_static_dir)
    : ip_(ip)
    , port_(port)
    , base_(nullptr)
    , listener_(nullptr)
    , heartbeat_timer_(nullptr)
    , update_timer_(nullptr)
    , running_(false)
    , dbmgr_configs_(dbmgr_configs)
    , redis_uri_(redis_uri)
    , server_id_(server_id)
    , item_handler_()
    , gm_http_port_(gm_http_port)
    , gm_static_dir_(gm_static_dir)
{
}

GameServer::~GameServer() {
    stop();
}

bool GameServer::start() {
    base_ = event_base_new();
    if (!base_) {
        SPDLOG_ERROR("[Game]Failed to create event_base");
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
        SPDLOG_ERROR("[Game]Failed to create listener on {}:{}", ip_, port_);
        event_base_free(base_);
        base_ = nullptr;
        return false;
    }

    // 创建心跳定时器
    struct timeval tv;
    tv.tv_sec = GATE_HEARTBEAT_TIMEOUT;
    tv.tv_usec = 0;
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    evtimer_add(heartbeat_timer_, &tv);

    // 创建游戏逻辑更新定时器（每秒执行一次，用于作物生长、自动拾取等）
    struct timeval update_tv;
    update_tv.tv_sec = 1;
    update_tv.tv_usec = 0;
    update_timer_ = event_new(base_, -1, EV_PERSIST, on_update_timer, this);
    evtimer_add(update_timer_, &update_tv);

    running_ = true;
    SPDLOG_INFO("[Game]Listening on {}:{}", ip_, port_);

    // 初始化 DBMgr 连接管理器
    if (!dbmgr_configs_.empty()) {
        if (!dbmgr_mgr_.init(base_, dbmgr_configs_)) {
            SPDLOG_ERROR("[Game]Failed to init DBMgrConnectionManager");
        } else {
            SPDLOG_INFO("[Game]DBMgr connections initialized ({} DBMgrs)", dbmgr_configs_.size());
            // 设置 PlayerManager 的 DBMgr 连接管理器
            player_mgr_.set_dbmgr_manager(&dbmgr_mgr_);
            player_mgr_.set_event_base(base_);
        }
    } else {
        SPDLOG_INFO("[Game]No DBMgr configs, skipping DBMgr connections");
    }

    // 初始化世界状态
    item_handler_.world()->generate_default();
    SPDLOG_INFO("[Game]World state initialized");

    // 创建回调函数（用于解耦子系统与 GameServer）
    auto send_to_gate_func = [this](std::shared_ptr<GateSession> s, uint32_t msg_id, const std::string& payload) {
        send_to_gate(s, msg_id, payload);
    };
    auto send_game_msg_func = [this](uint64_t pid, uint32_t msg_id, const uint8_t* payload, size_t len) {
        send_game_msg(pid, msg_id, payload, len);
    };

    // 初始化提取的子系统
    scene_mgr_ = std::make_unique<GameSceneManager>(&player_mgr_, &dbmgr_mgr_, send_game_msg_func);
    game_clock_ = std::make_unique<GameClock>(&player_mgr_, scene_mgr_.get(), &dbmgr_mgr_, send_game_msg_func);

    // Init quest system
    quest_config_ = std::make_unique<QuestConfig>();
    if (quest_config_->load_from_file("config/quest_data.json")) {
        quest_mgr_ = std::make_unique<QuestManager>(&player_mgr_, quest_config_.get());
        quest_mgr_->init();
        SPDLOG_INFO("[Game]Quest system initialized");
    } else {
        SPDLOG_WARN("[Game]Quest system disabled: failed to load config");
    }

    // Initialize Redis connection
    if (!redis_uri_.empty()) {
        if (redis_conn_.connect(redis_uri_)) {
            SPDLOG_INFO("[Game]Redis connected: {}", redis_uri_);
        } else {
            SPDLOG_WARN("[Game]Redis connection failed: {} (online tracking disabled)", redis_uri_);
        }
    } else {
        SPDLOG_INFO("[Game]No Redis URI configured, online tracking disabled");
    }

    // Initialize stubs
    id_pool_ = std::make_unique<PlayerIdPool>(&dbmgr_mgr_);
    login_stub_ = std::make_unique<LoginStub>(&player_mgr_, &dbmgr_mgr_, &redis_conn_, server_id_, id_pool_.get(), send_to_gate_func);
    login_stub_->set_clock(game_clock_.get());
    online_stub_ = std::make_unique<OnlineStub>(&redis_conn_);
    SPDLOG_INFO("[Game]LoginStub and OnlineStub initialized");

    // Wire up offline callback for Redis cleanup
    player_mgr_.set_offline_callback([this](uint64_t player_id) {
        login_stub_->on_player_offline(player_id);
    });

    admin_handler_ = std::make_unique<AdminHandler>(
        &player_mgr_, scene_mgr_.get(), game_clock_.get(), &dbmgr_mgr_,
        send_to_gate_func,
        [this]() { if (listener_) evconnlistener_disable(listener_); },
        [this]() { stop(); }
    );

    // Initialize GM system
    gm_stub_ = std::make_unique<GMStub>(&player_mgr_, &dbmgr_mgr_);
    gm_stub_->init();

    gm_http_handler_ = std::make_unique<GmHttpHandler>(gm_stub_.get(), &player_mgr_);
    if (!gm_http_handler_->start(base_, gm_http_port_, gm_static_dir_)) {
        SPDLOG_ERROR("[Game]Failed to start GM HTTP server on port {}", gm_http_port_);
    } else {
        SPDLOG_INFO("[Game]GM HTTP server started on port {}", gm_http_port_);
    }

    // 注册物品交互消息处理
    msg_handler_.register_handler(MSG_ID_ITEM_USE_REQ,
        [this](uint64_t player_id, const uint8_t* payload, size_t payload_len) {
            handle_item_use_req(player_id, payload, payload_len);
        });
    SPDLOG_INFO("[Game]Item interaction handler registered");

    // 注册场景切换消息处理
    msg_handler_.register_handler(MSG_ID_SCENE_CHANGE_REQ,
        [this](uint64_t player_id, const uint8_t* payload, size_t payload_len) {
            scene_mgr_->handle_scene_change_req(player_id, payload, payload_len);
        });
    SPDLOG_INFO("[Game]Scene change handler registered");

    // 注册强制睡觉就绪消息处理
    msg_handler_.register_handler(MSG_ID_FORCE_SLEEP_READY,
        [this](uint64_t player_id, const uint8_t* payload, size_t payload_len) {
            game_clock_->handle_force_sleep_ready(player_id, payload, payload_len);
        });
    SPDLOG_INFO("[Game]Force sleep handler registered");

    // 注册位置更新消息处理
    msg_handler_.register_handler(MSG_ID_POSITION_UPDATE,
        [this](uint64_t player_id, const uint8_t* payload, size_t payload_len) {
            handle_position_update(player_id, payload, payload_len);
        });
    SPDLOG_INFO("[Game]Position update handler registered");

    // Quest message handlers
    msg_handler_.register_handler(MSG_ID_QUEST_ACCEPT_REQ,
        [this](uint64_t player_id, const uint8_t* payload, size_t payload_len) {
            if (!quest_mgr_) return;
            farm::QuestAcceptReq req;
            if (req.ParseFromArray(payload, static_cast<int>(payload_len))) {
                quest_mgr_->handle_quest_accept(player_id, req.quest_id());
            }
        });

    msg_handler_.register_handler(MSG_ID_QUEST_SUBMIT_REQ,
        [this](uint64_t player_id, const uint8_t* payload, size_t payload_len) {
            if (!quest_mgr_) return;
            farm::QuestSubmitReq req;
            if (req.ParseFromArray(payload, static_cast<int>(payload_len))) {
                quest_mgr_->handle_quest_submit(player_id, req.quest_id());
            }
        });

    msg_handler_.register_handler(MSG_ID_QUEST_ABANDON_REQ,
        [this](uint64_t player_id, const uint8_t* payload, size_t payload_len) {
            if (!quest_mgr_) return;
            farm::QuestAbandonReq req;
            if (req.ParseFromArray(payload, static_cast<int>(payload_len))) {
                quest_mgr_->handle_quest_abandon(player_id, req.quest_id());
            }
        });

    // 初始化默认场景（farm）
    scene_mgr_->get_or_create_scene("farm");
    SPDLOG_INFO("[Game]Default scenes initialized");

    // 加载时钟数据
    game_clock_->load_clock_data();

    // 进入事件循环（阻塞）
    event_base_dispatch(base_);

    running_ = false;
    return true;
}

void GameServer::stop() {
    running_ = false;
    // Stop GM HTTP server
    if (gm_http_handler_) {
        gm_http_handler_->stop();
    }
    // Disconnect Redis
    redis_conn_.disconnect();
    // Save all players and stop their timers before shutting down connections
    player_mgr_.save_all_players();
    for (auto* p : player_mgr_.get_all_players()) {
        p->stop_save_timer();
    }
    // Shutdown DBMgr connections
    dbmgr_mgr_.shutdown();
    if (update_timer_) {
        event_free(update_timer_);
        update_timer_ = nullptr;
    }
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

void GameServer::add_dbmgr(uint32_t index, const std::string& host, uint16_t port) {
    dbmgr_mgr_.add_dbmgr(index, host, port);
}

void GameServer::remove_dbmgr(uint32_t index) {
    dbmgr_mgr_.remove_dbmgr(index);
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
        SPDLOG_ERROR("[Game]Failed to create bufferevent for fd={}", fd);
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
        SPDLOG_INFO("[Game]New Gate connection from {}:{} fd={}", ip_str, ntohs(sin->sin_port), fd);
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
    SPDLOG_INFO("[Game]Gate disconnected fd={} gate_id={}", fd, gate_id);

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

    for (auto& [fd, session] : gate_sessions_) {
        if (session->state() == GateSessionState::DISCONNECTED) continue;

        if (now - session->last_heartbeat() > GATE_HEARTBEAT_TIMEOUT) {
            SPDLOG_INFO("[Game]Heartbeat timeout fd={} gate_id={}", session->fd(), session->gate_id());
            timeout_fds.push_back(fd);
        }
    }

    for (evutil_socket_t fd : timeout_fds) {
        auto it = gate_sessions_.find(fd);
        if (it != gate_sessions_.end()) {
            handle_disconnect(it->second);
        }
    }
}

void GameServer::on_update_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* server = static_cast<GameServer*>(ctx);
    server->update_game_logic();
}

void GameServer::update_game_logic() {
    time_t now = std::time(nullptr);

    // Update game clock (1 second interval)
    game_clock_->update();

    // Update item handler (crops, drop expiry)
    item_handler_.update(this);

    // Auto-pickup: for each player, check nearby drop items
    auto all_players = player_mgr_.get_all_players();
    for (Player* player : all_players) {
        if (!player || player->data_state() != PlayerBizDataState::LOADED) continue;

        float px = player->get_pos_x();
        float py = player->get_pos_y();

        auto nearby_ids = item_handler_.drops()->find_nearby(px, py);
        if (nearby_ids.empty()) continue;

        // Load inventory
        PlayerInventory inv;
        if (!load_inventory_from_player(player, inv)) continue;

        bool inventory_changed = false;
        for (uint32_t drop_id : nearby_ids) {
            auto drop = item_handler_.drops()->get(drop_id);
            if (!drop) continue;

            int32_t max_stack = ItemEffects::get_max_stack((*drop)->item_id);
            int32_t leftover = inv.add_item((*drop)->item_id, (*drop)->count, max_stack);

            if (leftover < (*drop)->count) {
                int32_t picked_up = (*drop)->count - leftover;
                SPDLOG_INFO("[Game]Auto-pickup: player={} picked up {} x item_id={} (drop_id={})",
                            player->player_id(), picked_up, (*drop)->item_id, drop_id);

                item_handler_.drops()->remove(drop_id);
                inventory_changed = true;

                if (leftover > 0) {
                    SPDLOG_INFO("[Game]Auto-pickup: player={} inventory full, remaining drops left on ground",
                                player->player_id());
                    break;
                }
            }
        }

        if (inventory_changed) {
            save_inventory_to_player(player, inv);
        }
    }
}

bool GameServer::load_inventory_from_player(Player* player, PlayerInventory& inv) {
    const std::string& inv_json = player->get_inventory();
    if (!inv_json.empty() && inv_json != "{}") {
        return inv.deserialize(inv_json);
    }

    // Default inventory for new players
    inv = PlayerInventory::create_default();
    return true;
}

void GameServer::save_inventory_to_player(Player* player, const PlayerInventory& inv) {
    std::string json = inv.serialize();
    player->set_inventory(json);
}

// ===========================================
// 内部消息路由
// ===========================================

void GameServer::route_internal_message(std::shared_ptr<GateSession> session,
                                        uint32_t msg_id,
                                        const std::vector<uint8_t>& payload) {
    // 管理消息（5000-5999）可以在任何状态下处理
    if (msg_id >= 5000 && msg_id < 6000) {
        admin_handler_->handle(session, msg_id, payload);
        return;
    }

    // 身份识别前只允许 GATE_IDENTIFY 消息
    if (session->state() == GateSessionState::CONNECTED) {
        if (msg_id == MSG_ID_GATE_IDENTIFY) {
            handle_gate_identify(session, payload);
        } else {
            SPDLOG_INFO("[Game]Ignoring msg_id={} from unidentified Gate fd={}", msg_id, session->fd());
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
            login_stub_->handle_account_msg(session, payload);
            break;
        default:
            SPDLOG_INFO("[Game]Unknown internal msg_id={} from fd={}", msg_id, session->fd());
            break;
    }
}

// ===========================================
// 具体消息处理
// ===========================================

void GameServer::handle_gate_identify(std::shared_ptr<GateSession> session,
                                      const std::vector<uint8_t>& payload) {
    farm::GateIdentify req;
    if (!payload.empty() && !req.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Game]Failed to parse GateIdentify");
        return;
    }

    session->set_gate_id(req.gate_id());
    session->set_state(GateSessionState::IDENTIFIED);
    session->update_heartbeat();

    SPDLOG_INFO("[Game]Gate identified: gate_id={} address={} fd={}", req.gate_id(), req.address(), session->fd());

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
    if (!payload.empty() && !hb.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Game]Failed to parse InternHeartbeat");
        return;
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
    if (!payload.empty() && !req.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Game]Failed to parse PlayerJoin");
        return;
    }

    uint64_t player_id = req.player_id();
    SPDLOG_INFO("[Game]Player join: player_id={} from gate_id={}", player_id, session->gate_id());

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

        SPDLOG_INFO("[Game]Player {} already exists", player_id);
    }
    // 注意：成功添加的情况下，响应会在 handle_player_join_callback 中发送
}

void GameServer::handle_player_leave(std::shared_ptr<GateSession> session,
                                     const std::vector<uint8_t>& payload) {
    farm::PlayerLeave req;
    if (!payload.empty() && !req.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Game]Failed to parse PlayerLeave");
        return;
    }

    uint64_t player_id = req.player_id();
    SPDLOG_INFO("[Game]Player leave: player_id={} from gate_id={}", player_id, session->gate_id());

    if (quest_mgr_) {
        quest_mgr_->on_player_logout(player_id);
    }

    // 保存数据并移除玩家
    player_mgr_.remove_player_with_save(player_id);
}

void GameServer::handle_player_join_callback(std::shared_ptr<GateSession> session,
                                             uint64_t player_id, bool success,
                                             const std::string& msg) {
    SPDLOG_INFO("[Game]Player join callback: player_id={} success={} msg={}", player_id, success, msg);

    farm::PlayerJoinResp resp;
    resp.set_player_id(player_id);

    if (success) {
        resp.set_code(0);
        resp.set_msg("joined");
        SPDLOG_INFO("[Game]Player {} joined successfully", player_id);

        if (quest_mgr_) {
            quest_mgr_->on_player_login(player_id);
        }
    } else {
        resp.set_code(1);
        resp.set_msg(msg);
        SPDLOG_ERROR("[Game]Player {} join failed: {}", player_id, msg);
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_gate(session, MSG_ID_PLAYER_JOIN_RESP, resp_data);
}

void GameServer::handle_client_msg(std::shared_ptr<GateSession> session,
                                   const std::vector<uint8_t>& payload) {
    farm::ClientMessage req;
    if (!payload.empty() && !req.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Game]Failed to parse ClientMessage");
        return;
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
    Player* player = player_mgr_.get_player(player_id).value_or(nullptr);
    if (!player) {
        SPDLOG_ERROR("[Game]Warning: player_id={} not found, discarding msg_id={}", player_id, msg_id);
        return;
    }

    // 分发到业务 handler
    msg_handler_.dispatch(msg_id, player_id,
                          reinterpret_cast<const uint8_t*>(inner_payload.data()),
                          inner_payload.size());
}

void GameServer::handle_enter_game_req(std::shared_ptr<GateSession> session,
                                        uint64_t player_id, const std::string& payload) {
    login_stub_->handle_enter_game_req(session, player_id, payload);
}

void GameServer::handle_position_update(uint64_t player_id,
                                        const uint8_t* payload, size_t payload_len) {
    // Parse PositionUpdate
    farm::PositionUpdate req;
    if (payload_len > 0 && !req.ParseFromArray(payload, static_cast<int>(payload_len))) {
        SPDLOG_ERROR("[Game]Failed to parse PositionUpdate for player_id={}", player_id);
        return;
    }

    // Find player
    Player* player = player_mgr_.get_player(player_id).value_or(nullptr);
    if (!player || player->data_state() != PlayerBizDataState::LOADED) {
        return;
    }

    // Check scene exists
    const std::string& scene_id = player->get_scene_id();
    auto scene_opt = scene_mgr_->get_scene(scene_id);
    if (!scene_opt.has_value() || !scene_opt.value()) {
        return;
    }
    SceneState* scene = scene_opt.value();

    // Timeout check: discard stale updates
    int64_t now_ms = static_cast<int64_t>(std::time(nullptr)) * 1000;
    if (req.timestamp() > 0 && (now_ms - static_cast<int64_t>(req.timestamp())) > POSITION_UPDATE_TIMEOUT_MS) {
        SPDLOG_DEBUG("[Game]Stale position update from player_id={} (age={}ms)", player_id, now_ms - req.timestamp());
        return;
    }

    float new_x = req.pos_x();
    float new_y = req.pos_y();
    float new_z = req.pos_z();

    // Boundary check (pixel coordinates, scene dimensions in tiles, TILE_SIZE=16)
    constexpr float TILE_SIZE_PX = 16.0f;
    float max_x = scene->width() * TILE_SIZE_PX - TILE_SIZE_PX;
    float max_y = scene->height() * TILE_SIZE_PX - TILE_SIZE_PX;

    if (new_x < 0.0f || new_x > max_x || new_y < 0.0f || new_y > max_y) {
        SPDLOG_WARN("[Game]Boundary violation: player_id={} pos=({:.1f},{:.1f}) bounds=[0,0]-[{:.1f},{:.1f}]",
                    player_id, new_x, new_y, max_x, max_y);
        // Send correction with clamped position
        float corr_x = std::max(0.0f, std::min(new_x, max_x));
        float corr_y = std::max(0.0f, std::min(new_y, max_y));
        farm::PositionCorrect corr;
        corr.set_pos_x(corr_x);
        corr.set_pos_y(corr_y);
        corr.set_pos_z(new_z);
        corr.set_timestamp(req.timestamp());
        std::string corr_data;
        corr.SerializeToString(&corr_data);
        send_game_msg(player_id, MSG_ID_POSITION_CORRECT,
                      reinterpret_cast<const uint8_t*>(corr_data.data()), corr_data.size());
        return;
    }

    // Speed check
    float old_x = player->get_pos_x();
    float old_y = player->get_pos_y();
    float dx = new_x - old_x;
    float dy = new_y - old_y;
    float distance = std::sqrt(dx * dx + dy * dy);

    if (distance > 0.01f) {
        // Estimate time delta: use 100ms as default if no prior timestamp
        float time_delta_s = 0.1f;
        float speed = distance / time_delta_s;

        if (speed > MAX_PLAYER_SPEED) {
            SPDLOG_WARN("[Game]Speed violation: player_id={} speed={:.1f} max={:.1f} dist={:.1f}",
                        player_id, speed, MAX_PLAYER_SPEED, distance);
            // Send correction with last known good position
            farm::PositionCorrect corr;
            corr.set_pos_x(old_x);
            corr.set_pos_y(old_y);
            corr.set_pos_z(player->get_pos_z());
            corr.set_timestamp(req.timestamp());
            std::string corr_data;
            corr.SerializeToString(&corr_data);
            send_game_msg(player_id, MSG_ID_POSITION_CORRECT,
                          reinterpret_cast<const uint8_t*>(corr_data.data()), corr_data.size());
            return;
        }
    }

    // Valid update — apply
    player->set_pos_x(new_x);
    player->set_pos_y(new_y);
    player->set_pos_z(new_z);

    SPDLOG_DEBUG("[Game]Position updated: player_id={} pos=({:.1f},{:.1f})", player_id, new_x, new_y);
}

void GameServer::handle_item_use_req(uint64_t player_id,
                                      const uint8_t* payload, size_t payload_len) {
    // Parse ItemUseReq
    farm::ItemUseReq req;
    if (payload_len > 0 && !req.ParseFromArray(payload, static_cast<int>(payload_len))) {
        SPDLOG_ERROR("[Game]Failed to parse ItemUseReq");
        return;
    }

    SPDLOG_INFO("[Game]ItemUseReq: player_id={} target=({},{}) direction={} active_slot={}",
                player_id, req.target_x(), req.target_y(), req.direction(), req.active_slot());

    // Get player
    Player* player = player_mgr_.get_player(player_id).value_or(nullptr);
    if (!player) {
        SPDLOG_ERROR("[Game]ItemUseReq: player_id={} not found", player_id);
        return;
    }

    // Process item use
    ItemUseResult result = item_handler_.handle_item_use(
        player,
        static_cast<int32_t>(req.target_x()),
        static_cast<int32_t>(req.target_y()),
        req.direction(),
        static_cast<int32_t>(req.active_slot()),
        this);

    // Send ItemUseResp
    farm::ItemUseResp resp;
    resp.set_code(static_cast<int32_t>(result));

    switch (result) {
        case ItemUseResult::SUCCESS:
            resp.set_msg("success");
            break;
        case ItemUseResult::NO_ACTIVE_ITEM:
            resp.set_msg("no active item");
            break;
        case ItemUseResult::NO_MATCHING_EFFECT:
            resp.set_msg("no matching effect");
            break;
        case ItemUseResult::ENERGY_EXHAUSTED:
            resp.set_msg("energy exhausted");
            break;
        case ItemUseResult::INVALID_TARGET:
            resp.set_msg("invalid target");
            break;
        default:
            resp.set_msg("internal error");
            break;
    }

    // Include energy data in response
    farm::EnergySync* energy_sync = resp.mutable_energy();
    energy_sync->set_current(player->get_energy());
    energy_sync->set_max(farm::MAX_ENERGY);

    // Send response via PlayerMsg wrapper
    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_game_msg(player_id, MSG_ID_ITEM_USE_RESP,
                  reinterpret_cast<const uint8_t*>(resp_data.data()),
                  resp_data.size());

    if (result == ItemUseResult::SUCCESS) {
        SPDLOG_INFO("[Game]ItemUseReq processed successfully for player_id={}", player_id);
    }
}

// ===========================================
// 发送消息辅助
// ===========================================

void GameServer::send_to_gate(std::shared_ptr<GateSession> session,
                              uint32_t msg_id, std::string_view payload) {
    auto packed = MessageParser::pack(msg_id, payload);
    bufferevent_write(session->bev(), packed.data(), packed.size());
}

void GameServer::send_game_msg(uint64_t player_id, uint32_t msg_id,
                               const uint8_t* payload, size_t payload_len) {
    // 查找玩家对应的 GateSession
    Player* player = player_mgr_.get_player(player_id).value_or(nullptr);
    if (!player || !player->gate_session()) {
        SPDLOG_ERROR("[Game]Cannot send GAME_MSG: player_id={} has no Gate session", player_id);
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
