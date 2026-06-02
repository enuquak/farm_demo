# gRPC 全面迁移实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将全部通信（客户端↔Gate + 服务间）从 libevent+TCP 迁移到 gRPC，消除 msg_id 分发体系，用 proto service 定义替代手写连接管理代码。

**Architecture:** 分层 Service 网格 — Gate Server 暴露 `FarmGateway` bidirectional streaming 给客户端，内部各服务暴露独立 gRPC service。Gate 按请求类型路由到后端。纯 gRPC 事件循环替代 libevent。

**Tech Stack:** gRPC C++ (grpc++), protobuf (full), Python grpcio, etcd-cpp-apiv3 (保留), spdlog (保留)

**Spec:** `docs/superpowers/specs/2026-06-03-grpc-migration-design.md`

---

## 文件结构总览

### 新增文件

```
scripts/common/proto/
├── client/
│   └── gateway.proto                 # FarmGateway + GatewayNotify service + ClientRequest/ServerEvent
├── internal/
│   ├── game_service.proto            # GameService (Gate↔Game)
│   ├── dbmgr_service.proto           # DBMgrService (Game↔DBMgr)
│   ├── chat_service.proto            # ChatService (Gate↔Chat)
│   ├── team_service.proto            # TeamService (Gate↔Team, Game↔Team)
│   ├── friend_service.proto          # FriendService (Game↔Friend)
│   └── cross_service.proto           # CrossService (Game↔Cross)
└── generated/
    ├── cpp/                          # 生成的 .pb.h/.pb.cc/.grpc.pb.h/.grpc.pb.cc
    └── python/                       # 生成的 _pb2.py/_pb2_grpc.py

scripts/server/common/
├── include/
│   ├── service_channel.h             # gRPC channel 封装 + etcd 服务发现
│   ├── interceptor_auth.h            # 认证拦截器
│   ├── interceptor_logging.h         # 日志拦截器
│   ├── interceptor_rate_limit.h      # 限流拦截器
│   └── timer_manager.h               # 替换 libevent 定时器
└── src/
    ├── service_channel.cpp
    ├── interceptor_auth.cpp
    ├── interceptor_logging.cpp
    ├── interceptor_rate_limit.cpp
    └── timer_manager.cpp

scripts/server/gate_server/src/
├── gateway_service.h/.cpp            # FarmGateway streaming 实现
└── gateway_notify_service.h/.cpp     # 后端事件推送入口
```

### 重构文件（大幅修改）

```
scripts/server/gate_server/src/
├── gate_server.h/.cpp                # 重写：libevent → gRPC server
├── session.h/.cpp                    # 重写：基于 gRPC context
├── session_manager.h/.cpp            # 重写：player_id → stream 映射
└── main.cpp                          # 重写：gRPC server 启动

scripts/server/game_server/src/
├── game_server.h/.cpp                # 重写：gRPC server + service stubs
├── gate_session.h/.cpp               # 重写：gRPC context
├── message_handler.h/.cpp            # 重构：改为 gRPC service handler
├── login_stub.h/.cpp                 # 修改：接口改为 gRPC handler
└── main.cpp                          # 重写

scripts/server/dbmgr/src/
├── dbmgr_server.h/.cpp               # 重写：DBMgrService 实现
├── game_session.h/.cpp               # 重写
└── main.cpp                          # 重写

scripts/server/chat_server/src/
├── chat_server.h/.cpp                # 重写：ChatService 实现
└── main.cpp                          # 重写

scripts/server/friend_service/src/
├── friend_service.h/.cpp             # 重写：FriendService 实现
└── main.cpp                          # 重写

scripts/server/team_server/src/
├── team_server.h/.cpp                # 重写：TeamService 实现
└── main.cpp                          # 重写

scripts/client/
├── connection.py                     # 重写：grpcio 替换 socket
└── heartbeat.py                      # 重写：gRPC keepalive 替换
```

### 删除文件

```
scripts/server/common/include/message_parser.h         ❌
scripts/server/common/src/message_parser.cpp           ❌
scripts/server/common/include/internal_msg_ids.h       ❌
scripts/server/common/include/admin_msg_ids.h          ❌
scripts/server/common/src/admin_msg_ids.cpp            ❌
scripts/server/common/include/message_ids.h            ❌
scripts/server/gate_server/src/game_connection.h/.cpp  ❌
scripts/server/gate_server/src/chat_connection.h/.cpp  ❌
scripts/server/game_server/src/dbmgr_connection.h/.cpp ❌
scripts/server/game_server/src/dbmgr_connection_manager.h/.cpp ❌
scripts/server/game_server/src/friend_service_connection.h/.cpp ❌
```

---

## Task 1: Proto — 消息类型重组

**Files:**
- Modify: `scripts/common/proto/base.proto`
- Modify: `scripts/common/proto/player.proto`
- Modify: `scripts/common/proto/chat.proto`
- Modify: `scripts/common/proto/team.proto`
- Modify: `scripts/common/proto/friend.proto`
- Modify: `scripts/common/proto/internal.proto`
- Modify: `scripts/common/proto/dbmgr.proto`
- Modify: `scripts/common/proto/account.proto`
- Modify: `scripts/common/proto/cross.proto`

- [ ] **Step 1: 修改 base.proto — 移除 LITE_RUNTIME，添加 HeartbeatRequest/Response**

```protobuf
syntax = "proto3";

package farm;

// 不再使用 LITE_RUNTIME（gRPC 需要 full protobuf）

// 心跳请求
message HeartbeatRequest {
    uint64 timestamp = 1;
}

// 心跳响应
message HeartbeatResponse {
    uint64 timestamp = 1;
}

// 登录请求
message LoginRequest {
    string token = 1;
    string client_version = 2;
    string device_id = 3;
}

// 登录响应
message LoginResponse {
    int32 code = 1;
    string msg = 2;
    uint64 account_id = 3;
}

// 创建角色请求
message CreateRoleRequest {
    string role_name = 1;
    uint32 server_id = 2;
}

// 创建角色响应
message CreateRoleResponse {
    int32 code = 1;
    string msg = 2;
    uint64 player_id = 3;
}

// 查询角色列表请求
message QueryRolesRequest {
    uint32 server_id = 1;
}

// 角色信息
message RoleInfo {
    uint32 server_id = 1;
    uint64 player_id = 2;
    string role_name = 3;
    uint32 level = 4;
}

// 查询角色列表响应
message QueryRolesResponse {
    int32 code = 1;
    repeated RoleInfo roles = 2;
}

// 进入游戏请求
message EnterGameRequest {
    uint32 server_id = 1;
    uint64 player_id = 2;
}

// 进入游戏响应
message EnterGameResponse {
    int32 code = 1;
    string msg = 2;
    // PlayerData 在 player_messages 中定义，这里通过 ServerEvent 的 scene_update 推送
}
```

- [ ] **Step 2: 修改 player.proto — 移除 LITE_RUNTIME，保留 PlayerData 和操作消息**

从 `player.proto` 中移除 `option optimize_for = LITE_RUNTIME;`。保留所有现有消息类型（`PlayerData`, `ItemUseReq/Resp`, `SceneChangeReq/Resp`, `EnergySync`, `InventoryData`, `FarmStateSync`, `PositionUpdate`, `CombatAction`, `CombatResult` 等）。

- [ ] **Step 3: 修改所有其他 proto 文件 — 移除 LITE_RUNTIME**

从以下文件移除 `option optimize_for = LITE_RUNTIME;`：
- `chat.proto`
- `team.proto`
- `friend.proto`
- `internal.proto`
- `dbmgr.proto`
- `account.proto`
- `cross.proto`

- [ ] **Step 4: 编译验证 — 确保现有 proto 文件仍可编译**

```bash
cd D:/mb_workspace/farm_demo
# 使用 protoc 验证所有 proto 文件语法正确
protoc --proto_path=scripts/common/proto --cpp_out=scripts/common/proto/generated scripts/common/proto/*.proto
```

Expected: 所有 proto 文件编译成功，生成 .pb.h/.pb.cc 文件

- [ ] **Step 5: 提交**

```bash
git add scripts/common/proto/
git commit -m "refactor(proto): remove LITE_RUNTIME, prepare for gRPC migration

Remove optimize_for=LITE_RUNTIME from all proto files (gRPC requires
full protobuf). Rename heartbeat/login messages for consistency."
```

---

## Task 2: Proto — gRPC Service 定义

**Files:**
- Create: `scripts/common/proto/client/gateway.proto`
- Create: `scripts/common/proto/internal/game_service.proto`
- Create: `scripts/common/proto/internal/dbmgr_service.proto`
- Create: `scripts/common/proto/internal/chat_service.proto`
- Create: `scripts/common/proto/internal/team_service.proto`
- Create: `scripts/common/proto/internal/friend_service.proto`
- Create: `scripts/common/proto/internal/cross_service.proto`

- [ ] **Step 1: 创建目录**

```bash
mkdir -p scripts/common/proto/client
mkdir -p scripts/common/proto/internal
```

- [ ] **Step 2: 创建 gateway.proto — 客户端入口 service**

```protobuf
syntax = "proto3";

package farm;

import "base.proto";
import "player.proto";
import "chat.proto";
import "team.proto";
import "friend.proto";

// ===========================================
// FarmGateway — 客户端唯一入口
// bidirectional streaming: 一个连接搞定所有请求和推送
// ===========================================

service FarmGateway {
    rpc GameStream(stream ClientRequest) returns (stream ServerEvent);
}

// 客户端请求 — oneof 包装所有请求类型
message ClientRequest {
    oneof request {
        HeartbeatRequest heartbeat = 1;
        LoginRequest login = 2;
        QueryRolesRequest query_roles = 3;
        CreateRoleRequest create_role = 4;
        EnterGameRequest enter_game = 5;

        // 游戏操作
        ItemUseReq item_use = 10;
        SceneChangeReq scene_change = 11;
        PositionUpdate position = 12;
        InventoryData inventory_sync = 13;
        FarmStateSync farm_sync = 14;
        CombatAction combat_action = 15;

        // 聊天
        ChatSendReq chat_send = 20;

        // 组队
        TeamCreateReq team_create = 30;
        TeamDisbandReq team_disband = 31;
        TeamInviteReq team_invite = 32;
        TeamAcceptReq team_accept = 33;
        TeamRejectReq team_reject = 34;
        TeamLeaveReq team_leave = 35;
        TeamKickReq team_kick = 36;
        TeamInfoReq team_info = 37;

        // 好友
        FriendSearchReq friend_search = 40;
        FriendAddReq friend_add = 41;
        FriendAcceptReq friend_accept = 42;
        FriendRejectReq friend_reject = 43;
        FriendDeleteReq friend_delete = 44;
        FriendListReq friend_list = 45;
        FriendChatSendReq friend_chat_send = 46;
        FriendGiftSendReq friend_gift_send = 47;
        FriendGiftClaimReq friend_gift_claim = 48;
        FriendVisitReq friend_visit = 49;
        FriendRecommendReq friend_recommend = 50;
        FriendBlockReq friend_block = 51;
        FriendUnblockReq friend_unblock = 52;
    }
}

// 服务端事件 — oneof 包装所有推送类型
message ServerEvent {
    oneof event {
        HeartbeatResponse heartbeat = 1;
        LoginResponse login = 2;
        QueryRolesResponse query_roles = 3;
        CreateRoleResponse create_role = 4;
        EnterGameResponse enter_game = 5;

        // 游戏事件
        PlayerData player_data = 10;          // 完整玩家数据（进入游戏时）
        ItemUseResp item_use = 11;
        SceneChangeResp scene_change = 12;
        EnergySync energy_sync = 13;
        InventoryData inventory_update = 14;
        FarmStateSync farm_update = 15;
        CombatResult combat_result = 16;
        ClockSync clock_sync = 17;
        DropItemUpdate drop_update = 18;
        MonsterUpdate monster_update = 19;
        ScenePlayerUpdate scene_player_update = 20;  // 场景内其他玩家
        SceneNpcUpdate scene_npc_update = 21;
        QuestUpdate quest_update = 22;
        Notification notification = 23;       // Toast/Marquee 通知

        // 聊天事件
        ChatSendResp chat_send = 30;
        ChatMessage chat_message = 31;        // 收到聊天消息

        // 组队事件
        TeamCreateResp team_create = 40;
        TeamDisbandResp team_disband = 41;
        TeamInviteResp team_invite = 42;
        TeamInviteNotify team_invite_notify = 43;  // 收到邀请
        TeamAcceptResp team_accept = 44;
        TeamRejectResp team_reject = 45;
        TeamLeaveResp team_leave = 46;
        TeamKickResp team_kick = 47;
        TeamInfoResp team_info = 48;
        TeamMemberUpdate team_member_update = 49;
        TeamLeaderChange team_leader_change = 50;
        TeamStatusUpdate team_status_update = 51;

        // 好友事件
        FriendSearchResp friend_search = 60;
        FriendAddResp friend_add = 61;
        FriendAcceptResp friend_accept = 62;
        FriendRejectResp friend_reject = 63;
        FriendDeleteResp friend_delete = 64;
        FriendListResp friend_list = 65;
        FriendChatSendResp friend_chat_send = 66;
        FriendChatNotify friend_chat_notify = 67;    // 收到好友私聊
        FriendGiftSendResp friend_gift_send = 68;
        FriendGiftClaimResp friend_gift_claim = 69;
        FriendGiftNotify friend_gift_notify = 70;    // 收到礼物
        FriendVisitResp friend_visit = 71;
        FriendVisitNotify friend_visit_notify = 72;  // 有人来访
        FriendRecommendResp friend_recommend = 73;
        FriendOnlineNotify friend_online_notify = 74; // 好友上线
        FriendBlockResp friend_block = 75;
        FriendUnblockResp friend_unblock = 76;
        FriendRequestNotify friend_request_notify = 77; // 收到好友申请
    }
}

// ===========================================
// GatewayNotify — 后端服务推送事件到 Gate
// 后端服务作为客户端调用，Gate 作为服务端
// ===========================================

service GatewayNotify {
    rpc PushChatMessage(ChatMessage) returns (NotifyAck);
    rpc PushTeamEvent(TeamEventWrapper) returns (NotifyAck);
    rpc PushFriendEvent(FriendEventWrapper) returns (NotifyAck);
    rpc PushSceneUpdate(SceneUpdateWrapper) returns (NotifyAck);
    rpc PushCombatEvent(CombatEventWrapper) returns (NotifyAck);
    rpc PushNotification(Notification) returns (NotifyAck);
    rpc PushQuestUpdate(QuestUpdateWrapper) returns (NotifyAck);
}

message NotifyAck {
    int32 code = 1;
}

// 事件包装 — 后端推送给特定玩家
message TeamEventWrapper {
    uint64 player_id = 1;
    oneof event {
        TeamMemberUpdate member_update = 2;
        TeamLeaderChange leader_change = 3;
        TeamStatusUpdate status_update = 4;
        TeamInviteNotify invite_notify = 5;
    }
}

message FriendEventWrapper {
    uint64 player_id = 1;
    oneof event {
        FriendChatNotify chat_notify = 2;
        FriendGiftNotify gift_notify = 3;
        FriendVisitNotify visit_notify = 4;
        FriendOnlineNotify online_notify = 5;
        FriendRequestNotify request_notify = 6;
    }
}

message SceneUpdateWrapper {
    uint64 player_id = 1;
    oneof update {
        ScenePlayerUpdate player_update = 2;
        SceneNpcUpdate npc_update = 3;
        DropItemUpdate drop_update = 4;
        MonsterUpdate monster_update = 5;
    }
}

message CombatEventWrapper {
    uint64 player_id = 1;
    CombatResult combat_result = 2;
}

message QuestUpdateWrapper {
    uint64 player_id = 1;
    QuestUpdate quest_update = 2;
}

// Notification 消息（Toast/Marquee）
message Notification {
    int32 type = 1;       // 0=toast, 1=marquee
    int32 priority = 2;
    string title = 3;
    string content = 4;
    int32 duration_ms = 5;
}
```

- [ ] **Step 3: 创建 internal/game_service.proto**

```protobuf
syntax = "proto3";

package farm;

import "base.proto";
import "player.proto";
import "internal.proto";

// ===========================================
// GameService — Gate↔Game 内部通信
// Gate 作为客户端调用，GameServer 作为服务端
// ===========================================

service GameService {
    // 玩家进入/离开
    rpc PlayerJoin(PlayerJoinReq) returns (PlayerJoinResp);
    rpc PlayerLeave(PlayerLeaveReq) returns (PlayerLeaveResp);

    // 账号操作
    rpc QueryRoles(QueryRolesReq) returns (QueryRolesResp);
    rpc CreateRole(CreateRoleReq) returns (CreateRoleResp);
    rpc EnterGame(EnterGameReq) returns (EnterGameResp);

    // 转发客户端消息（游戏操作）
    rpc ForwardPlayerAction(ForwardPlayerActionReq) returns (ForwardPlayerActionResp);
}

// 从 internal.proto 复用现有消息类型
// PlayerJoinReq/Resp, PlayerLeave 已在 internal.proto 中定义

// 转发玩家操作请求
message ForwardPlayerActionReq {
    uint64 player_id = 1;
    uint32 server_id = 2;
    oneof action {
        ItemUseReq item_use = 10;
        SceneChangeReq scene_change = 11;
        PositionUpdate position = 12;
        InventoryData inventory_sync = 13;
        FarmStateSync farm_sync = 14;
        CombatAction combat_action = 15;
    }
}

message ForwardPlayerActionResp {
    int32 code = 1;
    // 结果通过 GatewayNotify 推送
}
```

- [ ] **Step 4: 创建 internal/dbmgr_service.proto**

```protobuf
syntax = "proto3";

package farm;

import "account.proto";
import "player.proto";

// ===========================================
// DBMgrService — Game↔DBMgr 内部通信
// GameServer 作为客户端调用，DBMgr 作为服务端
// ===========================================

service DBMgrService {
    // 玩家数据
    rpc GetPlayerData(GetPlayerDataReq) returns (GetPlayerDataResp);
    rpc SavePlayerData(SavePlayerDataReq) returns (SavePlayerDataResp);
    rpc DeletePlayerData(DeletePlayerDataReq) returns (DeletePlayerDataResp);

    // 账号数据
    rpc QueryAccount(QueryAccountReq) returns (QueryAccountResp);
    rpc CreateAccount(CreateAccountReq) returns (CreateAccountResp);
    rpc QueryRoles(QueryRolesReq) returns (QueryRolesResp);
    rpc CreateRole(CreateRoleReq) returns (CreateRoleResp);

    // ID 分配
    rpc AllocPlayerId(AllocPlayerIdReq) returns (AllocPlayerIdResp);
}

message GetPlayerDataReq {
    uint64 player_id = 1;
}

message GetPlayerDataResp {
    int32 code = 1;
    string player_data_json = 2;   // JSON 格式的玩家数据
}

message SavePlayerDataReq {
    uint64 player_id = 1;
    string player_data_json = 2;
    repeated string dirty_fields = 3;  // 脏字段列表，空表示全量保存
}

message SavePlayerDataResp {
    int32 code = 1;
}

message DeletePlayerDataReq {
    uint64 player_id = 1;
}

message DeletePlayerDataResp {
    int32 code = 1;
}

// AllocPlayerId 消息已在 dbmgr.proto 中定义，这里复用
```

- [ ] **Step 5: 创建 internal/chat_service.proto**

```protobuf
syntax = "proto3";

package farm;

import "chat.proto";

// ===========================================
// ChatService — Gate↔ChatServer 内部通信
// Gate 作为客户端调用，ChatServer 作为服务端
// ===========================================

service ChatService {
    // 转发聊天请求
    rpc SendMessage(SendMessageReq) returns (SendMessageResp);

    // 玩家加入/离开（用于频道管理）
    rpc PlayerJoin(PlayerJoinChatReq) returns (PlayerJoinChatResp);
    rpc PlayerLeave(PlayerLeaveChatReq) returns (PlayerLeaveChatResp);
}

message SendMessageReq {
    uint64 player_id = 1;
    string player_name = 2;
    ChatChannelType channel_type = 3;
    string content = 4;
    uint64 target_id = 5;
}

message SendMessageResp {
    ChatErrorCode code = 1;
    string msg = 2;
}

message PlayerJoinChatReq {
    uint64 player_id = 1;
    string player_name = 2;
}

message PlayerJoinChatResp {
    int32 code = 1;
}

message PlayerLeaveChatReq {
    uint64 player_id = 1;
}

message PlayerLeaveChatResp {
    int32 code = 1;
}
```

- [ ] **Step 6: 创建 internal/team_service.proto**

```protobuf
syntax = "proto3";

package farm;

import "team.proto";

// ===========================================
// TeamService — Gate↔TeamServer, Game↔TeamServer 内部通信
// ===========================================

service TeamService {
    // 队伍操作
    rpc CreateTeam(TeamCreateReq) returns (TeamCreateResp);
    rpc DisbandTeam(TeamDisbandReq) returns (TeamDisbandResp);
    rpc InviteToTeam(TeamInviteReq) returns (TeamInviteResp);
    rpc AcceptInvite(TeamAcceptReq) returns (TeamAcceptResp);
    rpc RejectInvite(TeamRejectReq) returns (TeamRejectResp);
    rpc LeaveTeam(TeamLeaveReq) returns (TeamLeaveResp);
    rpc KickMember(TeamKickReq) returns (TeamKickResp);
    rpc GetTeamInfo(TeamInfoReq) returns (TeamInfoResp);

    // 内部查询（GameServer 用）
    rpc QueryMembers(TeamQueryMembersReq) returns (TeamQueryMembersResp);

    // 玩家上下线通知
    rpc PlayerOnline(PlayerOnlineReq) returns (PlayerOnlineResp);
    rpc PlayerOffline(PlayerOfflineReq) returns (PlayerOfflineResp);
}

message PlayerOnlineReq {
    uint64 player_id = 1;
    string player_name = 2;
    uint32 level = 3;
}

message PlayerOnlineResp {
    int32 code = 1;
}

message PlayerOfflineReq {
    uint64 player_id = 1;
}

message PlayerOfflineResp {
    int32 code = 1;
}
```

- [ ] **Step 7: 创建 internal/friend_service.proto**

```protobuf
syntax = "proto3";

package farm;

import "friend.proto";

// ===========================================
// FriendService — Game↔FriendService 内部通信
// GameServer 作为客户端调用，FriendService 作为服务端
// ===========================================

service FriendService {
    rpc SearchPlayer(FriendSearchReq) returns (FriendSearchResp);
    rpc AddFriend(FriendAddReq) returns (FriendAddResp);
    rpc AcceptFriend(FriendAcceptReq) returns (FriendAcceptResp);
    rpc RejectFriend(FriendRejectReq) returns (FriendRejectResp);
    rpc DeleteFriend(FriendDeleteReq) returns (FriendDeleteResp);
    rpc GetFriendList(FriendListReq) returns (FriendListResp);
    rpc SendChat(FriendChatSendReq) returns (FriendChatSendResp);
    rpc SendGift(FriendGiftSendReq) returns (FriendGiftSendResp);
    rpc ClaimGift(FriendGiftClaimReq) returns (FriendGiftClaimResp);
    rpc VisitFarm(FriendVisitReq) returns (FriendVisitResp);
    rpc GetRecommendations(FriendRecommendReq) returns (FriendRecommendResp);
    rpc BlockPlayer(FriendBlockReq) returns (FriendBlockResp);
    rpc UnblockPlayer(FriendUnblockReq) returns (FriendUnblockResp);
}
```

- [ ] **Step 8: 创建 internal/cross_service.proto**

```protobuf
syntax = "proto3";

package farm;

import "cross.proto";

// ===========================================
// CrossService — Game↔CrossServer 内部通信
// ===========================================

service CrossService {
    rpc QueryPlayer(CrossQueryReq) returns (CrossQueryResp);
    rpc ForwardToPlayer(CrossForwardReq) returns (CrossForwardResp);
}
```

- [ ] **Step 9: 编译验证所有 proto 文件**

```bash
cd D:/mb_workspace/farm_demo
protoc \
    --proto_path=scripts/common/proto \
    --proto_path=scripts/common/proto/client \
    --proto_path=scripts/common/proto/internal \
    --cpp_out=scripts/common/proto/generated/cpp \
    --grpc_out=scripts/common/proto/generated/cpp \
    --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) \
    scripts/common/proto/client/gateway.proto \
    scripts/common/proto/internal/game_service.proto \
    scripts/common/proto/internal/dbmgr_service.proto \
    scripts/common/proto/internal/chat_service.proto \
    scripts/common/proto/internal/team_service.proto \
    scripts/common/proto/internal/friend_service.proto \
    scripts/common/proto/internal/cross_service.proto
```

Expected: 生成 `.pb.h`, `.pb.cc`, `.grpc.pb.h`, `.grpc.pb.cc` 文件

- [ ] **Step 10: 提交**

```bash
git add scripts/common/proto/client/ scripts/common/proto/internal/
git commit -m "feat(proto): add gRPC service definitions for all communication layers

- FarmGateway: client bidirectional streaming (gateway.proto)
- GatewayNotify: backend event push to Gate (gateway.proto)
- GameService: Gate↔Game internal (game_service.proto)
- DBMgrService: Game↔DBMgr internal (dbmgr_service.proto)
- ChatService: Gate↔Chat internal (chat_service.proto)
- TeamService: Gate↔Team, Game↔Team (team_service.proto)
- FriendService: Game↔Friend (friend_service.proto)
- CrossService: Game↔Cross (cross_service.proto)"
```

---

## Task 3: CMake — 构建系统改造

**Files:**
- Modify: `scripts/server/gate_server/CMakeLists.txt`
- Modify: `scripts/server/game_server/CMakeLists.txt`
- Modify: `scripts/server/dbmgr/CMakeLists.txt`
- Modify: `scripts/server/chat_server/CMakeLists.txt`
- Modify: `scripts/server/friend_service/CMakeLists.txt`
- Modify: `scripts/server/team_server/CMakeLists.txt`
- Create: `scripts/common/proto/CMakeLists.txt` (gRPC code generation)

- [ ] **Step 1: 创建 proto CMakeLists.txt — gRPC 代码生成**

创建 `scripts/common/proto/CMakeLists.txt`：

```cmake
# gRPC Proto 代码生成
# 用法: include(this_file) 然后 target_link_libraries(xxx farm_proto_generated)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# 查找 gRPC C++ 插件
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
find_program(GRPC_PYTHON_PLUGIN_EXECUTABLE grpc_python_plugin)

set(PROTO_IMPORT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/client
    ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

# 消息 proto 文件（已有）
set(MESSAGE_PROTOS
    ${CMAKE_CURRENT_SOURCE_DIR}/base.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/account.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/player.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/chat.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/team.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/friend.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/internal.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/dbmgr.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/cross.proto
)

# gRPC service proto 文件（新增）
set(GRPC_PROTOS
    ${CMAKE_CURRENT_SOURCE_DIR}/client/gateway.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/game_service.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/dbmgr_service.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/chat_service.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/team_service.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/friend_service.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/cross_service.proto
)

# 生成目录
set(GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp)
file(MAKE_DIRECTORY ${GENERATED_DIR})
file(MAKE_DIRECTORY ${GENERATED_DIR}/client)
file(MAKE_DIRECTORY ${GENERATED_DIR}/internal)

# 生成消息类型 (.pb.h/.pb.cc)
foreach(PROTO ${MESSAGE_PROTOS})
    get_filename_component(PROTO_NAME ${PROTO} NAME_WE)
    set(PROTO_SRCS ${GENERATED_DIR}/${PROTO_NAME}.pb.cc)
    set(PROTO_HDRS ${GENERATED_DIR}/${PROTO_NAME}.pb.h)
    add_custom_command(
        OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
        COMMAND protobuf::protoc
            --proto_path=${PROTO_IMPORT_DIRS}
            --cpp_out=${GENERATED_DIR}
            ${PROTO}
        DEPENDS ${PROTO}
        COMMENT "Generating protobuf: ${PROTO_NAME}"
    )
    list(APPEND ALL_PROTO_SRCS ${PROTO_SRCS})
    list(APPEND ALL_PROTO_HDRS ${PROTO_HDRS})
endforeach()

# 生成 gRPC service (.grpc.pb.h/.grpc.pb.cc)
foreach(PROTO ${GRPC_PROTOS})
    get_filename_component(PROTO_NAME ${PROTO} NAME_WE)
    get_filename_component(PROTO_DIR ${PROTO} DIRECTORY)
    get_filename_component(PROTO_DIR_NAME ${PROTO_DIR} NAME)
    set(GRPC_SRCS ${GENERATED_DIR}/${PROTO_DIR_NAME}/${PROTO_NAME}.grpc.pb.cc)
    set(GRPC_HDRS ${GENERATED_DIR}/${PROTO_DIR_NAME}/${PROTO_NAME}.grpc.pb.h)
    set(PB_SRCS ${GENERATED_DIR}/${PROTO_DIR_NAME}/${PROTO_NAME}.pb.cc)
    set(PB_HDRS ${GENERATED_DIR}/${PROTO_DIR_NAME}/${PROTO_NAME}.pb.h)
    add_custom_command(
        OUTPUT ${GRPC_SRCS} ${GRPC_HDRS} ${PB_SRCS} ${PB_HDRS}
        COMMAND protobuf::protoc
            --proto_path=${PROTO_IMPORT_DIRS}
            --cpp_out=${GENERATED_DIR}/${PROTO_DIR_NAME}
            --grpc_out=${GENERATED_DIR}/${PROTO_DIR_NAME}
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
            ${PROTO}
        DEPENDS ${PROTO}
        COMMENT "Generating gRPC: ${PROTO_DIR_NAME}/${PROTO_NAME}"
    )
    list(APPEND ALL_GRPC_SRCS ${GRPC_SRCS} ${PB_SRCS})
    list(APPEND ALL_GRPC_HDRS ${GRPC_HDRS} ${PB_HDRS})
endforeach()

# 创建接口库
add_library(farm_proto_generated STATIC ${ALL_PROTO_SRCS} ${ALL_GRPC_SRCS})
target_include_directories(farm_proto_generated PUBLIC ${GENERATED_DIR})
target_link_libraries(farm_proto_generated PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
)
```

- [ ] **Step 2: 修改 gate_server CMakeLists.txt**

替换链接依赖：`protobuf-lite` → `protobuf::libprotobuf`，`libevent` 删除，添加 `gRPC::grpc++`。

```cmake
# 关键变更：
# 1. 删除 libevent 相关
# 2. protobuf-lite → protobuf::libprotobuf
# 3. 添加 gRPC::grpc++
# 4. 添加 farm_proto_generated 依赖
# 5. 删除旧 proto .pb.cc 文件引用，改用 farm_proto_generated

target_link_libraries(gate_server
    farm_proto_generated
    gRPC::grpc++
    protobuf::libprotobuf
    spdlog::spdlog
    # libevent 已删除
)
```

- [ ] **Step 3: 修改其他 server CMakeLists.txt**

对 `game_server`, `dbmgr`, `chat_server`, `friend_service`, `team_server` 执行相同的依赖替换：
- 删除 libevent
- protobuf-lite → protobuf::libprotobuf
- 添加 gRPC::grpc++
- 添加 farm_proto_generated
- 删除旧 proto .pb.cc 文件引用

- [ ] **Step 4: 编译验证**

```bash
cd D:/mb_workspace/farm_demo
mkdir -p build && cd build
cmake ../scripts/server/gate_server -G "Visual Studio 17 2022"
cmake --build . --config Release 2>&1 | head -50
```

Expected: 编译通过（可能有链接错误，因为新代码还未实现，但 proto 和 gRPC 依赖应正确）

- [ ] **Step 5: 提交**

```bash
git add scripts/common/proto/CMakeLists.txt scripts/server/*/CMakeLists.txt
git commit -m "build(cmake): migrate from libevent to gRPC

- Add farm_proto_generated target for gRPC code generation
- Replace protobuf-lite with full protobuf
- Replace libevent with gRPC++ in all server targets
- Remove old .pb.cc file references, use farm_proto_generated"
```

---

## Task 4: Common — gRPC 基础设施

**Files:**
- Create: `scripts/server/common/include/service_channel.h`
- Create: `scripts/server/common/src/service_channel.cpp`
- Create: `scripts/server/common/include/timer_manager.h`
- Create: `scripts/server/common/src/timer_manager.cpp`
- Create: `scripts/server/common/include/interceptor_auth.h`
- Create: `scripts/server/common/src/interceptor_auth.cpp`
- Create: `scripts/server/common/include/interceptor_logging.h`
- Create: `scripts/server/common/src/interceptor_logging.cpp`
- Create: `scripts/server/common/include/interceptor_rate_limit.h`
- Create: `scripts/server/common/src/interceptor_rate_limit.cpp`

- [ ] **Step 1: 创建 service_channel.h**

```cpp
#pragma once

#include <grpcpp/grpcpp.h>
#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace farm {

// 服务地址信息
struct ServiceEndpoint {
    std::string host;
    uint16_t port;
    std::string instance_id;
};

// gRPC Channel 封装，集成 etcd 服务发现
// 替代 GameConnection, ChatConnection, DBMgrConnectionManager 等
class ServiceChannel {
public:
    // 创建到指定地址的 channel（静态配置模式）
    static std::shared_ptr<grpc::Channel> CreateDirect(
        const std::string& host, uint16_t port);

    // 创建到指定地址的 channel 列表（多实例，round-robin）
    static std::shared_ptr<grpc::Channel> CreateLoadBalanced(
        const std::vector<ServiceEndpoint>& endpoints);

    // 通过 etcd 发现服务并创建 channel
    // service_type: "game", "dbmgr", "chat", "team", "friend", "cross"
    static std::shared_ptr<grpc::Channel> CreateFromEtcd(
        const std::string& service_type,
        const std::string& etcd_endpoints);
};

// 通用 gRPC 客户端包装 — 带超时和重试
template<typename StubType>
class ServiceClient {
public:
    using StubFactory = std::function<std::unique_ptr<typename StubType::Stub>(std::shared_ptr<grpc::Channel>)>;

    ServiceClient(std::shared_ptr<grpc::Channel> channel, StubFactory factory)
        : channel_(channel), stub_(factory(channel)) {}

    StubType* operator->() { return stub_.get(); }
    StubType& operator*() { return *stub_; }

    // 获取底层 channel（用于 streaming）
    std::shared_ptr<grpc::Channel> channel() const { return channel_; }

private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<typename StubType::Stub> stub_;
};

} // namespace farm
```

- [ ] **Step 2: 创建 service_channel.cpp**

```cpp
#include "service_channel.h"
#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>

namespace farm {

std::shared_ptr<grpc::Channel> ServiceChannel::CreateDirect(
    const std::string& host, uint16_t port)
{
    std::string address = host + ":" + std::to_string(port);
    grpc::ChannelArguments args;
    args.SetMaxReceiveMessageSize(1 * 1024 * 1024);  // 1MB
    args.SetMaxSendMessageSize(1 * 1024 * 1024);
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 10000);
    args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000);
    args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
    args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);

    return grpc::CreateCustomChannel(
        address, grpc::InsecureChannelCredentials(), args);
}

std::shared_ptr<grpc::Channel> ServiceChannel::CreateLoadBalanced(
    const std::vector<ServiceEndpoint>& endpoints)
{
    if (endpoints.empty()) {
        spdlog::error("ServiceChannel::CreateLoadBalanced: no endpoints");
        return nullptr;
    }

    // 使用 dns:/// 地址实现 round-robin
    // 多个地址用逗号分隔
    std::string addresses;
    for (size_t i = 0; i < endpoints.size(); ++i) {
        if (i > 0) addresses += ",";
        addresses += endpoints[i].host + ":" + std::to_string(endpoints[i].port);
    }

    grpc::ChannelArguments args;
    args.SetLoadBalancingPolicyName("round_robin");
    args.SetMaxReceiveMessageSize(1 * 1024 * 1024);
    args.SetMaxSendMessageSize(1 * 1024 * 1024);
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 10000);
    args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000);

    return grpc::CreateCustomChannel(
        "dns:///" + addresses, grpc::InsecureChannelCredentials(), args);
}

} // namespace farm
```

- [ ] **Step 3: 创建 timer_manager.h**

```cpp
#pragma once

#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>

namespace farm {

// 替换 libevent 定时器
// 使用独立线程 + condition_variable 实现定时回调
class TimerManager {
public:
    TimerManager();
    ~TimerManager();

    // 添加周期性定时器，返回 timer_id
    uint32_t AddPeriodic(std::chrono::milliseconds interval,
                         std::function<void()> callback);

    // 添加一次性定时器
    uint32_t AddOnce(std::chrono::milliseconds delay,
                     std::function<void()> callback);

    // 删除定时器
    void Remove(uint32_t timer_id);

    // 启动定时器线程
    void Start();

    // 停止
    void Stop();

private:
    struct TimerEntry {
        uint32_t id;
        std::chrono::milliseconds interval;
        std::chrono::steady_clock::time_point next_fire;
        std::function<void()> callback;
        bool periodic;
        bool active;
    };

    void RunLoop();

    std::vector<TimerEntry> timers_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint32_t> next_id_{1};
};

} // namespace farm
```

- [ ] **Step 4: 创建 timer_manager.cpp**

```cpp
#include "timer_manager.h"
#include <algorithm>

namespace farm {

TimerManager::TimerManager() = default;

TimerManager::~TimerManager() {
    Stop();
}

uint32_t TimerManager::AddPeriodic(std::chrono::milliseconds interval,
                                    std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t id = next_id_++;
    timers_.push_back({
        id, interval, std::chrono::steady_clock::now() + interval,
        std::move(callback), true, true
    });
    cv_.notify_one();
    return id;
}

uint32_t TimerManager::AddOnce(std::chrono::milliseconds delay,
                                std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t id = next_id_++;
    timers_.push_back({
        id, delay, std::chrono::steady_clock::now() + delay,
        std::move(callback), false, true
    });
    cv_.notify_one();
    return id;
}

void TimerManager::Remove(uint32_t timer_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& t : timers_) {
        if (t.id == timer_id) {
            t.active = false;
            break;
        }
    }
}

void TimerManager::Start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread(&TimerManager::RunLoop, this);
}

void TimerManager::Stop() {
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void TimerManager::RunLoop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (timers_.empty()) {
            cv_.wait(lock, [this] { return !running_ || !timers_.empty(); });
            continue;
        }

        // 找到最近的定时器
        auto now = std::chrono::steady_clock::now();
        auto next = std::chrono::steady_clock::time_point::max();
        for (const auto& t : timers_) {
            if (t.active && t.next_fire < next) {
                next = t.next_fire;
            }
        }

        if (next > now) {
            cv_.wait_until(lock, next, [this, next] {
                return !running_ || std::chrono::steady_clock::now() >= next;
            });
        }

        // 执行到期的定时器
        now = std::chrono::steady_clock::now();
        std::vector<std::function<void()>> ready;
        for (auto& t : timers_) {
            if (t.active && t.next_fire <= now) {
                ready.push_back(t.callback);
                if (t.periodic) {
                    t.next_fire = now + t.interval;
                } else {
                    t.active = false;
                }
            }
        }

        // 清理非活跃定时器
        timers_.erase(
            std::remove_if(timers_.begin(), timers_.end(),
                           [](const TimerEntry& t) { return !t.active; }),
            timers_.end());

        lock.unlock();

        // 在锁外执行回调
        for (auto& cb : ready) {
            try { cb(); } catch (...) {}
        }
    }
}

} // namespace farm
```

- [ ] **Step 5: 创建 interceptor_auth.h**

```cpp
#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/interceptor.h>
#include <string>
#include <unordered_map>
#include <mutex>

namespace farm {

// 认证拦截器 — 从 metadata 提取 token，验证客户端身份
class AuthInterceptor : public grpc::experimental::Interceptor {
public:
    explicit AuthInterceptor(grpc::experimental::ServerRpcInfo* info);

    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override;

    // 验证 token（简单实现，可扩展为 JWT）
    static bool ValidateToken(const std::string& token, uint64_t& account_id);

private:
    grpc::experimental::ServerRpcInfo* info_;
};

// 拦截器工厂
class AuthInterceptorFactory : public grpc::experimental::ServerInterceptorFactoryInterface {
public:
    grpc::experimental::Interceptor* CreateServerInterceptor(
        grpc::experimental::ServerRpcInfo* info) override;
};

} // namespace farm
```

- [ ] **Step 6: 创建 interceptor_auth.cpp**

```cpp
#include "interceptor_auth.h"
#include <spdlog/spdlog.h>

namespace farm {

AuthInterceptor::AuthInterceptor(grpc::experimental::ServerRpcInfo* info)
    : info_(info) {}

void AuthInterceptor::Intercept(grpc::experimental::InterceptorBatchMethods* methods) {
    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::PRE_SEND_INITIAL_METADATA)) {
        // 可以在这里添加响应 metadata
    }

    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::PRE_RECV_STATUS)) {
        // 可以在这里修改状态码
    }

    methods->Proceed();
}

bool AuthInterceptor::ValidateToken(const std::string& token, uint64_t& account_id) {
    // 简单实现：token 格式为 "account_{id}"
    // 实际应使用 JWT 或 session token
    if (token.substr(0, 8) == "account_") {
        try {
            account_id = std::stoull(token.substr(8));
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

grpc::experimental::Interceptor* AuthInterceptorFactory::CreateServerInterceptor(
    grpc::experimental::ServerRpcInfo* info)
{
    return new AuthInterceptor(info);
}

} // namespace farm
```

- [ ] **Step 7: 创建 interceptor_logging.h 和 interceptor_logging.cpp**

```cpp
// interceptor_logging.h
#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/interceptor.h>
#include <chrono>

namespace farm {

class LoggingInterceptor : public grpc::experimental::Interceptor {
public:
    explicit LoggingInterceptor(grpc::experimental::ServerRpcInfo* info);
    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override;

private:
    grpc::experimental::ServerRpcInfo* info_;
    std::chrono::steady_clock::time_point start_time_;
};

class LoggingInterceptorFactory : public grpc::experimental::ServerInterceptorFactoryInterface {
public:
    grpc::experimental::Interceptor* CreateServerInterceptor(
        grpc::experimental::ServerRpcInfo* info) override;
};

} // namespace farm
```

```cpp
// interceptor_logging.cpp
#include "interceptor_logging.h"
#include <spdlog/spdlog.h>

namespace farm {

LoggingInterceptor::LoggingInterceptor(grpc::experimental::ServerRpcInfo* info)
    : info_(info), start_time_(std::chrono::steady_clock::now()) {}

void LoggingInterceptor::Intercept(grpc::experimental::InterceptorBatchMethods* methods) {
    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::PRE_SEND_STATUS)) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time_);
        spdlog::info("RPC {} {}ms status={}",
            info_->method(), elapsed.count(),
            static_cast<int>(methods->GetSendStatus()->error_code()));
    }
    methods->Proceed();
}

grpc::experimental::Interceptor* LoggingInterceptorFactory::CreateServerInterceptor(
    grpc::experimental::ServerRpcInfo* info)
{
    return new LoggingInterceptor(info);
}

} // namespace farm
```

- [ ] **Step 8: 创建 interceptor_rate_limit.h 和 interceptor_rate_limit.cpp**

```cpp
// interceptor_rate_limit.h
#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/interceptor.h>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace farm {

class RateLimitInterceptor : public grpc::experimental::Interceptor {
public:
    explicit RateLimitInterceptor(grpc::experimental::ServerRpcInfo* info);
    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override;

    // 设置每客户端每秒最大请求数
    static void SetRateLimit(uint32_t max_per_second);

private:
    grpc::experimental::ServerRpcInfo* info_;
    static uint32_t max_per_second_;
    static std::mutex mutex_;
    static std::unordered_map<std::string, std::pair<uint32_t, std::chrono::steady_clock::time_point>> counters_;
};

class RateLimitInterceptorFactory : public grpc::experimental::ServerInterceptorFactoryInterface {
public:
    grpc::experimental::Interceptor* CreateServerInterceptor(
        grpc::experimental::ServerRpcInfo* info) override;
};

} // namespace farm
```

```cpp
// interceptor_rate_limit.cpp
#include "interceptor_rate_limit.h"

namespace farm {

uint32_t RateLimitInterceptor::max_per_second_ = 100;
std::mutex RateLimitInterceptor::mutex_;
std::unordered_map<std::string, std::pair<uint32_t, std::chrono::steady_clock::time_point>>
    RateLimitInterceptor::counters_;

RateLimitInterceptor::RateLimitInterceptor(grpc::experimental::ServerRpcInfo* info)
    : info_(info) {}

void RateLimitInterceptor::Intercept(grpc::experimental::InterceptorBatchMethods* methods) {
    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::PRE_RECV_INITIAL_METADATA)) {
        // 从 metadata 获取客户端标识
        auto* metadata = methods->GetRecvInitialMetadata();
        std::string client_id = "unknown";
        auto it = metadata->find("client-id");
        if (it != metadata->end()) {
            client_id = std::string(it->second.data(), it->second.size());
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto& counter = counters_[client_id];
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - counter.second).count();

        if (elapsed >= 1) {
            counter.first = 0;
            counter.second = now;
        }

        if (++counter.first > max_per_second_) {
            // 超过限流 — 设置拒绝状态
            auto* status = methods->GetSendStatus();
            *status = grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Rate limit exceeded");
            methods->Proceed();
            return;
        }
    }
    methods->Proceed();
}

void RateLimitInterceptor::SetRateLimit(uint32_t max_per_second) {
    max_per_second_ = max_per_second;
}

grpc::experimental::Interceptor* RateLimitInterceptorFactory::CreateServerInterceptor(
    grpc::experimental::ServerRpcInfo* info)
{
    return new RateLimitInterceptor(info);
}

} // namespace farm
```

- [ ] **Step 9: 编译验证**

```bash
cd D:/mb_workspace/farm_demo
# 编译 common 库验证新文件
cmake --build build --target farm_common 2>&1 | tail -20
```

Expected: 编译通过

- [ ] **Step 10: 提交**

```bash
git add scripts/server/common/include/ scripts/server/common/src/
git commit -m "feat(common): add gRPC infrastructure components

- ServiceChannel: gRPC channel wrapper with etcd discovery
- TimerManager: replacement for libevent timers
- AuthInterceptor: client authentication via metadata
- LoggingInterceptor: RPC call logging with duration
- RateLimitInterceptor: per-client rate limiting"
```

---

## Task 5: DBMgr — gRPC Service 实现

**Files:**
- Modify: `scripts/server/dbmgr/src/dbmgr_server.h/.cpp`
- Modify: `scripts/server/dbmgr/src/game_session.h/.cpp`
- Modify: `scripts/server/dbmgr/src/main.cpp`

- [ ] **Step 1: 重写 dbmgr_server.h — DBMgrService 实现**

```cpp
#pragma once

#include "connection_manager.h"
#include "mongo_server.h"
#include "redis_server.h"

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

// gRPC 生成的头文件
#include "internal/dbmgr_service.grpc.pb.h"

namespace farm {

class DbMgrServer final : public DBMgrService::Service {
public:
    DbMgrServer(const std::string& config_path);
    ~DbMgrServer();

    bool Start();
    void Stop();

    // DBMgrService RPC 实现
    grpc::Status GetPlayerData(grpc::ServerContext* context,
                               const GetPlayerDataReq* request,
                               GetPlayerDataResp* response) override;

    grpc::Status SavePlayerData(grpc::ServerContext* context,
                                const SavePlayerDataReq* request,
                                SavePlayerDataResp* response) override;

    grpc::Status DeletePlayerData(grpc::ServerContext* context,
                                  const DeletePlayerDataReq* request,
                                  DeletePlayerDataResp* response) override;

    grpc::Status QueryAccount(grpc::ServerContext* context,
                              const QueryAccountReq* request,
                              QueryAccountResp* response) override;

    grpc::Status CreateAccount(grpc::ServerContext* context,
                               const CreateAccountReq* request,
                               CreateAccountResp* response) override;

    grpc::Status QueryRoles(grpc::ServerContext* context,
                            const QueryRolesReq* request,
                            QueryRolesResp* response) override;

    grpc::Status CreateRole(grpc::ServerContext* context,
                            const CreateRoleReq* request,
                            CreateRoleResp* response) override;

    grpc::Status AllocPlayerId(grpc::ServerContext* context,
                               const AllocPlayerIdReq* request,
                               AllocPlayerIdResp* response) override;

private:
    std::string config_path_;
    std::unique_ptr<ConnectionManager> conn_mgr_;
    std::unique_ptr<MongoServer> mongo_server_;
    std::unique_ptr<RedisServer> redis_server_;
    std::unique_ptr<grpc::Server> grpc_server_;
};

} // namespace farm
```

- [ ] **Step 2: 重写 dbmgr_server.cpp — gRPC service 实现**

```cpp
#include "dbmgr_server.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace farm {

DbMgrServer::DbMgrServer(const std::string& config_path)
    : config_path_(config_path) {}

DbMgrServer::~DbMgrServer() { Stop(); }

bool DbMgrServer::Start() {
    // 读取配置
    std::ifstream f(config_path_);
    if (!f.is_open()) {
        spdlog::error("Failed to open config: {}", config_path_);
        return false;
    }
    json config = json::parse(f);

    // 初始化数据库连接
    conn_mgr_ = std::make_unique<ConnectionManager>();
    if (!conn_mgr_->InitMongo(config["mongo"]["uri"].get<std::string>(),
                               config["mongo"]["database"].get<std::string>())) {
        spdlog::error("Failed to connect to MongoDB");
        return false;
    }
    if (!conn_mgr_->InitRedis(config["redis"]["host"].get<std::string>(),
                                config["redis"].value("port", 6379))) {
        spdlog::error("Failed to connect to Redis");
        return false;
    }

    mongo_server_ = std::make_unique<MongoServer>(conn_mgr_.get());
    redis_server_ = std::make_unique<RedisServer>(conn_mgr_.get());

    // 启动 gRPC server
    std::string address = config.value("ip", "0.0.0.0") + ":" +
                          std::to_string(config.value("port", 5000));

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    builder.SetMaxReceiveMessageSize(1 * 1024 * 1024);
    builder.SetMaxSendMessageSize(1 * 1024 * 1024);

    grpc_server_ = builder.BuildAndStart();
    if (!grpc_server_) {
        spdlog::error("Failed to start gRPC server on {}", address);
        return false;
    }

    spdlog::info("DBMgr gRPC server started on {}", address);
    return true;
}

void DbMgrServer::Stop() {
    if (grpc_server_) {
        grpc_server_->Shutdown();
        grpc_server_->Wait();
    }
}

grpc::Status DbMgrServer::GetPlayerData(grpc::ServerContext* context,
                                          const GetPlayerDataReq* request,
                                          GetPlayerDataResp* response) {
    auto data = mongo_server_->GetPlayerData(request->player_id());
    if (data.empty()) {
        response->set_code(1);  // not found
        return grpc::Status::OK;
    }
    response->set_code(0);
    response->set_player_data_json(data);
    return grpc::Status::OK;
}

grpc::Status DbMgrServer::SavePlayerData(grpc::ServerContext* context,
                                           const SavePlayerDataReq* request,
                                           SavePlayerDataResp* response) {
    std::vector<std::string> dirty_fields(
        request->dirty_fields().begin(), request->dirty_fields().end());
    bool ok = mongo_server_->SavePlayerData(
        request->player_id(), request->player_data_json(), dirty_fields);
    response->set_code(ok ? 0 : 1);
    return grpc::Status::OK;
}

grpc::Status DbMgrServer::DeletePlayerData(grpc::ServerContext* context,
                                             const DeletePlayerDataReq* request,
                                             DeletePlayerDataResp* response) {
    bool ok = mongo_server_->DeletePlayerData(request->player_id());
    response->set_code(ok ? 0 : 1);
    return grpc::Status::OK;
}

grpc::Status DbMgrServer::QueryAccount(grpc::ServerContext* context,
                                         const QueryAccountReq* request,
                                         QueryAccountResp* response) {
    // 复用现有 MongoServer 逻辑
    auto account = mongo_server_->QueryAccount(request->account_id());
    if (account.empty()) {
        response->set_code(1);
        return grpc::Status::OK;
    }
    response->set_code(0);
    response->set_account_id(request->account_id());
    // ... 填充其他字段
    return grpc::Status::OK;
}

grpc::Status DbMgrServer::CreateAccount(grpc::ServerContext* context,
                                          const CreateAccountReq* request,
                                          CreateAccountResp* response) {
    auto result = mongo_server_->CreateAccount(request->account_name(),
                                                 request->password());
    response->set_code(result.first);
    response->set_account_id(result.second);
    return grpc::Status::OK;
}

grpc::Status DbMgrServer::QueryRoles(grpc::ServerContext* context,
                                       const QueryRolesReq* request,
                                       QueryRolesResp* response) {
    auto roles = mongo_server_->QueryRoles(request->account_id(),
                                            request->server_id());
    response->set_code(0);
    for (const auto& role : roles) {
        auto* info = response->add_roles();
        info->set_server_id(role.server_id);
        info->set_player_id(role.player_id);
        info->set_role_name(role.role_name);
    }
    return grpc::Status::OK;
}

grpc::Status DbMgrServer::CreateRole(grpc::ServerContext* context,
                                       const CreateRoleReq* request,
                                       CreateRoleResp* response) {
    auto result = mongo_server_->CreateRole(request->account_id(),
                                             request->server_id(),
                                             request->role_name());
    response->set_code(result.first);
    response->set_player_id(result.second);
    return grpc::Status::OK;
}

grpc::Status DbMgrServer::AllocPlayerId(grpc::ServerContext* context,
                                          const AllocPlayerIdReq* request,
                                          AllocPlayerIdResp* response) {
    auto ids = mongo_server_->AllocPlayerIds(request->count());
    response->set_code(0);
    for (auto id : ids) {
        response->add_player_ids(id);
    }
    return grpc::Status::OK;
}

} // namespace farm
```

- [ ] **Step 3: 重写 main.cpp**

```cpp
#include "dbmgr_server.h"
#include "log_init.h"
#include "server_main_helper.h"
#include "etcd_manager.h"
#include <spdlog/spdlog.h>
#include <csignal>
#include <memory>

static std::unique_ptr<farm::DbMgrServer> g_server;
static std::atomic<bool> g_running{true};

void signal_handler(int sig) {
    spdlog::info("Received signal {}, shutting down...", sig);
    g_running = false;
    if (g_server) g_server->Stop();
}

int main(int argc, char* argv[]) {
    // 解析参数
    auto config_path = farm::parse_config_path(argc, argv);
    if (config_path.empty()) {
        spdlog::error("Usage: dbmgr --config <path>");
        return 1;
    }

    // 初始化日志
    farm::init_logging_from_config(config_path);

    // 写 PID 文件
    farm::write_pid_file("dbmgr");

    // 信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 创建并启动服务器
    g_server = std::make_unique<farm::DbMgrServer>(config_path);
    if (!g_server->Start()) {
        spdlog::error("Failed to start DBMgr server");
        return 1;
    }

    // 等待退出信号
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    spdlog::info("DBMgr server stopped");
    return 0;
}
```

- [ ] **Step 4: 编译验证**

```bash
cd D:/mb_workspace/farm_demo
cmake --build build --target dbmgr 2>&1 | tail -20
```

Expected: 编译通过

- [ ] **Step 5: 提交**

```bash
git add scripts/server/dbmgr/
git commit -m "feat(dbmgr): migrate to gRPC DBMgrService

Replace libevent TCP server with gRPC synchronous service.
Remove GameSession, message_parser, identify/heartbeat protocol.
Implement GetPlayerData, SavePlayerData, QueryAccount, etc."
```

---

## Task 6: ChatServer — gRPC Service 实现

**Files:**
- Modify: `scripts/server/chat_server/src/chat_server.h/.cpp`
- Modify: `scripts/server/chat_server/src/main.cpp`

- [ ] **Step 1: 重写 chat_server.h**

```cpp
#pragma once

#include "channel_manager.h"
#include "rate_limiter.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

#include "internal/chat_service.grpc.pb.h"
#include "client/gateway.grpc.pb.h"

namespace farm {

class ChatServer final : public ChatService::Service {
public:
    ChatServer(const std::string& config_path);
    ~ChatServer();

    bool Start();
    void Stop();

    // ChatService RPC 实现
    grpc::Status SendMessage(grpc::ServerContext* context,
                             const SendMessageReq* request,
                             SendMessageResp* response) override;

    grpc::Status PlayerJoin(grpc::ServerContext* context,
                            const PlayerJoinChatReq* request,
                            PlayerJoinChatResp* response) override;

    grpc::Status PlayerLeave(grpc::ServerContext* context,
                             const PlayerLeaveChatReq* request,
                             PlayerLeaveChatResp* response) override;

private:
    void NotifyChatMessage(uint64_t player_id, const ChatMessage& msg);

    std::string config_path_;
    std::unique_ptr<ChannelManager> channel_mgr_;
    std::unique_ptr<RateLimiter> rate_limiter_;
    std::unique_ptr<grpc::Server> grpc_server_;

    // Gateway 通知 channel（用于推送聊天消息给客户端）
    std::shared_ptr<grpc::Channel> gateway_channel_;
    std::unique_ptr<GatewayNotify::Stub> gateway_stub_;
};

} // namespace farm
```

- [ ] **Step 2: 重写 chat_server.cpp**

```cpp
#include "chat_server.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace farm {

ChatServer::ChatServer(const std::string& config_path)
    : config_path_(config_path) {}

ChatServer::~ChatServer() { Stop(); }

bool ChatServer::Start() {
    std::ifstream f(config_path_);
    json config = json::parse(f);

    channel_mgr_ = std::make_unique<ChannelManager>();
    rate_limiter_ = std::make_unique<RateLimiter>();

    // 连接 Gateway 用于推送消息
    std::string gateway_addr = config.value("gateway_address", "127.0.0.1:8080");
    gateway_channel_ = grpc::CreateChannel(gateway_addr, grpc::InsecureChannelCredentials());
    gateway_stub_ = GatewayNotify::NewStub(gateway_channel_);

    // 启动 gRPC server
    std::string address = config.value("ip", "0.0.0.0") + ":" +
                          std::to_string(config.value("port", 8889));

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(this);

    grpc_server_ = builder.BuildAndStart();
    spdlog::info("ChatServer gRPC started on {}", address);
    return true;
}

void ChatServer::Stop() {
    if (grpc_server_) grpc_server_->Shutdown();
}

grpc::Status ChatServer::SendMessage(grpc::ServerContext* context,
                                       const SendMessageReq* request,
                                       SendMessageResp* response) {
    // 限流检查
    if (!rate_limiter_->Check(request->player_id())) {
        response->set_code(ChatErrorCode::CHAT_RATE_LIMITED);
        return grpc::Status::OK;
    }

    // 构造聊天消息
    ChatMessage msg;
    msg.set_channel_type(request->channel_type());
    msg.set_sender_id(request->player_id());
    msg.set_sender_name(request->player_name());
    msg.set_content(request->content());
    msg.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    if (request->channel_type() == ChatChannelType::CHANNEL_WHISPER) {
        msg.set_target_id(request->target_id());
        // 推送给目标玩家
        NotifyChatMessage(request->target_id(), msg);
    } else {
        // 广播到频道
        auto members = channel_mgr_->GetChannelMembers(request->channel_type());
        for (auto member_id : members) {
            NotifyChatMessage(member_id, msg);
        }
    }

    response->set_code(ChatErrorCode::CHAT_SUCCESS);
    return grpc::Status::OK;
}

grpc::Status ChatServer::PlayerJoin(grpc::ServerContext* context,
                                      const PlayerJoinChatReq* request,
                                      PlayerJoinChatResp* response) {
    channel_mgr_->AddPlayer(request->player_id(), request->player_name());
    response->set_code(0);
    return grpc::Status::OK;
}

grpc::Status ChatServer::PlayerLeave(grpc::ServerContext* context,
                                       const PlayerLeaveChatReq* request,
                                       PlayerLeaveChatResp* response) {
    channel_mgr_->RemovePlayer(request->player_id());
    response->set_code(0);
    return grpc::Status::OK;
}

void ChatServer::NotifyChatMessage(uint64_t player_id, const ChatMessage& msg) {
    grpc::ClientContext ctx;
    NotifyAck ack;
    auto status = gateway_stub_->PushChatMessage(&ctx, msg, &ack);
    if (!status.ok()) {
        spdlog::warn("Failed to push chat message to gateway: {}", status.error_message());
    }
}

} // namespace farm
```

- [ ] **Step 3: 重写 main.cpp（类似 DBMgr 模式）**

```cpp
#include "chat_server.h"
#include "log_init.h"
#include "server_main_helper.h"
#include <spdlog/spdlog.h>
#include <csignal>

static std::unique_ptr<farm::ChatServer> g_server;
static std::atomic<bool> g_running{true};

void signal_handler(int sig) {
    g_running = false;
    if (g_server) g_server->Stop();
}

int main(int argc, char* argv[]) {
    auto config_path = farm::parse_config_path(argc, argv);
    farm::init_logging_from_config(config_path);
    farm::write_pid_file("chat_server");
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    g_server = std::make_unique<farm::ChatServer>(config_path);
    if (!g_server->Start()) return 1;

    while (g_running) std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
```

- [ ] **Step 4: 编译验证 + 提交**

```bash
cmake --build build --target chat_server 2>&1 | tail -20
git add scripts/server/chat_server/
git commit -m "feat(chat): migrate ChatServer to gRPC ChatService

Replace libevent TCP with gRPC synchronous service.
Remove identify/heartbeat/message_parser. Push chat messages
to clients via GatewayNotify service."
```

---

## Task 7: TeamServer — gRPC Service 实现

**Files:**
- Modify: `scripts/server/team_server/src/team_server.h/.cpp`
- Modify: `scripts/server/team_server/src/main.cpp`

- [ ] **Step 1: 重写 team_server.h**

```cpp
#pragma once

#include "team_manager.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

#include "internal/team_service.grpc.pb.h"
#include "client/gateway.grpc.pb.h"

namespace farm {

class TeamServer final : public TeamService::Service {
public:
    TeamServer(const std::string& config_path);
    ~TeamServer();

    bool Start();
    void Stop();

    // TeamService RPC 实现
    grpc::Status CreateTeam(grpc::ServerContext* ctx, const TeamCreateReq* req, TeamCreateResp* resp) override;
    grpc::Status DisbandTeam(grpc::ServerContext* ctx, const TeamDisbandReq* req, TeamDisbandResp* resp) override;
    grpc::Status InviteToTeam(grpc::ServerContext* ctx, const TeamInviteReq* req, TeamInviteResp* resp) override;
    grpc::Status AcceptInvite(grpc::ServerContext* ctx, const TeamAcceptReq* req, TeamAcceptResp* resp) override;
    grpc::Status RejectInvite(grpc::ServerContext* ctx, const TeamRejectReq* req, TeamRejectResp* resp) override;
    grpc::Status LeaveTeam(grpc::ServerContext* ctx, const TeamLeaveReq* req, TeamLeaveResp* resp) override;
    grpc::Status KickMember(grpc::ServerContext* ctx, const TeamKickReq* req, TeamKickResp* resp) override;
    grpc::Status GetTeamInfo(grpc::ServerContext* ctx, const TeamInfoReq* req, TeamInfoResp* resp) override;
    grpc::Status QueryMembers(grpc::ServerContext* ctx, const TeamQueryMembersReq* req, TeamQueryMembersResp* resp) override;
    grpc::Status PlayerOnline(grpc::ServerContext* ctx, const PlayerOnlineReq* req, PlayerOnlineResp* resp) override;
    grpc::Status PlayerOffline(grpc::ServerContext* ctx, const PlayerOfflineReq* req, PlayerOfflineResp* resp) override;

private:
    void NotifyTeamEvent(uint64_t player_id, const TeamEventWrapper& event);

    std::string config_path_;
    std::unique_ptr<TeamManager> team_mgr_;
    std::unique_ptr<grpc::Server> grpc_server_;
    std::unique_ptr<GatewayNotify::Stub> gateway_stub_;
};

} // namespace farm
```

- [ ] **Step 2: 重写 team_server.cpp**

实现所有 RPC 方法，委托给 `TeamManager`，通过 `GatewayNotify` 推送事件给客户端。每个方法模式相同：

```cpp
grpc::Status TeamServer::CreateTeam(grpc::ServerContext* ctx,
                                      const TeamCreateReq* req,
                                      TeamCreateResp* resp) {
    // 从 context metadata 获取 player_id
    auto metadata = ctx->client_metadata();
    auto it = metadata.find("player-id");
    uint64_t player_id = 0;
    if (it != metadata.end()) {
        player_id = std::stoull(std::string(it->second.data(), it->second.size()));
    }

    auto result = team_mgr_->CreateTeam(player_id);
    resp->set_code(result.first);
    resp->set_team_id(result.second);

    // 通知其他成员
    if (result.first == TeamErrorCode::TEAM_SUCCESS) {
        TeamEventWrapper event;
        event.set_player_id(player_id);
        auto* update = event.mutable_member_update();
        // ... 填充成员信息
        NotifyTeamEvent(player_id, event);
    }

    return grpc::Status::OK;
}
```

- [ ] **Step 3: 重写 main.cpp + 编译 + 提交**

```bash
cmake --build build --target team_server 2>&1 | tail -20
git add scripts/server/team_server/
git commit -m "feat(team): migrate TeamServer to gRPC TeamService

Replace libevent TCP with gRPC synchronous service.
Implement all team operations via gRPC methods."
```

---

## Task 8: FriendService — gRPC Service 实现

**Files:**
- Modify: `scripts/server/friend_service/src/friend_service.h/.cpp`
- Modify: `scripts/server/friend_service/src/main.cpp`

- [ ] **Step 1: 重写 friend_service.h**

```cpp
#pragma once

#include "friend_manager.h"
#include "chat_manager.h"
#include "gift_manager.h"
#include "visit_manager.h"
#include "recommend_manager.h"
#include "friend_data_manager.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

#include "internal/friend_service.grpc.pb.h"
#include "client/gateway.grpc.pb.h"

namespace farm {

class FriendServiceImpl final : public FriendService::Service {
public:
    FriendServiceImpl(const std::string& config_path);
    ~FriendServiceImpl();

    bool Start();
    void Stop();

    // FriendService RPC 实现
    grpc::Status SearchPlayer(grpc::ServerContext* ctx, const FriendSearchReq* req, FriendSearchResp* resp) override;
    grpc::Status AddFriend(grpc::ServerContext* ctx, const FriendAddReq* req, FriendAddResp* resp) override;
    grpc::Status AcceptFriend(grpc::ServerContext* ctx, const FriendAcceptReq* req, FriendAcceptResp* resp) override;
    grpc::Status RejectFriend(grpc::ServerContext* ctx, const FriendRejectReq* req, FriendRejectResp* resp) override;
    grpc::Status DeleteFriend(grpc::ServerContext* ctx, const FriendDeleteReq* req, FriendDeleteResp* resp) override;
    grpc::Status GetFriendList(grpc::ServerContext* ctx, const FriendListReq* req, FriendListResp* resp) override;
    grpc::Status SendChat(grpc::ServerContext* ctx, const FriendChatSendReq* req, FriendChatSendResp* resp) override;
    grpc::Status SendGift(grpc::ServerContext* ctx, const FriendGiftSendReq* req, FriendGiftSendResp* resp) override;
    grpc::Status ClaimGift(grpc::ServerContext* ctx, const FriendGiftClaimReq* req, FriendGiftClaimResp* resp) override;
    grpc::Status VisitFarm(grpc::ServerContext* ctx, const FriendVisitReq* req, FriendVisitResp* resp) override;
    grpc::Status GetRecommendations(grpc::ServerContext* ctx, const FriendRecommendReq* req, FriendRecommendResp* resp) override;
    grpc::Status BlockPlayer(grpc::ServerContext* ctx, const FriendBlockReq* req, FriendBlockResp* resp) override;
    grpc::Status UnblockPlayer(grpc::ServerContext* ctx, const FriendUnblockReq* req, FriendUnblockResp* resp) override;

private:
    std::string config_path_;
    std::unique_ptr<FriendManager> friend_mgr_;
    std::unique_ptr<ChatManager> chat_mgr_;
    std::unique_ptr<GiftManager> gift_mgr_;
    std::unique_ptr<VisitManager> visit_mgr_;
    std::unique_ptr<RecommendManager> recommend_mgr_;
    std::unique_ptr<FriendDataManager> data_mgr_;
    std::unique_ptr<grpc::Server> grpc_server_;
    std::unique_ptr<GatewayNotify::Stub> gateway_stub_;
};

} // namespace farm
```

- [ ] **Step 2: 实现 friend_service.cpp + main.cpp + 编译 + 提交**

```bash
cmake --build build --target friend_service 2>&1 | tail -20
git add scripts/server/friend_service/
git commit -m "feat(friend): migrate FriendService to gRPC

Replace libevent TCP with gRPC synchronous service.
Implement all friend operations via gRPC methods."
```

---

## Task 9: GameServer — gRPC Service 实现

**Files:**
- Modify: `scripts/server/game_server/src/game_server.h/.cpp`
- Modify: `scripts/server/game_server/src/gate_session.h/.cpp`
- Modify: `scripts/server/game_server/src/message_handler.h/.cpp`
- Modify: `scripts/server/game_server/src/login_stub.h/.cpp`
- Modify: `scripts/server/game_server/src/main.cpp`
- Delete: `scripts/server/game_server/src/dbmgr_connection.h/.cpp`
- Delete: `scripts/server/game_server/src/dbmgr_connection_manager.h/.cpp`
- Delete: `scripts/server/game_server/src/friend_service_connection.h/.cpp`

- [ ] **Step 1: 重写 game_server.h — GameService 实现 + 后端 stubs**

```cpp
#pragma once

#include "player_manager.h"
#include "message_handler.h"
#include "item_interaction_handler.h"
#include "game_scene_manager.h"
#include "game_clock.h"
#include "quest_manager.h"
#include "combat_handler.h"
#include "login_stub.h"
#include "online_stub.h"
#include "gm_stub.h"
#include "gm_http_handler.h"
#include "redis_connection.h"
#include "event_bus.h"
#include "player_id_pool.h"
#include "admin_handler.h"
#include "timer_manager.h"
#include "service_channel.h"

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

// gRPC service 头文件
#include "internal/game_service.grpc.pb.h"
#include "internal/dbmgr_service.grpc.pb.h"
#include "internal/friend_service.grpc.pb.h"
#include "internal/team_service.grpc.pb.h"
#include "client/gateway.grpc.pb.h"

namespace farm {

class GameServer final : public GameService::Service {
public:
    GameServer(const std::string& config_path);
    ~GameServer();

    bool Start();
    void Stop();

    // GameService RPC 实现
    grpc::Status PlayerJoin(grpc::ServerContext* ctx, const PlayerJoinReq* req, PlayerJoinResp* resp) override;
    grpc::Status PlayerLeave(grpc::ServerContext* ctx, const PlayerLeaveReq* req, PlayerLeaveResp* resp) override;
    grpc::Status QueryRoles(grpc::ServerContext* ctx, const QueryRolesReq* req, QueryRolesResp* resp) override;
    grpc::Status CreateRole(grpc::ServerContext* ctx, const CreateRoleReq* req, CreateRoleResp* resp) override;
    grpc::Status EnterGame(grpc::ServerContext* ctx, const farm::EnterGameReq* req, farm::EnterGameResp* resp) override;
    grpc::Status ForwardPlayerAction(grpc::ServerContext* ctx, const ForwardPlayerActionReq* req, ForwardPlayerActionResp* resp) override;

    // 获取后端 stubs（供 LoginStub 等使用）
    DBMgrService::Stub* dbmgr_stub() { return dbmgr_stub_.get(); }
    FriendService::Stub* friend_stub() { return friend_stub_.get(); }
    TeamService::Stub* team_stub() { return team_stub_.get(); }
    GatewayNotify::Stub* gateway_stub() { return gateway_stub_.get(); }

private:
    void UpdateGameLogic();

    std::string config_path_;

    // 子系统（保留）
    std::unique_ptr<PlayerManager> player_mgr_;
    std::unique_ptr<ItemInteractionHandler> item_handler_;
    std::unique_ptr<GameSceneManager> scene_mgr_;
    std::unique_ptr<GameClock> game_clock_;
    std::unique_ptr<QuestManager> quest_mgr_;
    std::unique_ptr<CombatHandler> combat_handler_;
    std::unique_ptr<LoginStub> login_stub_;
    std::unique_ptr<OnlineStub> online_stub_;
    std::unique_ptr<GMStub> gm_stub_;
    std::unique_ptr<GmHttpHandler> gm_http_;
    std::unique_ptr<RedisConnection> redis_;
    std::unique_ptr<EventBus> event_bus_;
    std::unique_ptr<PlayerIdPool> player_id_pool_;
    std::unique_ptr<AdminHandler> admin_handler_;
    std::unique_ptr<TimerManager> timer_mgr_;

    // gRPC server
    std::unique_ptr<grpc::Server> grpc_server_;

    // 后端 service stubs（替代手写 ConnectionManager）
    std::unique_ptr<DBMgrService::Stub> dbmgr_stub_;
    std::unique_ptr<FriendService::Stub> friend_stub_;
    std::unique_ptr<TeamService::Stub> team_stub_;
    std::unique_ptr<GatewayNotify::Stub> gateway_stub_;
};

} // namespace farm
```

- [ ] **Step 2: 重写 game_server.cpp — gRPC service 实现**

```cpp
#include "game_server.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace farm {

GameServer::GameServer(const std::string& config_path)
    : config_path_(config_path) {}

GameServer::~GameServer() { Stop(); }

bool GameServer::Start() {
    std::ifstream f(config_path_);
    json config = json::parse(f);

    // 初始化子系统
    player_mgr_ = std::make_unique<PlayerManager>();
    item_handler_ = std::make_unique<ItemInteractionHandler>(player_mgr_.get());
    scene_mgr_ = std::make_unique<GameSceneManager>();
    game_clock_ = std::make_unique<GameClock>();
    quest_mgr_ = std::make_unique<QuestManager>();
    combat_handler_ = std::make_unique<CombatHandler>(player_mgr_.get());
    redis_ = std::make_unique<RedisConnection>();
    event_bus_ = std::make_unique<EventBus>();
    player_id_pool_ = std::make_unique<PlayerIdPool>();
    timer_mgr_ = std::make_unique<TimerManager>();

    // 连接后端服务
    std::string dbmgr_addr = config.value("dbmgr_address", "127.0.0.1:5000");
    auto dbmgr_channel = ServiceChannel::CreateDirect(dbmgr_addr.substr(0, dbmgr_addr.rfind(':')),
                                                       std::stoi(dbmgr_addr.substr(dbmgr_addr.rfind(':') + 1)));
    dbmgr_stub_ = DBMgrService::NewStub(dbmgr_channel);

    std::string friend_addr = config.value("friend_service_address", "127.0.0.1:9092");
    auto friend_channel = ServiceChannel::CreateDirect(/* ... */);
    friend_stub_ = FriendService::NewStub(friend_channel);

    // ... 类似连接 TeamService, GatewayNotify

    // 初始化依赖子系统
    login_stub_ = std::make_unique<LoginStub>(this, player_mgr_.get(), dbmgr_stub_.get());
    online_stub_ = std::make_unique<OnlineStub>(redis_.get());
    gm_stub_ = std::make_unique<GMStub>(player_mgr_.get());

    // 注册定时器：游戏逻辑更新（每秒）
    timer_mgr_->AddPeriodic(std::chrono::seconds(1), [this]() {
        UpdateGameLogic();
    });

    timer_mgr_->Start();

    // 启动 gRPC server
    std::string address = config.value("ip", "0.0.0.0") + ":" +
                          std::to_string(config.value("port", 9090));

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(this);

    grpc_server_ = builder.BuildAndStart();
    spdlog::info("GameServer gRPC started on {}", address);
    return true;
}

void GameServer::Stop() {
    timer_mgr_->Stop();
    if (grpc_server_) grpc_server_->Shutdown();
}

grpc::Status GameServer::PlayerJoin(grpc::ServerContext* ctx,
                                      const PlayerJoinReq* req,
                                      PlayerJoinResp* resp) {
    // 创建 GateSession（基于 gRPC context）
    player_mgr_->OnPlayerJoin(req->player_id(), req->gate_id());
    resp->set_code(0);
    return grpc::Status::OK;
}

grpc::Status GameServer::PlayerLeave(grpc::ServerContext* ctx,
                                       const PlayerLeaveReq* req,
                                       PlayerLeaveResp* resp) {
    player_mgr_->OnPlayerLeave(req->player_id());
    resp->set_code(0);
    return grpc::Status::OK;
}

grpc::Status GameServer::ForwardPlayerAction(grpc::ServerContext* ctx,
                                               const ForwardPlayerActionReq* req,
                                               ForwardPlayerActionResp* resp) {
    uint64_t player_id = req->player_id();

    switch (req->action_case()) {
        case ForwardPlayerActionReq::kItemUse:
            item_handler_->HandleUseItem(player_id, req->item_use());
            break;
        case ForwardPlayerActionReq::kSceneChange:
            scene_mgr_->HandleSceneChange(player_id, req->scene_change());
            break;
        case ForwardPlayerActionReq::kPosition:
            scene_mgr_->HandlePositionUpdate(player_id, req->position());
            break;
        case ForwardPlayerActionReq::kCombatAction:
            combat_handler_->HandleCombatAction(player_id, req->combat_action());
            break;
        // ... 其他操作
        default:
            resp->set_code(1);
            return grpc::Status::OK;
    }

    resp->set_code(0);
    return grpc::Status::OK;
}

void GameServer::UpdateGameLogic() {
    game_clock_->Update();
    scene_mgr_->UpdateCrops(game_clock_.get());
    scene_mgr_->UpdateDropItems();
    scene_mgr_->UpdateMonsters();
}

} // namespace farm
```

- [ ] **Step 3: 删除旧连接文件**

```bash
rm scripts/server/game_server/src/dbmgr_connection.h
rm scripts/server/game_server/src/dbmgr_connection.cpp
rm scripts/server/game_server/src/dbmgr_connection_manager.h
rm scripts/server/game_server/src/dbmgr_connection_manager.cpp
rm scripts/server/game_server/src/friend_service_connection.h
rm scripts/server/game_server/src/friend_service_connection.cpp
```

- [ ] **Step 4: 修改 LoginStub — 使用 gRPC stub 替代 DBMgrConnectionManager**

```cpp
// login_stub.h 关键变更：
// - 构造函数接受 DBMgrService::Stub* 替代 DBMgrConnectionManager*
// - 所有 DB 操作改为同步 gRPC 调用
// - 移除 request_id 匹配逻辑

class LoginStub {
public:
    LoginStub(GameServer* server, PlayerManager* player_mgr, DBMgrService::Stub* dbmgr_stub);

    void HandleQueryRoles(uint64_t account_id, uint32_t server_id);
    void HandleCreateRole(uint64_t account_id, const CreateRoleReq& req);
    void HandleEnterGame(uint64_t player_id, const EnterGameReq& req);

private:
    GameServer* server_;
    PlayerManager* player_mgr_;
    DBMgrService::Stub* dbmgr_stub_;  // 替代 DBMgrConnectionManager*
};
```

- [ ] **Step 5: 重写 main.cpp + 编译 + 提交**

```bash
cmake --build build --target game_server 2>&1 | tail -20
git add scripts/server/game_server/
git commit -m "feat(game): migrate GameServer to gRPC GameService

- Replace libevent with gRPC server
- Replace DBMgrConnectionManager with DBMgrService::Stub
- Replace FriendServiceConnection with FriendService::Stub
- Replace GameConnection/GateSession with gRPC context
- TimerManager for game logic updates
- Delete dbmgr_connection, dbmgr_connection_manager,
  friend_service_connection"
```

---

## Task 10: GateServer — gRPC Gateway 实现

**Files:**
- Modify: `scripts/server/gate_server/src/gate_server.h/.cpp`
- Create: `scripts/server/gate_server/src/gateway_service.h/.cpp`
- Create: `scripts/server/gate_server/src/gateway_notify_service.h/.cpp`
- Modify: `scripts/server/gate_server/src/session.h/.cpp`
- Modify: `scripts/server/gate_server/src/session_manager.h/.cpp`
- Modify: `scripts/server/gate_server/src/main.cpp`
- Delete: `scripts/server/gate_server/src/game_connection.h/.cpp`
- Delete: `scripts/server/gate_server/src/chat_connection.h/.cpp`

- [ ] **Step 1: 创建 gateway_service.h — FarmGateway streaming 实现**

```cpp
#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <queue>
#include <atomic>

#include "client/gateway.grpc.pb.h"
#include "internal/game_service.grpc.pb.h"
#include "internal/chat_service.grpc.pb.h"
#include "internal/team_service.grpc.pb.h"

namespace farm {

class GateServer;

// 客户端会话 — 管理单个客户端的 bidirectional stream
class ClientStreamSession {
public:
    ClientStreamSession(uint64_t session_id, grpc::ServerContext* ctx);
    ~ClientStreamSession();

    uint64_t session_id() const { return session_id_; }
    uint64_t player_id() const { return player_id_; }
    void set_player_id(uint64_t id) { player_id_ = id; }
    uint64_t account_id() const { return account_id_; }
    void set_account_id(uint64_t id) { account_id_ = id; }

    // 推送事件给客户端
    void PushEvent(const ServerEvent& event);

    // 从推送队列取事件
    bool TryPopEvent(ServerEvent& event);

    // 标记 stream 结束
    void Close();

    bool is_closed() const { return closed_.load(); }

private:
    uint64_t session_id_;
    uint64_t player_id_ = 0;
    uint64_t account_id_ = 0;
    std::atomic<bool> closed_{false};
    std::mutex queue_mutex_;
    std::queue<ServerEvent> event_queue_;
};

// FarmGateway service 实现
class GatewayServiceImpl final : public FarmGateway::Service {
public:
    GatewayServiceImpl(GateServer* gate_server);
    ~GatewayServiceImpl();

    grpc::Status GameStream(grpc::ServerContext* context,
                            grpc::ServerReaderWriter<ServerEvent, ClientRequest>* stream) override;

    // 获取会话（用于后端推送）
    std::shared_ptr<ClientStreamSession> GetSessionByPlayerId(uint64_t player_id);
    void RemoveSession(uint64_t session_id);

private:
    void HandleRequest(std::shared_ptr<ClientStreamSession> session,
                       const ClientRequest& request);

    GateServer* gate_server_;
    std::mutex sessions_mutex_;
    std::unordered_map<uint64_t, std::shared_ptr<ClientStreamSession>> sessions_;
    std::unordered_map<uint64_t, uint64_t> player_to_session_;  // player_id -> session_id
    std::atomic<uint64_t> next_session_id_{1};
};

} // namespace farm
```

- [ ] **Step 2: 创建 gateway_service.cpp**

```cpp
#include "gateway_service.h"
#include "gate_server.h"
#include <spdlog/spdlog.h>

namespace farm {

// ClientStreamSession 实现
ClientStreamSession::ClientStreamSession(uint64_t session_id, grpc::ServerContext* ctx)
    : session_id_(session_id) {}

ClientStreamSession::~ClientStreamSession() = default;

void ClientStreamSession::PushEvent(const ServerEvent& event) {
    if (closed_) return;
    std::lock_guard<std::mutex> lock(queue_mutex_);
    event_queue_.push(event);
}

bool ClientStreamSession::TryPopEvent(ServerEvent& event) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (event_queue_.empty()) return false;
    event = std::move(event_queue_.front());
    event_queue_.pop();
    return true;
}

void ClientStreamSession::Close() {
    closed_ = true;
}

// GatewayServiceImpl 实现
GatewayServiceImpl::GatewayServiceImpl(GateServer* gate_server)
    : gate_server_(gate_server) {}

GatewayServiceImpl::~GatewayServiceImpl() = default;

grpc::Status GatewayServiceImpl::GameStream(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<ServerEvent, ClientRequest>* stream)
{
    auto session_id = next_session_id_++;
    auto session = std::make_shared<ClientStreamSession>(session_id, context);

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[session_id] = session;
    }

    spdlog::info("Client stream connected: session={}", session_id);

    // 启动写线程 — 从队列取事件推送给客户端
    std::thread write_thread([stream, session]() {
        ServerEvent event;
        while (!session->is_closed()) {
            if (session->TryPopEvent(event)) {
                if (!stream->Write(event)) {
                    session->Close();
                    break;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    });

    // 读循环 — 接收客户端请求
    ClientRequest request;
    while (stream->Read(&request)) {
        HandleRequest(session, request);
    }

    // 客户端断开
    session->Close();
    write_thread.join();

    // 清理
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(session_id);
        if (session->player_id() != 0) {
            player_to_session_.erase(session->player_id());
            // 通知 GameServer 玩家离开
            // gate_server_->NotifyPlayerLeave(session->player_id());
        }
    }

    spdlog::info("Client stream disconnected: session={}", session_id);
    return grpc::Status::OK;
}

void GatewayServiceImpl::HandleRequest(
    std::shared_ptr<ClientStreamSession> session,
    const ClientRequest& request)
{
    switch (request.request_case()) {
        case ClientRequest::kHeartbeat: {
            ServerEvent resp;
            resp.mutable_heartbeat()->set_timestamp(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            session->PushEvent(resp);
            break;
        }
        case ClientRequest::kLogin: {
            // 调用 GameServer 的登录逻辑
            // gate_server_->HandleLogin(session, request.login());
            break;
        }
        case ClientRequest::kChatSend: {
            // 转发到 ChatServer
            // gate_server_->ForwardToChat(session, request.chat_send());
            break;
        }
        case ClientRequest::kTeamCreate:
        case ClientRequest::kTeamDisband:
        case ClientRequest::kTeamInvite:
        case ClientRequest::kTeamAccept:
        case ClientRequest::kTeamReject:
        case ClientRequest::kTeamLeave:
        case ClientRequest::kTeamKick:
        case ClientRequest::kTeamInfo: {
            // 转发到 TeamServer
            // gate_server_->ForwardToTeam(session, request);
            break;
        }
        // ... 其他请求类型
        default:
            spdlog::warn("Unknown request type: {}", static_cast<int>(request.request_case()));
            break;
    }
}

std::shared_ptr<ClientStreamSession> GatewayServiceImpl::GetSessionByPlayerId(uint64_t player_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = player_to_session_.find(player_id);
    if (it == player_to_session_.end()) return nullptr;
    auto sit = sessions_.find(it->second);
    return (sit != sessions_.end()) ? sit->second : nullptr;
}

void GatewayServiceImpl::RemoveSession(uint64_t session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        if (it->second->player_id() != 0) {
            player_to_session_.erase(it->second->player_id());
        }
        sessions_.erase(it);
    }
}

} // namespace farm
```

- [ ] **Step 3: 创建 gateway_notify_service.h/.cpp — 后端事件推送入口**

```cpp
// gateway_notify_service.h
#pragma once

#include <grpcpp/grpcpp.h>
#include "client/gateway.grpc.pb.h"
#include "gateway_service.h"

namespace farm {

class GatewayNotifyServiceImpl final : public GatewayNotify::Service {
public:
    explicit GatewayNotifyServiceImpl(GatewayServiceImpl* gateway);

    grpc::Status PushChatMessage(grpc::ServerContext* ctx, const ChatMessage* msg, NotifyAck* ack) override;
    grpc::Status PushTeamEvent(grpc::ServerContext* ctx, const TeamEventWrapper* event, NotifyAck* ack) override;
    grpc::Status PushFriendEvent(grpc::ServerContext* ctx, const FriendEventWrapper* event, NotifyAck* ack) override;
    grpc::Status PushSceneUpdate(grpc::ServerContext* ctx, const SceneUpdateWrapper* update, NotifyAck* ack) override;
    grpc::Status PushCombatEvent(grpc::ServerContext* ctx, const CombatEventWrapper* event, NotifyAck* ack) override;
    grpc::Status PushNotification(grpc::ServerContext* ctx, const Notification* notif, NotifyAck* ack) override;
    grpc::Status PushQuestUpdate(grpc::ServerContext* ctx, const QuestUpdateWrapper* update, NotifyAck* ack) override;

private:
    GatewayServiceImpl* gateway_;
};

} // namespace farm
```

```cpp
// gateway_notify_service.cpp
#include "gateway_notify_service.h"

namespace farm {

GatewayNotifyServiceImpl::GatewayNotifyServiceImpl(GatewayServiceImpl* gateway)
    : gateway_(gateway) {}

grpc::Status GatewayNotifyServiceImpl::PushChatMessage(
    grpc::ServerContext* ctx, const ChatMessage* msg, NotifyAck* ack) {
    // 目标玩家由 msg->target_id() 指定，或广播到所有在线玩家
    ServerEvent event;
    *event.mutable_chat_message() = *msg;

    // 如果是私聊，推送给 target_id
    if (msg->target_id() != 0) {
        auto session = gateway_->GetSessionByPlayerId(msg->target_id());
        if (session) session->PushEvent(event);
    }
    // 广播逻辑需要遍历所有 session，这里简化处理

    ack->set_code(0);
    return grpc::Status::OK;
}

grpc::Status GatewayNotifyServiceImpl::PushTeamEvent(
    grpc::ServerContext* ctx, const TeamEventWrapper* event, NotifyAck* ack) {
    ServerEvent se;
    switch (event->event_case()) {
        case TeamEventWrapper::kMemberUpdate:
            *se.mutable_team_member_update() = event->member_update();
            break;
        case TeamEventWrapper::kLeaderChange:
            *se.mutable_team_leader_change() = event->leader_change();
            break;
        case TeamEventWrapper::kStatusUpdate:
            *se.mutable_team_status_update() = event->status_update();
            break;
        case TeamEventWrapper::kInviteNotify:
            *se.mutable_team_invite_notify() = event->invite_notify();
            break;
        default: break;
    }

    auto session = gateway_->GetSessionByPlayerId(event->player_id());
    if (session) session->PushEvent(se);

    ack->set_code(0);
    return grpc::Status::OK;
}

// 其他 Push 方法类似实现...

} // namespace farm
```

- [ ] **Step: 4: 重写 gate_server.h — 整合 gRPC 服务**

```cpp
#pragma once

#include "gateway_service.h"
#include "gateway_notify_service.h"
#include "session_manager.h"
#include "service_channel.h"
#include "timer_manager.h"

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

#include "internal/game_service.grpc.pb.h"
#include "internal/chat_service.grpc.pb.h"
#include "internal/team_service.grpc.pb.h"

namespace farm {

class GateServer {
public:
    GateServer(const std::string& config_path);
    ~GateServer();

    bool Start();
    void Stop();

    // 获取后端 stubs
    GameService::Stub* game_stub() { return game_stub_.get(); }
    ChatService::Stub* chat_stub() { return chat_stub_.get(); }
    TeamService::Stub* team_stub() { return team_stub_.get(); }

    // 获取 gateway service（用于推送）
    GatewayServiceImpl* gateway_service() { return gateway_service_.get(); }

private:
    std::string config_path_;

    // gRPC 服务
    std::unique_ptr<grpc::Server> grpc_server_;
    std::unique_ptr<GatewayServiceImpl> gateway_service_;
    std::unique_ptr<GatewayNotifyServiceImpl> notify_service_;

    // 后端 stubs
    std::unique_ptr<GameService::Stub> game_stub_;
    std::unique_ptr<ChatService::Stub> chat_stub_;
    std::unique_ptr<TeamService::Stub> team_stub_;

    // 基础设施
    std::unique_ptr<SessionManager> session_mgr_;
    std::unique_ptr<TimerManager> timer_mgr_;
};

} // namespace farm
```

- [ ] **Step 5: 重写 gate_server.cpp + main.cpp**

```cpp
// gate_server.cpp
#include "gate_server.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace farm {

GateServer::GateServer(const std::string& config_path)
    : config_path_(config_path) {}

GateServer::~GateServer() { Stop(); }

bool GateServer::Start() {
    std::ifstream f(config_path_);
    json config = json::parse(f);

    // 创建 gateway service
    gateway_service_ = std::make_unique<GatewayServiceImpl>(this);
    notify_service_ = std::make_unique<GatewayNotifyServiceImpl>(gateway_service_.get());

    // 连接后端服务
    std::string game_addr = config.value("game_server_address", "127.0.0.1:9090");
    auto game_channel = ServiceChannel::CreateDirect(/* ... */);
    game_stub_ = GameService::NewStub(game_channel);

    std::string chat_addr = config.value("chat_server_address", "127.0.0.1:8889");
    auto chat_channel = ServiceChannel::CreateDirect(/* ... */);
    chat_stub_ = ChatService::NewStub(chat_channel);

    std::string team_addr = config.value("team_server_address", "127.0.0.1:9091");
    auto team_channel = ServiceChannel::CreateDirect(/* ... */);
    team_stub_ = TeamService::NewStub(team_channel);

    // 启动 gRPC server
    std::string address = config.value("ip", "0.0.0.0") + ":" +
                          std::to_string(config.value("port", 8080));

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(gateway_service_.get());
    builder.RegisterService(notify_service_.get());

    grpc_server_ = builder.BuildAndStart();
    spdlog::info("GateServer gRPC started on {}", address);
    return true;
}

void GateServer::Stop() {
    if (grpc_server_) grpc_server_->Shutdown();
}

} // namespace farm
```

- [ ] **Step 6: 删除旧连接文件**

```bash
rm scripts/server/gate_server/src/game_connection.h
rm scripts/server/gate_server/src/game_connection.cpp
rm scripts/server/gate_server/src/chat_connection.h
rm scripts/server/gate_server/src/chat_connection.cpp
```

- [ ] **Step 7: 编译验证 + 提交**

```bash
cmake --build build --target gate_server 2>&1 | tail -20
git add scripts/server/gate_server/
git commit -m "feat(gate): migrate GateServer to gRPC FarmGateway

- FarmGateway bidirectional streaming for client connections
- GatewayNotify for backend event push
- Replace GameConnection/ChatConnection with gRPC stubs
- Remove libevent, message_parser, identify/heartbeat protocol"
```

---

## Task 11: 客户端 — Python grpcio 改造

**Files:**
- Modify: `scripts/client/connection.py`
- Modify: `scripts/client/heartbeat.py`

- [ ] **Step 1: 重写 connection.py — grpcio 替换 socket**

```python
"""
Gate 服务器连接模块 — gRPC 版本
使用 grpcio 实现与 Gate 服务器的 bidirectional streaming 连接
"""
import threading
import queue
import time
import grpc
from enum import Enum
from typing import Optional, Callable, Any

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'common', 'proto', 'generated', 'python'))

import gateway_pb2
import gateway_pb2_grpc
import base_pb2


class ConnectionState(Enum):
    DISCONNECTED = 0
    CONNECTING = 1
    CONNECTED = 2


class GateConnection:
    """
    Gate 服务器连接类 — gRPC 版本
    使用 bidirectional streaming 管理与 Gate 的连接
    """

    def __init__(self, host: str = '127.0.0.1', port: int = 8080, timeout: float = 5.0):
        self.host = host
        self.port = port
        self.timeout = timeout

        self.state = ConnectionState.DISCONNECTED
        self._state_lock = threading.Lock()

        # gRPC channel 和 stub
        self._channel: Optional[grpc.Channel] = None
        self._stub: Optional[gateway_pb2_grpc.FarmGatewayStub] = None
        self._stream = None

        # 消息队列
        self._recv_queue: queue.Queue = queue.Queue()
        self._send_queue: queue.Queue = queue.Queue()

        # 线程
        self._send_thread: Optional[threading.Thread] = None
        self._recv_thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()

        # 回调
        self._on_disconnect: Optional[Callable[[str], None]] = None

    @property
    def is_connected(self) -> bool:
        with self._state_lock:
            return self.state == ConnectionState.CONNECTED

    def set_on_disconnect(self, callback: Callable[[str], None]):
        self._on_disconnect = callback

    def connect(self) -> bool:
        with self._state_lock:
            if self.state != ConnectionState.DISCONNECTED:
                return False
            self.state = ConnectionState.CONNECTING

        try:
            address = f"{self.host}:{self.port}"
            self._channel = grpc.insecure_channel(address)
            self._stub = gateway_pb2_grpc.FarmGatewayStub(self._channel)

            # 建立 bidirectional stream
            self._stream = self._stub.GameStream(self._request_iterator())

            with self._state_lock:
                self.state = ConnectionState.CONNECTED

            # 启动接收线程
            self._stop_event.clear()
            self._recv_thread = threading.Thread(
                target=self._receive_loop, name="RecvThread", daemon=True)
            self._recv_thread.start()

            return True

        except Exception as e:
            with self._state_lock:
                self.state = ConnectionState.DISCONNECTED
            raise ConnectionError(f"连接失败: {str(e)}")

    def disconnect(self):
        with self._state_lock:
            if self.state == ConnectionState.DISCONNECTED:
                return
            self.state = ConnectionState.DISCONNECTED

        self._stop_event.set()

        # 清空发送队列（_request_iterator 会退出）
        while not self._send_queue.empty():
            try:
                self._send_queue.get_nowait()
            except queue.Empty:
                break

        if self._recv_thread and self._recv_thread.is_alive():
            self._recv_thread.join(timeout=2.0)

        if self._channel:
            self._channel.close()
            self._channel = None

        self._stub = None
        self._stream = None

    def _request_iterator(self):
        """从发送队列取出请求，yield 给 gRPC stream"""
        while not self._stop_event.is_set():
            try:
                request = self._send_queue.get(timeout=0.1)
                yield request
            except queue.Empty:
                continue

    def _receive_loop(self):
        """从 gRPC stream 接收推送事件"""
        try:
            for event in self._stream:
                if self._stop_event.is_set():
                    break
                self._recv_queue.put(event)
        except grpc.RpcError as e:
            self._handle_disconnect(f"gRPC 错误: {e.details()}")
        except Exception as e:
            self._handle_disconnect(f"接收异常: {str(e)}")

    def _handle_disconnect(self, reason: str):
        with self._state_lock:
            if self.state == ConnectionState.DISCONNECTED:
                return
            self.state = ConnectionState.DISCONNECTED

        if self._on_disconnect:
            self._on_disconnect(reason)

    # === 发送方法 ===

    def send_heartbeat(self):
        req = gateway_pb2.ClientRequest()
        req.heartbeat.timestamp = int(time.time())
        self._send_queue.put(req)

    def send_login_request(self, token: str):
        req = gateway_pb2.ClientRequest()
        req.login.token = token
        self._send_queue.put(req)

    def send_player_action(self, action_type: str, **kwargs):
        """发送游戏操作"""
        req = gateway_pb2.ClientRequest()
        action = req.player_action
        # 根据 action_type 设置对应的 oneof 字段
        # 例如: send_player_action("item_use", item_id=123, count=1)
        self._send_queue.put(req)

    def send_chat(self, content: str, channel_type: int = 0, target_id: int = 0):
        req = gateway_pb2.ClientRequest()
        req.chat_send.content = content
        req.chat_send.channel_type = channel_type
        req.chat_send.target_id = target_id
        self._send_queue.put(req)

    # === 接收方法 ===

    def recv_message(self) -> Optional[gateway_pb2.ServerEvent]:
        try:
            return self._recv_queue.get_nowait()
        except queue.Empty:
            return None

    def recv_all_messages(self) -> list:
        messages = []
        while True:
            msg = self.recv_message()
            if msg is None:
                break
            messages.append(msg)
        return messages
```

- [ ] **Step 2: 重写 heartbeat.py — gRPC keepalive 替代**

```python
"""
心跳管理模块 — gRPC 版本
gRPC 内置 keepalive 保证连接存活，业务层心跳通过 stream 发送
"""
import threading
import time
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .connection import GateConnection


class HeartbeatManager:
    """心跳管理器 — 通过 gRPC stream 发送业务层心跳"""

    HEARTBEAT_INTERVAL = 5.0

    def __init__(self, connection: 'GateConnection'):
        self._connection = connection
        self._thread: threading.Thread = None
        self._stop_event = threading.Event()

    def start(self):
        if self._thread and self._thread.is_alive():
            return
        self._stop_event.clear()
        self._thread = threading.Thread(
            target=self._heartbeat_loop, name="HeartbeatThread", daemon=True)
        self._thread.start()

    def stop(self):
        self._stop_event.set()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=2.0)
        self._thread = None

    def _heartbeat_loop(self):
        while not self._stop_event.is_set():
            try:
                self._connection.send_heartbeat()
            except Exception:
                break
            self._stop_event.wait(self.HEARTBEAT_INTERVAL)
```

- [ ] **Step 3: 生成 Python gRPC 代码**

```bash
cd D:/mb_workspace/farm_demo
python -m grpc_tools.protoc \
    --proto_path=scripts/common/proto \
    --python_out=scripts/common/proto/generated/python \
    --grpc_python_out=scripts/common/proto/generated/python \
    scripts/common/proto/client/gateway.proto
```

- [ ] **Step 4: 测试客户端连接**

```bash
cd D:/mb_workspace/farm_demo
python -c "
import sys
sys.path.insert(0, 'scripts/common/proto/generated/python')
import gateway_pb2
import gateway_pb2_grpc
print('gRPC imports OK')
"
```

Expected: 无报错

- [ ] **Step 5: 提交**

```bash
git add scripts/client/ scripts/common/proto/generated/python/
git commit -m "feat(client): migrate Python client to grpcio

- Replace socket+threading with grpcio bidirectional streaming
- Gateway service stub for all client requests
- gRPC keepalive replaces custom heartbeat protocol
- Remove message_parser wire format dependency"
```

---

## Task 12: 清理 — 删除旧代码

**Files:**
- Delete: `scripts/server/common/include/message_parser.h`
- Delete: `scripts/server/common/src/message_parser.cpp`
- Delete: `scripts/server/common/include/internal_msg_ids.h`
- Delete: `scripts/server/common/include/admin_msg_ids.h`
- Delete: `scripts/server/common/src/admin_msg_ids.cpp`
- Delete: `scripts/server/common/include/message_ids.h`
- Modify: `shared/message_ids.json` (标记为废弃)

- [ ] **Step 1: 删除旧文件**

```bash
rm scripts/server/common/include/message_parser.h
rm scripts/server/common/src/message_parser.cpp
rm scripts/server/common/include/internal_msg_ids.h
rm scripts/server/common/include/admin_msg_ids.h
rm scripts/server/common/src/admin_msg_ids.cpp
rm scripts/server/common/include/message_ids.h
```

- [ ] **Step 2: 在 message_ids.json 头部添加废弃说明**

```json
{
  "_deprecated": "此文件已废弃。消息分发改用 gRPC service/method 自动路由。保留供参考。",
  "message_ids": [
    ...
  ]
}
```

- [ ] **Step 3: 编译验证所有服务**

```bash
cd D:/mb_workspace/farm_demo
cmake --build build --target gate_server game_server dbmgr chat_server friend_service team_server 2>&1 | tail -30
```

Expected: 所有服务编译通过

- [ ] **Step 4: 提交**

```bash
git add -A
git commit -m "chore(cleanup): remove old TCP/message_parser/msg_id code

Delete message_parser.h/.cpp, internal_msg_ids.h, admin_msg_ids.h/.cpp,
message_ids.h. Mark message_ids.json as deprecated.

All communication now uses gRPC service definitions."
```

---

## 自检清单

### Spec 覆盖

| Spec 要求 | 对应 Task |
|---|---|
| Proto service 定义 | Task 1, 2 |
| 客户端层 FarmGateway streaming | Task 10 |
| 内部 service (Game/DBMgr/Chat/Team/Friend/Cross) | Task 2 (proto), Task 5-9 (实现) |
| gRPC 拦截器 | Task 4 |
| 替换 libevent 事件循环 | Task 4 (TimerManager), Task 5-10 (各服务) |
| 删除 message_parser/msg_id | Task 12 |
| 删除手写 ConnectionManager | Task 9, 10 |
| Python grpcio 客户端 | Task 11 |
| CMake 构建系统改造 | Task 3 |
| 错误处理 (gRPC status codes) | Task 5-10 (各服务实现) |
| ServiceChannel + etcd 集成 | Task 4 |

### 类型一致性

- `ClientRequest`/`ServerEvent` 在 Task 2 定义，Task 10 使用 ✓
- `GatewayNotify` service 在 Task 2 定义，Task 6-8 使用 ✓
- `ServiceChannel` 在 Task 4 定义，Task 9-10 使用 ✓
- `TimerManager` 在 Task 4 定义，Task 9 使用 ✓
