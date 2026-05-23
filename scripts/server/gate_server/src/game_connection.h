#pragma once

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <string>
#include <cstdint>
#include <ctime>
#include <vector>
#include <functional>

namespace farm {

enum class GameConnState {
    DISCONNECTED,
    CONNECTING,
    IDENTIFIED
};

// Game 消息回调
using GameMessageCallback = std::function<void(uint32_t msg_id, const std::vector<uint8_t>& payload)>;

class GameConnection {
public:
    GameConnection(struct event_base* base, const std::string& gate_id);
    ~GameConnection();

    // 连接到 Game Server
    bool connect(const std::string& ip, uint16_t port);

    // 断开连接
    void disconnect();

    // 发送消息到 Game
    bool send(uint32_t msg_id, const std::string& payload);
    bool send(uint32_t msg_id, const uint8_t* payload, size_t len);

    // 状态查询
    GameConnState state() const { return state_; }
    bool is_identified() const { return state_ == GameConnState::IDENTIFIED; }
    const std::string& game_address() const { return game_ip_ + ":" + std::to_string(game_port_); }

    // 设置消息回调
    void set_message_callback(GameMessageCallback callback) { msg_callback_ = std::move(callback); }

    // 启动/停止心跳
    void start_heartbeat();
    void stop_heartbeat();

    // 启动/停止重连
    void start_reconnect();
    void stop_reconnect();

private:
    // libevent 回调
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);
    static void on_reconnect_timer(evutil_socket_t fd, short events, void* ctx);

    // 内部处理
    void handle_read();
    void handle_connect_success();
    void handle_disconnect();
    void send_identify();
    void send_heartbeat();
    void try_reconnect();

    struct event_base* base_;
    std::string gate_id_;
    std::string game_ip_;
    uint16_t game_port_;
    GameConnState state_;

    struct bufferevent* bev_;
    struct event* heartbeat_timer_;
    struct event* reconnect_timer_;
    time_t last_heartbeat_;
    std::vector<uint8_t> read_buffer_;

    GameMessageCallback msg_callback_;
};

}  // namespace farm
