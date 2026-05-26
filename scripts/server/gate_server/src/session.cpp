#include "session.h"
#include <event2/bufferevent.h>

namespace farm {

Session::Session(evutil_socket_t fd, struct bufferevent* bev)
    : fd_(fd)
    , bev_(bev)
    , state_(SessionState::CONNECTED)
    , player_id_(0)
    , server_id_(0)
    , last_heartbeat_(std::time(nullptr))
    , connect_time_(std::time(nullptr))
{
}

Session::~Session() {
    if (bev_) {
        bufferevent_free(bev_);
        bev_ = nullptr;
    }
}

void Session::append_read_data(const uint8_t* data, size_t len) {
    read_buffer_.insert(read_buffer_.end(), data, data + len);
}

void Session::consume_read_data(size_t len) {
    if (len >= read_buffer_.size()) {
        read_buffer_.clear();
    } else {
        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + len);
    }
}

}  // namespace farm
