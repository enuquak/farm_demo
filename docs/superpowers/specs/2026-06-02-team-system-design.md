# 组队系统设计文档

## 概述

为农场游戏新增组队功能，主要用于矿洞战斗协作和队伍聊天。玩家可以创建最多 4 人的队伍，一起探索矿洞、挑战 Boss，经验和战斗成果共享。

### 核心需求

- 队伍人数：2-4 人
- 组队方式：邀请制（队长邀请，被邀请者同意）
- 经验分配：击杀怪物时，队伍内平均分配经验
- 掉落分配：各自拾取，不做平均
- 队长退出：自动转移给最早加入的队员
- 队伍聊天：复用 ChatServer 已预留的 Party 频道

## 整体架构

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Client    │────>│  GateServer │────>│  GameServer │
│  (Python)   │<────│   (C++)     │<────│   (C++)     │
└─────────────┘     └──────┬──────┘     └─────────────┘
                           │
                     ┌─────┴─────┐
                     ▼           ▼
              ┌─────────────┐ ┌─────────────┐
              │  TeamServer │ │  ChatServer │
              │   (C++)     │ │   (C++)     │
              └──────┬──────┘ └─────────────┘
                     │
                     ▼
              ┌─────────────┐
              │    Redis    │
              └─────────────┘
```

### 消息路由规则

- `msg_id 6000-6999` → TeamServer（组队消息）
- `msg_id 4000-4999` → ChatServer（聊天消息）
- `msg_id 2000-3999` → GameServer（游戏消息）

### TeamServer 职责

1. 队伍创建/解散
2. 邀请/接受/拒绝/离开
3. 队员管理（踢人、队长转移）
4. 队伍状态维护（存储在 Redis）
5. 通知 GameServer 队伍变更（用于战斗经验分配）
6. 通知 ChatServer 队伍变更（用于队伍聊天广播）

### 跨服务通信

- **TeamServer → GameServer**：内部 RPC，通知队伍进入/离开矿洞
- **TeamServer → ChatServer**：内部 RPC，通知队伍成员变更
- **GameServer → TeamServer**：查询队伍成员列表（战斗结算时）
- **ChatServer → TeamServer**：查询队伍成员列表（队伍频道广播时）

## 数据模型与存储

### 队伍状态（Redis）

```
Redis 结构：
  team:{team_id}:info → Hash {
    "team_id": uint64,
    "leader_id": uint64,
    "created_at": uint64,
    "status": "idle"|"in_cave",
    "cave_level": int
  }
  team:{team_id}:members → List<player_id>  // 队长在首位
  team:player:{player_id} → String team_id  // 0 = 无队伍
  team:invite:{player_id} → Hash {
    "team_id": uint64,
    "inviter_id": uint64,
    "timestamp": uint64
  }
```

### 队伍数据结构（内存）

```cpp
struct TeamInfo {
    uint64_t team_id;
    uint64_t leader_id;
    std::vector<uint64_t> members;  // 队长在 index 0
    TeamStatus status;
    int cave_level;
    uint64_t created_at;
};

enum class TeamStatus : int32_t {
    IDLE = 0,
    IN_CAVE = 1,
};
```

### 经验分配逻辑

```
怪物被玩家 P 击杀
       │
       ▼
GameServer 查询 TeamServer：P 是否在队伍中？
       │
       ▼ 是
获取队伍在线成员列表（在同一矿洞层的成员）
       │
       ▼
exp_per_member = total_exp / 在场成员数
       │
       ▼
每个在场成员获得 exp_per_member
```

**掉落分配**：掉落物在原地生成，谁拾取归谁。

## 消息协议设计

### MsgID 分配（6000-6099）

| MsgID | 名称 | 方向 | 说明 |
|-------|------|------|------|
| 6001 | TEAM_CREATE_REQ | Client → TeamServer | 创建队伍 |
| 6002 | TEAM_CREATE_RESP | TeamServer → Client | 创建结果 |
| 6003 | TEAM_DISBAND_REQ | Client → TeamServer | 解散队伍（队长） |
| 6004 | TEAM_DISBAND_RESP | TeamServer → Client | 解散结果 |
| 6005 | TEAM_INVITE_REQ | Client → TeamServer | 邀请玩家 |
| 6006 | TEAM_INVITE_RESP | TeamServer → Client | 邀请结果 |
| 6007 | TEAM_INVITE_NOTIFY | TeamServer → Client | 收到邀请通知 |
| 6008 | TEAM_ACCEPT_REQ | Client → TeamServer | 接受邀请 |
| 6009 | TEAM_ACCEPT_RESP | TeamServer → Client | 接受结果 |
| 6010 | TEAM_REJECT_REQ | Client → TeamServer | 拒绝邀请 |
| 6011 | TEAM_REJECT_RESP | TeamServer → Client | 拒绝结果 |
| 6012 | TEAM_LEAVE_REQ | Client → TeamServer | 离开队伍 |
| 6013 | TEAM_LEAVE_RESP | TeamServer → Client | 离开结果 |
| 6014 | TEAM_KICK_REQ | Client → TeamServer | 踢出成员（队长） |
| 6015 | TEAM_KICK_RESP | TeamServer → Client | 踢出结果 |
| 6016 | TEAM_INFO_REQ | Client → TeamServer | 查询队伍信息 |
| 6017 | TEAM_INFO_RESP | TeamServer → Client | 队伍信息 |
| 6018 | TEAM_MEMBER_UPDATE | TeamServer → Client | 成员变更通知 |
| 6019 | TEAM_LEADER_CHANGE | TeamServer → Client | 队长变更通知 |
| 6020 | TEAM_STATUS_UPDATE | TeamServer → Client | 队伍状态变更 |

### Protobuf 定义

```protobuf
syntax = "proto3";
package farm;
option optimize_for = LITE_RUNTIME;

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
    int32 code = 1;
    uint64 team_id = 2;
}

// 解散队伍
message TeamDisbandReq {}
message TeamDisbandResp {
    int32 code = 1;
}

// 邀请玩家
message TeamInviteReq {
    uint64 target_id = 1;
}
message TeamInviteResp {
    int32 code = 1;
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
    int32 code = 1;
    repeated TeamMember members = 2;
}

// 拒绝邀请
message TeamRejectReq {
    uint64 team_id = 1;
}
message TeamRejectResp {
    int32 code = 1;
}

// 离开队伍
message TeamLeaveReq {}
message TeamLeaveResp {
    int32 code = 1;
}

// 踢出成员
message TeamKickReq {
    uint64 target_id = 1;
}
message TeamKickResp {
    int32 code = 1;
}

// 队伍信息查询
message TeamInfoResp {
    int32 code = 1;
    uint64 team_id = 2;
    uint64 leader_id = 3;
    repeated TeamMember members = 4;
    uint32 status = 5;
    int32 cave_level = 6;
}

// 成员变更通知
message TeamMemberUpdate {
    uint32 update_type = 1;     // 0=join, 1=leave, 2=kick, 3=online, 4=offline
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
```

## 核心流程

### 1. 创建队伍

```
Client A              GateServer           TeamServer              Redis
   │                      │                     │                    │
   │──TEAM_CREATE_REQ──▶│                     │                    │
   │                      │──6001────────────▶│                    │
   │                      │                     │──生成 team_id─────▶│
   │                      │                     │──SADD members────▶│
   │                      │                     │──SET player:team─▶│
   │                      │◀─TEAM_CREATE_RESP─│                    │
   │◀─TEAM_CREATE_RESP──│                     │                    │
```

### 2. 邀请 & 加入

```
Client A(队长)        GateServer           TeamServer         Redis      Client B
   │                      │                     │               │           │
   │──TEAM_INVITE_REQ──▶│                     │               │           │
   │                      │──6005────────────▶│               │           │
   │                      │                     │──检查队伍人数  │           │
   │                      │                     │──HSET invite──▶│           │
   │                      │◀─TEAM_INVITE_RESP─│               │           │
   │◀─TEAM_INVITE_RESP──│                     │               │           │
   │                      │                     │──────────────────────────▶│
   │                      │                     │  TEAM_INVITE_NOTIFY      │
   │                      │                     │               │           │
   │                      │                     │◀─────────────────────────│
   │                      │                     │  TEAM_ACCEPT_REQ         │
   │                      │                     │──SADD members─▶│          │
   │                      │                     │──SET player:team▶│        │
   │                      │                     │──HDEL invite──▶│          │
   │                      │                     │─────────────────────────▶│
   │                      │                     │  TEAM_ACCEPT_RESP        │
   │                      │                     │               │          │
   │                      │                     │──通知所有成员──│          │
   │◀─TEAM_MEMBER_UPDATE─│                     │               │          │
   │                      │                     │─────────────────────────▶│
   │                      │                     │  TEAM_MEMBER_UPDATE      │
```

### 3. 队长退出 → 自动转移

```
Client A(队长)        GateServer           TeamServer         Redis
   │                      │                     │               │
   │──TEAM_LEAVE_REQ───▶│                     │               │
   │                      │──6012────────────▶│               │
   │                      │                     │──检查是否队长  │
   │                      │                     │  是 → 转移队长  │
   │                      │                     │──更新 members─▶│
   │                      │                     │──SET new leader▶│
   │                      │◀─TEAM_LEAVE_RESP──│               │
   │                      │                     │──通知所有成员──│
   │                      │                     │  TEAM_LEADER_CHANGE
   │                      │                     │  TEAM_MEMBER_UPDATE
```

### 4. 组队战斗经验分配

```
GameServer                          TeamServer
   │                                    │
   │ 怪物被玩家 P 击杀                   │
   │                                    │
   │──查询 P 的队伍 ──────────────────▶│
   │◀─返回队伍成员列表────────────────│
   │                                    │
   │ 过滤：在同一矿洞层的在线成员        │
   │ exp_per = total_exp / 在场成员数    │
   │                                    │
   │ 给每个在场成员增加经验              │
```

### 5. 队伍聊天

```
Client A              GateServer           ChatServer          TeamServer
   │                      │                     │                   │
   │──ChatSendReq──────▶│                     │                   │
   │  (channel=PARTY)    │──4001────────────▶│                   │
   │                      │                     │──查询队伍成员────▶│
   │                      │                     │◀─成员列表────────│
   │                      │                     │──广播给成员──────│
   │                      │◀─ChatMessage──────│                   │
   │◀─ChatMessage───────│                     │                   │
```

## 错误处理与边界情况

### 错误码

```cpp
enum class TeamErrorCode : int32_t {
    SUCCESS = 0,
    ALREADY_IN_TEAM = 1,       // 已在队伍中
    TEAM_FULL = 2,             // 队伍已满（4人）
    NOT_IN_TEAM = 3,           // 不在任何队伍中
    NOT_LEADER = 4,            // 不是队长，无权限
    TARGET_NOT_FOUND = 5,      // 目标玩家不存在
    TARGET_ALREADY_IN_TEAM = 6,// 目标已有队伍
    TARGET_OFFLINE = 7,        // 目标不在线
    INVITE_NOT_FOUND = 8,      // 邀请不存在/已过期
    INVITE_EXPIRED = 9,        // 邀请已过期
    CANNOT_KICK_SELF = 10,     // 不能踢自己
    TEAM_IN_CAVE = 11,         // 队伍在矿洞中，不能解散
    SELF_OPERATION = 12,       // 不能邀请自己
    PLAYER_BLOCKED = 13,       // 被对方拉黑
};
```

### 边界情况

| 场景 | 处理 |
|------|------|
| 玩家已在一个队伍，尝试创建新队伍 | 返回 ALREADY_IN_TEAM |
| 玩家已在一个队伍，被邀请加入另一个 | 返回 TARGET_ALREADY_IN_TEAM |
| 邀请 60 秒内未响应 | 自动过期，通知邀请者 |
| 队伍在矿洞中，队长尝试解散 | 返回 TEAM_IN_CAVE，需先离开矿洞 |
| 队伍所有成员下线 | 队伍保留，成员上线后仍在队伍中 |
| 队长下线，其他成员上线 | 队长仍为下线玩家，上线后仍是队长 |
| 网络断开重连 | 客户端重新查询队伍信息，恢复 UI 状态 |

## 客户端 UI 设计

### TeamManager（客户端管理器）

```python
class TeamManager:
    """客户端组队管理器"""
    def __init__(self, connection):
        self._connection = connection
        self._team_id: int = 0
        self._leader_id: int = 0
        self._members: List[TeamMember] = []
        self._pending_invites: List[TeamInviteNotify] = []

    @property
    def in_team(self) -> bool:
        return self._team_id != 0

    @property
    def is_leader(self) -> bool:
        return self._leader_id == self._player_id

    def create_team(self) -> None: ...
    def invite_player(self, target_id: int) -> None: ...
    def accept_invite(self, team_id: int) -> None: ...
    def reject_invite(self, team_id: int) -> None: ...
    def leave_team(self) -> None: ...
    def kick_member(self, target_id: int) -> None: ...
    def disband_team(self) -> None: ...
```

### 队伍信息 HUD（战斗场景中显示）

```
┌──────────────────────────────┐
│  队伍 (3/4)        [离开队伍] │
├──────────────────────────────┤
│  👑 玩家A  Lv.15  HP 80/100  │
│     玩家B  Lv.12  HP 60/100  │
│     玩家C  Lv.8   HP 45/80   │
└──────────────────────────────┘
```

- 位置：屏幕左上角（战斗场景中）
- 队长有 👑 标识
- 实时显示队员 HP
- 队伍聊天消息在聊天面板的「队伍」标签页显示

### 邀请通知 UI

```
┌─────────────────────────────────────┐
│  玩家A 邀请你加入队伍               │
│  等级: 15  当前: 矿洞第 2 层         │
│                                     │
│  [接受]              [拒绝]         │
└─────────────────────────────────────┘
```

- 收到邀请时弹出通知
- 60 秒后自动消失（视为拒绝）

### 队伍操作菜单（右键玩家头像）

```
┌─────────────────┐
│  私聊            │
│  加为好友        │
│  邀请组队        │
│  ...             │
└─────────────────┘
```

## 实现范围

### TeamServer 代码结构

```
scripts/server/team_server/
├── main.cpp                    // 入口，事件循环
├── team_server.h/cpp           // 核心服务类
├── gate_session.h/cpp          // 与 GateServer 的连接会话
├── team_manager.h/cpp          // 队伍管理
├── redis_client.h/cpp          // Redis 客户端
└── game_server_client.h/cpp    // 与 GameServer 的内部通信
```

### GateServer 扩展

```cpp
// 新增路由规则
if (msg_id >= 6000 && msg_id < 7000) {
    team_server_conn->forward(player_id, msg_id, payload);
}
```

### 实现阶段

| 阶段 | 内容 | 依赖 |
|------|------|------|
| Phase 1 | TeamServer 基础框架 + Redis 连接 + GateServer 路由 | 无 |
| Phase 2 | 创建/解散/邀请/加入/离开 核心逻辑 | Phase 1 |
| Phase 3 | 队长转移/踢人/状态查询 | Phase 2 |
| Phase 4 | GameServer 集成（战斗经验分配） | Phase 2 + 怪兽战斗系统 |
| Phase 5 | ChatServer 集成（队伍聊天广播） | Phase 2 + 聊天系统 |
| Phase 6 | 客户端 UI（队伍 HUD + 邀请通知） | Phase 3 |

### 依赖关系

```
TeamServer
  ├── 依赖: GateServer（消息路由）
  ├── 依赖: Redis（状态存储）
  ├── 通知: GameServer（队伍变更 → 经验分配）
  └── 通知: ChatServer（队伍变更 → 聊天广播）

GameServer
  └── 查询: TeamServer（击杀怪物时获取队伍成员）

ChatServer
  └── 查询: TeamServer（队伍频道广播时获取成员列表）
```

## 测试策略

### 单元测试

```
TeamServer 单元测试：
├── team_manager_test.cpp     // 队伍管理逻辑测试
├── redis_client_test.cpp     // Redis 操作测试
└── invite_timeout_test.cpp   // 邀请超时测试
```

### 集成测试

```
集成测试场景：
├── 创建队伍完整流程（创建 → 邀请 → 接受 → 成员列表更新）
├── 队长转移流程（队长退出 → 自动转移 → 新队长生效）
├── 经验分配流程（组队击杀 → 经验平均分配）
├── 队伍聊天流程（发送队伍消息 → 成员收到）
└── 边界情况测试（重复邀请、满员邀请、离线邀请等）
```

### 客户端测试

```
客户端测试：
├── team_manager_test.py       // TeamManager 逻辑测试
├── team_hud_test.py           // 队伍 HUD 渲染测试
└── team_invite_notify_test.py // 邀请通知测试
```
