#include "cross_server.h"
#include "log_macros.h"

#include <cross.pb.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include <cstring>
#include <sstream>

namespace farm {

CrossServer::CrossServer(const CrossServerConfig& config)
    : config_(config)
{
}

CrossServer::~CrossServer() {
    stop();
}

bool CrossServer::start() {
    base_ = event_base_new();
    if (!base_) {
        SPDLOG_ERROR("[Cross]Failed to create event base");
        return false;
    }

    // Bind address
    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(config_.port);
    if (config_.ip.empty() || config_.ip == "0.0.0.0") {
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        inet_pton(AF_INET, config_.ip.c_str(), &sin.sin_addr);
    }

    listener_ = evconnlistener_new_bind(base_, on_accept, this,
                                         LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
                                         128, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin));
    if (!listener_) {
        SPDLOG_ERROR("[Cross]Failed to bind to {}:{}", config_.ip, config_.port);
        event_base_free(base_);
        base_ = nullptr;
        return false;
    }

    // Heartbeat timer (check every 5 seconds)
    struct timeval tv = {5, 0};
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    event_add(heartbeat_timer_, &tv);

    running_ = true;
    SPDLOG_INFO("[Cross]Started on {}:{}", config_.ip, config_.port);

    // Event loop (blocking)
    event_base_dispatch(base_);

    // Cleanup
    running_ = false;
    if (heartbeat_timer_) {
        event_free(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
    if (listener_) {
        evconnlistener_free(listener_);
        listener_ = nullptr;
    }
    if (base_) {
        event_base_free(base_);
        base_ = nullptr;
    }

    return true;
}

void CrossServer::stop() {
    if (base_ && running_) {
        event_base_loopexit(base_, nullptr);
    }
}

// Context structure for active connection libevent callbacks
struct ConnectionContext {
    CrossServer* server;
    uint32_t server_id;
};

void CrossServer::add_game_server(uint32_t server_id, const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lock(connections_mutex_);

    // Skip if already exists
    if (connections_.find(server_id) != connections_.end()) {
        SPDLOG_WARN("[Cross]Game server {} already connected", server_id);
        return;
    }

    // Create connection
    auto conn = std::make_unique<GameConnection>(server_id, host, port);

    // Create context (libevent callbacks need void* context)
    auto* ctx = new ConnectionContext{this, server_id};

    // Create bufferevent and connect
    struct bufferevent* bev = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        SPDLOG_ERROR("[Cross]Failed to create bufferevent for server {}", server_id);
        delete ctx;
        return;
    }

    conn->set_bev(bev);
    conn->set_state(ConnectionState::CONNECTING);

    // Set callbacks (pass CrossServer and server_id via ConnectionContext)
    bufferevent_setcb(bev,
        [](struct bufferevent* bev, void* ctx) {
            auto* conn_ctx = static_cast<ConnectionContext*>(ctx);
            auto* server = conn_ctx->server;
            uint32_t sid = conn_ctx->server_id;

            // FIX (Issue 3): Read data and parse messages under lock, collect
            // parsed messages, release lock, then process them outside lock.
            // This prevents deadlock when on_connection_message re-acquires
            // connections_mutex_.
            std::vector<ParsedMessage> messages;
            {
                std::lock_guard<std::mutex> lock(server->connections_mutex_);
                auto it = server->connections_.find(sid);
                if (it == server->connections_.end()) {
                    return;
                }
                auto& conn = it->second;

                // Read data
                uint8_t buf[4096];
                size_t n;
                while ((n = bufferevent_read(bev, buf, sizeof(buf))) > 0) {
                    conn->append_read_data(buf, n);
                }

                // Parse messages and collect them
                ParsedMessage msg;
                size_t consumed;
                while (MessageParser::try_parse(conn->read_buffer().data(),
                                                 conn->read_buffer().size(),
                                                 msg, consumed)) {
                    messages.push_back(std::move(msg));
                    conn->consume_read_data(consumed);
                }
            }

            // Process messages outside the lock to avoid deadlock
            for (auto& msg : messages) {
                server->on_connection_message(sid, msg.msg_id, msg.payload);
            }
        },
        nullptr,
        [](struct bufferevent* bev, short events, void* ctx) {
            auto* conn_ctx = static_cast<ConnectionContext*>(ctx);
            auto* server = conn_ctx->server;
            uint32_t sid = conn_ctx->server_id;

            // FIX (Issue 4): Release lock before calling handlers to prevent
            // deadlock if those handlers re-acquire connections_mutex_.
            if (events & BEV_EVENT_CONNECTED) {
                bool should_identify = false;
                {
                    std::lock_guard<std::mutex> lock(server->connections_mutex_);
                    auto it = server->connections_.find(sid);
                    if (it != server->connections_.end()) {
                        it->second->set_state(ConnectionState::CONNECTED);
                        should_identify = true;
                    }
                }
                if (should_identify) {
                    CrossIdentify identify;
                    identify.set_server_id(0);  // CrossServer's server_id is 0
                    identify.set_address("cross_server");
                    std::string s;
                    identify.SerializeToString(&s);
                    auto packed = MessageParser::pack(MSG_ID_CROSS_IDENTIFY, s);
                    bufferevent_write(bev, packed.data(), packed.size());
                }
            }
            if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
                {
                    std::lock_guard<std::mutex> lock(server->connections_mutex_);
                    auto it = server->connections_.find(sid);
                    if (it != server->connections_.end()) {
                        it->second->set_state(ConnectionState::DISCONNECTED);
                    }
                }
                server->on_connection_disconnect(sid);
            }
        },
        ctx
    );
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    // Initiate connection
    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &sin.sin_addr);

    if (bufferevent_socket_connect(bev, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin)) < 0) {
        SPDLOG_ERROR("[Cross]Failed to connect to server {} at {}:{}", server_id, host, port);
        bufferevent_free(bev);
        delete ctx;
        return;
    }

    connections_[server_id] = std::move(conn);
    connection_contexts_[server_id] = ctx;
    SPDLOG_INFO("[Cross]Connecting to game server {} at {}:{}", server_id, host, port);
}

void CrossServer::remove_game_server(uint32_t server_id) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(server_id);
    if (it != connections_.end()) {
        connections_.erase(it);
        // Free the heap-allocated ConnectionContext
        auto ctx_it = connection_contexts_.find(server_id);
        if (ctx_it != connection_contexts_.end()) {
            delete ctx_it->second;
            connection_contexts_.erase(ctx_it);
        }
        SPDLOG_INFO("[Cross]Removed game server {}", server_id);
    }
}

void CrossServer::update_route(uint64_t player_id, uint32_t server_id) {
    route_cache_.update(player_id, server_id);
}

void CrossServer::remove_route(uint64_t player_id) {
    route_cache_.remove(player_id);
}

// ============================================================================
// libevent callbacks (passive connections)
// ============================================================================

void CrossServer::on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                            struct sockaddr* addr, int len, void* ctx) {
    auto* server = static_cast<CrossServer*>(ctx);
    server->handle_accept(fd, addr);
}

// FIX (Issue 1): Copy session shared_ptr under lock, release lock, then call
// handle_read. Previously the lock was held across handle_read, which calls
// route_message -> handle_cross_identify -> acquires sessions_mutex_ again,
// causing a deadlock since sessions_mutex_ is a non-recursive std::mutex.
void CrossServer::on_read(struct bufferevent* bev, void* ctx) {
    auto* server = static_cast<CrossServer*>(ctx);
    evutil_socket_t fd = bufferevent_getfd(bev);
    std::shared_ptr<GameSession> session;
    {
        std::lock_guard<std::mutex> lock(server->sessions_mutex_);
        auto it = server->sessions_.find(fd);
        if (it != server->sessions_.end()) {
            session = it->second;
        }
    }
    if (session) {
        server->handle_read(session);
    }
}

// FIX (Issue 2): Copy session under lock, release, then call handle_disconnect.
// This prevents deadlock if handle_disconnect or any method it calls tries to
// acquire sessions_mutex_.
void CrossServer::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* server = static_cast<CrossServer*>(ctx);
    evutil_socket_t fd = bufferevent_getfd(bev);

    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        std::shared_ptr<GameSession> session;
        {
            std::lock_guard<std::mutex> lock(server->sessions_mutex_);
            auto it = server->sessions_.find(fd);
            if (it != server->sessions_.end()) {
                session = it->second;
            }
        }
        if (session) {
            server->handle_disconnect(session);
        }
    }
}

void CrossServer::on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* server = static_cast<CrossServer*>(ctx);
    server->check_heartbeat();
}

// ============================================================================
// Connection handling (passive connections)
// ============================================================================

void CrossServer::handle_accept(evutil_socket_t fd, struct sockaddr* addr) {
    struct bufferevent* bev = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        SPDLOG_ERROR("[Cross]Failed to create bufferevent for fd {}", fd);
        return;
    }

    auto session = std::make_shared<GameSession>(fd, bev);
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[fd] = session;
    }

    bufferevent_setcb(bev, on_read, nullptr, on_event, this);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    SPDLOG_INFO("[Cross]New connection from fd {}", fd);
}

void CrossServer::handle_read(std::shared_ptr<GameSession> session) {
    struct bufferevent* bev = session->bev();
    uint8_t buf[4096];
    size_t n;

    while ((n = bufferevent_read(bev, buf, sizeof(buf))) > 0) {
        session->append_read_data(buf, n);
    }

    // Parse messages
    ParsedMessage msg;
    size_t consumed;
    while (MessageParser::try_parse(session->read_buffer().data(),
                                     session->read_buffer().size(),
                                     msg, consumed)) {
        route_message(session, msg.msg_id, msg.payload);
        session->consume_read_data(consumed);
    }
}

// handle_disconnect now acquires sessions_mutex_ internally so it can safely
// be called both from unlocked contexts (on_event, check_heartbeat) and from
// code that does not hold the lock.
void CrossServer::handle_disconnect(std::shared_ptr<GameSession> session) {
    SPDLOG_INFO("[Cross]Game server {} disconnected (fd={})",
                session->server_id(), session->fd());

    // Clear route cache for this server (no lock needed, thread-safe cache)
    if (session->server_id() != 0) {
        route_cache_.clear_server(session->server_id());
    }

    // Remove from session maps under lock
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        if (session->server_id() != 0) {
            server_sessions_.erase(session->server_id());
        }
        sessions_.erase(session->fd());
    }
}

void CrossServer::check_heartbeat() {
    time_t now = std::time(nullptr);
    std::vector<std::shared_ptr<GameSession>> to_remove;

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& [fd, session] : sessions_) {
            if (session->state() == GameSessionState::IDENTIFIED) {
                if (now - session->last_heartbeat() > CROSS_HEARTBEAT_TIMEOUT) {
                    SPDLOG_WARN("[Cross]Heartbeat timeout for server {}", session->server_id());
                    to_remove.push_back(session);
                }
            } else if (session->state() == GameSessionState::CONNECTED) {
                if (now - session->connect_time() > CROSS_IDENTIFY_TIMEOUT) {
                    SPDLOG_WARN("[Cross]Identify timeout for fd {}", fd);
                    to_remove.push_back(session);
                }
            }
        }
    }

    // Call handle_disconnect outside the lock -- it acquires sessions_mutex_
    // internally, so holding it here would deadlock.
    for (auto& session : to_remove) {
        handle_disconnect(session);
    }

    // Check active connection timeouts
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& [id, conn] : connections_) {
        if (conn->state() == ConnectionState::IDENTIFIED) {
            if (now - conn->last_heartbeat() > CROSS_HEARTBEAT_TIMEOUT) {
                SPDLOG_WARN("[Cross]Heartbeat timeout for outbound connection to server {}", id);
                // TODO: reconnect logic
            }
        }
    }
}

// ============================================================================
// Message routing (passive connections)
// ============================================================================

void CrossServer::route_message(std::shared_ptr<GameSession> session, uint32_t msg_id,
                                 const std::vector<uint8_t>& payload) {
    switch (msg_id) {
        case MSG_ID_CROSS_IDENTIFY:
            handle_cross_identify(session, payload);
            break;
        case MSG_ID_CROSS_HEARTBEAT:
            handle_cross_heartbeat(session, payload);
            break;
        case MSG_ID_CROSS_QUERY_REQ:
            handle_cross_query_req(session, payload);
            break;
        case MSG_ID_CROSS_FORWARD_RESP:
            handle_cross_forward_resp(session, payload);
            break;
        default:
            SPDLOG_WARN("[Cross]Unknown message ID: {}", msg_id);
            break;
    }
}

void CrossServer::handle_cross_identify(std::shared_ptr<GameSession> session,
                                         const std::vector<uint8_t>& payload) {
    CrossIdentify identify;
    if (!identify.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Cross]Failed to parse CrossIdentify");
        return;
    }

    session->set_server_id(identify.server_id());
    session->set_state(GameSessionState::IDENTIFIED);
    session->update_heartbeat();

    // Add to server_sessions_ mapping
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        server_sessions_[identify.server_id()] = session;
    }

    SPDLOG_INFO("[Cross]Game server {} identified (fd={})", identify.server_id(), session->fd());

    // Send response
    CrossIdentifyResp resp;
    resp.set_code(0);
    resp.set_msg("OK");
    send_to_session(session, MSG_ID_CROSS_IDENTIFY_RESP, resp.SerializeAsString());
}

void CrossServer::handle_cross_heartbeat(std::shared_ptr<GameSession> session,
                                          const std::vector<uint8_t>& payload) {
    session->update_heartbeat();

    CrossHeartbeatResp resp;
    resp.set_timestamp(static_cast<uint64_t>(std::time(nullptr)));
    send_to_session(session, MSG_ID_CROSS_HEARTBEAT_RESP, resp.SerializeAsString());
}

void CrossServer::handle_cross_query_req(std::shared_ptr<GameSession> session,
                                          const std::vector<uint8_t>& payload) {
    CrossQueryReq req;
    if (!req.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Cross]Failed to parse CrossQueryReq");
        return;
    }

    SPDLOG_INFO("[Cross]Query request from server {} for player {} (type={})",
                session->server_id(), req.target_player_id(), static_cast<int>(req.query_type()));

    // Query route
    auto server_id_opt = route_cache_.get_server_id(req.target_player_id());
    if (!server_id_opt.has_value()) {
        // Player not online
        CrossQueryResp resp;
        resp.set_request_id(req.request_id());
        resp.set_code(static_cast<int32_t>(CrossErrorCode::CROSS_PLAYER_NOT_FOUND));
        send_to_session(session, MSG_ID_CROSS_QUERY_RESP, resp.SerializeAsString());
        return;
    }

    uint32_t target_server_id = server_id_opt.value();

    // Find target server connection (prefer active connection, then passive)
    bool found = false;
    bool use_connection = false;  // true=active, false=passive

    // Check active connection
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(target_server_id);
        if (it != connections_.end() && it->second->state() == ConnectionState::IDENTIFIED) {
            found = true;
            use_connection = true;
        }
    }

    // Check passive connection
    if (!found) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = server_sessions_.find(target_server_id);
        if (it != server_sessions_.end() && it->second->state() == GameSessionState::IDENTIFIED) {
            found = true;
            use_connection = false;
        }
    }

    if (!found) {
        // Target server not online
        CrossQueryResp resp;
        resp.set_request_id(req.request_id());
        resp.set_code(static_cast<int32_t>(CrossErrorCode::CROSS_TARGET_SERVER_OFFLINE));
        send_to_session(session, MSG_ID_CROSS_QUERY_RESP, resp.SerializeAsString());
        return;
    }

    // Record pending request
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_requests_[req.request_id()] = {session, std::time(nullptr)};
    }

    // Forward request
    CrossForwardReq forward;
    forward.set_request_id(req.request_id());
    forward.set_source_server_id(session->server_id());
    // CrossQueryReq does not carry source_player_id; set to 0 (not available)
    forward.set_source_player_id(0);
    forward.set_target_player_id(req.target_player_id());
    forward.set_query_type(req.query_type());
    forward.set_request_data(req.request_data());

    std::string serialized;
    forward.SerializeToString(&serialized);

    if (use_connection) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(target_server_id);
        if (it != connections_.end()) {
            it->second->send(MSG_ID_CROSS_FORWARD_REQ, serialized);
        }
    } else {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = server_sessions_.find(target_server_id);
        if (it != server_sessions_.end()) {
            send_to_session(it->second, MSG_ID_CROSS_FORWARD_REQ, serialized);
        }
    }
}

void CrossServer::handle_cross_forward_resp(std::shared_ptr<GameSession> session,
                                             const std::vector<uint8_t>& payload) {
    CrossForwardResp resp;
    if (!resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Cross]Failed to parse CrossForwardResp");
        return;
    }

    // Find corresponding pending request
    std::shared_ptr<GameSession> target_session;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        auto it = pending_requests_.find(resp.request_id());
        if (it == pending_requests_.end()) {
            SPDLOG_WARN("[Cross]No pending request found for request_id={}", resp.request_id());
            return;
        }
        target_session = it->second.session;
        pending_requests_.erase(it);
    }

    // Forward response to requester
    CrossQueryResp query_resp;
    query_resp.set_request_id(resp.request_id());
    query_resp.set_code(resp.code());
    query_resp.set_response_data(resp.response_data());

    send_to_session(target_session, MSG_ID_CROSS_QUERY_RESP, query_resp.SerializeAsString());
}

// ============================================================================
// Active connection message handling
// ============================================================================

void CrossServer::on_connection_message(uint32_t server_id, uint32_t msg_id,
                                         const std::vector<uint8_t>& payload) {
    switch (msg_id) {
        case MSG_ID_CROSS_IDENTIFY_RESP: {
            CrossIdentifyResp resp;
            if (resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
                if (resp.code() == 0) {
                    std::lock_guard<std::mutex> lock(connections_mutex_);
                    auto it = connections_.find(server_id);
                    if (it != connections_.end()) {
                        it->second->set_state(ConnectionState::IDENTIFIED);
                        it->second->update_heartbeat();
                        SPDLOG_INFO("[Cross]Identified with game server {}", server_id);
                    }
                } else {
                    SPDLOG_ERROR("[Cross]Identify failed with server {}: {}", server_id, resp.msg());
                }
            }
            break;
        }
        case MSG_ID_CROSS_HEARTBEAT: {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            auto it = connections_.find(server_id);
            if (it != connections_.end()) {
                it->second->update_heartbeat();
                CrossHeartbeatResp resp;
                resp.set_timestamp(static_cast<uint64_t>(std::time(nullptr)));
                it->second->send(MSG_ID_CROSS_HEARTBEAT_RESP, resp.SerializeAsString());
            }
            break;
        }
        // FIX (Issue 5): Removed MSG_ID_CROSS_FORWARD_REQ case.
        // CROSS_FORWARD_REQ flows FROM CrossServer TO Game Servers, not the
        // other way around. Handling it here and forwarding it back would
        // create a forwarding loop. Game servers respond with
        // CROSS_FORWARD_RESP, which is handled below.
        case MSG_ID_CROSS_FORWARD_RESP: {
            // Forward response from active connection
            CrossForwardResp resp;
            if (resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
                // Find corresponding pending request
                std::shared_ptr<GameSession> target_session;
                {
                    std::lock_guard<std::mutex> lock(pending_mutex_);
                    auto it = pending_requests_.find(resp.request_id());
                    if (it != pending_requests_.end()) {
                        target_session = it->second.session;
                        pending_requests_.erase(it);
                    }
                }

                if (target_session) {
                    CrossQueryResp query_resp;
                    query_resp.set_request_id(resp.request_id());
                    query_resp.set_code(resp.code());
                    query_resp.set_response_data(resp.response_data());
                    send_to_session(target_session, MSG_ID_CROSS_QUERY_RESP, query_resp.SerializeAsString());
                }
            }
            break;
        }
    }
}

void CrossServer::on_connection_disconnect(uint32_t server_id) {
    SPDLOG_WARN("[Cross]Disconnected from game server {}", server_id);
    // Clear route cache for this server
    route_cache_.clear_server(server_id);
}

// ============================================================================
// Helper methods
// ============================================================================

void CrossServer::send_to_session(std::shared_ptr<GameSession> session,
                                   uint32_t msg_id, const std::string& payload) {
    auto data = MessageParser::pack(msg_id, payload);
    bufferevent_write(session->bev(), data.data(), data.size());
}

std::shared_ptr<GameSession> CrossServer::get_session_by_server_id(uint32_t server_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = server_sessions_.find(server_id);
    if (it != server_sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

}  // namespace farm
