#pragma once

#include "channel_manager.h"
#include "rate_limiter.h"

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>

namespace farm {

struct ChatGateSession {
    evutil_socket_t fd;
    struct bufferevent* bev;
    std::string gate_id;
    bool identified;
    std::vector<uint8_t> read_buffer;
};

class ChatServer {
public:
    ChatServer(const std::string& ip, uint16_t port);
    ~ChatServer();

    bool start();
    void stop();

private:
    static void on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                          struct sockaddr* addr, int len, void* ctx);
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);

    void handle_accept(evutil_socket_t fd, struct sockaddr* addr);
    void handle_read(std::shared_ptr<ChatGateSession> session);
    void handle_disconnect(std::shared_ptr<ChatGateSession> session);
    void route_message(std::shared_ptr<ChatGateSession> session,
                       uint32_t msg_id, const std::vector<uint8_t>& payload);

    void handle_gate_identify(std::shared_ptr<ChatGateSession> session,
                              const std::vector<uint8_t>& payload);
    void handle_heartbeat(std::shared_ptr<ChatGateSession> session,
                          const std::vector<uint8_t>& payload);
    void handle_player_join(std::shared_ptr<ChatGateSession> session,
                            const std::vector<uint8_t>& payload);
    void handle_player_leave(std::shared_ptr<ChatGateSession> session,
                             const std::vector<uint8_t>& payload);
    void handle_client_msg(std::shared_ptr<ChatGateSession> session,
                           const std::vector<uint8_t>& payload);
    void handle_chat_send_req(uint64_t player_id, const uint8_t* payload, size_t len);

    void send_to_gate(std::shared_ptr<ChatGateSession> session,
                      uint32_t msg_id, std::string_view payload);
    void send_to_player(uint64_t player_id, uint32_t msg_id,
                        const uint8_t* payload, size_t len);

    std::string ip_;
    uint16_t port_;
    struct event_base* base_;
    struct evconnlistener* listener_;
    bool running_;

    std::unordered_map<evutil_socket_t, std::shared_ptr<ChatGateSession>> gate_sessions_;
    std::unordered_map<uint64_t, std::shared_ptr<ChatGateSession>> player_to_gate_;

    RateLimiter rate_limiter_;
    std::unique_ptr<ChannelManager> channel_mgr_;
};

}  // namespace farm
