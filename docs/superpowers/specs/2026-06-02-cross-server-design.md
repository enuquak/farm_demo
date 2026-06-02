# CrossServer 跨服通信设计文档

## 概述

新增 CrossServer 进程，作为通用的跨服数据交换层，支持不同 Game Server 之间的数据查询与同步。

## 需求决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 跨服场景 | 跨服数据同步（好友数据查询等） | 核心需求 |
| 数据范围 | 通用：基础信息、动态信息、扩展信息、操作类 | 保持通用性 |
| 通信模式 | 请求-响应模式 | 简单可靠 |
| 玩家路由 | etcd 维护玩家-服务器映射 | 复用现有基础设施 |
| 部署模式 | 多实例无状态 | 高可用 |
| 通信协议 | libevent + Protobuf over TCP | 技术栈统一 |
| 拓扑扩展 | 先单区域，未来扩展多区域 | 渐进式扩展 |

## 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                          etcd                                   │
│  /farm/services/game/{server_id}    → Game Server 注册          │
│  /farm/services/cross/{instance_id} → CrossServer 注册          │
│  /farm/players/{player_id}          → 玩家所在 Server 映射      │
└─────────────────────────────────────────────────────────────────┘
        ▲ watch                              ▲ watch
        │                                    │
┌───────┴───────┐                    ┌───────┴───────┐
│ Game Server 1 │                    │ Game Server 2 │
│  (server_id=1)│                    │  (server_id=2)│
└───────┬───────┘                    └───────┬───────┘
        │                                    │
        │  TCP (libevent+protobuf)           │
        ▼                                    ▼
┌───────────────────────────────────────────────────────┐
│                   CrossServer (N instances)            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │ 路由缓存     │  │ 消息转发     │  │ 连接管理     │  │
│  │ (etcd watch) │  │ (请求-响应)  │  │ (GameServer) │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  │
└───────────────────────────────────────────────────────┘
```

**核心职责**：
1. **路由缓存**：watch etcd `/farm/players/` 前缀，维护 player_id → server_id 映射
2. **消息转发**：接收 Game Server 的跨服请求，路由到目标 Server，返回响应
3. **连接管理**：维护与所有 Game Server 的长连接

## 消息协议设计

### 消息 ID 分配

CrossServer 使用消息 ID 范围 **6000-6999**：

| MsgID | 名称 | 方向 | 说明 |
|-------|------|------|------|
| 6001 | CROSS_IDENTIFY | Game → Cross | Game Server 身份识别 |
| 6002 | CROSS_IDENTIFY_RESP | Cross → Game | 身份识别响应 |
| 6003 | CROSS_HEARTBEAT | Game ↔ Cross | 心跳 |
| 6004 | CROSS_HEARTBEAT_RESP | Game ↔ Cross | 心跳响应 |
| 6010 | CROSS_QUERY_REQ | Game → Cross | 跨服数据查询请求 |
| 6011 | CROSS_QUERY_RESP | Cross → Game | 跨服数据查询响应 |
| 6020 | CROSS_FORWARD_REQ | Cross → Game | 转发请求到目标 Server |
| 6021 | CROSS_FORWARD_RESP | Game → Cross | 目标 Server 返回响应 |

### 核心消息流

```
GameServer1                    CrossServer                    GameServer2
     │                              │                              │
     │──CROSS_QUERY_REQ──────────▶│                              │
     │  {target_player_id,        │                              │
     │   query_type,              │                              │
     │   request_data}            │                              │
     │                              │──查询本地路由缓存──▶│        │
     │                              │◀─server_id=2───────│        │
     │                              │                              │
     │                              │──CROSS_FORWARD_REQ────────▶│
     │                              │  {source_server_id,        │
     │                              │   source_player_id,        │
     │                              │   query_type,              │
     │                              │   request_data}            │
     │                              │                              │──处理请求
     │                              │◀─CROSS_FORWARD_RESP────────│
     │                              │  {response_data}           │
     │◀─CROSS_QUERY_RESP─────────│                              │
     │  {response_data}           │                              │
```

### 查询类型枚举

```cpp
enum class CrossQueryType : uint32_t {
    BASIC_INFO      = 1,   // 基础信息（名字、等级、头像）
    ONLINE_STATUS   = 2,   // 在线状态
    DYNAMIC_INFO    = 3,   // 动态信息（位置、当前状态）
    EXTENDED_INFO   = 4,   // 扩展信息（成就、称号）
    CUSTOM          = 99,  // 自定义查询（透传业务数据）
};
```

### Protobuf 定义

```protobuf
// 跨服查询请求（Game → Cross）
message CrossQueryReq {
    uint64 target_player_id = 1;   // 目标玩家
    uint32 query_type = 2;         // 查询类型
    bytes request_data = 3;        // 业务数据（透传）
}

// 跨服查询响应（Cross → Game）
message CrossQueryResp {
    int32 code = 0;                // 0=成功, 其他=错误码
    bytes response_data = 1;       // 业务数据（透传）
}

// 转发请求（Cross → 目标 Game Server）
message CrossForwardReq {
    uint32 source_server_id = 1;   // 来源 Server ID
    uint64 source_player_id = 2;   // 来源玩家 ID（请求者）
    uint64 target_player_id = 3;   // 目标玩家 ID
    uint32 query_type = 4;         // 查询类型
    bytes request_data = 5;        // 业务数据（透传）
}

// 转发响应（目标 Game Server → Cross）
message CrossForwardResp {
    int32 code = 0;
    bytes response_data = 1;
}
```

**设计要点**：
- `request_data` 和 `response_data` 使用 `bytes` 类型，业务层自行序列化，CrossServer 只做透传
- 这样 CrossServer 不需要理解具体业务，保持通用性

## CrossServer 内部设计

### 文件结构

```
scripts/server/cross_server/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                    // 入口，etcd 注册，事件循环
│   ├── cross_server.h/cpp          // 核心服务类
│   ├── game_session.h/cpp          // 与 Game Server 的会话管理
│   ├── game_connection.h/cpp       // 主动连接 Game Server
│   ├── route_cache.h/cpp           // 路由缓存（etcd watch）
│   └── message_handler.h/cpp       // 消息路由处理
```

### 核心类设计

```cpp
class CrossServer {
public:
    CrossServer(const std::string& ip, uint16_t port, uint32_t instance_id);
    bool start();
    void stop();

private:
    // libevent 回调
    static void on_accept(...);
    static void on_read(...);
    static void on_event(...);
    static void on_heartbeat_timer(...);

    // 消息处理
    void handle_identify(std::shared_ptr<GameSession> session, ...);
    void handle_query_req(std::shared_ptr<GameSession> session, ...);
    void handle_forward_resp(std::shared_ptr<GameSession> session, ...);

    // 发送到目标 Game Server
    bool forward_to_game(uint32_t server_id, uint32_t msg_id, ...);

    // 成员
    struct event_base* base_;
    struct evconnlistener* listener_;
    
    // 被动连接：Game Server 连进来
    std::unordered_map<evutil_socket_t, std::shared_ptr<GameSession>> sessions_;
    
    // 主动连接：连接到 Game Server
    std::unordered_map<uint32_t, std::unique_ptr<GameConnection>> connections_;
    
    // 路由缓存
    RouteCache route_cache_;
};
```

### RouteCache 设计

```cpp
class RouteCache {
public:
    // 初始化：连接 etcd，watch /farm/players/ 前缀
    bool init(const std::string& etcd_endpoints);
    
    // 查询玩家所在 Server
    std::optional<uint32_t> get_server_id(uint64_t player_id);
    
    // 更新玩家路由（etcd watch 回调调用）
    void update(uint64_t player_id, uint32_t server_id);
    void remove(uint64_t player_id);

private:
    std::unordered_map<uint64_t, uint32_t> player_to_server_;
    std::mutex mutex_;  // watch 回调在 etcd 后台线程
    std::unique_ptr<EtcdManager> etcd_;
};
```

### Game Server 端集成

Game Server 需要新增：

```cpp
// 1. 启动时注册玩家到 etcd
etcd.put("/farm/players/" + player_id, server_id);

// 2. 玩家上线/下线时更新
etcd.put("/farm/players/" + player_id, server_id);     // 上线
etcd.del("/farm/players/" + player_id);                 // 下线

// 3. 新增处理 CrossServer 转发请求
void handle_cross_forward_req(std::shared_ptr<CrossSession> session, ...);
```

## 错误处理与边界情况

### 错误码定义

```cpp
enum class CrossErrorCode : int32_t {
    SUCCESS = 0,
    PLAYER_NOT_FOUND = 1,       // 路由表中找不到目标玩家
    TARGET_SERVER_OFFLINE = 2,  // 目标 Game Server 不在线
    TARGET_SERVER_TIMEOUT = 3,  // 目标 Server 响应超时
    INVALID_QUERY_TYPE = 4,     // 无效的查询类型
    FORWARD_FAILED = 5,         // 转发失败
};
```

### 边界情况处理

#### 1. 目标玩家不在线

```
场景：玩家 A 查询玩家 B 的数据，但 B 不在线
处理：
1. CrossServer 查询路由缓存，找不到 B
2. 返回 PLAYER_NOT_FOUND 错误
3. Game Server 通知客户端 "该玩家不在线"
```

#### 2. 目标 Game Server 不在线

```
场景：玩家 B 所在的 Game Server 2 宕机
处理：
1. CrossServer 检测到连接断开
2. 清理该 Server 的所有路由缓存（或等待 etcd lease 过期）
3. 返回 TARGET_SERVER_OFFLINE 错误
```

#### 3. 响应超时

```
场景：目标 Server 处理过慢或网络延迟
处理：
1. CrossServer 设置超时定时器（默认 5 秒）
2. 超时后返回 TARGET_SERVER_TIMEOUT 错误
3. 记录超时日志用于监控
```

#### 4. CrossServer 自身重启

```
场景：CrossServer 进程重启
处理：
1. 重启后重新 watch etcd /farm/players/ 前缀
2. 重建路由缓存（etcd 是数据源，无状态）
3. Game Server 检测到连接断开后自动重连
```

#### 5. Game Server 动态增减

```
场景：新增或下线 Game Server
处理：
1. CrossServer watch etcd /farm/services/game/ 前缀
2. 新增：主动建立连接
3. 下线：断开连接，清理该 Server 的路由缓存
```

## 测试策略

### 单元测试

```
cross_server/tests/
├── test_route_cache.cpp        // 路由缓存测试
│   ├── test_update_and_query   // 更新后查询
│   ├── test_remove             // 删除路由
│   └── test_concurrent         // 并发读写
├── test_message_handler.cpp    // 消息处理测试
└── test_game_connection.cpp    // 连接管理测试
```

### 集成测试

```
集成测试场景：
├── 基本查询流程：Server1 → CrossServer → Server2 → 返回数据
├── 玩家不在线：查询不存在的玩家，返回错误
├── 目标 Server 下线：模拟 Server 宕机，返回错误
├── CrossServer 重启：重启后路由缓存自动恢复
└── 多 CrossServer 实例：两个实例同时工作，互不影响
```

### 手动测试

```
手动测试：
├── 启动 2 个 Game Server + 1 个 CrossServer
├── 玩家 A 在 Server1 登录
├── 玩家 B 在 Server2 登录
├── A 查询 B 的基础信息
├── 验证返回数据正确
└── B 下线后，A 再次查询，验证返回错误
```

## 实现范围

### Phase 1：基础框架（本次实现）

| 模块 | 内容 |
|------|------|
| CrossServer 进程 | main.cpp, 事件循环, etcd 注册 |
| 路由缓存 | RouteCache, etcd watch |
| 消息转发 | 基本的请求-响应转发 |
| Game Server 集成 | 玩家上下线更新 etcd, 处理跨服查询 |
| 单元测试 | RouteCache 测试 |

### Phase 2：完善功能（后续）

| 模块 | 内容 |
|------|------|
| 超时处理 | 请求超时定时器 |
| 监控指标 | 转发延迟、成功率统计 |
| 多区域支持 | CrossServer 之间中转 |
| 性能优化 | 批量查询、连接池 |

## 依赖关系

```
CrossServer 依赖：
├── common/etcd_manager.h       // 已有
├── common/message_parser.h     // 已有
├── common/internal_msg_ids.h   // 需新增 6000-6999
└── common/log_*.h              // 已有

Game Server 需修改：
├── 玩家登录流程 → 注册到 etcd
├── 玩家下线流程 → 从 etcd 删除
└── 新增 handle_cross_forward_req
```

## etcd Key 结构扩展

```
/farm/
├── services/
│   ├── game/{server_id}          → {"ip":"0.0.0.0","port":9090,"server_id":1}
│   ├── cross/{instance_id}       → {"ip":"0.0.0.0","port":7070,"instance_id":1}
│   └── ...
│
└── players/
    └── {player_id}               → {"server_id":1}  // 玩家所在 Server
```

- `/farm/players/` 下的 key 绑定 Game Server 的 lease
- Game Server 下线时，lease 过期自动清除其所有玩家路由
- CrossServer watch 此前缀维护本地路由缓存
