#pragma once

#include "session_manager.h"
#include "game_connection.h"
#include "admin_msg_ids.h"
#include <event2/event.h>
#include <event2/listener.h>
#include <string>
#include <cstdint>
#include <optional>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace farm {

// Game Server 配置
struct GameServerConfig {
    uint32_t server_id;
    std::string ip;
    uint16_t port;
};

class GateServer {
public:
    GateServer(const std::string& ip, uint16_t port);
    ~GateServer();

    // 启动服务器（阻塞）
    bool start();

    // 停止服务器
    void stop();

    // 添加 Game Server 连接配置
    void add_game_server(uint32_t server_id, const std::string& ip, uint16_t port);

    // 移除 Game Server 连接（用于 etcd watch 回调）
    void remove_game_server(uint32_t server_id);

    // 将操作分发到事件循环线程（线程安全）
    void dispatch_to_event_loop(std::function<void()> func);

private:
    // libevent 回调
    static void on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                          struct sockaddr* addr, int len, void* ctx);
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);
    static void on_dispatch_callback(evutil_socket_t fd, short events, void* ctx);

    // 消息处理
    void handle_accept(evutil_socket_t fd, struct sockaddr* addr);
    void handle_read(std::shared_ptr<Session> session);
    void handle_disconnect(std::shared_ptr<Session> session);
    void check_heartbeat();

    // 消息路由
    void route_message(std::shared_ptr<Session> session, uint32_t msg_id,
                       const std::vector<uint8_t>& payload);

    // 具体消息处理
    void handle_heartbeat(std::shared_ptr<Session> session, const std::vector<uint8_t>& payload);
    void handle_login(std::shared_ptr<Session> session, const std::vector<uint8_t>& payload);

    // Game 连接相关
    void handle_game_message(uint32_t server_id, uint32_t msg_id, const std::vector<uint8_t>& payload);
    void handle_game_identify_resp(uint32_t server_id, const std::vector<uint8_t>& payload);
    void handle_game_heartbeat_resp(uint32_t server_id, const std::vector<uint8_t>& payload);
    void handle_player_join_resp(uint32_t server_id, const std::vector<uint8_t>& payload);
    void handle_game_msg(uint32_t server_id, const std::vector<uint8_t>& payload);
    void handle_account_msg_resp(uint32_t server_id, const std::vector<uint8_t>& payload);
    void forward_to_game(std::shared_ptr<Session> session, uint32_t msg_id,
                         const std::vector<uint8_t>& payload);
    void forward_account_msg_to_game(std::shared_ptr<Session> session, uint32_t msg_id,
                                     const std::vector<uint8_t>& payload);
    void forward_player_msg_to_game(std::shared_ptr<Session> session, uint32_t server_id,
                                    uint32_t msg_id, const std::vector<uint8_t>& payload);

    // 管理消息处理
    void handle_admin_message(uint32_t msg_id, const std::vector<uint8_t>& payload);
    void handle_shutdown(const AdminShutdownMsg& msg);
    void handle_shutdown_resp(const AdminShutdownResp& resp);

    // 获取 Game 连接
    std::optional<GameConnection*> get_game_connection(uint32_t server_id);
    std::optional<GameConnection*> get_any_game_connection();

    std::string ip_;
    uint16_t port_;
    struct event_base* base_;
    struct evconnlistener* listener_;
    struct event* heartbeat_timer_;
    SessionManager session_mgr_;
    bool running_;

    // 保护 game_conns_ 的互斥锁（watch 回调在后台线程）
    mutable std::mutex game_conns_mutex_;

    // Game Server 配置
    std::vector<GameServerConfig> game_server_configs_;

    // Game 连接: server_id -> GameConnection
    std::unordered_map<uint32_t, std::unique_ptr<GameConnection>> game_conns_;

    // 玩家路由表: player_id -> server_id
    std::unordered_map<uint64_t, uint32_t> player_to_server_;

    // 轮询索引（用于 get_any_game_connection）
    size_t next_game_index_ = 0;
};

}  // namespace farm
