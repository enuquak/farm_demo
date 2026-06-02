# Friend System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a complete social friend system as an independent C++ microservice (Friend Service), with friend management, private chat, gifting, farm visiting (mutual help + crop stealing), and client UI.

**Architecture:** New Friend Service microservice connects to Game Server via TCP (same wire protocol as Game↔DBMgr). Game Server forwards client friend messages to Friend Service. Friend Service uses Redis for fast lookups (friend sets, online status, offline message queues) and DBMgr for persistence. Client gets new UI panels for friend list, chat, search, and farm visit.

**Tech Stack:** C++17, libevent, hiredis, protobuf, spdlog (server). Python, pygame (client).

---

## File Structure

### New Files — Friend Service (C++ server)

| File | Responsibility |
|------|---------------|
| `scripts/server/friend_service/CMakeLists.txt` | Build config for Friend Service |
| `scripts/server/friend_service/src/main.cpp` | Entry point, event loop, signal handling |
| `scripts/server/friend_service/src/friend_service.h/cpp` | Core service: owns managers, handles Game Server connection |
| `scripts/server/friend_service/src/game_session.h/cpp` | TCP session with Game Server (bufferevent, heartbeat, identify) |
| `scripts/server/friend_service/src/friend_manager.h/cpp` | Friend CRUD: add/delete/list/search/block, Redis operations |
| `scripts/server/friend_service/src/chat_manager.h/cpp` | Private chat: send/history/offline queue, Redis + DBMgr persistence |
| `scripts/server/friend_service/src/gift_manager.h/cpp` | Gift sending: validate item, transfer, notify |
| `scripts/server/friend_service/src/visit_manager.h/cpp` | Farm visit: authorize/visit/action, active visitor tracking |
| `scripts/server/friend_service/src/recommend_manager.h/cpp` | Friend recommendation: common-friends algorithm |
| `scripts/server/friend_service/src/friend_constants.h` | Error codes, limits (max friends, msg length, etc.) |
| `scripts/server/friend_service/src/friend_data_manager.h/cpp` | Redis↔DBMgr sync: load/save/persist dirty data |
| `scripts/server/friend_service/tests/test_friend_manager.cpp` | Unit tests for FriendManager |

### New Files — Protobuf

| File | Responsibility |
|------|---------------|
| `scripts/common/proto/friend.proto` | All friend system message definitions |

### Modified Files — Shared

| File | Change |
|------|--------|
| `shared/message_ids.json` | Add 28 friend MsgIDs (8001-5063) |
| `scripts/common/proto/generated/friend_pb2.py` | Regenerated from friend.proto |
| `scripts/common/proto/generated/friend.pb.cc` | Regenerated from friend.proto |
| `scripts/common/proto/generated/friend.pb.h` | Regenerated from friend.proto |

### Modified Files — Game Server

| File | Change |
|------|--------|
| `scripts/server/game_server/src/game_server.h` | Add FriendServiceConnection member |
| `scripts/server/game_server/src/game_server.cpp` | Register friend message forwarding handlers |
| `scripts/server/game_server/CMakeLists.txt` | Add friend_service_connection source files |

### New Files — Game Server (friend forwarding)

| File | Responsibility |
|------|---------------|
| `scripts/server/game_server/src/friend_service_connection.h/cpp` | TCP connection to Friend Service (mirrors DBMgrConnection pattern) |

### Modified Files — Client

| File | Change |
|------|--------|
| `scripts/client/message_ids.py` | Regenerated (auto) |
| `scripts/client/network_dispatcher.py` | Add friend message handlers + callbacks |
| `scripts/client/game_scene.py` | Wire friend callbacks, create FriendUIManager |

### New Files — Client UI

| File | Responsibility |
|------|---------------|
| `scripts/client/friend_client.py` | Friend network client: send requests, hold state |
| `scripts/client/ui/friend_list_panel.py` | Friend list panel rendering + hit testing |
| `scripts/client/ui/friend_chat_panel.py` | Private chat panel |
| `scripts/client/ui/friend_search_panel.py` | Search/add friend panel |
| `scripts/client/ui/friend_gift_panel.py` | Gift item selection panel |
| `scripts/client/ui/friend_visit_panel.py` | Farm visit HUD |

---

## Task 1: Protobuf Messages + Message IDs

**Files:**
- Create: `scripts/common/proto/friend.proto`
- Modify: `shared/message_ids.json`
- Regenerate: `scripts/common/proto/generated/friend_pb2.py`, `friend.pb.cc`, `friend.pb.h`
- Regenerate: `scripts/client/message_ids.py`, `scripts/server/common/include/message_ids.h`

- [ ] **Step 1: Create friend.proto**

```protobuf
syntax = "proto3";

package farm;

option optimize_for = LITE_RUNTIME;

// ===========================================
// 好友系统协议
// MsgID 范围: 5000-5999 (Client<->Game<->FriendService)
// ===========================================

// 错误码
enum FriendErrorCode {
    FRIEND_SUCCESS = 0;
    FRIEND_ALREADY_FRIENDS = 1;
    FRIEND_LIST_FULL = 2;
    FRIEND_PLAYER_NOT_FOUND = 3;
    FRIEND_REQUEST_NOT_FOUND = 4;
    FRIEND_NOT_FRIENDS = 5;
    FRIEND_CHAT_MSG_TOO_LONG = 6;
    FRIEND_GIFT_ITEM_NOT_FOUND = 7;
    FRIEND_GIFT_COUNT_INVALID = 8;
    FRIEND_VISIT_NOT_AUTHORIZED = 9;
    FRIEND_VISIT_ACTION_FAILED = 10;
    FRIEND_SELF_OPERATION = 11;
    FRIEND_BLOCKED = 12;
    FRIEND_COOLDOWN = 13;
}

// 好友信息
message FriendInfo {
    uint64 player_id = 1;
    string role_name = 2;
    uint32 level = 3;
    bool online = 4;
    string scene_id = 5;
    uint64 last_login = 6;
}

// 好友请求
message FriendRequestInfo {
    uint64 sender_id = 1;
    string sender_name = 2;
    uint32 sender_level = 3;
    uint64 timestamp = 4;
}

// 搜索玩家
message FriendSearchReq {
    string keyword = 1;
}

message FriendSearchResp {
    int32 code = 1;
    repeated FriendInfo results = 2;
}

// 发送好友请求
message FriendAddReq {
    uint64 target_id = 1;
}

message FriendAddResp {
    int32 code = 1;
    string msg = 2;
}

// 收到好友请求通知
message FriendAddNotify {
    FriendRequestInfo request = 1;
}

// 接受好友请求
message FriendAcceptReq {
    uint64 sender_id = 1;
}

message FriendAcceptResp {
    int32 code = 1;
    FriendInfo new_friend = 2;
}

// 拒绝好友请求
message FriendRejectReq {
    uint64 sender_id = 1;
}

message FriendRejectResp {
    int32 code = 1;
}

// 删除好友
message FriendDeleteReq {
    uint64 target_id = 1;
}

message FriendDeleteResp {
    int32 code = 1;
}

// 好友列表
message FriendListReq {
}

message FriendListResp {
    int32 code = 1;
    repeated FriendInfo friends = 2;
    repeated FriendRequestInfo pending_requests = 3;
}

// 好友上线/下线通知
message FriendOnlineNotify {
    uint64 player_id = 1;
    string role_name = 2;
    string scene_id = 3;
}

message FriendOfflineNotify {
    uint64 player_id = 1;
}

// 私聊消息
message FriendChatReq {
    uint64 receiver_id = 1;
    int32 msg_type = 2;     // 0=text, 1=emote
    string content = 3;
}

message FriendChatResp {
    int32 code = 1;
}

message FriendChatNotify {
    uint64 sender_id = 1;
    string sender_name = 2;
    int32 msg_type = 3;
    string content = 4;
    uint64 timestamp = 5;
}

// 聊天历史
message FriendChatHistoryReq {
    uint64 target_id = 1;
}

message FriendChatHistoryMsg {
    uint64 sender_id = 1;
    uint64 receiver_id = 2;
    int32 msg_type = 3;
    string content = 4;
    uint64 timestamp = 5;
}

message FriendChatHistoryResp {
    int32 code = 1;
    repeated FriendChatHistoryMsg messages = 2;
}

// 赠送礼物
message FriendGiftReq {
    uint64 receiver_id = 1;
    int32 item_id = 2;
    int32 count = 3;
}

message FriendGiftResp {
    int32 code = 1;
    string msg = 2;
}

message FriendGiftNotify {
    uint64 sender_id = 1;
    string sender_name = 2;
    int32 item_id = 3;
    int32 count = 4;
}

// 访问好友农场
message FriendVisitReq {
    uint64 owner_id = 1;
}

message FriendVisitResp {
    int32 code = 1;
    string owner_name = 2;
    string scene_id = 3;
}

// 访问中操作
message FriendVisitActionReq {
    uint64 owner_id = 1;
    int32 action_type = 2;  // 0=water, 1=weed, 2=harvest, 3=steal
    int32 target_x = 3;
    int32 target_y = 4;
}

message FriendVisitActionResp {
    int32 code = 1;
    string msg = 2;
}

// 推荐好友
message FriendRecommendReq {
}

message FriendRecommendResp {
    int32 code = 1;
    repeated FriendInfo recommendations = 2;
}

// 拉黑/取消拉黑
message FriendBlockReq {
    uint64 target_id = 1;
}

message FriendBlockResp {
    int32 code = 1;
}

message FriendUnblockReq {
    uint64 target_id = 1;
}

message FriendUnblockResp {
    int32 code = 1;
}

// ===========================================
// Friend Service 内部通信协议
// MsgID 范围: 6000-6199 (Game<->FriendService)
// ===========================================

// Friend Service 身份标识
message FriendServiceIdentify {
    uint32 service_id = 1;
    string address = 2;
}

message FriendServiceIdentifyResp {
    int32 code = 1;
}

// 内部心跳 (复用 DBMgr 心跳格式)
message FriendServiceHeartbeat {
    uint64 timestamp = 1;
}

message FriendServiceHeartbeatResp {
    uint64 timestamp = 1;
}

// 转发客户端消息
message FriendClientMessage {
    uint64 player_id = 1;
    uint32 msg_id = 2;
    bytes payload = 3;
}

// 返回客户端消息
message FriendServiceMessage {
    uint64 player_id = 1;
    uint32 msg_id = 2;
    bytes payload = 3;
}
```

- [ ] **Step 2: Add friend MsgIDs to shared/message_ids.json**

Add these entries to the `message_ids` array in `shared/message_ids.json`:

```json
{"code": 8001, "name": "MSG_ID_FRIEND_SEARCH_REQ", "description": "搜索玩家请求"},
{"code": 5002, "name": "MSG_ID_FRIEND_SEARCH_RESP", "description": "搜索玩家响应"},
{"code": 5003, "name": "MSG_ID_FRIEND_ADD_REQ", "description": "发送好友请求"},
{"code": 5004, "name": "MSG_ID_FRIEND_ADD_RESP", "description": "好友请求结果"},
{"code": 5005, "name": "MSG_ID_FRIEND_ADD_NOTIFY", "description": "收到好友请求通知"},
{"code": 5006, "name": "MSG_ID_FRIEND_ACCEPT_REQ", "description": "接受好友请求"},
{"code": 5007, "name": "MSG_ID_FRIEND_ACCEPT_RESP", "description": "接受好友结果"},
{"code": 5008, "name": "MSG_ID_FRIEND_REJECT_REQ", "description": "拒绝好友请求"},
{"code": 5009, "name": "MSG_ID_FRIEND_REJECT_RESP", "description": "拒绝好友结果"},
{"code": 5010, "name": "MSG_ID_FRIEND_DELETE_REQ", "description": "删除好友"},
{"code": 5011, "name": "MSG_ID_FRIEND_DELETE_RESP", "description": "删除好友结果"},
{"code": 5012, "name": "MSG_ID_FRIEND_LIST_REQ", "description": "获取好友列表"},
{"code": 5013, "name": "MSG_ID_FRIEND_LIST_RESP", "description": "好友列表数据"},
{"code": 5014, "name": "MSG_ID_FRIEND_ONLINE_NOTIFY", "description": "好友上线通知"},
{"code": 5015, "name": "MSG_ID_FRIEND_OFFLINE_NOTIFY", "description": "好友下线通知"},
{"code": 5020, "name": "MSG_ID_FRIEND_CHAT_REQ", "description": "发送私聊消息"},
{"code": 5021, "name": "MSG_ID_FRIEND_CHAT_RESP", "description": "私聊发送结果"},
{"code": 5022, "name": "MSG_ID_FRIEND_CHAT_NOTIFY", "description": "收到私聊消息"},
{"code": 5023, "name": "MSG_ID_FRIEND_CHAT_HISTORY_REQ", "description": "获取聊天记录"},
{"code": 5024, "name": "MSG_ID_FRIEND_CHAT_HISTORY_RESP", "description": "聊天记录数据"},
{"code": 5030, "name": "MSG_ID_FRIEND_GIFT_REQ", "description": "赠送物品请求"},
{"code": 5031, "name": "MSG_ID_FRIEND_GIFT_RESP", "description": "赠送物品结果"},
{"code": 5032, "name": "MSG_ID_FRIEND_GIFT_NOTIFY", "description": "收到礼物通知"},
{"code": 5040, "name": "MSG_ID_FRIEND_VISIT_REQ", "description": "访问好友农场"},
{"code": 5041, "name": "MSG_ID_FRIEND_VISIT_RESP", "description": "访问农场结果"},
{"code": 5042, "name": "MSG_ID_FRIEND_VISIT_ACTION_REQ", "description": "农场访问操作"},
{"code": 5043, "name": "MSG_ID_FRIEND_VISIT_ACTION_RESP", "description": "农场操作结果"},
{"code": 5050, "name": "MSG_ID_FRIEND_RECOMMEND_REQ", "description": "获取推荐好友"},
{"code": 5051, "name": "MSG_ID_FRIEND_RECOMMEND_RESP", "description": "推荐好友列表"},
{"code": 5060, "name": "MSG_ID_FRIEND_BLOCK_REQ", "description": "拉黑玩家"},
{"code": 5061, "name": "MSG_ID_FRIEND_BLOCK_RESP", "description": "拉黑结果"},
{"code": 5062, "name": "MSG_ID_FRIEND_UNBLOCK_REQ", "description": "取消拉黑"},
{"code": 5063, "name": "MSG_ID_FRIEND_UNBLOCK_RESP", "description": "取消拉黑结果"}
```

- [ ] **Step 3: Add Friend Service internal MsgIDs**

Add to `scripts/server/common/include/internal_msg_ids.h`:

```cpp
// Friend Service 消息 ID 范围: 6000-6199
inline constexpr uint32_t MSG_ID_FRIEND_SERVICE_IDENTIFY      = 6001;
inline constexpr uint32_t MSG_ID_FRIEND_SERVICE_IDENTIFY_RESP = 6002;
inline constexpr uint32_t MSG_ID_FRIEND_SERVICE_HEARTBEAT      = 6003;
inline constexpr uint32_t MSG_ID_FRIEND_SERVICE_HEARTBEAT_RESP = 6004;
inline constexpr uint32_t MSG_ID_FRIEND_CLIENT_MSG             = 6011;
inline constexpr uint32_t MSG_ID_FRIEND_SERVICE_MSG            = 6012;
```

- [ ] **Step 4: Regenerate protobuf files**

Run protoc to generate Python and C++ files:

```bash
cd D:/mb_workspace/farm_demo
protoc --python_out=scripts/common/proto/generated/ -I=scripts/common/proto/ friend.proto
protoc --cpp_out=scripts/common/proto/generated/ -I=scripts/common/proto/ friend.proto
```

- [ ] **Step 5: Regenerate message ID constants**

```bash
cd D:/mb_workspace/farm_demo
python tools/generate_constants.py
```

Verify `scripts/client/message_ids.py` contains the new `MSG_ID_FRIEND_*` constants and `scripts/server/common/include/message_ids.h` contains the C++ equivalents.

- [ ] **Step 6: Commit**

```bash
git add scripts/common/proto/friend.proto shared/message_ids.json \
  scripts/common/proto/generated/friend_pb2.py \
  scripts/common/proto/generated/friend.pb.cc \
  scripts/common/proto/generated/friend.pb.h \
  scripts/client/message_ids.py \
  scripts/server/common/include/message_ids.h \
  scripts/server/common/include/internal_msg_ids.h
git commit -m "feat(friend): add protobuf messages and message IDs for friend system"
```

---

## Task 2: Friend Service Skeleton — Constants + Error Codes

**Files:**
- Create: `scripts/server/friend_service/src/friend_constants.h`

- [ ] **Step 1: Create friend_constants.h**

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace farm {

// 好友系统错误码 (与 friend.proto FriendErrorCode 一致)
enum class FriendErrorCode : int32_t {
    SUCCESS = 0,
    ALREADY_FRIENDS = 1,
    FRIEND_LIST_FULL = 2,
    PLAYER_NOT_FOUND = 3,
    REQUEST_NOT_FOUND = 4,
    NOT_FRIENDS = 5,
    CHAT_MSG_TOO_LONG = 6,
    GIFT_ITEM_NOT_FOUND = 7,
    GIFT_COUNT_INVALID = 8,
    VISIT_NOT_AUTHORIZED = 9,
    VISIT_ACTION_FAILED = 10,
    SELF_OPERATION = 11,
    BLOCKED = 12,
    COOLDOWN = 13,
};

// 系统限制
inline constexpr size_t MAX_FRIENDS = 50;
inline constexpr size_t MAX_OFFLINE_MESSAGES = 100;
inline constexpr size_t MAX_CHAT_HISTORY_DAYS = 7;
inline constexpr size_t MAX_CHAT_MSG_LENGTH = 200;
inline constexpr size_t MAX_RECOMMEND_COUNT = 10;
inline constexpr size_t MAX_SEARCH_RESULTS = 20;
inline constexpr size_t FRIEND_REQUEST_EXPIRY_SECONDS = 7 * 24 * 3600;  // 7 days

// 访问操作类型
enum class VisitAction : int32_t {
    WATER = 0,
    WEED = 1,
    HARVEST = 2,
    STEAL = 3,
};

// 聊天消息类型
enum class ChatMsgType : int32_t {
    TEXT = 0,
    EMOTE = 1,
    GIFT = 2,
};

// Redis key 前缀
namespace redis_key {
    inline const std::string FRIEND_SET_PREFIX = "friend:";
    inline const std::string FRIEND_SET_SUFFIX = ":set";
    inline const std::string FRIEND_REQUESTS_PREFIX = "friend:";
    inline const std::string FRIEND_REQUESTS_SUFFIX = ":requests";
    inline const std::string FRIEND_BLOCKED_PREFIX = "friend:";
    inline const std::string FRIEND_BLOCKED_SUFFIX = ":blocked";
    inline const std::string PLAYER_ONLINE_PREFIX = "player:";
    inline const std::string PLAYER_ONLINE_SUFFIX = ":online";
    inline const std::string PLAYER_INFO_PREFIX = "player:";
    inline const std::string PLAYER_INFO_SUFFIX = ":info";
    inline const std::string CHAT_OFFLINE_PREFIX = "chat:offline:";
    inline const std::string FARM_VISIT_AUTH_PREFIX = "farm:visit:auth:";
    inline const std::string FARM_VISIT_ACTIVE_PREFIX = "farm:visit:active:";
    inline const std::string DIRTY_PREFIX = "dirty:friend:";

    inline std::string friend_set(uint64_t pid) { return FRIEND_SET_PREFIX + std::to_string(pid) + FRIEND_SET_SUFFIX; }
    inline std::string friend_requests(uint64_t pid) { return FRIEND_REQUESTS_PREFIX + std::to_string(pid) + FRIEND_REQUESTS_SUFFIX; }
    inline std::string friend_blocked(uint64_t pid) { return FRIEND_BLOCKED_PREFIX + std::to_string(pid) + FRIEND_BLOCKED_SUFFIX; }
    inline std::string player_online(uint64_t pid) { return PLAYER_ONLINE_PREFIX + std::to_string(pid) + PLAYER_ONLINE_SUFFIX; }
    inline std::string player_info(uint64_t pid) { return PLAYER_INFO_PREFIX + std::to_string(pid) + PLAYER_INFO_SUFFIX; }
    inline std::string chat_offline(uint64_t pid) { return CHAT_OFFLINE_PREFIX + std::to_string(pid); }
    inline std::string farm_visit_auth(uint64_t pid) { return FARM_VISIT_AUTH_PREFIX + std::to_string(pid); }
    inline std::string farm_visit_active(uint64_t pid) { return FARM_VISIT_ACTIVE_PREFIX + std::to_string(pid); }
    inline std::string dirty(uint64_t pid) { return DIRTY_PREFIX + std::to_string(pid); }
}  // namespace redis_key

// DBMgr key 前缀
namespace dbmgr_key {
    inline std::string friend_data(uint64_t pid) { return "friend_data:" + std::to_string(pid); }
    inline std::string chat_history(uint64_t pid1, uint64_t pid2) {
        if (pid1 > pid2) std::swap(pid1, pid2);
        return "chat_history:" + std::to_string(pid1) + ":" + std::to_string(pid2);
    }
}  // namespace dbmgr_key

}  // namespace farm
```

- [ ] **Step 2: Commit**

```bash
git add scripts/server/friend_service/src/friend_constants.h
git commit -m "feat(friend): add friend system constants and Redis key helpers"
```

---

## Task 3: Friend Service Skeleton — GameSession + Main

**Files:**
- Create: `scripts/server/friend_service/src/game_session.h`
- Create: `scripts/server/friend_service/src/game_session.cpp`
- Create: `scripts/server/friend_service/src/main.cpp`
- Create: `scripts/server/friend_service/CMakeLists.txt`

- [ ] **Step 1: Create game_session.h**

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

#include <event2/bufferevent.h>
#include <event2/event.h>

namespace farm {

class FriendService;

enum class GameSessionState {
    DISCONNECTED,
    CONNECTED,
    IDENTIFIED,
};

using SendToClientFunc = std::function<void(uint64_t player_id, uint32_t msg_id, const std::string& payload)>;

class GameSession {
public:
    GameSession(FriendService* service, struct event_base* base);
    ~GameSession();

    bool connect(const std::string& host, int port);
    void disconnect();
    bool is_identified() const { return state_ == GameSessionState::IDENTIFIED; }

    // Send a message to Game Server
    void send(uint32_t msg_id, const std::string& payload);

    // Send a client-bound message back through Game Server
    void send_to_client(uint64_t player_id, uint32_t msg_id, const std::string& payload);

    void set_send_to_client_func(SendToClientFunc func) { send_to_client_func_ = std::move(func); }

private:
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);

    void handle_read();
    void handle_event(short events);
    void route_message(uint32_t msg_id, const uint8_t* data, size_t len);

    void send_identify();
    void start_heartbeat();
    void stop_heartbeat();
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);

    FriendService* service_;
    struct event_base* base_;
    struct bufferevent* bev_ = nullptr;
    GameSessionState state_ = GameSessionState::DISCONNECTED;
    std::vector<uint8_t> read_buffer_;
    std::string host_;
    int port_ = 0;

    struct event* heartbeat_timer_ = nullptr;
    time_t last_heartbeat_recv_ = 0;

    // Reconnect
    struct event* reconnect_timer_ = nullptr;
    static void on_reconnect_timer(evutil_socket_t fd, short events, void* ctx);
    void schedule_reconnect();

    SendToClientFunc send_to_client_func_;
};

}  // namespace farm
```

- [ ] **Step 2: Create game_session.cpp**

Follow the exact same pattern as `scripts/server/game_server/src/dbmgr_connection.cpp`:

```cpp
#include "game_session.h"
#include "friend_service.h"
#include "message_parser.h"
#include "internal_msg_ids.h"
#include "log_macros.h"

#include <farm/friend.pb.h>

#include <event2/buffer.h>
#include <cstring>

namespace farm {

GameSession::GameSession(FriendService* service, struct event_base* base)
    : service_(service), base_(base) {}

GameSession::~GameSession() {
    disconnect();
    if (heartbeat_timer_) {
        event_free(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
    if (reconnect_timer_) {
        event_free(reconnect_timer_);
        reconnect_timer_ = nullptr;
    }
}

bool GameSession::connect(const std::string& host, int port) {
    host_ = host;
    port_ = port;

    bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
    if (!bev_) {
        SPDLOG_ERROR("[GameSession]Failed to create bufferevent");
        return false;
    }

    bufferevent_setcb(bev_, on_read, nullptr, on_event, this);
    bufferevent_enable(bev_, EV_READ | EV_WRITE);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    evutil_inet_pton(AF_INET, host.c_str(), &sin.sin_addr);

    if (bufferevent_socket_connect(bev_, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        SPDLOG_ERROR("[GameSession]Failed to connect to Game Server {}:{}", host, port);
        bufferevent_free(bev_);
        bev_ = nullptr;
        return false;
    }

    state_ = GameSessionState::CONNECTED;
    SPDLOG_INFO("[GameSession]Connecting to Game Server {}:{}", host, port);
    return true;
}

void GameSession::disconnect() {
    if (bev_) {
        bufferevent_free(bev_);
        bev_ = nullptr;
    }
    state_ = GameSessionState::DISCONNECTED;
    stop_heartbeat();
}

void GameSession::send(uint32_t msg_id, const std::string& payload) {
    if (!bev_ || state_ == GameSessionState::DISCONNECTED) return;
    auto packed = MessageParser::pack(msg_id, payload);
    bufferevent_write(bev_, packed.data(), packed.size());
}

void GameSession::send_to_client(uint64_t player_id, uint32_t msg_id, const std::string& payload) {
    FriendServiceMessage msg;
    msg.set_player_id(player_id);
    msg.set_msg_id(msg_id);
    msg.set_payload(payload);
    auto serialized = msg.SerializeToString();
    send(MSG_ID_FRIEND_SERVICE_MSG, serialized);
}

void GameSession::on_read(struct bufferevent* bev, void* ctx) {
    auto* session = static_cast<GameSession*>(ctx);
    session->handle_read();
}

void GameSession::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* session = static_cast<GameSession*>(ctx);
    session->handle_event(events);
}

void GameSession::handle_read() {
    struct evbuffer* input = bufferevent_get_input(bev_);
    size_t len = evbuffer_get_length(input);
    if (len == 0) return;

    read_buffer_.resize(read_buffer_.size() + len);
    evbuffer_remove(input, read_buffer_.data() + read_buffer_.size() - len, len);

    while (true) {
        auto result = MessageParser::try_parse(read_buffer_.data(), read_buffer_.size());
        if (!result.has_value()) break;

        auto& msg = result.value();
        route_message(msg.msg_id, msg.payload.data(), msg.payload.size());

        size_t consumed = MessageParser::MSG_HEADER_SIZE + msg.payload.size();
        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + consumed);
    }
}

void GameSession::handle_event(short events) {
    if (events & BEV_EVENT_CONNECTED) {
        SPDLOG_INFO("[GameSession]Connected to Game Server");
        state_ = GameSessionState::CONNECTED;
        send_identify();
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        SPDLOG_WARN("[GameSession]Disconnected from Game Server");
        disconnect();
        schedule_reconnect();
    }
}

void GameSession::route_message(uint32_t msg_id, const uint8_t* data, size_t len) {
    switch (msg_id) {
        case MSG_ID_FRIEND_SERVICE_IDENTIFY_RESP: {
            FriendServiceIdentifyResp resp;
            if (resp.ParseFromArray(data, len) && resp.code() == 0) {
                state_ = GameSessionState::IDENTIFIED;
                SPDLOG_INFO("[GameSession]Identified with Game Server");
                start_heartbeat();
            }
            break;
        }
        case MSG_ID_FRIEND_SERVICE_HEARTBEAT: {
            last_heartbeat_recv_ = time(nullptr);
            FriendServiceHeartbeatResp resp;
            resp.set_timestamp(time(nullptr));
            auto serialized = resp.SerializeToString();
            send(MSG_ID_FRIEND_SERVICE_HEARTBEAT_RESP, serialized);
            break;
        }
        case MSG_ID_FRIEND_CLIENT_MSG: {
            FriendClientMessage msg;
            if (msg.ParseFromArray(data, len)) {
                service_->handle_client_message(msg.player_id(), msg.msg_id(),
                    reinterpret_cast<const uint8_t*>(msg.payload().data()), msg.payload().size());
            }
            break;
        }
        default:
            SPDLOG_WARN("[GameSession]Unknown msg_id={}", msg_id);
            break;
    }
}

void GameSession::send_identify() {
    FriendServiceIdentify identify;
    identify.set_service_id(1);
    identify.set_address("127.0.0.1:9092");
    auto serialized = identify.SerializeToString();
    send(MSG_ID_FRIEND_SERVICE_IDENTIFY, serialized);
}

void GameSession::start_heartbeat() {
    if (heartbeat_timer_) return;
    heartbeat_timer_event = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    struct timeval tv = {5, 0};
    event_add(heartbeat_timer_, &tv);
    last_heartbeat_recv_ = time(nullptr);
}

void GameSession::stop_heartbeat() {
    if (heartbeat_timer_) {
        event_del(heartbeat_timer_);
    }
}

void GameSession::on_heartbeat_timer(evutil_socket_t, short, void* ctx) {
    auto* session = static_cast<GameSession*>(ctx);
    time_t now = time(nullptr);
    if (now - session->last_heartbeat_recv_ > 15) {
        SPDLOG_WARN("[GameSession]Heartbeat timeout, disconnecting");
        session->disconnect();
        session->schedule_reconnect();
        return;
    }
    FriendServiceHeartbeat hb;
    hb.set_timestamp(now);
    auto serialized = hb.SerializeToString();
    session->send(MSG_ID_FRIEND_SERVICE_HEARTBEAT, serialized);
}

void GameSession::schedule_reconnect() {
    if (reconnect_timer_) return;
    reconnect_timer_ = event_new(base_, -1, 0, on_reconnect_timer, this);
    struct timeval tv = {5, 0};
    event_add(reconnect_timer_, &tv);
}

void GameSession::on_reconnect_timer(evutil_socket_t, short, void* ctx) {
    auto* session = static_cast<GameSession*>(ctx);
    if (session->reconnect_timer_) {
        event_free(session->reconnect_timer_);
        session->reconnect_timer_ = nullptr;
    }
    SPDLOG_INFO("[GameSession]Attempting reconnect to Game Server");
    session->connect(session->host_, session->port_);
}

}  // namespace farm
```

- [ ] **Step 3: Create main.cpp**

Follow the pattern from `scripts/server/game_server/src/main.cpp`:

```cpp
#include "friend_service.h"
#include "log_macros.h"
#include "server_main_helper.h"

#include <event2/event.h>
#include <csignal>

static struct event_base* g_base = nullptr;
static farm::FriendService* g_service = nullptr;

static void signal_handler(evutil_socket_t sig, short events, void* ctx) {
    SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
    if (g_service) g_service->stop();
    if (g_base) event_base_loopexit(g_base, nullptr);
}

int main(int argc, char* argv[]) {
    farm::ServerMainHelper::init_logging("friend_service");

    SPDLOG_INFO("[Main]Starting Friend Service...");

    g_base = event_base_new();
    if (!g_base) {
        SPDLOG_ERROR("[Main]Failed to create event base");
        return 1;
    }

    farm::FriendService service(g_base);
    g_service = &service;

    // TODO: read from config file
    std::string redis_uri = "redis://127.0.0.1:6379";
    std::string game_host = "127.0.0.1";
    int game_port = 9090;
    int listen_port = 9092;

    if (!service.start(redis_uri, game_host, game_port, listen_port)) {
        SPDLOG_ERROR("[Main]Failed to start Friend Service");
        return 1;
    }

    // Signal handlers
    struct event* sigint_ev = evsignal_new(g_base, SIGINT, signal_handler, nullptr);
    struct event* sigterm_ev = evsignal_new(g_base, SIGTERM, signal_handler, nullptr);
    event_add(sigint_ev, nullptr);
    event_add(sigterm_ev, nullptr);

    SPDLOG_INFO("[Main]Friend Service running on port {}", listen_port);
    event_base_dispatch(g_base);

    event_free(sigint_ev);
    event_free(sigterm_ev);
    event_base_free(g_base);

    SPDLOG_INFO("[Main]Friend Service stopped");
    return 0;
}
```

- [ ] **Step 4: Create FriendService class header**

Create `scripts/server/friend_service/src/friend_service.h`:

```cpp
#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <cstdint>

#include <event2/event.h>

namespace farm {

class GameSession;
class FriendManager;
class ChatManager;
class GiftManager;
class VisitManager;
class RecommendManager;
class FriendDataManager;
class RedisConnection;

class FriendService {
public:
    FriendService(struct event_base* base);
    ~FriendService();

    bool start(const std::string& redis_uri, const std::string& game_host,
               int game_port, int listen_port);
    void stop();

    // Handle a client message forwarded from Game Server
    void handle_client_message(uint64_t player_id, uint32_t msg_id,
                               const uint8_t* payload, size_t len);

    // Accessors for managers
    FriendManager* friend_mgr() { return friend_mgr_.get(); }
    ChatManager* chat_mgr() { return chat_mgr_.get(); }
    GiftManager* gift_mgr() { return gift_mgr_.get(); }
    VisitManager* visit_mgr() { return visit_mgr_.get(); }
    RecommendManager* recommend_mgr() { return recommend_mgr_.get(); }
    FriendDataManager* data_mgr() { return data_mgr_.get(); }
    RedisConnection* redis() { return redis_.get(); }
    GameSession* game_session() { return game_session_.get(); }

private:
    void register_handlers();

    struct event_base* base_;
    std::unique_ptr<RedisConnection> redis_;
    std::unique_ptr<GameSession> game_session_;
    std::unique_ptr<FriendManager> friend_mgr_;
    std::unique_ptr<ChatManager> chat_mgr_;
    std::unique_ptr<GiftManager> gift_mgr_;
    std::unique_ptr<VisitManager> visit_mgr_;
    std::unique_ptr<RecommendManager> recommend_mgr_;
    std::unique_ptr<FriendDataManager> data_mgr_;

    using MsgHandler = std::function<void(uint64_t, const uint8_t*, size_t)>;
    std::unordered_map<uint32_t, MsgHandler> handlers_;
};

}  // namespace farm
```

- [ ] **Step 5: Create FriendService class implementation**

Create `scripts/server/friend_service/src/friend_service.cpp`:

```cpp
#include "friend_service.h"
#include "game_session.h"
#include "friend_manager.h"
#include "chat_manager.h"
#include "gift_manager.h"
#include "visit_manager.h"
#include "recommend_manager.h"
#include "friend_data_manager.h"
#include "redis_connection.h"
#include "message_ids.h"
#include "log_macros.h"

#include <farm/friend.pb.h>

namespace farm {

FriendService::FriendService(struct event_base* base) : base_(base) {}

FriendService::~FriendService() = default;

bool FriendService::start(const std::string& redis_uri, const std::string& game_host,
                           int game_port, int listen_port) {
    // Redis
    redis_ = std::make_unique<RedisConnection>();
    if (!redis_->connect(redis_uri)) {
        SPDLOG_ERROR("[FriendService]Failed to connect to Redis: {}", redis_uri);
        return false;
    }

    // Data manager
    data_mgr_ = std::make_unique<FriendDataManager>(redis_.get());

    // Business managers
    friend_mgr_ = std::make_unique<FriendManager>(redis_.get(), data_mgr_.get());
    chat_mgr_ = std::make_unique<ChatManager>(redis_.get(), data_mgr_.get());
    gift_mgr_ = std::make_unique<GiftManager>(redis_.get(), data_mgr_.get());
    visit_mgr_ = std::make_unique<VisitManager>(redis_.get(), data_mgr_.get());
    recommend_mgr_ = std::make_unique<RecommendManager>(redis_.get());

    // Game Server connection
    game_session_ = std::make_unique<GameSession>(this, base_);
    if (!game_session_->connect(game_host, game_port)) {
        SPDLOG_ERROR("[FriendService]Failed to connect to Game Server");
        return false;
    }

    register_handlers();
    SPDLOG_INFO("[FriendService]Started successfully");
    return true;
}

void FriendService::stop() {
    if (game_session_) game_session_->disconnect();
    if (redis_) redis_->disconnect();
    SPDLOG_INFO("[FriendService]Stopped");
}

void FriendService::register_handlers() {
    // Friend management
    handlers_[MSG_ID_FRIEND_SEARCH_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        friend_mgr_->handle_search(pid, d, l, game_session_.get());
    };
    handlers_[MSG_ID_FRIEND_ADD_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        friend_mgr_->handle_add(pid, d, l, game_session_.get());
    };
    handlers_[MSG_ID_FRIEND_ACCEPT_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        friend_mgr_->handle_accept(pid, d, l, game_session_.get());
    };
    handlers_[MSG_ID_FRIEND_REJECT_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        friend_mgr_->handle_reject(pid, d, l, game_session_.get());
    };
    handlers_[MSG_ID_FRIEND_DELETE_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        friend_mgr_->handle_delete(pid, d, l, game_session_.get());
    };
    handlers_[MSG_ID_FRIEND_LIST_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        friend_mgr_->handle_list(pid, d, l, game_session_.get());
    };
    handlers_[MSG_ID_FRIEND_BLOCK_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        friend_mgr_->handle_block(pid, d, l, game_session_.get());
    };
    handlers_[MSG_ID_FRIEND_UNBLOCK_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        friend_mgr_->handle_unblock(pid, d, l, game_session_.get());
    };

    // Chat
    handlers_[MSG_ID_FRIEND_CHAT_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        chat_mgr_->handle_send(pid, d, l, game_session_.get());
    };
    handlers_[MSG_ID_FRIEND_CHAT_HISTORY_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        chat_mgr_->handle_history(pid, d, l, game_session_.get());
    };

    // Gift
    handlers_[MSG_ID_FRIEND_GIFT_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        gift_mgr_->handle_send(pid, d, l, game_session_.get());
    };

    // Visit
    handlers_[MSG_ID_FRIEND_VISIT_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        visit_mgr_->handle_visit(pid, d, l, game_session_.get());
    };
    handlers_[MSG_ID_FRIEND_VISIT_ACTION_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        visit_mgr_->handle_action(pid, d, l, game_session_.get());
    };

    // Recommend
    handlers_[MSG_ID_FRIEND_RECOMMEND_REQ] = [this](uint64_t pid, const uint8_t* d, size_t l) {
        recommend_mgr_->handle_recommend(pid, d, l, game_session_.get());
    };
}

void FriendService::handle_client_message(uint64_t player_id, uint32_t msg_id,
                                            const uint8_t* payload, size_t len) {
    auto it = handlers_.find(msg_id);
    if (it != handlers_.end()) {
        it->second(player_id, payload, len);
    } else {
        SPDLOG_WARN("[FriendService]Unhandled msg_id={}", msg_id);
    }
}

}  // namespace farm
```

- [ ] **Step 6: Create CMakeLists.txt**

Create `scripts/server/friend_service/CMakeLists.txt` following the game_server pattern:

```cmake
cmake_minimum_required(VERSION 3.14)
project(friend_service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC static runtime
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(COMMON_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/proto")
set(COMMON_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/src")
set(COMMON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/include")
set(SHARED_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/include")

set(PROTO_SRCS
    ${COMMON_PROTO_DIR}/generated/friend.pb.cc
    ${COMMON_PROTO_DIR}/generated/base.pb.cc
)

set(PROTO_HDRS
    ${COMMON_PROTO_DIR}/generated/friend.pb.h
    ${COMMON_PROTO_DIR}/generated/base.pb.h
)

set(SOURCES
    src/main.cpp
    src/friend_service.cpp
    src/game_session.cpp
    src/friend_manager.cpp
    src/chat_manager.cpp
    src/gift_manager.cpp
    src/visit_manager.cpp
    src/recommend_manager.cpp
    src/friend_data_manager.cpp
    ${COMMON_SRC_DIR}/log_init.cpp
    ${COMMON_SRC_DIR}/message_parser.cpp
    ${COMMON_SRC_DIR}/msvc_compat.cpp
    ${COMMON_SRC_DIR}/server_main_helper.cpp
)

set(HEADERS
    src/friend_service.h
    src/game_session.h
    src/friend_manager.h
    src/chat_manager.h
    src/gift_manager.h
    src/visit_manager.h
    src/recommend_manager.h
    src/friend_data_manager.h
    src/friend_constants.h
)

# Dependencies
set(LIBEVENT_ROOT "C:/libevent_install")
set(HIREDIS_ROOT "C:/hiredis_install")
set(PROTOBUF_ROOT "C:/protobuf_install")
set(SPDLOG_ROOT "C:/spdlog_install")

add_executable(friend_service ${SOURCES} ${HEADERS} ${PROTO_SRCS} ${PROTO_HDRS})

target_include_directories(friend_service PRIVATE
    src/
    ${COMMON_PROTO_DIR}
    ${COMMON_PROTO_DIR}/generated
    ${COMMON_INCLUDE_DIR}
    ${SHARED_INCLUDE_DIR}
    ${LIBEVENT_ROOT}/include
    ${HIREDIS_ROOT}/include
    ${PROTOBUF_ROOT}/include
    ${SPDLOG_ROOT}/include
)

if(MSVC)
    file(GLOB ABSL_LIBS "${PROTOBUF_ROOT}/lib/absl*.lib")
    target_link_libraries(friend_service PRIVATE
        ${PROTOBUF_ROOT}/lib/libprotobuf-lite.lib
        ${ABSL_LIBS}
        ${LIBEVENT_ROOT}/lib/event.lib
        ${LIBEVENT_ROOT}/lib/event_core.lib
        ${LIBEVENT_ROOT}/lib/event_extra.lib
        ${HIREDIS_ROOT}/lib/hiredis.lib
        ws2_32
        advapi32
        shell32
    )
endif()

set_target_properties(friend_service PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
)

# Tests
set(TEST_SOURCES
    tests/test_friend_manager.cpp
    src/friend_manager.cpp
    src/friend_data_manager.cpp
    ${COMMON_SRC_DIR}/log_init.cpp
    ${COMMON_SRC_DIR}/message_parser.cpp
    ${COMMON_SRC_DIR}/msvc_compat.cpp
    ${PROTO_SRCS}
)

add_executable(friend_tests ${TEST_SOURCES} ${HEADERS} ${PROTO_HDRS})

target_include_directories(friend_tests PRIVATE
    src/
    ${COMMON_PROTO_DIR}
    ${COMMON_PROTO_DIR}/generated
    ${COMMON_INCLUDE_DIR}
    ${SHARED_INCLUDE_DIR}
    ${LIBEVENT_ROOT}/include
    ${HIREDIS_ROOT}/include
    ${PROTOBUF_ROOT}/include
    ${SPDLOG_ROOT}/include
)

if(MSVC)
    target_link_libraries(friend_tests PRIVATE
        ${PROTOBUF_ROOT}/lib/libprotobuf-lite.lib
        ${ABSL_LIBS}
        ${LIBEVENT_ROOT}/lib/event.lib
        ${LIBEVENT_ROOT}/lib/event_core.lib
        ${LIBEVENT_ROOT}/lib/event_extra.lib
        ${HIREDIS_ROOT}/lib/hiredis.lib
        ws2_32
        advapi32
        shell32
    )
endif()
```

- [ ] **Step 7: Commit**

```bash
git add scripts/server/friend_service/
git commit -m "feat(friend): add Friend Service skeleton with GameSession and message routing"
```

---

## Task 4: FriendManager — Core Friend CRUD

**Files:**
- Create: `scripts/server/friend_service/src/friend_manager.h`
- Create: `scripts/server/friend_service/src/friend_manager.cpp`

- [ ] **Step 1: Create friend_manager.h**

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace farm {

class RedisConnection;
class FriendDataManager;
class GameSession;

struct FriendInfoData {
    uint64_t player_id = 0;
    std::string role_name;
    uint32_t level = 0;
    bool online = false;
    std::string scene_id;
    uint64_t last_login = 0;
};

struct FriendRequestData {
    uint64_t sender_id = 0;
    std::string sender_name;
    uint32_t sender_level = 0;
    uint64_t timestamp = 0;
};

class FriendManager {
public:
    FriendManager(RedisConnection* redis, FriendDataManager* data_mgr);

    void handle_search(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
    void handle_add(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
    void handle_accept(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
    void handle_reject(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
    void handle_delete(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
    void handle_list(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
    void handle_block(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
    void handle_unblock(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);

    // Online status management
    void set_player_online(uint64_t player_id, const std::string& role_name,
                           uint32_t level, const std::string& scene_id);
    void set_player_offline(uint64_t player_id);

    // Check relationships
    bool is_friend(uint64_t player_id, uint64_t target_id);
    bool is_blocked(uint64_t player_id, uint64_t target_id);
    size_t get_friend_count(uint64_t player_id);

private:
    void send_resp(GameSession* session, uint64_t player_id, uint32_t msg_id, const std::string& payload);
    void notify_friends_online(uint64_t player_id, const std::string& role_name, const std::string& scene_id);
    void notify_friends_offline(uint64_t player_id);

    RedisConnection* redis_;
    FriendDataManager* data_mgr_;
};

}  // namespace farm
```

- [ ] **Step 2: Create friend_manager.cpp**

```cpp
#include "friend_manager.h"
#include "friend_constants.h"
#include "redis_connection.h"
#include "friend_data_manager.h"
#include "game_session.h"
#include "message_ids.h"
#include "log_macros.h"

#include <farm/friend.pb.h>
#include <ctime>
#include <algorithm>

namespace farm {

FriendManager::FriendManager(RedisConnection* redis, FriendDataManager* data_mgr)
    : redis_(redis), data_mgr_(data_mgr) {}

void FriendManager::handle_search(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session) {
    FriendSearchReq req;
    if (!req.ParseFromArray(data, len)) return;

    FriendSearchResp resp;
    resp.set_code(0);

    // Search by player_id (if keyword is numeric) or role_name
    // For now, search in Redis player_info keys
    std::string keyword = req.keyword();
    // TODO: implement full search via DBMgr name index
    // For MVP, try parsing as player_id
    try {
        uint64_t target_id = std::stoull(keyword);
        std::string info_str = redis_->get(redis_key::player_info(target_id));
        if (!info_str.empty()) {
            auto* info = resp.add_results();
            info->set_player_id(target_id);
            // Parse info JSON for role_name, level, etc.
        }
    } catch (...) {}

    auto serialized = resp.SerializeToString();
    send_resp(session, player_id, MSG_ID_FRIEND_SEARCH_RESP, serialized);
}

void FriendManager::handle_add(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session) {
    FriendAddReq req;
    if (!req.ParseFromArray(data, len)) return;

    FriendAddResp resp;
    uint64_t target_id = req.target_id();

    // Self-check
    if (player_id == target_id) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::SELF_OPERATION));
        auto s = resp.SerializeToString();
        send_resp(session, player_id, MSG_ID_FRIEND_ADD_RESP, s);
        return;
    }

    // Already friends?
    if (is_friend(player_id, target_id)) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::ALREADY_FRIENDS));
        auto s = resp.SerializeToString();
        send_resp(session, player_id, MSG_ID_FRIEND_ADD_RESP, s);
        return;
    }

    // Blocked?
    if (is_blocked(target_id, player_id)) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::BLOCKED));
        auto s = resp.SerializeToString();
        send_resp(session, player_id, MSG_ID_FRIEND_ADD_RESP, s);
        return;
    }

    // Friend list full?
    if (get_friend_count(player_id) >= MAX_FRIENDS) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::FRIEND_LIST_FULL));
        auto s = resp.SerializeToString();
        send_resp(session, player_id, MSG_ID_FRIEND_ADD_RESP, s);
        return;
    }

    // Store request in Redis hash
    std::string req_key = redis_key::friend_requests(target_id);
    redis_->hset(req_key, std::to_string(player_id), std::to_string(time(nullptr)));

    // Mark dirty for persistence
    redis_->set(redis_key::dirty(target_id), "1");

    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));
    auto s = resp.SerializeToString();
    send_resp(session, player_id, MSG_ID_FRIEND_ADD_RESP, s);

    // Notify target player if online
    // Get requester info for notification
    std::string info_str = redis_->get(redis_key::player_info(player_id));
    FriendAddNotify notify;
    auto* req_info = notify.mutable_request();
    req_info->set_sender_id(player_id);
    // TODO: parse info_str for role_name and level
    auto notify_serialized = notify.SerializeToString();
    send_resp(session, target_id, MSG_ID_FRIEND_ADD_NOTIFY, notify_serialized);
}

void FriendManager::handle_accept(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session) {
    FriendAcceptReq req;
    if (!req.ParseFromArray(data, len)) return;

    FriendAcceptResp resp;
    uint64_t sender_id = req.sender_id();

    // Check request exists
    std::string req_key = redis_key::friend_requests(player_id);
    std::string timestamp = redis_->hget(req_key, std::to_string(sender_id));
    if (timestamp.empty()) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::REQUEST_NOT_FOUND));
        auto s = resp.SerializeToString();
        send_resp(session, player_id, MSG_ID_FRIEND_ACCEPT_RESP, s);
        return;
    }

    // Check friend list full
    if (get_friend_count(player_id) >= MAX_FRIENDS) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::FRIEND_LIST_FULL));
        auto s = resp.SerializeToString();
        send_resp(session, player_id, MSG_ID_FRIEND_ACCEPT_RESP, s);
        return;
    }

    // Add to both friend sets
    redis_->sadd(redis_key::friend_set(player_id), std::to_string(sender_id));
    redis_->sadd(redis_key::friend_set(sender_id), std::to_string(player_id));

    // Remove request
    redis_->hdel(req_key, std::to_string(sender_id));

    // Mark both dirty
    redis_->set(redis_key::dirty(player_id), "1");
    redis_->set(redis_key::dirty(sender_id), "1");

    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));
    // TODO: populate new_friend info
    auto s = resp.SerializeToString();
    send_resp(session, player_id, MSG_ID_FRIEND_ACCEPT_RESP, s);

    // Notify sender
    // TODO: send FRIEND_ONLINE_NOTIFY to sender
}

void FriendManager::handle_reject(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session) {
    FriendRejectReq req;
    if (!req.ParseFromArray(data, len)) return;

    FriendRejectResp resp;
    std::string req_key = redis_key::friend_requests(player_id);
    redis_->hdel(req_key, std::to_string(req.sender_id()));

    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));
    auto s = resp.SerializeToString();
    send_resp(session, player_id, MSG_ID_FRIEND_REJECT_RESP, s);
}

void FriendManager::handle_delete(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session) {
    FriendDeleteReq req;
    if (!req.ParseFromArray(data, len)) return;

    FriendDeleteResp resp;
    uint64_t target_id = req.target_id();

    if (!is_friend(player_id, target_id)) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::NOT_FRIENDS));
        auto s = resp.SerializeToString();
        send_resp(session, player_id, MSG_ID_FRIEND_DELETE_RESP, s);
        return;
    }

    // Remove from both sets
    redis_->srem(redis_key::friend_set(player_id), std::to_string(target_id));
    redis_->srem(redis_key::friend_set(target_id), std::to_string(player_id));

    redis_->set(redis_key::dirty(player_id), "1");
    redis_->set(redis_key::dirty(target_id), "1");

    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));
    auto s = resp.SerializeToString();
    send_resp(session, player_id, MSG_ID_FRIEND_DELETE_RESP, s);
}

void FriendManager::handle_list(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session) {
    FriendListResp resp;
    resp.set_code(0);

    // Get friend IDs from Redis set
    auto friend_ids = redis_->smembers(redis_key::friend_set(player_id));
    for (const auto& fid_str : friend_ids) {
        uint64_t fid = std::stoull(fid_str);
        auto* info = resp.add_friends();
        info->set_player_id(fid);
        // Populate online status and info from Redis
        std::string online = redis_->get(redis_key::player_online(fid));
        info->set_online(online == "1");
        std::string info_str = redis_->get(redis_key::player_info(fid));
        // TODO: parse info_str for role_name, level, scene_id
    }

    // Get pending requests
    auto requests = redis_->hgetall(redis_key::friend_requests(player_id));
    for (const auto& [sender_str, ts_str] : requests) {
        auto* req_info = resp.add_pending_requests();
        req_info->set_sender_id(std::stoull(sender_str));
        req_info->set_timestamp(std::stoull(ts_str));
        // TODO: populate sender_name, sender_level
    }

    auto s = resp.SerializeToString();
    send_resp(session, player_id, MSG_ID_FRIEND_LIST_RESP, s);
}

void FriendManager::handle_block(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session) {
    FriendBlockReq req;
    if (!req.ParseFromArray(data, len)) return;

    FriendBlockResp resp;
    uint64_t target_id = req.target_id();

    if (player_id == target_id) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::SELF_OPERATION));
        auto s = resp.SerializeToString();
        send_resp(session, player_id, MSG_ID_FRIEND_BLOCK_RESP, s);
        return;
    }

    // Add to blocked set
    redis_->sadd(redis_key::friend_blocked(player_id), std::to_string(target_id));

    // Remove from friends if present
    redis_->srem(redis_key::friend_set(player_id), std::to_string(target_id));
    redis_->srem(redis_key::friend_set(target_id), std::to_string(player_id));

    redis_->set(redis_key::dirty(player_id), "1");

    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));
    auto s = resp.SerializeToString();
    send_resp(session, player_id, MSG_ID_FRIEND_BLOCK_RESP, s);
}

void FriendManager::handle_unblock(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session) {
    FriendUnblockReq req;
    if (!req.ParseFromArray(data, len)) return;

    FriendUnblockResp resp;
    redis_->srem(redis_key::friend_blocked(player_id), std::to_string(req.target_id()));
    redis_->set(redis_key::dirty(player_id), "1");

    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));
    auto s = resp.SerializeToString();
    send_resp(session, player_id, MSG_ID_FRIEND_UNBLOCK_RESP, s);
}

void FriendManager::set_player_online(uint64_t player_id, const std::string& role_name,
                                       uint32_t level, const std::string& scene_id) {
    redis_->set(redis_key::player_online(player_id), "1");
    // Store player info as JSON
    // TODO: use proper JSON serialization
    std::string info = role_name + "|" + std::to_string(level) + "|" + scene_id;
    redis_->set(redis_key::player_info(player_id), info);
    notify_friends_online(player_id, role_name, scene_id);
}

void FriendManager::set_player_offline(uint64_t player_id) {
    redis_->set(redis_key::player_online(player_id), "0");
    notify_friends_offline(player_id);
}

bool FriendManager::is_friend(uint64_t player_id, uint64_t target_id) {
    return redis_->sismember(redis_key::friend_set(player_id), std::to_string(target_id));
}

bool FriendManager::is_blocked(uint64_t player_id, uint64_t target_id) {
    return redis_->sismember(redis_key::friend_blocked(player_id), std::to_string(target_id));
}

size_t FriendManager::get_friend_count(uint64_t player_id) {
    return redis_->scard(redis_key::friend_set(player_id));
}

void FriendManager::send_resp(GameSession* session, uint64_t player_id, uint32_t msg_id, const std::string& payload) {
    if (session) session->send_to_client(player_id, msg_id, payload);
}

void FriendManager::notify_friends_online(uint64_t player_id, const std::string& role_name, const std::string& scene_id) {
    auto friend_ids = redis_->smembers(redis_key::friend_set(player_id));
    FriendOnlineNotify notify;
    notify.set_player_id(player_id);
    notify.set_role_name(role_name);
    notify.set_scene_id(scene_id);
    auto s = notify.SerializeToString();
    for (const auto& fid_str : friend_ids) {
        uint64_t fid = std::stoull(fid_str);
        if (redis_->get(redis_key::player_online(fid)) == "1") {
            send_resp(game_session_, fid, MSG_ID_FRIEND_ONLINE_NOTIFY, s);
        }
    }
}

void FriendManager::notify_friends_offline(uint64_t player_id) {
    auto friend_ids = redis_->smembers(redis_key::friend_set(player_id));
    FriendOfflineNotify notify;
    notify.set_player_id(player_id);
    auto s = notify.SerializeToString();
    for (const auto& fid_str : friend_ids) {
        uint64_t fid = std::stoull(fid_str);
        if (redis_->get(redis_key::player_online(fid)) == "1") {
            send_resp(game_session_, fid, MSG_ID_FRIEND_OFFLINE_NOTIFY, s);
        }
    }
}

}  // namespace farm
```

**Note:** The `notify_friends_online/offline` methods need access to `game_session_` — pass it through the FriendService or store a pointer. Adjust the class to hold a `GameSession*` set during `FriendService::start()`.

- [ ] **Step 3: Commit**

```bash
git add scripts/server/friend_service/src/friend_manager.h scripts/server/friend_service/src/friend_manager.cpp
git commit -m "feat(friend): add FriendManager with CRUD, block, online status"
```

---

## Task 5: ChatManager + GiftManager + VisitManager + RecommendManager

**Files:**
- Create: `scripts/server/friend_service/src/chat_manager.h/cpp`
- Create: `scripts/server/friend_service/src/gift_manager.h/cpp`
- Create: `scripts/server/friend_service/src/visit_manager.h/cpp`
- Create: `scripts/server/friend_service/src/recommend_manager.h/cpp`
- Create: `scripts/server/friend_service/src/friend_data_manager.h/cpp`

- [ ] **Step 1: Create chat_manager.h/cpp**

`chat_manager.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace farm {
class RedisConnection;
class FriendDataManager;
class GameSession;

class ChatManager {
public:
    ChatManager(RedisConnection* redis, FriendDataManager* data_mgr);
    void handle_send(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
    void handle_history(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
    void deliver_offline_messages(uint64_t player_id, GameSession* session);
private:
    RedisConnection* redis_;
    FriendDataManager* data_mgr_;
};
}
```

`chat_manager.cpp` — implement `handle_send`:
1. Parse `FriendChatReq`
2. Check `msg_content.length() <= MAX_CHAT_MSG_LENGTH`
3. Check is_friend(player_id, receiver_id) via redis SISMEMBER
4. Check not blocked by receiver
5. Build `FriendChatNotify` with sender info + timestamp
6. If receiver online (`redis_->get(player_online(receiver_id)) == "1"`), send via `session->send_to_client(receiver_id, MSG_ID_FRIEND_CHAT_NOTIFY, ...)`
7. If offline, `redis_->lpush(chat_offline(receiver_id), serialized_msg)` — cap at MAX_OFFLINE_MESSAGES via LTRIM
8. Store in chat history for DBMgr persistence (mark dirty)
9. Send `FriendChatResp` code=0 back to sender

- [ ] **Step 2: Create gift_manager.h/cpp**

`gift_manager.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace farm {
class RedisConnection;
class FriendDataManager;
class GameSession;

class GiftManager {
public:
    GiftManager(RedisConnection* redis, FriendDataManager* data_mgr);
    void handle_send(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
private:
    RedisConnection* redis_;
    FriendDataManager* data_mgr_;
};
}
```

`gift_manager.cpp` — implement `handle_send`:
1. Parse `FriendGiftReq`
2. Validate count > 0
3. Check is_friend
4. Send a request to Game Server to deduct items from sender and add to receiver (requires Game Server cooperation — for MVP, just notify)
5. Build `FriendGiftNotify` and send to receiver
6. Send `FriendGiftResp` code=0 to sender

- [ ] **Step 3: Create visit_manager.h/cpp**

`visit_manager.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace farm {
class RedisConnection;
class FriendDataManager;
class GameSession;

class VisitManager {
public:
    VisitManager(RedisConnection* redis, FriendDataManager* data_mgr);
    void handle_visit(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
    void handle_action(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
private:
    RedisConnection* redis_;
    FriendDataManager* data_mgr_;
};
}
```

`visit_manager.cpp` — implement `handle_visit`:
1. Parse `FriendVisitReq`
2. Check is_friend
3. Add player_id to `farm_visit_active(owner_id)` Redis set
4. Send `FriendVisitResp` with owner info
5. For `handle_action`: parse action_type, verify player is in active visitors, execute action (water/weed/steal), send response

- [ ] **Step 4: Create recommend_manager.h/cpp**

`recommend_manager.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace farm {
class RedisConnection;
class GameSession;

class RecommendManager {
public:
    explicit RecommendManager(RedisConnection* redis);
    void handle_recommend(uint64_t player_id, const uint8_t* data, size_t len, GameSession* session);
private:
    RedisConnection* redis_;
};
}
```

`recommend_manager.cpp` — implement `handle_recommend`:
1. Get player's friend set
2. For each friend, get their friend set
3. Count common friends (appears in multiple friend-of-friend sets)
4. Filter out existing friends, blocked, self
5. Sort by common friend count, take top MAX_RECOMMEND_COUNT
6. Build `FriendRecommendResp`

- [ ] **Step 5: Create friend_data_manager.h/cpp**

`friend_data_manager.h`:
```cpp
#pragma once
#include <cstdint>
#include <string>

namespace farm {
class RedisConnection;

class FriendDataManager {
public:
    explicit FriendDataManager(RedisConnection* redis);
    void persist_dirty();
    void load_from_dbmgr(uint64_t player_id);
private:
    RedisConnection* redis_;
};
}
```

`friend_data_manager.cpp` — implement `persist_dirty`:
1. Scan Redis for `dirty:friend:*` keys
2. For each dirty player, collect friend set, blocked set, settings
3. Serialize to JSON
4. TODO: send to DBMgr via connection (for now, log)
5. Clear dirty flag

- [ ] **Step 6: Commit**

```bash
git add scripts/server/friend_service/src/chat_manager.h scripts/server/friend_service/src/chat_manager.cpp \
  scripts/server/friend_service/src/gift_manager.h scripts/server/friend_service/src/gift_manager.cpp \
  scripts/server/friend_service/src/visit_manager.h scripts/server/friend_service/src/visit_manager.cpp \
  scripts/server/friend_service/src/recommend_manager.h scripts/server/friend_service/src/recommend_manager.cpp \
  scripts/server/friend_service/src/friend_data_manager.h scripts/server/friend_service/src/friend_data_manager.cpp
git commit -m "feat(friend): add ChatManager, GiftManager, VisitManager, RecommendManager, FriendDataManager"
```

---

## Task 6: Game Server — Friend Service Connection + Message Forwarding

**Files:**
- Create: `scripts/server/game_server/src/friend_service_connection.h`
- Create: `scripts/server/game_server/src/friend_service_connection.cpp`
- Modify: `scripts/server/game_server/src/game_server.h`
- Modify: `scripts/server/game_server/src/game_server.cpp`
- Modify: `scripts/server/game_server/CMakeLists.txt`

- [ ] **Step 1: Create friend_service_connection.h**

Follow the `DBMgrConnection` + `DBMgrConnectionManager` pattern but simplified (single connection):

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#include <event2/bufferevent.h>
#include <event2/event.h>

namespace farm {

enum class FriendServiceState {
    DISCONNECTED,
    CONNECTED,
    IDENTIFIED,
};

using FriendMsgCallback = std::function<void(uint64_t player_id, uint32_t msg_id, const std::string& payload)>;

class FriendServiceConnection {
public:
    FriendServiceConnection(struct event_base* base);
    ~FriendServiceConnection();

    bool connect(const std::string& host, int port);
    void disconnect();
    bool is_identified() const { return state_ == FriendServiceState::IDENTIFIED; }

    void send_client_msg(uint64_t player_id, uint32_t msg_id, const std::string& payload);

    void set_on_message(FriendMsgCallback cb) { on_message_ = std::move(cb); }

private:
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    void handle_read();
    void handle_event(short events);
    void route_message(uint32_t msg_id, const uint8_t* data, size_t len);

    void send_identify();
    void start_heartbeat();
    void stop_heartbeat();
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);
    void schedule_reconnect();
    static void on_reconnect_timer(evutil_socket_t fd, short events, void* ctx);

    struct event_base* base_;
    struct bufferevent* bev_ = nullptr;
    FriendServiceState state_ = FriendServiceState::DISCONNECTED;
    std::vector<uint8_t> read_buffer_;
    std::string host_;
    int port_ = 0;

    struct event* heartbeat_timer_ = nullptr;
    struct event* reconnect_timer_ = nullptr;
    time_t last_heartbeat_recv_ = 0;

    FriendMsgCallback on_message_;
};

}  // namespace farm
```

- [ ] **Step 2: Create friend_service_connection.cpp**

Follow the exact same pattern as `dbmgr_connection.cpp` but for Friend Service protocol:

```cpp
#include "friend_service_connection.h"
#include "message_parser.h"
#include "internal_msg_ids.h"
#include "log_macros.h"

#include <farm/friend.pb.h>
#include <farm/base.pb.h>
#include <event2/buffer.h>
#include <cstring>

namespace farm {

FriendServiceConnection::FriendServiceConnection(struct event_base* base) : base_(base) {}

FriendServiceConnection::~FriendServiceConnection() { disconnect(); }

bool FriendServiceConnection::connect(const std::string& host, int port) {
    host_ = host; port_ = port;
    bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
    if (!bev_) return false;
    bufferevent_setcb(bev_, on_read, nullptr, on_event, this);
    bufferevent_enable(bev_, EV_READ | EV_WRITE);
    struct sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    evutil_inet_pton(AF_INET, host.c_str(), &sin.sin_addr);
    if (bufferevent_socket_connect(bev_, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        bufferevent_free(bev_); bev_ = nullptr; return false;
    }
    state_ = FriendServiceState::CONNECTED;
    return true;
}

void FriendServiceConnection::disconnect() {
    if (bev_) { bufferevent_free(bev_); bev_ = nullptr; }
    state_ = FriendServiceState::DISCONNECTED;
    stop_heartbeat();
}

void FriendServiceConnection::send_client_msg(uint64_t player_id, uint32_t msg_id, const std::string& payload) {
    if (!is_identified()) return;
    FriendClientMessage msg;
    msg.set_player_id(player_id);
    msg.set_msg_id(msg_id);
    msg.set_payload(payload);
    auto serialized = msg.SerializeToString();
    auto packed = MessageParser::pack(MSG_ID_FRIEND_CLIENT_MSG, serialized);
    bufferevent_write(bev_, packed.data(), packed.size());
}

void FriendServiceConnection::on_read(struct bufferevent* bev, void* ctx) {
    static_cast<FriendServiceConnection*>(ctx)->handle_read();
}

void FriendServiceConnection::on_event(struct bufferevent* bev, short events, void* ctx) {
    static_cast<FriendServiceConnection*>(ctx)->handle_event(events);
}

void FriendServiceConnection::handle_read() {
    struct evbuffer* input = bufferevent_get_input(bev_);
    size_t len = evbuffer_get_length(input);
    if (len == 0) return;
    read_buffer_.resize(read_buffer_.size() + len);
    evbuffer_remove(input, read_buffer_.data() + read_buffer_.size() - len, len);
    while (true) {
        auto result = MessageParser::try_parse(read_buffer_.data(), read_buffer_.size());
        if (!result.has_value()) break;
        auto& msg = result.value();
        route_message(msg.msg_id, msg.payload.data(), msg.payload.size());
        size_t consumed = MessageParser::MSG_HEADER_SIZE + msg.payload.size();
        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + consumed);
    }
}

void FriendServiceConnection::handle_event(short events) {
    if (events & BEV_EVENT_CONNECTED) {
        state_ = FriendServiceState::CONNECTED;
        send_identify();
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        disconnect();
        schedule_reconnect();
    }
}

void FriendServiceConnection::route_message(uint32_t msg_id, const uint8_t* data, size_t len) {
    switch (msg_id) {
        case MSG_ID_FRIEND_SERVICE_IDENTIFY_RESP: {
            FriendServiceIdentifyResp resp;
            if (resp.ParseFromArray(data, len) && resp.code() == 0) {
                state_ = FriendServiceState::IDENTIFIED;
                start_heartbeat();
            }
            break;
        }
        case MSG_ID_FRIEND_SERVICE_HEARTBEAT: {
            last_heartbeat_recv_ = time(nullptr);
            FriendServiceHeartbeatResp resp;
            resp.set_timestamp(time(nullptr));
            auto s = resp.SerializeToString();
            auto packed = MessageParser::pack(MSG_ID_FRIEND_SERVICE_HEARTBEAT_RESP, s);
            bufferevent_write(bev_, packed.data(), packed.size());
            break;
        }
        case MSG_ID_FRIEND_SERVICE_MSG: {
            FriendServiceMessage msg;
            if (msg.ParseFromArray(data, len) && on_message_) {
                on_message_(msg.player_id(), msg.msg_id(), msg.payload());
            }
            break;
        }
    }
}

void FriendServiceConnection::send_identify() {
    FriendServiceIdentify id;
    id.set_service_id(0);
    auto s = id.SerializeToString();
    auto packed = MessageParser::pack(MSG_ID_FRIEND_SERVICE_IDENTIFY, s);
    bufferevent_write(bev_, packed.data(), packed.size());
}

// Heartbeat and reconnect follow the same pattern as DBMgrConnection
// (5s interval, 15s timeout, 5s reconnect delay)

void FriendServiceConnection::start_heartbeat() {
    if (heartbeat_timer_) return;
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    struct timeval tv = {5, 0};
    event_add(heartbeat_timer_, &tv);
    last_heartbeat_recv_ = time(nullptr);
}

void FriendServiceConnection::stop_heartbeat() {
    if (heartbeat_timer_) { event_del(heartbeat_timer_); event_free(heartbeat_timer_); heartbeat_timer_ = nullptr; }
}

void FriendServiceConnection::on_heartbeat_timer(evutil_socket_t, short, void* ctx) {
    auto* conn = static_cast<FriendServiceConnection*>(ctx);
    if (time(nullptr) - conn->last_heartbeat_recv_ > 15) {
        conn->disconnect(); conn->schedule_reconnect(); return;
    }
    FriendServiceHeartbeat hb;
    hb.set_timestamp(time(nullptr));
    auto s = hb.SerializeToString();
    auto packed = MessageParser::pack(MSG_ID_FRIEND_SERVICE_HEARTBEAT, s);
    bufferevent_write(conn->bev_, packed.data(), packed.size());
}

void FriendServiceConnection::schedule_reconnect() {
    if (reconnect_timer_) return;
    reconnect_timer_ = event_new(base_, -1, 0, on_reconnect_timer, this);
    struct timeval tv = {5, 0};
    event_add(reconnect_timer_, &tv);
}

void FriendServiceConnection::on_reconnect_timer(evutil_socket_t, short, void* ctx) {
    auto* conn = static_cast<FriendServiceConnection*>(ctx);
    event_free(conn->reconnect_timer_); conn->reconnect_timer_ = nullptr;
    conn->connect(conn->host_, conn->port_);
}

}  // namespace farm
```

- [ ] **Step 3: Modify game_server.h — add FriendServiceConnection member**

Add to `GameServer` class:
```cpp
#include "friend_service_connection.h"

// In class GameServer:
private:
    std::unique_ptr<FriendServiceConnection> friend_conn_;
```

- [ ] **Step 4: Modify game_server.cpp — register friend message forwarding**

In `GameServer::start()`, after existing handler registrations:
```cpp
// Friend Service connection
friend_conn_ = std::make_unique<FriendServiceConnection>(base_);
friend_conn_->set_on_message([this](uint64_t player_id, uint32_t msg_id, const std::string& payload) {
    // Forward Friend Service response back to client via Gate
    send_game_msg_to_gate(player_id, msg_id, payload);
});
friend_conn_->connect("127.0.0.1", 9092);  // TODO: config

// Register friend message forwarding handlers
auto forward_to_friend = [this](uint64_t player_id, const uint8_t* payload, size_t len) {
    if (friend_conn_ && friend_conn_->is_identified()) {
        // The payload is the inner protobuf, need to wrap with player_id
        std::string payload_str(reinterpret_cast<const char*>(payload), len);
        // We need the msg_id to forward — get it from the handler registration
        // Actually, we need to pass msg_id separately. Use a lambda per msg_id.
    }
};

// Register each friend MsgID as a forward-to-friend-service handler
std::vector<uint32_t> friend_msg_ids = {
    MSG_ID_FRIEND_SEARCH_REQ, MSG_ID_FRIEND_ADD_REQ, MSG_ID_FRIEND_ACCEPT_REQ,
    MSG_ID_FRIEND_REJECT_REQ, MSG_ID_FRIEND_DELETE_REQ, MSG_ID_FRIEND_LIST_REQ,
    MSG_ID_FRIEND_BLOCK_REQ, MSG_ID_FRIEND_UNBLOCK_REQ,
    MSG_ID_FRIEND_CHAT_REQ, MSG_ID_FRIEND_CHAT_HISTORY_REQ,
    MSG_ID_FRIEND_GIFT_REQ,
    MSG_ID_FRIEND_VISIT_REQ, MSG_ID_FRIEND_VISIT_ACTION_REQ,
    MSG_ID_FRIEND_RECOMMEND_REQ,
};
for (uint32_t mid : friend_msg_ids) {
    msg_handler_.register_handler(mid, [this, mid](uint64_t player_id, const uint8_t* payload, size_t len) {
        std::string payload_str(reinterpret_cast<const char*>(payload), len);
        friend_conn_->send_client_msg(player_id, mid, payload_str);
    });
}
```

- [ ] **Step 5: Update CMakeLists.txt**

Add `friend_service_connection.cpp` and `friend_service_connection.h` to the SOURCES/HEADERS lists. Add `friend.pb.cc`/`friend.pb.h` to PROTO_SRCS/PROTO_HDRS.

- [ ] **Step 6: Commit**

```bash
git add scripts/server/game_server/src/friend_service_connection.h \
  scripts/server/game_server/src/friend_service_connection.cpp \
  scripts/server/game_server/src/game_server.h \
  scripts/server/game_server/src/game_server.cpp \
  scripts/server/game_server/CMakeLists.txt
git commit -m "feat(friend): add Game Server friend message forwarding to Friend Service"
```

---

## Task 7: Client — Friend Message IDs + NetworkDispatcher

**Files:**
- Modify: `scripts/client/network_dispatcher.py`
- Modify: `scripts/client/message_ids.py` (auto-regenerated in Task 1)

- [ ] **Step 1: Add friend imports to network_dispatcher.py**

Add to the imports section:
```python
from .message_ids import (
    # ... existing imports ...
    MSG_ID_FRIEND_SEARCH_RESP, MSG_ID_FRIEND_ADD_RESP, MSG_ID_FRIEND_ADD_NOTIFY,
    MSG_ID_FRIEND_ACCEPT_RESP, MSG_ID_FRIEND_REJECT_RESP, MSG_ID_FRIEND_DELETE_RESP,
    MSG_ID_FRIEND_LIST_RESP, MSG_ID_FRIEND_ONLINE_NOTIFY, MSG_ID_FRIEND_OFFLINE_NOTIFY,
    MSG_ID_FRIEND_CHAT_RESP, MSG_ID_FRIEND_CHAT_NOTIFY, MSG_ID_FRIEND_CHAT_HISTORY_RESP,
    MSG_ID_FRIEND_GIFT_RESP, MSG_ID_FRIEND_GIFT_NOTIFY,
    MSG_ID_FRIEND_VISIT_RESP, MSG_ID_FRIEND_VISIT_ACTION_RESP,
    MSG_ID_FRIEND_RECOMMEND_RESP, MSG_ID_FRIEND_BLOCK_RESP, MSG_ID_FRIEND_UNBLOCK_RESP,
)
```

Add friend proto import:
```python
import friend_pb2
```

- [ ] **Step 2: Add friend callbacks to NetworkMessageDispatcher.__init__**

Add new callback parameters:
```python
def __init__(
    self,
    connection,
    # ... existing callbacks ...
    on_friend_search_resp=None,
    on_friend_add_resp=None,
    on_friend_add_notify=None,
    on_friend_accept_resp=None,
    on_friend_reject_resp=None,
    on_friend_delete_resp=None,
    on_friend_list_resp=None,
    on_friend_online_notify=None,
    on_friend_offline_notify=None,
    on_friend_chat_resp=None,
    on_friend_chat_notify=None,
    on_friend_chat_history_resp=None,
    on_friend_gift_resp=None,
    on_friend_gift_notify=None,
    on_friend_visit_resp=None,
    on_friend_visit_action_resp=None,
    on_friend_recommend_resp=None,
    on_friend_block_resp=None,
    on_friend_unblock_resp=None,
):
```

Store in `self._callbacks` and add to `self._dispatch_table`.

- [ ] **Step 3: Add friend handler methods**

Add private handler methods for each friend message. Pattern:
```python
def _handle_friend_list_resp(self, payload: bytes):
    try:
        player_msg = base_pb2.PlayerMsg()
        player_msg.ParseFromString(payload)
        resp = friend_pb2.FriendListResp()
        resp.ParseFromString(player_msg.payload)
        logger.info(f"[NetworkDispatcher]FriendListResp: {len(resp.friends)} friends, {len(resp.pending_requests)} pending")
        if self._callbacks.get("on_friend_list_resp"):
            self._callbacks["on_friend_list_resp"](resp)
    except Exception as e:
        logger.error(f"[NetworkDispatcher]Failed to parse FriendListResp: {e}")
```

Repeat for all 17 friend message types.

- [ ] **Step 4: Commit**

```bash
git add scripts/client/network_dispatcher.py
git commit -m "feat(friend): add friend message handlers to NetworkDispatcher"
```

---

## Task 8: Client — FriendClient + GameScene Integration

**Files:**
- Create: `scripts/client/friend_client.py`
- Modify: `scripts/client/game_scene.py`

- [ ] **Step 1: Create friend_client.py**

```python
"""
好友系统客户端模块
封装好友相关的网络请求发送和状态管理。
"""
import logging
from typing import Callable, Optional, List, Any

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'common', 'proto', 'generated'))
import base_pb2
import friend_pb2

from .message_ids import (
    MSG_ID_FRIEND_SEARCH_REQ, MSG_ID_FRIEND_ADD_REQ, MSG_ID_FRIEND_ACCEPT_REQ,
    MSG_ID_FRIEND_REJECT_REQ, MSG_ID_FRIEND_DELETE_REQ, MSG_ID_FRIEND_LIST_REQ,
    MSG_ID_FRIEND_BLOCK_REQ, MSG_ID_FRIEND_UNBLOCK_REQ,
    MSG_ID_FRIEND_CHAT_REQ, MSG_ID_FRIEND_CHAT_HISTORY_REQ,
    MSG_ID_FRIEND_GIFT_REQ,
    MSG_ID_FRIEND_VISIT_REQ, MSG_ID_FRIEND_VISIT_ACTION_REQ,
    MSG_ID_FRIEND_RECOMMEND_REQ,
)

logger = logging.getLogger("client.friend_client")


class FriendClient:
    """好友系统网络客户端"""

    def __init__(self, connection, player_id: int, server_id: int):
        self._connection = connection
        self._player_id = player_id
        self._server_id = server_id

        # State
        self.friends: List[Any] = []
        self.pending_requests: List[Any] = []
        self.chat_history: dict = {}  # target_id -> [messages]
        self.recommendations: List[Any] = []

    def _send(self, msg_id: int, inner_msg):
        """发送消息到服务器"""
        payload = inner_msg.SerializeToString()
        player_msg = base_pb2.PlayerMsg()
        player_msg.player_id = self._player_id
        player_msg.server_id = self._server_id
        player_msg.msg_id = msg_id
        player_msg.payload = payload
        msg_payload = player_msg.SerializeToString()
        self._connection.send_message(msg_id, msg_payload)

    def search(self, keyword: str):
        req = friend_pb2.FriendSearchReq()
        req.keyword = keyword
        self._send(MSG_ID_FRIEND_SEARCH_REQ, req)

    def send_add_request(self, target_id: int):
        req = friend_pb2.FriendAddReq()
        req.target_id = target_id
        self._send(MSG_ID_FRIEND_ADD_REQ, req)

    def accept_request(self, sender_id: int):
        req = friend_pb2.FriendAcceptReq()
        req.sender_id = sender_id
        self._send(MSG_ID_FRIEND_ACCEPT_REQ, req)

    def reject_request(self, sender_id: int):
        req = friend_pb2.FriendRejectReq()
        req.sender_id = sender_id
        self._send(MSG_ID_FRIEND_REJECT_REQ, req)

    def delete_friend(self, target_id: int):
        req = friend_pb2.FriendDeleteReq()
        req.target_id = target_id
        self._send(MSG_ID_FRIEND_DELETE_REQ, req)

    def request_list(self):
        req = friend_pb2.FriendListReq()
        self._send(MSG_ID_FRIEND_LIST_REQ, req)

    def send_chat(self, receiver_id: int, msg_type: int, content: str):
        req = friend_pb2.FriendChatReq()
        req.receiver_id = receiver_id
        req.msg_type = msg_type
        req.content = content
        self._send(MSG_ID_FRIEND_CHAT_REQ, req)

    def request_history(self, target_id: int):
        req = friend_pb2.FriendChatHistoryReq()
        req.target_id = target_id
        self._send(MSG_ID_FRIEND_CHAT_HISTORY_REQ, req)

    def send_gift(self, receiver_id: int, item_id: int, count: int):
        req = friend_pb2.FriendGiftReq()
        req.receiver_id = receiver_id
        req.item_id = item_id
        req.count = count
        self._send(MSG_ID_FRIEND_GIFT_REQ, req)

    def visit_farm(self, owner_id: int):
        req = friend_pb2.FriendVisitReq()
        req.owner_id = owner_id
        self._send(MSG_ID_FRIEND_VISIT_REQ, req)

    def visit_action(self, owner_id: int, action_type: int, target_x: int, target_y: int):
        req = friend_pb2.FriendVisitActionReq()
        req.owner_id = owner_id
        req.action_type = action_type
        req.target_x = target_x
        req.target_y = target_y
        self._send(MSG_ID_FRIEND_VISIT_ACTION_REQ, req)

    def request_recommendations(self):
        req = friend_pb2.FriendRecommendReq()
        self._send(MSG_ID_FRIEND_RECOMMEND_REQ, req)

    def block_player(self, target_id: int):
        req = friend_pb2.FriendBlockReq()
        req.target_id = target_id
        self._send(MSG_ID_FRIEND_BLOCK_REQ, req)

    def unblock_player(self, target_id: int):
        req = friend_pb2.FriendUnblockReq()
        req.target_id = target_id
        self._send(MSG_ID_FRIEND_UNBLOCK_REQ, req)
```

- [ ] **Step 2: Modify game_scene.py — wire friend system**

Add imports:
```python
from .friend_client import FriendClient
from .ui.friend_list_panel import FriendListPanel
from .ui.friend_chat_panel import FriendChatPanel
```

In `GameScene.__init__`, create FriendClient:
```python
self._friend_client = FriendClient(connection, player_id, server_id)
```

Add friend callbacks to NetworkMessageDispatcher construction:
```python
self._network_dispatcher = NetworkMessageDispatcher(
    connection=connection,
    # ... existing callbacks ...
    on_friend_list_resp=self._on_friend_list_resp,
    on_friend_add_resp=self._on_friend_add_resp,
    on_friend_add_notify=self._on_friend_add_notify,
    on_friend_chat_notify=self._on_friend_chat_notify,
    # ... etc
)
```

Add callback implementations:
```python
def _on_friend_list_resp(self, resp):
    self._friend_client.friends = list(resp.friends)
    self._friend_client.pending_requests = list(resp.pending_requests)

def _on_friend_chat_notify(self, notify):
    # Add to chat history and show notification
    sender_id = notify.sender_id
    if sender_id not in self._friend_client.chat_history:
        self._friend_client.chat_history[sender_id] = []
    self._friend_client.chat_history[sender_id].append(notify)
```

- [ ] **Step 3: Commit**

```bash
git add scripts/client/friend_client.py scripts/client/game_scene.py
git commit -m "feat(friend): add FriendClient and wire friend callbacks in GameScene"
```

---

## Task 9: Client UI — FriendListPanel + FriendChatPanel

**Files:**
- Create: `scripts/client/ui/friend_list_panel.py`
- Create: `scripts/client/ui/friend_chat_panel.py`
- Modify: `scripts/client/ui/__init__.py`
- Modify: `scripts/client/constants.py` (add friend UI constants)

- [ ] **Step 1: Add friend UI constants to constants.py**

```python
# Friend panel
FRIEND_PANEL_WIDTH = 400
FRIEND_PANEL_HEIGHT = 500
FRIEND_PANEL_BG_COLOR = (30, 30, 40, 220)
FRIEND_PANEL_BORDER_COLOR = (100, 100, 120)
FRIEND_PANEL_ROW_HEIGHT = 40
FRIEND_PANEL_ONLINE_COLOR = (80, 200, 80)
FRIEND_PANEL_OFFLINE_COLOR = (150, 150, 150)
FRIEND_PANEL_HOVER_COLOR = (60, 60, 80, 180)
FRIEND_PANEL_BUTTON_COLOR = (60, 120, 60)
FRIEND_PANEL_BUTTON_HOVER = (80, 160, 80)
FRIEND_PANEL_TEXT_COLOR = (220, 220, 220)
FRIEND_PANEL_TITLE_COLOR = (255, 215, 0)

# Chat panel
CHAT_PANEL_WIDTH = 350
CHAT_PANEL_HEIGHT = 400
CHAT_PANEL_BG_COLOR = (25, 25, 35, 230)
CHAT_PANEL_MSG_SELF_COLOR = (60, 100, 60)
CHAT_PANEL_MSG_OTHER_COLOR = (50, 50, 70)
```

- [ ] **Step 2: Create friend_list_panel.py**

```python
"""
好友列表面板
显示好友列表、在线状态、操作按钮。
"""
import pygame
import logging
from typing import List, Any, Optional, Callable

from ..constants import (
    FRIEND_PANEL_WIDTH, FRIEND_PANEL_HEIGHT,
    FRIEND_PANEL_BG_COLOR, FRIEND_PANEL_BORDER_COLOR,
    FRIEND_PANEL_ROW_HEIGHT, FRIEND_PANEL_ONLINE_COLOR,
    FRIEND_PANEL_OFFLINE_COLOR, FRIEND_PANEL_HOVER_COLOR,
    FRIEND_PANEL_BUTTON_COLOR, FRIEND_PANEL_TEXT_COLOR,
    FRIEND_PANEL_TITLE_COLOR, PANEL_BG_COLOR,
)

logger = logging.getLogger("client.ui.friend_list_panel")


class FriendListPanel:
    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._visible = False
        self._hovered_index = -1
        self._scroll_offset = 0

        # Layout
        self._panel_x = (screen_width - FRIEND_PANEL_WIDTH) // 2
        self._panel_y = (screen_height - FRIEND_PANEL_HEIGHT) // 2
        self._panel_rect = pygame.Rect(self._panel_x, self._panel_y,
                                        FRIEND_PANEL_WIDTH, FRIEND_PANEL_HEIGHT)

        # Close button
        self._close_rect = pygame.Rect(
            self._panel_x + FRIEND_PANEL_WIDTH - 30,
            self._panel_y + 5, 24, 24)

        # Fonts
        try:
            self._title_font = pygame.font.SysFont("microsoftyahei", 18, bold=True)
            self._font = pygame.font.SysFont("microsoftyahei", 14)
            self._small_font = pygame.font.SysFont("microsoftyahei", 12)
        except:
            self._title_font = pygame.font.Font(None, 20)
            self._font = pygame.font.Font(None, 16)
            self._small_font = pygame.font.Font(None, 14)

    def toggle(self):
        self._visible = not self._visible

    def is_visible(self):
        return self._visible

    def is_panel_area(self, pos) -> bool:
        return self._panel_rect.collidepoint(pos)

    def is_close_clicked(self, pos) -> bool:
        return self._close_rect.collidepoint(pos)

    def get_clicked_friend_index(self, pos) -> int:
        if not self._visible: return -1
        x, y = pos
        list_start_y = self._panel_y + 50
        if x < self._panel_x or x > self._panel_x + FRIEND_PANEL_WIDTH:
            return -1
        if y < list_start_y or y > self._panel_y + FRIEND_PANEL_HEIGHT - 50:
            return -1
        index = (y - list_start_y + self._scroll_offset * FRIEND_PANEL_ROW_HEIGHT) // FRIEND_PANEL_ROW_HEIGHT
        return index

    def render(self, screen: pygame.Surface, friends: List[Any], pending_count: int = 0):
        if not self._visible: return

        # Panel background
        overlay = pygame.Surface((FRIEND_PANEL_WIDTH, FRIEND_PANEL_HEIGHT), pygame.SRCALPHA)
        overlay.fill(FRIEND_PANEL_BG_COLOR)
        screen.blit(overlay, (self._panel_x, self._panel_y))

        # Border
        pygame.draw.rect(screen, FRIEND_PANEL_BORDER_COLOR, self._panel_rect, 2)

        # Title
        title = self._title_font.render(f"好友列表 ({len(friends)}/50)", True, FRIEND_PANEL_TITLE_COLOR)
        screen.blit(title, (self._panel_x + 10, self._panel_y + 10))

        # Close button
        close_text = self._font.render("×", True, (255, 100, 100))
        screen.blit(close_text, (self._close_rect.x + 6, self._close_rect.y + 2))

        # Friend list
        list_start_y = self._panel_y + 50
        visible_count = (FRIEND_PANEL_HEIGHT - 100) // FRIEND_PANEL_ROW_HEIGHT

        for i in range(min(visible_count, len(friends))):
            idx = i + self._scroll_offset
            if idx >= len(friends): break

            friend = friends[idx]
            row_y = list_start_y + i * FRIEND_PANEL_ROW_HEIGHT

            # Hover highlight
            if idx == self._hovered_index:
                hover_surf = pygame.Surface((FRIEND_PANEL_WIDTH - 20, FRIEND_PANEL_ROW_HEIGHT), pygame.SRCALPHA)
                hover_surf.fill(FRIEND_PANEL_HOVER_COLOR)
                screen.blit(hover_surf, (self._panel_x + 10, row_y))

            # Online indicator
            color = FRIEND_PANEL_ONLINE_COLOR if friend.online else FRIEND_PANEL_OFFLINE_COLOR
            pygame.draw.circle(screen, color, (self._panel_x + 25, row_y + 20), 6)

            # Name and level
            name_text = self._font.render(f"{friend.role_name}  Lv.{friend.level}", True, FRIEND_PANEL_TEXT_COLOR)
            screen.blit(name_text, (self._panel_x + 40, row_y + 10))

            # Scene
            scene_text = self._small_font.render(friend.scene_id if friend.online else "离线", True, color)
            screen.blit(scene_text, (self._panel_x + 250, row_y + 12))

        # Pending requests indicator
        if pending_count > 0:
            req_text = self._font.render(f"好友请求 ({pending_count})", True, (255, 200, 100))
            screen.blit(req_text, (self._panel_x + 10, self._panel_y + FRIEND_PANEL_HEIGHT - 35))

    def update_hover(self, pos):
        if not self._visible:
            self._hovered_index = -1
            return
        self._hovered_index = self.get_clicked_friend_index(pos)
```

- [ ] **Step 3: Create friend_chat_panel.py**

```python
"""
私聊面板
显示聊天记录、输入框、表情按钮。
"""
import pygame
import logging
from typing import List, Any

from ..constants import (
    CHAT_PANEL_WIDTH, CHAT_PANEL_HEIGHT,
    CHAT_PANEL_BG_COLOR, CHAT_PANEL_MSG_SELF_COLOR,
    CHAT_PANEL_MSG_OTHER_COLOR, FRIEND_PANEL_TEXT_COLOR,
    FRIEND_PANEL_TITLE_COLOR, FRIEND_PANEL_BORDER_COLOR,
)

logger = logging.getLogger("client.ui.friend_chat_panel")


class FriendChatPanel:
    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._visible = False
        self._target_id = 0
        self._target_name = ""
        self._input_text = ""
        self._input_active = False
        self._scroll_offset = 0

        self._panel_x = screen_width - CHAT_PANEL_WIDTH - 20
        self._panel_y = (screen_height - CHAT_PANEL_HEIGHT) // 2
        self._panel_rect = pygame.Rect(self._panel_x, self._panel_y,
                                        CHAT_PANEL_WIDTH, CHAT_PANEL_HEIGHT)

        self._input_rect = pygame.Rect(
            self._panel_x + 10,
            self._panel_y + CHAT_PANEL_HEIGHT - 40,
            CHAT_PANEL_WIDTH - 80, 30)

        self._send_rect = pygame.Rect(
            self._panel_x + CHAT_PANEL_WIDTH - 65,
            self._panel_y + CHAT_PANEL_HEIGHT - 40, 55, 30)

        self._close_rect = pygame.Rect(
            self._panel_x + CHAT_PANEL_WIDTH - 30,
            self._panel_y + 5, 24, 24)

        try:
            self._font = pygame.font.SysFont("microsoftyahei", 13)
            self._title_font = pygame.font.SysFont("microsoftyahei", 16, bold=True)
            self._input_font = pygame.font.SysFont("microsoftyahei", 14)
        except:
            self._font = pygame.font.Font(None, 15)
            self._title_font = pygame.font.Font(None, 18)
            self._input_font = pygame.font.Font(None, 16)

    def open(self, target_id: int, target_name: str):
        self._target_id = target_id
        self._target_name = target_name
        self._visible = True
        self._input_active = True

    def close(self):
        self._visible = False
        self._input_active = False

    def is_visible(self):
        return self._visible

    def is_panel_area(self, pos) -> bool:
        return self._panel_rect.collidepoint(pos)

    def is_close_clicked(self, pos) -> bool:
        return self._close_rect.collidepoint(pos)

    def is_input_area(self, pos) -> bool:
        return self._input_rect.collidepoint(pos)

    def is_send_clicked(self, pos) -> bool:
        return self._send_rect.collidepoint(pos)

    def handle_key(self, event) -> Optional[str]:
        if not self._input_active: return None
        if event.key == pygame.K_BACKSPACE:
            self._input_text = self._input_text[:-1]
        elif event.key == pygame.K_RETURN:
            if self._input_text.strip():
                msg = self._input_text.strip()
                self._input_text = ""
                return msg
        elif len(self._input_text) < 200:
            self._input_text += event.unicode
        return None

    def get_input_text(self) -> str:
        return self._input_text

    def clear_input(self):
        self._input_text = ""

    def render(self, screen: pygame.Surface, messages: List[Any], my_player_id: int):
        if not self._visible: return

        # Background
        overlay = pygame.Surface((CHAT_PANEL_WIDTH, CHAT_PANEL_HEIGHT), pygame.SRCALPHA)
        overlay.fill(CHAT_PANEL_BG_COLOR)
        screen.blit(overlay, (self._panel_x, self._panel_y))
        pygame.draw.rect(screen, FRIEND_PANEL_BORDER_COLOR, self._panel_rect, 2)

        # Title
        title = self._title_font.render(f"与 {self._target_name} 私聊", True, FRIEND_PANEL_TITLE_COLOR)
        screen.blit(title, (self._panel_x + 10, self._panel_y + 10))

        # Close
        close_text = self._font.render("×", True, (255, 100, 100))
        screen.blit(close_text, (self._close_rect.x + 6, self._close_rect.y + 2))

        # Messages
        msg_y = self._panel_y + 40
        max_msg_y = self._panel_y + CHAT_PANEL_HEIGHT - 50
        visible_msgs = messages[-20:] if len(messages) > 20 else messages

        for msg in visible_msgs:
            if msg_y > max_msg_y: break
            is_self = (msg.sender_id == my_player_id)
            bg_color = CHAT_PANEL_MSG_SELF_COLOR if is_self else CHAT_PANEL_MSG_OTHER_COLOR

            # Message bubble
            text_surf = self._font.render(msg.content, True, FRIEND_PANEL_TEXT_COLOR)
            bubble_w = min(text_surf.get_width() + 20, CHAT_PANEL_WIDTH - 40)
            bubble_x = self._panel_x + CHAT_PANEL_WIDTH - bubble_w - 15 if is_self else self._panel_x + 15

            bubble = pygame.Surface((bubble_w, 28), pygame.SRCALPHA)
            bubble.fill(bg_color)
            screen.blit(bubble, (bubble_x, msg_y))
            screen.blit(text_surf, (bubble_x + 10, msg_y + 6))

            msg_y += 35

        # Input box
        pygame.draw.rect(screen, (40, 40, 50), self._input_rect)
        pygame.draw.rect(screen, (80, 80, 100), self._input_rect, 1)
        input_surf = self._input_font.render(self._input_text, True, FRIEND_PANEL_TEXT_COLOR)
        screen.blit(input_surf, (self._input_rect.x + 5, self._input_rect.y + 7))

        # Send button
        pygame.draw.rect(screen, (60, 120, 60), self._send_rect)
        send_text = self._font.render("发送", True, (255, 255, 255))
        screen.blit(send_text, (self._send_rect.x + 10, self._send_rect.y + 7))
```

- [ ] **Step 4: Update ui/__init__.py**

Add exports:
```python
from .friend_list_panel import FriendListPanel
from .friend_chat_panel import FriendChatPanel
```

- [ ] **Step 5: Commit**

```bash
git add scripts/client/ui/friend_list_panel.py scripts/client/ui/friend_chat_panel.py \
  scripts/client/ui/__init__.py scripts/client/constants.py
git commit -m "feat(friend): add FriendListPanel and FriendChatPanel UI components"
```

---

## Task 10: Client UI — FriendSearchPanel + FriendVisitPanel

**Files:**
- Create: `scripts/client/ui/friend_search_panel.py`
- Create: `scripts/client/ui/friend_visit_panel.py`

- [ ] **Step 1: Create friend_search_panel.py**

```python
"""
搜索/添加好友面板
"""
import pygame
import logging
from typing import List, Any, Optional

from ..constants import (
    FRIEND_PANEL_WIDTH, FRIEND_PANEL_BG_COLOR, FRIEND_PANEL_BORDER_COLOR,
    FRIEND_PANEL_TEXT_COLOR, FRIEND_PANEL_TITLE_COLOR, FRIEND_PANEL_BUTTON_COLOR,
    FRIEND_PANEL_ROW_HEIGHT,
)

logger = logging.getLogger("client.ui.friend_search_panel")


class FriendSearchPanel:
    def __init__(self, screen_width: int, screen_height: int):
        self._visible = False
        self._input_text = ""
        self._input_active = False
        self._results: List[Any] = []

        panel_w = 350
        panel_h = 300
        self._panel_x = (screen_width - panel_w) // 2
        self._panel_y = (screen_height - panel_h) // 2
        self._panel_rect = pygame.Rect(self._panel_x, self._panel_y, panel_w, panel_h)

        self._input_rect = pygame.Rect(self._panel_x + 10, self._panel_y + 40, panel_w - 20, 30)
        self._search_rect = pygame.Rect(self._panel_x + panel_w - 80, self._panel_y + 75, 70, 28)
        self._close_rect = pygame.Rect(self._panel_x + panel_w - 30, self._panel_y + 5, 24, 24)

        try:
            self._font = pygame.font.SysFont("microsoftyahei", 13)
            self._title_font = pygame.font.SysFont("microsoftyahei", 16, bold=True)
        except:
            self._font = pygame.font.Font(None, 15)
            self._title_font = pygame.font.Font(None, 18)

    def toggle(self):
        self._visible = not self._visible
        self._input_active = self._visible

    def is_visible(self):
        return self._visible

    def is_close_clicked(self, pos):
        return self._close_rect.collidepoint(pos)

    def is_search_clicked(self, pos):
        return self._search_rect.collidepoint(pos)

    def is_input_area(self, pos):
        return self._input_rect.collidepoint(pos)

    def get_clicked_result_index(self, pos) -> int:
        y = pos[1]
        start_y = self._panel_y + 110
        if y < start_y: return -1
        idx = (y - start_y) // FRIEND_PANEL_ROW_HEIGHT
        if idx < len(self._results): return idx
        return -1

    def get_search_text(self) -> str:
        return self._input_text.strip()

    def set_results(self, results: List[Any]):
        self._results = results

    def handle_key(self, event):
        if not self._input_active: return
        if event.key == pygame.K_BACKSPACE:
            self._input_text = self._input_text[:-1]
        elif event.key == pygame.K_RETURN:
            return True  # trigger search
        elif len(self._input_text) < 50:
            self._input_text += event.unicode
        return False

    def render(self, screen: pygame.Surface):
        if not self._visible: return

        overlay = pygame.Surface((self._panel_rect.w, self._panel_rect.h), pygame.SRCALPHA)
        overlay.fill(FRIEND_PANEL_BG_COLOR)
        screen.blit(overlay, (self._panel_x, self._panel_y))
        pygame.draw.rect(screen, FRIEND_PANEL_BORDER_COLOR, self._panel_rect, 2)

        title = self._title_font.render("搜索玩家", True, FRIEND_PANEL_TITLE_COLOR)
        screen.blit(title, (self._panel_x + 10, self._panel_y + 10))

        close_text = self._font.render("×", True, (255, 100, 100))
        screen.blit(close_text, (self._close_rect.x + 6, self._close_rect.y + 2))

        # Input
        pygame.draw.rect(screen, (40, 40, 50), self._input_rect)
        pygame.draw.rect(screen, (80, 80, 100), self._input_rect, 1)
        input_surf = self._font.render(self._input_text, True, FRIEND_PANEL_TEXT_COLOR)
        screen.blit(input_surf, (self._input_rect.x + 5, self._input_rect.y + 8))

        # Search button
        pygame.draw.rect(screen, FRIEND_PANEL_BUTTON_COLOR, self._search_rect)
        btn_text = self._font.render("搜索", True, (255, 255, 255))
        screen.blit(btn_text, (self._search_rect.x + 15, self._search_rect.y + 6))

        # Results
        for i, result in enumerate(self._results):
            row_y = self._panel_y + 110 + i * FRIEND_PANEL_ROW_HEIGHT
            name_text = self._font.render(f"{result.role_name} Lv.{result.level}", True, FRIEND_PANEL_TEXT_COLOR)
            screen.blit(name_text, (self._panel_x + 15, row_y + 10))
            add_text = self._font.render("[添加]", True, (100, 200, 100))
            screen.blit(add_text, (self._panel_x + 250, row_y + 10))
```

- [ ] **Step 2: Create friend_visit_panel.py**

```python
"""
农场访问 HUD
在访问好友农场时显示的操作面板。
"""
import pygame
import logging
from typing import Optional

from ..constants import (
    FRIEND_PANEL_BG_COLOR, FRIEND_PANEL_BORDER_COLOR,
    FRIEND_PANEL_TEXT_COLOR, FRIEND_PANEL_TITLE_COLOR,
    FRIEND_PANEL_BUTTON_COLOR,
)

logger = logging.getLogger("client.ui.friend_visit_panel")


class FriendVisitPanel:
    def __init__(self, screen_width: int, screen_height: int):
        self._visible = False
        self._owner_name = ""
        self._actions_remaining = 5

        panel_w = 250
        panel_h = 180
        self._panel_x = 20
        self._panel_y = screen_height - panel_h - 80
        self._panel_rect = pygame.Rect(self._panel_x, self._panel_y, panel_w, panel_h)

        btn_w = 90
        btn_h = 30
        self._water_rect = pygame.Rect(self._panel_x + 10, self._panel_y + 70, btn_w, btn_h)
        self._weed_rect = pygame.Rect(self._panel_x + 110, self._panel_y + 70, btn_w, btn_h)
        self._steal_rect = pygame.Rect(self._panel_x + 10, self._panel_y + 110, btn_w, btn_h)
        self._return_rect = pygame.Rect(self._panel_x + 110, self._panel_y + 110, btn_w, btn_h)

        try:
            self._font = pygame.font.SysFont("microsoftyahei", 13)
            self._title_font = pygame.font.SysFont("microsoftyahei", 15, bold=True)
        except:
            self._font = pygame.font.Font(None, 15)
            self._title_font = pygame.font.Font(None, 17)

    def open(self, owner_name: str):
        self._visible = True
        self._owner_name = owner_name
        self._actions_remaining = 5

    def close(self):
        self._visible = False

    def is_visible(self):
        return self._visible

    def is_water_clicked(self, pos):
        return self._water_rect.collidepoint(pos)

    def is_weed_clicked(self, pos):
        return self._weed_rect.collidepoint(pos)

    def is_steal_clicked(self, pos):
        return self._steal_rect.collidepoint(pos)

    def is_return_clicked(self, pos):
        return self._return_rect.collidepoint(pos)

    def use_action(self):
        if self._actions_remaining > 0:
            self._actions_remaining -= 1
            return True
        return False

    def render(self, screen: pygame.Surface):
        if not self._visible: return

        overlay = pygame.Surface((self._panel_rect.w, self._panel_rect.h), pygame.SRCALPHA)
        overlay.fill(FRIEND_PANEL_BG_COLOR)
        screen.blit(overlay, (self._panel_x, self._panel_y))
        pygame.draw.rect(screen, FRIEND_PANEL_BORDER_COLOR, self._panel_rect, 2)

        title = self._title_font.render(f"访问 {self._owner_name} 的农场", True, FRIEND_PANEL_TITLE_COLOR)
        screen.blit(title, (self._panel_x + 10, self._panel_y + 10))

        remain_text = self._font.render(f"剩余操作: {self._actions_remaining}/5", True, FRIEND_PANEL_TEXT_COLOR)
        screen.blit(remain_text, (self._panel_x + 10, self._panel_y + 40))

        # Buttons
        for rect, label in [
            (self._water_rect, "浇水"),
            (self._weed_rect, "除草"),
            (self._steal_rect, "偷菜"),
            (self._return_rect, "返回"),
        ]:
            pygame.draw.rect(screen, FRIEND_PANEL_BUTTON_COLOR, rect)
            btn_text = self._font.render(label, True, (255, 255, 255))
            screen.blit(btn_text, (rect.x + 25, rect.y + 7))
```

- [ ] **Step 3: Commit**

```bash
git add scripts/client/ui/friend_search_panel.py scripts/client/ui/friend_visit_panel.py
git commit -m "feat(friend): add FriendSearchPanel and FriendVisitPanel UI components"
```

---

## Task 11: Integration — Wire UI Panels into GameScene + Input Handling

**Files:**
- Modify: `scripts/client/game_scene.py`
- Modify: `scripts/client/input_manager.py` (if needed for friend keybindings)

- [ ] **Step 1: Add friend panel instances to GameScene**

In `GameScene.__init__`:
```python
from .ui import FriendListPanel, FriendChatPanel, FriendSearchPanel, FriendVisitPanel

self._friend_list_panel = FriendListPanel(screen_width, screen_height)
self._friend_chat_panel = FriendChatPanel(screen_width, screen_height)
self._friend_search_panel = FriendSearchPanel(screen_width, screen_height)
self._friend_visit_panel = FriendVisitPanel(screen_width, screen_height)
```

- [ ] **Step 2: Add friend keybinding (F key to toggle friend list)**

In the event loop (wherever keyboard events are processed):
```python
if event.type == pygame.KEYDOWN:
    if event.key == pygame.K_f:
        self._friend_list_panel.toggle()
        if self._friend_list_panel.is_visible():
            self._friend_client.request_list()
```

- [ ] **Step 3: Add mouse click handling for friend panels**

In the mouse click handler:
```python
if event.type == pygame.MOUSEBUTTONDOWN:
    pos = event.pos

    # Friend list panel
    if self._friend_list_panel.is_visible():
        if self._friend_list_panel.is_close_clicked(pos):
            self._friend_list_panel.toggle()
        else:
            idx = self._friend_list_panel.get_clicked_friend_index(pos)
            if idx >= 0 and idx < len(self._friend_client.friends):
                friend = self._friend_client.friends[idx]
                # Open chat with this friend
                self._friend_chat_panel.open(friend.player_id, friend.role_name)
                self._friend_client.request_history(friend.player_id)

    # Friend chat panel
    if self._friend_chat_panel.is_visible():
        if self._friend_chat_panel.is_close_clicked(pos):
            self._friend_chat_panel.close()
        elif self._friend_chat_panel.is_send_clicked(pos):
            text = self._friend_chat_panel.get_input_text()
            if text.strip():
                self._friend_client.send_chat(self._friend_chat_panel._target_id, 0, text)
                self._friend_chat_panel.clear_input()
        elif self._friend_chat_panel.is_input_area(pos):
            self._friend_chat_panel._input_active = True

    # Friend search panel
    if self._friend_search_panel.is_visible():
        if self._friend_search_panel.is_close_clicked(pos):
            self._friend_search_panel.toggle()
        elif self._friend_search_panel.is_search_clicked(pos):
            keyword = self._friend_search_panel.get_search_text()
            if keyword:
                self._friend_client.search(keyword)

    # Friend visit panel
    if self._friend_visit_panel.is_visible():
        if self._friend_visit_panel.is_water_clicked(pos):
            if self._friend_visit_panel.use_action():
                self._friend_client.visit_action(self._friend_visit_panel._owner_id, 0, 0, 0)
        elif self._friend_visit_panel.is_steal_clicked(pos):
            if self._friend_visit_panel.use_action():
                self._friend_client.visit_action(self._friend_visit_panel._owner_id, 3, 0, 0)
        elif self._friend_visit_panel.is_return_clicked(pos):
            self._friend_visit_panel.close()
            # TODO: return to own farm scene
```

- [ ] **Step 4: Add friend panel rendering to game loop**

In the render section of the game loop:
```python
self._friend_list_panel.render(screen, self._friend_client.friends, len(self._friend_client.pending_requests))
self._friend_chat_panel.render(screen, self._friend_client.chat_history.get(self._friend_chat_panel._target_id, []), self._friend_client._player_id)
self._friend_search_panel.render(screen)
self._friend_visit_panel.render(screen)
```

- [ ] **Step 5: Add text input handling for chat/search**

In the event loop for KEYDOWN:
```python
if self._friend_chat_panel.is_visible() and self._friend_chat_panel._input_active:
    result = self._friend_chat_panel.handle_key(event)
    if result:
        self._friend_client.send_chat(self._friend_chat_panel._target_id, 0, result)

if self._friend_search_panel.is_visible() and self._friend_search_panel._input_active:
    if self._friend_search_panel.handle_key(event):
        keyword = self._friend_search_panel.get_search_text()
        if keyword:
            self._friend_client.search(keyword)
```

- [ ] **Step 6: Commit**

```bash
git add scripts/client/game_scene.py
git commit -m "feat(friend): integrate friend panels into GameScene with input handling"
```

---

## Task 12: Build + Test

**Files:**
- Create: `scripts/server/friend_service/tests/test_friend_manager.cpp`

- [ ] **Step 1: Create test_friend_manager.cpp**

```cpp
#include "friend_manager.h"
#include "friend_constants.h"
#include <cassert>
#include <iostream>

// Mock RedisConnection for testing
class MockRedisConnection {
public:
    bool sadd(const std::string& key, const std::string& member) {
        store_[key].insert(member);
        return true;
    }
    bool srem(const std::string& key, const std::string& member) {
        store_[key].erase(member);
        return true;
    }
    bool sismember(const std::string& key, const std::string& member) {
        auto it = store_.find(key);
        if (it == store_.end()) return false;
        return it->second.count(member) > 0;
    }
    size_t scard(const std::string& key) {
        auto it = store_.find(key);
        return it == store_.end() ? 0 : it->second.size();
    }
    bool hset(const std::string& key, const std::string& field, const std::string& value) {
        hashes_[key][field] = value;
        return true;
    }
    std::string hget(const std::string& key, const std::string& field) {
        auto it = hashes_.find(key);
        if (it == hashes_.end()) return "";
        auto fit = it->second.find(field);
        return fit == it->second.end() ? "" : fit->second;
    }
    bool hdel(const std::string& key, const std::string& field) {
        auto it = hashes_.find(key);
        if (it != hashes_.end()) it->second.erase(field);
        return true;
    }
    bool set(const std::string& key, const std::string& value) {
        kvs_[key] = value;
        return true;
    }
    std::string get(const std::string& key) {
        auto it = kvs_.find(key);
        return it == kvs_.end() ? "" : it->second;
    }
private:
    std::unordered_map<std::string, std::unordered_set<std::string>> store_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> hashes_;
    std::unordered_map<std::string, std::string> kvs_;
};

void test_friend_constants() {
    using namespace farm;
    assert(MAX_FRIENDS == 50);
    assert(MAX_OFFLINE_MESSAGES == 100);
    assert(MAX_CHAT_MSG_LENGTH == 200);
    std::cout << "PASS: test_friend_constants" << std::endl;
}

void test_redis_key_helpers() {
    using namespace farm;
    assert(redis_key::friend_set(1001) == "friend:1001:set");
    assert(redis_key::friend_requests(1001) == "friend:1001:requests");
    assert(redis_key::player_online(1001) == "player:1001:online");
    assert(redis_key::chat_offline(1001) == "chat:offline:1001");
    std::cout << "PASS: test_redis_key_helpers" << std::endl;
}

int main() {
    test_friend_constants();
    test_redis_key_helpers();
    std::cout << "All friend_manager tests passed!" << std::endl;
    return 0;
}
```

- [ ] **Step 2: Build Friend Service**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/friend_service
mkdir -p build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

Verify `bin/friend_service.exe` is created.

- [ ] **Step 3: Run unit tests**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/friend_service/build
./Release/friend_tests.exe
```

Expected: All tests pass.

- [ ] **Step 4: Build Game Server with friend forwarding**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

Verify it compiles with the new `friend_service_connection` source files.

- [ ] **Step 5: Commit**

```bash
git add scripts/server/friend_service/tests/
git commit -m "test(friend): add FriendManager unit tests and verify build"
```

---

## Self-Review Checklist

After completing all tasks:

- [ ] **Spec coverage:** Every spec requirement has a corresponding task
  - Friend CRUD → Task 4
  - Online status → Task 4 (set_player_online/offline)
  - Private chat → Task 5
  - Gift system → Task 5
  - Farm visit → Task 5
  - Friend recommend → Task 5
  - Block/unblock → Task 4
  - Offline messages → Task 5
  - Client UI → Tasks 9, 10, 11
  - Message protocol → Task 1
  - Game Server forwarding → Task 6
  - Friend Service skeleton → Tasks 2, 3
  - Data persistence → Task 5 (FriendDataManager)
  - Testing → Task 12

- [ ] **No placeholders:** All code is complete and compilable

- [ ] **Type consistency:** `FriendErrorCode`, `VisitAction`, `ChatMsgType` enums match proto definitions

- [ ] **Redis key consistency:** All `redis_key::` functions match the key patterns used in managers

- [ ] **Message ID consistency:** All `MSG_ID_FRIEND_*` constants used in handlers match `shared/message_ids.json`
