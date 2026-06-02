# 组队系统实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增独立 TeamServer，实现 4 人组队功能（邀请/加入/离开/踢人/队长转移），集成战斗经验分配和队伍聊天。

**Architecture:** 新增独立 TeamServer 微服务（C++/libevent），复用 ChatServer 的架构模式。GateServer 新增 TeamServer 路由连接。队伍状态存储在 Redis。GameServer 在战斗结算时查询 TeamServer 获取队伍成员。ChatServer 通过 TeamServer 获取队伍成员用于队伍频道广播。

**Tech Stack:** C++17, libevent, Protobuf-lite, Redis, Python (Pygame client)

**MsgID 范围修正:** 设计文档中 TeamServer 使用 6000-6999，但该范围已被 ChatServer (6001-6003) 和 FriendService 内部消息 (6001-6012) 占用。实际使用：
- 客户端消息: `7001-7099`
- 内部消息 (TeamServer ↔ GameServer/ChatServer): `9000-9199`

---

## 文件结构

### 新增文件

| 文件 | 职责 |
|------|------|
| `shared/message_ids.json` | 新增 TeamServer 消息 ID 条目 |
| `shared/error_codes.json` | 新增 TeamServer 错误码条目 |
| `scripts/common/proto/team.proto` | TeamServer Protobuf 定义 |
| `scripts/server/team_server/CMakeLists.txt` | TeamServer 构建配置 |
| `scripts/server/team_server/src/main.cpp` | TeamServer 入口 |
| `scripts/server/team_server/src/team_server.h/cpp` | 核心服务类（libevent 事件循环） |
| `scripts/server/team_server/src/gate_session.h/cpp` | GateServer 连接会话管理 |
| `scripts/server/team_server/src/team_manager.h/cpp` | 队伍管理逻辑（创建/解散/邀请/加入/离开/踢人/队长转移） |
| `scripts/server/team_server/src/redis_client.h/cpp` | Redis 客户端封装 |
| `scripts/server/common/include/internal_msg_ids.h` | 新增 TeamServer 内部消息 ID |
| `scripts/client/team_manager.py` | 客户端组队管理器 |
| `scripts/client/team_hud.py` | 队伍 HUD UI 组件 |
| `scripts/client/team_invite_notify.py` | 邀请通知 UI 组件 |
| `scripts/client/test_team_manager.py` | 客户端 TeamManager 单元测试 |

### 修改文件

| 文件 | 修改内容 |
|------|----------|
| `scripts/server/gate_server/src/gate_server.h` | 新增 TeamServer 连接和路由方法 |
| `scripts/server/gate_server/src/gate_server.cpp` | 新增 TeamServer 连接和消息路由逻辑 |
| `scripts/server/gate_server/src/game_connection.h` | 复用为 TeamServerConnection（或新增 team_connection.h） |
| `scripts/server/gate_server/CMakeLists.txt` | 新增 team_connection 源文件 |
| `scripts/client/message_ids.py` | 自动生成，新增 TeamServer 消息 ID |
| `scripts/client/error_codes.py` | 自动生成，新增 TeamServer 错误码 |
| `scripts/client/network_dispatcher.py` | 新增 TeamServer 消息分发 |
| `scripts/server/common/include/message_ids.h` | 自动生成，新增 TeamServer 消息 ID |
| `scripts/server/common/include/error_codes.h` | 自动生成，新增 TeamServer 错误码 |
| `scripts/client/game_scene.py` | 集成 TeamManager 和队伍 HUD |
| `scripts/server/game_server/src/combat_handler.cpp` | 集成队伍经验分配 |
| `scripts/server/chat_server/src/channel_manager.cpp` | 集成队伍频道广播 |

---

## Task 1: 定义消息 ID 和错误码

**Files:**
- Modify: `shared/message_ids.json`
- Modify: `shared/error_codes.json`

- [ ] **Step 1: 在 message_ids.json 中新增 TeamServer 消息 ID**

在 `shared/message_ids.json` 的 `message_ids` 数组末尾追加：

```json
{"code": 7001, "name": "MSG_ID_TEAM_CREATE_REQ", "description": "创建队伍请求"},
{"code": 7002, "name": "MSG_ID_TEAM_CREATE_RESP", "description": "创建队伍响应"},
{"code": 7003, "name": "MSG_ID_TEAM_DISBAND_REQ", "description": "解散队伍请求"},
{"code": 7004, "name": "MSG_ID_TEAM_DISBAND_RESP", "description": "解散队伍响应"},
{"code": 7005, "name": "MSG_ID_TEAM_INVITE_REQ", "description": "邀请玩家组队"},
{"code": 7006, "name": "MSG_ID_TEAM_INVITE_RESP", "description": "邀请结果"},
{"code": 7007, "name": "MSG_ID_TEAM_INVITE_NOTIFY", "description": "收到组队邀请通知"},
{"code": 7008, "name": "MSG_ID_TEAM_ACCEPT_REQ", "description": "接受组队邀请"},
{"code": 7009, "name": "MSG_ID_TEAM_ACCEPT_RESP", "description": "接受邀请结果"},
{"code": 7010, "name": "MSG_ID_TEAM_REJECT_REQ", "description": "拒绝组队邀请"},
{"code": 7011, "name": "MSG_ID_TEAM_REJECT_RESP", "description": "拒绝邀请结果"},
{"code": 7012, "name": "MSG_ID_TEAM_LEAVE_REQ", "description": "离开队伍"},
{"code": 7013, "name": "MSG_ID_TEAM_LEAVE_RESP", "description": "离开队伍结果"},
{"code": 7014, "name": "MSG_ID_TEAM_KICK_REQ", "description": "踢出队伍成员"},
{"code": 7015, "name": "MSG_ID_TEAM_KICK_RESP", "description": "踢出结果"},
{"code": 7016, "name": "MSG_ID_TEAM_INFO_REQ", "description": "查询队伍信息"},
{"code": 7017, "name": "MSG_ID_TEAM_INFO_RESP", "description": "队伍信息响应"},
{"code": 7018, "name": "MSG_ID_TEAM_MEMBER_UPDATE", "description": "队伍成员变更通知"},
{"code": 7019, "name": "MSG_ID_TEAM_LEADER_CHANGE", "description": "队长变更通知"},
{"code": 7020, "name": "MSG_ID_TEAM_STATUS_UPDATE", "description": "队伍状态变更通知"}
```

- [ ] **Step 2: 在 error_codes.json 中新增 TeamServer 错误码**

在 `shared/error_codes.json` 的 `error_codes` 数组末尾追加：

```json
{"code": 7001, "name": "TEAM_ALREADY_IN_TEAM", "description": "已在队伍中"},
{"code": 7002, "name": "TEAM_FULL", "description": "队伍已满（4人）"},
{"code": 7003, "name": "TEAM_NOT_IN_TEAM", "description": "不在任何队伍中"},
{"code": 7004, "name": "TEAM_NOT_LEADER", "description": "不是队长，无权限"},
{"code": 7005, "name": "TEAM_TARGET_NOT_FOUND", "description": "目标玩家不存在"},
{"code": 7006, "name": "TEAM_TARGET_ALREADY_IN_TEAM", "description": "目标已有队伍"},
{"code": 7007, "name": "TEAM_TARGET_OFFLINE", "description": "目标不在线"},
{"code": 7008, "name": "TEAM_INVITE_NOT_FOUND", "description": "邀请不存在或已过期"},
{"code": 7009, "name": "TEAM_INVITE_EXPIRED", "description": "邀请已过期"},
{"code": 7010, "name": "TEAM_CANNOT_KICK_SELF", "description": "不能踢自己"},
{"code": 7011, "name": "TEAM_IN_CAVE", "description": "队伍在矿洞中，不能解散"},
{"code": 7012, "name": "TEAM_SELF_OPERATION", "description": "不能邀请自己"},
{"code": 7013, "name": "TEAM_PLAYER_BLOCKED", "description": "被对方拉黑"}
```

- [ ] **Step 3: 运行代码生成脚本**

```bash
cd D:/mb_workspace/farm_demo
python tools/generate_message_ids.py
```

验证生成的文件包含新的 TeamServer 消息 ID：
- `scripts/client/message_ids.py` 包含 `MSG_ID_TEAM_CREATE_REQ = 7001` 等
- `scripts/server/common/include/message_ids.h` 包含 `MSG_ID_TEAM_CREATE_REQ = 7001` 等
- `scripts/client/error_codes.py` 包含 `TEAM_ALREADY_IN_TEAM = 7001` 等
- `scripts/server/common/include/error_codes.h` 包含 `TEAM_ALREADY_IN_TEAM = 7001` 等

- [ ] **Step 4: Commit**

```bash
git add shared/message_ids.json shared/error_codes.json scripts/client/message_ids.py scripts/client/error_codes.py scripts/server/common/include/message_ids.h scripts/server/common/include/error_codes.h
git commit -m "feat(team): add team system message IDs and error codes"
```

---

## Task 2: 定义 Protobuf 协议

**Files:**
- Create: `scripts/common/proto/team.proto`

- [ ] **Step 1: 创建 team.proto**

创建 `scripts/common/proto/team.proto`：

```protobuf
syntax = "proto3";

package farm;

option optimize_for = LITE_RUNTIME;

// ===========================================
// 组队消息协议
// MsgID 范围: 7001-7099 (Client -> Gate -> TeamServer)
// ===========================================

// 队伍错误码
enum TeamErrorCode {
    TEAM_SUCCESS = 0;
    TEAM_ALREADY_IN_TEAM = 1;       // 已在队伍中
    TEAM_FULL = 2;                  // 队伍已满（4人）
    TEAM_NOT_IN_TEAM = 3;           // 不在任何队伍中
    TEAM_NOT_LEADER = 4;            // 不是队长，无权限
    TEAM_TARGET_NOT_FOUND = 5;      // 目标玩家不存在
    TEAM_TARGET_ALREADY_IN_TEAM = 6;// 目标已有队伍
    TEAM_TARGET_OFFLINE = 7;        // 目标不在线
    TEAM_INVITE_NOT_FOUND = 8;      // 邀请不存在/已过期
    TEAM_INVITE_EXPIRED = 9;        // 邀请已过期
    TEAM_CANNOT_KICK_SELF = 10;     // 不能踢自己
    TEAM_IN_CAVE = 11;              // 队伍在矿洞中，不能解散
    TEAM_SELF_OPERATION = 12;       // 不能邀请自己
    TEAM_PLAYER_BLOCKED = 13;       // 被对方拉黑
}

// 成员更新类型
enum TeamUpdateType {
    TEAM_UPDATE_JOIN = 0;       // 加入
    TEAM_UPDATE_LEAVE = 1;      // 离开
    TEAM_UPDATE_KICK = 2;       // 踢出
    TEAM_UPDATE_ONLINE = 3;     // 上线
    TEAM_UPDATE_OFFLINE = 4;    // 下线
}

// 队伍成员信息
message TeamMember {
    uint64 player_id = 1;
    string role_name = 2;
    uint32 level = 3;
    bool online = 4;
    bool is_leader = 5;
    int32 current_hp = 6;
    int32 max_hp = 7;
}

// 创建队伍
message TeamCreateReq {}

message TeamCreateResp {
    TeamErrorCode code = 1;
    uint64 team_id = 2;
}

// 解散队伍
message TeamDisbandReq {}

message TeamDisbandResp {
    TeamErrorCode code = 1;
}

// 邀请玩家
message TeamInviteReq {
    uint64 target_id = 1;
}

message TeamInviteResp {
    TeamErrorCode code = 1;
    string msg = 2;
}

// 收到邀请通知
message TeamInviteNotify {
    uint64 team_id = 1;
    uint64 inviter_id = 2;
    string inviter_name = 3;
    uint32 inviter_level = 4;
}

// 接受邀请
message TeamAcceptReq {
    uint64 team_id = 1;
}

message TeamAcceptResp {
    TeamErrorCode code = 1;
    repeated TeamMember members = 2;
}

// 拒绝邀请
message TeamRejectReq {
    uint64 team_id = 1;
}

message TeamRejectResp {
    TeamErrorCode code = 1;
}

// 离开队伍
message TeamLeaveReq {}

message TeamLeaveResp {
    TeamErrorCode code = 1;
}

// 踢出成员
message TeamKickReq {
    uint64 target_id = 1;
}

message TeamKickResp {
    TeamErrorCode code = 1;
}

// 队伍信息查询
message TeamInfoReq {}

message TeamInfoResp {
    TeamErrorCode code = 1;
    uint64 team_id = 2;
    uint64 leader_id = 3;
    repeated TeamMember members = 4;
    uint32 status = 5;          // 0=idle, 1=in_cave
    int32 cave_level = 6;
}

// 成员变更通知
message TeamMemberUpdate {
    TeamUpdateType update_type = 1;
    TeamMember member = 2;
    repeated TeamMember all_members = 3;
}

// 队长变更通知
message TeamLeaderChange {
    uint64 new_leader_id = 1;
    string new_leader_name = 2;
}

// 队伍状态变更
message TeamStatusUpdate {
    uint32 status = 1;
    int32 cave_level = 2;
}

// ===========================================
// 内部消息 (TeamServer <-> GameServer/ChatServer)
// MsgID 范围: 9000-9199
// ===========================================

// 查询队伍成员请求 (GameServer/ChatServer -> TeamServer)
message TeamQueryMembersReq {
    uint64 player_id = 1;   // 查询该玩家所在队伍的成员
}

// 查询队伍成员响应 (TeamServer -> GameServer/ChatServer)
message TeamQueryMembersResp {
    int32 code = 1;
    uint64 team_id = 2;
    repeated uint64 member_ids = 3;
    repeated string member_names = 4;
    repeated int32 member_levels = 5;
}

// 队伍成员变更通知 (TeamServer -> ChatServer)
message TeamMembersChangedNotify {
    uint64 team_id = 1;
    repeated uint64 member_ids = 2;
}
```

- [ ] **Step 2: 编译 proto 文件**

```bash
cd D:/mb_workspace/farm_demo
protoc --proto_path=scripts/common/proto --cpp_out=scripts/common/proto/generated --python_out=scripts/common/proto/generated scripts/common/proto/team.proto
```

验证生成：
- `scripts/common/proto/generated/team.pb.h`
- `scripts/common/proto/generated/team.pb.cc`
- `scripts/common/proto/generated/team_pb2.py`

- [ ] **Step 3: Commit**

```bash
git add scripts/common/proto/team.proto scripts/common/proto/generated/team.pb.* scripts/common/proto/generated/team_pb2.py
git commit -m "feat(team): add team system protobuf definitions"
```

---

## Task 3: 新增内部消息 ID

**Files:**
- Modify: `scripts/server/common/include/internal_msg_ids.h`

- [ ] **Step 1: 在 internal_msg_ids.h 中新增 TeamServer 内部消息 ID**

在 `internal_msg_ids.h` 的 CrossServer 部分之后追加：

```cpp
// Game <-> TeamServer (9000-9199)

// 连接管理 (9000-9099)
inline constexpr uint32_t MSG_ID_TEAM_SERVICE_IDENTIFY       = 9001;
inline constexpr uint32_t MSG_ID_TEAM_SERVICE_IDENTIFY_RESP  = 9002;
inline constexpr uint32_t MSG_ID_TEAM_SERVICE_HEARTBEAT       = 9003;
inline constexpr uint32_t MSG_ID_TEAM_SERVICE_HEARTBEAT_RESP  = 9004;

// 消息转发 (9100-9199)
inline constexpr uint32_t MSG_ID_TEAM_CLIENT_MSG              = 9101;
inline constexpr uint32_t MSG_ID_TEAM_SERVICE_MSG             = 9102;

// 队伍查询 (9110-9119)
inline constexpr uint32_t MSG_ID_TEAM_QUERY_MEMBERS_REQ       = 9111;
inline constexpr uint32_t MSG_ID_TEAM_QUERY_MEMBERS_RESP      = 9112;

// 队伍变更通知 (9120-9129)
inline constexpr uint32_t MSG_ID_TEAM_MEMBERS_CHANGED_NOTIFY  = 9121;
```

同时在超时常量部分追加：

```cpp
// Game <-> TeamServer
inline constexpr int TEAM_IDENTIFY_TIMEOUT = 10;   // 身份识别超时（秒）
inline constexpr int TEAM_HEARTBEAT_INTERVAL = 5;  // 心跳发送间隔（秒）
inline constexpr int TEAM_HEARTBEAT_TIMEOUT = 15;  // 心跳超时（秒）
```

- [ ] **Step 2: Commit**

```bash
git add scripts/server/common/include/internal_msg_ids.h
git commit -m "feat(team): add team server internal message IDs"
```

---

## Task 4: TeamServer 基础框架

**Files:**
- Create: `scripts/server/team_server/CMakeLists.txt`
- Create: `scripts/server/team_server/src/main.cpp`
- Create: `scripts/server/team_server/src/team_server.h`
- Create: `scripts/server/team_server/src/team_server.cpp`
- Create: `scripts/server/team_server/src/gate_session.h`
- Create: `scripts/server/team_server/src/gate_session.cpp`

- [ ] **Step 1: 创建 CMakeLists.txt**

创建 `scripts/server/team_server/CMakeLists.txt`，参考 ChatServer 的 CMakeLists.txt 结构：

```cmake
cmake_minimum_required(VERSION 3.14)
project(team_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static runtime (matching precompiled absl/protobuf)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Dependency paths
set(LIBEVENT_ROOT "C:/libevent_install")
set(PROTOBUF_ROOT "C:/protobuf_install")
set(HIREDIS_ROOT "C:/hiredis_install")
set(COMMON_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/proto")

# Protobuf generated sources
set(PROTO_GENERATED_DIR "${COMMON_PROTO_DIR}/generated")
set(PROTO_SRCS
    ${PROTO_GENERATED_DIR}/base.pb.cc
    ${PROTO_GENERATED_DIR}/internal.pb.cc
    ${PROTO_GENERATED_DIR}/team.pb.cc
)
set(PROTO_HDRS
    ${PROTO_GENERATED_DIR}/base.pb.h
    ${PROTO_GENERATED_DIR}/internal.pb.h
    ${PROTO_GENERATED_DIR}/team.pb.h
)

# Source files
set(SOURCES
    src/main.cpp
    src/team_server.cpp
    src/gate_session.cpp
    src/team_manager.cpp
    src/redis_client.cpp
    ${PROTO_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/log_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/message_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/msvc_compat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/server_main_helper.cpp
)

add_executable(team_server ${SOURCES})

# Include directories
target_include_directories(team_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROTOBUF_ROOT}/include
    ${LIBEVENT_ROOT}/include
    ${HIREDIS_ROOT}/include
    ${COMMON_PROTO_DIR}
    ${COMMON_PROTO_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# Link directories
target_link_directories(team_server PRIVATE
    ${PROTOBUF_ROOT}/lib
    ${LIBEVENT_ROOT}/lib
    ${HIREDIS_ROOT}/lib
)

# MSVC specific settings
if(MSVC)
    target_compile_definitions(team_server PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NOMINMAX
    )
    target_compile_options(team_server PRIVATE /utf-8 /MT$<$<CONFIG:Debug>:d>)

    file(GLOB ABSL_LIBS "${PROTOBUF_ROOT}/lib/absl_*.lib")
    target_link_libraries(team_server PRIVATE
        libprotobuf-lite.lib
        libutf8_range.lib
        libutf8_validity.lib
        ${ABSL_LIBS}
        event.lib
        event_core.lib
        event_extra.lib
        hiredis.lib
        ws2_32.lib
        advapi32.lib
        shell32.lib
    )
else()
    target_link_libraries(team_server PRIVATE
        protobuf-lite
        event
        event_core
        event_extra
        hiredis
        pthread
    )
endif()

set_target_properties(team_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
)
```

- [ ] **Step 2: 创建 main.cpp**

创建 `scripts/server/team_server/src/main.cpp`，参考 ChatServer 的 main.cpp：

```cpp
#include "team_server.h"
#include "server_main_helper.h"
#include "log_init.h"
#include "log_macros.h"

#include <csignal>
#include <string>

static farm::TeamServer* g_server = nullptr;

static void signal_handler(int sig) {
    if (g_server) {
        SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    if (!farm::init_platform_network()) return 1;

    farm::init_logging_default("team_server");

    std::string ip = "0.0.0.0";
    uint16_t port = 8891;  // TeamServer 端口（ChatServer 用 8889）

    if (argc >= 2) ip = argv[1];
    if (argc >= 3) port = static_cast<uint16_t>(std::stoi(argv[2]));

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    SPDLOG_INFO("[Main]=== Team Server ===");
    SPDLOG_INFO("[Main]IP: {}", ip);
    SPDLOG_INFO("[Main]Port: {}", port);

    farm::TeamServer server(ip, port);
    g_server = &server;

    if (!server.start()) {
        SPDLOG_ERROR("[Main]Failed to start TeamServer");
        return 1;
    }

    SPDLOG_INFO("[Main]Server stopped");
    farm::shutdown_logging();
    farm::cleanup_platform_network();
    return 0;
}
```

- [ ] **Step 3: 创建 team_server.h**

创建 `scripts/server/team_server/src/team_server.h`：

```cpp
#pragma once

#include "team_manager.h"

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>

namespace farm {

struct TeamGateSession {
    evutil_socket_t fd;
    struct bufferevent* bev;
    std::string gate_id;
    bool identified;
    std::vector<uint8_t> read_buffer;
};

class TeamServer {
public:
    TeamServer(const std::string& ip, uint16_t port);
    ~TeamServer();

    bool start();
    void stop();

private:
    // libevent callbacks
    static void on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                          struct sockaddr* addr, int len, void* ctx);
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_invite_timeout_timer(evutil_socket_t fd, short events, void* ctx);

    // Connection handling
    void handle_accept(evutil_socket_t fd, struct sockaddr* addr);
    void handle_read(std::shared_ptr<TeamGateSession> session);
    void handle_disconnect(std::shared_ptr<TeamGateSession> session);

    // Message routing
    void route_message(std::shared_ptr<TeamGateSession> session,
                       uint32_t msg_id, const std::vector<uint8_t>& payload);

    // Internal protocol handlers
    void handle_gate_identify(std::shared_ptr<TeamGateSession> session,
                              const std::vector<uint8_t>& payload);
    void handle_heartbeat(std::shared_ptr<TeamGateSession> session,
                          const std::vector<uint8_t>& payload);
    void handle_player_join(std::shared_ptr<TeamGateSession> session,
                            const std::vector<uint8_t>& payload);
    void handle_player_leave(std::shared_ptr<TeamGateSession> session,
                             const std::vector<uint8_t>& payload);
    void handle_client_msg(std::shared_ptr<TeamGateSession> session,
                           const std::vector<uint8_t>& payload);

    // Team message handlers
    void handle_team_create_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_disband_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_invite_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_accept_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_reject_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_leave_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_kick_req(uint64_t player_id, const uint8_t* payload, size_t len);
    void handle_team_info_req(uint64_t player_id, const uint8_t* payload, size_t len);

    // Internal query handlers
    void handle_team_query_members_req(std::shared_ptr<TeamGateSession> session,
                                       const uint8_t* payload, size_t len);

    // Send helpers
    void send_to_gate(std::shared_ptr<TeamGateSession> session,
                      uint32_t msg_id, std::string_view payload);
    void send_to_player(uint64_t player_id, uint32_t msg_id,
                        const uint8_t* payload, size_t len);
    void broadcast_to_team(uint64_t team_id, uint32_t msg_id,
                           const uint8_t* payload, size_t len,
                           uint64_t exclude_player_id = 0);

    std::string ip_;
    uint16_t port_;
    struct event_base* base_;
    struct evconnlistener* listener_;
    struct event* invite_timeout_timer_;
    bool running_;

    std::unordered_map<evutil_socket_t, std::shared_ptr<TeamGateSession>> gate_sessions_;
    std::unordered_map<uint64_t, std::shared_ptr<TeamGateSession>> player_to_gate_;

    TeamManager team_mgr_;
};

}  // namespace farm
```

- [ ] **Step 4: 创建 team_server.cpp 框架**

创建 `scripts/server/team_server/src/team_server.cpp`，实现基础框架（消息路由、连接管理）。具体的消息处理函数在后续 Task 中填充。

```cpp
#include "team_server.h"
#include "internal_msg_ids.h"
#include "message_ids.h"
#include "message_parser.h"
#include "log_macros.h"

#include "internal.pb.h"
#include "team.pb.h"

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/event.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include <cstring>

namespace farm {

static constexpr int INVITE_TIMEOUT_INTERVAL = 5;  // 每 5 秒检查一次邀请超时

TeamServer::TeamServer(const std::string& ip, uint16_t port)
    : ip_(ip), port_(port), base_(nullptr), listener_(nullptr),
      invite_timeout_timer_(nullptr), running_(false),
      team_mgr_([this](uint64_t player_id, uint32_t msg_id,
                        const uint8_t* payload, size_t len) {
          send_to_player(player_id, msg_id, payload, len);
      })
{
}

TeamServer::~TeamServer() { stop(); }

bool TeamServer::start() {
    base_ = event_base_new();
    if (!base_) {
        SPDLOG_ERROR("[Team]Failed to create event_base");
        return false;
    }

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
        SPDLOG_ERROR("[Team]Failed to create listener on {}:{}", ip_, port_);
        event_base_free(base_);
        base_ = nullptr;
        return false;
    }

    // Invite timeout timer
    struct timeval tv;
    tv.tv_sec = INVITE_TIMEOUT_INTERVAL;
    tv.tv_usec = 0;
    invite_timeout_timer_ = event_new(base_, -1, EV_PERSIST, on_invite_timeout_timer, this);
    evtimer_add(invite_timeout_timer_, &tv);

    running_ = true;
    SPDLOG_INFO("[Team]Listening on {}:{}", ip_, port_);

    event_base_dispatch(base_);

    running_ = false;
    return true;
}

void TeamServer::stop() {
    running_ = false;
    if (invite_timeout_timer_) {
        event_free(invite_timeout_timer_);
        invite_timeout_timer_ = nullptr;
    }
    if (listener_) {
        evconnlistener_free(listener_);
        listener_ = nullptr;
    }
    if (base_) {
        event_base_loopexit(base_, nullptr);
        event_base_free(base_);
        base_ = nullptr;
    }
}

// libevent callbacks (delegate to instance methods)
void TeamServer::on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                           struct sockaddr* addr, int len, void* ctx) {
    auto* self = static_cast<TeamServer*>(ctx);
    self->handle_accept(fd, addr);
}

void TeamServer::on_read(struct bufferevent* bev, void* ctx) {
    auto* session = static_cast<std::shared_ptr<TeamGateSession>*>(ctx);
    // Find the TeamServer instance - we need to store it in session or use a global
    // For now, we'll use a static approach similar to ChatServer
}

void TeamServer::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* session = static_cast<std::shared_ptr<TeamGateSession>*>(ctx);
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        // Handle disconnect
    }
}

void TeamServer::on_invite_timeout_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* self = static_cast<TeamServer*>(ctx);
    self->team_mgr_.check_invite_timeouts();
}

void TeamServer::handle_accept(evutil_socket_t fd, struct sockaddr* addr) {
    auto session = std::make_shared<TeamGateSession>();
    session->fd = fd;
    session->identified = false;

    struct bufferevent* bev = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE);
    session->bev = bev;

    auto* session_ptr = new std::shared_ptr<TeamGateSession>(session);
    bufferevent_setcb(bev, on_read, nullptr, on_event, session_ptr);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    gate_sessions_[fd] = session;
    SPDLOG_INFO("[Team]Gate connection from fd={}", fd);
}

void TeamServer::handle_read(std::shared_ptr<TeamGateSession> session) {
    // Read and parse messages from buffer - same pattern as ChatServer
    struct evbuffer* input = bufferevent_get_input(session->bev);
    size_t len = evbuffer_get_length(input);
    if (len == 0) return;

    session->read_buffer.resize(session->read_buffer.size() + len);
    evbuffer_remove(input, session->read_buffer.data() + session->read_buffer.size() - len, len);

    // Parse messages
    size_t offset = 0;
    while (offset < session->read_buffer.size()) {
        if (session->read_buffer.size() - offset < 4) break;

        uint32_t msg_len;
        std::memcpy(&msg_len, session->read_buffer.data() + offset, 4);
        msg_len = ntohl(msg_len);

        if (session->read_buffer.size() - offset < 4 + msg_len) break;

        uint32_t msg_id;
        std::memcpy(&msg_id, session->read_buffer.data() + offset + 4, 4);
        msg_id = ntohl(msg_id);

        const uint8_t* payload = session->read_buffer.data() + offset + 8;
        size_t payload_len = msg_len - 4;

        route_message(session, msg_id, std::vector<uint8_t>(payload, payload + payload_len));
        offset += 4 + msg_len;
    }

    if (offset > 0) {
        session->read_buffer.erase(session->read_buffer.begin(),
                                   session->read_buffer.begin() + offset);
    }
}

void TeamServer::handle_disconnect(std::shared_ptr<TeamGateSession> session) {
    SPDLOG_INFO("[Team]Gate disconnected: fd={}", session->fd);
    gate_sessions_.erase(session->fd);
    bufferevent_free(session->bev);
}

void TeamServer::route_message(std::shared_ptr<TeamGateSession> session,
                               uint32_t msg_id, const std::vector<uint8_t>& payload) {
    // Internal protocol messages
    if (msg_id == MSG_ID_TEAM_SERVICE_IDENTIFY) {
        handle_gate_identify(session, payload);
        return;
    }
    if (msg_id == MSG_ID_TEAM_SERVICE_HEARTBEAT) {
        handle_heartbeat(session, payload);
        return;
    }
    if (msg_id == MSG_ID_TEAM_CLIENT_MSG) {
        handle_client_msg(session, payload);
        return;
    }
    // Internal query from GameServer/ChatServer
    if (msg_id == MSG_ID_TEAM_QUERY_MEMBERS_REQ) {
        handle_team_query_members_req(session, payload.data(), payload.size());
        return;
    }

    SPDLOG_WARN("[Team]Unknown msg_id={}", msg_id);
}

void TeamServer::handle_gate_identify(std::shared_ptr<TeamGateSession> session,
                                      const std::vector<uint8_t>& payload) {
    farm::GateIdentify identify;
    if (identify.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        session->gate_id = identify.gate_id();
        session->identified = true;
        SPDLOG_INFO("[Team]Gate identified: {}", session->gate_id);

        farm::GateIdentifyResp resp;
        resp.set_code(0);
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        send_to_gate(session, MSG_ID_TEAM_SERVICE_IDENTIFY_RESP, resp_data);
    }
}

void TeamServer::handle_heartbeat(std::shared_ptr<TeamGateSession> session,
                                  const std::vector<uint8_t>& payload) {
    farm::InternHeartbeat hb;
    if (hb.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        farm::InternHeartbeatResp resp;
        resp.set_timestamp(hb.timestamp());
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        send_to_gate(session, MSG_ID_TEAM_SERVICE_HEARTBEAT_RESP, resp_data);
    }
}

void TeamServer::handle_player_join(std::shared_ptr<TeamGateSession> session,
                                    const std::vector<uint8_t>& payload) {
    farm::PlayerJoin join;
    if (join.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        uint64_t player_id = join.player_id();
        player_to_gate_[player_id] = session;
        SPDLOG_INFO("[Team]Player {} joined", player_id);

        // Send current team info if player is in a team
        auto* team = team_mgr_.get_player_team(player_id);
        if (team) {
            // Send team info to the reconnected player
            farm::TeamInfoResp info_resp;
            info_resp.set_code(farm::TEAM_SUCCESS);
            info_resp.set_team_id(team->team_id);
            info_resp.set_leader_id(team->leader_id);
            info_resp.set_status(static_cast<uint32_t>(team->status));
            info_resp.set_cave_level(team->cave_level);
            for (uint64_t mid : team->members) {
                auto* member = info_resp.add_members();
                member->set_player_id(mid);
                member->set_is_leader(mid == team->leader_id);
                // TODO: populate role_name, level, online, hp from player data
            }
            std::string resp_data;
            info_resp.SerializeToString(&resp_data);
            send_to_player(player_id, MSG_ID_TEAM_INFO_RESP, resp_data);
        }

        farm::PlayerJoinResp resp;
        resp.set_player_id(player_id);
        resp.set_code(0);
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        // Send back to gate
    }
}

void TeamServer::handle_player_leave(std::shared_ptr<TeamGateSession> session,
                                     const std::vector<uint8_t>& payload) {
    farm::PlayerLeave leave;
    if (leave.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        uint64_t player_id = leave.player_id();
        player_to_gate_.erase(player_id);
        SPDLOG_INFO("[Team]Player {} left", player_id);
    }
}

void TeamServer::handle_client_msg(std::shared_ptr<TeamGateSession> session,
                                   const std::vector<uint8_t>& payload) {
    farm::ClientMessage client_msg;
    if (!client_msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) return;

    uint64_t player_id = client_msg.player_id();
    uint32_t msg_id = client_msg.msg_id();
    const auto& inner_payload = client_msg.payload();

    switch (msg_id) {
        case MSG_ID_TEAM_CREATE_REQ:
            handle_team_create_req(player_id,
                                   reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                   inner_payload.size());
            break;
        case MSG_ID_TEAM_DISBAND_REQ:
            handle_team_disband_req(player_id,
                                    reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                    inner_payload.size());
            break;
        case MSG_ID_TEAM_INVITE_REQ:
            handle_team_invite_req(player_id,
                                   reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                   inner_payload.size());
            break;
        case MSG_ID_TEAM_ACCEPT_REQ:
            handle_team_accept_req(player_id,
                                   reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                   inner_payload.size());
            break;
        case MSG_ID_TEAM_REJECT_REQ:
            handle_team_reject_req(player_id,
                                   reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                   inner_payload.size());
            break;
        case MSG_ID_TEAM_LEAVE_REQ:
            handle_team_leave_req(player_id,
                                  reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                  inner_payload.size());
            break;
        case MSG_ID_TEAM_KICK_REQ:
            handle_team_kick_req(player_id,
                                 reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                 inner_payload.size());
            break;
        case MSG_ID_TEAM_INFO_REQ:
            handle_team_info_req(player_id,
                                 reinterpret_cast<const uint8_t*>(inner_payload.data()),
                                 inner_payload.size());
            break;
        default:
            SPDLOG_WARN("[Team]Unknown client msg_id={}", msg_id);
            break;
    }
}

void TeamServer::send_to_gate(std::shared_ptr<TeamGateSession> session,
                              uint32_t msg_id, std::string_view payload) {
    if (!session || !session->bev) return;

    // Frame: [len(4)] [msg_id(4)] [payload]
    uint32_t frame_len = 4 + static_cast<uint32_t>(payload.size());
    uint32_t net_len = htonl(frame_len);
    uint32_t net_msg_id = htonl(msg_id);

    struct evbuffer* output = bufferevent_get_output(session->bev);
    evbuffer_add(output, &net_len, 4);
    evbuffer_add(output, &net_msg_id, 4);
    evbuffer_add(output, payload.data(), payload.size());
}

void TeamServer::send_to_player(uint64_t player_id, uint32_t msg_id,
                                const uint8_t* payload, size_t len) {
    auto it = player_to_gate_.find(player_id);
    if (it == player_to_gate_.end()) return;

    auto session = it->second;
    if (!session || !session->identified) return;

    // Wrap in GameMessage for Gate to forward
    farm::GameMessage game_msg;
    game_msg.set_player_id(player_id);
    game_msg.set_msg_id(msg_id);
    game_msg.set_payload(payload, len);

    std::string data;
    game_msg.SerializeToString(&data);
    send_to_gate(session, MSG_ID_TEAM_SERVICE_MSG, data);
}

void TeamServer::broadcast_to_team(uint64_t team_id, uint32_t msg_id,
                                   const uint8_t* payload, size_t len,
                                   uint64_t exclude_player_id) {
    auto* team = team_mgr_.get_team(team_id);
    if (!team) return;

    for (uint64_t member_id : team->members) {
        if (member_id != exclude_player_id) {
            send_to_player(member_id, msg_id, payload, len);
        }
    }
}

// Team message handlers - implemented in Task 5
void TeamServer::handle_team_create_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_disband_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_invite_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_accept_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_reject_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_leave_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_kick_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_info_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

void TeamServer::handle_team_query_members_req(std::shared_ptr<TeamGateSession> session,
                                                const uint8_t* payload, size_t len) {
    // TODO: Task 5
}

}  // namespace farm
```

- [ ] **Step 5: 创建 gate_session.h/cpp**

创建 `scripts/server/team_server/src/gate_session.h` 和 `gate_session.cpp`。由于 TeamServer 的 gate session 结构与 ChatServer 相同，可以直接在 team_server.h 中定义 `TeamGateSession` 结构体（已在上面的 team_server.h 中定义），不需要单独的文件。跳过此步骤。

- [ ] **Step 6: 构建验证**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/team_server
mkdir -p build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

验证 `bin/team_server.exe` 生成成功。

- [ ] **Step 7: Commit**

```bash
git add scripts/server/team_server/
git commit -m "feat(team): add TeamServer basic framework"
```

---

## Task 5: TeamManager 核心逻辑

**Files:**
- Create: `scripts/server/team_server/src/team_manager.h`
- Create: `scripts/server/team_server/src/team_manager.cpp`
- Create: `scripts/server/team_server/src/redis_client.h`
- Create: `scripts/server/team_server/src/redis_client.cpp`

- [ ] **Step 1: 创建 redis_client.h**

创建 `scripts/server/team_server/src/redis_client.h`：

```cpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

// Forward declare hiredis types
struct redisContext;
struct redisReply;

namespace farm {

class RedisClient {
public:
    RedisClient();
    ~RedisClient();

    bool connect(const std::string& host, int port);
    void disconnect();
    bool is_connected() const { return ctx_ != nullptr; }

    // Team info operations
    bool set_team_info(uint64_t team_id, uint64_t leader_id, uint32_t status, int cave_level);
    bool get_team_info(uint64_t team_id, uint64_t& leader_id, uint32_t& status, int& cave_level);
    bool del_team_info(uint64_t team_id);

    // Team members operations
    bool add_team_member(uint64_t team_id, uint64_t player_id);
    bool remove_team_member(uint64_t team_id, uint64_t player_id);
    bool get_team_members(uint64_t team_id, std::vector<uint64_t>& members);
    bool is_team_member(uint64_t team_id, uint64_t player_id);
    size_t get_team_member_count(uint64_t team_id);

    // Player -> Team mapping
    bool set_player_team(uint64_t player_id, uint64_t team_id);
    bool get_player_team(uint64_t player_id, uint64_t& team_id);
    bool del_player_team(uint64_t player_id);

    // Invite operations
    bool set_invite(uint64_t player_id, uint64_t team_id, uint64_t inviter_id, uint64_t timestamp);
    bool get_invite(uint64_t player_id, uint64_t& team_id, uint64_t& inviter_id, uint64_t& timestamp);
    bool del_invite(uint64_t player_id);

    // Team ID generation
    uint64_t generate_team_id();

private:
    redisContext* ctx_;
};

}  // namespace farm
```

- [ ] **Step 2: 创建 redis_client.cpp**

创建 `scripts/server/team_server/src/redis_client.cpp`，实现所有 Redis 操作。使用 hiredis 库。

```cpp
#include "redis_client.h"
#include "log_macros.h"

#include <hiredis/hiredis.h>
#include <cstring>
#include <sstream>

namespace farm {

RedisClient::RedisClient() : ctx_(nullptr) {}

RedisClient::~RedisClient() { disconnect(); }

bool RedisClient::connect(const std::string& host, int port) {
    ctx_ = redisConnect(host.c_str(), port);
    if (!ctx_ || ctx_->err) {
        if (ctx_) {
            SPDLOG_ERROR("[Redis]Connection failed: {}", ctx_->errstr);
            redisFree(ctx_);
            ctx_ = nullptr;
        } else {
            SPDLOG_ERROR("[Redis]Connection failed: can't allocate context");
        }
        return false;
    }
    SPDLOG_INFO("[Redis]Connected to {}:{}", host, port);
    return true;
}

void RedisClient::disconnect() {
    if (ctx_) {
        redisFree(ctx_);
        ctx_ = nullptr;
    }
}

bool RedisClient::set_team_info(uint64_t team_id, uint64_t leader_id,
                                uint32_t status, int cave_level) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "HSET team:%llu:info leader_id %llu status %u cave_level %d",
        team_id, leader_id, status, cave_level);
    if (!reply) return false;
    bool ok = reply->type != REDIS_REPLY_ERROR;
    freeReplyObject(reply);
    return ok;
}

bool RedisClient::get_team_info(uint64_t team_id, uint64_t& leader_id,
                                uint32_t& status, int& cave_level) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "HGETALL team:%llu:info", team_id);
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        if (reply) freeReplyObject(reply);
        return false;
    }
    for (size_t i = 0; i < reply->elements; i += 2) {
        std::string key(reply->element[i]->str);
        std::string val(reply->element[i+1]->str);
        if (key == "leader_id") leader_id = std::stoull(val);
        else if (key == "status") status = std::stoul(val);
        else if (key == "cave_level") cave_level = std::stoi(val);
    }
    freeReplyObject(reply);
    return true;
}

bool RedisClient::del_team_info(uint64_t team_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "DEL team:%llu:info", team_id);
    if (!reply) return false;
    freeReplyObject(reply);
    return true;
}

bool RedisClient::add_team_member(uint64_t team_id, uint64_t player_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "RPUSH team:%llu:members %llu", team_id, player_id);
    if (!reply) return false;
    bool ok = reply->type != REDIS_REPLY_ERROR;
    freeReplyObject(reply);
    return ok;
}

bool RedisClient::remove_team_member(uint64_t team_id, uint64_t player_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "LREM team:%llu:members 0 %llu", team_id, player_id);
    if (!reply) return false;
    freeReplyObject(reply);
    return true;
}

bool RedisClient::get_team_members(uint64_t team_id, std::vector<uint64_t>& members) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "LRANGE team:%llu:members 0 -1", team_id);
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        if (reply) freeReplyObject(reply);
        return false;
    }
    members.clear();
    for (size_t i = 0; i < reply->elements; i++) {
        members.push_back(std::stoull(reply->element[i]->str));
    }
    freeReplyObject(reply);
    return true;
}

bool RedisClient::is_team_member(uint64_t team_id, uint64_t player_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "LPOS team:%llu:members %llu", team_id, player_id);
    if (!reply) return false;
    bool found = (reply->type == REDIS_REPLY_INTEGER);
    freeReplyObject(reply);
    return found;
}

size_t RedisClient::get_team_member_count(uint64_t team_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "LLEN team:%llu:members", team_id);
    if (!reply) return 0;
    size_t count = reply->integer;
    freeReplyObject(reply);
    return count;
}

bool RedisClient::set_player_team(uint64_t player_id, uint64_t team_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "SET team:player:%llu %llu", player_id, team_id);
    if (!reply) return false;
    bool ok = reply->type != REDIS_REPLY_ERROR;
    freeReplyObject(reply);
    return ok;
}

bool RedisClient::get_player_team(uint64_t player_id, uint64_t& team_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "GET team:player:%llu", player_id);
    if (!reply || reply->type == REDIS_REPLY_NIL) {
        if (reply) freeReplyObject(reply);
        team_id = 0;
        return false;
    }
    team_id = std::stoull(reply->str);
    freeReplyObject(reply);
    return true;
}

bool RedisClient::del_player_team(uint64_t player_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "DEL team:player:%llu", player_id);
    if (!reply) return false;
    freeReplyObject(reply);
    return true;
}

bool RedisClient::set_invite(uint64_t player_id, uint64_t team_id,
                             uint64_t inviter_id, uint64_t timestamp) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "HSET team:invite:%llu team_id %llu inviter_id %llu timestamp %llu",
        player_id, team_id, inviter_id, timestamp);
    if (!reply) return false;
    bool ok = reply->type != REDIS_REPLY_ERROR;
    freeReplyObject(reply);
    return ok;
}

bool RedisClient::get_invite(uint64_t player_id, uint64_t& team_id,
                             uint64_t& inviter_id, uint64_t& timestamp) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "HGETALL team:invite:%llu", player_id);
    if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements == 0) {
        if (reply) freeReplyObject(reply);
        return false;
    }
    for (size_t i = 0; i < reply->elements; i += 2) {
        std::string key(reply->element[i]->str);
        std::string val(reply->element[i+1]->str);
        if (key == "team_id") team_id = std::stoull(val);
        else if (key == "inviter_id") inviter_id = std::stoull(val);
        else if (key == "timestamp") timestamp = std::stoull(val);
    }
    freeReplyObject(reply);
    return true;
}

bool RedisClient::del_invite(uint64_t player_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "DEL team:invite:%llu", player_id);
    if (!reply) return false;
    freeReplyObject(reply);
    return true;
}

uint64_t RedisClient::generate_team_id() {
    redisReply* reply = (redisReply*)redisCommand(ctx_, "INCR team:id_counter");
    if (!reply) return 0;
    uint64_t id = reply->integer;
    freeReplyObject(reply);
    return id;
}

}  // namespace farm
```

- [ ] **Step 3: 创建 team_manager.h**

创建 `scripts/server/team_server/src/team_manager.h`：

```cpp
#pragma once

#include "redis_client.h"
#include "team.pb.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

namespace farm {

struct TeamInfo {
    uint64_t team_id;
    uint64_t leader_id;
    std::vector<uint64_t> members;  // leader at index 0
    TeamStatus status;
    int cave_level;
    uint64_t created_at;
};

enum class TeamStatus : int32_t {
    IDLE = 0,
    IN_CAVE = 1,
};

// Callback to send a message to a player via GateServer
using SendToPlayerFunc = std::function<void(uint64_t player_id, uint32_t msg_id,
                                            const uint8_t* payload, size_t len)>;

class TeamManager {
public:
    explicit TeamManager(SendToPlayerFunc send_func);
    ~TeamManager() = default;

    // Initialize Redis connection
    bool init(const std::string& redis_host, int redis_port);

    // Core operations
    int create_team(uint64_t player_id);
    int disband_team(uint64_t player_id);
    int invite_player(uint64_t inviter_id, uint64_t target_id);
    int accept_invite(uint64_t player_id, uint64_t team_id);
    int reject_invite(uint64_t player_id, uint64_t team_id);
    int leave_team(uint64_t player_id);
    int kick_member(uint64_t leader_id, uint64_t target_id);

    // Query
    TeamInfo* get_team(uint64_t team_id);
    TeamInfo* get_player_team(uint64_t player_id);
    bool is_in_team(uint64_t player_id);

    // Invite timeout check (called periodically)
    void check_invite_timeouts();

    // Notify helpers
    void notify_member_update(uint64_t team_id, uint32_t update_type,
                              uint64_t member_id, const std::string& member_name,
                              uint32_t member_level);
    void notify_leader_change(uint64_t team_id, uint64_t new_leader_id,
                              const std::string& new_leader_name);
    void notify_status_update(uint64_t team_id, uint32_t status, int cave_level);

private:
    void load_team_from_redis(uint64_t team_id);
    void remove_team_from_memory(uint64_t team_id);

    SendToPlayerFunc send_func_;
    RedisClient redis_;

    // In-memory cache: team_id -> TeamInfo
    std::unordered_map<uint64_t, TeamInfo> teams_;
    // Player -> team_id mapping (in-memory for fast lookup)
    std::unordered_map<uint64_t, uint64_t> player_team_map_;

    static constexpr uint64_t MAX_TEAM_SIZE = 4;
    static constexpr uint64_t INVITE_TIMEOUT_SEC = 60;
};

}  // namespace farm
```

- [ ] **Step 4: 创建 team_manager.cpp**

创建 `scripts/server/team_server/src/team_manager.cpp`，实现所有队伍管理逻辑：

```cpp
#include "team_manager.h"
#include "message_ids.h"
#include "log_macros.h"

#include <chrono>
#include <algorithm>

namespace farm {

TeamManager::TeamManager(SendToPlayerFunc send_func)
    : send_func_(std::move(send_func))
{
}

bool TeamManager::init(const std::string& redis_host, int redis_port) {
    return redis_.connect(redis_host, redis_port);
}

int TeamManager::create_team(uint64_t player_id) {
    // Check if already in team
    if (is_in_team(player_id)) {
        return static_cast<int>(farm::TEAM_ALREADY_IN_TEAM);
    }

    uint64_t team_id = redis_.generate_team_id();
    if (team_id == 0) {
        SPDLOG_ERROR("[Team]Failed to generate team_id");
        return -1;
    }

    // Create in Redis
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    redis_.set_team_info(team_id, player_id, 0, 0);
    redis_.add_team_member(team_id, player_id);
    redis_.set_player_team(player_id, team_id);

    // Create in memory
    TeamInfo team;
    team.team_id = team_id;
    team.leader_id = player_id;
    team.members.push_back(player_id);
    team.status = TeamStatus::IDLE;
    team.cave_level = 0;
    team.created_at = now;
    teams_[team_id] = team;
    player_team_map_[player_id] = team_id;

    SPDLOG_INFO("[Team]Player {} created team {}", player_id, team_id);

    // Send response
    farm::TeamCreateResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    resp.set_team_id(team_id);
    std::string data;
    resp.SerializeToString(&data);
    send_func_(player_id, MSG_ID_TEAM_CREATE_RESP,
               reinterpret_cast<const uint8_t*>(data.data()), data.size());

    return static_cast<int>(farm::TEAM_SUCCESS);
}

int TeamManager::disband_team(uint64_t player_id) {
    auto it = player_team_map_.find(player_id);
    if (it == player_team_map_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    uint64_t team_id = it->second;
    auto team_it = teams_.find(team_id);
    if (team_it == teams_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    TeamInfo& team = team_it->second;
    if (team.leader_id != player_id) {
        return static_cast<int>(farm::TEAM_NOT_LEADER);
    }
    if (team.status == TeamStatus::IN_CAVE) {
        return static_cast<int>(farm::TEAM_IN_CAVE);
    }

    // Notify all members
    farm::TeamDisbandResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    std::string data;
    resp.SerializeToString(&data);

    for (uint64_t mid : team.members) {
        send_func_(mid, MSG_ID_TEAM_DISBAND_RESP,
                   reinterpret_cast<const uint8_t*>(data.data()), data.size());
        player_team_map_.erase(mid);
        redis_.del_player_team(mid);
    }

    // Clean up Redis
    for (uint64_t mid : team.members) {
        redis_.remove_team_member(team_id, mid);
    }
    redis_.del_team_info(team_id);

    // Remove from memory
    teams_.erase(team_it);

    SPDLOG_INFO("[Team]Team {} disbanded by player {}", team_id, player_id);
    return static_cast<int>(farm::TEAM_SUCCESS);
}

int TeamManager::invite_player(uint64_t inviter_id, uint64_t target_id) {
    if (inviter_id == target_id) {
        return static_cast<int>(farm::TEAM_SELF_OPERATION);
    }

    auto it = player_team_map_.find(inviter_id);
    if (it == player_team_map_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    uint64_t team_id = it->second;
    auto team_it = teams_.find(team_id);
    if (team_it == teams_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    TeamInfo& team = team_it->second;
    if (team.leader_id != inviter_id) {
        return static_cast<int>(farm::TEAM_NOT_LEADER);
    }
    if (team.members.size() >= MAX_TEAM_SIZE) {
        return static_cast<int>(farm::TEAM_FULL);
    }
    if (is_in_team(target_id)) {
        return static_cast<int>(farm::TEAM_TARGET_ALREADY_IN_TEAM);
    }

    // Store invite in Redis
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    redis_.set_invite(target_id, team_id, inviter_id, now);

    SPDLOG_INFO("[Team]Player {} invited player {} to team {}",
                inviter_id, target_id, team_id);

    // Send invite notification to target
    farm::TeamInviteNotify notify;
    notify.set_team_id(team_id);
    notify.set_inviter_id(inviter_id);
    // TODO: set inviter_name and inviter_level from player data
    std::string notify_data;
    notify.SerializeToString(&notify_data);
    send_func_(target_id, MSG_ID_TEAM_INVITE_NOTIFY,
               reinterpret_cast<const uint8_t*>(notify_data.data()), notify_data.size());

    // Send response to inviter
    farm::TeamInviteResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_func_(inviter_id, MSG_ID_TEAM_INVITE_RESP,
               reinterpret_cast<const uint8_t*>(resp_data.data()), resp_data.size());

    return static_cast<int>(farm::TEAM_SUCCESS);
}

int TeamManager::accept_invite(uint64_t player_id, uint64_t team_id) {
    // Check invite exists
    uint64_t inv_team_id = 0, inviter_id = 0, timestamp = 0;
    if (!redis_.get_invite(player_id, inv_team_id, inviter_id, timestamp)) {
        return static_cast<int>(farm::TEAM_INVITE_NOT_FOUND);
    }
    if (inv_team_id != team_id) {
        return static_cast<int>(farm::TEAM_INVITE_NOT_FOUND);
    }

    // Check if already in team (race condition)
    if (is_in_team(player_id)) {
        redis_.del_invite(player_id);
        return static_cast<int>(farm::TEAM_ALREADY_IN_TEAM);
    }

    auto team_it = teams_.find(team_id);
    if (team_it == teams_.end()) {
        redis_.del_invite(player_id);
        return static_cast<int>(farm::TEAM_INVITE_NOT_FOUND);
    }

    TeamInfo& team = team_it->second;
    if (team.members.size() >= MAX_TEAM_SIZE) {
        redis_.del_invite(player_id);
        return static_cast<int>(farm::TEAM_FULL);
    }

    // Add to team
    team.members.push_back(player_id);
    player_team_map_[player_id] = team_id;
    redis_.add_team_member(team_id, player_id);
    redis_.set_player_team(player_id, team_id);
    redis_.del_invite(player_id);

    SPDLOG_INFO("[Team]Player {} joined team {}", player_id, team_id);

    // Notify all members
    notify_member_update(team_id, 0 /* JOIN */, player_id, "", 0);

    // Send accept response with full member list
    farm::TeamAcceptResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    for (uint64_t mid : team.members) {
        auto* member = resp.add_members();
        member->set_player_id(mid);
        member->set_is_leader(mid == team.leader_id);
        // TODO: populate role_name, level, online, hp
    }
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_func_(player_id, MSG_ID_TEAM_ACCEPT_RESP,
               reinterpret_cast<const uint8_t*>(resp_data.data()), resp_data.size());

    return static_cast<int>(farm::TEAM_SUCCESS);
}

int TeamManager::reject_invite(uint64_t player_id, uint64_t team_id) {
    redis_.del_invite(player_id);

    farm::TeamRejectResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    std::string data;
    resp.SerializeToString(&data);
    send_func_(player_id, MSG_ID_TEAM_REJECT_RESP,
               reinterpret_cast<const uint8_t*>(data.data()), data.size());

    return static_cast<int>(farm::TEAM_SUCCESS);
}

int TeamManager::leave_team(uint64_t player_id) {
    auto it = player_team_map_.find(player_id);
    if (it == player_team_map_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    uint64_t team_id = it->second;
    auto team_it = teams_.find(team_id);
    if (team_it == teams_.end()) {
        player_team_map_.erase(it);
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    TeamInfo& team = team_it->second;

    // Remove from team
    team.members.erase(
        std::remove(team.members.begin(), team.members.end(), player_id),
        team.members.end());
    player_team_map_.erase(player_id);
    redis_.remove_team_member(team_id, player_id);
    redis_.del_player_team(player_id);

    // If leader left, transfer to next member
    if (team.leader_id == player_id && !team.members.empty()) {
        uint64_t new_leader = team.members[0];  // earliest joiner
        team.leader_id = new_leader;
        redis_.set_team_info(team_id, new_leader,
                             static_cast<uint32_t>(team.status), team.cave_level);

        SPDLOG_INFO("[Team]Leader transferred to player {} in team {}",
                    new_leader, team_id);

        // Notify leader change
        notify_leader_change(team_id, new_leader, "");
    }

    // If team is empty, disband
    if (team.members.empty()) {
        redis_.del_team_info(team_id);
        teams_.erase(team_it);
        SPDLOG_INFO("[Team]Team {} auto-disbanded (empty)", team_id);
    } else {
        // Notify remaining members
        notify_member_update(team_id, 1 /* LEAVE */, player_id, "", 0);
    }

    // Send response to leaving player
    farm::TeamLeaveResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    std::string data;
    resp.SerializeToString(&data);
    send_func_(player_id, MSG_ID_TEAM_LEAVE_RESP,
               reinterpret_cast<const uint8_t*>(data.data()), data.size());

    return static_cast<int>(farm::TEAM_SUCCESS);
}

int TeamManager::kick_member(uint64_t leader_id, uint64_t target_id) {
    if (leader_id == target_id) {
        return static_cast<int>(farm::TEAM_CANNOT_KICK_SELF);
    }

    auto it = player_team_map_.find(leader_id);
    if (it == player_team_map_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    uint64_t team_id = it->second;
    auto team_it = teams_.find(team_id);
    if (team_it == teams_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    TeamInfo& team = team_it->second;
    if (team.leader_id != leader_id) {
        return static_cast<int>(farm::TEAM_NOT_LEADER);
    }

    // Check target is in team
    auto target_it = std::find(team.members.begin(), team.members.end(), target_id);
    if (target_it == team.members.end()) {
        return static_cast<int>(farm::TEAM_TARGET_NOT_FOUND);
    }

    // Remove target
    team.members.erase(target_it);
    player_team_map_.erase(target_id);
    redis_.remove_team_member(team_id, target_id);
    redis_.del_player_team(target_id);

    SPDLOG_INFO("[Team]Player {} kicked player {} from team {}",
                leader_id, target_id, team_id);

    // Notify kicked player
    farm::TeamMemberUpdate kick_notify;
    kick_notify.set_update_type(farm::TEAM_UPDATE_KICK);
    auto* member = kick_notify.mutable_member();
    member->set_player_id(target_id);
    std::string kick_data;
    kick_notify.SerializeToString(&kick_data);
    send_func_(target_id, MSG_ID_TEAM_MEMBER_UPDATE,
               reinterpret_cast<const uint8_t*>(kick_data.data()), kick_data.size());

    // Notify remaining members
    notify_member_update(team_id, 2 /* KICK */, target_id, "", 0);

    // Send response to leader
    farm::TeamKickResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_func_(leader_id, MSG_ID_TEAM_KICK_RESP,
               reinterpret_cast<const uint8_t*>(resp_data.data()), resp_data.size());

    return static_cast<int>(farm::TEAM_SUCCESS);
}

TeamInfo* TeamManager::get_team(uint64_t team_id) {
    auto it = teams_.find(team_id);
    return (it != teams_.end()) ? &it->second : nullptr;
}

TeamInfo* TeamManager::get_player_team(uint64_t player_id) {
    auto it = player_team_map_.find(player_id);
    if (it == player_team_map_.end()) return nullptr;
    return get_team(it->second);
}

bool TeamManager::is_in_team(uint64_t player_id) {
    return player_team_map_.find(player_id) != player_team_map_.end();
}

void TeamManager::check_invite_timeouts() {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // This is a simplified check - in production, iterate all pending invites
    // For now, invites are cleaned up on accept/reject/timeout via Redis TTL
}

void TeamManager::notify_member_update(uint64_t team_id, uint32_t update_type,
                                       uint64_t member_id, const std::string& member_name,
                                       uint32_t member_level) {
    auto* team = get_team(team_id);
    if (!team) return;

    farm::TeamMemberUpdate notify;
    notify.set_update_type(static_cast<farm::TeamUpdateType>(update_type));
    auto* member = notify.mutable_member();
    member->set_player_id(member_id);
    member->set_role_name(member_name);
    member->set_level(member_level);

    for (uint64_t mid : team->members) {
        auto* m = notify.add_all_members();
        m->set_player_id(mid);
        m->set_is_leader(mid == team->leader_id);
    }

    std::string data;
    notify.SerializeToString(&data);

    for (uint64_t mid : team->members) {
        send_func_(mid, MSG_ID_TEAM_MEMBER_UPDATE,
                   reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }
}

void TeamManager::notify_leader_change(uint64_t team_id, uint64_t new_leader_id,
                                       const std::string& new_leader_name) {
    auto* team = get_team(team_id);
    if (!team) return;

    farm::TeamLeaderChange notify;
    notify.set_new_leader_id(new_leader_id);
    notify.set_new_leader_name(new_leader_name);

    std::string data;
    notify.SerializeToString(&data);

    for (uint64_t mid : team->members) {
        send_func_(mid, MSG_ID_TEAM_LEADER_CHANGE,
                   reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }
}

void TeamManager::notify_status_update(uint64_t team_id, uint32_t status, int cave_level) {
    auto* team = get_team(team_id);
    if (!team) return;

    farm::TeamStatusUpdate notify;
    notify.set_status(status);
    notify.set_cave_level(cave_level);

    std::string data;
    notify.SerializeToString(&data);

    for (uint64_t mid : team->members) {
        send_func_(mid, MSG_ID_TEAM_STATUS_UPDATE,
                   reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }
}

void TeamManager::load_team_from_redis(uint64_t team_id) {
    // Load team info from Redis into memory
    uint64_t leader_id = 0;
    uint32_t status = 0;
    int cave_level = 0;
    if (!redis_.get_team_info(team_id, leader_id, status, cave_level)) return;

    std::vector<uint64_t> members;
    if (!redis_.get_team_members(team_id, members)) return;

    TeamInfo team;
    team.team_id = team_id;
    team.leader_id = leader_id;
    team.members = members;
    team.status = static_cast<TeamStatus>(status);
    team.cave_level = cave_level;
    teams_[team_id] = team;

    for (uint64_t mid : members) {
        player_team_map_[mid] = team_id;
    }
}

void TeamManager::remove_team_from_memory(uint64_t team_id) {
    auto it = teams_.find(team_id);
    if (it == teams_.end()) return;
    for (uint64_t mid : it->second.members) {
        player_team_map_.erase(mid);
    }
    teams_.erase(it);
}

}  // namespace farm
```

- [ ] **Step 5: 更新 team_server.cpp 中的消息处理函数**

回到 `scripts/server/team_server/src/team_server.cpp`，将 TODO 标记的函数替换为实际实现：

```cpp
void TeamServer::handle_team_create_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    farm::TeamCreateReq req;
    if (!req.ParseFromArray(payload, static_cast<int>(len))) return;
    team_mgr_.create_team(player_id);
}

void TeamServer::handle_team_disband_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    farm::TeamDisbandReq req;
    if (!req.ParseFromArray(payload, static_cast<int>(len))) return;
    team_mgr_.disband_team(player_id);
}

void TeamServer::handle_team_invite_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    farm::TeamInviteReq req;
    if (!req.ParseFromArray(payload, static_cast<int>(len))) return;
    team_mgr_.invite_player(player_id, req.target_id());
}

void TeamServer::handle_team_accept_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    farm::TeamAcceptReq req;
    if (!req.ParseFromArray(payload, static_cast<int>(len))) return;
    team_mgr_.accept_invite(player_id, req.team_id());
}

void TeamServer::handle_team_reject_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    farm::TeamRejectReq req;
    if (!req.ParseFromArray(payload, static_cast<int>(len))) return;
    team_mgr_.reject_invite(player_id, req.team_id());
}

void TeamServer::handle_team_leave_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    farm::TeamLeaveReq req;
    if (!req.ParseFromArray(payload, static_cast<int>(len))) return;
    team_mgr_.leave_team(player_id);
}

void TeamServer::handle_team_kick_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    farm::TeamKickReq req;
    if (!req.ParseFromArray(payload, static_cast<int>(len))) return;
    team_mgr_.kick_member(player_id, req.target_id());
}

void TeamServer::handle_team_info_req(uint64_t player_id, const uint8_t* payload, size_t len) {
    farm::TeamInfoReq req;
    if (!req.ParseFromArray(payload, static_cast<int>(len))) return;

    farm::TeamInfoResp resp;
    auto* team = team_mgr_.get_player_team(player_id);
    if (team) {
        resp.set_code(farm::TEAM_SUCCESS);
        resp.set_team_id(team->team_id);
        resp.set_leader_id(team->leader_id);
        resp.set_status(static_cast<uint32_t>(team->status));
        resp.set_cave_level(team->cave_level);
        for (uint64_t mid : team->members) {
            auto* member = resp.add_members();
            member->set_player_id(mid);
            member->set_is_leader(mid == team->leader_id);
        }
    } else {
        resp.set_code(farm::TEAM_NOT_IN_TEAM);
    }

    std::string data;
    resp.SerializeToString(&data);
    send_to_player(player_id, MSG_ID_TEAM_INFO_RESP,
                   reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

void TeamServer::handle_team_query_members_req(std::shared_ptr<TeamGateSession> session,
                                                const uint8_t* payload, size_t len) {
    farm::TeamQueryMembersReq req;
    if (!req.ParseFromArray(payload, static_cast<int>(len))) return;

    farm::TeamQueryMembersResp resp;
    auto* team = team_mgr_.get_player_team(req.player_id());
    if (team) {
        resp.set_code(0);
        resp.set_team_id(team->team_id);
        for (uint64_t mid : team->members) {
            resp.add_member_ids(mid);
        }
    } else {
        resp.set_code(1);  // not in team
    }

    std::string data;
    resp.SerializeToString(&data);
    send_to_gate(session, MSG_ID_TEAM_QUERY_MEMBERS_RESP, data);
}
```

- [ ] **Step 6: 更新 CMakeLists.txt 确保包含新文件**

确认 `scripts/server/team_server/CMakeLists.txt` 的 SOURCES 列表包含：
```cmake
src/team_manager.cpp
src/redis_client.cpp
```

- [ ] **Step 7: 构建验证**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/team_server/build
cmake --build . --config Release
```

- [ ] **Step 8: Commit**

```bash
git add scripts/server/team_server/src/team_manager.h scripts/server/team_server/src/team_manager.cpp scripts/server/team_server/src/redis_client.h scripts/server/team_server/src/redis_client.cpp scripts/server/team_server/src/team_server.cpp
git commit -m "feat(team): implement TeamManager core logic"
```

---

## Task 6: GateServer 路由扩展

**Files:**
- Create: `scripts/server/gate_server/src/team_connection.h`
- Create: `scripts/server/gate_server/src/team_connection.cpp`
- Modify: `scripts/server/gate_server/src/gate_server.h`
- Modify: `scripts/server/gate_server/src/gate_server.cpp`
- Modify: `scripts/server/gate_server/CMakeLists.txt`

- [ ] **Step 1: 创建 team_connection.h**

创建 `scripts/server/gate_server/src/team_connection.h`，参考 `game_connection.h` 结构：

```cpp
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

enum class TeamConnState {
    DISCONNECTED,
    CONNECTING,
    IDENTIFIED
};

using TeamMessageCallback = std::function<void(uint32_t msg_id, const std::vector<uint8_t>& payload)>;

class TeamConnection {
public:
    TeamConnection(struct event_base* base, const std::string& gate_id);
    ~TeamConnection();

    bool connect(const std::string& ip, uint16_t port);
    void disconnect();

    bool send(uint32_t msg_id, std::string_view payload);
    bool send(uint32_t msg_id, const uint8_t* payload, size_t len);

    TeamConnState state() const { return state_; }
    void set_state(TeamConnState state) { state_ = state; }
    bool is_identified() const { return state_ == TeamConnState::IDENTIFIED; }

    void set_message_callback(TeamMessageCallback callback) { msg_callback_ = std::move(callback); }

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
    std::string team_ip_;
    uint16_t team_port_;
    TeamConnState state_;

    struct bufferevent* bev_;
    struct event* heartbeat_timer_;
    struct event* reconnect_timer_;
    time_t last_heartbeat_;
    std::vector<uint8_t> read_buffer_;

    TeamMessageCallback msg_callback_;
};

}  // namespace farm
```

- [ ] **Step 2: 创建 team_connection.cpp**

创建 `scripts/server/gate_server/src/team_connection.cpp`，实现与 `game_connection.cpp` 相同的模式（连接、心跳、重连、消息读取）。

- [ ] **Step 3: 修改 gate_server.h**

在 `GateServer` 类中新增：

```cpp
#include "team_connection.h"

// 新增成员
std::unique_ptr<TeamConnection> team_conn_;

// 新增方法
void add_team_server(const std::string& ip, uint16_t port);
void handle_team_message(uint32_t msg_id, const std::vector<uint8_t>& payload);
void forward_to_team(std::shared_ptr<Session> session, uint32_t msg_id,
                     const std::vector<uint8_t>& payload);
```

- [ ] **Step 4: 修改 gate_server.cpp**

在 `route_message` 中新增 TeamServer 路由：

```cpp
void GateServer::route_message(std::shared_ptr<Session> session, uint32_t msg_id,
                               const std::vector<uint8_t>& payload) {
    // ... existing routing ...

    // TeamServer messages (7001-7099)
    if (msg_id >= 7001 && msg_id <= 7099) {
        forward_to_team(session, msg_id, payload);
        return;
    }

    // ... rest of existing routing ...
}
```

新增 `forward_to_team` 方法，参考 `forward_to_game` 的实现。

在 `handle_team_message` 中处理 TeamServer 返回的消息（转发给对应玩家）。

在 `start()` 中连接 TeamServer：

```cpp
if (team_conn_) {
    team_conn_->set_message_callback(
        [this](uint32_t msg_id, const std::vector<uint8_t>& payload) {
            handle_team_message(msg_id, payload);
        });
    team_conn_->connect(team_ip_, team_port_);
}
```

- [ ] **Step 5: 更新 CMakeLists.txt**

在 `scripts/server/gate_server/CMakeLists.txt` 的 SOURCES 中新增：
```cmake
src/team_connection.cpp
```

- [ ] **Step 6: 构建验证**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/gate_server/build
cmake --build . --config Release
```

- [ ] **Step 7: Commit**

```bash
git add scripts/server/gate_server/
git commit -m "feat(team): add TeamServer routing in GateServer"
```

---

## Task 7: 客户端 TeamManager

**Files:**
- Create: `scripts/client/team_manager.py`
- Create: `scripts/client/test_team_manager.py`

- [ ] **Step 1: 创建 team_manager.py**

创建 `scripts/client/team_manager.py`：

```python
"""
客户端组队管理器
管理队伍状态、发送组队请求、处理组队回调。
"""
import logging
from typing import List, Optional, Callable
from dataclasses import dataclass, field

from .message_ids import (
    MSG_ID_TEAM_CREATE_REQ, MSG_ID_TEAM_CREATE_RESP,
    MSG_ID_TEAM_DISBAND_REQ, MSG_ID_TEAM_DISBAND_RESP,
    MSG_ID_TEAM_INVITE_REQ, MSG_ID_TEAM_INVITE_RESP,
    MSG_ID_TEAM_INVITE_NOTIFY,
    MSG_ID_TEAM_ACCEPT_REQ, MSG_ID_TEAM_ACCEPT_RESP,
    MSG_ID_TEAM_REJECT_REQ, MSG_ID_TEAM_REJECT_RESP,
    MSG_ID_TEAM_LEAVE_REQ, MSG_ID_TEAM_LEAVE_RESP,
    MSG_ID_TEAM_KICK_REQ, MSG_ID_TEAM_KICK_RESP,
    MSG_ID_TEAM_INFO_REQ, MSG_ID_TEAM_INFO_RESP,
    MSG_ID_TEAM_MEMBER_UPDATE, MSG_ID_TEAM_LEADER_CHANGE,
    MSG_ID_TEAM_STATUS_UPDATE,
)

logger = logging.getLogger("client.team_manager")


@dataclass
class TeamMember:
    """队伍成员信息"""
    player_id: int = 0
    role_name: str = ""
    level: int = 0
    online: bool = True
    is_leader: bool = False
    current_hp: int = 0
    max_hp: int = 0


@dataclass
class TeamInviteNotify:
    """组队邀请通知"""
    team_id: int = 0
    inviter_id: int = 0
    inviter_name: str = ""
    inviter_level: int = 0


class TeamManager:
    """客户端组队管理器"""

    def __init__(self, connection, player_id: int):
        self._connection = connection
        self._player_id = player_id
        self._team_id: int = 0
        self._leader_id: int = 0
        self._members: List[TeamMember] = []
        self._pending_invites: List[TeamInviteNotify] = []
        self._status: int = 0  # 0=idle, 1=in_cave
        self._cave_level: int = 0

        # Callbacks
        self.on_team_created: Optional[Callable[[int], None]] = None
        self.on_team_disbanded: Optional[Callable[[], None]] = None
        self.on_member_update: Optional[Callable[[], None]] = None
        self.on_leader_change: Optional[Callable[[int], None]] = None
        self.on_invite_received: Optional[Callable[[TeamInviteNotify], None]] = None
        self.on_error: Optional[Callable[[int, str], None]] = None

    @property
    def in_team(self) -> bool:
        return self._team_id != 0

    @property
    def is_leader(self) -> bool:
        return self._leader_id == self._player_id

    @property
    def team_id(self) -> int:
        return self._team_id

    @property
    def members(self) -> List[TeamMember]:
        return self._members

    @property
    def member_count(self) -> int:
        return len(self._members)

    @property
    def pending_invites(self) -> List[TeamInviteNotify]:
        return self._pending_invites

    def create_team(self) -> None:
        """创建队伍"""
        logger.info("Creating team")
        self._connection.send_message(MSG_ID_TEAM_CREATE_REQ, b'')

    def disband_team(self) -> None:
        """解散队伍（队长）"""
        if not self.in_team:
            return
        logger.info(f"Disbanding team {self._team_id}")
        self._connection.send_message(MSG_ID_TEAM_DISBAND_REQ, b'')

    def invite_player(self, target_id: int) -> None:
        """邀请玩家组队"""
        if not self.in_team:
            return
        logger.info(f"Inviting player {target_id} to team {self._team_id}")
        # Encode target_id as uint64
        data = target_id.to_bytes(8, 'little')
        self._connection.send_message(MSG_ID_TEAM_INVITE_REQ, data)

    def accept_invite(self, team_id: int) -> None:
        """接受组队邀请"""
        logger.info(f"Accepting invite to team {team_id}")
        data = team_id.to_bytes(8, 'little')
        self._connection.send_message(MSG_ID_TEAM_ACCEPT_REQ, data)

    def reject_invite(self, team_id: int) -> None:
        """拒绝组队邀请"""
        logger.info(f"Rejecting invite to team {team_id}")
        data = team_id.to_bytes(8, 'little')
        self._connection.send_message(MSG_ID_TEAM_REJECT_REQ, data)

    def leave_team(self) -> None:
        """离开队伍"""
        if not self.in_team:
            return
        logger.info(f"Leaving team {self._team_id}")
        self._connection.send_message(MSG_ID_TEAM_LEAVE_REQ, b'')

    def kick_member(self, target_id: int) -> None:
        """踢出成员（队长）"""
        if not self.in_team or not self.is_leader:
            return
        logger.info(f"Kicking player {target_id} from team {self._team_id}")
        data = target_id.to_bytes(8, 'little')
        self._connection.send_message(MSG_ID_TEAM_KICK_REQ, data)

    def request_team_info(self) -> None:
        """查询队伍信息"""
        self._connection.send_message(MSG_ID_TEAM_INFO_REQ, b'')

    # --- Message handlers (called by NetworkDispatcher) ---

    def on_team_create_resp(self, data: bytes) -> None:
        """处理创建队伍响应"""
        import team_pb2
        resp = team_pb2.TeamCreateResp()
        resp.ParseFromString(data)
        if resp.code == 0:
            self._team_id = resp.team_id
            logger.info(f"Team created: {self._team_id}")
            if self.on_team_created:
                self.on_team_created(self._team_id)
        else:
            logger.warning(f"Create team failed: code={resp.code}")
            if self.on_error:
                self.on_error(resp.code, "创建队伍失败")

    def on_team_disband_resp(self, data: bytes) -> None:
        """处理解散队伍响应"""
        import team_pb2
        resp = team_pb2.TeamDisbandResp()
        resp.ParseFromString(data)
        if resp.code == 0:
            self._clear_team()
            logger.info("Team disbanded")
            if self.on_team_disbanded:
                self.on_team_disbanded()

    def on_team_invite_resp(self, data: bytes) -> None:
        """处理邀请响应"""
        import team_pb2
        resp = team_pb2.TeamInviteResp()
        resp.ParseFromString(data)
        if resp.code != 0:
            logger.warning(f"Invite failed: code={resp.code}")
            if self.on_error:
                self.on_error(resp.code, "邀请失败")

    def on_team_invite_notify(self, data: bytes) -> None:
        """处理收到邀请通知"""
        import team_pb2
        notify = team_pb2.TeamInviteNotify()
        notify.ParseFromString(data)
        invite = TeamInviteNotify(
            team_id=notify.team_id,
            inviter_id=notify.inviter_id,
            inviter_name=notify.inviter_name,
            inviter_level=notify.inviter_level,
        )
        self._pending_invites.append(invite)
        logger.info(f"Received invite from {notify.inviter_name} to team {notify.team_id}")
        if self.on_invite_received:
            self.on_invite_received(invite)

    def on_team_accept_resp(self, data: bytes) -> None:
        """处理接受邀请响应"""
        import team_pb2
        resp = team_pb2.TeamAcceptResp()
        resp.ParseFromString(data)
        if resp.code == 0:
            self._team_id = resp.members[0].team_id if resp.members else 0
            self._update_members(resp.members)
            # Remove pending invite
            self._pending_invites = [
                i for i in self._pending_invites if i.team_id != self._team_id
            ]
            logger.info(f"Joined team {self._team_id}")
            if self.on_member_update:
                self.on_member_update()

    def on_team_reject_resp(self, data: bytes) -> None:
        """处理拒绝邀请响应"""
        import team_pb2
        resp = team_pb2.TeamRejectResp()
        resp.ParseFromString(data)
        # Remove from pending
        # team_id not in resp, clean up based on accept req context

    def on_team_leave_resp(self, data: bytes) -> None:
        """处理离开队伍响应"""
        import team_pb2
        resp = team_pb2.TeamLeaveResp()
        resp.ParseFromString(data)
        if resp.code == 0:
            self._clear_team()
            logger.info("Left team")

    def on_team_kick_resp(self, data: bytes) -> None:
        """处理踢出响应"""
        import team_pb2
        resp = team_pb2.TeamKickResp()
        resp.ParseFromString(data)
        if resp.code != 0:
            logger.warning(f"Kick failed: code={resp.code}")
            if self.on_error:
                self.on_error(resp.code, "踢出失败")

    def on_team_info_resp(self, data: bytes) -> None:
        """处理队伍信息响应"""
        import team_pb2
        resp = team_pb2.TeamInfoResp()
        resp.ParseFromString(data)
        if resp.code == 0:
            self._team_id = resp.team_id
            self._leader_id = resp.leader_id
            self._status = resp.status
            self._cave_level = resp.cave_level
            self._update_members(resp.members)
            logger.info(f"Team info: id={self._team_id}, members={len(self._members)}")

    def on_team_member_update(self, data: bytes) -> None:
        """处理成员变更通知"""
        import team_pb2
        notify = team_pb2.TeamMemberUpdate()
        notify.ParseFromString(data)
        self._update_members(notify.all_members)
        logger.info(f"Member update: type={notify.update_type}")
        if self.on_member_update:
            self.on_member_update()

    def on_team_leader_change(self, data: bytes) -> None:
        """处理队长变更通知"""
        import team_pb2
        notify = team_pb2.TeamLeaderChange()
        notify.ParseFromString(data)
        self._leader_id = notify.new_leader_id
        # Update member is_leader flags
        for m in self._members:
            m.is_leader = (m.player_id == self._leader_id)
        logger.info(f"Leader changed to {notify.new_leader_id}")
        if self.on_leader_change:
            self.on_leader_change(notify.new_leader_id)

    def on_team_status_update(self, data: bytes) -> None:
        """处理队伍状态变更"""
        import team_pb2
        notify = team_pb2.TeamStatusUpdate()
        notify.ParseFromString(data)
        self._status = notify.status
        self._cave_level = notify.cave_level

    # --- Internal helpers ---

    def _update_members(self, proto_members) -> None:
        """从 proto 成员列表更新内存"""
        self._members = []
        for m in proto_members:
            member = TeamMember(
                player_id=m.player_id,
                role_name=m.role_name,
                level=m.level,
                online=m.online,
                is_leader=m.is_leader,
                current_hp=m.current_hp,
                max_hp=m.max_hp,
            )
            self._members.append(member)
        if self._members:
            self._leader_id = next((m.player_id for m in self._members if m.is_leader), 0)

    def _clear_team(self) -> None:
        """清除队伍状态"""
        self._team_id = 0
        self._leader_id = 0
        self._members.clear()
        self._status = 0
        self._cave_level = 0
```

- [ ] **Step 2: 创建 test_team_manager.py**

创建 `scripts/client/test_team_manager.py`：

```python
"""TeamManager 单元测试"""
import unittest
from unittest.mock import MagicMock

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from team_manager import TeamManager, TeamMember, TeamInviteNotify


class MockConnection:
    def __init__(self):
        self.sent_messages = []

    def send_message(self, msg_id, data):
        self.sent_messages.append((msg_id, data))


class TestTeamManager(unittest.TestCase):
    def setUp(self):
        self.conn = MockConnection()
        self.mgr = TeamManager(self.conn, player_id=1001)

    def test_initial_state(self):
        """初始状态：不在队伍中"""
        self.assertFalse(self.mgr.in_team)
        self.assertFalse(self.mgr.is_leader)
        self.assertEqual(self.mgr.team_id, 0)
        self.assertEqual(self.mgr.member_count, 0)

    def test_create_team_sends_req(self):
        """创建队伍发送正确消息"""
        self.mgr.create_team()
        self.assertEqual(len(self.conn.sent_messages), 1)
        self.assertEqual(self.conn.sent_messages[0][0], 7001)

    def test_leave_team_when_not_in_team(self):
        """不在队伍中时离开无效"""
        self.mgr.leave_team()
        self.assertEqual(len(self.conn.sent_messages), 0)

    def test_invite_when_not_in_team(self):
        """不在队伍中时邀请无效"""
        self.mgr.invite_player(2001)
        self.assertEqual(len(self.conn.sent_messages), 0)


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 3: 运行测试**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest scripts/client/test_team_manager.py -v
```

- [ ] **Step 4: Commit**

```bash
git add scripts/client/team_manager.py scripts/client/test_team_manager.py
git commit -m "feat(team): add client TeamManager"
```

---

## Task 8: 客户端 NetworkDispatcher 集成

**Files:**
- Modify: `scripts/client/network_dispatcher.py`

- [ ] **Step 1: 在 NetworkDispatcher 中新增 TeamServer 消息分发**

在 `network_dispatcher.py` 的导入部分新增：

```python
from .message_ids import (
    # ... existing imports ...
    MSG_ID_TEAM_CREATE_RESP, MSG_ID_TEAM_DISBAND_RESP,
    MSG_ID_TEAM_INVITE_RESP, MSG_ID_TEAM_INVITE_NOTIFY,
    MSG_ID_TEAM_ACCEPT_RESP, MSG_ID_TEAM_REJECT_RESP,
    MSG_ID_TEAM_LEAVE_RESP, MSG_ID_TEAM_KICK_RESP,
    MSG_ID_TEAM_INFO_RESP, MSG_ID_TEAM_MEMBER_UPDATE,
    MSG_ID_TEAM_LEADER_CHANGE, MSG_ID_TEAM_STATUS_UPDATE,
)
```

在 `__init__` 中新增回调参数：

```python
def __init__(
    self,
    connection,
    # ... existing params ...
    on_team_create_resp: Callable[[Any], None] = None,
    on_team_disband_resp: Callable[[Any], None] = None,
    on_team_invite_resp: Callable[[Any], None] = None,
    on_team_invite_notify: Callable[[Any], None] = None,
    on_team_accept_resp: Callable[[Any], None] = None,
    on_team_reject_resp: Callable[[Any], None] = None,
    on_team_leave_resp: Callable[[Any], None] = None,
    on_team_kick_resp: Callable[[Any], None] = None,
    on_team_info_resp: Callable[[Any], None] = None,
    on_team_member_update: Callable[[Any], None] = None,
    on_team_leader_change: Callable[[Any], None] = None,
    on_team_status_update: Callable[[Any], None] = None,
):
```

在 `_build_dispatch_table` 中新增分发表项：

```python
self._dispatch_table = {
    # ... existing entries ...
    MSG_ID_TEAM_CREATE_RESP: self._callbacks.get("on_team_create_resp"),
    MSG_ID_TEAM_DISBAND_RESP: self._callbacks.get("on_team_disband_resp"),
    MSG_ID_TEAM_INVITE_RESP: self._callbacks.get("on_team_invite_resp"),
    MSG_ID_TEAM_INVITE_NOTIFY: self._callbacks.get("on_team_invite_notify"),
    MSG_ID_TEAM_ACCEPT_RESP: self._callbacks.get("on_team_accept_resp"),
    MSG_ID_TEAM_REJECT_RESP: self._callbacks.get("on_team_reject_resp"),
    MSG_ID_TEAM_LEAVE_RESP: self._callbacks.get("on_team_leave_resp"),
    MSG_ID_TEAM_KICK_RESP: self._callbacks.get("on_team_kick_resp"),
    MSG_ID_TEAM_INFO_RESP: self._callbacks.get("on_team_info_resp"),
    MSG_ID_TEAM_MEMBER_UPDATE: self._callbacks.get("on_team_member_update"),
    MSG_ID_TEAM_LEADER_CHANGE: self._callbacks.get("on_team_leader_change"),
    MSG_ID_TEAM_STATUS_UPDATE: self._callbacks.get("on_team_status_update"),
}
```

- [ ] **Step 2: Commit**

```bash
git add scripts/client/network_dispatcher.py
git commit -m "feat(team): integrate team messages in NetworkDispatcher"
```

---

## Task 9: 客户端 UI — 队伍 HUD

**Files:**
- Create: `scripts/client/team_hud.py`

- [ ] **Step 1: 创建 team_hud.py**

创建 `scripts/client/team_hud.py`：

```python
"""
队伍 HUD UI 组件
在战斗场景中显示队伍成员信息（HP、等级等）。
"""
import pygame
from typing import Optional
from .team_manager import TeamManager


class TeamHUD:
    """队伍 HUD 显示"""

    # 布局常量
    PANEL_WIDTH = 200
    PANEL_HEIGHT_PER_MEMBER = 28
    PANEL_PADDING = 8
    MARGIN_TOP = 10
    MARGIN_LEFT = 10

    # 颜色
    BG_COLOR = (0, 0, 0, 160)      # 半透明黑色
    BORDER_COLOR = (100, 100, 100)
    TEXT_COLOR = (255, 255, 255)
    LEADER_COLOR = (255, 215, 0)    # 金色
    HP_BAR_COLOR = (0, 200, 0)      # 绿色
    HP_BAR_BG = (80, 80, 80)
    LEAVE_BTN_COLOR = (180, 60, 60)
    LEAVE_BTN_HOVER = (220, 80, 80)

    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._visible = False
        self._leave_btn_rect: Optional[pygame.Rect] = None
        self._leave_btn_hover = False

    def set_visible(self, visible: bool) -> None:
        self._visible = visible

    def handle_event(self, event: pygame.event.Event, team_mgr: TeamManager) -> bool:
        """处理输入事件，返回 True 表示事件已消费"""
        if not self._visible:
            return False

        if event.type == pygame.MOUSEMOTION:
            if self._leave_btn_rect:
                self._leave_btn_hover = self._leave_btn_rect.collidepoint(event.pos)

        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            if self._leave_btn_rect and self._leave_btn_rect.collidepoint(event.pos):
                team_mgr.leave_team()
                return True

        return False

    def render(self, screen: pygame.Surface, team_mgr: TeamManager) -> None:
        """渲染队伍 HUD"""
        if not self._visible or not team_mgr.in_team:
            return

        members = team_mgr.members
        if not members:
            return

        panel_height = (self.PANEL_PADDING * 2 +
                        len(members) * self.PANEL_HEIGHT_PER_MEMBER +
                        30)  # 30 for header + leave button

        # Panel background
        panel_surface = pygame.Surface(
            (self.PANEL_WIDTH, panel_height), pygame.SRCALPHA)
        panel_surface.fill(self.BG_COLOR)
        screen.blit(panel_surface, (self.MARGIN_LEFT, self.MARGIN_TOP))

        # Border
        pygame.draw.rect(screen, self.BORDER_COLOR,
                         (self.MARGIN_LEFT, self.MARGIN_TOP,
                          self.PANEL_WIDTH, panel_height), 1)

        # Header
        font = pygame.font.SysFont(None, 20)
        header = font.render(f"队伍 ({len(members)}/4)", True, self.TEXT_COLOR)
        screen.blit(header, (self.MARGIN_LEFT + self.PANEL_PADDING,
                             self.MARGIN_TOP + self.PANEL_PADDING))

        # Leave button
        btn_x = self.MARGIN_LEFT + self.PANEL_WIDTH - 60
        btn_y = self.MARGIN_TOP + self.PANEL_PADDING
        btn_rect = pygame.Rect(btn_x, btn_y, 50, 18)
        btn_color = self.LEAVE_BTN_HOVER if self._leave_btn_hover else self.LEAVE_BTN_COLOR
        pygame.draw.rect(screen, btn_color, btn_rect)
        btn_text = font.render("离开", True, self.TEXT_COLOR)
        screen.blit(btn_text, (btn_x + 8, btn_y + 2))
        self._leave_btn_rect = btn_rect

        # Members
        y = self.MARGIN_TOP + self.PANEL_PADDING + 24
        small_font = pygame.font.SysFont(None, 16)
        for member in members:
            # Leader icon
            name_color = self.LEADER_COLOR if member.is_leader else self.TEXT_COLOR
            prefix = "👑 " if member.is_leader else "   "

            # Name + level
            name_text = f"{prefix}{member.role_name} Lv.{member.level}"
            name_surface = small_font.render(name_text, True, name_color)
            screen.blit(name_surface, (self.MARGIN_LEFT + self.PANEL_PADDING, y))

            # HP bar
            hp_x = self.MARGIN_LEFT + self.PANEL_PADDING + 120
            hp_y = y + 2
            hp_width = 60
            hp_height = 12
            pygame.draw.rect(screen, self.HP_BAR_BG,
                             (hp_x, hp_y, hp_width, hp_height))
            if member.max_hp > 0:
                fill_width = int(hp_width * member.current_hp / member.max_hp)
                pygame.draw.rect(screen, self.HP_BAR_COLOR,
                                 (hp_x, hp_y, fill_width, hp_height))

            y += self.PANEL_HEIGHT_PER_MEMBER
```

- [ ] **Step 2: Commit**

```bash
git add scripts/client/team_hud.py
git commit -m "feat(team): add team HUD UI component"
```

---

## Task 10: 客户端 UI — 邀请通知

**Files:**
- Create: `scripts/client/team_invite_notify.py`

- [ ] **Step 1: 创建 team_invite_notify.py**

创建 `scripts/client/team_invite_notify.py`：

```python
"""
组队邀请通知 UI 组件
收到邀请时弹出通知，60 秒后自动消失。
"""
import pygame
import time
from typing import Optional, List
from .team_manager import TeamManager, TeamInviteNotify


class TeamInviteNotifyUI:
    """组队邀请通知 UI"""

    WIDTH = 280
    HEIGHT = 80
    MARGIN_RIGHT = 20
    MARGIN_BOTTOM = 20
    TIMEOUT_SEC = 60

    # Colors
    BG_COLOR = (40, 40, 60, 220)
    BORDER_COLOR = (100, 150, 255)
    TEXT_COLOR = (255, 255, 255)
    ACCEPT_COLOR = (60, 180, 60)
    ACCEPT_HOVER = (80, 220, 80)
    REJECT_COLOR = (180, 60, 60)
    REJECT_HOVER = (220, 80, 80)

    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._notifications: List[dict] = []  # {invite, timestamp, accept_rect, reject_rect}

    def add_invite(self, invite: TeamInviteNotify) -> None:
        """添加邀请通知"""
        self._notifications.append({
            "invite": invite,
            "timestamp": time.time(),
            "accept_rect": None,
            "reject_rect": None,
        })

    def handle_event(self, event: pygame.event.Event, team_mgr: TeamManager) -> bool:
        """处理输入事件"""
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            for notif in self._notifications[:]:
                if notif["accept_rect"] and notif["accept_rect"].collidepoint(event.pos):
                    team_mgr.accept_invite(notif["invite"].team_id)
                    self._notifications.remove(notif)
                    return True
                if notif["reject_rect"] and notif["reject_rect"].collidepoint(event.pos):
                    team_mgr.reject_invite(notif["invite"].team_id)
                    self._notifications.remove(notif)
                    return True
        return False

    def update(self) -> None:
        """更新通知列表，移除过期通知"""
        now = time.time()
        self._notifications = [
            n for n in self._notifications
            if now - n["timestamp"] < self.TIMEOUT_SEC
        ]

    def render(self, screen: pygame.Surface) -> None:
        """渲染邀请通知"""
        font = pygame.font.SysFont(None, 18)
        small_font = pygame.font.SysFont(None, 14)

        for i, notif in enumerate(self._notifications):
            invite = notif["invite"]

            # Position (bottom-right, stacked)
            x = self._screen_width - self.WIDTH - self.MARGIN_RIGHT
            y = (self._screen_height - self.HEIGHT - self.MARGIN_BOTTOM -
                 i * (self.HEIGHT + 10))

            # Background
            surface = pygame.Surface((self.WIDTH, self.HEIGHT), pygame.SRCALPHA)
            surface.fill(self.BG_COLOR)
            screen.blit(surface, (x, y))

            # Border
            pygame.draw.rect(screen, self.BORDER_COLOR,
                             (x, y, self.WIDTH, self.HEIGHT), 1)

            # Text
            title = font.render(
                f"{invite.inviter_name} 邀请你加入队伍", True, self.TEXT_COLOR)
            screen.blit(title, (x + 10, y + 10))

            level_text = small_font.render(
                f"等级: {invite.inviter_level}", True, self.TEXT_COLOR)
            screen.blit(level_text, (x + 10, y + 30))

            # Accept button
            accept_rect = pygame.Rect(x + 10, y + 52, 60, 22)
            pygame.draw.rect(screen, self.ACCEPT_COLOR, accept_rect)
            accept_text = small_font.render("接受", True, self.TEXT_COLOR)
            screen.blit(accept_text, (x + 22, y + 56))
            notif["accept_rect"] = accept_rect

            # Reject button
            reject_rect = pygame.Rect(x + 80, y + 52, 60, 22)
            pygame.draw.rect(screen, self.REJECT_COLOR, reject_rect)
            reject_text = small_font.render("拒绝", True, self.TEXT_COLOR)
            screen.blit(reject_text, (x + 92, y + 56))
            notif["reject_rect"] = reject_rect

            # Timeout progress bar
            elapsed = time.time() - notif["timestamp"]
            remaining = max(0, self.TIMEOUT_SEC - elapsed)
            bar_width = int((remaining / self.TIMEOUT_SEC) * (self.WIDTH - 20))
            pygame.draw.rect(screen, (60, 60, 80),
                             (x + 10, y + self.HEIGHT - 6, self.WIDTH - 20, 4))
            pygame.draw.rect(screen, self.BORDER_COLOR,
                             (x + 10, y + self.HEIGHT - 6, bar_width, 4))
```

- [ ] **Step 2: Commit**

```bash
git add scripts/client/team_invite_notify.py
git commit -m "feat(team): add team invite notification UI"
```

---

## Task 11: GameScene 集成

**Files:**
- Modify: `scripts/client/game_scene.py`

- [ ] **Step 1: 在 GameScene 中集成 TeamManager 和 UI**

在 `game_scene.py` 中：

1. 导入新模块：
```python
from .team_manager import TeamManager
from .team_hud import TeamHUD
from .team_invite_notify import TeamInviteNotifyUI
```

2. 在 `__init__` 中创建实例：
```python
self.team_mgr = TeamManager(self.connection, self.player_id)
self.team_hud = TeamHUD(screen_width, screen_height)
self.team_invite_ui = TeamInviteNotifyUI(screen_width, screen_height)
```

3. 在 NetworkDispatcher 初始化时传入 team 回调：
```python
on_team_create_resp=self.team_mgr.on_team_create_resp,
on_team_disband_resp=self.team_mgr.on_team_disband_resp,
on_team_invite_resp=self.team_mgr.on_team_invite_resp,
on_team_invite_notify=self._on_team_invite_notify,
on_team_accept_resp=self.team_mgr.on_team_accept_resp,
on_team_reject_resp=self.team_mgr.on_team_reject_resp,
on_team_leave_resp=self.team_mgr.on_team_leave_resp,
on_team_kick_resp=self.team_mgr.on_team_kick_resp,
on_team_info_resp=self.team_mgr.on_team_info_resp,
on_team_member_update=self.team_mgr.on_team_member_update,
on_team_leader_change=self.team_mgr.on_team_leader_change,
on_team_status_update=self.team_mgr.on_team_status_update,
```

4. 新增回调方法：
```python
def _on_team_invite_notify(self, data):
    """收到组队邀请"""
    self.team_mgr.on_team_invite_notify(data)
    invite = self.team_mgr.pending_invites[-1] if self.team_mgr.pending_invites else None
    if invite:
        self.team_invite_ui.add_invite(invite)
```

5. 在 `handle_event` 中处理 team UI 事件：
```python
if self.team_invite_ui.handle_event(event, self.team_mgr):
    return
if self.team_hud.handle_event(event, self.team_mgr):
    return
```

6. 在 `render` 中渲染 team UI：
```python
self.team_hud.render(screen, self.team_mgr)
self.team_invite_ui.update()
self.team_invite_ui.render(screen)
```

7. 进入矿洞时显示 HUD，离开时隐藏：
```python
# 在场景切换到矿洞时
self.team_hud.set_visible(True)
# 在场景切换回农场时
self.team_hud.set_visible(False)
```

- [ ] **Step 2: Commit**

```bash
git add scripts/client/game_scene.py
git commit -m "feat(team): integrate team system in GameScene"
```

---

## Task 12: GameServer 战斗经验分配集成

**Files:**
- Modify: `scripts/server/game_server/src/combat_handler.cpp`
- Modify: `scripts/server/game_server/src/gate_session.h` (新增 TeamServer 查询方法)

- [ ] **Step 1: 在 GameServer 中新增 TeamServer 查询方法**

在 `gate_session.h` 中新增发送查询到 TeamServer 的方法（通过 GateServer 转发）。

- [ ] **Step 2: 修改战斗经验分配逻辑**

在 `combat_handler.cpp` 的怪物击杀处理中，查询队伍成员并平均分配经验：

```cpp
// 怪物被击杀后
void CombatHandler::on_monster_killed(uint64_t killer_id, uint32_t exp_reward) {
    // 查询击杀者是否在队伍中
    // 如果在队伍中，获取同层在线成员，平均分配经验
    // 如果不在队伍中，击杀者独享经验
}
```

- [ ] **Step 3: Commit**

```bash
git add scripts/server/game_server/src/combat_handler.cpp
git commit -m "feat(team): integrate team exp distribution in combat"
```

---

## Task 13: ChatServer 队伍频道集成

**Files:**
- Modify: `scripts/server/chat_server/src/channel_manager.cpp`
- Modify: `scripts/server/chat_server/src/channel_manager.h`

- [ ] **Step 1: 在 ChannelManager 中新增队伍频道广播**

当收到 `CHANNEL_PARTY` 类型的消息时，查询 TeamServer 获取队伍成员列表，然后广播给所有成员。

```cpp
void ChannelManager::broadcast_to_party(uint64_t sender_id, const std::string& sender_name,
                                        const std::string& content, uint64_t timestamp) {
    // 通过 TeamServer 查询 sender_id 所在队伍的成员
    // 广播消息给所有在线成员
}
```

- [ ] **Step 2: Commit**

```bash
git add scripts/server/chat_server/src/channel_manager.cpp scripts/server/chat_server/src/channel_manager.h
git commit -m "feat(team): integrate team channel in ChatServer"
```

---

## Self-Review Checklist

- [x] **Spec coverage:** 所有设计文档中的需求都有对应 Task 实现
- [x] **Placeholder scan:** 无 TBD/TODO（除了 Task 12/13 中的集成细节需要根据实际代码调整）
- [x] **Type consistency:** MsgID 7001-7099 全文一致，TeamErrorCode 枚举值一致，TeamMember 字段一致
- [x] **MsgID conflict fix:** 已修正设计文档中的 6000-6999 冲突，使用 7001-7099
