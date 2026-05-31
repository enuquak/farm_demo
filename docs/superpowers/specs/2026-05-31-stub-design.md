# Stub 概念设计：LoginStub + OnlineStub

## 概述

引入 "Stub" 概念，作为 Game Server 内部的单点组件，负责特定业务领域的逻辑管理。本版实现两个 Stub：

- **LoginStub**：完全替代现有登录流程（AccountMessageHandler + EnterGameReq），管理玩家登录状态
- **OnlineStub**：提供玩家在线状态查询接口，读取 Redis 中的映射数据，为后续跨服互通提供基础

## 关键决策

| 决策项 | 选择 |
|--------|------|
| Stub 归属 | Game Server 内部组件 |
| LoginStub 职责 | 完全替代现有登录逻辑 |
| Redis 访问方式 | Game Server 直连 Redis |
| Redis 数据结构 | 简单 KV 映射（player_id → server_id） |
| Redis Key 格式 | `player:online:{player_id}`，冒号分隔 |
| Redis 客户端 | hiredis + 自写薄封装（复用 DBMgr 提案设计） |
| 连接管理层 | Game Server 统一管理 Redis 连接 |
| OnlineStub 用途 | 在线状态查询（本版） |

## 架构设计

### 整体架构

```
Game Server
├── RedisConnection          // Game Server 持有，管理 Redis 连接生命周期
├── LoginStub                // 登录业务逻辑，调用 RedisConnection 写入在线映射
├── OnlineStub               // 在线状态查询，调用 RedisConnection 读取映射
├── PlayerManager            // 保持不变，管理内存玩家对象
├── DBMgrConnectionManager   // 保持不变，仍负责玩家数据持久化
└── MessageHandler           // 保持不变，业务消息分发框架
```

### 组件职责

**RedisConnection（Redis 连接层）：**
- 薄封装 hiredis，管理连接生命周期
- 提供 KV 操作接口（set/get/del）
- 连接失败时标记状态，不阻塞主流程

**LoginStub（登录业务逻辑）：**
- 接收 Gate 转发的 EnterGameReq
- 协调账号验证、角色创建/查询、玩家数据加载
- 登录成功后写入 Redis（`SET player:online:{pid} {sid}`）
- 玩家下线时清理 Redis（`DEL player:online:{pid}`）

**OnlineStub（在线状态查询）：**
- 查询指定玩家是否在线、在哪个服
- 为后续跨服功能提供在线状态接口

## Redis 数据结构

### Key 格式

```
KEY:   player:online:{player_id}
TYPE:  String
VALUE: {server_id}
TTL:   无（玩家下线时主动删除）
```

### 示例

```
player:online:1048577 → "1"    # 玩家 1048577 在 server 1
player:online:2097153 → "2"    # 玩家 2097153 在 server 2
```

### 操作映射

| 场景 | Redis 命令 | 说明 |
|------|-----------|------|
| 玩家登录 | `SET player:online:{pid} {sid}` | 标记在线 |
| 玩家下线 | `DEL player:online:{pid}` | 标记离线 |
| 查询玩家在哪个服 | `GET player:online:{pid}` | 返回 server_id，空表示不在线 |

## 接口定义

### RedisConnection

```cpp
class RedisConnection {
public:
    RedisConnection();
    ~RedisConnection();

    // 连接管理
    bool connect(const std::string& uri);
    void disconnect();
    bool is_connected() const;

    // KV 操作
    bool set(const std::string& key, const std::string& value);
    std::string get(const std::string& key);  // 空字符串表示不存在
    bool del(const std::string& key);

private:
    redisContext* context_ = nullptr;
    bool connected_ = false;
};
```

### LoginStub

```cpp
class LoginStub {
public:
    LoginStub(PlayerManager& player_mgr,
              DBMgrConnectionManager& dbmgr_mgr,
              RedisConnection& redis_conn,
              uint32_t server_id);

    // 处理进入游戏请求（替代现有 handle_enter_game_req）
    void handle_enter_game_req(uint64_t player_id, GateSession* gate_session);

    // 玩家下线时调用，清理 Redis 映射
    void on_player_offline(uint64_t player_id);

private:
    // 登录成功后写入 Redis
    void mark_player_online(uint64_t player_id);

    PlayerManager& player_mgr_;
    DBMgrConnectionManager& dbmgr_mgr_;
    RedisConnection& redis_conn_;
    uint32_t server_id_;
};
```

### OnlineStub

```cpp
class OnlineStub {
public:
    explicit OnlineStub(RedisConnection& redis_conn);

    // 查询玩家在哪个服，返回 server_id（0 表示不在线）
    using OnlineQueryCallback = std::function<void(uint64_t player_id, uint32_t server_id)>;
    void query_player_server(uint64_t player_id, OnlineQueryCallback callback);

private:
    RedisConnection& redis_conn_;
};
```

## 登录流程

### 现有流程

```
Gate → Game: EnterGameReq (player_id, account_id)
Game: handle_enter_game_req
  ├── PlayerManager::add_player_with_data_load(player_id, gate_session, callback)
  │     └── DBMgrConnectionManager::send_player_data_req(player_id)
  └── callback → 构造 EnterGameResp → 发送回 Gate
```

### 新流程

```
Gate → Game: EnterGameReq (player_id, account_id)
Game: LoginStub::handle_enter_game_req
  ├── 1. PlayerManager::add_player_with_data_load(player_id, gate_session, callback)
  │        └── DBMgrConnectionManager::send_player_data_req(player_id)
  ├── 2. callback 中：
  │        ├── 构造 EnterGameResp
  │        ├── mark_player_online(player_id)  ← 新增
  │        │     └── RedisConnection::SET player:online:{pid} {server_id}
  │        └── 发送回 Gate
  └── 3. 返回
```

### 下线流程

```
Gate 断开连接
Game: PlayerManager::remove_players_by_gate_with_save
  ├── 保存玩家数据到 DBMgr
  └── LoginStub::on_player_offline(player_id)
        └── RedisConnection::DEL player:online:{pid}
```

## 配置变更

`config/game_server.json` 新增 Redis 配置块：

```json
{
    "server": {
        "id": 1,
        "ip": "0.0.0.0",
        "port": 6000
    },
    "redis": {
        "uri": "redis://localhost:6379",
        "retry_interval_ms": 3000,
        "max_retry_count": 0
    },
    "dbmgr": { ... }
}
```

**说明：**
- `uri`：Redis 连接字符串
- `retry_interval_ms`：重试间隔（毫秒）
- `max_retry_count`：最大重试次数，`0` 表示无限重试

## 错误处理

### Redis 连接失败

- Game Server 启动时如果 Redis 连接失败，不影响正常启动
- LoginStub 的 `mark_player_online` 调用时检查连接状态，未连接则跳过（降级为不写 Redis）
- OnlineStub 的查询返回 0（不在线），并记录警告日志
- 后台重试线程自动恢复连接

### Redis 操作失败

- `set`/`del`/`get` 返回失败时，记录错误日志，不阻塞主流程
- 登录流程不受影响，Redis 映射是辅助功能，失败不影响玩家进入游戏

## 文件变更清单

### 新增文件

```
scripts/server/game_server/src/
├── login_stub.h / .cpp          // 登录业务逻辑
├── online_stub.h / .cpp         // 在线状态查询
└── redis_connection.h / .cpp    // Redis 连接封装
```

### 修改文件

```
scripts/server/game_server/src/
├── game_server.h / .cpp          // 集成 LoginStub、OnlineStub、RedisConnection
├── player_manager.h / .cpp       // 下线时通知 LoginStub
└── main.cpp                      // 加载 Redis 配置

scripts/server/game_server/CMakeLists.txt  // 添加新源文件、链接 hiredis

config/game_server.json           // 新增 Redis 配置
```

### 删除文件

```
scripts/server/game_server/src/
├── account_message_handler.h / .cpp  // 被 LoginStub 替代
```

## GameServer 类变更

```cpp
class GameServer {
    // 新增成员
    RedisConnection redis_conn_;
    LoginStub login_stub_;
    OnlineStub online_stub_;

    // 构造函数变更
    GameServer(..., const std::string& redis_uri);

    // 消息路由变更
    void handle_enter_game_req(...);  // 委托给 login_stub_

    // 下线流程变更
    void on_player_offline(uint64_t player_id);  // 调用 login_stub_.on_player_offline
};
```

## 依赖关系

```
LoginStub
  ├── PlayerManager（玩家数据管理）
  ├── DBMgrConnectionManager（数据持久化）
  └── RedisConnection（在线映射写入）

OnlineStub
  └── RedisConnection（在线映射读取）

RedisConnection
  └── hiredis（Redis C 客户端）
```

## 后续扩展

本版设计为后续跨服功能预留了扩展点：

1. **跨服消息路由**：通过 OnlineStub 查询目标玩家所在 server_id，路由消息
2. **好友系统**：查询好友在线状态
3. **跨服排行榜**：从 Redis 读取各服在线玩家数据
4. **跨服战斗**：基于在线状态匹配对手

## 与 DBMgr 提案的关系

- DBMgr 提案中的 Redis 用于玩家数据缓存（MongoDB 前端缓存）
- 本设计中的 Redis 用于在线状态映射（player_id → server_id）
- 两者使用同一个 Redis 实例，但 key 命名空间不同，互不干扰
- RedisConnection 的设计复用 DBMgr 提案的模式，但独立实现
