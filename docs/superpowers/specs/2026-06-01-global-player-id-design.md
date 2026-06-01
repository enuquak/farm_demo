# 全局唯一 Player ID 设计方案

## 背景

当前 `PlayerIdGenerator` 在 Game Server 内存中使用 `(server_id << 20) | sequence` 格式生成 player_id，存在两个问题：
1. 无持久化：服务器重启后 sequence 重置为 1，会与已有的玩家 ID 冲突
2. 无全局协调：虽然位移理论上保证跨服唯一，但重启后无法保证

**目标**：所有服务器的 player_id 全局唯一，使用纯自增序号（1, 2, 3, ...）格式。

## 方案概述

DBMgr 集中式 ID 分配 + 批量预取。DBMgr 启动时从 MongoDB 原子预取一批 ID 缓存在本地，分配给 Game Server。本地池剩余低于 30% 时自动异步补充。

保证全局唯一的核心是 MongoDB 的 `findOneAndUpdate` 原子操作，而非 DBMgr 本身——即使多个 DBMgr 同时请求，MongoDB 也会串行处理，返回不重叠的 ID 区间。

## 架构与数据流

```
DBMgr 启动
  └→ MongoDB(counters: findOneAndUpdate, $inc by 64)
  └→ 本地 ID 池: [1..64], 剩余 64

玩家注册分配 ID:
  └→ 从本地池取一个 (如 player_id=1), 剩余 63
  └→ 剩余 63/64 = 98% > 30% → 不触发预取
  └→ ...消耗到剩余 19/64 = 29% < 30% → 异步触发预取下一批 64 个
  └→ DBMgr 拿到 [65..128], 追加到本地池
  └→ 本地池变为 [20..128], 剩余 109

本地池耗尽时（极端情况，预取还没回来）:
  └→ 同步阻塞等待下一批 ID 到达
```

### 多服务器场景

```
Server 1 的 DBMgr → MongoDB(counters, $inc by 64)
  → 返回 [1..64]

Server 2 的 DBMgr → MongoDB(counters, $inc by 64)
  → 返回 [65..128]

Server 3 的 DBMgr → MongoDB(counters, $inc by 64)
  → 返回 [129..192]
```

## MongoDB counters 集合

```json
// 集合名: counters
{
    "_id": "player_id",
    "seq": 192
}
```

- 操作：`findOneAndUpdate({_id: "player_id"}, {$inc: {seq: 64}}, {upsert: true, returnDocument: "after"})`
- 返回更新后的 `seq` 值，本地分配 `[seq - count + 1 .. seq]`
- `upsert: true` 保证首次调用时自动创建文档
- DBMgr 启动时调用 `init_counter()` 确保文档存在，不重置 seq

## Proto 消息定义

`scripts/common/proto/dbmgr.proto` 新增：

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

## 消息 ID

`scripts/server/common/include/internal_msg_ids.h` 新增：

```cpp
inline constexpr uint32_t MSG_ID_ALLOC_PLAYER_ID_REQ   = 4205;
inline constexpr uint32_t MSG_ID_ALLOC_PLAYER_ID_RESP   = 4206;
```

## DBMgr 端实现

### MongoServer 新增

```cpp
// 初始化 counters 集合（upsert 确保文档存在）
bool init_counter();

// 原子分配 N 个 player_id，返回起始 ID
// 成功: 返回 start_id (>0), 失败: 返回 0
int64_t alloc_player_ids(uint32_t count);
```

- `alloc_player_ids()` 内部用 `mongoc_collection_find_and_modify` 执行 `$inc: {seq: count}`
- `start_id = returned_seq - count + 1`

### DbMgrServer 新增

```cpp
void handle_alloc_player_id_req(std::shared_ptr<GameSession> session,
                                const std::vector<uint8_t>& payload);
```

- 在 `route_message()` 中添加 `MSG_ID_ALLOC_PLAYER_ID_REQ` 路由
- 调用 `mongo_server_.alloc_player_ids(req.count())`，构建 `AllocPlayerIdResp` 返回

## Game Server 端实现

### DBMgrConnectionManager 新增

```cpp
using AllocPlayerIdCallback = std::function<void(int32_t code, uint64_t start_id, uint32_t count)>;

uint64_t send_alloc_player_id_req(uint32_t count, AllocPlayerIdCallback callback);

void handle_alloc_player_id_resp(DBMgrConnection* conn,
                                 const std::vector<uint8_t>& payload);
```

- 异步请求/响应，复用现有的 pending request 机制
- 路由到 `dbmgr_index = 0`

### 新增 PlayerIdPool 类

`scripts/server/game_server/src/player_id_pool.h`：

```cpp
class PlayerIdPool {
public:
    PlayerIdPool(DBMgrConnectionManager* dbmgr_mgr, uint32_t batch_size = 64);

    // 获取一个 player_id（池耗尽时同步等待）
    uint64_t acquire();

    // 预取回调（DBMgr 响应后调用）
    void on_alloc_response(uint64_t start_id, uint32_t count);

private:
    void try_replenish();  // 检查是否需要预取

    DBMgrConnectionManager* dbmgr_mgr_;
    uint32_t batch_size_;
    std::queue<uint64_t> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool fetching_ = false;
};
```

- `acquire()`：从 `pool_` 取一个 ID；池空时 `cv_.wait()` 等待
- `on_alloc_response()`：追加 ID 到池，剩余 < 30% 时触发异步预取
- `try_replenish()`：向 DBMgr 发送 `send_alloc_player_id_req`

### LoginStub 改造

- 移除 `PlayerIdGenerator player_id_gen_`
- 新增 `PlayerIdPool* id_pool_`（由 GameServer 创建并传入）
- `handle_create_role()` 中：`uint64_t player_id = id_pool_->acquire();`

## 错误处理

| 场景 | 处理方式 |
|------|----------|
| DBMgr 未连接 | `acquire()` 池空 + DBMgr 不可用 → 同步等待，超时后返回错误 |
| DBMgr 重启 | 重连后主动触发一次预取补充池 |
| MongoDB 不可用 | `alloc_player_ids()` 返回错误 → 不追加到池，可选重试 |
| ID 间隔 | 重启导致未用完的 ID 段被跳过，对功能无影响 |

## 修改文件清单

| 文件 | 操作 |
|------|------|
| `scripts/common/proto/dbmgr.proto` | 新增 AllocPlayerIdReq/Resp 消息 |
| `scripts/server/common/include/internal_msg_ids.h` | 新增 MSG_ID_ALLOC_PLAYER_ID_REQ/RESP |
| `scripts/server/dbmgr/src/mongo_server.h` | 新增 init_counter(), alloc_player_ids() |
| `scripts/server/dbmgr/src/mongo_server.cpp` | 实现上述方法 |
| `scripts/server/dbmgr/src/dbmgr_server.h` | 新增 handle_alloc_player_id_req() |
| `scripts/server/dbmgr/src/dbmgr_server.cpp` | 实现消息路由和处理 |
| `scripts/server/game_server/src/player_id_pool.h` | 新增 PlayerIdPool 类 |
| `scripts/server/game_server/src/dbmgr_connection_manager.h` | 新增 AllocPlayerIdCallback, send_alloc_player_id_req() |
| `scripts/server/game_server/src/dbmgr_connection_manager.cpp` | 实现 ID 分配请求/响应 |
| `scripts/server/game_server/src/login_stub.h` | 移除 PlayerIdGenerator, 改用 PlayerIdPool |
| `scripts/server/game_server/src/login_stub.cpp` | 改造 handle_create_role() |
| `scripts/server/game_server/src/game_server.h` | 新增 PlayerIdPool 成员 |
| `scripts/server/game_server/src/game_server.cpp` | 创建并注入 PlayerIdPool |
| `scripts/server/game_server/src/player_id_generator.h` | 删除（不再使用） |
