# DBMgr Redis/Mongo 全面增强设计

## 概述

对 DBMgr 进行全面增强，包括：
- Redis 连接池（支持连接复用）
- 异步 Redis 操作（基于 libevent）
- Redis Cluster 高可用支持
- MongoDB 副本集读写分离
- 修复已知问题（硬编码 URI、遗留代码清理、统一 Redis 封装）

## 关键决策

| 决策项 | 选择 |
|--------|------|
| Redis 连接模式 | 连接池 + Cluster 支持 |
| Redis 异步框架 | libevent + hiredis async API |
| Redis 高可用 | Cluster 模式（非 Sentinel） |
| MongoDB 连接模式 | 副本集（Replica Set） |
| 读写分离 | secondaryPreferred 默认从节点读 |
| 共享代码位置 | scripts/server/common/ |

## 配置格式

### config/dbmgr.json

```json
{
    "server": {
        "index": 0,
        "ip": "0.0.0.0",
        "port": 5000
    },
    "database": {
        "mongo": {
            "uri": "mongodb://host1:27017,host2:27017,host3:27017/farm?replicaSet=rs0&authSource=admin",
            "read_preference": "secondaryPreferred",
            "max_staleness_seconds": 90,
            "connect_timeout_ms": 5000,
            "socket_timeout_ms": 30000,
            "retry_interval_ms": 3000,
            "max_retry_count": 0
        },
        "redis": {
            "uri": "redis://host1:6379,host2:6379,host3:6379",
            "cluster_mode": true,
            "pool_size": 8,
            "min_idle": 2,
            "max_wait_ms": 3000,
            "connect_timeout_ms": 5000,
            "command_timeout_ms": 1000,
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

## 架构设计

### 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                         DbMgrServer                             │
│  ┌─────────────────┐  ┌──────────────────┐  ┌────────────────┐ │
│  │ ConnectionManager│  │   MongoServer    │  │  RedisServer   │ │
│  │  - 状态管理       │  │  - 玩家数据 CRUD  │  │  - 缓存操作     │ │
│  │  - 后台重试       │  │  - 账号数据 CRUD  │  │  - 异步回调     │ │
│  └────────┬────────┘  └────────┬─────────┘  └───────┬────────┘ │
│           │                    │                     │          │
│  ┌────────┴────────────────────────────────────────────────────┐│
│  │              MongoReplicaSetConnection                     ││
│  │  - 副本集连接管理                                            ││
│  │  - 读写分离控制                                              ││
│  └────────────────────────────────────────────────────────────┘│
│  ┌────────────────────────────────────────────────────────────┐│
│  │              RedisPool / RedisAsyncClient                  ││
│  │  - 连接池管理                                                ││
│  │  - 异步上下文                                                ││
│  │  - Cluster 支持                                             ││
│  └────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
           │                              │
    ┌──────┴──────┐               ┌───────┴───────┐
    │   MongoDB   │               │  Redis Cluster │
    │  Replica Set│               │  (多节点)      │
    │  Primary    │               │               │
    │  Secondary  │               │               │
    └─────────────┘               └───────────────┘
```

### 类职责

#### RedisPool（连接池）

```cpp
struct RedisPoolConfig {
    std::string uri;
    int pool_size = 8;
    int min_idle = 2;
    int max_wait_ms = 3000;
    int connect_timeout_ms = 5000;
    int command_timeout_ms = 1000;
    int retry_interval_ms = 3000;
    int max_retry_count = 0;
    bool cluster_mode = false;
};

class RedisConnectionGuard {
public:
    RedisConnectionGuard(RedisPool* pool, redisContext* ctx);
    ~RedisConnectionGuard();  // 自动归还连接
    redisContext* context();
    bool is_valid() const;
};

class RedisPool {
public:
    RedisPool(const RedisPoolConfig& config);
    bool init();
    void shutdown();
    std::unique_ptr<RedisConnectionGuard> acquire();
    void release(redisContext* ctx);
    size_t available() const;
    size_t total() const;
    bool is_ready() const;
};
```

**关键设计点：**
- RAII 连接管理，避免连接泄漏
- 阻塞等待机制，超时返回 nullptr
- 连接验证，无效连接自动重建
- Cluster slot 映射维护

#### RedisAsyncClient（异步客户端）

```cpp
using RedisCallback = std::function<void(bool success, const std::string& result)>;
using RedisArrayCallback = std::function<void(bool success, const std::vector<std::string>& results)>;

class RedisAsyncClient {
public:
    RedisAsyncClient(struct event_base* base, const RedisPoolConfig& config);
    bool connect();
    void disconnect();

    // KV 操作
    void set(const std::string& key, const std::string& value, int ttl_seconds, RedisCallback cb);
    void get(const std::string& key, RedisCallback cb);
    void del(const std::string& key, RedisCallback cb);

    // Set 操作
    void sadd(const std::string& key, const std::string& member, RedisCallback cb);
    void srem(const std::string& key, const std::string& member, RedisCallback cb);
    void sismember(const std::string& key, const std::string& member, RedisCallback cb);
    void smembers(const std::string& key, RedisArrayCallback cb);
    void scard(const std::string& key, RedisCallback cb);

    // Hash 操作
    void hset(const std::string& key, const std::string& field, const std::string& value, RedisCallback cb);
    void hget(const std::string& key, const std::string& field, RedisCallback cb);
    void hdel(const std::string& key, const std::string& field, RedisCallback cb);
    void hgetall(const std::string& key, RedisArrayCallback cb);

    // 批量操作
    void pipeline(const std::vector<std::string>& commands, RedisArrayCallback cb);
};
```

#### RedisClusterClient（Cluster 客户端）

```cpp
struct ClusterNode {
    std::string host;
    int port;
    std::string node_id;
    bool is_master;
    std::vector<std::pair<uint16_t, uint16_t>> slots;
};

class RedisClusterClient {
public:
    RedisClusterClient(struct event_base* base, const RedisPoolConfig& config);
    bool init();
    void shutdown();

    // 完整的 Redis 操作接口（内部处理 MOVED/ASK 重定向）
    bool set(const std::string& key, const std::string& value, int ttl_seconds);
    std::string get(const std::string& key);
    bool del(const std::string& key);
    bool sadd(const std::string& key, const std::string& member);
    bool srem(const std::string& key, const std::string& member);
    bool sismember(const std::string& key, const std::string& member);
    std::unordered_set<std::string> smembers(const std::string& key);
    size_t scard(const std::string& key);
    bool hset(const std::string& key, const std::string& field, const std::string& value);
    std::string hget(const std::string& key, const std::string& field);
    bool hdel(const std::string& key, const std::string& field);
    std::unordered_map<std::string, std::string> hgetall(const std::string& key);

    std::vector<ClusterNode> get_nodes() const;
    bool is_healthy() const;

private:
    uint16_t key_slot(const std::string& key);  // CRC16 % 16384
    redisContext* get_connection(uint16_t slot);
    bool refresh_slots();  // MOVED 时触发
    redisReply* execute_command(uint16_t slot, const char* fmt, ...);
};
```

**Cluster 命令路由：**
```
命令: SET player:1001 data
    ↓
CRC16("player:1001") % 16384 = 12345
    ↓
slot_map_[12345] -> node_A:6379
    ↓
发送命令到 node_A
    ↓
┌─────────┬──────────┐
│   OK    │  MOVED   │
│   返回   │  重定向   │
└─────────┴────┬─────┘
               ↓
        更新 slot 映射，重新发送
```

#### MongoReplicaSetConnection（副本集连接）

```cpp
struct MongoReplicaSetConfig {
    std::string uri;  // 包含数据库名，如 mongodb://host1:27017,host2:27017/farm?replicaSet=rs0
    int connect_timeout_ms = 5000;
    int socket_timeout_ms = 30000;
    int server_selection_timeout_ms = 30000;
    int retry_interval_ms = 3000;
    int max_retry_count = 0;
    std::string read_preference = "secondaryPreferred";
    int max_staleness_seconds = 90;
};

class MongoReplicaSetConnection {
public:
    MongoReplicaSetConnection(const MongoReplicaSetConfig& config);
    bool connect();
    void disconnect();
    bool is_connected() const;
    mongoc_client_t* client();

    // 副本集状态
    bool is_master() const;
    std::string get_primary() const;
    std::vector<std::string> get_secondaries() const;

    // 读写分离控制
    void read_from_primary();  // 写后读一致性
    void restore_read_preference();
};
```

**读写分离策略：**
```
写操作 (set_all, set_account)
    ↓
Primary 节点

读操作 (get_all, get)
    ↓
Secondary 节点 (secondaryPreferred)

特殊场景: read_from_primary()
    ↓
强制读 Primary（写后读一致性）
```

### 状态枚举

```cpp
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED,
    FAILED_PERMANENT
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

```json
{
    "_id": "ObjectId",
    "player_id": "int64",
    "data": { }
}
```

| 操作 | MongoDB 命令 | 读写分离 |
|------|-------------|---------|
| get_all | find({player_id}) | 从 Secondary 读 |
| get | find({player_id}) → 提取 key | 从 Secondary 读 |
| set_all | updateOne + $set | 写 Primary |
| set | updateOne + $set | 写 Primary |
| del | updateOne + $unset | 写 Primary |

### accounts 集合

```json
{
    "_id": "ObjectId",
    "account_id": "string",
    "roles": [
        { "server_id": "int32", "player_id": "int64", "role_name": "string" }
    ]
}
```

| 操作 | MongoDB 命令 | 读写分离 |
|------|-------------|---------|
| get_account | find({account_id}) | 从 Secondary 读 |
| set_account | updateOne + $push | 写 Primary |

## 启动流程

```
main()
  ├── 解析配置（新格式）
  ├── 创建 ConnectionManager
  │     ├── MongoReplicaSetConnection
  │     │     ├── 解析 URI
  │     │     ├── 连接副本集
  │     │     └── 验证副本集状态
  │     └── RedisPool
  │           ├── 初始化连接池
  │           ├── Cluster 模式：获取 slot 映射
  │           └── 验证连接有效性
  ├── ConnectionManager::init()
  │     ├── 尝试连接 MongoDB → 成功/失败
  │     ├── 尝试连接 Redis → 成功/失败
  │     └── 任一失败 → 启动后台重试线程
  ├── 创建 DbMgrServer
  │     ├── MongoServer（使用新连接）
  │     ├── RedisServer（使用连接池或 Cluster）
  │     └── RedisAsyncClient（异步操作）
  └── DbMgrServer::start()
        ├── 检查 ConnectionManager::is_ready()
        │     ├── true → 正常启动
        │     └── false → 拒绝数据请求
        └── 进入事件循环
```

## 关闭流程

```
信号触发（SIGINT/SIGTERM）
  ├── DbMgrServer::stop()
  │     ├── 停止接受新连接
  │     ├── 等待进行中的请求完成
  │     └── 通知 ConnectionManager 关闭
  ├── ConnectionManager::shutdown()
  │     ├── 停止重试线程
  │     ├── MongoReplicaSetConnection::disconnect()
  │     │     └── mongoc_client_destroy()
  │     └── RedisPool::shutdown()
  │           ├── 关闭所有空闲连接
  │           ├── 等待活跃连接归还
  │           └── 关闭 Cluster 刷新线程
  └── 进程退出
```

## 错误处理

| 场景 | 处理方式 |
|------|---------|
| MongoDB 主节点宕机 | 副本集自动选举新主，连接自动恢复 |
| Redis Cluster 节点宕机 | MOVED 重定向到新主节点 |
| 连接池耗尽 | 阻塞等待，超时返回错误 |
| 异步操作超时 | 回调返回失败，记录日志 |

## 监控指标

```cpp
struct PoolStats {
    size_t total_connections;
    size_t idle_connections;
    size_t active_connections;
    size_t waiting_requests;
    size_t connection_errors;
};

struct ClusterStats {
    size_t total_nodes;
    size_t healthy_nodes;
    size_t slot_coverage;
    std::string current_master;
};
```

## 文件变更清单

### 新增文件

```
scripts/server/common/include/
├── redis_pool.h
├── redis_async.h
├── redis_cluster.h
├── mongo_replica_set.h
└── db_types.h

scripts/server/common/src/
├── redis_pool.cpp
├── redis_async.cpp
├── redis_cluster.cpp
└── mongo_replica_set.cpp
```

### 修改文件

```
scripts/server/dbmgr/src/
├── connection_manager.h/cpp
├── mongo_server.h/cpp
├── redis_server.h/cpp
├── dbmgr_server.h/cpp
└── main.cpp

scripts/server/dbmgr/CMakeLists.txt
scripts/server/common/CMakeLists.txt
config/dbmgr.json
```

### 删除文件

```
scripts/server/dbmgr/src/
├── data_manager.h
├── redis_connection.h/cpp
└── mongo_connection.h/cpp
```

## 接口定义

### MongoServer

```cpp
class MongoServer {
public:
    MongoServer(MongoReplicaSetConnection& conn, const std::string& index_config_dir);
    bool init();

    DataResult get_all(uint64_t player_id, std::vector<uint8_t>& value);
    DataResult get(uint64_t player_id, const std::string& key, std::vector<uint8_t>& value);
    DataResult set_all(uint64_t player_id, const std::vector<uint8_t>& value);
    DataResult set(uint64_t player_id, const std::string& key, const std::vector<uint8_t>& value);
    DataResult del(uint64_t player_id, const std::string& key);

    AccountResult get_account(const std::string& account_id, std::vector<AccountRole>& roles);
    AccountResult set_account(const std::string& account_id, const AccountRole& new_role);

    bool init_counter();
    int64_t alloc_player_ids(uint32_t count);

private:
    MongoReplicaSetConnection& conn_;
    std::string index_config_dir_;
};
```

### RedisServer

```cpp
class RedisServer {
public:
    // 普通模式：使用连接池
    explicit RedisServer(RedisPool& pool);
    
    // Cluster 模式：使用 Cluster 客户端
    explicit RedisServer(RedisClusterClient& cluster);

    CacheResult get(const std::string& key, std::vector<uint8_t>& value);
    CacheResult set(const std::string& key, const std::vector<uint8_t>& value, int ttl_seconds);
    CacheResult del(const std::string& key);

    // Set 操作
    bool sadd(const std::string& key, const std::string& member);
    bool srem(const std::string& key, const std::string& member);
    bool sismember(const std::string& key, const std::string& member);
    std::unordered_set<std::string> smembers(const std::string& key);
    size_t scard(const std::string& key);

    // Hash 操作
    bool hset(const std::string& key, const std::string& field, const std::string& value);
    std::string hget(const std::string& key, const std::string& field);
    bool hdel(const std::string& key, const std::string& field);
    std::unordered_map<std::string, std::string> hgetall(const std::string& key);

private:
    RedisPool* pool_ = nullptr;           // 普通模式使用
    RedisClusterClient* cluster_ = nullptr;  // Cluster 模式使用
};
```

**说明：** 根据配置中的 `cluster_mode` 字段决定使用哪个构造函数。`DbMgrServer` 在初始化时根据配置创建对应的 `RedisServer` 实例。

### ConnectionManager

```cpp
class ConnectionManager {
public:
    ConnectionManager(const MongoReplicaSetConfig& mongo_config, 
                      const RedisPoolConfig& redis_config);
    bool init();
    void shutdown();
    bool is_ready() const;
    ConnectionState state() const;

    MongoReplicaSetConnection& mongo_connection();
    RedisPool& redis_pool();

    void read_from_primary_after_write();
    void restore_read_preference();

    void set_on_ready_callback(std::function<void()> callback);
};
```
