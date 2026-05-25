#include "game_session.h"
#include <cstring>

namespace farm {

GameSession::GameSession(evutil_socket_t fd, struct bufferevent* bev)
    : fd_(fd)
    , bev_(bev)
    , state_(GameSessionState::CONNECTED)
    , last_heartbeat_(std::time(nullptr))
    , connect_time_(std::time(nullptr))
{
}

GameSession::~GameSession() {
}

void GameSession::append_read_data(const uint8_t* data, size_t len) {
    read_buffer_.insert(read_buffer_.end(), data, data + len);
}

void GameSession::consume_read_data(size_t len) {
    if (len >= read_buffer_.size()) {
        read_buffer_.clear();
    } else {
        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + len);
    }
}

}  // namespace farm
