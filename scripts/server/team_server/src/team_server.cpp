#include "team_server.h"
#include "internal_msg_ids.h"
#include "message_ids.h"
#include "message_parser.h"
#include "log_macros.h"

#include "internal.pb.h"
#include "team.pb.h"

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

static constexpr int INVITE_TIMEOUT_INTERVAL = 5;  // 每 5 秒检查一次邀请超时

TeamServer::TeamServer(const std::string& ip, uint16_t port)
    : ip_(ip), port_(port), base_(nullptr), listener_(nullptr),
      invite_timeout_timer_(nullptr), running_(false),
      team_mgr_([this](uint64_t player_id, uint32_t msg_id,
                        const uint8_t* payload, size_t len) {
          send_to_player(player_id, msg_id, payload, len);
      })
{
}

TeamServer::~TeamServer() { stop(); }

bool TeamServer::start() {
    base_ = event_base_new();
    if (!base_) {
        SPDLOG_ERROR("[Team]Failed to create event_base");
        return false;
    }

    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port_);
    if (ip_.empty() || ip_ == "0.0.0.0") {
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        inet_pton(AF_INET, ip_.c_str(), &sin.sin_addr);
    }

    listener_ = evconnlistener_new_bind(
        base_, on_accept, this,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
        128, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin));

    if (!listener_) {
        SPDLOG_ERROR("[Team]Failed to create listener on {}:{}", ip_, port_);
        event_base_free(base_);
        base_ = nullptr;
        return false;
    }

    // Invite timeout timer
    struct timeval tv;
    tv.tv_sec = INVITE_TIMEOUT_INTERVAL;
    tv.tv_usec = 0;
    invite_timeout_timer_ = event_new(base_, -1, EV_PERSIST, on_invite_timeout_timer, this);
    evtimer_add(invite_timeout_timer_, &tv);

    running_ = true;
    SPDLOG_INFO("[Team]Listening on {}:{}", ip_, port_);

    event_base_dispatch(base_);

    running_ = false;
    return true;
}

void TeamServer::stop() {
    running_ = false;
    if (invite_timeout_timer_) {
        event_free(invite_timeout_timer_);
        invite_timeout_timer_ = nullptr;
    }
    if (listener_) {
        evconnlistener_free(listener_);
        listener_ = nullptr;
    }
    if (base_) {
        event_base_loopexit(base_, nullptr);
        event_base_free(base_);
        base_ = nullptr;
    }
}

// libevent callbacks (delegate to instance methods)
void TeamServer::on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                           struct sockaddr* addr, int len, void* ctx) {
    auto* self = static_cast<TeamServer*>(ctx);
    self->handle_accept(fd, addr);
}

void TeamServer::on_read(struct bufferevent* bev, void* ctx) {
    auto* session_ptr = static_cast<std::shared_ptr<TeamGateSession>*>(ctx);
    // We need to find the TeamServer instance - store it in session or use a static approach
    // For now, we'll use a static approach similar to ChatServer
    // This is a simplified version - in production, you'd want a more robust approach
}

void TeamServer::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* session_ptr = static_cast<std::shared_ptr<TeamGateSession>*>(ctx);
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        // Handle disconnect
    }
}

void TeamServer::on_invite_timeout_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* self = static_cast<TeamServer*>(ctx);
    self->team_mgr_.check_invite_timeouts();
}

void TeamServer::handle_accept(evutil_socket_t fd, struct sockaddr* addr) {
    auto session = std::make_shared<TeamGateSession>();
    session->fd = fd;
    session->identified = false;

    struct bufferevent* bev = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE);
    session->bev = bev;

    auto* session_ptr = new std::shared_ptr<TeamGateSession>(session);
    bufferevent_setcb(bev, on_read, nullptr, on_event, session_ptr);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    gate_sessions_[fd] = session;
    SPDLOG_INFO("[Team]Gate connection from fd={}", fd);
}

void TeamServer::handle_read(std::shared_ptr<TeamGateSession> session) {
    // Read and parse messages from buffer - same pattern as ChatServer
    struct evbuffer* input = bufferevent_get_input(session->bev);
    size_t len = evbuffer_get_length(input);
    if (len == 0) return;

    session->read_buffer.resize(session->read_buffer.size() + len);
    evbuffer_remove(input, session->read_buffer.data() + session->read_buffer.size() - len, len);

    // Parse messages
    size_t offset = 0;
    while (offset < session->read_buffer.size()) {
        if (session->read_buffer.size() - offset < 4) break;

        uint32_t msg_len;
        std::memcpy(&msg_len, session->read_buffer.data() + offset, 4);
        msg_len = ntohl(msg_len);

        if (session->read_buffer.size() - offset < 4 + msg_len) break;

        uint32_t msg_id;
        std::memcpy(&msg_id, session->read_buffer.data() + offset + 4, 4);
        msg_id = ntohl(msg_id);

        const uint8_t* payload = session->read_buffer.data() + offset + 8;
        size_t payload_len = msg_len - 4;

        route_message(session, msg_id, std::vector<uint8_t>(payload, payload + payload_len));
        offset += 4 + msg_len;
    }

    if (offset > 0) {
        session->read_buffer.erase(session->read_buffer.begin(),
                                   session->read_buffer.begin() + offset);
    }
}

void TeamServer::handle_disconnect(std::shared_ptr<TeamGateSession> session) {
    SPDLOG_INFO("[Team]Gate disconnected: fd={}", session->fd);
    gate_sessions_.erase(session->fd);
    bufferevent_free(session->bev);
}

void TeamServer::route_message(std::shared_ptr<TeamGateSession> session,
                               uint32_t msg_id, const std::vector<uint8_t>& payload) {
    // Internal protocol messages
    if (msg_id == MSG_ID_TEAM_SERVICE_IDENTIFY) {
        handle_gate_identify(session, payload);
        return;
    }
    if (msg_id == MSG_ID_TEAM_SERVICE_HEARTBEAT) {
        handle_heartbeat(session, payload);
        return;
    }
    if (msg_id == MSG_ID_TEAM_CLIENT_MSG) {
        handle_client_msg(session, payload);
        return;
    }
    // Internal query from GameServer/ChatServer
    if (msg_id == MSG_ID_TEAM_QUERY_MEMBERS_REQ) {
        handle_team_query_members_req(session, payload.data(), payload.size());
        return;
    }

    SPDLOG_WARN("[Team]Unknown msg_id={}", msg_id);
}

void TeamServer::handle_gate_identify(std::shared_ptr<TeamGateSession> session,
                                      const std::vector<uint8_t>& payload) {
    farm::GateIdentify identify;
    if (identify.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        session->gate_id = identify.gate_id();
        session->identified = true;
        SPDLOG_INFO("[Team]Gate identified: {}", session->gate_id);

        farm::GateIdentifyResp resp;
        resp.set_code(0);
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        send_to_gate(session, MSG_ID_TEAM_SERVICE_IDENTIFY_RESP, resp_data);
    }
}

void TeamServer::handle_heartbeat(std::shared_ptr<TeamGateSession> session,
                                  const std::vector<uint8_t>& payload) {
    farm::InternHeartbeat hb;
    if (hb.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        farm::InternHeartbeatResp resp;
        resp.set_timestamp(hb.timestamp());
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        send_to_gate(session, MSG_ID_TEAM_SERVICE_HEARTBEAT_RESP, resp_data);
    }
}

void TeamServer::handle_player_join(std::shared_ptr<TeamGateSession> session,
                                    const std::vector<uint8_t>& payload) {
    farm::PlayerJoin join;
    if (join.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        uint64_t player_id = join.player_id();
        player_to_gate_[player_id] = session;
        SPDLOG_INFO("[Team]Player {} joined", player_id);

        // Send current team info if player is in a team
        auto* team = team_mgr_.get_player_team(player_id);
        if (team) {
            // Send team info to the reconnected player
            farm::TeamInfoResp info_resp;
            info_resp.set_code(farm::TEAM_SUCCESS);
            info_resp.set_team_id(team->team_id);
            info_resp.set_leader_id(team->leader_id);
            info_resp.set_status(static_cast<uint32_t>(team->status));
            info_resp.set_cave_level(team->cave_level);
            for (uint64_t mid : team->members) {
                auto* member = info_resp.add_members();
                member->set_player_id(mid);
                member->set_is_leader(mid == team->leader_id);
                // TODO: populate role_name, level, online, hp from player data
            }
            std::string resp_data;
            info_resp.SerializeToString(&resp_data);
            send_to_player(player_id, MSG_ID_TEAM_INFO_RESP, resp_data);
        }

        farm::PlayerJoinResp resp;
        resp.set_player_id(player_id);
        resp.set_code(0);
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        // Send back to gate
    }
}

void TeamServer::handle_player_leave(std::shared_ptr<TeamGateSession> session,
                                     const std::vector<uint8_t>& payload) {
    farm::PlayerLeave leave;
    if (leave.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        uint64_t player_id = leave.player_id();
        player_to_gate_.erase(player_id);
        SPDLOG_INFO("[Team]Player {} left", player_id);
    }
}

void TeamServer::handle_client_msg(std::shared_ptr<TeamGateSession> session,
                                   const std::vector<uint8_t>& payload) {
    farm::ClientMessage client_msg;
    if (!client_msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) return;

    uint64_t player_id = client_msg.player_id();
    uint32_t msg_id = client_msg.msg_id();
    const auto& inner_payload = client_msg.payload();

    switch (msg_id) {
        case MSG_ID_TEAM_CREATE_REQ:
            handle_team_create_req(player_id,
                                   reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                   inner_payload.size());
            break;
        case MSG_ID_TEAM_DISBAND_REQ:
            handle_team_disband_req(player_id,
                                    reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                    inner_payload.size());
            break;
        case MSG_ID_TEAM_INVITE_REQ:
            handle_team_invite_req(player_id,
                                   reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                   inner_payload.size());
            break;
        case MSG_ID_TEAM_ACCEPT_REQ:
            handle_team_accept_req(player_id,
                                   reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                   inner_payload.size());
            break;
        case MSG_ID_TEAM_REJECT_REQ:
            handle_team_reject_req(player_id,
                                   reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                   inner_payload.size());
            break;
        case MSG_ID_TEAM_LEAVE_REQ:
            handle_team_leave_req(player_id,
                                  reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                  inner_payload.size());
            break;
        case MSG_ID_TEAM_KICK_REQ:
            handle_team_kick_req(player_id,
                                 reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                 inner_payload.size());
            break;
        case MSG_ID_TEAM_INFO_REQ:
            handle_team_info_req(player_id,
                                 reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                 inner_payload.size());
            break;
        default:
            SPDLOG_WARN("[Team]Unknown client msg_id={}", msg_id);
            break;
    }
}

void TeamServer::send_to_gate(std::shared_ptr<TeamGateSession> session,
                              uint32_t msg_id, std::string_view payload) {
    if (!session || !session->bev) return;

    // Frame: [len(4)] [msg_id(4)] [payload]
    uint32_t frame_len = 4 + static_cast<uint32_t>(payload.size());
    uint32_t net_len = htonl(frame_len);
    uint32_t net_msg_id = htonl(msg_id);

    struct evbuffer* output = bufferevent_get_output(session->bev);
    evbuffer_add(output, &net_len, 4);
    evbuffer_add(output, &net_msg_id, 4);
    evbuffer_add(output, payload.data(), payload.size());
}

void TeamServer::send_to_player(uint64_t player_id, uint32_t msg_id,
                                const uint8_t* payload, size_t len) {
    auto it = player_to_gate_.find(player_id);
    if (it == player_to_gate_.end()) return;

    auto session = it->second;
    if (!session || !session->identified) return;

    // Wrap in GameMessage for Gate to forward
    farm::GameMessage game_msg;
    game_msg.set_player_id(player_id);
    game_msg.set_msg_id(msg_id);
    game_msg.set_payload(payload, len);

    std::string data;
    game_msg.SerializeToString(&data);
    send_to_gate(session, MSG_ID_TEAM_SERVICE_MSG, data);
}

void TeamServer::broadcast_to_team(uint64_t team_id, uint32_t msg_id,
                                   const uint8_t* payload, size_t len,
                                   uint64_t exclude_player_id) {
    auto* team = team_mgr_.get_team(team_id);
    if (!team) return;

    for (uint64_t member_id : team->members) {
        if (member_id != exclude_player_id) {
            send_to_player(member_id, msg_id, payload, len);
        }
    }
}

// Team message handlers - implemented in Task 5
void TeamServer::handle_team_create_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_disband_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_invite_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_accept_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_reject_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_leave_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_kick_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_info_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_query_members_req(std::shared_ptr<TeamGateSession> session,
                                                const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

}  // namespace farm
