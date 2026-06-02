#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

#include <event2/bufferevent.h>
#include <event2/event.h>

namespace farm {

class FriendService;

enum class GameSessionState {
    DISCONNECTED,
    CONNECTED,
    IDENTIFIED,
};

class GameSession {
public:
    GameSession(FriendService* service, struct event_base* base);
    ~GameSession();

    bool connect(const std::string& host, int port);
    void disconnect();
    bool is_identified() const { return state_ == GameSessionState::IDENTIFIED; }

    void send(uint32_t msg_id, const std::string& payload);
    void send_to_client(uint64_t player_id, uint32_t msg_id, const std::string& payload);

private:
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    void handle_read();
    void handle_event(short events);
    void route_message(uint32_t msg_id, const uint8_t* data, size_t len);

    void send_identify();
    void start_heartbeat();
    void stop_heartbeat();
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);
    void schedule_reconnect();
    static void on_reconnect_timer(evutil_socket_t fd, short events, void* ctx);

    FriendService* service_;
    struct event_base* base_;
    struct bufferevent* bev_ = nullptr;
    GameSessionState state_ = GameSessionState::DISCONNECTED;
    std::vector<uint8_t> read_buffer_;
    std::string host_;
    int port_ = 0;

    struct event* heartbeat_timer_ = nullptr;
    struct event* reconnect_timer_ = nullptr;
    time_t last_heartbeat_recv_ = 0;
};

}  // namespace farm
