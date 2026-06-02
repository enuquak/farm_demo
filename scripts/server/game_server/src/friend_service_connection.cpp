#include "friend_service_connection.h"
#include "message_parser.h"
#include "internal_msg_ids.h"
#include "message_ids.h"
#include "log_macros.h"

#include <friend.pb.h>
#include <base.pb.h>
#include <event2/buffer.h>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace farm {

FriendServiceConnection::FriendServiceConnection(struct event_base* base) : base_(base) {}

FriendServiceConnection::~FriendServiceConnection() { disconnect(); }

bool FriendServiceConnection::connect(const std::string& host, int port) {
    host_ = host;
    port_ = port;
    bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
    if (!bev_) return false;
    bufferevent_setcb(bev_, on_read, nullptr, on_event, this);
    bufferevent_enable(bev_, EV_READ | EV_WRITE);
    struct sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(static_cast<uint16_t>(port));
    evutil_inet_pton(AF_INET, host.c_str(), &sin.sin_addr);
    if (bufferevent_socket_connect(bev_, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin)) < 0) {
        bufferevent_free(bev_);
        bev_ = nullptr;
        return false;
    }
    state_ = FriendServiceState::CONNECTED;
    return true;
}

void FriendServiceConnection::disconnect() {
    if (bev_) {
        bufferevent_free(bev_);
        bev_ = nullptr;
    }
    state_ = FriendServiceState::DISCONNECTED;
    stop_heartbeat();
}

void FriendServiceConnection::send_client_msg(uint64_t player_id, uint32_t msg_id, const std::string& payload) {
    if (!is_identified()) return;
    FriendClientMessage msg;
    msg.set_player_id(player_id);
    msg.set_msg_id(msg_id);
    msg.set_payload(payload);
    std::string serialized;
    msg.SerializeToString(&serialized);
    auto packed = MessageParser::pack(MSG_ID_FRIEND_CLIENT_MSG, serialized);
    bufferevent_write(bev_, packed.data(), packed.size());
}

void FriendServiceConnection::on_read(struct bufferevent* bev, void* ctx) {
    static_cast<FriendServiceConnection*>(ctx)->handle_read();
}

void FriendServiceConnection::on_event(struct bufferevent* bev, short events, void* ctx) {
    static_cast<FriendServiceConnection*>(ctx)->handle_event(events);
}

void FriendServiceConnection::handle_read() {
    struct evbuffer* input = bufferevent_get_input(bev_);
    size_t len = evbuffer_get_length(input);
    if (len == 0) return;
    size_t old_size = read_buffer_.size();
    read_buffer_.resize(old_size + len);
    evbuffer_remove(input, read_buffer_.data() + old_size, len);
    while (true) {
        ParsedMessage msg;
        size_t consumed = 0;
        if (!MessageParser::try_parse(read_buffer_.data(), read_buffer_.size(), msg, consumed)) break;
        route_message(msg.msg_id, msg.payload.data(), msg.payload.size());
        if (consumed >= read_buffer_.size()) {
            read_buffer_.clear();
        } else {
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + consumed);
        }
    }
}

void FriendServiceConnection::handle_event(short events) {
    if (events & BEV_EVENT_CONNECTED) {
        state_ = FriendServiceState::CONNECTED;
        send_identify();
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        disconnect();
        schedule_reconnect();
    }
}

void FriendServiceConnection::route_message(uint32_t msg_id, const uint8_t* data, size_t len) {
    switch (msg_id) {
        case MSG_ID_FRIEND_SERVICE_IDENTIFY_RESP: {
            FriendServiceIdentifyResp resp;
            if (resp.ParseFromArray(data, static_cast<int>(len)) && resp.code() == 0) {
                state_ = FriendServiceState::IDENTIFIED;
                start_heartbeat();
                SPDLOG_INFO("[Game]FriendService identified");
            }
            break;
        }
        case MSG_ID_FRIEND_SERVICE_HEARTBEAT: {
            last_heartbeat_recv_ = time(nullptr);
            FriendServiceHeartbeatResp resp;
            resp.set_timestamp(static_cast<uint64_t>(time(nullptr)));
            std::string s;
            resp.SerializeToString(&s);
            auto packed = MessageParser::pack(MSG_ID_FRIEND_SERVICE_HEARTBEAT_RESP, s);
            bufferevent_write(bev_, packed.data(), packed.size());
            break;
        }
        case MSG_ID_FRIEND_SERVICE_MSG: {
            FriendServiceMessage msg;
            if (msg.ParseFromArray(data, static_cast<int>(len)) && on_message_) {
                on_message_(msg.player_id(), msg.msg_id(), msg.payload());
            }
            break;
        }
    }
}

void FriendServiceConnection::send_identify() {
    FriendServiceIdentify id;
    id.set_service_id(0);
    std::string s;
    id.SerializeToString(&s);
    auto packed = MessageParser::pack(MSG_ID_FRIEND_SERVICE_IDENTIFY, s);
    bufferevent_write(bev_, packed.data(), packed.size());
}

void FriendServiceConnection::start_heartbeat() {
    if (heartbeat_timer_) return;
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    struct timeval tv = {5, 0};
    event_add(heartbeat_timer_, &tv);
    last_heartbeat_recv_ = time(nullptr);
}

void FriendServiceConnection::stop_heartbeat() {
    if (heartbeat_timer_) {
        event_del(heartbeat_timer_);
        event_free(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
}

void FriendServiceConnection::on_heartbeat_timer(evutil_socket_t, short, void* ctx) {
    auto* conn = static_cast<FriendServiceConnection*>(ctx);
    if (time(nullptr) - conn->last_heartbeat_recv_ > 15) {
        conn->disconnect();
        conn->schedule_reconnect();
        return;
    }
    FriendServiceHeartbeat hb;
    hb.set_timestamp(static_cast<uint64_t>(time(nullptr)));
    std::string s;
    hb.SerializeToString(&s);
    auto packed = MessageParser::pack(MSG_ID_FRIEND_SERVICE_HEARTBEAT, s);
    bufferevent_write(conn->bev_, packed.data(), packed.size());
}

void FriendServiceConnection::schedule_reconnect() {
    if (reconnect_timer_) return;
    reconnect_timer_ = event_new(base_, -1, 0, on_reconnect_timer, this);
    struct timeval tv = {5, 0};
    event_add(reconnect_timer_, &tv);
}

void FriendServiceConnection::on_reconnect_timer(evutil_socket_t, short, void* ctx) {
    auto* conn = static_cast<FriendServiceConnection*>(ctx);
    event_free(conn->reconnect_timer_);
    conn->reconnect_timer_ = nullptr;
    conn->connect(conn->host_, conn->port_);
}

}  // namespace farm
