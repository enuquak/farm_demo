#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

#include <event2/bufferevent.h>
#include <event2/event.h>

namespace farm {

enum class CrossServerState {
    DISCONNECTED,
    CONNECTED,
    IDENTIFIED,
};

using CrossMsgCallback = std::function<void(uint32_t msg_id, const uint8_t* data, size_t len)>;

class CrossServerConnection {
public:
    CrossServerConnection(struct event_base* base, uint32_t server_id);
    ~CrossServerConnection();

    bool connect(const std::string& host, int port);
    void disconnect();
    bool is_identified() const { return state_ == CrossServerState::IDENTIFIED; }

    void send_query_req(uint64_t request_id, uint64_t target_player_id,
                        uint32_t query_type, const std::string& request_data);
    void send_forward_resp(uint64_t request_id, int32_t code, const std::string& response_data);

    void set_on_message(CrossMsgCallback cb) { on_message_ = std::move(cb); }

private:
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    void handle_read();
    void handle_event(short events);
    void route_message(uint32_t msg_id, const uint8_t* data, size_t len);

    void send_identify();
    void start_heartbeat();
    void stop_heartbeat();
    void stop_reconnect();
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);
    void schedule_reconnect();
    static void on_reconnect_timer(evutil_socket_t fd, short events, void* ctx);

    struct event_base* base_;
    struct bufferevent* bev_ = nullptr;
    CrossServerState state_ = CrossServerState::DISCONNECTED;
    std::vector<uint8_t> read_buffer_;
    std::string host_;
    int port_ = 0;
    uint32_t server_id_;

    struct event* heartbeat_timer_ = nullptr;
    struct event* reconnect_timer_ = nullptr;
    time_t last_heartbeat_recv_ = 0;

    CrossMsgCallback on_message_;
};

}  // namespace farm
