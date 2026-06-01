#pragma once

#include "gate_session.h"
#include "player_manager.h"
#include "message_handler.h"
#include "message_parser.h"
#include "dbmgr_connection_manager.h"
#include "item_interaction_handler.h"
#include "login_stub.h"
#include "online_stub.h"
#include "gm_stub.h"
#include "gm_http_handler.h"
#include "redis_connection.h"

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <string>
#include <string_view>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>

namespace farm {

// 玩家加入回调类型
using PlayerJoinCallback = std::function<void(uint64_t player_id, bool success, const std::string& msg)>;

// Forward declarations for extracted subsystems
class GameSceneManager;
class GameClock;
class AdminHandler;
class GMStub;
class GmHttpHandler;

class GameServer {
public:
    GameServer(const std::string& ip, uint16_t port,
               const std::vector<DBMgrConfig>& dbmgr_configs = {},
               const std::string& redis_uri = "",
               uint32_t server_id = 1,
               uint16_t gm_http_port = 7070,
               const std::string& gm_static_dir = "static");
    ~GameServer();

    // 启动服务器（阻塞）
    bool start();

    // 停止服务器
    void stop();

    // 注册业务消息 handler（在 start() 之前调用）
    void register_handler(uint32_t msg_id, MessageCallback callback);

private:
    // libevent 回调
    static void on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                          struct sockaddr* addr, int len, void* ctx);
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);
    static void on_update_timer(evutil_socket_t fd, short events, void* ctx);

    // 连接处理
    void handle_accept(evutil_socket_t fd, struct sockaddr* addr);
    void handle_read(std::shared_ptr<GateSession> session);
    void handle_disconnect(std::shared_ptr<GateSession> session);
    void check_heartbeat();
    void update_game_logic();

    // Inventory helpers for auto-pickup
    bool load_inventory_from_player(Player* player, PlayerInventory& inv);
    void save_inventory_to_player(Player* player, const PlayerInventory& inv);

    // 内部消息路由
    void route_internal_message(std::shared_ptr<GateSession> session,
                                uint32_t msg_id,
                                const std::vector<uint8_t>& payload);

    // 具体消息处理
    void handle_gate_identify(std::shared_ptr<GateSession> session,
                              const std::vector<uint8_t>& payload);
    void handle_intern_heartbeat(std::shared_ptr<GateSession> session,
                                 const std::vector<uint8_t>& payload);
    void handle_player_join(std::shared_ptr<GateSession> session,
                            const std::vector<uint8_t>& payload);
    void handle_player_leave(std::shared_ptr<GateSession> session,
                             const std::vector<uint8_t>& payload);
    void handle_client_msg(std::shared_ptr<GateSession> session,
                           const std::vector<uint8_t>& payload);
    void handle_item_use_req(uint64_t player_id,
                              const uint8_t* payload, size_t payload_len);
    void handle_enter_game_req(std::shared_ptr<GateSession> session,
                               uint64_t player_id, const std::string& payload);

    // 玩家加入回调处理
    void handle_player_join_callback(std::shared_ptr<GateSession> session,
                                     uint64_t player_id, bool success,
                                     const std::string& msg);

    // 发送消息辅助
    void send_to_gate(std::shared_ptr<GateSession> session,
                      uint32_t msg_id, std::string_view payload);
    void send_game_msg(uint64_t player_id, uint32_t msg_id,
                       const uint8_t* payload, size_t payload_len);

    std::string ip_;
    uint16_t port_;
    struct event_base* base_;
    struct evconnlistener* listener_;
    struct event* heartbeat_timer_;
    struct event* update_timer_;
    bool running_;

    // Gate 会话管理（按 fd 索引）
    std::unordered_map<evutil_socket_t, std::shared_ptr<GateSession>> gate_sessions_;

    // 玩家管理
    PlayerManager player_mgr_;

    // 业务消息处理框架
    MessageHandler msg_handler_;

    // DBMgr 连接管理
    DBMgrConnectionManager dbmgr_mgr_;
    std::vector<DBMgrConfig> dbmgr_configs_;

    // 物品交互处理器
    ItemInteractionHandler item_handler_;

    // Redis connection
    RedisConnection redis_conn_;

    // Stubs
    std::unique_ptr<LoginStub> login_stub_;
    std::unique_ptr<OnlineStub> online_stub_;

    // Config
    std::string redis_uri_;
    uint32_t server_id_;

    // Extracted subsystems (initialized in start())
    std::unique_ptr<GameSceneManager> scene_mgr_;
    std::unique_ptr<GameClock> game_clock_;
    std::unique_ptr<AdminHandler> admin_handler_;

    // GM system
    std::unique_ptr<GMStub> gm_stub_;
    std::unique_ptr<GmHttpHandler> gm_http_handler_;
    uint16_t gm_http_port_ = 7070;
    std::string gm_static_dir_ = "static";
};

}  // namespace farm
