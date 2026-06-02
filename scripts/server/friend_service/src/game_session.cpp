#include "game_session.h"
#include "friend_service.h"
#include "message_parser.h"
#include "internal_msg_ids.h"
#include "log_macros.h"

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
#include <ctime>

#include "friend.pb.h"

namespace farm {

static constexpr int HEARTBEAT_INTERVAL_SEC = 5;
static constexpr int HEARTBEAT_TIMEOUT_SEC = 15;
static constexpr int RECONNECT_DELAY_SEC = 5;

GameSession::GameSession(FriendService* service, struct event_base* base)
    : service_(service)
    , base_(base)
{
}

GameSession::~GameSession() {
    disconnect();
}

bool GameSession::connect(const std::string& host, int port) {
    host_ = host;
    port_ = port;

    if (bev_) {
        bufferevent_free(bev_);
        bev_ = nullptr;
    }

    bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
    if (!bev_) {
        SPDLOG_ERROR("[GameSession]Failed to create bufferevent");
        return false;
    }

    bufferevent_setcb(bev_, on_read, nullptr, on_event, this);
    bufferevent_enable(bev_, EV_READ | EV_WRITE);

    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, host.c_str(), &sin.sin_addr);

    int ret = bufferevent_socket_connect(bev_,
        reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin));
    if (ret < 0) {
        SPDLOG_ERROR("[GameSession]connect failed to {}:{}", host, port);
        bufferevent_free(bev_);
        bev_ = nullptr;
        return false;
    }

    SPDLOG_INFO("[GameSession]Connecting to Game Server at {}:{}", host, port);
    return true;
}

void GameSession::disconnect() {
    stop_heartbeat();

    if (reconnect_timer_) {
        event_free(reconnect_timer_);
        reconnect_timer_ = nullptr;
    }

    if (bev_) {
        bufferevent_free(bev_);
        bev_ = nullptr;
    }

    state_ = GameSessionState::DISCONNECTED;
    read_buffer_.clear();
}

void GameSession::send(uint32_t msg_id, const std::string& payload) {
    if (!bev_) return;
    auto packed = MessageParser::pack(msg_id, payload);
    bufferevent_write(bev_, packed.data(), packed.size());
}

void GameSession::send_to_client(uint64_t player_id, uint32_t msg_id,
                                  const std::string& payload) {
    if (!is_identified()) {
        SPDLOG_WARN("[GameSession]Cannot send to client: not identified");
        return;
    }

    farm::FriendServiceMessage msg;
    msg.set_player_id(player_id);
    msg.set_msg_id(msg_id);
    msg.set_payload(payload);

    std::string data;
    msg.SerializeToString(&data);
    send(MSG_ID_FRIEND_SERVICE_MSG, data);
}

// ===========================================
// libevent callbacks (static)
// ===========================================

void GameSession::on_read(struct bufferevent* bev, void* ctx) {
    auto* session = static_cast<GameSession*>(ctx);
    session->handle_read();
}

void GameSession::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* session = static_cast<GameSession*>(ctx);
    session->handle_event(events);
}

void GameSession::handle_read() {
    struct evbuffer* input = bufferevent_get_input(bev_);

    char buf[4096];
    while (true) {
        ev_ssize_t n = evbuffer_remove(input, buf, sizeof(buf));
        if (n <= 0) break;
        read_buffer_.insert(read_buffer_.end(),
                            reinterpret_cast<uint8_t*>(buf),
                            reinterpret_cast<uint8_t*>(buf) + n);
    }

    while (!read_buffer_.empty()) {
        ParsedMessage msg;
        size_t consumed = 0;
        if (MessageParser::try_parse(read_buffer_.data(), read_buffer_.size(),
                                      msg, consumed)) {
            // Consume parsed bytes
            if (consumed >= read_buffer_.size()) {
                read_buffer_.clear();
            } else {
                read_buffer_.erase(read_buffer_.begin(),
                                   read_buffer_.begin() + consumed);
            }
            route_message(msg.msg_id, msg.payload.data(), msg.payload.size());
        } else {
            break;
        }
    }
}

void GameSession::handle_event(short events) {
    if (events & BEV_EVENT_CONNECTED) {
        SPDLOG_INFO("[GameSession]TCP connected to Game Server at {}:{}", host_, port_);
        state_ = GameSessionState::CONNECTED;
        send_identify();
        return;
    }

    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        SPDLOG_INFO("[GameSession]Disconnected from Game Server (events=0x{:x})", events);
        disconnect();
        schedule_reconnect();
    }
}

void GameSession::route_message(uint32_t msg_id, const uint8_t* data, size_t len) {
    switch (msg_id) {
        case MSG_ID_FRIEND_SERVICE_IDENTIFY_RESP: {
            farm::FriendServiceIdentifyResp resp;
            if (len > 0 && !resp.ParseFromArray(data, static_cast<int>(len))) {
                SPDLOG_ERROR("[GameSession]Failed to parse FriendServiceIdentifyResp");
                return;
            }
            if (resp.code() == 0) {
                state_ = GameSessionState::IDENTIFIED;
                last_heartbeat_recv_ = std::time(nullptr);
                start_heartbeat();
                SPDLOG_INFO("[GameSession]Identified with Game Server");
            } else {
                SPDLOG_ERROR("[GameSession]Identify rejected: code={}", resp.code());
            }
            break;
        }
        case MSG_ID_FRIEND_SERVICE_HEARTBEAT: {
            last_heartbeat_recv_ = std::time(nullptr);

            farm::FriendServiceHeartbeatResp resp;
            resp.set_timestamp(static_cast<uint64_t>(std::time(nullptr)));
            std::string resp_data;
            resp.SerializeToString(&resp_data);
            send(MSG_ID_FRIEND_SERVICE_HEARTBEAT_RESP, resp_data);
            break;
        }
        case MSG_ID_FRIEND_CLIENT_MSG: {
            farm::FriendClientMessage client_msg;
            if (len > 0 && !client_msg.ParseFromArray(data, static_cast<int>(len))) {
                SPDLOG_ERROR("[GameSession]Failed to parse FriendClientMessage");
                return;
            }
            service_->handle_client_message(
                client_msg.player_id(),
                client_msg.msg_id(),
                reinterpret_cast<const uint8_t*>(client_msg.payload().data()),
                client_msg.payload().size());
            break;
        }
        default:
            SPDLOG_INFO("[GameSession]Unknown msg_id={} from Game Server", msg_id);
            break;
    }
}

// ===========================================
// Identify
// ===========================================

void GameSession::send_identify() {
    farm::FriendServiceIdentify identify;
    identify.set_service_id(1);  // Friend Service ID
    identify.set_address("friend_service");

    std::string data;
    identify.SerializeToString(&data);
    send(MSG_ID_FRIEND_SERVICE_IDENTIFY, data);
    SPDLOG_INFO("[GameSession]Sent FriendServiceIdentify");
}

// ===========================================
// Heartbeat
// ===========================================

void GameSession::start_heartbeat() {
    stop_heartbeat();

    struct timeval tv;
    tv.tv_sec = HEARTBEAT_INTERVAL_SEC;
    tv.tv_usec = 0;
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    evtimer_add(heartbeat_timer_, &tv);
}

void GameSession::stop_heartbeat() {
    if (heartbeat_timer_) {
        event_free(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
}

void GameSession::on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* session = static_cast<GameSession*>(ctx);

    time_t now = std::time(nullptr);
    if (now - session->last_heartbeat_recv_ > HEARTBEAT_TIMEOUT_SEC) {
        SPDLOG_INFO("[GameSession]Heartbeat timeout, disconnecting");
        session->disconnect();
        session->schedule_reconnect();
        return;
    }

    // Send heartbeat
    farm::FriendServiceHeartbeat hb;
    hb.set_timestamp(static_cast<uint64_t>(now));
    std::string data;
    hb.SerializeToString(&data);
    session->send(MSG_ID_FRIEND_SERVICE_HEARTBEAT, data);
}

// ===========================================
// Reconnect
// ===========================================

void GameSession::schedule_reconnect() {
    if (reconnect_timer_) {
        event_free(reconnect_timer_);
        reconnect_timer_ = nullptr;
    }

    struct timeval tv;
    tv.tv_sec = RECONNECT_DELAY_SEC;
    tv.tv_usec = 0;
    reconnect_timer_ = event_new(base_, -1, 0, on_reconnect_timer, this);
    evtimer_add(reconnect_timer_, &tv);

    SPDLOG_INFO("[GameSession]Reconnect scheduled in {} seconds", RECONNECT_DELAY_SEC);
}

void GameSession::on_reconnect_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* session = static_cast<GameSession*>(ctx);
    session->reconnect_timer_ = nullptr;

    SPDLOG_INFO("[GameSession]Attempting reconnect to {}:{}", session->host_, session->port_);
    session->connect(session->host_, session->port_);
}

}  // namespace farm
