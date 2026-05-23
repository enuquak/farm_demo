#pragma once

#include "session_manager.h"
#include "game_connection.h"
#include <event2/event.h>
#include <event2/listener.h>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace farm {

class GateServer {
public:
    GateServer(const std::string& ip, uint16_t port);
    ~GateServer();

    // 启动服务器（阻塞）
    bool start();

    // 停止服务器
    void stop();

    // 配置 Game Server 连接
    void set_game_server(const std::string& ip, uint16_t port);

private:
    // libevent 回调
    static void on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                          struct sockaddr* addr, int len, void* ctx);
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);

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
    void handle_game_message(uint32_t msg_id, const std::vector<uint8_t>& payload);
    void handle_game_identify_resp(const std::vector<uint8_t>& payload);
    void handle_game_heartbeat_resp(const std::vector<uint8_t>& payload);
    void handle_player_join_resp(const std::vector<uint8_t>& payload);
    void handle_game_msg(const std::vector<uint8_t>& payload);
    void forward_to_game(std::shared_ptr<Session> session, uint32_t msg_id,
                         const std::vector<uint8_t>& payload);

    std::string ip_;
    uint16_t port_;
    struct event_base* base_;
    struct evconnlistener* listener_;
    struct event* heartbeat_timer_;
    SessionManager session_mgr_;
    bool running_;

    // Game 连接
    std::unique_ptr<GameConnection> game_conn_;
    std::string game_ip_;
    uint16_t game_port_;

    // 玩家路由表: player_id -> game_conn (目前只有一个 Game)
    std::unordered_map<uint64_t, GameConnection*> player_to_game_;
};

}  // namespace farm
