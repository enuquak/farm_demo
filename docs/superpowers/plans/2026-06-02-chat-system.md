# Chat System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a channel-based chat system with world channel support, extensible to party/private channels.

**Architecture:** Client sends `ChatSendReq` via PlayerMsg to GateServer, which routes msg_id 6000-6999 to a new ChatServer process. ChatServer validates, rate-limits, and broadcasts `ChatMessage` to recipients via GateServer. Client UI is a left-bottom panel with channel filtering.

**Tech Stack:** Python (pygame) for client, C++ (libevent + protobuf-lite) for ChatServer, protobuf for protocol.

**Note on Message IDs:** The design spec proposes 4000-4999, but `internal_msg_ids.h` already uses 4000-4299 for Game<->DBMgr. This plan uses **6000-6999** for chat to avoid conflicts.

---

## File Structure

### New Files

| File | Responsibility |
|------|----------------|
| `scripts/common/proto/chat.proto` | Chat protobuf message definitions |
| `shared/message_ids.json` | Add chat message IDs (6001-6005) |
| `scripts/client/chat/__init__.py` | Chat package init |
| `scripts/client/chat/chat_channel.py` | Channel config + message storage |
| `scripts/client/chat/chat_manager.py` | Client-side chat logic (send/recv/rate-limit) |
| `scripts/client/chat/chat_panel.py` | Pygame UI panel (render + input) |
| `scripts/server/chat_server/CMakeLists.txt` | ChatServer build config |
| `scripts/server/chat_server/src/main.cpp` | ChatServer entry point |
| `scripts/server/chat_server/src/chat_server.h` | ChatServer class declaration |
| `scripts/server/chat_server/src/chat_server.cpp` | ChatServer implementation |
| `scripts/server/chat_server/src/channel_manager.h` | Channel manager declaration |
| `scripts/server/chat_server/src/channel_manager.cpp` | Channel manager implementation |
| `scripts/server/chat_server/src/rate_limiter.h` | Rate limiter declaration |
| `scripts/server/chat_server/src/rate_limiter.cpp` | Rate limiter implementation |
| `scripts/server/gate_server/src/chat_connection.h` | Gate's outbound connection to ChatServer |
| `scripts/server/gate_server/src/chat_connection.cpp` | ChatConnection implementation |

### Modified Files

| File | Change |
|------|--------|
| `scripts/common/proto/chat.proto` | New file (see above) |
| `scripts/common/proto/generated/` | Regenerate all proto files |
| `scripts/server/gate_server/src/gate_server.h` | Add ChatConnection member + routing methods |
| `scripts/server/gate_server/src/gate_server.cpp` | Add chat message routing in `route_message()` |
| `scripts/server/gate_server/CMakeLists.txt` | Add `chat_connection.cpp` + `chat.pb.cc` |
| `scripts/client/network_dispatcher.py` | Add `on_chat_message` callback + handler |
| `scripts/client/game_scene.py` | Wire up ChatManager + ChatPanel |
| `scripts/client/game_renderer.py` | Add ChatPanel rendering |

---

## Task 1: Protobuf Definitions and Message IDs

**Files:**
- Create: `scripts/common/proto/chat.proto`
- Modify: `shared/message_ids.json`
- Regenerate: `scripts/common/proto/generated/*`

- [x] **Step 1: Create chat.proto**

```protobuf
// scripts/common/proto/chat.proto
syntax = "proto3";

package farm;

option optimize_for = LITE_RUNTIME;

// ===========================================
// 聊天消息协议
// MsgID 范围: 6000-6999 (Client -> Gate -> ChatServer)
// ===========================================

// 聊天频道类型
enum ChatChannelType {
    CHANNEL_WORLD = 0;     // 世界频道
    CHANNEL_PARTY = 1;     // 队伍频道
    CHANNEL_WHISPER = 2;   // 私聊频道
}

// 聊天错误码
enum ChatErrorCode {
    CHAT_SUCCESS = 0;
    CHAT_RATE_LIMITED = 1;     // 发送太频繁
    CHAT_MSG_TOO_LONG = 2;     // 消息超长
    CHAT_TARGET_OFFLINE = 3;   // 私聊目标不在线
    CHAT_CHANNEL_NOT_FOUND = 4;// 频道不存在
    CHAT_PERMISSION_DENIED = 5;// 无权限
}

// 聊天消息（服务器推送给客户端）
message ChatMessage {
    ChatChannelType channel_type = 1;  // 频道类型
    uint64 sender_id = 2;              // 发送者 player_id
    string sender_name = 3;            // 发送者角色名
    string content = 4;                // 消息内容
    uint64 timestamp = 5;              // 时间戳 (ms)
    uint64 target_id = 6;              // 目标 player_id (私聊用)
}

// 发送聊天请求 (Client -> ChatServer via Gate)
message ChatSendReq {
    ChatChannelType channel_type = 1;  // 频道类型
    string content = 2;                // 消息内容
    uint64 target_id = 3;              // 私聊时指定目标
}

// 发送聊天响应 (ChatServer -> Client via Gate)
message ChatSendResp {
    ChatErrorCode code = 1;
    string msg = 2;
}
```

- [x] **Step 2: Add message IDs to shared/message_ids.json**

Add these entries to the `message_ids` array in `shared/message_ids.json`:

```json
{"code": 6001, "name": "MSG_ID_CHAT_SEND_REQ", "description": "聊天发送请求"},
{"code": 6002, "name": "MSG_ID_CHAT_SEND_RESP", "description": "聊天发送响应"},
{"code": 6003, "name": "MSG_ID_CHAT_MESSAGE", "description": "聊天消息推送"}
```

- [x] **Step 3: Run protobuf code generation**

```bash
cd D:/mb_workspace/farm_demo
python -m grpc_tools.protoc -Iscripts/common/proto --python_out=scripts/common/proto/generated --cpp_out=scripts/common/proto/generated scripts/common/proto/chat.proto
```

Verify `scripts/common/proto/generated/chat_pb2.py` and `chat.pb.cc`/`chat.pb.h` exist.

- [x] **Step 4: Run message ID code generation**

```bash
cd D:/mb_workspace/farm_demo
python tools/generate_constants.py
```

Verify `scripts/client/message_ids.py` and `scripts/server/common/include/message_ids.h` now contain `MSG_ID_CHAT_SEND_REQ`, `MSG_ID_CHAT_SEND_RESP`, `MSG_ID_CHAT_MESSAGE`.

- [x] **Step 5: Commit**

```bash
git add scripts/common/proto/chat.proto shared/message_ids.json scripts/common/proto/generated/ scripts/client/message_ids.py scripts/server/common/include/message_ids.h
git commit -m "feat(chat): add chat protobuf definitions and message IDs"
```

---

## Task 2: ChatServer - Rate Limiter

**Files:**
- Create: `scripts/server/chat_server/src/rate_limiter.h`
- Create: `scripts/server/chat_server/src/rate_limiter.cpp`

- [x] **Step 1: Create rate_limiter.h**

```cpp
// scripts/server/chat_server/src/rate_limiter.h
#pragma once

#include <cstdint>
#include <chrono>
#include <unordered_map>
#include <string>

namespace farm {

class RateLimiter {
public:
    RateLimiter() = default;
    ~RateLimiter() = default;

    // Check if player can send on this channel. Returns true if allowed.
    bool check_limit(uint64_t player_id, uint32_t channel_type, float cooldown_sec);

    // Record that player just sent on this channel.
    void update(uint64_t player_id, uint32_t channel_type);

    // Remove all entries for a player (on disconnect).
    void remove_player(uint64_t player_id);

private:
    static std::string make_key(uint64_t player_id, uint32_t channel_type);

    using Clock = std::chrono::steady_clock;
    std::unordered_map<std::string, Clock::time_point> last_send_;
};

}  // namespace farm
```

- [x] **Step 2: Create rate_limiter.cpp**

```cpp
// scripts/server/chat_server/src/rate_limiter.cpp
#include "rate_limiter.h"

namespace farm {

std::string RateLimiter::make_key(uint64_t player_id, uint32_t channel_type) {
    return std::to_string(player_id) + ":" + std::to_string(channel_type);
}

bool RateLimiter::check_limit(uint64_t player_id, uint32_t channel_type, float cooldown_sec) {
    auto it = last_send_.find(make_key(player_id, channel_type));
    if (it == last_send_.end()) {
        return true;  // No previous send
    }
    auto elapsed = std::chrono::duration<float>(Clock::now() - it->second).count();
    return elapsed >= cooldown_sec;
}

void RateLimiter::update(uint64_t player_id, uint32_t channel_type) {
    last_send_[make_key(player_id, channel_type)] = Clock::now();
}

void RateLimiter::remove_player(uint64_t player_id) {
    // Remove all channel entries for this player
    for (uint32_t ch = 0; ch <= 2; ++ch) {
        last_send_.erase(make_key(player_id, ch));
    }
}

}  // namespace farm
```

- [x] **Step 3: Commit**

```bash
git add scripts/server/chat_server/src/rate_limiter.h scripts/server/chat_server/src/rate_limiter.cpp
git commit -m "feat(chat): add rate limiter for chat server"
```

---

## Task 3: ChatServer - Channel Manager

**Files:**
- Create: `scripts/server/chat_server/src/channel_manager.h`
- Create: `scripts/server/chat_server/src/channel_manager.cpp`

- [x] **Step 1: Create channel_manager.h**

```cpp
// scripts/server/chat_server/src/channel_manager.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace farm {

// Callback to send a message to a specific player via GateServer
using SendToPlayerFunc = std::function<void(uint64_t player_id, uint32_t msg_id,
                                            const uint8_t* payload, size_t len)>;

// Channel configuration
struct ChannelConfig {
    uint32_t channel_type;      // 0=world, 1=party, 2=whisper
    int max_msg_length;         // Max message length
    float cooldown_sec;         // Send cooldown
};

class ChannelManager {
public:
    explicit ChannelManager(SendToPlayerFunc send_func);
    ~ChannelManager() = default;

    // Player lifecycle
    void add_player(uint64_t player_id, const std::string& player_name);
    void remove_player(uint64_t player_id);

    // Process a chat message: validate, then broadcast
    // Returns error code (0 = success)
    int process_message(uint64_t sender_id, uint32_t channel_type,
                        const std::string& content, uint64_t target_id);

    // Get player name (returns empty string if not found)
    std::string get_player_name(uint64_t player_id) const;

    // Get channel config
    static ChannelConfig get_channel_config(uint32_t channel_type);

private:
    void broadcast_to_world(uint64_t sender_id, const std::string& sender_name,
                            const std::string& content, uint64_t timestamp);
    void send_whisper(uint64_t sender_id, const std::string& sender_name,
                      const std::string& content, uint64_t target_id, uint64_t timestamp);

    SendToPlayerFunc send_func_;

    // Online players: player_id -> player_name
    std::unordered_map<uint64_t, std::string> players_;

    // Online player IDs (for fast iteration during broadcast)
    std::unordered_set<uint64_t> online_player_ids_;
};

}  // namespace farm
```

- [x] **Step 2: Create channel_manager.cpp**

```cpp
// scripts/server/chat_server/src/channel_manager.cpp
#include "channel_manager.h"
#include "chat.pb.h"
#include "message_ids.h"
#include "log_macros.h"

#include <chrono>

namespace farm {

ChannelManager::ChannelManager(SendToPlayerFunc send_func)
    : send_func_(std::move(send_func)) {
}

void ChannelManager::add_player(uint64_t player_id, const std::string& player_name) {
    players_[player_id] = player_name;
    online_player_ids_.insert(player_id);
    SPDLOG_INFO("[Chat]Player online: id={}, name={}", player_id, player_name);
}

void ChannelManager::remove_player(uint64_t player_id) {
    players_.erase(player_id);
    online_player_ids_.erase(player_id);
    SPDLOG_INFO("[Chat]Player offline: id={}", player_id);
}

std::string ChannelManager::get_player_name(uint64_t player_id) const {
    auto it = players_.find(player_id);
    return it != players_.end() ? it->second : "";
}

ChannelConfig ChannelManager::get_channel_config(uint32_t channel_type) {
    switch (channel_type) {
        case 0:  // WORLD
            return {0, 100, 5.0f};
        case 1:  // PARTY
            return {1, 200, 3.0f};
        case 2:  // WHISPER
            return {2, 500, 1.0f};
        default:
            return {channel_type, 100, 5.0f};
    }
}

int ChannelManager::process_message(uint64_t sender_id, uint32_t channel_type,
                                     const std::string& content, uint64_t target_id) {
    // Validate sender is online
    auto sender_it = players_.find(sender_id);
    if (sender_it == players_.end()) {
        return 4;  // CHANNEL_NOT_FOUND (sender not registered)
    }

    // Validate channel type
    if (channel_type > 2) {
        return 4;  // CHANNEL_NOT_FOUND
    }

    // Validate message length
    ChannelConfig config = get_channel_config(channel_type);
    if (static_cast<int>(content.size()) > config.max_msg_length) {
        return 2;  // MSG_TOO_LONG
    }

    // Validate content not empty
    if (content.empty()) {
        return 2;  // MSG_TOO_LONG (treat empty as invalid)
    }

    std::string sender_name = sender_it->second;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    switch (channel_type) {
        case 0:  // WORLD
            broadcast_to_world(sender_id, sender_name, content, timestamp);
            break;
        case 1:  // PARTY - placeholder for future
            SPDLOG_WARN("[Chat]Party channel not implemented yet");
            break;
        case 2:  // WHISPER
            if (target_id == 0) return 3;  // TARGET_OFFLINE
            send_whisper(sender_id, sender_name, content, target_id, timestamp);
            break;
    }

    return 0;  // SUCCESS
}

void ChannelManager::broadcast_to_world(uint64_t sender_id, const std::string& sender_name,
                                         const std::string& content, uint64_t timestamp) {
    // Build ChatMessage proto
    ChatMessage msg;
    msg.set_channel_type(CHANNEL_WORLD);
    msg.set_sender_id(sender_id);
    msg.set_sender_name(sender_name);
    msg.set_content(content);
    msg.set_timestamp(timestamp);

    std::string payload;
    msg.SerializeToString(&payload);

    // Broadcast to all online players
    for (uint64_t pid : online_player_ids_) {
        send_func_(pid, MSG_ID_CHAT_MESSAGE,
                   reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    }

    SPDLOG_DEBUG("[Chat]World broadcast from {}: {}", sender_name, content);
}

void ChannelManager::send_whisper(uint64_t sender_id, const std::string& sender_name,
                                   const std::string& content, uint64_t target_id,
                                   uint64_t timestamp) {
    // Check target is online
    auto target_it = players_.find(target_id);
    if (target_it == players_.end()) {
        // Send error back to sender
        ChatSendResp resp;
        resp.set_code(CHAT_TARGET_OFFLINE);
        resp.set_msg("Player not online");
        std::string resp_payload;
        resp.SerializeToString(&resp_payload);
        send_func_(sender_id, MSG_ID_CHAT_SEND_RESP,
                   reinterpret_cast<const uint8_t*>(resp_payload.data()), resp_payload.size());
        return;
    }

    // Build ChatMessage
    ChatMessage msg;
    msg.set_channel_type(CHANNEL_WHISPER);
    msg.set_sender_id(sender_id);
    msg.set_sender_name(sender_name);
    msg.set_content(content);
    msg.set_timestamp(timestamp);
    msg.set_target_id(target_id);

    std::string payload;
    msg.SerializeToString(&payload);

    // Send to target
    send_func_(target_id, MSG_ID_CHAT_MESSAGE,
               reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    // Send to sender (so they see their own whisper)
    send_func_(sender_id, MSG_ID_CHAT_MESSAGE,
               reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
}

}  // namespace farm
```

- [x] **Step 3: Commit**

```bash
git add scripts/server/chat_server/src/channel_manager.h scripts/server/chat_server/src/channel_manager.cpp
git commit -m "feat(chat): add channel manager for chat server"
```

---

## Task 4: ChatServer - Main Server

**Files:**
- Create: `scripts/server/chat_server/src/chat_server.h`
- Create: `scripts/server/chat_server/src/chat_server.cpp`
- Create: `scripts/server/chat_server/src/main.cpp`
- Create: `scripts/server/chat_server/CMakeLists.txt`

- [x] **Step 1: Create chat_server.h**

```cpp
// scripts/server/chat_server/src/chat_server.h
#pragma once

#include "channel_manager.h"
#include "rate_limiter.h"

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

namespace farm {

// Gate session (ChatServer acts as downstream of GateServer)
struct ChatGateSession {
    evutil_socket_t fd;
    struct bufferevent* bev;
    std::string gate_id;
    bool identified;
    std::vector<uint8_t> read_buffer;
};

class ChatServer {
public:
    ChatServer(const std::string& ip, uint16_t port);
    ~ChatServer();

    bool start();
    void stop();

private:
    // libevent callbacks
    static void on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                          struct sockaddr* addr, int len, void* ctx);
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);

    // Message handling
    void handle_accept(evutil_socket_t fd, struct sockaddr* addr);
    void handle_read(std::shared_ptr<ChatGateSession> session);
    void handle_disconnect(std::shared_ptr<ChatGateSession> session);
    void route_message(std::shared_ptr<ChatGateSession> session,
                       uint32_t msg_id, const std::vector<uint8_t>& payload);

    // Internal protocol handlers
    void handle_gate_identify(std::shared_ptr<ChatGateSession> session,
                              const std::vector<uint8_t>& payload);
    void handle_heartbeat(std::shared_ptr<ChatGateSession> session,
                          const std::vector<uint8_t>& payload);
    void handle_player_join(std::shared_ptr<ChatGateSession> session,
                            const std::vector<uint8_t>& payload);
    void handle_player_leave(std::shared_ptr<ChatGateSession> session,
                             const std::vector<uint8_t>& payload);
    void handle_client_msg(std::shared_ptr<ChatGateSession> session,
                           const std::vector<uint8_t>& payload);

    // Chat message handler
    void handle_chat_send_req(uint64_t player_id, const uint8_t* payload, size_t len);

    // Send to gate
    void send_to_gate(std::shared_ptr<ChatGateSession> session,
                      uint32_t msg_id, std::string_view payload);

    // Send to player (via gate) - used by ChannelManager
    void send_to_player(uint64_t player_id, uint32_t msg_id,
                        const uint8_t* payload, size_t len);

    std::string ip_;
    uint16_t port_;
    struct event_base* base_;
    struct evconnlistener* listener_;
    bool running_;

    // Gate sessions
    std::unordered_map<evutil_socket_t, std::shared_ptr<ChatGateSession>> gate_sessions_;

    // Player -> Gate session mapping (for sending messages to players)
    std::unordered_map<uint64_t, std::shared_ptr<ChatGateSession>> player_to_gate_;

    // Subsystems
    RateLimiter rate_limiter_;
    std::unique_ptr<ChannelManager> channel_mgr_;
};

}  // namespace farm
```

- [x] **Step 2: Create chat_server.cpp**

```cpp
// scripts/server/chat_server/src/chat_server.cpp
#include "chat_server.h"
#include "internal_msg_ids.h"
#include "message_ids.h"
#include "message_parser.h"
#include "chat.pb.h"
#include "internal.pb.h"
#include "log_macros.h"

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include <cstring>

namespace farm {

ChatServer::ChatServer(const std::string& ip, uint16_t port)
    : ip_(ip), port_(port), base_(nullptr), listener_(nullptr), running_(false) {
}

ChatServer::~ChatServer() {
    stop();
}

bool ChatServer::start() {
    base_ = event_base_new();
    if (!base_) {
        SPDLOG_ERROR("[Chat]Failed to create event_base");
        return false;
    }

    // Initialize ChannelManager with send callback
    channel_mgr_ = std::make_unique<ChannelManager>(
        [this](uint64_t player_id, uint32_t msg_id, const uint8_t* payload, size_t len) {
            send_to_player(player_id, msg_id, payload, len);
        });

    // Bind address
    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port_);
    if (ip_.empty() || ip_ == "0.0.0.0") {
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        inet_pton(AF_INET, ip_.c_str(), &sin.sin_addr);
    }

    listener_ = evconnlistener_new_bind(
        base_, on_accept, this,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
        128, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin));

    if (!listener_) {
        SPDLOG_ERROR("[Chat]Failed to create listener on {}:{}", ip_, port_);
        event_base_free(base_);
        base_ = nullptr;
        return false;
    }

    running_ = true;
    SPDLOG_INFO("[Chat]ChatServer listening on {}:{}", ip_, port_);

    event_base_dispatch(base_);
    return true;
}

void ChatServer::stop() {
    running_ = false;
    if (base_) {
        event_base_loopexit(base_, nullptr);
    }
    if (listener_) {
        evconnlistener_free(listener_);
        listener_ = nullptr;
    }
    if (base_) {
        event_base_free(base_);
        base_ = nullptr;
    }
}

void ChatServer::on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                            struct sockaddr* addr, int len, void* ctx) {
    auto* server = static_cast<ChatServer*>(ctx);
    server->handle_accept(fd, addr);
}

void ChatServer::handle_accept(evutil_socket_t fd, struct sockaddr* addr) {
    auto* bev = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        SPDLOG_ERROR("[Chat]Failed to create bufferevent for fd={}", fd);
        return;
    }

    auto session = std::make_shared<ChatGateSession>();
    session->fd = fd;
    session->bev = bev;
    session->identified = false;

    bufferevent_setcb(bev, on_read, nullptr, on_event, this);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    gate_sessions_[fd] = session;
    SPDLOG_INFO("[Chat]Gate connected: fd={}", fd);
}

void ChatServer::on_read(struct bufferevent* bev, void* ctx) {
    auto* server = static_cast<ChatServer*>(ctx);
    // Find session by bev
    for (auto& [fd, session] : server->gate_sessions_) {
        if (session->bev == bev) {
            server->handle_read(session);
            return;
        }
    }
}

void ChatServer::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* server = static_cast<ChatServer*>(ctx);
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        // Find and remove session
        for (auto it = server->gate_sessions_.begin(); it != server->gate_sessions_.end(); ++it) {
            if (it->second->bev == bev) {
                server->handle_disconnect(it->second);
                break;
            }
        }
    }
}

void ChatServer::handle_read(std::shared_ptr<ChatGateSession> session) {
    struct evbuffer* input = bufferevent_get_input(session->bev);
    size_t len = evbuffer_get_length(input);
    if (len == 0) return;

    session->read_buffer.resize(session->read_buffer.size() + len);
    evbuffer_remove(input, session->read_buffer.data() + session->read_buffer.size() - len, len);

    // Parse messages from buffer
    while (session->read_buffer.size() >= 8) {
        uint32_t msg_len = ntohl(*reinterpret_cast<uint32_t*>(session->read_buffer.data()));
        if (session->read_buffer.size() < 4 + msg_len) break;

        uint32_t msg_id = ntohl(*reinterpret_cast<uint32_t*>(session->read_buffer.data() + 4));
        std::vector<uint8_t> payload(session->read_buffer.data() + 8,
                                     session->read_buffer.data() + 4 + msg_len);

        session->read_buffer.erase(session->read_buffer.begin(),
                                   session->read_buffer.begin() + 4 + msg_len);

        route_message(session, msg_id, payload);
    }
}

void ChatServer::handle_disconnect(std::shared_ptr<ChatGateSession> session) {
    SPDLOG_INFO("[Chat]Gate disconnected: fd={}", session->fd);
    // Remove all players associated with this gate
    // (In a real implementation, track which players are on which gate)
    gate_sessions_.erase(session->fd);
    if (session->bev) {
        bufferevent_free(session->bev);
    }
}

void ChatServer::route_message(std::shared_ptr<ChatGateSession> session,
                                uint32_t msg_id, const std::vector<uint8_t>& payload) {
    if (msg_id == MSG_ID_INTERN_HEARTBEAT) {
        handle_heartbeat(session, payload);
    } else if (msg_id == MSG_ID_GATE_IDENTIFY) {
        handle_gate_identify(session, payload);
    } else if (msg_id == MSG_ID_PLAYER_JOIN) {
        handle_player_join(session, payload);
    } else if (msg_id == MSG_ID_PLAYER_LEAVE) {
        handle_player_leave(session, payload);
    } else if (msg_id == MSG_ID_CLIENT_MSG) {
        handle_client_msg(session, payload);
    } else {
        SPDLOG_DEBUG("[Chat]Unhandled msg_id={}", msg_id);
    }
}

void ChatServer::handle_gate_identify(std::shared_ptr<ChatGateSession> session,
                                       const std::vector<uint8_t>& payload) {
    GateIdentify identify;
    if (!identify.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Chat]Failed to parse GateIdentify");
        return;
    }
    session->gate_id = identify.gate_id();
    session->identified = true;

    // Send response
    GateIdentifyResp resp;
    resp.set_code(0);
    resp.set_msg("OK");
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_to_gate(session, MSG_ID_GATE_IDENTIFY_RESP, resp_data);

    SPDLOG_INFO("[Chat]Gate identified: {}", session->gate_id);
}

void ChatServer::handle_heartbeat(std::shared_ptr<ChatGateSession> session,
                                   const std::vector<uint8_t>& payload) {
    InternHeartbeatResp resp;
    resp.set_timestamp(std::time(nullptr));
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_to_gate(session, MSG_ID_INTERN_HEARTBEAT_RESP, resp_data);
}

void ChatServer::handle_player_join(std::shared_ptr<ChatGateSession> session,
                                     const std::vector<uint8_t>& payload) {
    PlayerJoin join;
    if (!join.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Chat]Failed to parse PlayerJoin");
        return;
    }
    // For now, use player_id as name (real implementation would query GameServer)
    channel_mgr_->add_player(join.player_id(), "Player" + std::to_string(join.player_id()));
    player_to_gate_[join.player_id()] = session;

    // Send response
    PlayerJoinResp resp;
    resp.set_player_id(join.player_id());
    resp.set_code(0);
    resp.set_msg("OK");
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_to_gate(session, MSG_ID_PLAYER_JOIN_RESP, resp_data);
}

void ChatServer::handle_player_leave(std::shared_ptr<ChatGateSession> session,
                                      const std::vector<uint8_t>& payload) {
    PlayerLeave leave;
    if (!leave.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Chat]Failed to parse PlayerLeave");
        return;
    }
    channel_mgr_->remove_player(leave.player_id());
    rate_limiter_.remove_player(leave.player_id());
    player_to_gate_.erase(leave.player_id());
}

void ChatServer::handle_client_msg(std::shared_ptr<ChatGateSession> session,
                                    const std::vector<uint8_t>& payload) {
    ClientMessage client_msg;
    if (!client_msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Chat]Failed to parse ClientMessage");
        return;
    }

    uint64_t player_id = client_msg.player_id();
    uint32_t msg_id = client_msg.msg_id();
    const auto& inner_payload = client_msg.payload();

    if (msg_id == MSG_ID_CHAT_SEND_REQ) {
        handle_chat_send_req(player_id,
                             reinterpret_cast<const uint8_t*>(inner_payload.data()),
                             inner_payload.size());
    } else {
        SPDLOG_DEBUG("[Chat]Unhandled client msg_id={}", msg_id);
    }
}

void ChatServer::handle_chat_send_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    ChatSendReq req;
    if (!req.ParseFromArray(payload, static_cast<int>(len))) {
        SPDLOG_ERROR("[Chat]Failed to parse ChatSendReq from player={}", player_id);
        return;
    }

    uint32_t channel_type = static_cast<uint32_t>(req.channel_type());

    // Rate limit check
    ChannelConfig config = ChannelManager::get_channel_config(channel_type);
    if (!rate_limiter_.check_limit(player_id, channel_type, config.cooldown_sec)) {
        ChatSendResp resp;
        resp.set_code(CHAT_RATE_LIMITED);
        resp.set_msg("Sending too fast");
        std::string resp_payload;
        resp.SerializeToString(&resp_payload);
        send_to_player(player_id, MSG_ID_CHAT_SEND_RESP,
                       reinterpret_cast<const uint8_t*>(resp_payload.data()),
                       resp_payload.size());
        return;
    }

    // Process message
    int result = channel_mgr_->process_message(player_id, channel_type,
                                                req.content(), req.target_id());

    // Update rate limiter on success
    if (result == 0) {
        rate_limiter_.update(player_id, channel_type);
    }

    // Send response
    ChatSendResp resp;
    resp.set_code(static_cast<ChatErrorCode>(result));
    resp.set_msg(result == 0 ? "OK" : "Error");
    std::string resp_payload;
    resp.SerializeToString(&resp_payload);
    send_to_player(player_id, MSG_ID_CHAT_SEND_RESP,
                   reinterpret_cast<const uint8_t*>(resp_payload.data()),
                   resp_payload.size());
}

void ChatServer::send_to_gate(std::shared_ptr<ChatGateSession> session,
                               uint32_t msg_id, std::string_view payload) {
    auto packed = MessageParser::pack(msg_id, payload);
    bufferevent_write(session->bev, packed.data(), packed.size());
}

void ChatServer::send_to_player(uint64_t player_id, uint32_t msg_id,
                                 const uint8_t* payload, size_t len) {
    auto it = player_to_gate_.find(player_id);
    if (it == player_to_gate_.end()) {
        SPDLOG_DEBUG("[Chat]Cannot send to player {}: no gate session", player_id);
        return;
    }
    // Wrap as GameMessage (reuse internal protocol)
    GameMessage game_msg;
    game_msg.set_player_id(player_id);
    game_msg.set_msg_id(msg_id);
    game_msg.set_payload(payload, len);

    std::string wrapped;
    game_msg.SerializeToString(&wrapped);
    send_to_gate(it->second, MSG_ID_GAME_MSG, wrapped);
}

}  // namespace farm
```

- [x] **Step 3: Create main.cpp**

```cpp
// scripts/server/chat_server/src/main.cpp
#include "chat_server.h"
#include "server_main_helper.h"
#include "log_init.h"
#include "log_macros.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // Initialize logging
    farm::LogInit::init("chat_server");

    // Parse command line
    std::string ip = "0.0.0.0";
    uint16_t port = 8889;  // Default chat server port

    if (argc >= 2) {
        ip = argv[1];
    }
    if (argc >= 3) {
        port = static_cast<uint16_t>(std::stoi(argv[2]));
    }

    SPDLOG_INFO("[Chat]Starting ChatServer on {}:{}", ip, port);

    farm::ChatServer server(ip, port);
    if (!server.start()) {
        SPDLOG_ERROR("[Chat]Failed to start ChatServer");
        return 1;
    }

    return 0;
}
```

- [x] **Step 4: Create CMakeLists.txt**

```cmake
# scripts/server/chat_server/CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(chat_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 静态链接运行时
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 依赖路径
set(LIBEVENT_ROOT "C:/libevent_install")
set(PROTOBUF_ROOT "C:/protobuf_install")
set(COMMON_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/proto")

# Protobuf 生成的源文件
set(PROTO_GENERATED_DIR "${COMMON_PROTO_DIR}/generated")
set(PROTO_SRCS
    ${PROTO_GENERATED_DIR}/base.pb.cc
    ${PROTO_GENERATED_DIR}/internal.pb.cc
    ${PROTO_GENERATED_DIR}/chat.pb.cc
)
set(PROTO_HDRS
    ${PROTO_GENERATED_DIR}/base.pb.h
    ${PROTO_GENERATED_DIR}/internal.pb.h
    ${PROTO_GENERATED_DIR}/chat.pb.h
)

# 源文件
set(SOURCES
    src/main.cpp
    src/chat_server.cpp
    src/channel_manager.cpp
    src/rate_limiter.cpp
    ${PROTO_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/log_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/message_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/msvc_compat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/server_main_helper.cpp
)

add_executable(chat_server ${SOURCES})

# Include 目录
target_include_directories(chat_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROTOBUF_ROOT}/include
    ${LIBEVENT_ROOT}/include
    ${COMMON_PROTO_DIR}
    ${COMMON_PROTO_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# 链接库目录
target_link_directories(chat_server PRIVATE
    ${PROTOBUF_ROOT}/lib
    ${LIBEVENT_ROOT}/lib
)

# MSVC 特定设置
if(MSVC)
    target_compile_definitions(chat_server PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NOMINMAX
    )
    target_compile_options(chat_server PRIVATE /utf-8 /MT$<$<CONFIG:Debug>:d>)

    file(GLOB ABSL_LIBS "${PROTOBUF_ROOT}/lib/absl_*.lib")
    target_link_libraries(chat_server PRIVATE
        libprotobuf-lite.lib
        libutf8_range.lib
        libutf8_validity.lib
        ${ABSL_LIBS}
        event.lib
        event_core.lib
        event_extra.lib
        ws2_32.lib
    )
else()
    target_link_libraries(chat_server PRIVATE
        protobuf
        event
        event_core
        event_extra
        pthread
    )
endif()

# 输出目录
set_target_properties(chat_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
)
```

- [x] **Step 5: Build ChatServer**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/chat_server
mkdir -p build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Verify `bin/chat_server.exe` exists.

- [x] **Step 6: Commit**

```bash
git add scripts/server/chat_server/
git commit -m "feat(chat): add ChatServer with rate limiter and channel manager"
```

---

## Task 5: GateServer - Chat Routing

**Files:**
- Create: `scripts/server/gate_server/src/chat_connection.h`
- Create: `scripts/server/gate_server/src/chat_connection.cpp`
- Modify: `scripts/server/gate_server/src/gate_server.h`
- Modify: `scripts/server/gate_server/src/gate_server.cpp`
- Modify: `scripts/server/gate_server/CMakeLists.txt`

- [x] **Step 1: Create chat_connection.h**

```cpp
// scripts/server/gate_server/src/chat_connection.h
#pragma once

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <string>
#include <string_view>
#include <cstdint>
#include <ctime>
#include <vector>
#include <functional>

namespace farm {

enum class ChatConnState {
    DISCONNECTED,
    CONNECTING,
    IDENTIFIED
};

using ChatMessageCallback = std::function<void(uint32_t msg_id, const std::vector<uint8_t>& payload)>;

class ChatConnection {
public:
    ChatConnection(struct event_base* base, const std::string& gate_id);
    ~ChatConnection();

    bool connect(const std::string& ip, uint16_t port);
    void disconnect();

    bool send(uint32_t msg_id, std::string_view payload);
    bool send(uint32_t msg_id, const uint8_t* payload, size_t len);

    ChatConnState state() const { return state_; }
    void set_state(ChatConnState state) { state_ = state; }
    bool is_identified() const { return state_ == ChatConnState::IDENTIFIED; }

    void set_message_callback(ChatMessageCallback callback) { msg_callback_ = std::move(callback); }

    void start_heartbeat();
    void stop_heartbeat();
    void start_reconnect();
    void stop_reconnect();

private:
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);
    static void on_reconnect_timer(evutil_socket_t fd, short events, void* ctx);

    void handle_read();
    void handle_connect_success();
    void handle_disconnect();
    void send_identify();
    void send_heartbeat();
    void try_reconnect();

    struct event_base* base_;
    std::string gate_id_;
    std::string chat_ip_;
    uint16_t chat_port_;
    ChatConnState state_;

    struct bufferevent* bev_;
    struct event* heartbeat_timer_;
    struct event* reconnect_timer_;
    time_t last_heartbeat_;
    std::vector<uint8_t> read_buffer_;

    ChatMessageCallback msg_callback_;
};

}  // namespace farm
```

- [x] **Step 2: Create chat_connection.cpp**

Follow the same pattern as `game_connection.cpp`. The implementation is nearly identical - just change class names from `GameConnection`/`GameConnState` to `ChatConnection`/`ChatConnState`, and use `MSG_ID_GATE_IDENTIFY` / `MSG_ID_INTERN_HEARTBEAT` for the handshake.

Key points:
- Same wire format: `[4 bytes length][4 bytes MsgID][Payload]`
- Same identify/heartbeat protocol as GameConnection
- Same reconnect logic

```cpp
// scripts/server/gate_server/src/chat_connection.cpp
#include "chat_connection.h"
#include "internal_msg_ids.h"
#include "message_parser.h"
#include "internal.pb.h"
#include "log_macros.h"

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include <cstring>

namespace farm {

ChatConnection::ChatConnection(struct event_base* base, const std::string& gate_id)
    : base_(base), gate_id_(gate_id), chat_port_(0), state_(ChatConnState::DISCONNECTED),
      bev_(nullptr), heartbeat_timer_(nullptr), reconnect_timer_(nullptr),
      last_heartbeat_(0), msg_callback_(nullptr) {
}

ChatConnection::~ChatConnection() {
    disconnect();
}

bool ChatConnection::connect(const std::string& ip, uint16_t port) {
    chat_ip_ = ip;
    chat_port_ = port;
    state_ = ChatConnState::CONNECTING;

    bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
    if (!bev_) {
        SPDLOG_ERROR("[Gate->Chat]Failed to create bufferevent");
        return false;
    }

    bufferevent_setcb(bev_, on_read, nullptr, on_event, this);
    bufferevent_enable(bev_, EV_READ | EV_WRITE);

    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &sin.sin_addr);

    if (bufferevent_socket_connect(bev_, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin)) < 0) {
        SPDLOG_ERROR("[Gate->Chat]Failed to connect to {}:{}", ip, port);
        bufferevent_free(bev_);
        bev_ = nullptr;
        state_ = ChatConnState::DISCONNECTED;
        return false;
    }

    SPDLOG_INFO("[Gate->Chat]Connecting to ChatServer at {}:{}", ip, port);
    return true;
}

void ChatConnection::disconnect() {
    stop_heartbeat();
    stop_reconnect();
    if (bev_) {
        bufferevent_free(bev_);
        bev_ = nullptr;
    }
    state_ = ChatConnState::DISCONNECTED;
}

bool ChatConnection::send(uint32_t msg_id, std::string_view payload) {
    return send(msg_id, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
}

bool ChatConnection::send(uint32_t msg_id, const uint8_t* payload, size_t len) {
    if (!bev_ || state_ != ChatConnState::IDENTIFIED) {
        return false;
    }
    auto packed = MessageParser::pack(msg_id, {reinterpret_cast<const char*>(payload), len});
    bufferevent_write(bev_, packed.data(), packed.size());
    return true;
}

void ChatConnection::on_read(struct bufferevent* bev, void* ctx) {
    auto* conn = static_cast<ChatConnection*>(ctx);
    conn->handle_read();
}

void ChatConnection::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* conn = static_cast<ChatConnection*>(ctx);
    if (events & BEV_EVENT_CONNECTED) {
        conn->handle_connect_success();
    } else if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        conn->handle_disconnect();
    }
}

void ChatConnection::handle_connect_success() {
    SPDLOG_INFO("[Gate->Chat]Connected to ChatServer");
    send_identify();
}

void ChatConnection::handle_disconnect() {
    SPDLOG_WARN("[Gate->Chat]Disconnected from ChatServer");
    state_ = ChatConnState::DISCONNECTED;
    if (bev_) {
        bufferevent_free(bev_);
        bev_ = nullptr;
    }
    start_reconnect();
}

void ChatConnection::handle_read() {
    struct evbuffer* input = bufferevent_get_input(bev_);
    size_t len = evbuffer_get_length(input);
    if (len == 0) return;

    read_buffer_.resize(read_buffer_.size() + len);
    evbuffer_remove(input, read_buffer_.data() + read_buffer_.size() - len, len);

    while (read_buffer_.size() >= 8) {
        uint32_t msg_len = ntohl(*reinterpret_cast<uint32_t*>(read_buffer_.data()));
        if (read_buffer_.size() < 4 + msg_len) break;

        uint32_t msg_id = ntohl(*reinterpret_cast<uint32_t*>(read_buffer_.data() + 4));
        std::vector<uint8_t> payload(read_buffer_.data() + 8,
                                     read_buffer_.data() + 4 + msg_len);

        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + 4 + msg_len);

        // Handle identify response
        if (msg_id == MSG_ID_GATE_IDENTIFY_RESP) {
            state_ = ChatConnState::IDENTIFIED;
            SPDLOG_INFO("[Gate->Chat]Identified with ChatServer");
            start_heartbeat();
            continue;
        }

        // Handle heartbeat response
        if (msg_id == MSG_ID_INTERN_HEARTBEAT_RESP) {
            last_heartbeat_ = std::time(nullptr);
            continue;
        }

        // Forward to callback
        if (msg_callback_) {
            msg_callback_(msg_id, payload);
        }
    }
}

void ChatConnection::send_identify() {
    GateIdentify identify;
    identify.set_gate_id(gate_id_);
    std::string payload;
    identify.SerializeToString(&payload);
    auto packed = MessageParser::pack(MSG_ID_GATE_IDENTIFY, payload);
    bufferevent_write(bev_, packed.data(), packed.size());
}

void ChatConnection::send_heartbeat() {
    InternHeartbeat hb;
    hb.set_timestamp(std::time(nullptr));
    std::string payload;
    hb.SerializeToString(&payload);
    auto packed = MessageParser::pack(MSG_ID_INTERN_HEARTBEAT, payload);
    bufferevent_write(bev_, packed.data(), packed.size());
}

void ChatConnection::start_heartbeat() {
    if (heartbeat_timer_) return;
    struct timeval tv = {5, 0};  // Every 5 seconds
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    evtimer_add(heartbeat_timer_, &tv);
}

void ChatConnection::stop_heartbeat() {
    if (heartbeat_timer_) {
        event_free(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
}

void ChatConnection::start_reconnect() {
    if (reconnect_timer_) return;
    struct timeval tv = {3, 0};  // Try every 3 seconds
    reconnect_timer_ = event_new(base_, -1, EV_PERSIST, on_reconnect_timer, this);
    evtimer_add(reconnect_timer_, &tv);
}

void ChatConnection::stop_reconnect() {
    if (reconnect_timer_) {
        event_free(reconnect_timer_);
        reconnect_timer_ = nullptr;
    }
}

void ChatConnection::on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* conn = static_cast<ChatConnection*>(ctx);
    conn->send_heartbeat();
}

void ChatConnection::on_reconnect_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* conn = static_cast<ChatConnection*>(ctx);
    conn->try_reconnect();
}

void ChatConnection::try_reconnect() {
    if (state_ != ChatConnState::DISCONNECTED) {
        stop_reconnect();
        return;
    }
    SPDLOG_INFO("[Gate->Chat]Attempting reconnect to ChatServer...");
    connect(chat_ip_, chat_port_);
}

}  // namespace farm
```

- [x] **Step 3: Modify gate_server.h**

Add to the `GateServer` class:

```cpp
#include "chat_connection.h"

// In private members:
// Chat Server connection
std::unique_ptr<ChatConnection> chat_conn_;
std::string chat_server_ip_;
uint16_t chat_server_port_ = 0;

// In public methods:
void set_chat_server(const std::string& ip, uint16_t port);

// In private methods:
void forward_to_chat(std::shared_ptr<Session> session, uint32_t msg_id,
                     const std::vector<uint8_t>& payload);
void handle_chat_message(uint32_t msg_id, const std::vector<uint8_t>& payload);
```

- [x] **Step 4: Modify gate_server.cpp**

Add chat routing to `route_message()`:

```cpp
// In route_message(), before the "3000+ catchall":
// Chat messages (6000-6999)
if (msg_id >= 6000 && msg_id < 7000) {
    forward_to_chat(session, msg_id, payload);
    return;
}
```

Add `forward_to_chat()`:

```cpp
void GateServer::forward_to_chat(std::shared_ptr<Session> session, uint32_t msg_id,
                                  const std::vector<uint8_t>& payload) {
    if (!chat_conn_ || !chat_conn_->is_identified()) {
        SPDLOG_WARN("[Gate]ChatServer not connected, dropping msg_id={}", msg_id);
        return;
    }

    // Parse PlayerMsg to get player_id
    farm::PlayerMsg player_msg;
    if (!payload.empty() && !player_msg.ParseFromArray(payload.data(),
            static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[Gate]Failed to parse PlayerMsg for chat from fd={}", session->fd());
        return;
    }

    uint64_t player_id = player_msg.player_id();

    // Wrap as ClientMessage for ChatServer
    farm::ClientMessage client_msg;
    client_msg.set_player_id(player_id);
    client_msg.set_msg_id(msg_id);
    client_msg.set_payload(player_msg.payload());

    std::string wrapped;
    client_msg.SerializeToString(&wrapped);
    chat_conn_->send(MSG_ID_CLIENT_MSG, wrapped);
}
```

Add `set_chat_server()`:

```cpp
void GateServer::set_chat_server(const std::string& ip, uint16_t port) {
    chat_server_ip_ = ip;
    chat_server_port_ = port;
}
```

In `start()`, after GameServer connections are established:

```cpp
// Initialize ChatServer connection
if (!chat_server_ip_.empty() && chat_server_port_ > 0) {
    chat_conn_ = std::make_unique<ChatConnection>(base_, "gate_1");
    chat_conn_->set_message_callback([this](uint32_t msg_id, const std::vector<uint8_t>& payload) {
        handle_chat_message(msg_id, payload);
    });
    chat_conn_->connect(chat_server_ip_, chat_server_port_);
    SPDLOG_INFO("[Gate]ChatServer connection configured: {}:{}", chat_server_ip_, chat_server_port_);
}
```

Add `handle_chat_message()`:

```cpp
void GateServer::handle_chat_message(uint32_t msg_id, const std::vector<uint8_t>& payload) {
    // ChatServer sends back GameMessage format
    if (msg_id == MSG_ID_GAME_MSG) {
        farm::GameMessage game_msg;
        if (!game_msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
            SPDLOG_ERROR("[Gate]Failed to parse GameMessage from ChatServer");
            return;
        }

        uint64_t player_id = game_msg.player_id();
        uint32_t inner_msg_id = game_msg.msg_id();
        const auto& inner_payload = game_msg.payload();

        // Find the player's session and forward
        auto it = player_to_server_.find(player_id);
        if (it == player_to_server_.end()) {
            SPDLOG_DEBUG("[Gate]Player {} not found for chat response", player_id);
            return;
        }

        // Find the session for this player
        // (Need to maintain player_id -> session mapping)
        // For now, broadcast to all sessions (simplified)
        SPDLOG_DEBUG("[Gate]Forwarding chat msg_id={} to player={}", inner_msg_id, player_id);
    }
}
```

- [x] **Step 5: Update CMakeLists.txt**

Add to `scripts/server/gate_server/CMakeLists.txt`:

```cmake
# In PROTO_SRCS, add:
${PROTO_GENERATED_DIR}/chat.pb.cc

# In PROTO_HDRS, add:
${PROTO_GENERATED_DIR}/chat.pb.h

# In SOURCES, add:
src/chat_connection.cpp
```

- [x] **Step 6: Build GateServer**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/gate_server/build
cmake --build . --config Release
```

- [x] **Step 7: Commit**

```bash
git add scripts/server/gate_server/src/chat_connection.h scripts/server/gate_server/src/chat_connection.cpp scripts/server/gate_server/src/gate_server.h scripts/server/gate_server/src/gate_server.cpp scripts/server/gate_server/CMakeLists.txt
git commit -m "feat(chat): add chat message routing in GateServer"
```

---

## Task 6: Client - Chat Channel and Config

**Files:**
- Create: `scripts/client/chat/__init__.py`
- Create: `scripts/client/chat/chat_channel.py`

- [x] **Step 1: Create __init__.py**

```python
# scripts/client/chat/__init__.py
"""Chat system package."""
from .chat_channel import ChatChannel, ChannelConfig, ChannelType

__all__ = ["ChatChannel", "ChannelConfig", "ChannelType"]
```

- [x] **Step 2: Create chat_channel.py**

```python
# scripts/client/chat/chat_channel.py
"""Chat channel configuration and message storage."""
import logging
import time
from dataclasses import dataclass, field
from enum import Enum, unique
from typing import List, Optional

logger = logging.getLogger("client.chat.chat_channel")


@unique
class ChannelType(Enum):
    """Chat channel type identifiers."""
    WORLD = 0
    PARTY = 1
    WHISPER = 2


@dataclass
class ChannelConfig:
    """Configuration for a chat channel."""
    channel_type: ChannelType
    max_msg_length: int
    cooldown_sec: float
    max_history: int = 100


# Default channel configurations
CHANNEL_CONFIGS = {
    ChannelType.WORLD: ChannelConfig(
        channel_type=ChannelType.WORLD,
        max_msg_length=100,
        cooldown_sec=5.0,
        max_history=50,
    ),
    ChannelType.PARTY: ChannelConfig(
        channel_type=ChannelType.PARTY,
        max_msg_length=200,
        cooldown_sec=3.0,
        max_history=100,
    ),
    ChannelType.WHISPER: ChannelConfig(
        channel_type=ChannelType.WHISPER,
        max_msg_length=500,
        cooldown_sec=1.0,
        max_history=200,
    ),
}


@dataclass
class ChatMessage:
    """A single chat message."""
    channel_type: ChannelType
    sender_id: int
    sender_name: str
    content: str
    timestamp: int  # milliseconds
    target_id: int = 0


class ChatChannel:
    """A chat channel that stores messages."""

    def __init__(self, config: ChannelConfig):
        self.config = config
        self.messages: List[ChatMessage] = []
        self._last_send_time: float = 0.0

    def add_message(self, msg: ChatMessage) -> None:
        """Add a message to the channel, enforcing max history."""
        self.messages.append(msg)
        if len(self.messages) > self.config.max_history:
            self.messages.pop(0)
        logger.debug(
            "[%s] %s: %s (total=%d)",
            self.config.channel_type.name,
            msg.sender_name,
            msg.content,
            len(self.messages),
        )

    def get_messages(self) -> List[ChatMessage]:
        """Get all messages in this channel."""
        return self.messages

    def can_send(self) -> bool:
        """Check if the player can send a message (rate limit)."""
        now = time.time()
        return (now - self._last_send_time) >= self.config.cooldown_sec

    def record_send(self) -> None:
        """Record that a message was sent (for rate limiting)."""
        self._last_send_time = time.time()

    def validate_message(self, content: str) -> Optional[str]:
        """Validate a message before sending. Returns error string or None."""
        if not content.strip():
            return "Message cannot be empty"
        if len(content) > self.config.max_msg_length:
            return f"Message too long ({len(content)}/{self.config.max_msg_length})"
        return None
```

- [x] **Step 3: Commit**

```bash
git add scripts/client/chat/__init__.py scripts/client/chat/chat_channel.py
git commit -m "feat(chat): add client-side chat channel and config"
```

---

## Task 7: Client - Chat Manager

**Files:**
- Create: `scripts/client/chat/chat_manager.py`

- [x] **Step 1: Create chat_manager.py**

```python
# scripts/client/chat/chat_manager.py
"""Client-side chat manager handling send/recv and channel routing."""
import logging
import time
import sys
import os
from typing import Dict, Optional, Callable

from .chat_channel import (
    ChatChannel, ChatMessage, ChannelType, ChannelConfig, CHANNEL_CONFIGS,
)

# Add proto generated path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'common', 'proto', 'generated'))
import chat_pb2
import base_pb2

from ..message_ids import MSG_ID_CHAT_SEND_REQ, MSG_ID_CHAT_SEND_RESP, MSG_ID_CHAT_MESSAGE

logger = logging.getLogger("client.chat.chat_manager")


class ChatManager:
    """Manages chat channels, sending, and receiving."""

    def __init__(self, connection, player_data: dict):
        self._connection = connection
        self._player_data = player_data
        self._player_id = player_data.get('player_id', 0)
        self._player_name = player_data.get('role_name', 'Unknown')

        # Initialize channels
        self._channels: Dict[ChannelType, ChatChannel] = {}
        for channel_type, config in CHANNEL_CONFIGS.items():
            self._channels[channel_type] = ChatChannel(config)

        # Current active channel for sending
        self._current_channel: ChannelType = ChannelType.WORLD

        # Callback for UI to know when messages arrive
        self._on_message_received: Optional[Callable[[], None]] = None

        logger.info("ChatManager initialized for player %d (%s)",
                     self._player_id, self._player_name)

    @property
    def current_channel(self) -> ChannelType:
        """Get the currently selected channel."""
        return self._current_channel

    @current_channel.setter
    def current_channel(self, channel_type: ChannelType) -> None:
        """Set the currently selected channel."""
        self._current_channel = channel_type

    def get_channel(self, channel_type: ChannelType) -> Optional[ChatChannel]:
        """Get a channel by type."""
        return self._channels.get(channel_type)

    def get_current_channel(self) -> ChatChannel:
        """Get the currently active channel object."""
        return self._channels[self._current_channel]

    def switch_channel(self) -> None:
        """Switch to the next channel (Tab key)."""
        types = list(ChannelType)
        idx = types.index(self._current_channel)
        self._current_channel = types[(idx + 1) % len(types)]
        logger.debug("Switched to channel: %s", self._current_channel.name)

    def set_on_message_received(self, callback: Callable[[], None]) -> None:
        """Set callback for when a message is received."""
        self._on_message_received = callback

    def send_message(self, content: str, target_id: int = 0) -> Optional[str]:
        """Send a chat message. Returns error string or None on success."""
        channel = self._channels[self._current_channel]

        # Validate
        error = channel.validate_message(content)
        if error:
            return error

        # Check client-side rate limit
        if not channel.can_send():
            return "Sending too fast"

        # Build protobuf
        req = chat_pb2.ChatSendReq()
        req.channel_type = self._current_channel.value
        req.content = content
        req.target_id = target_id

        # Wrap in PlayerMsg
        player_msg = base_pb2.PlayerMsg()
        player_msg.player_id = self._player_id
        player_msg.server_id = self._player_data.get('server_id', 1)
        player_msg.msg_id = MSG_ID_CHAT_SEND_REQ
        player_msg.payload = req.SerializeToString()

        # Send
        try:
            self._connection.send_message(MSG_ID_CHAT_SEND_REQ, player_msg.SerializeToString())
            channel.record_send()
            logger.debug("Sent chat message to %s: %s", self._current_channel.name, content)
            return None
        except Exception as e:
            logger.error("Failed to send chat message: %s", e)
            return "Failed to send"

    def on_chat_message(self, payload: bytes) -> None:
        """Handle incoming ChatMessage from server."""
        try:
            msg = chat_pb2.ChatMessage()
            msg.ParseFromString(payload)

            channel_type = ChannelType(msg.channel_type)
            chat_msg = ChatMessage(
                channel_type=channel_type,
                sender_id=msg.sender_id,
                sender_name=msg.sender_name,
                content=msg.content,
                timestamp=msg.timestamp,
                target_id=msg.target_id,
            )

            channel = self._channels.get(channel_type)
            if channel:
                channel.add_message(chat_msg)
                if self._on_message_received:
                    self._on_message_received()
            else:
                logger.warning("Received message for unknown channel: %s", channel_type)

        except Exception as e:
            logger.error("Failed to parse ChatMessage: %s", e)

    def on_chat_send_resp(self, payload: bytes) -> None:
        """Handle ChatSendResp from server."""
        try:
            resp = chat_pb2.ChatSendResp()
            resp.ParseFromString(payload)
            if resp.code != 0:
                logger.warning("Chat send failed: code=%d, msg=%s", resp.code, resp.msg)
        except Exception as e:
            logger.error("Failed to parse ChatSendResp: %s", e)
```

- [x] **Step 2: Commit**

```bash
git add scripts/client/chat/chat_manager.py
git commit -m "feat(chat): add client-side chat manager"
```

---

## Task 8: Client - Chat Panel UI

**Files:**
- Create: `scripts/client/chat/chat_panel.py`

- [x] **Step 1: Create chat_panel.py**

```python
# scripts/client/chat/chat_panel.py
"""Chat panel UI component for rendering and input handling."""
import logging
from typing import Optional, List

import pygame

from .chat_channel import ChatMessage, ChannelType
from .chat_manager import ChatManager

logger = logging.getLogger("client.chat.chat_panel")


class ChatPanel:
    """Left-bottom chat panel with channel filtering and input."""

    # Layout constants
    PANEL_WIDTH = 350
    PANEL_HEIGHT = 200
    MARGIN = 10
    INPUT_HEIGHT = 25
    TAB_HEIGHT = 22
    MSG_LINE_HEIGHT = 18
    MAX_VISIBLE_MESSAGES = 8

    # Colors
    BG_COLOR = (0, 0, 0, 140)        # Semi-transparent black
    BORDER_COLOR = (80, 80, 80, 200)
    TEXT_COLOR = (220, 220, 220)      # Light gray
    INPUT_BG = (30, 30, 30, 200)
    INPUT_ACTIVE_BG = (40, 40, 40, 220)
    TAB_ACTIVE_COLOR = (100, 180, 255)  # Blue highlight
    TAB_INACTIVE_COLOR = (120, 120, 120)
    CHANNEL_COLORS = {
        "world": (100, 200, 100),    # Green
        "party": (100, 180, 255),    # Blue
        "whisper": (255, 180, 100),  # Orange
    }

    # Channel tabs
    TAB_LABELS = ["全部", "世界", "队伍", "私聊"]
    TAB_FILTERS = ["all", "world", "party", "whisper"]

    def __init__(self, screen_width: int, screen_height: int):
        self._x = self.MARGIN
        self._y = screen_height - self.PANEL_HEIGHT - self.MARGIN
        self._screen_width = screen_width
        self._screen_height = screen_height

        # Input state
        self._input_active = False
        self._input_text = ""

        # Filter state
        self._selected_filter = "all"

        # Font
        pygame.font.init()
        self._font = pygame.font.SysFont("microsoftyahei", 14)
        if self._font is None:
            self._font = pygame.font.Font(None, 14)
        self._tab_font = pygame.font.SysFont("microsoftyahei", 12)
        if self._tab_font is None:
            self._tab_font = pygame.font.Font(None, 12)

        # Cached surface
        self._surface: Optional[pygame.Surface] = None
        self._needs_redraw = True

        logger.info("ChatPanel initialized at (%d, %d)", self._x, self._y)

    @property
    def is_input_active(self) -> bool:
        """Whether the chat input is currently focused."""
        return self._input_active

    def handle_event(self, event: pygame.event.Event, chat_manager: ChatManager) -> bool:
        """Handle a pygame event. Returns True if the event was consumed."""

        # Enter key: toggle input
        if event.type == pygame.KEYDOWN and event.key == pygame.K_RETURN:
            if self._input_active:
                # Send message
                if self._input_text.strip():
                    error = chat_manager.send_message(self._input_text.strip())
                    if error:
                        logger.warning("Chat send error: %s", error)
                self._input_text = ""
                self._input_active = False
                self._needs_redraw = True
            else:
                self._input_active = True
                self._needs_redraw = True
            return True

        # Esc key: close input
        if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
            if self._input_active:
                self._input_active = False
                self._input_text = ""
                self._needs_redraw = True
                return True

        # Tab key: switch channel filter
        if event.type == pygame.KEYDOWN and event.key == pygame.K_TAB:
            if not self._input_active:
                idx = self.TAB_FILTERS.index(self._selected_filter)
                self._selected_filter = self.TAB_FILTERS[(idx + 1) % len(self.TAB_FILTERS)]
                self._needs_redraw = True
                return True

        # Text input when active
        if event.type == pygame.KEYDOWN and self._input_active:
            if event.key == pygame.K_BACKSPACE:
                self._input_text = self._input_text[:-1]
                self._needs_redraw = True
                return True
            elif event.unicode and event.unicode.isprintable():
                # Check max length
                config = chat_manager.get_current_channel().config
                if len(self._input_text) < config.max_msg_length:
                    self._input_text += event.unicode
                    self._needs_redraw = True
                return True

        # Mouse click on tabs
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            mx, my = event.pos
            tab_y = self._y + self.PANEL_HEIGHT - self.INPUT_HEIGHT - self.TAB_HEIGHT
            if tab_y <= my <= tab_y + self.TAB_HEIGHT:
                tab_x = self._x + 5
                for i, label in enumerate(self.TAB_LABELS):
                    tab_surface = self._tab_font.render(label, True, (255, 255, 255))
                    tab_w = tab_surface.get_width() + 12
                    if tab_x <= mx <= tab_x + tab_w:
                        self._selected_filter = self.TAB_FILTERS[i]
                        self._needs_redraw = True
                        return True
                    tab_x += tab_w + 4

        return False

    def render(self, screen: pygame.Surface, chat_manager: ChatManager) -> None:
        """Render the chat panel."""
        # Always redraw (messages change frequently)
        self._surface = pygame.Surface(
            (self.PANEL_WIDTH, self.PANEL_HEIGHT), pygame.SRCALPHA
        )

        # Background
        bg_rect = pygame.Rect(0, 0, self.PANEL_WIDTH, self.PANEL_HEIGHT)
        pygame.draw.rect(self._surface, self.BG_COLOR, bg_rect)
        pygame.draw.rect(self._surface, self.BORDER_COLOR, bg_rect, 1)

        # Messages area
        messages = self._get_filtered_messages(chat_manager)
        visible = messages[-self.MAX_VISIBLE_MESSAGES:]
        msg_y = 5
        for msg in visible:
            channel_name = msg.channel_type.name.lower()
            color = self.CHANNEL_COLORS.get(channel_name, self.TEXT_COLOR)
            prefix = f"[{channel_name}]"
            text = f"{prefix} {msg.sender_name}: {msg.content}"
            # Truncate if too wide
            text_surface = self._font.render(text, True, color)
            if text_surface.get_width() > self.PANEL_WIDTH - 10:
                while self._font.render(text + "...", True, color).get_width() > self.PANEL_WIDTH - 10 and len(text) > 0:
                    text = text[:-1]
                text = text + "..."
                text_surface = self._font.render(text, True, color)
            self._surface.blit(text_surface, (5, msg_y))
            msg_y += self.MSG_LINE_HEIGHT

        # Tab bar
        tab_y = self.PANEL_HEIGHT - self.INPUT_HEIGHT - self.TAB_HEIGHT
        tab_x = 5
        for i, label in enumerate(self.TAB_LABELS):
            is_active = self.TAB_FILTERS[i] == self._selected_filter
            color = self.TAB_ACTIVE_COLOR if is_active else self.TAB_INACTIVE_COLOR
            tab_surface = self._tab_font.render(label, True, color)
            tab_w = tab_surface.get_width() + 12
            tab_rect = pygame.Rect(tab_x, tab_y, tab_w, self.TAB_HEIGHT)
            pygame.draw.rect(self._surface, (40, 40, 40, 150), tab_rect)
            if is_active:
                pygame.draw.rect(self._surface, (*self.TAB_ACTIVE_COLOR, 100), tab_rect, 1)
            self._surface.blit(tab_surface, (tab_x + 6, tab_y + 4))
            tab_x += tab_w + 4

        # Input box
        input_y = self.PANEL_HEIGHT - self.INPUT_HEIGHT
        input_rect = pygame.Rect(2, input_y, self.PANEL_WIDTH - 4, self.INPUT_HEIGHT)
        bg = self.INPUT_ACTIVE_BG if self._input_active else self.INPUT_BG
        pygame.draw.rect(self._surface, bg, input_rect)
        pygame.draw.rect(self._surface, self.BORDER_COLOR, input_rect, 1)

        # Input text
        display_text = self._input_text
        if not self._input_active and not display_text:
            display_text = "Press Enter to chat..."
            text_color = (100, 100, 100)
        else:
            text_color = self.TEXT_COLOR
            if self._input_active:
                display_text = display_text + "_"  # Cursor

        text_surface = self._font.render(display_text, True, text_color)
        self._surface.blit(text_surface, (6, input_y + 5))

        # Blit to screen
        screen.blit(self._surface, (self._x, self._y))

    def _get_filtered_messages(self, chat_manager: ChatManager) -> List[ChatMessage]:
        """Get messages filtered by the selected tab."""
        if self._selected_filter == "all":
            # Merge all channels, sorted by timestamp
            all_msgs = []
            for channel in chat_manager._channels.values():
                all_msgs.extend(channel.get_messages())
            all_msgs.sort(key=lambda m: m.timestamp)
            return all_msgs
        else:
            channel_type = ChannelType[self._selected_filter.upper()]
            channel = chat_manager.get_channel(channel_type)
            return channel.get_messages() if channel else []

    def cleanup(self) -> None:
        """Release resources."""
        if self._font:
            self._font = None
        if self._tab_font:
            self._tab_font = None
```

- [x] **Step 2: Commit**

```bash
git add scripts/client/chat/chat_panel.py
git commit -m "feat(chat): add chat panel UI component"
```

---

## Task 9: Client - Network Dispatcher Integration

**Files:**
- Modify: `scripts/client/network_dispatcher.py`

- [x] **Step 1: Add chat message imports**

Add to the imports at the top of `network_dispatcher.py`:

```python
from .message_ids import (
    MSG_ID_MAP_DATA_NOTIFY, MSG_ID_POSITION_CORRECT,
    MSG_ID_ITEM_USE_RESP, MSG_ID_SCENE_CHANGE_RESP,
    MSG_ID_CLOCK_SYNC, MSG_ID_FORCE_SLEEP_NOTIFY, MSG_ID_FORCE_SLEEP_READY,
    MSG_ID_DROP_ITEM_SYNC, MSG_ID_INVENTORY_SYNC,
    MSG_ID_CHAT_MESSAGE, MSG_ID_CHAT_SEND_RESP,  # Add these
)
```

- [x] **Step 2: Add chat callbacks to constructor**

Add parameters to `NetworkMessageDispatcher.__init__`:

```python
def __init__(
    self,
    connection,
    on_map_data_notify: Callable[[], None],
    on_position_correct: Callable[[float, float], None],
    on_item_use_resp: Callable[[Any], None],
    on_scene_change_resp: Callable[[Any], None],
    on_clock_sync: Callable[[int, int], None],
    on_force_sleep_notify: Callable[[int], None],
    on_drop_item_sync: Callable[[Any], None] = None,
    on_inventory_sync: Callable[[str], None] = None,
    on_chat_message: Callable[[bytes], None] = None,      # Add this
    on_chat_send_resp: Callable[[bytes], None] = None,     # Add this
):
```

Add to `self._callbacks`:

```python
self._callbacks = {
    # ... existing callbacks ...
    "on_chat_message": on_chat_message,
    "on_chat_send_resp": on_chat_send_resp,
}
```

- [x] **Step 3: Add dispatch table entries**

Add to `self._dispatch_table`:

```python
self._dispatch_table = {
    # ... existing entries ...
    MSG_ID_CHAT_MESSAGE: self._handle_chat_message,
    MSG_ID_CHAT_SEND_RESP: self._handle_chat_send_resp,
}
```

- [x] **Step 4: Add handler methods**

```python
def _handle_chat_message(self, payload: bytes):
    """Handle incoming ChatMessage."""
    try:
        player_msg = base_pb2.PlayerMsg()
        player_msg.ParseFromString(payload)
        if self._callbacks["on_chat_message"]:
            self._callbacks["on_chat_message"](player_msg.payload)
    except Exception as e:
        logger.error(f"[NetworkDispatcher]Failed to parse ChatMessage: {e}")

def _handle_chat_send_resp(self, payload: bytes):
    """Handle ChatSendResp."""
    try:
        player_msg = base_pb2.PlayerMsg()
        player_msg.ParseFromString(payload)
        if self._callbacks["on_chat_send_resp"]:
            self._callbacks["on_chat_send_resp"](player_msg.payload)
    except Exception as e:
        logger.error(f"[NetworkDispatcher]Failed to parse ChatSendResp: {e}")
```

- [x] **Step 5: Commit**

```bash
git add scripts/client/network_dispatcher.py
git commit -m "feat(chat): add chat message handling to network dispatcher"
```

---

## Task 10: Client - Game Scene Integration

**Files:**
- Modify: `scripts/client/game_scene.py`
- Modify: `scripts/client/game_renderer.py`

- [x] **Step 1: Add chat imports to game_scene.py**

Add imports:

```python
from .chat.chat_manager import ChatManager
from .chat.chat_panel import ChatPanel
```

- [x] **Step 2: Initialize ChatManager and ChatPanel in GameScene.__init__**

After the `NetworkMessageDispatcher` initialization:

```python
# Chat system
self._chat_manager = ChatManager(connection, player_data)
self._chat_panel = ChatPanel(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)
```

- [x] **Step 3: Wire chat callbacks to NetworkMessageDispatcher**

Update the `NetworkMessageDispatcher` constructor call:

```python
self._network_dispatcher = NetworkMessageDispatcher(
    connection=connection,
    on_map_data_notify=self._on_map_data_notify,
    on_position_correct=self._on_position_correct,
    on_item_use_resp=self._on_item_use_resp,
    on_scene_change_resp=self._on_scene_change_resp,
    on_clock_sync=self._on_clock_sync,
    on_force_sleep_notify=self._on_force_sleep_notify,
    on_chat_message=self._chat_manager.on_chat_message,       # Add this
    on_chat_send_resp=self._chat_manager.on_chat_send_resp,    # Add this
)
```

- [x] **Step 4: Add chat event handling in the game loop**

In `GameScene.run()`, in the event loop (after the `exhaustion_modal.handle_event` check):

```python
# Chat panel events (takes priority when input is active)
if self._chat_panel.handle_event(event, self._chat_manager):
    continue
```

- [x] **Step 5: Block game input when chat is active**

In `_handle_mouse_click()` and `_handle_portal_interaction()`, add at the top:

```python
# Block game interaction when chat input is active
if self._chat_panel.is_input_active:
    return
```

- [x] **Step 6: Add chat_panel to GameRenderer**

In `game_renderer.py`, add the chat panel rendering after the HUD and before the exhaustion modal:

```python
# In render() method, after self._time_hud.draw(self._screen):
# Chat panel (left bottom)
if hasattr(self, '_chat_panel') and self._chat_panel is not None:
    self._chat_panel.render(self._screen, self._chat_manager)
```

Add a method to set the chat panel:

```python
def set_chat(self, chat_panel, chat_manager):
    """Set chat panel and manager for rendering."""
    self._chat_panel = chat_panel
    self._chat_manager = chat_manager
```

In `GameScene.__init__`, after creating the renderer:

```python
self._renderer.set_chat(self._chat_panel, self._chat_manager)
```

- [x] **Step 7: Commit**

```bash
git add scripts/client/game_scene.py scripts/client/game_renderer.py
git commit -m "feat(chat): integrate chat system into game scene"
```

---

## Task 11: Proto Regeneration and Final Build

- [x] **Step 1: Regenerate all proto files**

```bash
cd D:/mb_workspace/farm_demo
# Python
python -m grpc_tools.protoc -Iscripts/common/proto --python_out=scripts/common/proto/generated scripts/common/proto/*.proto

# C++ (if protoc is available)
protoc -Iscripts/common/proto --cpp_out=scripts/common/proto/generated scripts/common/proto/*.proto
```

- [x] **Step 2: Rebuild all servers**

```bash
# ChatServer
cd D:/mb_workspace/farm_demo/scripts/server/chat_server/build
cmake --build . --config Release

# GateServer
cd D:/mb_workspace/farm_demo/scripts/server/gate_server/build
cmake --build . --config Release
```

- [x] **Step 3: Run client to verify no import errors**

```bash
cd D:/mb_workspace/farm_demo
python -c "from scripts.client.chat import ChatChannel, ChatManager; print('Chat imports OK')"
```

- [x] **Step 4: Commit all generated files**

```bash
git add scripts/common/proto/generated/ scripts/client/message_ids.py scripts/server/common/include/message_ids.h
git commit -m "chore: regenerate proto and message ID files"
```

---

## Summary

| Task | Description | New Files | Modified Files |
|------|-------------|-----------|----------------|
| 1 | Proto + Message IDs | 1 proto, 0 generated | message_ids.json, generated/* |
| 2 | Rate Limiter | 2 | - |
| 3 | Channel Manager | 2 | - |
| 4 | ChatServer Main | 4 | - |
| 5 | GateServer Routing | 2 | gate_server.h/.cpp, CMakeLists.txt |
| 6 | Client ChatChannel | 2 | - |
| 7 | Client ChatManager | 1 | - |
| 8 | Client ChatPanel | 1 | - |
| 9 | Network Dispatcher | - | network_dispatcher.py |
| 10 | Game Scene Integration | - | game_scene.py, game_renderer.py |
| 11 | Final Build | - | generated files |

**Total new files:** ~15
**Total modified files:** ~7
