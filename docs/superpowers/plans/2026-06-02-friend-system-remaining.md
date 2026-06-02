# Friend System — Remaining Tasks Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the Friend System by implementing Game Server forwarding, client NetworkDispatcher, FriendClient, and client UI panels.

**Architecture:** Game Server forwards client friend messages to Friend Service via TCP. Client gets FriendClient for network requests and UI panels (friend list, chat, search, visit) integrated into GameScene.

**Tech Stack:** C++17, libevent, protobuf (server). Python, pygame (client).

**Prerequisites:** Tasks 1-5 are complete. Friend Service C++ microservice (19 files) is fully implemented. Proto definitions, message IDs, and generated code all exist.

---

## File Structure

### New Files — Game Server (friend forwarding)

| File | Responsibility |
|------|---------------|
| `scripts/server/game_server/src/friend_service_connection.h` | TCP connection to Friend Service (mirrors DBMgrConnection pattern) |
| `scripts/server/game_server/src/friend_service_connection.cpp` | Connection implementation: connect, identify, heartbeat, reconnect, message routing |

### Modified Files — Game Server

| File | Change |
|------|--------|
| `scripts/server/game_server/src/game_server.h` | Add `FriendServiceConnection` member |
| `scripts/server/game_server/src/game_server.cpp` | Register friend message forwarding handlers in `start()` |
| `scripts/server/game_server/CMakeLists.txt` | Add `friend_service_connection.cpp` to SOURCES |

### New Files — Client

| File | Responsibility |
|------|---------------|
| `scripts/client/friend_client.py` | Friend network client: send requests, hold state |
| `scripts/client/ui/friend_list_panel.py` | Friend list panel rendering + hit testing |
| `scripts/client/ui/friend_chat_panel.py` | Private chat panel |
| `scripts/client/ui/friend_search_panel.py` | Search/add friend panel |
| `scripts/client/ui/friend_visit_panel.py` | Farm visit HUD |

### Modified Files — Client

| File | Change |
|------|--------|
| `scripts/client/network_dispatcher.py` | Add friend message handlers + callbacks |
| `scripts/client/game_scene.py` | Wire friend callbacks, create FriendClient, add UI panels |
| `scripts/client/constants.py` | Add friend UI constants |
| `scripts/client/ui/__init__.py` | Export friend panels |

---

## Task 6: Game Server — Friend Service Connection + Message Forwarding

**Files:**
- Create: `scripts/server/game_server/src/friend_service_connection.h`
- Create: `scripts/server/game_server/src/friend_service_connection.cpp`
- Modify: `scripts/server/game_server/src/game_server.h`
- Modify: `scripts/server/game_server/src/game_server.cpp`
- Modify: `scripts/server/game_server/CMakeLists.txt`

### Step 1: Create friend_service_connection.h

Create the header file following the DBMgrConnection pattern but simplified (single connection, no manager):

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

### Step 2: Create friend_service_connection.cpp

Implement the connection following the same pattern as `dbmgr_connection.cpp`:

```cpp
#include "friend_service_connection.h"
#include "message_parser.h"
#include "internal_msg_ids.h"
#include "message_ids.h"
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
    size_t old_size = read_buffer_.size();
    read_buffer_.resize(old_size + len);
    evbuffer_remove(input, read_buffer_.data() + old_size, len);
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
                SPDLOG_INFO("[Game]FriendService identified");
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

### Step 3: Modify game_server.h — add FriendServiceConnection member

Add the include and member to `GameServer` class:

```cpp
// Add include at the top:
#include "friend_service_connection.h"

// Add member in GameServer class (private section, near dbmgr_mgr_):
    std::unique_ptr<FriendServiceConnection> friend_conn_;
```

### Step 4: Modify game_server.cpp — register friend message forwarding

In `GameServer::start()`, after the combat system initialization (around line 194), add:

```cpp
// Friend Service connection
friend_conn_ = std::make_unique<FriendServiceConnection>(base_);
friend_conn_->set_on_message([this](uint64_t player_id, uint32_t msg_id, const std::string& payload) {
    // Forward Friend Service response back to client via Gate
    send_game_msg(player_id, msg_id,
                  reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
});
friend_conn_->connect("127.0.0.1", 9092);  // TODO: read from config
SPDLOG_INFO("[Game]FriendService connection initiated");

// Register friend message forwarding handlers
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

### Step 5: Update CMakeLists.txt

Add `friend_service_connection.cpp` and `.h` to the SOURCES/HEADERS lists. Add `friend.pb.cc` to PROTO_SRCS.

### Step 6: Commit

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

### Step 1: Add friend imports to network_dispatcher.py

Add to the imports section (after existing message ID imports):

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

### Step 2: Add friend callbacks to NetworkMessageDispatcher.__init__

Add new callback parameters to the `__init__` method:

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

Store in `self._callbacks` dict and add to `self._dispatch_table`.

### Step 3: Add friend handler methods

Add private handler methods for each friend message type. Pattern:

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

Repeat for all 17 friend message types (search_resp, add_resp, add_notify, accept_resp, reject_resp, delete_resp, list_resp, online_notify, offline_notify, chat_resp, chat_notify, chat_history_resp, gift_resp, gift_notify, visit_resp, visit_action_resp, recommend_resp, block_resp, unblock_resp).

### Step 4: Commit

```bash
git add scripts/client/network_dispatcher.py
git commit -m "feat(friend): add friend message handlers to NetworkDispatcher"
```

---

## Task 8: Client — FriendClient + GameScene Integration

**Files:**
- Create: `scripts/client/friend_client.py`
- Modify: `scripts/client/game_scene.py`

### Step 1: Create friend_client.py

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

### Step 2: Modify game_scene.py — wire friend system

Add imports:

```python
from .friend_client import FriendClient
from .ui.friend_list_panel import FriendListPanel
from .ui.friend_chat_panel import FriendChatPanel
```

In `GameScene.__init__`, after player data setup, create FriendClient:

```python
self._friend_client = FriendClient(connection, player_data.get("player_id", 0), player_data.get("server_id", 1))
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
    on_friend_chat_resp=self._on_friend_chat_resp,
    on_friend_online_notify=self._on_friend_online_notify,
    on_friend_offline_notify=self._on_friend_offline_notify,
    # ... etc
)
```

Add callback implementations:

```python
def _on_friend_list_resp(self, resp):
    self._friend_client.friends = list(resp.friends)
    self._friend_client.pending_requests = list(resp.pending_requests)

def _on_friend_chat_notify(self, notify):
    sender_id = notify.sender_id
    if sender_id not in self._friend_client.chat_history:
        self._friend_client.chat_history[sender_id] = []
    self._friend_client.chat_history[sender_id].append(notify)
```

### Step 3: Commit

```bash
git add scripts/client/friend_client.py scripts/client/game_scene.py
git commit -m "feat(friend): add FriendClient and wire friend callbacks in GameScene"
```

---

## Task 9: Client UI — FriendListPanel + FriendChatPanel

**Files:**
- Create: `scripts/client/ui/friend_list_panel.py`
- Create: `scripts/client/ui/friend_chat_panel.py`
- Modify: `scripts/client/constants.py`
- Modify: `scripts/client/ui/__init__.py`

### Step 1: Add friend UI constants to constants.py

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

### Step 2: Create friend_list_panel.py

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
    FRIEND_PANEL_TITLE_COLOR,
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

### Step 3: Create friend_chat_panel.py

```python
"""
私聊面板
显示聊天记录、输入框、表情按钮。
"""
import pygame
import logging
from typing import List, Any, Optional

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

### Step 4: Update ui/__init__.py

Add exports:

```python
from .friend_list_panel import FriendListPanel
from .friend_chat_panel import FriendChatPanel
```

### Step 5: Commit

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

### Step 1: Create friend_search_panel.py

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

### Step 2: Create friend_visit_panel.py

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

### Step 3: Commit

```bash
git add scripts/client/ui/friend_search_panel.py scripts/client/ui/friend_visit_panel.py
git commit -m "feat(friend): add FriendSearchPanel and FriendVisitPanel UI components"
```

---

## Task 11: Integration — Wire UI Panels into GameScene + Input Handling

**Files:**
- Modify: `scripts/client/game_scene.py`

### Step 1: Add friend panel instances to GameScene

In `GameScene.__init__`, after FriendClient creation:

```python
from .ui import FriendListPanel, FriendChatPanel, FriendSearchPanel, FriendVisitPanel

self._friend_list_panel = FriendListPanel(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)
self._friend_chat_panel = FriendChatPanel(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)
self._friend_search_panel = FriendSearchPanel(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)
self._friend_visit_panel = FriendVisitPanel(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)
```

### Step 2: Add friend keybinding (F key to toggle friend list)

In the event loop (wherever keyboard events are processed):

```python
if event.type == pygame.KEYDOWN:
    if event.key == pygame.K_f:
        self._friend_list_panel.toggle()
        if self._friend_list_panel.is_visible():
            self._friend_client.request_list()
```

### Step 3: Add mouse click handling for friend panels

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

### Step 4: Add friend panel rendering to game loop

In the render section of the game loop:

```python
self._friend_list_panel.render(screen, self._friend_client.friends, len(self._friend_client.pending_requests))
self._friend_chat_panel.render(screen, self._friend_client.chat_history.get(self._friend_chat_panel._target_id, []), self._friend_client._player_id)
self._friend_search_panel.render(screen)
self._friend_visit_panel.render(screen)
```

### Step 5: Add text input handling for chat/search

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

### Step 6: Commit

```bash
git add scripts/client/game_scene.py
git commit -m "feat(friend): integrate friend panels into GameScene with input handling"
```

---

## Self-Review Checklist

- [ ] **Spec coverage:** Every spec requirement has a corresponding task
  - Friend CRUD → Task 4 (done), forwarding → Task 6
  - Private chat → Task 5 (done), client UI → Task 9
  - Farm visit → Task 5 (done), client UI → Task 10
  - Client NetworkDispatcher → Task 7
  - FriendClient → Task 8
  - UI panels → Tasks 9, 10, 11
  - Game Server forwarding → Task 6

- [ ] **No placeholders:** All code is complete and compilable

- [ ] **Type consistency:** `FriendErrorCode`, `VisitAction`, `ChatMsgType` enums match proto definitions

- [ ] **Message ID consistency:** All `MSG_ID_FRIEND_*` constants used in handlers match `shared/message_ids.json`

- [ ] **Pattern consistency:** FriendServiceConnection follows DBMgrConnection pattern; NetworkDispatcher follows existing handler pattern
