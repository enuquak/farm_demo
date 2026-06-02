#include "cross_server_connection.h"
#include "message_parser.h"
#include "internal_msg_ids.h"
#include "message_ids.h"
#include "log_macros.h"

#include <cross.pb.h>
#include <event2/buffer.h>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace farm {

CrossServerConnection::CrossServerConnection(struct event_base* base, uint32_t server_id)
    : base_(base), server_id_(server_id) {}

CrossServerConnection::~CrossServerConnection() { disconnect(); }

bool CrossServerConnection::connect(const std::string& host, int port) {
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
    state_ = CrossServerState::CONNECTED;
    return true;
}

void CrossServerConnection::disconnect() {
    if (bev_) {
        bufferevent_free(bev_);
        bev_ = nullptr;
    }
    state_ = CrossServerState::DISCONNECTED;
    stop_heartbeat();
    stop_reconnect();
}

void CrossServerConnection::send_query_req(uint64_t request_id, uint64_t target_player_id,
                                            uint32_t query_type, const std::string& request_data) {
    if (!is_identified()) return;
    CrossQueryReq msg;
    msg.set_request_id(request_id);
    msg.set_target_player_id(target_player_id);
    msg.set_query_type(static_cast<CrossQueryType>(query_type));
    msg.set_request_data(request_data);
    std::string serialized;
    msg.SerializeToString(&serialized);
    auto packed = MessageParser::pack(MSG_ID_CROSS_QUERY_REQ, serialized);
    bufferevent_write(bev_, packed.data(), packed.size());
}

void CrossServerConnection::send_forward_resp(uint64_t request_id, int32_t code,
                                               const std::string& response_data) {
    if (!is_identified()) return;
    CrossForwardResp msg;
    msg.set_request_id(request_id);
    msg.set_code(code);
    msg.set_response_data(response_data);
    std::string serialized;
    msg.SerializeToString(&serialized);
    auto packed = MessageParser::pack(MSG_ID_CROSS_FORWARD_RESP, serialized);
    bufferevent_write(bev_, packed.data(), packed.size());
}

void CrossServerConnection::on_read(struct bufferevent* bev, void* ctx) {
    static_cast<CrossServerConnection*>(ctx)->handle_read();
}

void CrossServerConnection::on_event(struct bufferevent* bev, short events, void* ctx) {
    static_cast<CrossServerConnection*>(ctx)->handle_event(events);
}

void CrossServerConnection::handle_read() {
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

void CrossServerConnection::handle_event(short events) {
    if (events & BEV_EVENT_CONNECTED) {
        state_ = CrossServerState::CONNECTED;
        send_identify();
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        disconnect();
        schedule_reconnect();
    }
}

void CrossServerConnection::route_message(uint32_t msg_id, const uint8_t* data, size_t len) {
    switch (msg_id) {
        case MSG_ID_CROSS_IDENTIFY_RESP: {
            CrossIdentifyResp resp;
            if (resp.ParseFromArray(data, static_cast<int>(len)) && resp.code() == 0) {
                state_ = CrossServerState::IDENTIFIED;
                start_heartbeat();
                SPDLOG_INFO("[Game]CrossServer identified");
            }
            break;
        }
        case MSG_ID_CROSS_HEARTBEAT: {
            last_heartbeat_recv_ = time(nullptr);
            CrossHeartbeatResp resp;
            resp.set_timestamp(static_cast<uint64_t>(time(nullptr)));
            std::string s;
            resp.SerializeToString(&s);
            auto packed = MessageParser::pack(MSG_ID_CROSS_HEARTBEAT_RESP, s);
            bufferevent_write(bev_, packed.data(), packed.size());
            break;
        }
        case MSG_ID_CROSS_QUERY_RESP:
        case MSG_ID_CROSS_FORWARD_REQ: {
            if (on_message_) {
                on_message_(msg_id, data, len);
            }
            break;
        }
    }
}

void CrossServerConnection::send_identify() {
    CrossIdentify id;
    id.set_server_id(server_id_);
    std::string s;
    id.SerializeToString(&s);
    auto packed = MessageParser::pack(MSG_ID_CROSS_IDENTIFY, s);
    bufferevent_write(bev_, packed.data(), packed.size());
}

void CrossServerConnection::start_heartbeat() {
    if (heartbeat_timer_) return;
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    struct timeval tv = {5, 0};
    event_add(heartbeat_timer_, &tv);
    last_heartbeat_recv_ = time(nullptr);
}

void CrossServerConnection::stop_heartbeat() {
    if (heartbeat_timer_) {
        event_del(heartbeat_timer_);
        event_free(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
}

void CrossServerConnection::stop_reconnect() {
    if (reconnect_timer_) {
        event_del(reconnect_timer_);
        event_free(reconnect_timer_);
        reconnect_timer_ = nullptr;
    }
}

void CrossServerConnection::on_heartbeat_timer(evutil_socket_t, short, void* ctx) {
    auto* conn = static_cast<CrossServerConnection*>(ctx);
    if (time(nullptr) - conn->last_heartbeat_recv_ > 15) {
        conn->disconnect();
        conn->schedule_reconnect();
        return;
    }
    CrossHeartbeat hb;
    hb.set_timestamp(static_cast<uint64_t>(time(nullptr)));
    std::string s;
    hb.SerializeToString(&s);
    auto packed = MessageParser::pack(MSG_ID_CROSS_HEARTBEAT, s);
    bufferevent_write(conn->bev_, packed.data(), packed.size());
}

void CrossServerConnection::schedule_reconnect() {
    if (reconnect_timer_) return;
    reconnect_timer_ = event_new(base_, -1, 0, on_reconnect_timer, this);
    struct timeval tv = {5, 0};
    event_add(reconnect_timer_, &tv);
}

void CrossServerConnection::on_reconnect_timer(evutil_socket_t, short, void* ctx) {
    auto* conn = static_cast<CrossServerConnection*>(ctx);
    event_free(conn->reconnect_timer_);
    conn->reconnect_timer_ = nullptr;
    conn->connect(conn->host_, conn->port_);
}

}  // namespace farm
