# DBMgr Redis + MongoDB 集成实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 dbmgr 的数据存储从文件系统替换为 MongoDB，引入 Redis 作为纯缓存层

**Architecture:** ConnectionManager 统一管理 MongoDB/Redis 连接状态和重试；MongoServer/RedisServer 作为业务层分别处理数据操作和缓存操作；DbMgrServer 通过 ConnectionManager 协调各组件

**Tech Stack:** C++17, libmongoc (MongoDB C driver), hiredis (Redis C client), nlohmann/json, libevent, protobuf

---

## Task 1: 安装依赖库

**目标:** 安装 libmongoc 和 hiredis 开发库

- [ ] **Step 1: 下载并编译 libmongoc**

```bash
# Windows (使用 vcpkg 或手动编译)
# 方案1: vcpkg
vcpkg install mongo-c-driver:x64-windows

# 方案2: 手动编译
# 下载 https://github.com/mongodb/mongo-c-driver/releases
# 解压后使用 CMake 编译
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=C:/mongoc_install
cmake --build . --config Release
cmake --install .
```

- [ ] **Step 2: 下载并编译 hiredis**

```bash
# Windows
# 方案1: vcpkg
vcpkg install hiredis:x64-windows

# 方案2: 手动编译
# 下载 https://github.com/redis/hiredis
git clone https://github.com/redis/hiredis.git
cd hiredis
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=C:/hiredis_install
cmake --build . --config Release
cmake --install .
```

- [ ] **Step 3: 验证安装**

确认以下目录存在：
- `C:/mongoc_install/include/libmongoc-1.0/` (或 vcpkg 对应路径)
- `C:/hiredis/include/` (或 vcpkg 对应路径)

---

## Task 2: 创建索引配置文件

**目标:** 创建 MongoDB 集合索引配置文件

**Files:**
- Create: `config/mongo/players_index.json`
- Create: `config/mongo/accounts_index.json`

- [ ] **Step 1: 创建 players_index.json**

```json
[
    { "key": "player_id", "unique": true },
    { "key": "data.level", "unique": false }
]
```

- [ ] **Step 2: 创建 accounts_index.json**

```json
[
    { "key": "account_id", "unique": true }
]
```

- [ ] **Step 3: 提交配置文件**

```bash
git add config/mongo/
git commit -m "config: add MongoDB index configuration files"
```

---

## Task 3: 更新 dbmgr.json 配置格式

**目标:** 更新 dbmgr.json 配置文件格式，移除旧配置，添加 MongoDB/Redis 配置

**Files:**
- Modify: `config/dbmgr.json`

- [ ] **Step 1: 更新 dbmgr.json**

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

- [ ] **Step 2: 提交配置变更**

```bash
git add config/dbmgr.json
git commit -m "config: update dbmgr.json for MongoDB/Redis integration"
```

---

## Task 4: 实现 MongoConnection

**目标:** 实现 MongoDB 连接层，薄封装 libmongoc

**Files:**
- Create: `scripts/server/dbmgr/src/mongo_connection.h`
- Create: `scripts/server/dbmgr/src/mongo_connection.cpp`

- [ ] **Step 1: 创建 mongo_connection.h**

```cpp
#pragma once

#include <string>
#include <mongoc/mongoc.h>

namespace farm {

class MongoConnection {
public:
    MongoConnection();
    ~MongoConnection();

    // 禁止拷贝
    MongoConnection(const MongoConnection&) = delete;
    MongoConnection& operator=(const MongoConnection&) = delete;

    // 连接 MongoDB
    bool connect(const std::string& uri);

    // 断开连接
    void disconnect();

    // 是否已连接
    bool is_connected() const;

    // 获取底层 client（供 MongoServer 使用）
    mongoc_client_t* client();

private:
    mongoc_client_t* client_ = nullptr;
    bool connected_ = false;
};

}  // namespace farm
```

- [ ] **Step 2: 创建 mongo_connection.cpp**

```cpp
#include "mongo_connection.h"
#include "log_macros.h"

namespace farm {

MongoConnection::MongoConnection() {
    // 初始化 libmongoc（只需调用一次，但多次调用是安全的）
    mongoc_init();
}

MongoConnection::~MongoConnection() {
    disconnect();
}

bool MongoConnection::connect(const std::string& uri) {
    if (connected_) {
        SPDLOG_WARN("[MongoConnection]Already connected, disconnecting first");
        disconnect();
    }

    client_ = mongoc_client_new(uri.c_str());
    if (!client_) {
        SPDLOG_ERROR("[MongoConnection]Failed to create client from uri: {}", uri);
        return false;
    }

    // 测试连接：执行 ping 命令
    bson_t ping_cmd;
    bson_init(&ping_cmd);
    BSON_APPEND_INT32(&ping_cmd, "ping", 1);

    bson_t reply;
    bson_error_t error;
    bool ok = mongoc_client_command_simple(client_, "admin", &ping_cmd, nullptr, &reply, &error);
    bson_destroy(&ping_cmd);
    bson_destroy(&reply);

    if (!ok) {
        SPDLOG_ERROR("[MongoConnection]Connection test failed: {}", error.message);
        mongoc_client_destroy(client_);
        client_ = nullptr;
        return false;
    }

    connected_ = true;
    SPDLOG_INFO("[MongoConnection]Connected to MongoDB: {}", uri);
    return true;
}

void MongoConnection::disconnect() {
    if (client_) {
        mongoc_client_destroy(client_);
        client_ = nullptr;
    }
    connected_ = false;
    SPDLOG_INFO("[MongoConnection]Disconnected from MongoDB");
}

bool MongoConnection::is_connected() const {
    return connected_;
}

mongoc_client_t* MongoConnection::client() {
    return client_;
}

}  // namespace farm
```

- [ ] **Step 3: 提交 MongoConnection**

```bash
git add scripts/server/dbmgr/src/mongo_connection.h scripts/server/dbmgr/src/mongo_connection.cpp
git commit -m "feat: implement MongoConnection wrapper"
```

---

## Task 5: 实现 RedisConnection

**目标:** 实现 Redis 连接层，薄封装 hiredis

**Files:**
- Create: `scripts/server/dbmgr/src/redis_connection.h`
- Create: `scripts/server/dbmgr/src/redis_connection.cpp`

- [ ] **Step 1: 创建 redis_connection.h**

```cpp
#pragma once

#include <string>
#include <hiredis/hiredis.h>

namespace farm {

class RedisConnection {
public:
    RedisConnection();
    ~RedisConnection();

    // 禁止拷贝
    RedisConnection(const RedisConnection&) = delete;
    RedisConnection& operator=(const RedisConnection&) = delete;

    // 连接 Redis（uri 格式: redis://host:port 或 tcp://host:port）
    bool connect(const std::string& uri);

    // 断开连接
    void disconnect();

    // 是否已连接
    bool is_connected() const;

    // 获取底层 context（供 RedisServer 使用）
    redisContext* context();

private:
    redisContext* context_ = nullptr;
    bool connected_ = false;
};

}  // namespace farm
```

- [ ] **Step 2: 创建 redis_connection.cpp**

```cpp
#include "redis_connection.h"
#include "log_macros.h"

#include <cstring>

namespace farm {

RedisConnection::RedisConnection() {
}

RedisConnection::~RedisConnection() {
    disconnect();
}

bool RedisConnection::connect(const std::string& uri) {
    if (connected_) {
        SPDLOG_WARN("[RedisConnection]Already connected, disconnecting first");
        disconnect();
    }

    // 解析 URI（支持 redis://host:port 和 tcp://host:port）
    std::string host = "127.0.0.1";
    int port = 6379;

    std::string uri_str = uri;
    // 移除协议前缀
    if (uri_str.find("redis://") == 0) {
        uri_str = uri_str.substr(8);
    } else if (uri_str.find("tcp://") == 0) {
        uri_str = uri_str.substr(6);
    }

    // 解析 host:port
    auto colon_pos = uri_str.find(':');
    if (colon_pos != std::string::npos) {
        host = uri_str.substr(0, colon_pos);
        port = std::stoi(uri_str.substr(colon_pos + 1));
    } else {
        host = uri_str;
    }

    // 连接
    context_ = redisConnect(host.c_str(), port);
    if (!context_) {
        SPDLOG_ERROR("[RedisConnection]Failed to allocate redis context");
        return false;
    }
    if (context_->err) {
        SPDLOG_ERROR("[RedisConnection]Connection failed: {}", context_->errstr);
        redisFree(context_);
        context_ = nullptr;
        return false;
    }

    // 测试连接
    redisReply* reply = (redisReply*)redisCommand(context_, "PING");
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        SPDLOG_ERROR("[RedisConnection]PING test failed");
        if (reply) freeReplyObject(reply);
        redisFree(context_);
        context_ = nullptr;
        return false;
    }
    freeReplyObject(reply);

    connected_ = true;
    SPDLOG_INFO("[RedisConnection]Connected to Redis: {}:{}", host, port);
    return true;
}

void RedisConnection::disconnect() {
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
    connected_ = false;
    SPDLOG_INFO("[RedisConnection]Disconnected from Redis");
}

bool RedisConnection::is_connected() const {
    return connected_;
}

redisContext* RedisConnection::context() {
    return context_;
}

}  // namespace farm
```

- [ ] **Step 3: 提交 RedisConnection**

```bash
git add scripts/server/dbmgr/src/redis_connection.h scripts/server/dbmgr/src/redis_connection.cpp
git commit -m "feat: implement RedisConnection wrapper"
```

---

## Task 6: 实现 ConnectionManager

**目标:** 实现连接管理器，统一管理 MongoDB/Redis 连接状态和后台重试

**Files:**
- Create: `scripts/server/dbmgr/src/connection_manager.h`
- Create: `scripts/server/dbmgr/src/connection_manager.cpp`

- [ ] **Step 1: 创建 connection_manager.h**

```cpp
#pragma once

#include "mongo_connection.h"
#include "redis_connection.h"

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

namespace farm {

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED,           // 重试中
    FAILED_PERMANENT  // 超过最大重试次数
};

// 连接配置
struct MongoConfig {
    std::string uri;
    int retry_interval_ms = 3000;
    int max_retry_count = 0;  // 0 = 无限重试
};

struct RedisConfig {
    std::string uri;
    int retry_interval_ms = 3000;
    int max_retry_count = 0;  // 0 = 无限重试
};

class ConnectionManager {
public:
    ConnectionManager(const MongoConfig& mongo_config, const RedisConfig& redis_config);
    ~ConnectionManager();

    // 禁止拷贝
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // 初始化：尝试连接，失败则启动后台重试
    bool init();

    // 关闭：停止重试线程，断开连接
    void shutdown();

    // 是否所有连接就绪
    bool is_ready() const;

    // 获取连接状态
    ConnectionState state() const;

    // 获取连接对象
    MongoConnection& mongo_connection();
    RedisConnection& redis_connection();

    // 状态变化回调（DbMgrServer 注册）
    void set_on_ready_callback(std::function<void()> callback);

private:
    // 尝试连接 MongoDB
    bool try_connect_mongo();

    // 尝试连接 Redis
    bool try_connect_redis();

    // 后台重试线程函数
    void retry_thread_func();

    // 更新状态
    void update_state();

    MongoConfig mongo_config_;
    RedisConfig redis_config_;

    MongoConnection mongo_conn_;
    RedisConnection redis_conn_;

    std::atomic<ConnectionState> state_{ConnectionState::DISCONNECTED};
    std::atomic<bool> running_{false};
    std::thread retry_thread_;
    std::mutex mutex_;

    // 回调
    std::function<void()> on_ready_callback_;

    // 重试计数
    int mongo_retry_count_ = 0;
    int redis_retry_count_ = 0;
};

}  // namespace farm
```

- [ ] **Step 2: 创建 connection_manager.cpp**

```cpp
#include "connection_manager.h"
#include "log_macros.h"

#include <chrono>

namespace farm {

ConnectionManager::ConnectionManager(const MongoConfig& mongo_config, const RedisConfig& redis_config)
    : mongo_config_(mongo_config)
    , redis_config_(redis_config) {
}

ConnectionManager::~ConnectionManager() {
    shutdown();
}

bool ConnectionManager::init() {
    SPDLOG_INFO("[ConnectionManager]Initializing connections...");

    state_ = ConnectionState::CONNECTING;

    bool mongo_ok = try_connect_mongo();
    bool redis_ok = try_connect_redis();

    if (mongo_ok && redis_ok) {
        state_ = ConnectionState::CONNECTED;
        SPDLOG_INFO("[ConnectionManager]All connections established");
        if (on_ready_callback_) {
            on_ready_callback_();
        }
        return true;
    }

    // 部分或全部连接失败，启动后台重试
    state_ = ConnectionState::FAILED;
    running_ = true;
    retry_thread_ = std::thread(&ConnectionManager::retry_thread_func, this);

    SPDLOG_WARN("[ConnectionManager]Some connections failed, retry thread started");
    return false;
}

void ConnectionManager::shutdown() {
    running_ = false;

    if (retry_thread_.joinable()) {
        retry_thread_.join();
    }

    mongo_conn_.disconnect();
    redis_conn_.disconnect();

    state_ = ConnectionState::DISCONNECTED;
    SPDLOG_INFO("[ConnectionManager]Shutdown complete");
}

bool ConnectionManager::is_ready() const {
    return state_ == ConnectionState::CONNECTED;
}

ConnectionState ConnectionManager::state() const {
    return state_;
}

MongoConnection& ConnectionManager::mongo_connection() {
    return mongo_conn_;
}

RedisConnection& ConnectionManager::redis_connection() {
    return redis_conn_;
}

void ConnectionManager::set_on_ready_callback(std::function<void()> callback) {
    on_ready_callback_ = callback;
}

bool ConnectionManager::try_connect_mongo() {
    if (mongo_conn_.is_connected()) {
        return true;
    }

    SPDLOG_INFO("[ConnectionManager]Connecting to MongoDB...");
    if (mongo_conn_.connect(mongo_config_.uri)) {
        mongo_retry_count_ = 0;
        return true;
    }

    mongo_retry_count_++;
    SPDLOG_ERROR("[ConnectionManager]MongoDB connection failed (attempt {})", mongo_retry_count_);
    return false;
}

bool ConnectionManager::try_connect_redis() {
    if (redis_conn_.is_connected()) {
        return true;
    }

    SPDLOG_INFO("[ConnectionManager]Connecting to Redis...");
    if (redis_conn_.connect(redis_config_.uri)) {
        redis_retry_count_ = 0;
        return true;
    }

    redis_retry_count_++;
    SPDLOG_ERROR("[ConnectionManager]Redis connection failed (attempt {})", redis_retry_count_);
    return false;
}

void ConnectionManager::retry_thread_func() {
    SPDLOG_INFO("[ConnectionManager]Retry thread started");

    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(mongo_config_.retry_interval_ms));

        if (!running_) break;

        // 检查是否超过最大重试次数
        if (mongo_config_.max_retry_count > 0 && mongo_retry_count_ >= mongo_config_.max_retry_count) {
            SPDLOG_ERROR("[ConnectionManager]MongoDB max retry count reached");
            state_ = ConnectionState::FAILED_PERMANENT;
            break;
        }
        if (redis_config_.max_retry_count > 0 && redis_retry_count_ >= redis_config_.max_retry_count) {
            SPDLOG_ERROR("[ConnectionManager]Redis max retry count reached");
            state_ = ConnectionState::FAILED_PERMANENT;
            break;
        }

        // 尝试重连
        bool mongo_ok = try_connect_mongo();
        bool redis_ok = try_connect_redis();

        if (mongo_ok && redis_ok) {
            state_ = ConnectionState::CONNECTED;
            SPDLOG_INFO("[ConnectionManager]All connections recovered");
            if (on_ready_callback_) {
                on_ready_callback_();
            }
            break;
        }
    }

    SPDLOG_INFO("[ConnectionManager]Retry thread exiting");
}

void ConnectionManager::update_state() {
    if (mongo_conn_.is_connected() && redis_conn_.is_connected()) {
        state_ = ConnectionState::CONNECTED;
    } else if (state_ == ConnectionState::CONNECTED) {
        state_ = ConnectionState::FAILED;
    }
}

}  // namespace farm
```

- [ ] **Step 3: 提交 ConnectionManager**

```bash
git add scripts/server/dbmgr/src/connection_manager.h scripts/server/dbmgr/src/connection_manager.cpp
git commit -m "feat: implement ConnectionManager with retry logic"
```

---

## Task 7: 实现 MongoServer

**目标:** 实现 MongoDB 业务层，包括索引自动创建和数据操作

**Files:**
- Create: `scripts/server/dbmgr/src/mongo_server.h`
- Create: `scripts/server/dbmgr/src/mongo_server.cpp`

- [ ] **Step 1: 创建 mongo_server.h**

```cpp
#pragma once

#include "mongo_connection.h"
#include "data_manager.h"  // 复用 DataResult、AccountResult、AccountRole 定义

#include <string>
#include <vector>
#include <cstdint>

namespace farm {

class MongoServer {
public:
    MongoServer(MongoConnection& conn, const std::string& index_config_dir);

    // 初始化：读取索引配置，创建缺失索引
    bool init();

    // 玩家数据操作
    DataResult get_all(uint64_t player_id, std::vector<uint8_t>& value);
    DataResult get(uint64_t player_id, const std::string& key, std::vector<uint8_t>& value);
    DataResult set_all(uint64_t player_id, const std::vector<uint8_t>& value);
    DataResult set(uint64_t player_id, const std::string& key, const std::vector<uint8_t>& value);
    DataResult del(uint64_t player_id, const std::string& key);

    // 账号数据操作
    AccountResult get_account(const std::string& account_id, std::vector<AccountRole>& roles);
    AccountResult set_account(const std::string& account_id, const AccountRole& new_role);

private:
    // 创建集合索引
    bool create_indexes(const std::string& collection_name);

    // 读取索引配置文件
    bool read_index_config(const std::string& collection_name, std::vector<bson_t*>& indexes);

    // 获取集合
    mongoc_collection_t* get_collection(const std::string& name);

    MongoConnection& conn_;
    std::string index_config_dir_;
};

}  // namespace farm
```

- [ ] **Step 2: 创建 mongo_server.cpp**

```cpp
#include "mongo_server.h"
#include "log_macros.h"

#include <mongoc/mongoc.h>
#include <nlohmann/json.hpp>
#include <fstream>

namespace farm {

MongoServer::MongoServer(MongoConnection& conn, const std::string& index_config_dir)
    : conn_(conn)
    , index_config_dir_(index_config_dir) {
}

bool MongoServer::init() {
    if (!conn_.is_connected()) {
        SPDLOG_ERROR("[MongoServer]MongoDB not connected");
        return false;
    }

    // 创建 players 集合索引
    if (!create_indexes("players")) {
        SPDLOG_ERROR("[MongoServer]Failed to create players indexes");
        return false;
    }

    // 创建 accounts 集合索引
    if (!create_indexes("accounts")) {
        SPDLOG_ERROR("[MongoServer]Failed to create accounts indexes");
        return false;
    }

    SPDLOG_INFO("[MongoServer]Initialization complete");
    return true;
}

bool MongoServer::create_indexes(const std::string& collection_name) {
    std::vector<bson_t*> indexes;
    if (!read_index_config(collection_name, indexes)) {
        SPDLOG_WARN("[MongoServer]No index config for collection: {}, skipping", collection_name);
        return true;  // 没有配置文件不算失败
    }

    mongoc_collection_t* collection = get_collection(collection_name);
    if (!collection) {
        SPDLOG_ERROR("[MongoServer]Failed to get collection: {}", collection_name);
        for (auto* idx : indexes) bson_destroy(idx);
        return false;
    }

    // 创建索引
    bson_error_t error;
    bson_t reply;
    bool ok = mongoc_collection_create_indexes_with_opts(collection, 
        const_cast<const mongoc_index_model_t**>(nullptr), 0, nullptr, &reply, &error);
    
    // 注意：libmongoc 需要先创建 index model，这里简化处理
    // 实际实现需要遍历 indexes 数组，为每个创建 mongoc_index_model_t
    
    bson_destroy(&reply);
    mongoc_collection_destroy(collection);

    for (auto* idx : indexes) {
        bson_destroy(idx);
    }

    if (!ok) {
        SPDLOG_ERROR("[MongoServer]Failed to create indexes for {}: {}", collection_name, error.message);
    }

    return ok;
}

bool MongoServer::read_index_config(const std::string& collection_name, std::vector<bson_t*>& indexes) {
    std::string path = index_config_dir_ + "/" + collection_name + "_index.json";
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    nlohmann::json config;
    try {
        file >> config;
    } catch (const nlohmann::json::parse_error& e) {
        SPDLOG_ERROR("[MongoServer]Failed to parse index config {}: {}", path, e.what());
        return false;
    }

    if (!config.is_array()) {
        SPDLOG_ERROR("[MongoServer]Invalid index config format: {}", path);
        return false;
    }

    for (const auto& idx : config) {
        bson_t* index = bson_new();
        bson_t keys;
        bson_init(&keys);

        // 添加索引键
        std::string key = idx.value("key", "");
        if (key.empty()) {
            bson_destroy(index);
            bson_destroy(&keys);
            continue;
        }
        BSON_APPEND_INT32(&keys, key.c_str(), 1);

        // 构建索引文档
        BSON_APPEND_DOCUMENT(index, "key", &keys);
        bson_destroy(&keys);

        // 添加选项
        if (idx.contains("unique")) {
            BSON_APPEND_BOOL(index, "unique", idx["unique"].get<bool>());
        }
        if (idx.contains("sparse")) {
            BSON_APPEND_BOOL(index, "sparse", idx["sparse"].get<bool>());
        }
        if (idx.contains("expireAfterSeconds")) {
            BSON_APPEND_INT32(index, "expireAfterSeconds", idx["expireAfterSeconds"].get<int>());
        }

        indexes.push_back(index);
    }

    return !indexes.empty();
}

mongoc_collection_t* MongoServer::get_collection(const std::string& name) {
    // 从 URI 中提取数据库名
    std::string uri = "mongodb://localhost:27017/farm";  // 需要从配置获取
    mongoc_uri_t* mongoc_uri = mongoc_uri_new(uri.c_str());
    const char* db_name = mongoc_uri_get_database(mongoc_uri);
    
    mongoc_database_t* db = mongoc_client_get_database(conn_.client(), db_name);
    mongoc_collection_t* collection = mongoc_database_get_collection(db, name.c_str());
    
    mongoc_database_destroy(db);
    mongoc_uri_destroy(mongoc_uri);
    
    return collection;
}

DataResult MongoServer::get_all(uint64_t player_id, std::vector<uint8_t>& value) {
    mongoc_collection_t* collection = get_collection("players");
    if (!collection) return DataResult::IO_ERROR;

    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_INT64(&filter, "player_id", static_cast<int64_t>(player_id));

    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, &filter, nullptr, nullptr);
    const bson_t* doc;
    bool found = false;

    if (mongoc_cursor_next(cursor, &doc)) {
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, doc, "data")) {
            uint32_t len;
            const uint8_t* data;
            bson_iter_document(&iter, &len, &data);
            value.assign(data, data + len);
            found = true;
        }
    }

    mongoc_cursor_destroy(cursor);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    return found ? DataResult::SUCCESS : DataResult::KEY_NOT_FOUND;
}

DataResult MongoServer::get(uint64_t player_id, const std::string& key, std::vector<uint8_t>& value) {
    std::vector<uint8_t> all_data;
    DataResult result = get_all(player_id, all_data);
    if (result != DataResult::SUCCESS) return result;

    // 解析 JSON，提取指定 key
    try {
        std::string json_str(all_data.begin(), all_data.end());
        auto json = nlohmann::json::parse(json_str);
        if (json.contains(key)) {
            std::string val = json[key].dump();
            value.assign(val.begin(), val.end());
        } else {
            value.clear();
        }
    } catch (const nlohmann::json::exception& e) {
        SPDLOG_ERROR("[MongoServer]JSON parse error in get: {}", e.what());
        return DataResult::PARSE_ERROR;
    }

    return DataResult::SUCCESS;
}

DataResult MongoServer::set_all(uint64_t player_id, const std::vector<uint8_t>& value) {
    mongoc_collection_t* collection = get_collection("players");
    if (!collection) return DataResult::IO_ERROR;

    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_INT64(&filter, "player_id", static_cast<int64_t>(player_id));

    // 解析 data 为 bson
    std::string data_str(value.begin(), value.end());
    bson_t* data_doc = bson_new_from_json(reinterpret_cast<const uint8_t*>(data_str.c_str()), -1, nullptr);
    if (!data_doc) {
        bson_destroy(&filter);
        mongoc_collection_destroy(collection);
        return DataResult::PARSE_ERROR;
    }

    bson_t update;
    bson_init(&update);
    bson_t set_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$set", &set_doc);
    BSON_APPEND_INT64(&set_doc, "player_id", static_cast<int64_t>(player_id));
    BSON_APPEND_DOCUMENT(&set_doc, "data", data_doc);
    bson_append_document_end(&update, &set_doc);

    bson_error_t error;
    bson_t reply;
    bool ok = mongoc_collection_update_one(collection, &filter, &update, nullptr, &reply, &error);

    bson_destroy(&reply);
    bson_destroy(&update);
    bson_destroy(data_doc);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    if (!ok) {
        SPDLOG_ERROR("[MongoServer]set_all failed: {}", error.message);
        return DataResult::IO_ERROR;
    }

    return DataResult::SUCCESS;
}

DataResult MongoServer::set(uint64_t player_id, const std::string& key, const std::vector<uint8_t>& value) {
    mongoc_collection_t* collection = get_collection("players");
    if (!collection) return DataResult::IO_ERROR;

    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_INT64(&filter, "player_id", static_cast<int64_t>(player_id));

    // 解析 value
    std::string val_str(value.begin(), value.end());
    bson_t* val_doc = bson_new_from_json(reinterpret_cast<const uint8_t*>(val_str.c_str()), -1, nullptr);
    if (!val_doc) {
        bson_destroy(&filter);
        mongoc_collection_destroy(collection);
        return DataResult::PARSE_ERROR;
    }

    // 构建 update: { $set: { "data.key": value } }
    std::string field_path = "data." + key;
    bson_t update;
    bson_init(&update);
    bson_t set_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$set", &set_doc);
    BSON_APPEND_DOCUMENT(&set_doc, field_path.c_str(), val_doc);
    bson_append_document_end(&update, &set_doc);

    bson_error_t error;
    bson_t reply;
    bool ok = mongoc_collection_update_one(collection, &filter, &update, nullptr, &reply, &error);

    bson_destroy(&reply);
    bson_destroy(&update);
    bson_destroy(val_doc);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    if (!ok) {
        SPDLOG_ERROR("[MongoServer]set failed: {}", error.message);
        return DataResult::IO_ERROR;
    }

    return DataResult::SUCCESS;
}

DataResult MongoServer::del(uint64_t player_id, const std::string& key) {
    mongoc_collection_t* collection = get_collection("players");
    if (!collection) return DataResult::IO_ERROR;

    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_INT64(&filter, "player_id", static_cast<int64_t>(player_id));

    // 构建 update: { $unset: { "data.key": "" } }
    std::string field_path = "data." + key;
    bson_t update;
    bson_init(&update);
    bson_t unset_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$unset", &unset_doc);
    BSON_APPEND_UTF8(&unset_doc, field_path.c_str(), "");
    bson_append_document_end(&update, &unset_doc);

    bson_error_t error;
    bson_t reply;
    bool ok = mongoc_collection_update_one(collection, &filter, &update, nullptr, &reply, &error);

    bson_destroy(&reply);
    bson_destroy(&update);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    if (!ok) {
        SPDLOG_ERROR("[MongoServer]del failed: {}", error.message);
        return DataResult::IO_ERROR;
    }

    return DataResult::SUCCESS;
}

AccountResult MongoServer::get_account(const std::string& account_id, std::vector<AccountRole>& roles) {
    mongoc_collection_t* collection = get_collection("accounts");
    if (!collection) return AccountResult::IO_ERROR;

    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_UTF8(&filter, "account_id", account_id.c_str());

    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, &filter, nullptr, nullptr);
    const bson_t* doc;
    roles.clear();

    if (mongoc_cursor_next(cursor, &doc)) {
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, doc, "roles") && BSON_ITER_HOLDS_ARRAY(&iter)) {
            bson_iter_t array_iter;
            bson_iter_recurse(&iter, &array_iter);

            while (bson_iter_next(&array_iter)) {
                const bson_t* role_doc;
                if (bson_iter_document(&array_iter, nullptr, &role_doc)) {
                    AccountRole role;
                    bson_iter_t role_iter;
                    if (bson_iter_init_find(&role_iter, role_doc, "server_id")) {
                        role.server_id = bson_iter_int32(&role_iter);
                    }
                    if (bson_iter_init_find(&role_iter, role_doc, "player_id")) {
                        role.player_id = bson_iter_int64(&role_iter);
                    }
                    if (bson_iter_init_find(&role_iter, role_doc, "role_name")) {
                        role.role_name = bson_iter_utf8(&role_iter, nullptr);
                    }
                    roles.push_back(role);
                }
            }
        }
    }

    mongoc_cursor_destroy(cursor);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    return AccountResult::SUCCESS;
}

AccountResult MongoServer::set_account(const std::string& account_id, const AccountRole& new_role) {
    mongoc_collection_t* collection = get_collection("accounts");
    if (!collection) return AccountResult::IO_ERROR;

    // 先检查角色是否已存在
    std::vector<AccountRole> existing_roles;
    AccountResult get_result = get_account(account_id, existing_roles);
    if (get_result != AccountResult::SUCCESS) {
        mongoc_collection_destroy(collection);
        return get_result;
    }

    for (const auto& role : existing_roles) {
        if (role.server_id == new_role.server_id) {
            mongoc_collection_destroy(collection);
            return AccountResult::ROLE_ALREADY_EXISTS;
        }
    }

    // 添加新角色
    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_UTF8(&filter, "account_id", account_id.c_str());

    bson_t role_doc;
    bson_init(&role_doc);
    BSON_APPEND_INT32(&role_doc, "server_id", static_cast<int32_t>(new_role.server_id));
    BSON_APPEND_INT64(&role_doc, "player_id", static_cast<int64_t>(new_role.player_id));
    BSON_APPEND_UTF8(&role_doc, "role_name", new_role.role_name.c_str());

    bson_t update;
    bson_init(&update);
    bson_t push_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$push", &push_doc);
    BSON_APPEND_DOCUMENT(&push_doc, "roles", &role_doc);
    bson_append_document_end(&update, &push_doc);

    bson_error_t error;
    bson_t reply;
    bool ok = mongoc_collection_update_one(collection, &filter, &update, nullptr, &reply, &error);

    bson_destroy(&reply);
    bson_destroy(&update);
    bson_destroy(&role_doc);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    if (!ok) {
        SPDLOG_ERROR("[MongoServer]set_account failed: {}", error.message);
        return AccountResult::IO_ERROR;
    }

    return AccountResult::SUCCESS;
}

}  // namespace farm
```

- [ ] **Step 3: 提交 MongoServer**

```bash
git add scripts/server/dbmgr/src/mongo_server.h scripts/server/dbmgr/src/mongo_server.cpp
git commit -m "feat: implement MongoServer with data operations"
```

---

## Task 8: 实现 RedisServer

**目标:** 实现 Redis 业务层，提供通用缓存操作

**Files:**
- Create: `scripts/server/dbmgr/src/redis_server.h`
- Create: `scripts/server/dbmgr/src/redis_server.cpp`

- [ ] **Step 1: 创建 redis_server.h**

```cpp
#pragma once

#include "redis_connection.h"

#include <string>
#include <vector>
#include <cstdint>

namespace farm {

enum class CacheResult : int32_t {
    SUCCESS = 0,
    NOT_FOUND = 1,
    CONNECTION_ERROR = 2,
    TIMEOUT = 3,
};

class RedisServer {
public:
    RedisServer(RedisConnection& conn);

    // 获取缓存
    CacheResult get(const std::string& key, std::vector<uint8_t>& value);

    // 设置缓存（ttl_seconds 由调用方传入）
    CacheResult set(const std::string& key, const std::vector<uint8_t>& value, int ttl_seconds);

    // 删除缓存
    CacheResult del(const std::string& key);

    // 检查连接是否可用
    bool is_available() const;

private:
    RedisConnection& conn_;
};

}  // namespace farm
```

- [ ] **Step 2: 创建 redis_server.cpp**

```cpp
#include "redis_server.h"
#include "log_macros.h"

#include <cstring>

namespace farm {

RedisServer::RedisServer(RedisConnection& conn)
    : conn_(conn) {
}

CacheResult RedisServer::get(const std::string& key, std::vector<uint8_t>& value) {
    if (!conn_.is_connected()) {
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = (redisReply*)redisCommand(conn_.context(), "GET %s", key.c_str());
    if (!reply) {
        SPDLOG_ERROR("[RedisServer]GET command failed for key: {}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_NIL) {
        result = CacheResult::NOT_FOUND;
        value.clear();
    } else if (reply->type == REDIS_REPLY_STRING) {
        result = CacheResult::SUCCESS;
        value.assign(reply->str, reply->str + reply->len);
    } else {
        SPDLOG_ERROR("[RedisServer]Unexpected reply type for GET: {}", reply->type);
        result = CacheResult::CONNECTION_ERROR;
        value.clear();
    }

    freeReplyObject(reply);
    return result;
}

CacheResult RedisServer::set(const std::string& key, const std::vector<uint8_t>& value, int ttl_seconds) {
    if (!conn_.is_connected()) {
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = (redisReply*)redisCommand(conn_.context(), 
        "SET %s %b EX %d", 
        key.c_str(), 
        value.data(), value.size(),
        ttl_seconds);

    if (!reply) {
        SPDLOG_ERROR("[RedisServer]SET command failed for key: {}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_STATUS && reply->str && std::strcmp(reply->str, "OK") == 0) {
        result = CacheResult::SUCCESS;
    } else {
        SPDLOG_ERROR("[RedisServer]SET failed: {}", reply->str ? reply->str : "unknown");
        result = CacheResult::CONNECTION_ERROR;
    }

    freeReplyObject(reply);
    return result;
}

CacheResult RedisServer::del(const std::string& key) {
    if (!conn_.is_connected()) {
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = (redisReply*)redisCommand(conn_.context(), "DEL %s", key.c_str());
    if (!reply) {
        SPDLOG_ERROR("[RedisServer]DEL command failed for key: {}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer > 0 ? CacheResult::SUCCESS : CacheResult::NOT_FOUND;
    } else {
        result = CacheResult::CONNECTION_ERROR;
    }

    freeReplyObject(reply);
    return result;
}

bool RedisServer::is_available() const {
    return conn_.is_connected();
}

}  // namespace farm
```

- [ ] **Step 3: 提交 RedisServer**

```bash
git add scripts/server/dbmgr/src/redis_server.h scripts/server/dbmgr/src/redis_server.cpp
git commit -m "feat: implement RedisServer cache operations"
```

---

## Task 9: 更新 DbMgrServer 集成新组件

**目标:** 修改 DbMgrServer 以使用 ConnectionManager、MongoServer、RedisServer

**Files:**
- Modify: `scripts/server/dbmgr/src/dbmgr_server.h`
- Modify: `scripts/server/dbmgr/src/dbmgr_server.cpp`

- [ ] **Step 1: 更新 dbmgr_server.h**

```cpp
#pragma once

#include "game_session.h"
#include "connection_manager.h"
#include "mongo_server.h"
#include "redis_server.h"
#include "message_parser.h"
#include "admin_msg_ids.h"

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <memory>

namespace farm {

class DbMgrServer {
public:
    // 新构造函数：使用 ConnectionManager
    DbMgrServer(uint32_t index, const std::string& ip, uint16_t port,
                ConnectionManager& conn_mgr, const std::string& index_config_dir);
    ~DbMgrServer();

    // 启动服务器（阻塞）
    bool start();

    // 停止服务器
    void stop();

private:
    // libevent 回调（保持不变）
    static void on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                          struct sockaddr* addr, int len, void* ctx);
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);

    // 连接处理（保持不变）
    void handle_accept(evutil_socket_t fd, struct sockaddr* addr);
    void handle_read(std::shared_ptr<GameSession> session);
    void handle_disconnect(std::shared_ptr<GameSession> session);
    void check_heartbeat();
    void send_heartbeat(std::shared_ptr<GameSession> session);

    // 消息路由（保持不变）
    void route_message(std::shared_ptr<GameSession> session,
                       uint32_t msg_id,
                       const std::vector<uint8_t>& payload);

    // 具体消息处理（保持不变）
    void handle_dbmgr_identify_resp(std::shared_ptr<GameSession> session,
                                    const std::vector<uint8_t>& payload);
    void handle_dbmgr_heartbeat_resp(std::shared_ptr<GameSession> session,
                                     const std::vector<uint8_t>& payload);
    void handle_player_data_req(std::shared_ptr<GameSession> session,
                                const std::vector<uint8_t>& payload);
    void handle_account_data_req(std::shared_ptr<GameSession> session,
                                 const std::vector<uint8_t>& payload);
    void handle_account_set_req(std::shared_ptr<GameSession> session,
                                const std::vector<uint8_t>& payload);

    // 发送消息辅助（保持不变）
    void send_to_game(std::shared_ptr<GameSession> session,
                      uint32_t msg_id, const std::string& payload);

    // 管理消息处理（保持不变）
    void handle_admin_message(std::shared_ptr<GameSession> session,
                              uint32_t msg_id, const std::vector<uint8_t>& payload);
    void handle_shutdown(std::shared_ptr<GameSession> session, const AdminShutdownMsg& msg);
    void handle_shutdown_resp(std::shared_ptr<GameSession> session, const AdminShutdownResp& resp);

    // 检查数据库是否就绪
    bool is_db_ready() const;

    uint32_t index_;
    std::string ip_;
    uint16_t port_;
    struct event_base* base_;
    struct evconnlistener* listener_;
    struct event* heartbeat_timer_;
    bool running_;

    // Game 会话管理（按 fd 索引）
    std::unordered_map<evutil_socket_t, std::shared_ptr<GameSession>> game_sessions_;

    // 新组件
    ConnectionManager& conn_mgr_;
    MongoServer mongo_server_;
    RedisServer redis_server_;
};

}  // namespace farm
```

- [ ] **Step 2: 更新 dbmgr_server.cpp**

需要修改的关键部分：

1. 构造函数：初始化 MongoServer 和 RedisServer
2. start()：检查连接状态，初始化索引
3. handle_player_data_req()：使用 MongoServer
4. handle_account_data_req()：使用 MongoServer
5. handle_account_set_req()：使用 MongoServer
6. 移除 DataManager 相关代码

```cpp
// 构造函数
DbMgrServer::DbMgrServer(uint32_t index, const std::string& ip, uint16_t port,
                         ConnectionManager& conn_mgr, const std::string& index_config_dir)
    : index_(index)
    , ip_(ip)
    , port_(port)
    , base_(nullptr)
    , listener_(nullptr)
    , heartbeat_timer_(nullptr)
    , running_(false)
    , conn_mgr_(conn_mgr)
    , mongo_server_(conn_mgr.mongo_connection(), index_config_dir)
    , redis_server_(conn_mgr.redis_connection())
{
}

// start() 方法修改
bool DbMgrServer::start() {
    // 检查连接状态
    if (conn_mgr_.is_ready()) {
        // 连接就绪，初始化 MongoDB 索引
        if (!mongo_server_.init()) {
            SPDLOG_ERROR("[DBMgr]Failed to initialize MongoDB indexes");
            return false;
        }
        SPDLOG_INFO("[DBMgr]Database ready");
    } else {
        SPDLOG_WARN("[DBMgr]Database not ready, will reject data requests");
        // 注册回调，连接恢复后初始化
        conn_mgr_.set_on_ready_callback([this]() {
            if (mongo_server_.init()) {
                SPDLOG_INFO("[DBMgr]Database recovered and ready");
            }
        });
    }

    // ... 其余 libevent 初始化代码保持不变 ...

    running_ = true;
    SPDLOG_INFO("[DBMgr]DBMgr index={} listening on {}:{}", index_, ip_, port_);

    // 进入事件循环
    event_base_dispatch(base_);

    running_ = false;
    return true;
}

// 检查数据库就绪
bool DbMgrServer::is_db_ready() const {
    return conn_mgr_.is_ready();
}

// handle_player_data_req() 修改
void DbMgrServer::handle_player_data_req(std::shared_ptr<GameSession> session,
                                          const std::vector<uint8_t>& payload) {
    // 解析请求（保持不变）
    farm::PlayerDataReq req;
    if (!payload.empty() && !req.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[DBMgr]Failed to parse PlayerDataReq");
        return;
    }

    uint64_t request_id = req.request_id();
    uint64_t player_id = req.player_id();
    PlayerDataOp op = req.op();
    const std::string& key = req.key();
    const std::string& value = req.value();

    // 检查数据库就绪
    farm::PlayerDataResp resp;
    resp.set_request_id(request_id);

    if (!is_db_ready()) {
        resp.set_code(-1);  // DB_NOT_READY
        resp.set_msg("Database not ready");
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        send_to_game(session, MSG_ID_PLAYER_DATA_RESP, resp_data);
        return;
    }

    std::vector<uint8_t> result_value;
    DataResult result;

    switch (op) {
        case PlayerDataOp::GET_ALL:
            result = mongo_server_.get_all(player_id, result_value);
            break;
        case PlayerDataOp::GET:
            result = mongo_server_.get(player_id, key, result_value);
            break;
        case PlayerDataOp::SET_ALL:
            result = mongo_server_.set_all(player_id, std::vector<uint8_t>(value.begin(), value.end()));
            break;
        case PlayerDataOp::SET:
            result = mongo_server_.set(player_id, key, std::vector<uint8_t>(value.begin(), value.end()));
            break;
        case PlayerDataOp::DEL:
            result = mongo_server_.del(player_id, key);
            break;
        default:
            result = DataResult::PARSE_ERROR;
            break;
    }

    resp.set_code(static_cast<int32_t>(result));
    if (!result_value.empty()) {
        resp.set_value(result_value.data(), result_value.size());
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_to_game(session, MSG_ID_PLAYER_DATA_RESP, resp_data);
}

// handle_account_data_req() 和 handle_account_set_req() 类似修改
// 将 data_mgr_. 替换为 mongo_server_.
```

- [ ] **Step 3: 提交 DbMgrServer 更新**

```bash
git add scripts/server/dbmgr/src/dbmgr_server.h scripts/server/dbmgr/src/dbmgr_server.cpp
git commit -m "feat: integrate ConnectionManager, MongoServer, RedisServer into DbMgrServer"
```

---

## Task 10: 更新 main.cpp

**目标:** 更新启动流程，使用新的配置格式和 ConnectionManager

**Files:**
- Modify: `scripts/server/dbmgr/src/main.cpp`

- [ ] **Step 1: 更新 main.cpp**

```cpp
#include "dbmgr_server.h"
#include "connection_manager.h"
#include "server_main_helper.h"
#include "log_init.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>

#include <csignal>
#include <fstream>
#include <string>

static farm::DbMgrServer* g_server = nullptr;
static farm::ConnectionManager* g_conn_mgr = nullptr;

static void signal_handler(int sig) {
    if (g_server) {
        SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    if (!farm::init_platform_network()) return 1;

    farm::init_logging_default("dbmgr");

    // 解析配置文件
    std::string config_path = farm::parse_config_path(argc, argv, "config/dbmgr.json");
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        SPDLOG_ERROR("[Main]Config file not found: {}", config_path);
        return 1;
    }

    nlohmann::json config;
    try {
        config_file >> config;
    } catch (const nlohmann::json::parse_error& e) {
        SPDLOG_ERROR("[Main]Failed to parse config file: {}", e.what());
        return 1;
    }
    config_file.close();

    farm::init_logging_from_config("dbmgr", config);

    // 读取服务器配置
    uint32_t index = config.value("/server/index"_json_pointer, 0);
    uint16_t port = static_cast<uint16_t>(config.value("/server/port"_json_pointer, 5000));
    std::string ip = config.value("/server/ip"_json_pointer, "0.0.0.0");
    std::string pid_file = config.value("pid_file", "./runtimeData/dbmgr.pid");

    // 读取数据库配置
    farm::MongoConfig mongo_config;
    mongo_config.uri = config.value("/database/mongo/uri"_json_pointer, "mongodb://localhost:27017/farm");
    mongo_config.retry_interval_ms = config.value("/database/mongo/retry_interval_ms"_json_pointer, 3000);
    mongo_config.max_retry_count = config.value("/database/mongo/max_retry_count"_json_pointer, 0);

    farm::RedisConfig redis_config;
    redis_config.uri = config.value("/database/redis/uri"_json_pointer, "redis://localhost:6379");
    redis_config.retry_interval_ms = config.value("/database/redis/retry_interval_ms"_json_pointer, 3000);
    redis_config.max_retry_count = config.value("/database/redis/max_retry_count"_json_pointer, 0);

    std::string index_config_dir = "config/mongo";

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    farm::write_pid_file(pid_file);

    SPDLOG_INFO("[Main]=== DBMgr Server ===");
    SPDLOG_INFO("[Main]Index: {}", index);
    SPDLOG_INFO("[Main]IP: {}", ip);
    SPDLOG_INFO("[Main]Port: {}", port);
    SPDLOG_INFO("[Main]MongoDB URI: {}", mongo_config.uri);
    SPDLOG_INFO("[Main]Redis URI: {}", redis_config.uri);

    // 创建 ConnectionManager
    farm::ConnectionManager conn_mgr(mongo_config, redis_config);
    g_conn_mgr = &conn_mgr;

    // 初始化连接
    conn_mgr.init();

    // 创建并启动服务器
    farm::DbMgrServer server(index, ip, port, conn_mgr, index_config_dir);
    g_server = &server;

    if (!server.start()) {
        SPDLOG_ERROR("[Main]Server failed to start");
        return 1;
    }

    SPDLOG_INFO("[Main]Server stopped");
    farm::shutdown_logging();
    farm::cleanup_platform_network();
    return 0;
}
```

- [ ] **Step 2: 提交 main.cpp 更新**

```bash
git add scripts/server/dbmgr/src/main.cpp
git commit -m "feat: update main.cpp for MongoDB/Redis integration"
```

---

## Task 11: 更新 CMakeLists.txt

**目标:** 添加新源文件和依赖库链接

**Files:**
- Modify: `scripts/server/dbmgr/CMakeLists.txt`

- [ ] **Step 1: 更新 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.14)
project(dbmgr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 静态链接运行时
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 依赖路径
set(LIBEVENT_ROOT "C:/libevent_install")
set(PROTOBUF_ROOT "C:/protobuf_install")
set(MONGOC_ROOT "C:/mongoc_install")  # libmongoc 安装路径
set(HIREDIS_ROOT "C:/hiredis_install")  # hiredis 安装路径
set(COMMON_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/proto")

# Protobuf 生成的源文件
set(PROTO_GENERATED_DIR "${COMMON_PROTO_DIR}/generated")
set(PROTO_SRCS
    ${PROTO_GENERATED_DIR}/base.pb.cc
    ${PROTO_GENERATED_DIR}/dbmgr.pb.cc
    ${PROTO_GENERATED_DIR}/account.pb.cc
)
set(PROTO_HDRS
    ${PROTO_GENERATED_DIR}/base.pb.h
    ${PROTO_GENERATED_DIR}/dbmgr.pb.h
    ${PROTO_GENERATED_DIR}/account.pb.h
)

# 源文件
set(SOURCES
    src/main.cpp
    src/dbmgr_server.cpp
    src/game_session.cpp
    src/connection_manager.cpp
    src/mongo_connection.cpp
    src/mongo_server.cpp
    src/redis_connection.cpp
    src/redis_server.cpp
    ${PROTO_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/admin_msg_ids.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/log_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/message_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/msvc_compat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/server_main_helper.cpp
)

add_executable(dbmgr ${SOURCES})

# Include 目录
target_include_directories(dbmgr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROTOBUF_ROOT}/include
    ${LIBEVENT_ROOT}/include
    ${MONGOC_ROOT}/include/libmongoc-1.0
    ${HIREDIS_ROOT}/include
    ${COMMON_PROTO_DIR}
    ${COMMON_PROTO_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# 链接库目录
target_link_directories(dbmgr PRIVATE
    ${PROTOBUF_ROOT}/lib
    ${LIBEVENT_ROOT}/lib
    ${MONGOC_ROOT}/lib
    ${HIREDIS_ROOT}/lib
)

# MSVC 特定设置
if(MSVC)
    target_compile_definitions(dbmgr PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NOMINMAX
    )

    target_compile_options(dbmgr PRIVATE /utf-8 /MT$<$<CONFIG:Debug>:d>)

    # 链接库
    file(GLOB ABSL_LIBS "${PROTOBUF_ROOT}/lib/absl_*.lib")
    target_link_libraries(dbmgr PRIVATE
        libprotobuf-lite.lib
        libutf8_range.lib
        libutf8_validity.lib
        ${ABSL_LIBS}
        event.lib
        event_core.lib
        event_extra.lib
        mongoc-1.0.lib
        bson-1.0.lib
        hiredis.lib
        ws2_32.lib
        advapi32.lib
        shell32.lib
    )
else()
    # Linux/macOS
    target_link_libraries(dbmgr PRIVATE
        protobuf
        event
        event_core
        event_extra
        mongoc-1.0
        bson-1.0
        hiredis
        pthread
    )
endif()

# 输出目录
set_target_properties(dbmgr PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
)
```

- [ ] **Step 2: 提交 CMakeLists.txt 更新**

```bash
git add scripts/server/dbmgr/CMakeLists.txt
git commit -m "build: update CMakeLists.txt for MongoDB/Redis dependencies"
```

---

## Task 12: 删除旧的 DataManager 文件

**目标:** 移除被 MongoServer 替代的 DataManager

**Files:**
- Delete: `scripts/server/dbmgr/src/data_manager.h`
- Delete: `scripts/server/dbmgr/src/data_manager.cpp`

- [ ] **Step 1: 删除 DataManager 文件**

```bash
rm scripts/server/dbmgr/src/data_manager.h scripts/server/dbmgr/src/data_manager.cpp
```

- [ ] **Step 2: 提交删除**

```bash
git add -A scripts/server/dbmgr/src/
git commit -m "refactor: remove DataManager replaced by MongoServer"
```

---

## Task 13: 构建和测试

**目标:** 编译项目并验证基本功能

- [ ] **Step 1: 配置 CMake**

```bash
cd scripts/server/dbmgr
mkdir -p build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
```

- [ ] **Step 2: 编译项目**

```bash
cmake --build . --config Release
```

- [ ] **Step 3: 验证编译成功**

确认 `bin/dbmgr.exe` 生成

- [ ] **Step 4: 启动 MongoDB 和 Redis**

```bash
# 启动 MongoDB
mongod --dbpath ./data/db

# 启动 Redis
redis-server
```

- [ ] **Step 5: 运行 dbmgr 测试**

```bash
cd ../../../
bin/dbmgr.exe
```

预期输出：
- MongoDB 连接成功日志
- Redis 连接成功日志
- 索引创建成功日志
- 服务器监听启动日志

- [ ] **Step 6: 提交最终版本**

```bash
git add -A
git commit -m "feat: complete MongoDB/Redis integration for dbmgr"
```

---

## 自检清单

### Spec 覆盖检查

| Spec 要求 | 对应 Task |
|----------|----------|
| libmongoc + hiredis 依赖 | Task 1 |
| 索引配置文件 | Task 2 |
| dbmgr.json 配置格式 | Task 3 |
| MongoConnection | Task 4 |
| RedisConnection | Task 5 |
| ConnectionManager | Task 6 |
| MongoServer | Task 7 |
| RedisServer | Task 8 |
| DbMgrServer 集成 | Task 9 |
| main.cpp 启动流程 | Task 10 |
| CMakeLists.txt 更新 | Task 11 |
| 删除 DataManager | Task 12 |
| 构建和测试 | Task 13 |

### 接口一致性检查

- MongoConnection::client() → MongoServer 使用 ✓
- RedisConnection::context() → RedisServer 使用 ✓
- ConnectionManager 持有 MongoConnection 和 RedisConnection ✓
- MongoServer 接口与 DataManager 兼容 ✓
- RedisServer 接口与设计文档一致 ✓
