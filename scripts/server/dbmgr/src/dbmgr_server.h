#pragma once

#include "game_session.h"
#include "data_manager.h"
#include "message_parser.h"

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <memory>

namespace farm {

class DbMgrServer {
public:
    DbMgrServer(uint32_t index, const std::string& ip, uint16_t port, const std::string& data_dir);
    ~DbMgrServer();

    // 启动服务器（阻塞）
    bool start();

    // 停止服务器
    void stop();

private:
    // libevent 回调
    static void on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                          struct sockaddr* addr, int len, void* ctx);
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);

    // 连接处理
    void handle_accept(evutil_socket_t fd, struct sockaddr* addr);
    void handle_read(std::shared_ptr<GameSession> session);
    void handle_disconnect(std::shared_ptr<GameSession> session);
    void check_heartbeat();
    void send_heartbeat(std::shared_ptr<GameSession> session);

    // 消息路由
    void route_message(std::shared_ptr<GameSession> session,
                       uint32_t msg_id,
                       const std::vector<uint8_t>& payload);

    // 具体消息处理
    void handle_dbmgr_identify_resp(std::shared_ptr<GameSession> session,
                                    const std::vector<uint8_t>& payload);
    void handle_dbmgr_heartbeat_resp(std::shared_ptr<GameSession> session,
                                     const std::vector<uint8_t>& payload);
    void handle_player_data_req(std::shared_ptr<GameSession> session,
                                const std::vector<uint8_t>& payload);
    void handle_account_data_req(std::shared_ptr<GameSession> session,
                                 const std::vector<uint8_t>& payload);
    void handle_account_set_req(std::shared_ptr<GameSession> session,
                                const std::vector<uint8_t>& payload);

    // 发送消息辅助
    void send_to_game(std::shared_ptr<GameSession> session,
                      uint32_t msg_id, const std::string& payload);

    uint32_t index_;
    std::string ip_;
    uint16_t port_;
    std::string data_dir_;
    struct event_base* base_;
    struct evconnlistener* listener_;
    struct event* heartbeat_timer_;
    bool running_;

    // Game 会话管理（按 fd 索引）
    std::unordered_map<evutil_socket_t, std::shared_ptr<GameSession>> game_sessions_;

    // 数据管理器
    DataManager data_mgr_;
};

}  // namespace farm
