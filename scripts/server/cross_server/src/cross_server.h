#pragma once

#include "game_session.h"
#include "game_connection.h"
#include "route_cache.h"
#include "message_parser.h"
#include "internal_msg_ids.h"

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>

#include <string>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <ctime>

namespace farm {

struct CrossServerConfig {
    std::string ip = "0.0.0.0";
    uint16_t port = 7070;
    uint32_t instance_id = 1;
};

// Forward declaration
struct ConnectionContext;

class CrossServer {
    friend struct ConnectionContext;  // Allow callbacks to access private members

public:
    CrossServer(const CrossServerConfig& config);
    ~CrossServer();

    // Get event_base (for external use)
    struct event_base* base() const { return base_; }

    // Start server (blocking)
    bool start();

    // Stop server
    void stop();

    // Dynamically manage Game Server connections (thread-safe, can be called from etcd watch callbacks)
    void add_game_server(uint32_t server_id, const std::string& host, uint16_t port);
    void remove_game_server(uint32_t server_id);

    // Update route cache (for etcd watch callbacks)
    void update_route(uint64_t player_id, uint32_t server_id);
    void remove_route(uint64_t player_id);

private:
    // libevent callbacks (passive connections)
    static void on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                          struct sockaddr* addr, int len, void* ctx);
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);

    // Connection handling (passive connections)
    void handle_accept(evutil_socket_t fd, struct sockaddr* addr);
    void handle_read(std::shared_ptr<GameSession> session);
    void handle_disconnect(std::shared_ptr<GameSession> session);
    void check_heartbeat();

    // Message routing (passive connections)
    void route_message(std::shared_ptr<GameSession> session, uint32_t msg_id,
                       const std::vector<uint8_t>& payload);

    // Passive connection message handlers
    void handle_cross_identify(std::shared_ptr<GameSession> session,
                               const std::vector<uint8_t>& payload);
    void handle_cross_heartbeat(std::shared_ptr<GameSession> session,
                                const std::vector<uint8_t>& payload);
    void handle_cross_query_req(std::shared_ptr<GameSession> session,
                                const std::vector<uint8_t>& payload);
    void handle_cross_forward_resp(std::shared_ptr<GameSession> session,
                                   const std::vector<uint8_t>& payload);

    // Active connection message handlers
    void on_connection_message(uint32_t server_id, uint32_t msg_id,
                               const std::vector<uint8_t>& payload);
    void on_connection_disconnect(uint32_t server_id);

    // Send message helper
    void send_to_session(std::shared_ptr<GameSession> session,
                         uint32_t msg_id, const std::string& payload);

    // Find session by server_id
    std::shared_ptr<GameSession> get_session_by_server_id(uint32_t server_id);

    // Config
    CrossServerConfig config_;

    // libevent
    struct event_base* base_ = nullptr;
    struct evconnlistener* listener_ = nullptr;
    struct event* heartbeat_timer_ = nullptr;
    bool running_ = false;

    // Passive connections: Game Server connects in (indexed by fd)
    std::unordered_map<evutil_socket_t, std::shared_ptr<GameSession>> sessions_;
    // server_id -> session mapping (for fast lookup)
    std::unordered_map<uint32_t, std::shared_ptr<GameSession>> server_sessions_;
    std::mutex sessions_mutex_;

    // Active connections: connect to Game Server (indexed by server_id)
    std::unordered_map<uint32_t, std::unique_ptr<GameConnection>> connections_;
    std::mutex connections_mutex_;

    // Route cache
    RouteCache route_cache_;

    // Pending requests (request_id -> session + timestamp)
    struct PendingRequest {
        std::shared_ptr<GameSession> session;
        time_t timestamp;
    };
    std::unordered_map<uint64_t, PendingRequest> pending_requests_;
    std::mutex pending_mutex_;
    // ConnectionContext pointers for active connections (parallel to connections_)
    std::unordered_map<uint32_t, ConnectionContext*> connection_contexts_;
};

}  // namespace farm
