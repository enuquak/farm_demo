#include "dbmgr_connection.h"
#include <event2/bufferevent.h>

namespace farm {

DBMgrConnection::DBMgrConnection(uint32_t config_index, const std::string& host, uint16_t port,
                                 struct bufferevent* bev)
    : config_index_(config_index)
    , remote_index_(0)
    , host_(host)
    , port_(port)
    , bev_(bev)
    , state_(DBMgrConnectionState::CONNECTED)
    , last_heartbeat_(std::time(nullptr))
    , connect_time_(std::time(nullptr))
{
}

DBMgrConnection::~DBMgrConnection() {
    if (bev_) {
        bufferevent_free(bev_);
        bev_ = nullptr;
    }
}

struct bufferevent* DBMgrConnection::release_bev() {
    struct bufferevent* bev = bev_;
    bev_ = nullptr;
    return bev;
}

void DBMgrConnection::append_read_data(const uint8_t* data, size_t len) {
    read_buffer_.insert(read_buffer_.end(), data, data + len);
}

void DBMgrConnection::consume_read_data(size_t len) {
    if (len >= read_buffer_.size()) {
        read_buffer_.clear();
    } else {
        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + len);
    }
}

}  // namespace farm
