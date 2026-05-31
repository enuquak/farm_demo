# DBMgr Redis + MongoDB 集成设计

## 概述

将 dbmgr 的数据存储从文件系统替换为 MongoDB，同时引入 Redis 作为纯缓存层。MongoDB 存储玩家数据和账号数据，Redis 用于加速热数据访问。

## 关键决策

| 决策项 | 选择 |
|--------|------|
| C++ 标准 | C++17 |
| MongoDB 客户端 | libmongoc（C 驱动）+ 自写薄封装 |
| Redis 客户端 | hiredis（C 客户端）+ 自写薄封装 |
| 配置位置 | 仅 dbmgr.json |
| 文件存储 | 完全替换，删除文件存储逻辑 |
| 连接失败策略 | 进程保持、不对外服务、后台重试、自动恢复 |
| Redis TTL | 无默认值，由调用方传入 |
| MongoDB 集合 | `players` 和 `accounts` |
| 数据库名 | 包含在 MongoDB URI 中 |
| 索引配置 | 独立文件 `config/mongo/<collection>_index.json` |

## 配置格式

```json
{
    "server": {
        "index": 0,
        "ip": "0.0.0.0",
        "port": 5000
    },
    "database": {
        "mongo": {
            "uri": "mongodb://localhost:27017/farm",
            "retry_interval_ms": 3000,
            "max_retry_count": 0
        },
        "redis": {
            "uri": "redis://localhost:6379",
            "retry_interval_ms": 3000,
            "max_retry_count": 0
        }
    },
    "shutdown": {
        "timeout_ms": 60000
    },
    "pid_file": "./runtimeData/dbmgr.pid",
    "logging": {
        "dir": "./runtimeData/logs/server",
        "level": "debug",
        "max_file_size_mb": 20,
        "max_files": 7
    }
}
```

**说明：**
- `uri`：连接字符串，MongoDB 包含数据库名
- `retry_interval_ms`：重试间隔（毫秒）
- `max_retry_count`：最大重试次数，`0` 表示无限重试
- 删除了原有的 `server.data_dir`（不再需要文件存储）

## MongoDB 索引配置

每个集合的索引配置存放在 `config/mongo/` 目录下，文件名为 `<collection>_index.json`。

**目录结构：**
```
config/mongo/
├── players_index.json
└── accounts_index.json
```

**配置格式：**
```json
[
    { "key": "player_id", "unique": true },
    { "key": "data.level", "unique": false, "sparse": true }
]
```

**支持的索引选项：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `key` | string | 索引字段，支持嵌套（如 `data.level`） |
| `unique` | bool | 是否唯一索引 |
| `sparse` | bool | 是否稀疏索引（跳过不包含该字段的文档） |
| `expireAfterSeconds` | int | TTL 索引，文档过期时间（秒） |

**示例 - `config/mongo/players_index.json`：**
```json
[
    { "key": "player_id", "unique": true },
    { "key": "data.level", "unique": false },
    { "key": "data.last_login", "unique": false, "expireAfterSeconds": 2592000 }
]
```

**示例 - `config/mongo/accounts_index.json`：**
```json
[
    { "key": "account_id", "unique": true }
]
```

**索引创建时机：** `MongoServer::init()` 时读取配置文件，检查索引是否存在，不存在则自动创建。已存在的索引不会被修改或删除。

## 架构设计

### 整体架构

```
┌─────────────────────────────────────────────────────┐
│                     DbMgrServer                      │
│  ┌───────────────┐  ┌──────────────┐  ┌───────────┐ │
│  │ ConnectionMgr │  │ MongoServer  │  │RedisServer│ │
│  │  - 状态管理    │  │  - 玩家数据   │  │ - 缓存操作│ │
│  │  - 后台重试    │  │  - 账号数据   │  │           │ │
│  │  - 就绪通知    │  │              │  │           │ │
│  └───────┬───────┘  └──────┬───────┘  └─────┬─────┘ │
│          │                 │                │       │
│  ┌───────┴───────┐  ┌──────┴───────┐  ┌─────┴─────┐ │
│  │ MongoConnection│  │(使用连接)    │  │(使用连接) │ │
│  │ RedisConnection│  │              │  │           │ │
│  └───────┬───────┘  └──────────────┘  └───────────┘ │
│          │                                           │
└──────────┼───────────────────────────────────────────┘
           │
    ┌──────┴──────┐
    │   MongoDB   │    ┌───────────┐
    │ (主存储)     │    │   Redis   │
    │ players     │    │ (纯缓存)   │
    │ accounts    │    │           │
    └─────────────┘    └───────────┘
```

### 类职责

**ConnectionManager（连接管理器）：**
- 持有 `MongoConnection` 和 `RedisConnection`
- 管理连接状态机：DISCONNECTED → CONNECTING → CONNECTED / FAILED
- 后台重试线程（当状态为 FAILED 时）
- 提供 `is_ready()` 查询接口
- 通知 DbMgrServer 状态变化

**MongoConnection（MongoDB 连接层）：**
- 薄封装 libmongoc
- `connect(uri)` / `disconnect()` / `is_connected()`
- 提供 `client()` 给 MongoServer 使用

**MongoServer（MongoDB 业务层）：**
- 依赖 MongoConnection
- `init(index_config_dir)`：读取索引配置，自动创建缺失的索引
- 实现玩家数据和账号数据的 CRUD 操作

**RedisConnection（Redis 连接层）：**
- 薄封装 hiredis
- `connect(uri)` / `disconnect()` / `is_connected()`
- 提供 `context()` 给 RedisServer 使用

**RedisServer（Redis 业务层）：**
- 依赖 RedisConnection
- 实现通用缓存操作（get/set/del）

### 状态枚举

```cpp
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED,           // 重试中
    FAILED_PERMANENT  // 超过最大重试次数
};

enum class CacheResult : int32_t {
    SUCCESS = 0,
    NOT_FOUND = 1,
    CONNECTION_ERROR = 2,
    TIMEOUT = 3,
};
```

## 数据操作映射

### players 集合

**文档结构：**
```json
{
    "_id": "ObjectId",
    "player_id": "int64",
    "data": { }
}
```

**索引：** `player_id` 唯一索引

| DataManager 方法 | MongoDB 操作 |
|-----------------|-------------|
| `get_all(player_id)` | `find({player_id})` → 返回 `data` 字段 |
| `get(player_id, key)` | `find({player_id})` → 从 `data` 中取指定 key |
| `set_all(player_id, value)` | `replaceOne({player_id}, doc, {upsert: true})` |
| `set(player_id, key, value)` | `updateOne({player_id}, {$set: {data.key: value}}, {upsert: true})` |
| `del(player_id, key)` | `updateOne({player_id}, {$unset: {data.key: ""}})` |

### accounts 集合

**文档结构：**
```json
{
    "_id": "ObjectId",
    "account_id": "string",
    "roles": [
        { "server_id": "int32", "player_id": "int64", "role_name": "string" }
    ]
}
```

**索引：** `account_id` 唯一索引

| DataManager 方法 | MongoDB 操作 |
|-----------------|-------------|
| `get_account(account_id)` | `find({account_id})` → 返回 `roles` 数组 |
| `set_account(account_id, role)` | `updateOne({account_id}, {$push: {roles: role}}, {upsert: true})` |

集合和索引在 `MongoServer` 初始化时自动创建（如果不存在）。

**数据迁移：** 现有文件存储数据不自动迁移。如需迁移，需另行编写迁移脚本。

## 启动流程

```
main()
  ├── 解析配置
  ├── 创建 ConnectionManager
  ├── ConnectionManager::init()
  │     ├── 尝试连接 MongoDB → 成功/失败
  │     ├── 尝试连接 Redis → 成功/失败
  │     └── 任一失败 → 标记状态为 FAILED，启动后台重试线程
  ├── 创建 DbMgrServer（传入 ConnectionManager）
  └── DbMgrServer::start()
        ├── 检查 ConnectionManager::is_ready()
        │     ├── true → MongoServer::init()（创建索引）→ 正常启动监听，对外服务
        │     └── false → 仅启动监听，拒绝数据请求（返回错误码），记录日志
        └── 进入事件循环
```

### 后台重试机制

```
重试线程（当状态为 FAILED 时）：
  loop:
    ├── sleep(retry_interval_ms)
    ├── 尝试连接失败的组件
    ├── 成功 → 更新状态，检查是否全部就绪
    │     └── 全部就绪 → 标记 CONNECTED，通知 DbMgrServer 恢复服务
    ├── 失败 → 记录日志，继续重试
    └── 检查 max_retry_count（0 = 无限）
          └── 超过上限 → 标记状态为 FAILED_PERMANENT，停止重试
```

## 关闭流程

```
信号触发（SIGINT/SIGTERM）
  ├── DbMgrServer::stop()
  │     ├── 停止接受新连接
  │     ├── 等待进行中的请求完成（或超时）
  │     └── 通知 ConnectionManager 关闭
  ├── ConnectionManager::shutdown()
  │     ├── 停止重试线程
  │     ├── MongoConnection::disconnect() → mongoc_client_destroy + mongoc_cleanup
  │     └── RedisConnection::disconnect() → redisFree
  └── 进程退出
```

## 数据流

```
读请求 → RedisServer.get() → 命中 → 返回
                              ↓ 未命中
                     MongoServer.get() → 返回 + 回填 Redis

写请求 → MongoServer.set() → 成功 → 删除 Redis key → 返回
```

## 接口定义

### MongoConnection

```cpp
class MongoConnection {
    bool connect(const std::string& uri);
    void disconnect();
    bool is_connected() const;
    mongoc_client_t* client();
};
```

### MongoServer

```cpp
class MongoServer {
public:
    MongoServer(MongoConnection& conn, const std::string& index_config_dir);

    // 初始化：读取索引配置，创建缺失索引
    bool init();

    DataResult get_all(uint64_t player_id, std::vector<uint8_t>& value);
    DataResult get(uint64_t player_id, const std::string& key, std::vector<uint8_t>& value);
    DataResult set_all(uint64_t player_id, const std::vector<uint8_t>& value);
    DataResult set(uint64_t player_id, const std::string& key, const std::vector<uint8_t>& value);
    DataResult del(uint64_t player_id, const std::string& key);
    AccountResult get_account(const std::string& account_id, std::vector<AccountRole>& roles);
    AccountResult set_account(const std::string& account_id, const AccountRole& new_role);
};
```

### RedisConnection

```cpp
class RedisConnection {
    bool connect(const std::string& uri);
    void disconnect();
    bool is_connected() const;
    redisContext* context();
};
```

### RedisServer

```cpp
class RedisServer {
public:
    RedisServer(RedisConnection& conn);

    CacheResult get(const std::string& key, std::vector<uint8_t>& value);
    CacheResult set(const std::string& key, const std::vector<uint8_t>& value, int ttl_seconds);
    CacheResult del(const std::string& key);
};
```

## 文件变更清单

### 新增文件

```
scripts/server/dbmgr/src/
├── connection_manager.h / .cpp
├── mongo_connection.h / .cpp
├── mongo_server.h / .cpp
├── redis_connection.h / .cpp
└── redis_server.h / .cpp
```

### 新增配置文件

```
config/mongo/
├── players_index.json             // players 集合索引配置
└── accounts_index.json            // accounts 集合索引配置
```

### 修改文件

```
scripts/server/dbmgr/src/
├── dbmgr_server.h / .cpp          // 集成 ConnectionManager、MongoServer、RedisServer
└── main.cpp                       // 启动流程调整

scripts/server/dbmgr/CMakeLists.txt  // 添加新源文件、链接 libmongoc/hiredis

config/dbmgr.json                   // 配置格式变更
```

### 删除文件

```
scripts/server/dbmgr/src/data_manager.h / .cpp  // 被 MongoServer 替代
```

## DbMgrServer 构造函数变更

```cpp
// 之前
DbMgrServer(uint32_t index, const std::string& ip, uint16_t port, const std::string& data_dir);

// 之后
DbMgrServer(uint32_t index, const std::string& ip, uint16_t port,
            ConnectionManager& conn_mgr);
```

- 移除 `data_dir` 参数
- 新增 `ConnectionManager&` 参数
- 内部持有 `MongoServer` 和 `RedisServer`，通过 `ConnectionManager` 获取的连接构造
- `MongoServer` 构造时传入 `config/mongo/` 路径，用于读取索引配置
