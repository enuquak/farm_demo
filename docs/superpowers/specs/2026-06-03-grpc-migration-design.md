# gRPC 全面迁移设计

**日期**: 2026-06-03
**状态**: 设计确认
**范围**: 全部通信（客户端↔Gate + 服务间）

---

## 1. 背景与动机

当前项目使用 **libevent + Protobuf over TCP** 自定义协议进行所有通信。随着服务数量增加（7个服务）和功能复杂度提升，存在以下问题：

1. **大量样板代码** — 每个服务手写 ConnectionManager、消息解析、identify 握手、心跳、重连逻辑
2. **msg_id 分发脆弱** — 新增消息需要手动注册 msg_id、修改分发逻辑，容易遗漏
3. **缺乏标准化 RPC 能力** — 无内置 deadline、retry、load balancing、健康检查
4. **无流式通信** — 推送场景变化、聊天消息等需要手写队列和推送逻辑

**迁移目标**：
- 性能和连接效率（HTTP/2 多路复用）
- 减少样板代码（.proto service 定义替代手写分发）
- 标准化 RPC 能力（deadline、retry、load balancing、health check）
- 流式通信支持（bidirectional streaming 推送实时状态）

---

## 2. 架构设计

### 2.1 整体分层

```
┌─────────────────────────────────────────────────────────────┐
│                    客户端层 (Python grpcio)                   │
│  FarmGateway service — bidirectional streaming               │
└──────────────────────────┬──────────────────────────────────┘
                           │ gRPC (HTTP/2)
┌──────────────────────────▼──────────────────────────────────┐
│                    Gate Server (路由层)                       │
│  - 接受客户端 streaming 连接                                   │
│  - 按 service/method 路由到后端                               │
│  - 拦截器：认证、日志、限流                                     │
└──────┬───────────┬───────────┬──────────────────────────────┘
       │           │           │
       ▼           ▼           ▼
┌──────────┐ ┌──────────┐ ┌──────────┐
│GameServer│ │ChatServer│ │TeamServer│
│gRPC svc  │ │gRPC svc  │ │gRPC svc  │
└────┬─────┘ └──────────┘ └──────────┘
     │
     ├──▶ DBMgrService
     ├──▶ FriendService
     └──▶ CrossServerService
```

### 2.2 Proto Service 划分

#### 客户端层 Service（Gate Server 暴露）

```protobuf
// gateway.proto — 客户端唯一入口
service FarmGateway {
    // 双向流：客户端发送操作请求，服务端推送实时事件
    rpc GameStream(stream ClientRequest) returns (stream ServerEvent);
}

message ClientRequest {
    oneof request {
        LoginRequest login = 1;
        CreateRoleRequest create_role = 2;
        EnterGameRequest enter_game = 3;
        PlayerAction player_action = 4;      // 所有游戏操作
        ChatRequest chat = 5;
        TeamRequest team = 6;
        FriendRequest friend = 7;
    }
}

message ServerEvent {
    oneof event {
        LoginResponse login = 1;
        CreateRoleResponse create_role = 2;
        EnterGameResponse enter_game = 3;
        PlayerActionResult action_result = 4;
        SceneUpdate scene_update = 5;        // 场景状态推送
        ChatMessage chat_message = 6;
        TeamEvent team_event = 7;
        FriendEvent friend_event = 8;
        CombatEvent combat_event = 9;
        NotificationEvent notification = 10;
    }
}
```

#### 内部 Service（服务间通信）

```protobuf
// dbmgr_internal.proto
service DBMgrService {
    rpc QueryPlayerData(QueryPlayerDataReq) returns (QueryPlayerDataResp);
    rpc SavePlayerData(SavePlayerDataReq) returns (SavePlayerDataResp);
    rpc QueryAccount(QueryAccountReq) returns (QueryAccountResp);
    rpc CreateAccount(CreateAccountReq) returns (CreateAccountResp);
    rpc CreateRole(CreateRoleReq) returns (CreateRoleResp);
    rpc AllocPlayerId(AllocPlayerIdReq) returns (AllocPlayerIdResp);
}

// friend_internal.proto
service FriendService {
    rpc SearchPlayer(SearchPlayerReq) returns (SearchPlayerResp);
    rpc AddFriend(AddFriendReq) returns (AddFriendResp);
    rpc AcceptFriend(AcceptFriendReq) returns (AcceptFriendResp);
    rpc DeleteFriend(DeleteFriendReq) returns (DeleteFriendResp);
    rpc SendFriendChat(SendFriendChatReq) returns (SendFriendChatResp);
    rpc GetFriendList(GetFriendListReq) returns (GetFriendListResp);
}

// team_internal.proto
service TeamService {
    rpc CreateTeam(CreateTeamReq) returns (CreateTeamResp);
    rpc DisbandTeam(DisbandTeamReq) returns (DisbandTeamResp);
    rpc InviteToTeam(InviteToTeamReq) returns (InviteToTeamResp);
    rpc AcceptInvite(AcceptInviteReq) returns (AcceptInviteResp);
    rpc LeaveTeam(LeaveTeamReq) returns (LeaveTeamResp);
    rpc GetTeamInfo(GetTeamInfoReq) returns (GetTeamInfoResp);
}

// game_internal.proto — Gate→Game 的内部通信
service GameService {
    rpc PlayerJoin(PlayerJoinReq) returns (PlayerJoinResp);
    rpc PlayerLeave(PlayerLeaveReq) returns (PlayerLeaveResp);
    rpc ForwardClientMessage(ForwardMessageReq) returns (ForwardMessageResp);
}
```

### 2.3 Proto 文件组织

```
scripts/common/proto/
├── client/
│   └── gateway.proto              # 客户端↔Gate 的 service 定义
├── internal/
│   ├── game_service.proto         # Gate↔Game
│   ├── dbmgr_service.proto        # Game↔DBMgr
│   ├── friend_service.proto       # Game↔FriendService
│   ├── team_service.proto         # Game↔TeamService, Gate↔TeamService
│   ├── chat_service.proto         # Gate↔ChatServer
│   └── cross_service.proto        # Game↔CrossServer
├── messages/
│   ├── player_messages.proto      # 玩家相关消息类型
│   ├── scene_messages.proto       # 场景相关消息类型
│   ├── chat_messages.proto        # 聊天消息类型
│   ├── team_messages.proto        # 组队消息类型
│   └── friend_messages.proto      # 好友消息类型
└── generated/
    ├── cpp/
    └── python/
```

---

## 3. 连接管理与 Streaming 策略

### 3.1 客户端↔Gate：Bidirectional Streaming

```cpp
// 客户端连接生命周期：
// 1. 建立 gRPC channel → Gate
// 2. 调用 GameStream() 获取双向流
// 3. 通过 stream 发送所有请求（login、操作、聊天等）
// 4. 通过 stream 接收所有推送（场景更新、聊天消息、战斗事件等）
// 5. 断开时自动重连（gRPC 内置重连机制）
```

**关键设计决策**：
- **一个 stream 搞定一切** — 客户端只需维护一个 `GameStream` 连接，所有请求和推送都走这个 stream
- **gRPC keepalive + 客户端 heartbeat** — gRPC keepalive 保证连接存活（HTTP/2 PING），客户端定期发送 heartbeat 消息作为业务层存活检测，服务端通过 deadline 检测超时断开
- **gRPC metadata** — 替代当前的 identify 握手。客户端连接时通过 metadata 传递 `client_version`、`device_id` 等信息

### 3.2 Gate Server 路由逻辑

```cpp
class FarmGatewayServiceImpl final : public FarmGateway::Service {
    grpc::Status GameStream(
        ServerContext* context,
        ServerReaderWriter<ServerEvent, ClientRequest>* stream) override
    {
        // 读循环：接收 ClientRequest → 根据 oneof 类型路由
        //   login → 直接处理（Gate 认证逻辑）
        //   player_action → 转发到 GameServer via GameService::ForwardClientMessage
        //   chat → 转发到 ChatServer via ChatService
        //   team → 转发到 TeamServer via TeamService

        // 写循环：从队列取 ServerEvent → 推送给客户端
        //   GameServer 推送的场景更新
        //   ChatServer 推送的聊天消息
        //   TeamServer 推送的组队事件
    }
};
```

### 3.3 内部服务连接管理

```cpp
// 当前：每个服务手写 ConnectionManager
// 新方案：使用 gRPC Channel + 自动服务发现

class ServiceChannel {
    // 封装 grpc::Channel，集成 etcd 服务发现
    // - etcd watch 获取服务地址列表
    // - 创建 channel 时使用 gRPC 的 name resolver
    // - 自动负载均衡（gRPC 内置 round-robin）
    // - 自动重连（gRPC 内置）
    // - 健康检查（gRPC health checking protocol）
};
```

**替换关系**：

| 当前组件 | 新方案 | 说明 |
|---|---|---|
| `GameConnection` (手写TCP) | `grpc::Channel` → `GameService::Stub` | gRPC 自动管理连接、重连 |
| `ChatConnection` (手写TCP) | `grpc::Channel` → `ChatService::Stub` | 同上 |
| `DBMgrConnectionManager` | `grpc::Channel` → `DBMgrService::Stub` | 多实例用 gRPC load balancing |
| `FriendServiceConnection` | `grpc::Channel` → `FriendService::Stub` | 同上 |
| `message_parser.h` (手写协议) | **删除** | gRPC 内置序列化 |
| `msg_id 分发` | **删除** | gRPC 按 service/method 自动分发 |
| `identify 握手` | **删除** | gRPC metadata + interceptor |
| `心跳机制` | gRPC deadline + keepalive | 内置机制替代 |
| `自动重连` | gRPC 内置 reconnect | 无需手写 |

---

## 4. 事件循环与线程模型

### 4.1 从 libevent 到 gRPC CompletionQueue

```
当前模型：
  libevent event_base → bufferevent → 手写回调链
  每个 server 一个 event_base，主线程 event_base_dispatch()

新模型：
  grpc::Server → CompletionQueue → 自动 RPC 分发
  gRPC 内置线程池处理 RPC 调用
```

### 4.2 线程模型

```
┌─────────────────────────────────────────────────┐
│                Gate Server                        │
│                                                   │
│  ┌─────────────────────────────────────────────┐ │
│  │         gRPC Server (grpc::Server)           │ │
│  │                                               │ │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐        │ │
│  │  │Thread 1 │ │Thread 2 │ │Thread N │ ...     │ │
│  │  │(CQ poll)│ │(CQ poll)│ │(CQ poll)│        │ │
│  │  └────┬────┘ └────┬────┘ └────┬────┘        │ │
│  │       │           │           │              │ │
│  │       ▼           ▼           ▼              │ │
│  │  ┌──────────────────────────────────────┐   │ │
│  │  │     GameStream Handler (per client)   │   │ │
│  │  │  - 读：接收 ClientRequest → 路由      │   │ │
│  │  │  - 写：从队列取 ServerEvent → 推送    │   │ │
│  │  └──────────────────────────────────────┘   │ │
│  └─────────────────────────────────────────────┘ │
│                                                   │
│  ┌─────────────────────────────────────────────┐ │
│  │         后端连接管理 (per GameServer)         │ │
│  │  - grpc::Channel → GameService::Stub         │ │
│  │  - 异步 RPC 调用 (AsyncUnaryCall)            │ │
│  └─────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
```

### 4.3 异步 vs 同步混合模式

```cpp
// Gate Server — 异步 streaming（处理大量并发客户端连接）
class FarmGatewayServiceImpl : public FarmGateway::AsyncService {
    // 使用 CompletionQueue 处理大量客户端 stream
};

// 内部服务 — 同步 RPC（服务间调用量相对可控，代码更简洁）
class DBMgrServiceImpl : public DBMgrService::Service {
    grpc::Status QueryPlayerData(
        ServerContext* context,
        const QueryPlayerDataReq* request,
        QueryPlayerDataResp* response) override
    {
        // 直接查询 MongoDB，返回结果
        // gRPC 线程池管理并发
    }
};
```

### 4.4 替换 libevent 定时器

```cpp
// 当前：libevent 定时器
// 新方案：std::thread + std::condition_variable
class TimerManager {
    // 游戏定时器数量有限（自动保存、心跳检测等）
    // 独立线程 + 条件变量比 gRPC Alarm 更清晰
};
```

---

## 5. 拦截器设计

### 5.1 认证拦截器

```cpp
class AuthInterceptor : public grpc::experimental::Interceptor {
    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {
        // 从 metadata 提取 token
        // 验证客户端身份
        // 注入 player_id 到 ServerContext
    }
};
```

### 5.2 日志拦截器

```cpp
class LoggingInterceptor : public grpc::experimental::Interceptor {
    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {
        // 记录 RPC 方法名、耗时、状态码
        // 替代当前散落各处的 spdlog 调用
    }
};
```

### 5.3 限流拦截器

```cpp
class RateLimitInterceptor : public grpc::experimental::Interceptor {
    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {
        // 每个客户端的请求频率限制
        // 替代 ChatServer 的 RateLimiter
    }
};
```

---

## 6. 错误处理

### 6.1 gRPC 状态码映射

| gRPC 状态码 | 业务含义 |
|---|---|
| `OK` (0) | 成功 |
| `INVALID_ARGUMENT` | 参数错误（如无效的玩家ID） |
| `NOT_FOUND` | 资源不存在（如角色不存在） |
| `ALREADY_EXISTS` | 重复操作（如已添加的好友） |
| `PERMISSION_DENIED` | 权限不足 |
| `RESOURCE_EXHAUSTED` | 限流触发 |
| `INTERNAL` | 服务器内部错误 |
| `UNAVAILABLE` | 服务不可用（重连中） |

### 6.2 自定义错误详情

```protobuf
message FarmError {
    int32 error_code = 1;      // 业务错误码（保留兼容）
    string error_message = 2;  // 错误描述
    map<string, string> details = 3;  // 附加信息
}
```

---

## 7. 构建系统改造

### 7.1 依赖变化

| 当前依赖 | 新依赖 | 说明 |
|---|---|---|
| `libevent` | **删除** | gRPC 自带事件循环 |
| `protobuf-lite` | `protobuf` (full) | gRPC 需要 full protobuf |
| 无 | `gRPC::grpc++` | gRPC C++ 核心库 |
| `etcd-cpp-apiv3` | 保留 | 服务发现不变 |

### 7.2 CMake 改造

```cmake
# 新增依赖
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# gRPC C++ 插件生成代码
function(farm_generate_grpc PROTO_FILES)
    foreach(PROTO ${PROTO_FILES})
        protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO})
        protobuf_generate_grpc_cpp(GRPC_SRCS GRPC_HDRS ${PROTO})
    endforeach()
endfunction()

# 每个 server 的 CMakeLists.txt
add_executable(gate_server
    src/main.cpp
    src/gate_server.cpp
    ${PROTO_SRCS} ${GRPC_SRCS}
)
target_link_libraries(gate_server
    gRPC::grpc++
    protobuf::libprotobuf
    spdlog::spdlog
)
```

---

## 8. 迁移影响

### 8.1 删除的文件

```
scripts/server/common/
├── include/
│   ├── message_parser.h          ❌ 删除
│   ├── internal_msg_ids.h        ❌ 删除
│   ├── admin_msg_ids.h           ❌ 删除
│   └── message_ids.h             ❌ 删除
└── src/
    ├── message_parser.cpp        ❌ 删除
    └── admin_msg_ids.cpp         ❌ 删除

scripts/server/gate_server/src/
├── game_connection.h/.cpp        ❌ 删除
├── chat_connection.h/.cpp        ❌ 删除

scripts/server/game_server/src/
├── dbmgr_connection.h/.cpp       ❌ 删除
├── dbmgr_connection_manager.h/.cpp ❌ 删除
├── friend_service_connection.h/.cpp ❌ 删除

shared/
└── message_ids.json              ❌ 废弃
```

### 8.2 新增的文件

```
scripts/common/proto/
├── client/gateway.proto           ✨ 新增
├── internal/                      ✨ 新增目录
│   ├── game_service.proto
│   ├── dbmgr_service.proto
│   ├── friend_service.proto
│   ├── team_service.proto
│   ├── chat_service.proto
│   └── cross_service.proto
└── messages/                      ✨ 新增目录
    ├── player_messages.proto
    ├── scene_messages.proto
    ├── chat_messages.proto
    ├── team_messages.proto
    └── friend_messages.proto

scripts/server/common/
├── include/
│   ├── service_channel.h          ✨ 新增
│   ├── interceptor_auth.h         ✨ 新增
│   ├── interceptor_logging.h      ✨ 新增
│   └── interceptor_rate_limit.h   ✨ 新增
└── src/
    ├── service_channel.cpp
    ├── interceptor_auth.cpp
    ├── interceptor_logging.cpp
    └── interceptor_rate_limit.cpp

scripts/server/gate_server/src/
└── gateway_service.h/.cpp         ✨ 新增
```

### 8.3 重构的文件（大幅修改）

```
scripts/server/gate_server/src/
├── gate_server.h/.cpp             ⚠️ 重写
├── session_manager.h/.cpp         ⚠️ 重写
├── session.h/.cpp                 ⚠️ 重写

scripts/server/game_server/src/
├── game_server.h/.cpp             ⚠️ 重写
├── message_handler.h/.cpp         ⚠️ 重构
├── player.h/.cpp                  ⚠️ 修改
├── gate_session.h/.cpp            ⚠️ 重写

scripts/server/dbmgr/src/
├── dbmgr_server.h/.cpp            ⚠️ 重写
├── game_session.h/.cpp            ⚠️ 重写

scripts/server/chat_server/src/
├── chat_server.h/.cpp             ⚠️ 重写

scripts/server/friend_service/src/
├── friend_service.h/.cpp          ⚠️ 重写

scripts/server/team_server/src/
├── team_server.h/.cpp             ⚠️ 重写

scripts/client/
├── connection.py                  ⚠️ 重写
├── heartbeat.py                   ⚠️ 重构
```

### 8.4 保留不变的部分

```
scripts/server/common/
├── include/
│   ├── log_init.h                 ✅ 保留
│   ├── log_macros.h               ✅ 保留
│   ├── etcd_manager.h             ✅ 保留
│   ├── server_main_helper.h       ✅ 保留
│   └── game_constants.h           ✅ 保留

scripts/server/game_server/src/
├── world_state.h/.cpp             ✅ 保留
├── scene_state.h/.cpp             ✅ 保留
├── game_scene_manager.h/.cpp      ✅ 保留
├── crop_system.h/.cpp             ✅ 保留
├── item_interaction_handler.h/.cpp ✅ 保留
├── drop_item_manager.h/.cpp       ✅ 保留
├── game_clock.h/.cpp              ✅ 保留
├── quest_manager.h/.cpp           ✅ 保留
├── monster_manager.h/.cpp         ✅ 保留
├── combat_handler.h/.cpp          ✅ 保留
├── gm_stub.h/.cpp                 ✅ 保留
├── redis_connection.h/.cpp        ✅ 保留
├── event_bus.h/.cpp               ✅ 保留
└── player_id_pool.h/.cpp          ✅ 保留

shared/
├── error_codes.json               ✅ 保留
```

---

## 9. 客户端改造

```python
# 当前：socket + threading + 手写消息解析
# 新方案：grpcio

import grpc
from generated import gateway_pb2, gateway_pb2_grpc

class FarmClient:
    def __init__(self, server_address):
        self.channel = grpc.insecure_channel(server_address)
        self.stub = gateway_pb2_grpc.FarmGatewayStub(self.channel)
        self.stream = None

    def connect(self):
        self.stream = self.stub.GameStream(self._request_iterator())

    def _request_iterator(self):
        """从发送队列取出请求，yield 给 gRPC stream"""
        while True:
            request = self.send_queue.get()
            yield request

    def _receive_loop(self):
        """从 gRPC stream 接收推送事件"""
        for event in self.stream:
            if event.HasField('scene_update'):
                self.on_scene_update(event.scene_update)
            elif event.HasField('chat_message'):
                self.on_chat_message(event.chat_message)
```
