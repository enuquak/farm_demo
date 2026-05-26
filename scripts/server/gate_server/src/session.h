#pragma once

#include <event2/util.h>
#include <string>
#include <ctime>
#include <cstdint>
#include <vector>
#include <cstring>

struct bufferevent;

namespace farm {

enum class SessionState {
    CONNECTED,
    LOGGED_IN,
    DISCONNECTED
};

class Session {
public:
    Session(evutil_socket_t fd, struct bufferevent* bev);
    ~Session();

    evutil_socket_t fd() const { return fd_; }
    struct bufferevent* bev() const { return bev_; }
    SessionState state() const { return state_; }
    uint64_t player_id() const { return player_id_; }
    uint32_t server_id() const { return server_id_; }
    const std::string& account_id() const { return account_id_; }
    time_t last_heartbeat() const { return last_heartbeat_; }
    time_t connect_time() const { return connect_time_; }

    void set_state(SessionState state) { state_ = state; }
    void set_player_id(uint64_t player_id) { player_id_ = player_id; }
    void set_server_id(uint32_t server_id) { server_id_ = server_id; }
    void set_account_id(const std::string& account_id) { account_id_ = account_id; }
    void update_heartbeat() { last_heartbeat_ = std::time(nullptr); }

    // 读缓冲区管理（处理拆包）
    std::vector<uint8_t>& read_buffer() { return read_buffer_; }
    void append_read_data(const uint8_t* data, size_t len);
    void consume_read_data(size_t len);

private:
    evutil_socket_t fd_;
    struct bufferevent* bev_;
    SessionState state_;
    uint64_t player_id_;
    uint32_t server_id_;
    std::string account_id_;
    time_t last_heartbeat_;
    time_t connect_time_;
    std::vector<uint8_t> read_buffer_;
};

}  // namespace farm
