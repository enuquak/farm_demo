#pragma once

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <string>
#include <string_view>
#include <cstdint>
#include <ctime>
#include <vector>
#include <functional>

namespace farm {

enum class ChatConnState {
    DISCONNECTED,
    CONNECTING,
    IDENTIFIED
};

using ChatMessageCallback = std::function<void(uint32_t msg_id, const std::vector<uint8_t>& payload)>;

class ChatConnection {
public:
    ChatConnection(struct event_base* base, const std::string& gate_id);
    ~ChatConnection();

    bool connect(const std::string& ip, uint16_t port);
    void disconnect();

    bool send(uint32_t msg_id, std::string_view payload);
    bool send(uint32_t msg_id, const uint8_t* payload, size_t len);

    ChatConnState state() const { return state_; }
    void set_state(ChatConnState state) { state_ = state; }
    bool is_identified() const { return state_ == ChatConnState::IDENTIFIED; }

    void set_message_callback(ChatMessageCallback callback) { msg_callback_ = std::move(callback); }

    void start_heartbeat();
    void stop_heartbeat();
    void start_reconnect();
    void stop_reconnect();

private:
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);
    static void on_reconnect_timer(evutil_socket_t fd, short events, void* ctx);

    void handle_read();
    void handle_connect_success();
    void handle_disconnect();
    void send_identify();
    void send_heartbeat();
    void try_reconnect();

    struct event_base* base_;
    std::string gate_id_;
    std::string chat_ip_;
    uint16_t chat_port_;
    ChatConnState state_;

    struct bufferevent* bev_;
    struct event* heartbeat_timer_;
    struct event* reconnect_timer_;
    time_t last_heartbeat_;
    std::vector<uint8_t> read_buffer_;

    ChatMessageCallback msg_callback_;
};

}  // namespace farm
