#pragma once

#include <event2/util.h>
#include <string>
#include <ctime>
#include <cstdint>
#include <vector>

struct bufferevent;

namespace farm {

enum class DBMgrConnectionState {
    CONNECTED,     // TCP connected, waiting for DBMgrIdentify
    IDENTIFIED,    // Identity confirmed, ready for business
    DISCONNECTED   // Connection lost
};

/**
 * @brief Manages a single connection to a DBMgr instance.
 *
 * Lifecycle: CONNECTED -> IDENTIFIED (after DBMgrIdentify received)
 * On disconnect: any state -> DISCONNECTED (with auto-reconnect via manager)
 */
class DBMgrConnection {
public:
    DBMgrConnection(uint32_t config_index, const std::string& host, uint16_t port,
                    struct bufferevent* bev);
    ~DBMgrConnection();

    // Accessors
    uint32_t config_index() const { return config_index_; }
    uint32_t remote_index() const { return remote_index_; }
    const std::string& remote_address() const { return remote_address_; }
    const std::string& host() const { return host_; }
    uint16_t port() const { return port_; }

    struct bufferevent* bev() const { return bev_; }
    DBMgrConnectionState state() const { return state_; }
    time_t last_heartbeat() const { return last_heartbeat_; }
    time_t connect_time() const { return connect_time_; }

    // State management
    void set_state(DBMgrConnectionState state) { state_ = state; }
    void set_remote_index(uint32_t index) { remote_index_ = index; }
    void set_remote_address(const std::string& addr) { remote_address_ = addr; }
    void update_heartbeat() { last_heartbeat_ = std::time(nullptr); }

    // Release the bufferevent (caller takes ownership, no double-free)
    struct bufferevent* release_bev();

    // Read buffer management (TCP message framing)
    std::vector<uint8_t>& read_buffer() { return read_buffer_; }
    void append_read_data(const uint8_t* data, size_t len);
    void consume_read_data(size_t len);

private:
    uint32_t config_index_;    // Index from config (dbmgr_list position)
    uint32_t remote_index_;    // Index reported by DBMgrIdentify
    std::string remote_address_; // Address reported by DBMgrIdentify
    std::string host_;         // Target host from config
    uint16_t port_;            // Target port from config

    struct bufferevent* bev_;
    DBMgrConnectionState state_;
    time_t last_heartbeat_;
    time_t connect_time_;
    std::vector<uint8_t> read_buffer_;
};

}  // namespace farm
