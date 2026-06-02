#pragma once

#include <event2/util.h>
#include <string>
#include <string_view>
#include <ctime>
#include <cstdint>
#include <vector>
#include <functional>
#include <queue>

struct bufferevent;

namespace farm {

enum class ConnectionState {
    CONNECTING,   // 正在连接
    CONNECTED,    // TCP 已连接，等待身份识别
    IDENTIFIED,   // 身份已确认，可处理业务
    DISCONNECTED  // 已断开
};

class GameConnection {
public:
    using MessageCallback = std::function<void(uint32_t msg_id, const std::vector<uint8_t>& payload)>;
    using DisconnectCallback = std::function<void()>;

    GameConnection(uint32_t server_id, const std::string& host, uint16_t port);
    ~GameConnection();

    uint32_t server_id() const { return server_id_; }
    const std::string& host() const { return host_; }
    uint16_t port() const { return port_; }
    struct bufferevent* bev() const { return bev_; }
    ConnectionState state() const { return state_; }
    time_t last_heartbeat() const { return last_heartbeat_; }

    void set_bev(struct bufferevent* bev) { bev_ = bev; }
    void set_state(ConnectionState state) { state_ = state; }
    void update_heartbeat() { last_heartbeat_ = std::time(nullptr); }

    void set_message_callback(MessageCallback callback) { msg_callback_ = std::move(callback); }
    void set_disconnect_callback(DisconnectCallback callback) { disconnect_callback_ = std::move(callback); }

    // 发送消息
    bool send(uint32_t msg_id, const uint8_t* payload, size_t len);
    bool send(uint32_t msg_id, std::string_view payload);

    // 读缓冲区管理
    std::vector<uint8_t>& read_buffer() { return read_buffer_; }
    void append_read_data(const uint8_t* data, size_t len);
    void consume_read_data(size_t len);

private:
    uint32_t server_id_;
    std::string host_;
    uint16_t port_;
    struct bufferevent* bev_ = nullptr;
    ConnectionState state_;
    time_t last_heartbeat_;
    std::vector<uint8_t> read_buffer_;
    MessageCallback msg_callback_;
    DisconnectCallback disconnect_callback_;
};

}  // namespace farm
