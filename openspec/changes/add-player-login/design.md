## Context

当前系统由 Gate Server、Game Server 和 DBMgr（开发中）三个进程组成。Gate 主动连接 Game，通过 protobuf 消息进行通信。Game Server 使用 libevent 单线程事件循环，具备完整的连接管理、心跳保活、消息路由能力。

目前登录流程使用临时的 fd 作为 player_id，没有完整的账号和角色系统。需要实现支持多服务器、多角色的完整登录流程。

## Goals / Non-Goals

**Goals:**

- 实现完整的账号+角色登录系统
- 支持一个账号在多个服务器拥有不同角色
- 支持查询角色列表和创建新角色
- 通过 DBMgr 进行账号和角色数据持久化
- 使用 JSON 格式存储数据，便于后期迁移到 MongoDB

**Non-Goals:**

- 不实现账号注册和密码验证（简化为用户名直接登录）
- 不实现角色删除功能
- 不实现跨服务器角色迁移
- 不实现角色数据缓存

## Decisions

### 1. 数据模型：JSON 结构

**决策**: 使用 JSON 结构作为账号和角色的数据模型，直接存储到文件系统。

**账号数据结构**:
```json
{
  "account_id": "user_abc",
  "roles": [
    { "server_id": 1, "player_id": 1000001, "role_name": "farmer_1" },
    { "server_id": 2, "player_id": 2000001, "role_name": "farmer_2" }
  ]
}
```

**角色数据结构**:
```json
{
  "player_id": 1000001,
  "role_name": "farmer_1",
  "level": 10,
  "position": { "x": 100.0, "y": 0.0, "z": 200.0 },
  "scene_id": "farm_001"
}
```

**理由**:
- 与 MongoDB 的 BSON 格式高度兼容（JSON 是 BSON 的子集）
- 初期用 JSON 文件存储时无需转换
- 迁移到 MongoDB 时几乎可以直接使用

### 2. player_id 生成策略

**决策**: 使用 `server_id * 1,000,000 + timestamp + random` 生成 player_id。

**示例**:
- Server 1: 1,000,001, 1,000,002, 1,000,003...
- Server 2: 2,000,001, 2,000,002, 2,000,003...
- Server 3: 3,000,001, 3,000,002, 3,000,003...

**理由**:
- 简单、确定性强
- 通过 player_id 可以直接判断所属服务器
- 时间戳+随机数保证唯一性

### 3. 消息协议：AccountMsg 和 PlayerMsg

**决策**: 新增 AccountMsg 和 PlayerMsg 两种消息类型，替代原有的 Packet。

**AccountMsg**: 账号层级操作（查询角色、创建角色）
**PlayerMsg**: 玩家层级操作（登录进入游戏、游戏操作）

**消息 ID 范围**:
- 账号相关: 1000-1999
- 玩家相关: 2000-2999
- 内部消息: 3000-3299（已有）
- DBMgr 管理: 4000-4099（已有）
- DBMgr 账号数据: 4100-4199
- DBMgr 角色数据: 4200-4299（已有）

**理由**:
- 业务语义更清晰
- 便于消息路由和处理

### 4. DBMgr 数据存储：账号和角色分开路由

**决策**: 账号数据和角色数据存在同一批 DBMgr 进程中，但使用不同的路由策略。

**账号路由**: `hash(account_id) % dbmgr_count`
**角色路由**: `player_id % dbmgr_count`

**理由**:
- 账号数据和角色数据的访问模式不同
- 账号查询需要根据 account_id 路由
- 角色查询需要根据 player_id 路由
- 同一批 DBMgr 进程简化部署

### 5. Gate 路由逻辑

**决策**: Gate 根据消息类型和 server_id 路由消息到对应的 Game Server。

**账号消息**: 转发给任意一个 Game Server（轮询或随机）
**玩家消息**: 根据 session 中记录的 server_id 路由到对应 Game Server

**理由**:
- 账号消息不依赖特定服务器
- 玩家消息需要路由到玩家所在的服务器

### 6. 登录流程

**决策**: 完整的登录流程包括查询角色列表、选择角色、进入游戏三个步骤。

**流程**:
1. 客户端发送 AccountMsg(QueryRolesReq) 查询角色列表
2. 如果有角色，返回角色列表，客户端选择角色
3. 如果无角色，返回服务器列表，客户端创建新角色
4. 客户端发送 PlayerMsg(EnterGameReq) 进入游戏
5. Game Server 加载角色数据，返回 EnterGameResp

**理由**:
- 支持多服务器、多角色场景
- 用户体验友好

## Risks / Trade-offs

**[player_id 生成冲突]** → 时间戳+随机数理论上可能冲突，但概率极低。缓解：可以在 DBMgr 层添加唯一性检查。

**[账号数据并发写入]** → 同一账号可能同时在多个服务器创建角色，导致并发写入。缓解：DBMgr 层面可以使用文件锁或乐观锁。

**[Gate 配置静态]** → server_id 到 Game Server 的映射是静态配置，扩缩容需要修改配置。缓解：当前阶段不需要动态扩缩容。

**[JSON 文件读写性能]** → 每次账号查询都是全文件读写。缓解：初期数据量小，问题不大；切 MongoDB 后彻底解决。

## Open Questions

- 是否需要实现账号注册和密码验证？（当前简化为用户名直接登录）
- 是否需要实现角色删除功能？
- 是否需要实现角色数据缓存？
