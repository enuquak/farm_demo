---
name: dbmgr-data-layer
description: DBMgr 数据层：MongoDB/Redis 集成、ConnectionManager 状态机、数据路由、全局 ID 分配。
metadata:
  type: reference
---

# DBMgr 数据层

## 概述

DBMgr 是纯数据存取代理，负责 Game Server 与数据库之间的连接和读写操作。它不做任何业务逻辑，仅提供玩家数据和账号数据的 CRUD 接口。

- 数据存储使用 MongoDB（主存储），Redis 作为纯缓存层加速热数据访问
- 支持多 DBMgr 实例部署，通过 `--index` 参数区分
- 配置仅在 `config/dbmgr.json` 中，索引配置在 `config/mongo/` 目录下

## 架构设计

### MongoDB 集成

**连接层 (MongoConnection)：**
- 薄封装 libmongoc（C 驱动）
- 提供 `connect(uri)` / `disconnect()` / `is_connected()` / `client()` 接口
- 连接字符串包含数据库名（如 `mongodb://localhost:27017/farm`）

**业务层 (MongoServer)：**
- 依赖 MongoConnection，实现玩家数据和账号数据的 CRUD
- `init()` 时读取索引配置，自动创建缺失索引

**集合与文档结构：**

| 集合 | 文档结构 | 索引 |
|------|----------|------|
| `players` | `{_id, player_id: int64, data: {}}` | `player_id` 唯一索引 |
| `accounts` | `{_id, account_id: string, roles: [{server_id, player_id, role_name}]}` | `account_id` 唯一索引 |

**索引配置文件（`config/mongo/`）：**
- `players_index.json` — players 集合索引
- `accounts_index.json` — accounts 集合索引
- 支持 `key`、`unique`、`sparse`、`expireAfterSeconds` 选项
- 索引在 `MongoServer::init()` 时自动创建，已存在的不会修改或删除

### Redis 集成

**连接层 (RedisConnection)：**
- 薄封装 hiredis（C 客户端）
- 提供 `connect(uri)` / `disconnect()` / `is_connected()` / `context()` 接口

**业务层 (RedisServer)：**
- 依赖 RedisConnection，实现通用缓存操作 `get/set/del`
- TTL 无默认值，由调用方传入

**缓存策略：**
- **读穿透**：`RedisServer.get()` → 命中则返回；未命中 → `MongoServer.get()` → 返回并回填 Redis
- **写穿透**：`MongoServer.set()` → 成功 → 删除 Redis key → 返回（写穿透失效）
- 缓存结果枚举：`SUCCESS(0)`, `NOT_FOUND(1)`, `CONNECTION_ERROR(2)`, `TIMEOUT(3)`

### ConnectionManager 状态机

ConnectionManager 持有 MongoConnection 和 RedisConnection，管理连接生命周期：

```
DISCONNECTED → CONNECTING → CONNECTED
                          → FAILED → (后台重试) → CONNECTED
                                                 → FAILED_PERMANENT
```

- **CONNECTING**：正在尝试连接
- **CONNECTED**：全部就绪，可对外服务
- **FAILED**：部分连接失败，后台重试线程运行中
- **FAILED_PERMANENT**：超过最大重试次数，停止重试
- 提供 `is_ready()` 查询接口，通知 DbMgrServer 状态变化

**重试机制：**
- `retry_interval_ms`：重试间隔（毫秒），默认 3000
- `max_retry_count`：最大重试次数，`0` 表示无限重试
- 重试成功后自动恢复服务，全部就绪标记为 CONNECTED

## 关键流程

### 数据路由

多 DBMgr 实例部署时，Game Server 根据以下规则路由请求：

| 数据类型 | 路由规则 | 说明 |
|----------|----------|------|
| 玩家数据 | `player_id % dbmgr_count` | 按 player_id 取模 |
| 账号数据 | `hash(account_id) % dbmgr_count` | 按 account_id 哈希取模 |
| ID 分配 | 固定路由到 `dbmgr_index = 0` | 全局 ID 分配由首个 DBMgr 负责 |

### 全局 ID 分配

DBMgr 集中式 ID 分配 + 批量预取机制，保证 player_id 全局唯一（纯自增序号 1, 2, 3, ...）。

**核心流程：**
1. DBMgr 启动时从 MongoDB `counters` 集合原子预取一批 ID（默认 64 个）
2. 本地 ID 池分配给 Game Server
3. 剩余低于 30% 时自动异步补充下一批

**MongoDB counters 集合：**
```json
{ "_id": "player_id", "seq": 192 }
```

**原子操作：**
- `findOneAndUpdate({_id: "player_id"}, {$inc: {seq: 64}}, {upsert: true, returnDocument: "after"})`
- 返回更新后的 `seq`，本地分配 `[seq - count + 1 .. seq]`
- 多个 DBMgr 同时请求时 MongoDB 串行处理，返回不重叠的 ID 区间

**消息协议：**
- `MSG_ID_ALLOC_PLAYER_ID_REQ (4205)` — Game → DBMgr 请求分配
- `MSG_ID_ALLOC_PLAYER_ID_RESP (4206)` — DBMgr → Game 返回结果

**错误处理：**

| 场景 | 处理方式 |
|------|----------|
| DBMgr 未连接 | 池空 + DBMgr 不可用 → 同步等待，超时后返回错误 |
| DBMgr 重启 | 重连后主动触发一次预取补充池 |
| MongoDB 不可用 | alloc_player_ids() 返回错误 → 不追加到池，可选重试 |
| ID 间隔 | 重启导致未用完的 ID 段被跳过，对功能无影响 |

## 关键代码路径

### DBMgr 服务端

| 文件 | 职责 |
|------|------|
| `scripts/server/dbmgr/src/main.cpp` | 启动流程，解析配置 |
| `scripts/server/dbmgr/src/dbmgr_server.h/.cpp` | 主服务，集成 ConnectionManager、MongoServer、RedisServer |
| `scripts/server/dbmgr/src/connection_manager.h/.cpp` | 连接状态机，后台重试 |
| `scripts/server/dbmgr/src/mongo_connection.h/.cpp` | MongoDB 连接层 |
| `scripts/server/dbmgr/src/mongo_server.h/.cpp` | MongoDB 业务层（CRUD + ID 分配） |
| `scripts/server/dbmgr/src/redis_connection.h/.cpp` | Redis 连接层 |
| `scripts/server/dbmgr/src/redis_server.h/.cpp` | Redis 缓存操作 |
| `scripts/server/dbmgr/CMakeLists.txt` | 构建配置，链接 libmongoc/hiredis |

### 配置文件

| 文件 | 职责 |
|------|------|
| `config/dbmgr.json` | DBMgr 主配置（server/database/shutdown） |
| `config/mongo/players_index.json` | players 集合索引配置 |
| `config/mongo/accounts_index.json` | accounts 集合索引配置 |

### Proto 与消息定义

| 文件 | 职责 |
|------|------|
| `scripts/common/proto/dbmgr.proto` | DBMgr 通信协议（含 AllocPlayerIdReq/Resp） |
| `scripts/server/common/include/internal_msg_ids.h` | 消息 ID 常量 |

### Game Server 端相关

| 文件 | 职责 |
|------|------|
| `scripts/server/game_server/src/dbmgr_connection_manager.h/.cpp` | Game Server 侧 DBMgr 连接管理 |
| `scripts/server/game_server/src/player_id_pool.h` | 本地 ID 池，批量预取逻辑 |
| `scripts/server/game_server/src/login_stub.h/.cpp` | 角色创建，调用 ID 池分配 |

## 常见陷阱

1. **连接失败处理**：ConnectionManager 状态为 FAILED 时，DbMgrServer 拒绝数据请求返回错误码，而非崩溃。需确保调用方正确处理错误响应。

2. **索引未创建**：索引配置文件缺失或格式错误会导致 `MongoServer::init()` 失败。检查 `config/mongo/` 目录下文件是否存在且 JSON 格式正确。已存在的索引不会被修改，如需变更需手动删除后重启。

3. **ID 耗尽**：极端情况下本地池耗尽且预取未返回时，`acquire()` 会同步阻塞等待。需关注 MongoDB 可用性和网络延迟。

4. **Redis TTL**：Redis 无默认 TTL，必须由调用方显式传入。忘记设置 TTL 会导致缓存永不过期，与 MongoDB 数据不一致。

5. **数据迁移**：从文件存储切换到 MongoDB 时，现有数据不自动迁移，需另行编写迁移脚本。

6. **DBMgr 重启与 ID 间隔**：重启会导致未用完的 ID 段被跳过，这是设计预期行为，对功能无影响，但 ID 序列不连续。

## 扩展指南

### 添加新的数据操作

1. **MongoServer 新增方法**：在 `mongo_server.h/.cpp` 中添加新的 CRUD 方法，使用 `mongoc_collection_*` API
2. **Proto 定义**：在 `scripts/common/proto/dbmgr.proto` 中新增请求/响应消息
3. **消息 ID**：在 `internal_msg_ids.h` 中新增常量
4. **路由注册**：在 `dbmgr_server.cpp` 的 `route_message()` 中添加消息路由
5. **处理函数**：在 `dbmgr_server.h/.cpp` 中实现 `handle_xxx_req()` 方法
6. **如需缓存**：在处理函数中集成 Redis 读穿透/写穿透逻辑

### 添加新集合

1. 在 `config/mongo/` 下创建 `<collection>_index.json` 索引配置
2. 在 `MongoServer` 中添加对应的文档结构和 CRUD 方法
3. 如需路由，确定路由规则（取模或哈希）

### 添加新的全局 ID 类型

1. 在 MongoDB `counters` 集合中新增文档（如 `{_id: "item_id", seq: 0}`）
2. 在 `MongoServer` 中复用 `alloc_player_ids()` 模式实现新方法
3. 新增 Proto 消息和消息 ID
4. 在 Game Server 端实现对应的 ID 池

## 相关 Skill

- [[server-architecture]] — 整体服务器架构设计
- [[player-persistence]] — 玩家数据持久化流程
