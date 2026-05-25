## Context

当前系统由 Gate Server 和 Game Server 两个进程组成，Gate 主动连接 Game，通过 protobuf 消息进行通信。Game Server 使用 libevent 单线程事件循环，具备完整的连接管理、心跳保活、消息路由能力。

Game Server 目前没有数据持久化层，玩家数据仅存在于内存中，进程重启后丢失。需要引入 DBMgr 进程作为数据存取代理，为 Game 提供统一的读写接口。

## Goals / Non-Goals

**Goals:**

- 为 Game Server 提供数据持久化能力，支持玩家数据的增删改查
- DBMgr 作为纯数据代理，不包含任何业务逻辑
- 支持多 DBMgr 实例，通过 hash 路由实现数据分片
- 采用异步请求模型，不阻塞 Game 的事件循环
- 先用本地 JSON 文件存储，架构设计允许后续平滑切换到 MongoDB

**Non-Goals:**

- 不实现跨 DBMgr 的数据复制或自动故障转移
- 不在 DBMgr 层做数据校验、迁移或版本管理
- 不实现动态扩缩容（DBMgr 实例数量写死在配置中）
- 不实现数据缓存层

## Decisions

### 1. 连接方向：Game 主动连接 DBMgr

**决策**: Game 主动发起 TCP 连接到所有 DBMgr 实例，与 Gate→Game 的模式一致。

**理由**:
- 保持架构一致性，Gate→Game 和 Game→DBMgr 都是"上游主动连下游"
- Game 是请求发起方，主动连接更自然
- DBMgr 作为被动服务端，启动后等待连接即可

**替代方案**: DBMgr 主动连接 Game —— 不采用，因为 DBMgr 不知道 Game 的地址，且连接管理放在 Game 侧更合理（Game 需要知道所有 DBMgr 来做路由）。

### 2. 身份标识与心跳

**决策**: 连接建立后，DBMgr 主动发送 `DBMgrIdentify`（携带 index 和 address），Game 回复确认。心跳由 DBMgr 定期发送给 Game。

**理由**:
- 与 Gate→Game 的 `GateIdentify` 模式完全一致，降低理解成本
- DBMgr 作为"服务提供方"，由它维持连接活性更合理
- Game 侧检测心跳超时后标记该 DBMgr 为断开

### 3. 消息协议：异步请求/响应

**决策**: Game 发送 `PlayerDataReq`（携带 request_id），DBMgr 处理后回复 `PlayerDataResp`（携带相同的 request_id）。Game 侧维护 `pending_requests` 映射表，通过 request_id 匹配回调。

**理由**:
- libevent 单线程模型下不能阻塞等待响应
- request_id 机制简单可靠，不需要复杂的异步框架
- 支持同一 DBMgr 连接上同时有多个 in-flight 请求

**request_id 生成**: 使用自增计数器，64 位整型，进程生命周期内唯一。

```
Game 内部:
┌────────────────────────────────────────────┐
│ pending_db_requests_:                      │
│   request_id=1001 → callback(加载玩家数据)  │
│   request_id=1002 → callback(保存背包)      │
│   request_id=1003 → callback(读取位置)      │
└────────────────────────────────────────────┘
```

### 4. 存储模型：每玩家一个 JSON 文件

**决策**: 每个玩家独立一个 JSON 文件，路径为 `data/players/{player_id}.json`，内容为嵌套 JSON 结构。DBMgr 不理解 value 内容，Game 侧负责序列化/反序列化。

**理由**:
- 与 hash 路由天然配合：每个 DBMgr 只持有属于自己的玩家文件
- 文件粒度的读写避免了单大文件的并发问题
- 嵌套 JSON 结构比扁平 key-value 更自然，Game 可以用 protobuf 序列化整个玩家数据后存为 value

**文件结构示例**:
```json
{
  "level": 10,
  "gold": 500,
  "inventory": [
    {"item_id": 1, "count": 3}
  ],
  "position": {"x": 100, "y": 200}
}
```

### 5. 路由策略：player_id % dbmgr_count

**决策**: Game 侧计算 `dbmgr_index = player_id % dbmgr_count`，将请求发送到对应的 DBMgr 连接。

**理由**:
- 简单、确定性强、无额外依赖
- 与"每玩家一个文件"的存储模型完美配合
- 扩缩容时需要数据迁移，但当前阶段不考虑动态扩缩容

### 6. 数据操作模型

**决策**: 提供五种操作：GET（读单个 key）、SET（写单个 key）、DEL（删单个 key）、GET_ALL（读全部）、SET_ALL（写全部）。

**理由**:
- 覆盖基本 CRUD 需求
- GET_ALL/SET_ALL 适合玩家登录时整体加载、离线时整体保存
- 单 key 操作适合运行时局部更新（如只更新金币）

**注意**: 初期 JSON 文件是整体读写的，单 key 操作在 DBMgr 内部也是读整个文件 → 修改 → 写回。后续切 MongoDB 后可以做到真正的单字段读写。

### 7. 故障处理

**决策**: DBMgr 连接断开时，Game 对该 DBMgr 负责的所有玩家请求直接返回失败，由上层业务决定如何处理（如提示玩家"服务器繁忙"）。Game 持续尝试重连。

**理由**:
- 不做自动故障转移需要数据复制，复杂度高
- 直接失败比静默丢数据更安全
- 重连机制保证服务最终可恢复

### 8. 配置管理

**决策**: 写死配置。DBMgr 通过启动参数指定 index 和 port。Game 配置文件中写死所有 DBMgr 的地址列表。

**DBMgr 启动参数**:
```
dbmgr.exe --index 0 --port 5000 --data-dir ./data
```

**Game 配置** (JSON):
```json
{
  "dbmgr_list": [
    {"index": 0, "ip": "127.0.0.1", "port": 5000},
    {"index": 1, "ip": "127.0.0.1", "port": 5001},
    {"index": 2, "ip": "127.0.0.1", "port": 5002}
  ]
}
```

### 9. 新玩家数据初始化

**决策**: Game 请求读取玩家数据时，如果 DBMgr 返回空（文件不存在），Game 自行构造初始数据，然后通过 SET_ALL 写回 DBMgr。

**理由**:
- DBMgr 保持纯代理角色，不涉及业务逻辑
- 初始数据的结构和默认值由 Game 业务层决定
- 后续如果需要更复杂的初始化逻辑（如新手引导数据），只需修改 Game 侧

## Risks / Trade-offs

**[DBMgr 单点故障导致部分玩家不可用]** → 每个 DBMgr 负责一批玩家，挂掉后这些玩家的数据请求全部失败。缓解：Game 持续重连，DBMgr 恢复后自动恢复服务。未来可通过数据复制提升可用性。

**[JSON 文件读写性能]** → 每次单 key 操作都是全文件读写，频繁更新会有 I/O 压力。缓解：初期数据量小，问题不大；切 MongoDB 后彻底解决。

**[hash 路由扩缩容困难]** → 增减 DBMgr 实例需要重新分配所有玩家数据。缓解：当前写死配置不支持动态扩缩容；未来迁移 MongoDB 时可利用其原生分片能力。

**[Game 侧 pending_requests 内存泄漏]** → 如果 DBMgr 异常断开，in-flight 请求的 callback 不会被调用。缓解：DBMgr 断开时，清理所有发往该 DBMgr 的 pending requests，调用 callback 返回失败。

## Open Questions

- DBMgr 进程是否需要支持多线程？当前设计为单线程 libevent 模型，与 Game 一致。如果单个 DBMgr 的 JSON 文件读写成为瓶颈，可考虑线程池，但初期不需要。
