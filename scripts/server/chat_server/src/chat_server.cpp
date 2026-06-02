#include "chat_server.h"
#include "internal_msg_ids.h"
#include "message_ids.h"
#include "message_parser.h"
#include "log_macros.h"

#include "internal.pb.h"
#include "chat.pb.h"

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/event.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include <cstring>

namespace farm {

ChatServer::ChatServer(const std::string& ip, uint16_t port)
    : ip_(ip)
    , port_(port)
    , base_(nullptr)
    , listener_(nullptr)
    , running_(false)
{
}

ChatServer::~ChatServer() {
    stop();
}

bool ChatServer::start() {
    base_ = event_base_new();
    if (!base_) {
        SPDLOG_ERROR("[Chat]Failed to create event_base");
        return false;
    }

    // Initialize ChannelManager with send callback
    channel_mgr_ = std::make_unique<ChannelManager>(
        [this](uint64_t player_id, uint32_t msg_id,
               const uint8_t* payload, size_t len) {
            send_to_player(player_id, msg_id, payload, len);
        });

    // Bind address
    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port_);
    if (ip_.empty() || ip_ == "0.0.0.0") {
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        inet_pton(AF_INET, ip_.c_str(), &sin.sin_addr);
    }

    // Create listener
    listener_ = evconnlistener_new_bind(
        base_, on_accept, this,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
        128, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin));

    if (!listener_) {
        SPDLOG_ERROR("[Chat]Failed to create listener on {}:{}", ip_, port_);
        event_base_free(base_);
        base_ = nullptr;
        return false;
    }

    running_ = true;
    SPDLOG_INFO("[Chat]Listening on {}:{}", ip_, port_);

    // Enter event loop (blocking)
    event_base_dispatch(base_);

    running_ = false;
    return true;
}

void ChatServer::stop() {
    running_ = false;
    if (listener_) {
        evconnlistener_free(listener_);
        listener_ = nullptr;
    }
    gate_sessions_.clear();
    player_to_gate_.clear();
    if (base_) {
        event_base_loopexit(base_, nullptr);
        event_base_free(base_);
        base_ = nullptr;
    }
}

// ===========================================
// libevent callbacks
// ===========================================

void ChatServer::on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                           struct sockaddr* addr, int len, void* ctx) {
    auto* server = static_cast<ChatServer*>(ctx);
    server->handle_accept(fd, addr);
}

void ChatServer::handle_accept(evutil_socket_t fd, struct sockaddr* addr) {
    // Create bufferevent
    struct bufferevent* bev = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        SPDLOG_ERROR("[Chat]Failed to create bufferevent for fd={}", fd);
        evutil_closesocket(fd);
        return;
    }

    // Create session
    auto session = std::make_shared<ChatGateSession>();
    session->fd = fd;
    session->bev = bev;
    session->identified = false;
    gate_sessions_[fd] = session;

    // Set callbacks
    bufferevent_setcb(bev, on_read, nullptr, on_event, this);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    // Log client address
    char ip_str[INET_ADDRSTRLEN] = {0};
    if (addr->sa_family == AF_INET) {
        auto* sin = reinterpret_cast<struct sockaddr_in*>(addr);
        inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));
        SPDLOG_INFO("[Chat]New Gate connection from {}:{} fd={}",
                    ip_str, ntohs(sin->sin_port), fd);
    }
}

void ChatServer::on_read(struct bufferevent* bev, void* ctx) {
    auto* server = static_cast<ChatServer*>(ctx);
    evutil_socket_t fd = bufferevent_getfd(bev);
    auto it = server->gate_sessions_.find(fd);
    if (it == server->gate_sessions_.end()) {
        return;
    }
    server->handle_read(it->second);
}

void ChatServer::handle_read(std::shared_ptr<ChatGateSession> session) {
    struct bufferevent* bev = session->bev;
    struct evbuffer* input = bufferevent_get_input(bev);

    // Read all available data into session buffer
    char buf[4096];
    while (true) {
        ev_ssize_t n = evbuffer_remove(input, buf, sizeof(buf));
        if (n <= 0) break;
        session->read_buffer.insert(session->read_buffer.end(),
                                     reinterpret_cast<const uint8_t*>(buf),
                                     reinterpret_cast<const uint8_t*>(buf) + n);
    }

    // Try to parse messages
    while (!session->read_buffer.empty()) {
        ParsedMessage msg;
        size_t consumed = 0;
        if (MessageParser::try_parse(session->read_buffer.data(),
                                      session->read_buffer.size(), msg, consumed)) {
            session->read_buffer.erase(session->read_buffer.begin(),
                                        session->read_buffer.begin() + consumed);
            route_message(session, msg.msg_id, msg.payload);
        } else {
            break;  // Incomplete data, wait for more
        }
    }
}

void ChatServer::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* server = static_cast<ChatServer*>(ctx);
    evutil_socket_t fd = bufferevent_getfd(bev);
    auto it = server->gate_sessions_.find(fd);

    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        if (it != server->gate_sessions_.end()) {
            server->handle_disconnect(it->second);
        }
    }
}

void ChatServer::handle_disconnect(std::shared_ptr<ChatGateSession> session) {
    evutil_socket_t fd = session->fd;
    SPDLOG_INFO("[Chat]Gate disconnected fd={} gate_id={}", fd, session->gate_id);

    // Clean up player_to_gate_ entries for this session
    std::vector<uint64_t> players_to_remove;
    for (auto& [player_id, gate_session] : player_to_gate_) {
        if (gate_session->fd == fd) {
            players_to_remove.push_back(player_id);
        }
    }
    for (uint64_t pid : players_to_remove) {
        channel_mgr_->remove_player(pid);
        rate_limiter_.remove_player(pid);
        player_to_gate_.erase(pid);
    }

    // Remove session
    gate_sessions_.erase(fd);
}

// ===========================================
// Message routing
// ===========================================

void ChatServer::route_message(std::shared_ptr<ChatGateSession> session,
                               uint32_t msg_id, const std::vector<uint8_t>& payload) {
    // Before identification, only allow GATE_IDENTIFY
    if (!session->identified) {
        if (msg_id == MSG_ID_GATE_IDENTIFY) {
            handle_gate_identify(session, payload);
        } else {
            SPDLOG_INFO("[Chat]Ignoring msg_id={} from unidentified Gate fd={}",
                        msg_id, session->fd);
        }
        return;
    }

    // Identified Gate: route all internal messages
    switch (msg_id) {
        case MSG_ID_INTERN_HEARTBEAT:
            handle_heartbeat(session, payload);
            break;
        case MSG_ID_PLAYER_JOIN:
            handle_player_join(session, payload);
            break;
        case MSG_ID_PLAYER_LEAVE:
            handle_player_leave(session, payload);
            break;
        case MSG_ID_CLIENT_MSG:
            handle_client_msg(session, payload);
            break;
        default:
            SPDLOG_INFO("[Chat]Unknown internal msg_id={} from fd={}",
                        msg_id, session->fd);
            break;
    }
}

// ===========================================
// Specific message handlers
// ===========================================

void ChatServer::handle_gate_identify(std::shared_ptr<ChatGateSession> session,
                                      const std::vector<uint8_t>& payload) {
    GateIdentify req;
    if (!payload.empty() && !req.ParseFromArray(payload.data(),
                                                 static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Chat]Failed to parse GateIdentify");
        return;
    }

    session->gate_id = req.gate_id();
    session->identified = true;

    SPDLOG_INFO("[Chat]Gate identified: gate_id={} address={} fd={}",
                req.gate_id(), req.address(), session->fd);

    // Reply with GateIdentifyResp
    GateIdentifyResp resp;
    resp.set_code(0);
    resp.set_msg("identified");
    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_gate(session, MSG_ID_GATE_IDENTIFY_RESP, resp_data);
}

void ChatServer::handle_heartbeat(std::shared_ptr<ChatGateSession> session,
                                  const std::vector<uint8_t>& payload) {
    InternHeartbeat hb;
    if (!payload.empty() && !hb.ParseFromArray(payload.data(),
                                                static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Chat]Failed to parse InternHeartbeat");
        return;
    }

    // Reply with heartbeat response
    InternHeartbeatResp resp;
    resp.set_timestamp(static_cast<uint64_t>(std::time(nullptr)));
    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_gate(session, MSG_ID_INTERN_HEARTBEAT_RESP, resp_data);
}

void ChatServer::handle_player_join(std::shared_ptr<ChatGateSession> session,
                                    const std::vector<uint8_t>& payload) {
    PlayerJoin req;
    if (!payload.empty() && !req.ParseFromArray(payload.data(),
                                                 static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Chat]Failed to parse PlayerJoin");
        return;
    }

    uint64_t player_id = req.player_id();
    SPDLOG_INFO("[Chat]Player join: player_id={} from gate_id={}",
                player_id, session->gate_id);

    // Register player in channel manager
    // Player name will be set when the first chat message arrives
    channel_mgr_->add_player(player_id, "Player_" + std::to_string(player_id));

    // Store player -> gate mapping
    player_to_gate_[player_id] = session;

    // Send PlayerJoinResp
    PlayerJoinResp resp;
    resp.set_player_id(player_id);
    resp.set_code(0);
    resp.set_msg("joined");
    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_gate(session, MSG_ID_PLAYER_JOIN_RESP, resp_data);
}

void ChatServer::handle_player_leave(std::shared_ptr<ChatGateSession> session,
                                     const std::vector<uint8_t>& payload) {
    PlayerLeave req;
    if (!payload.empty() && !req.ParseFromArray(payload.data(),
                                                 static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Chat]Failed to parse PlayerLeave");
        return;
    }

    uint64_t player_id = req.player_id();
    SPDLOG_INFO("[Chat]Player leave: player_id={} from gate_id={}",
                player_id, session->gate_id);

    // Clean up
    channel_mgr_->remove_player(player_id);
    rate_limiter_.remove_player(player_id);
    player_to_gate_.erase(player_id);
}

void ChatServer::handle_client_msg(std::shared_ptr<ChatGateSession> session,
                                   const std::vector<uint8_t>& payload) {
    ClientMessage req;
    if (!payload.empty() && !req.ParseFromArray(payload.data(),
                                                 static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Chat]Failed to parse ClientMessage");
        return;
    }

    uint64_t player_id = req.player_id();
    uint32_t msg_id = req.msg_id();
    const std::string& inner_payload = req.payload();

    // Route by inner message ID
    switch (msg_id) {
        case MSG_ID_CHAT_SEND_REQ:
            handle_chat_send_req(player_id,
                                 reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                 inner_payload.size());
            break;
        default:
            SPDLOG_INFO("[Chat]Unknown client msg_id={} from player_id={}",
                        msg_id, player_id);
            break;
    }
}

void ChatServer::handle_chat_send_req(uint64_t player_id,
                                      const uint8_t* payload, size_t len) {
    ChatSendReq req;
    if (len > 0 && !req.ParseFromArray(payload, static_cast<int>(len))) {
        SPDLOG_ERROR("[Chat]Failed to parse ChatSendReq from player_id={}", player_id);
        return;
    }

    uint32_t channel_type = static_cast<uint32_t>(req.channel_type());
    const std::string& content = req.content();
    uint64_t target_id = req.target_id();

    SPDLOG_INFO("[Chat]ChatSendReq: player_id={} channel={} content_len={} target={}",
                player_id, channel_type, content.size(), target_id);

    // Rate limit check
    ChannelConfig config = ChannelManager::get_channel_config(channel_type);
    if (!rate_limiter_.check_limit(player_id, channel_type, config.cooldown_sec)) {
        SPDLOG_INFO("[Chat]Rate limited: player_id={} channel={}", player_id, channel_type);
        ChatSendResp resp;
        resp.set_code(CHAT_RATE_LIMITED);
        resp.set_msg("Sending too fast");
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        send_to_player(player_id, MSG_ID_CHAT_SEND_RESP,
                       reinterpret_cast<const uint8_t*>(resp_data.data()),
                       resp_data.size());
        return;
    }

    // Process message through channel manager
    int result = channel_mgr_->process_message(player_id, channel_type, content, target_id);

    // Update rate limiter on success
    if (result == 0) {
        rate_limiter_.update(player_id, channel_type);
    }

    // Send response
    ChatSendResp resp;
    resp.set_code(static_cast<ChatErrorCode>(result));
    if (result == 0) {
        resp.set_msg("success");
    } else {
        resp.set_msg("failed");
    }
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_to_player(player_id, MSG_ID_CHAT_SEND_RESP,
                   reinterpret_cast<const uint8_t*>(resp_data.data()),
                   resp_data.size());
}

// ===========================================
// Send helpers
// ===========================================

void ChatServer::send_to_gate(std::shared_ptr<ChatGateSession> session,
                              uint32_t msg_id, std::string_view payload) {
    auto packed = MessageParser::pack(msg_id, payload);
    bufferevent_write(session->bev, packed.data(), packed.size());
}

void ChatServer::send_to_player(uint64_t player_id, uint32_t msg_id,
                                const uint8_t* payload, size_t len) {
    auto it = player_to_gate_.find(player_id);
    if (it == player_to_gate_.end()) {
        SPDLOG_WARN("[Chat]Cannot send to player_id={}: no gate session", player_id);
        return;
    }

    // Wrap as GameMessage
    GameMessage game_msg;
    game_msg.set_player_id(player_id);
    game_msg.set_msg_id(msg_id);
    game_msg.set_payload(payload, len);

    std::string resp_data;
    game_msg.SerializeToString(&resp_data);

    send_to_gate(it->second, MSG_ID_GAME_MSG, resp_data);
}

}  // namespace farm
