#include "game_connection.h"
#include "message_parser.h"

#include <event2/bufferevent.h>
#include <cstring>

namespace farm {

GameConnection::GameConnection(uint32_t server_id, const std::string& host, uint16_t port)
    : server_id_(server_id)
    , host_(host)
    , port_(port)
    , state_(ConnectionState::DISCONNECTED)
    , last_heartbeat_(std::time(nullptr))
{
}

GameConnection::~GameConnection() {
}

bool GameConnection::send(uint32_t msg_id, const uint8_t* payload, size_t len) {
    if (!bev_ || state_ == ConnectionState::DISCONNECTED) {
        return false;
    }

    auto data = MessageParser::pack(msg_id, payload, len);
    return bufferevent_write(bev_, data.data(), data.size()) == 0;
}

bool GameConnection::send(uint32_t msg_id, std::string_view payload) {
    return send(msg_id, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
}

void GameConnection::append_read_data(const uint8_t* data, size_t len) {
    read_buffer_.insert(read_buffer_.end(), data, data + len);
}

void GameConnection::consume_read_data(size_t len) {
    if (len >= read_buffer_.size()) {
        read_buffer_.clear();
    } else {
        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + len);
    }
}

}  // namespace farm
