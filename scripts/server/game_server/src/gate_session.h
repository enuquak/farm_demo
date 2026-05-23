#pragma once

#include <event2/util.h>
#include <string>
#include <ctime>
#include <cstdint>
#include <vector>

struct bufferevent;

namespace farm {

enum class GateSessionState {
    CONNECTED,    // 已连接，等待身份识别
    IDENTIFIED,   // 已识别，可处理业务消息
    DISCONNECTED  // 已断开
};

class GateSession {
public:
    GateSession(evutil_socket_t fd, struct bufferevent* bev);
    ~GateSession();

    evutil_socket_t fd() const { return fd_; }
    struct bufferevent* bev() const { return bev_; }
    GateSessionState state() const { return state_; }
    const std::string& gate_id() const { return gate_id_; }
    time_t last_heartbeat() const { return last_heartbeat_; }
    time_t connect_time() const { return connect_time_; }

    void set_state(GateSessionState state) { state_ = state; }
    void set_gate_id(const std::string& gate_id) { gate_id_ = gate_id; }
    void update_heartbeat() { last_heartbeat_ = std::time(nullptr); }

    // 读缓冲区管理（处理 TCP 拆包）
    std::vector<uint8_t>& read_buffer() { return read_buffer_; }
    void append_read_data(const uint8_t* data, size_t len);
    void consume_read_data(size_t len);

private:
    evutil_socket_t fd_;
    struct bufferevent* bev_;
    GateSessionState state_;
    std::string gate_id_;
    time_t last_heartbeat_;
    time_t connect_time_;
    std::vector<uint8_t> read_buffer_;
};

}  // namespace farm
