# Global Player ID Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the in-memory per-server PlayerIdGenerator with a centralized MongoDB-backed ID allocation system that guarantees globally unique player_id across all servers.

**Architecture:** DBMgr pre-allocates batches of IDs from MongoDB's atomic `findOneAndUpdate` on a `counters` collection, caches them locally, and distributes them to Game Servers on demand. Game Server maintains a `PlayerIdPool` that auto-replenishes when remaining IDs drop below 30%.

**Tech Stack:** C++17, libmongoc (MongoDB C driver), protobuf, libevent

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `scripts/common/proto/dbmgr.proto` | Modify | Add `AllocPlayerIdReq` / `AllocPlayerIdResp` messages |
| `scripts/server/common/include/internal_msg_ids.h` | Modify | Add `MSG_ID_ALLOC_PLAYER_ID_REQ/RESP` constants |
| `scripts/server/dbmgr/src/mongo_server.h` | Modify | Add `init_counter()`, `alloc_player_ids()` declarations |
| `scripts/server/dbmgr/src/mongo_server.cpp` | Modify | Implement MongoDB counter operations |
| `scripts/server/dbmgr/src/dbmgr_server.h` | Modify | Add `handle_alloc_player_id_req()` declaration |
| `scripts/server/dbmgr/src/dbmgr_server.cpp` | Modify | Add message routing and handler |
| `scripts/server/game_server/src/player_id_pool.h` | Create | `PlayerIdPool` class with acquire/replenish logic |
| `scripts/server/game_server/src/dbmgr_connection_manager.h` | Modify | Add `AllocPlayerIdCallback`, `send_alloc_player_id_req()`, `handle_alloc_player_id_resp()` |
| `scripts/server/game_server/src/dbmgr_connection_manager.cpp` | Modify | Implement ID allocation request/response |
| `scripts/server/game_server/src/login_stub.h` | Modify | Replace `PlayerIdGenerator` with `PlayerIdPool*` |
| `scripts/server/game_server/src/login_stub.cpp` | Modify | Use `id_pool_->acquire()` in `handle_create_role()` |
| `scripts/server/game_server/src/game_server.h` | Modify | Add `PlayerIdPool` member |
| `scripts/server/game_server/src/game_server.cpp` | Modify | Create `PlayerIdPool`, inject into `LoginStub` |
| `scripts/server/game_server/src/player_id_generator.h` | Delete | No longer needed |

---

### Task 1: Proto Messages and Message IDs

**Files:**
- Modify: `scripts/common/proto/dbmgr.proto`
- Modify: `scripts/server/common/include/internal_msg_ids.h`

- [x] **Step 1: Add proto messages to `dbmgr.proto`**

Add the following after the `AccountSetResp` message (before the closing of the file):

```protobuf
// 分配 Player ID 请求 (Game -> DBMgr)
message AllocPlayerIdReq {
    uint64 request_id = 1;   // 请求 ID，用于异步匹配
    uint32 count = 2;        // 请求分配的数量（如 64）
}

// 分配 Player ID 响应 (DBMgr -> Game)
message AllocPlayerIdResp {
    uint64 request_id = 1;   // 对应请求的 ID
    int32 code = 2;          // 0=成功, 其他=失败
    uint64 start_id = 3;     // 分配的起始 ID（含）
    uint32 count = 4;        // 实际分配的数量
}
```

- [x] **Step 2: Add message IDs to `internal_msg_ids.h`**

Add after `MSG_ID_ACCOUNT_SET_RESP = 4204;`:

```cpp
inline constexpr uint32_t MSG_ID_ALLOC_PLAYER_ID_REQ     = 4205;
inline constexpr uint32_t MSG_ID_ALLOC_PLAYER_ID_RESP     = 4206;
```

- [x] **Step 3: Regenerate protobuf**

```bash
cd D:/mb_workspace/farm_demo/scripts/common/proto
protoc --cpp_out=. dbmgr.proto
```

Verify `generated/dbmgr.pb.h` and `generated/dbmgr.pb.cc` are updated with `AllocPlayerIdReq` and `AllocPlayerIdResp` classes.

- [x] **Step 4: Commit**

```bash
git add scripts/common/proto/dbmgr.proto scripts/common/proto/generated/dbmgr.pb.h scripts/common/proto/generated/dbmgr.pb.cc scripts/server/common/include/internal_msg_ids.h
git commit -m "feat(proto): add AllocPlayerIdReq/Resp messages and message IDs"
```

---

### Task 2: DBMgr - MongoServer ID Allocation

**Files:**
- Modify: `scripts/server/dbmgr/src/mongo_server.h`
- Modify: `scripts/server/dbmgr/src/mongo_server.cpp`

- [x] **Step 1: Add declarations to `mongo_server.h`**

Add after the `set_account` declaration:

```cpp
    // 初始化 counters 集合（确保 player_id 计数器文档存在）
    bool init_counter();

    // 原子分配 N 个 player_id，返回起始 ID
    // 成功: 返回 start_id (>0), 失败: 返回 0
    int64_t alloc_player_ids(uint32_t count);
```

- [x] **Step 2: Implement `init_counter()` in `mongo_server.cpp`**

Add at the end of the file (before the closing `}  // namespace farm`):

```cpp
bool MongoServer::init_counter() {
    mongoc_collection_t* collection = get_collection("counters");
    if (!collection) return false;

    // Upsert: ensure the counter document exists
    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_UTF8(&filter, "_id", "player_id");

    bson_t update;
    bson_init(&update);
    bson_t set_on_insert;
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$setOnInsert", &set_on_insert);
    BSON_APPEND_INT64(&set_on_insert, "seq", 0);
    bson_append_document_end(&update, &set_on_insert);

    bson_t opts;
    bson_init(&opts);
    BSON_APPEND_BOOL(&opts, "upsert", true);

    bson_error_t error;
    bson_t reply;
    bool ok = mongoc_collection_update_one(collection, &filter, &update, &opts, &reply, &error);

    bson_destroy(&reply);
    bson_destroy(&opts);
    bson_destroy(&update);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    if (!ok) {
        SPDLOG_ERROR("[MongoServer]init_counter failed: {}", error.message);
        return false;
    }

    SPDLOG_INFO("[MongoServer]Counter initialized");
    return true;
}

int64_t MongoServer::alloc_player_ids(uint32_t count) {
    mongoc_collection_t* collection = get_collection("counters");
    if (!collection) return 0;

    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_UTF8(&filter, "_id", "player_id");

    bson_t update;
    bson_init(&update);
    bson_t inc_doc;
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$inc", &inc_doc);
    BSON_APPEND_INT64(&inc_doc, "seq", static_cast<int64_t>(count));
    bson_append_document_end(&update, &inc_doc);

    bson_t opts;
    bson_init(&opts);
    BSON_APPEND_BOOL(&opts, "upsert", true);
    BSON_APPEND_BOOL(&opts, "returnDocument", true);  // MONGOC_RETURN_DOCUMENT_AFTER

    bson_error_t error;
    bson_t reply;
    bool ok = mongoc_collection_find_and_modify(collection, &filter, nullptr, &update, nullptr, &opts, &reply, &error);

    int64_t new_seq = 0;
    if (ok) {
        bson_iter_t iter;
        // reply contains {value: {seq: N}, ...} or {lastErrorObject: {updatedExisting: true, n: 1}, value: {seq: N}}
        // Try to find "value" first, then fall back to top-level "seq"
        if (bson_iter_init_find(&iter, &reply, "value") && BSON_ITER_HOLDS_DOCUMENT(&iter)) {
            const uint8_t* val_data = nullptr;
            uint32_t val_len = 0;
            bson_iter_document(&iter, &val_len, &val_data);
            if (val_data && val_len > 0) {
                bson_t val_doc;
                bson_init_static(&val_doc, val_data, val_len);
                bson_iter_t val_iter;
                if (bson_iter_init_find(&val_iter, &val_doc, "seq")) {
                    new_seq = bson_iter_int64(&val_iter);
                }
            }
        }
    } else {
        SPDLOG_ERROR("[MongoServer]alloc_player_ids failed: {}", error.message);
    }

    bson_destroy(&reply);
    bson_destroy(&opts);
    bson_destroy(&update);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);

    if (new_seq > 0) {
        SPDLOG_INFO("[MongoServer]Allocated {} player IDs, seq now at {}", count, new_seq);
    }

    return new_seq;
}
```

- [x] **Step 3: Commit**

```bash
git add scripts/server/dbmgr/src/mongo_server.h scripts/server/dbmgr/src/mongo_server.cpp
git commit -m "feat(dbmgr): add MongoDB counter-based player ID allocation"
```

---

### Task 3: DBMgr - Server Message Handler

**Files:**
- Modify: `scripts/server/dbmgr/src/dbmgr_server.h`
- Modify: `scripts/server/dbmgr/src/dbmgr_server.cpp`

- [x] **Step 1: Add declaration to `dbmgr_server.h`**

Add after `handle_account_set_req` declaration:

```cpp
    void handle_alloc_player_id_req(std::shared_ptr<GameSession> session,
                                    const std::vector<uint8_t>& payload);
```

- [x] **Step 2: Add route to `route_message()` in `dbmgr_server.cpp`**

In the `switch (msg_id)` block (around line 297), add a new case before `default`:

```cpp
        case MSG_ID_ALLOC_PLAYER_ID_REQ:
            handle_alloc_player_id_req(session, payload);
            break;
```

- [x] **Step 3: Implement `handle_alloc_player_id_req()`**

Add before the `send_to_game` section (around line 502):

```cpp
void DbMgrServer::handle_alloc_player_id_req(std::shared_ptr<GameSession> session,
                                              const std::vector<uint8_t>& payload) {
    farm::AllocPlayerIdReq req;
    if (!payload.empty() && !req.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[DBMgr]Failed to parse AllocPlayerIdReq");
        return;
    }

    uint32_t count = req.count();
    if (count == 0) count = 64;  // default batch size

    SPDLOG_INFO("[DBMgr]AllocPlayerIdReq request_id={} count={}", req.request_id(), count);

    farm::AllocPlayerIdResp resp;
    resp.set_request_id(req.request_id());

    if (!is_db_ready()) {
        resp.set_code(-1);
        resp.set_start_id(0);
        resp.set_count(0);
        SPDLOG_ERROR("[DBMgr]DB not ready for AllocPlayerIdReq");
    } else {
        int64_t start_id = mongo_server_.alloc_player_ids(count);
        if (start_id > 0) {
            resp.set_code(0);
            resp.set_start_id(start_id);
            resp.set_count(count);
        } else {
            resp.set_code(-1);
            resp.set_start_id(0);
            resp.set_count(0);
            SPDLOG_ERROR("[DBMgr]Failed to allocate player IDs");
        }
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_to_game(session, MSG_ID_ALLOC_PLAYER_ID_RESP, resp_data);
}
```

- [x] **Step 4: Commit**

```bash
git add scripts/server/dbmgr/src/dbmgr_server.h scripts/server/dbmgr/src/dbmgr_server.cpp
git commit -m "feat(dbmgr): handle AllocPlayerIdReq message"
```

---

### Task 4: Game Server - DBMgrConnectionManager ID Allocation

**Files:**
- Modify: `scripts/server/game_server/src/dbmgr_connection_manager.h`
- Modify: `scripts/server/game_server/src/dbmgr_connection_manager.cpp`

- [x] **Step 1: Add callback type and declarations to header**

In `dbmgr_connection_manager.h`, add after the `AccountSetCallback` typedef:

```cpp
// Callback for async AllocPlayerIdResp: (code, start_id, count)
using AllocPlayerIdCallback = std::function<void(int32_t code, uint64_t start_id, uint32_t count)>;
```

Add a new pending request struct after `PendingAccountRequest`:

```cpp
// Pending ID allocation request tracking
struct PendingAllocIdRequest {
    uint64_t request_id;
    uint32_t dbmgr_index;
    AllocPlayerIdCallback callback;
    time_t send_time;
};
```

Add public method declaration after `send_account_set_req`:

```cpp
    /**
     * @brief Send an AllocPlayerIdReq to DBMgr.
     * @param count     Number of IDs to allocate
     * @param callback  Called when response arrives
     * @return request_id (>0) on success, 0 on failure
     */
    uint64_t send_alloc_player_id_req(uint32_t count, AllocPlayerIdCallback callback);
```

Add private method declaration after `handle_account_set_resp`:

```cpp
    void handle_alloc_player_id_resp(DBMgrConnection* conn,
                                     const std::vector<uint8_t>& payload);
```

Add member after `pending_account_requests_`:

```cpp
    // Async ID allocation request tracking: request_id -> PendingAllocIdRequest
    std::unordered_map<uint64_t, PendingAllocIdRequest> pending_alloc_id_requests_;
```

- [x] **Step 2: Add route to `route_message()` in `dbmgr_connection_manager.cpp`**

In the `switch (msg_id)` block (around line 246), add a new case before `default`:

```cpp
        case MSG_ID_ALLOC_PLAYER_ID_RESP:
            handle_alloc_player_id_resp(conn, payload);
            break;
```

- [x] **Step 3: Implement `send_alloc_player_id_req()`**

Add after the `send_account_set_req` method (around line 624):

```cpp
uint64_t DBMgrConnectionManager::send_alloc_player_id_req(uint32_t count,
                                                            AllocPlayerIdCallback callback) {
    if (connections_.empty()) {
        SPDLOG_ERROR("[DBMgrConnection]No DBMgr connections configured");
        if (callback) callback(-1, 0, 0);
        return 0;
    }

    // Route to first DBMgr (index 0)
    uint32_t dbmgr_index = 0;

    if (dbmgr_index >= connections_.size() || !connections_[dbmgr_index]) {
        SPDLOG_ERROR("[DBMgrConnection]Invalid dbmgr_index={}", dbmgr_index);
        if (callback) callback(-1, 0, 0);
        return 0;
    }

    auto& conn = connections_[dbmgr_index];
    if (conn->state() != DBMgrConnectionState::IDENTIFIED) {
        SPDLOG_ERROR("[DBMgrConnection]DBMgr index={} not connected", dbmgr_index);
        if (callback) callback(-1, 0, 0);
        return 0;
    }

    // Generate request_id
    uint64_t request_id = generate_request_id();

    // Build AllocPlayerIdReq
    farm::AllocPlayerIdReq req;
    req.set_request_id(request_id);
    req.set_count(count);

    std::string req_data;
    req.SerializeToString(&req_data);

    // Register pending request
    PendingAllocIdRequest pending;
    pending.request_id = request_id;
    pending.dbmgr_index = dbmgr_index;
    pending.callback = std::move(callback);
    pending.send_time = std::time(nullptr);
    pending_alloc_id_requests_[request_id] = std::move(pending);

    // Send
    send_to_dbmgr(conn.get(), MSG_ID_ALLOC_PLAYER_ID_REQ, req_data);

    SPDLOG_INFO("[DBMgrConnection]Sent AllocPlayerIdReq request_id={} count={}", request_id, count);
    return request_id;
}
```

- [x] **Step 4: Implement `handle_alloc_player_id_resp()`**

Add after `handle_account_set_resp` (around the same area):

```cpp
void DBMgrConnectionManager::handle_alloc_player_id_resp(DBMgrConnection* conn,
                                                           const std::vector<uint8_t>& payload) {
    farm::AllocPlayerIdResp resp;
    if (!payload.empty() && !resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[DBMgrConnMgr]Failed to parse AllocPlayerIdResp");
        return;
    }

    uint64_t request_id = resp.request_id();

    auto it = pending_alloc_id_requests_.find(request_id);
    if (it == pending_alloc_id_requests_.end()) {
        SPDLOG_INFO("[DBMgrConnection]No pending alloc_id request for request_id={}", request_id);
        return;
    }

    // Invoke callback
    if (it->second.callback) {
        it->second.callback(resp.code(), resp.start_id(), resp.count());
    }

    // Remove from pending
    pending_alloc_id_requests_.erase(it);
}
```

- [x] **Step 5: Commit**

```bash
git add scripts/server/game_server/src/dbmgr_connection_manager.h scripts/server/game_server/src/dbmgr_connection_manager.cpp
git commit -m "feat(game): add AllocPlayerIdReq/Resp to DBMgrConnectionManager"
```

---

### Task 5: Game Server - PlayerIdPool

**Files:**
- Create: `scripts/server/game_server/src/player_id_pool.h`

- [x] **Step 1: Create `player_id_pool.h`**

```cpp
#pragma once

#include "dbmgr_connection_manager.h"
#include "log_macros.h"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstdint>

namespace farm {

class PlayerIdPool {
public:
    PlayerIdPool(DBMgrConnectionManager* dbmgr_mgr, uint32_t batch_size = 64)
        : dbmgr_mgr_(dbmgr_mgr)
        , batch_size_(batch_size)
        , fetching_(false)
    {
    }

    ~PlayerIdPool() = default;

    // 获取一个 player_id（池耗尽时同步等待）
    uint64_t acquire() {
        std::unique_lock<std::mutex> lock(mutex_);

        // 如果池为空，等待补充
        while (pool_.empty()) {
            // 触发一次预取
            if (!fetching_) {
                fetching_ = true;
                lock.unlock();
                dbmgr_mgr_->send_alloc_player_id_req(batch_size_,
                    [this](int32_t code, uint64_t start_id, uint32_t count) {
                        on_alloc_response(code, start_id, count);
                    });
                lock.lock();
            }
            cv_.wait(lock);
        }

        uint64_t id = pool_.front();
        pool_.pop();

        // 检查是否需要补充（剩余 < 30%）
        if (!fetching_ && pool_.size() < batch_size_ * 30 / 100) {
            fetching_ = true;
            lock.unlock();
            dbmgr_mgr_->send_alloc_player_id_req(batch_size_,
                [this](int32_t code, uint64_t start_id, uint32_t count) {
                    on_alloc_response(code, start_id, count);
                });
            lock.lock();
        }

        return id;
    }

    // 预取回调（DBMgr 响应后调用）
    void on_alloc_response(int32_t code, uint64_t start_id, uint32_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        fetching_ = false;

        if (code != 0 || count == 0) {
            SPDLOG_ERROR("[PlayerIdPool]Alloc failed, code={}", code);
            cv_.notify_all();  // 唤醒等待者（它们会重试）
            return;
        }

        // 将分配的 ID 逐个加入池
        for (uint32_t i = 0; i < count; ++i) {
            pool_.push(start_id + i);
        }

        SPDLOG_INFO("[PlayerIdPool]Replenished {} IDs, pool size now {}", count, pool_.size());

        // 唤醒等待的 acquire()
        cv_.notify_all();
    }

    // 当前池大小（用于监控/调试）
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

private:
    DBMgrConnectionManager* dbmgr_mgr_;
    uint32_t batch_size_;
    std::queue<uint64_t> pool_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool fetching_;
};

}  // namespace farm
```

- [x] **Step 2: Commit**

```bash
git add scripts/server/game_server/src/player_id_pool.h
git commit -m "feat(game): add PlayerIdPool with batch pre-allocation"
```

---

### Task 6: Game Server - LoginStub Refactor

**Files:**
- Modify: `scripts/server/game_server/src/login_stub.h`
- Modify: `scripts/server/game_server/src/login_stub.cpp`

- [x] **Step 1: Update `login_stub.h`**

Replace `#include "player_id_generator.h"` with `#include "player_id_pool.h"`.

Change the constructor parameter list — add `PlayerIdPool* id_pool` parameter (keep `server_id` as it's used by `mark_player_online()`):

```cpp
    LoginStub(PlayerManager* player_mgr,
              DBMgrConnectionManager* dbmgr_mgr,
              RedisConnection* redis_conn,
              uint32_t server_id,
              PlayerIdPool* id_pool,
              SendToGateFunc send_to_gate);
```

Replace the member `PlayerIdGenerator player_id_gen_;` with:

```cpp
    PlayerIdPool* id_pool_;
```

- [x] **Step 2: Update `login_stub.cpp` constructor**

Update the constructor to add `id_pool` parameter (keep `server_id`):

```cpp
LoginStub::LoginStub(PlayerManager* player_mgr,
                     DBMgrConnectionManager* dbmgr_mgr,
                     RedisConnection* redis_conn,
                     uint32_t server_id,
                     PlayerIdPool* id_pool,
                     SendToGateFunc send_to_gate)
    : player_mgr_(player_mgr)
    , dbmgr_mgr_(dbmgr_mgr)
    , redis_conn_(redis_conn)
    , server_id_(server_id)
    , id_pool_(id_pool)
    , send_to_gate_(std::move(send_to_gate))
{
}
```

- [x] **Step 3: Update `handle_create_role()`**

In `handle_create_role()`, replace:

```cpp
    // Generate player_id
    uint64_t player_id = player_id_gen_.generate(server_id);
```

with:

```cpp
    // Acquire player_id from global pool
    uint64_t player_id = id_pool_->acquire();
```

- [x] **Step 4: Commit**

```bash
git add scripts/server/game_server/src/login_stub.h scripts/server/game_server/src/login_stub.cpp
git commit -m "refactor(game): replace PlayerIdGenerator with PlayerIdPool in LoginStub"
```

---

### Task 7: Game Server - GameServer Integration

**Files:**
- Modify: `scripts/server/game_server/src/game_server.h`
- Modify: `scripts/server/game_server/src/game_server.cpp`

- [x] **Step 1: Add `PlayerIdPool` include and member to `game_server.h`**

Add include:

```cpp
#include "player_id_pool.h"
```

Add member (after `DBMgrConnectionManager dbmgr_mgr_`):

```cpp
    // Player ID pool (global unique ID allocation)
    std::unique_ptr<PlayerIdPool> id_pool_;
```

- [x] **Step 2: Create `PlayerIdPool` in `game_server.cpp` `start()`**

In the `start()` method, after the DBMgr connection manager init block (around line 113) and before the `send_to_gate_func` lambda, add:

```cpp
    // 初始化 Player ID 池
    if (!dbmgr_configs_.empty()) {
        id_pool_ = std::make_unique<PlayerIdPool>(&dbmgr_mgr_);
        SPDLOG_INFO("[Game]PlayerIdPool initialized");
    }
```

- [x] **Step 3: Update LoginStub construction**

Change the LoginStub creation line (around line 142):

From:
```cpp
    login_stub_ = std::make_unique<LoginStub>(&player_mgr_, &dbmgr_mgr_, &redis_conn_, server_id_, send_to_gate_func);
```

To:
```cpp
    login_stub_ = std::make_unique<LoginStub>(&player_mgr_, &dbmgr_mgr_, &redis_conn_, server_id_, id_pool_.get(), send_to_gate_func);
```

- [x] **Step 4: Commit**

```bash
git add scripts/server/game_server/src/game_server.h scripts/server/game_server/src/game_server.cpp
git commit -m "feat(game): integrate PlayerIdPool into GameServer and LoginStub"
```

---

### Task 8: Cleanup and Build Verification

**Files:**
- Delete: `scripts/server/game_server/src/player_id_generator.h`

- [x] **Step 1: Verify no remaining references to `player_id_generator.h`**

```bash
grep -r "player_id_generator" scripts/server/
```

Expected: No results (all references removed in Task 6).

- [x] **Step 2: Delete `player_id_generator.h`**

```bash
git rm scripts/server/game_server/src/player_id_generator.h
```

- [x] **Step 3: Build DBMgr**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/dbmgr
cmake --build build --config Release
```

Expected: Build succeeds with no errors.

- [x] **Step 4: Build Game Server**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server
cmake --build build --config Release
```

Expected: Build succeeds with no errors.

- [x] **Step 5: Commit**

```bash
git add -A
git commit -m "chore: remove player_id_generator.h, verify build"
```

---

### Task 9: Integration Wiring - DBMgr Counter Init

**Files:**
- Modify: `scripts/server/dbmgr/src/dbmgr_server.cpp`

- [x] **Step 1: Call `init_counter()` after MongoDB init**

In `dbmgr_server.cpp`, in the `start()` method, after `mongo_server_.init()` succeeds (around line 45-48), add:

```cpp
        if (!mongo_server_.init()) {
            SPDLOG_ERROR("[DBMgr]Failed to initialize MongoDB indexes");
            return false;
        }
        // 初始化 player_id 计数器
        if (!mongo_server_.init_counter()) {
            SPDLOG_ERROR("[DBMgr]Failed to initialize player ID counter");
            return false;
        }
        SPDLOG_INFO("[DBMgr]Database ready");
```

Also add in the recovery callback (around line 53-56):

```cpp
        conn_mgr_.set_on_ready_callback([this]() {
            if (mongo_server_.init() && mongo_server_.init_counter()) {
                SPDLOG_INFO("[DBMgr]Database recovered and ready");
            }
        });
```

- [x] **Step 2: Commit**

```bash
git add scripts/server/dbmgr/src/dbmgr_server.cpp
git commit -m "feat(dbmgr): initialize player ID counter on startup"
```

---

### Task 10: Update Sample Data

**Files:**
- Modify: `data/accounts/0.json`

- [x] **Step 1: Update sample account data**

The sample data currently has `player_id: 1048577` (old format: `1 << 20 | 1`). Update to the new format:

```json
{"account_id":"test_account","roles":[{"server_id":1,"player_id":1,"role_name":"TestFarmer"}]}
```

- [x] **Step 2: Commit**

```bash
git add data/accounts/0.json
git commit -m "chore: update sample account data to new player_id format"
```
