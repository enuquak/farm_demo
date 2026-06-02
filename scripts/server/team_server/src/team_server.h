#pragma once

#include "team_manager.h"

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>

namespace farm {

struct TeamGateSession {
    evutil_socket_t fd;
    struct bufferevent* bev;
    std::string gate_id;
    bool identified;
    std::vector<uint8_t> read_buffer;
};

class TeamServer {
public:
    TeamServer(const std::string& ip, uint16_t port);
    ~TeamServer();

    bool start();
    void stop();

private:
    // libevent callbacks
    static void on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                          struct sockaddr* addr, int len, void* ctx);
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_invite_timeout_timer(evutil_socket_t fd, short events, void* ctx);

    // Connection handling
    void handle_accept(evutil_socket_t fd, struct sockaddr* addr);
    void handle_read(std::shared_ptr<TeamGateSession> session);
    void handle_disconnect(std::shared_ptr<TeamGateSession> session);

    // Message routing
    void route_message(std::shared_ptr<TeamGateSession> session,
                       uint32_t msg_id, const std::vector<uint8_t>& payload);

    // Internal protocol handlers
    void handle_gate_identify(std::shared_ptr<TeamGateSession> session,
                              const std::vector<uint8_t>& payload);
    void handle_heartbeat(std::shared_ptr<TeamGateSession> session,
                          const std::vector<uint8_t>& payload);
    void handle_player_join(std::shared_ptr<TeamGateSession> session,
                            const std::vector<uint8_t>& payload);
    void handle_player_leave(std::shared_ptr<TeamGateSession> session,
                             const std::vector<uint8_t>& payload);
    void handle_client_msg(std::shared_ptr<TeamGateSession> session,
                           const std::vector<uint8_t>& payload);

    // Team message handlers
    void handle_team_create_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_disband_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_invite_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_accept_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_reject_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_leave_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_kick_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_info_req(uint64_t player_id, const uint8_t* payload, size_t len);

    // Internal query handlers
    void handle_team_query_members_req(std::shared_ptr<TeamGateSession> session,
                                       const uint8_t* payload, size_t len);

    // Send helpers
    void send_to_gate(std::shared_ptr<TeamGateSession> session,
                      uint32_t msg_id, std::string_view payload);
    void send_to_player(uint64_t player_id, uint32_t msg_id,
                        const uint8_t* payload, size_t len);
    void broadcast_to_team(uint64_t team_id, uint32_t msg_id,
                           const uint8_t* payload, size_t len,
                           uint64_t exclude_player_id = 0);

    std::string ip_;
    uint16_t port_;
    struct event_base* base_;
    struct evconnlistener* listener_;
    struct event* invite_timeout_timer_;
    bool running_;

    std::unordered_map<evutil_socket_t, std::shared_ptr<TeamGateSession>> gate_sessions_;
    std::unordered_map<uint64_t, std::shared_ptr<TeamGateSession>> player_to_gate_;

    TeamManager team_mgr_;
};

}  // namespace farm
