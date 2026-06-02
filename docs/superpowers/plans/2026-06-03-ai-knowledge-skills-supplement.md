# AI Knowledge Skills 补充实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 18 个新子系统创建独立的 AI Knowledge Skill 文件，使 skill 总数从 10 增加到 28。

**Architecture:** 每个 skill 文件遵循统一结构（frontmatter → 概述 → 架构设计 → 关键流程 → 关键代码路径 → 常见陷阱 → 扩展指南 → 相关 Skill），内容从对应的 spec/plan 文档提炼，以"开发指南"形式组织。

**Tech Stack:** Markdown, Claude Code Skills

---

## 文件结构

```
.claude/skills/
├── shared-constants/SKILL.md              ← 新建
├── global-player-id/SKILL.md              ← 新建
├── etcd-service-discovery/SKILL.md        ← 新建
├── dbmgr-redis-mongo-enhancement/SKILL.md ← 新建
├── chat-system/SKILL.md                   ← 新建
├── friend-system/SKILL.md                 ← 新建
├── team-system/SKILL.md                   ← 新建
├── monster-combat/SKILL.md                ← 新建
├── quest-system/SKILL.md                  ← 新建
├── npc-dialog/SKILL.md                    ← 新建
├── activity-system/SKILL.md               ← 新建
├── player-load-path/SKILL.md              ← 新建
├── player-movement/SKILL.md               ← 新建
├── cross-server/SKILL.md                  ← 新建
├── client-notification/SKILL.md           ← 新建
├── config-editor/SKILL.md                 ← 新建
├── code-split-refactor/SKILL.md           ← 新建
└── ai-knowledge-skills/SKILL.md           ← 新建
```

---

### Task 1: 创建 `shared-constants`

**Files:**
- Create: `.claude/skills/shared-constants/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-01-shared-constants-design.md`
- `docs/superpowers/plans/2026-06-01-shared-constants.md`

- [ ] **Step 1: 读取源文档**

读取上述 2 个文档，提取共享常量系统的关键信息。

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/shared-constants
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/shared-constants/SKILL.md`，内容包含：

```markdown
---
name: shared-constants
description: 共享常量系统架构、消息 ID 管理、错误码管理、代码自动生成。
metadata:
  type: reference
---

# 共享常量系统

## 概述

项目使用共享常量系统确保 C++ 服务器和 Python 客户端使用相同的消息 ID 和错误码。常量定义在 `shared/` 目录下的 JSON 文件中，通过代码生成脚本自动同步到两端。

## 架构设计

### 数据流

```
shared/*.json → file_watcher.py → generate_constants.py → scripts/client/*.py
                                                           scripts/server/common/include/*.h
```

### 消息 ID 分区规则

| 范围 | 用途 |
|------|------|
| 1-999 | 客户端基础消息（心跳、登录） |
| 1001-1999 | 角色相关（查询、创建、进入游戏） |
| 2001-2999 | 场景/位置/时钟 |
| 3001-3099 | Gate-Game 连接管理 |
| 3100-3199 | 玩家生命周期 |
| 3200-3299 | 消息转发 |
| 3300-3399 | 账号消息转发 |
| 4000-4099 | Game-DBMgr 连接管理 |
| 4100-4199 | 数据操作 |
| 4200-4299 | 账号数据操作 |
| 5000-5999 | 管理消息（停服） |
| 6000-6999 | 聊天系统 |

## 关键流程

### 添加新消息 ID 的完整步骤

1. 在 `shared/message_ids.json` 中添加新条目：
   ```json
   { "code": 6001, "name": "MSG_ID_CHAT_MSG_REQ", "description": "聊天消息请求" }
   ```
2. 运行代码生成：
   ```bash
   python scripts/tools/generate_constants.py
   ```
3. 验证生成文件：
   - C++: `scripts/server/common/include/message_ids.h`
   - Python: `scripts/client/message_ids.py`
4. 在 `scripts/common/proto/` 中定义对应的 Protobuf 消息
5. 在服务器中注册消息 handler

### 添加新错误码的步骤

1. 在 `shared/error_codes.json` 中添加新条目
2. 运行代码生成
3. 在客户端和服务器中使用生成的常量

## 关键代码路径

- 共享常量 JSON：`shared/message_ids.json`, `shared/error_codes.json`
- 生成脚本：`scripts/tools/generate_constants.py`
- 文件监控：`scripts/tools/file_watcher.py`
- 生成的 C++ 头文件：`scripts/server/common/include/message_ids.h`, `error_codes.h`
- 生成的 Python 模块：`scripts/client/message_ids.py`, `scripts/client/error_codes.py`

## 常见陷阱

### 常量不同步

客户端和服务器使用不同的消息 ID，导致消息无响应：
- 修改 `shared/` 下的 JSON 后必须运行 `generate_constants.py`
- 使用 `tools/build_all.bat` 构建时会自动运行生成脚本

### ID 冲突

新添加的消息 ID 与已有 ID 冲突：
- 添加前检查 `shared/message_ids.json` 中是否已存在该 ID
- 遵守分区规则，不同功能使用不同范围

### 生成脚本未运行

修改 JSON 后忘记运行生成脚本：
- 启动文件监控 `python scripts/tools/file_watcher.py` 自动触发生成
- 或在构建脚本中确认包含生成步骤

## 扩展指南

### 添加新的共享常量类型

1. 在 `shared/` 目录下创建新的 JSON 文件（如 `item_ids.json`）
2. 在 `generate_constants.py` 中添加新文件的处理逻辑
3. 运行生成脚本验证输出

## 相关 Skill

- [[server-architecture]] — 服务器架构与消息协议
- [[client-architecture]] — 客户端架构与消息处理
```

- [ ] **Step 4: 验证**

检查文件格式正确，frontmatter 完整，交叉引用指向存在的 skill。

---

### Task 2: 创建 `global-player-id`

**Files:**
- Create: `.claude/skills/global-player-id/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-01-global-player-id-design.md`
- `docs/superpowers/plans/2026-06-01-global-player-id.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/global-player-id
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/global-player-id/SKILL.md`，内容包含：

```markdown
---
name: global-player-id
description: Player ID 生成策略、PlayerIdGenerator 类、ID 分配流程与容量规划。
metadata:
  type: reference
---

# 全局 Player ID 系统

## 概述

Player ID 使用 `(server_id << 20) | sequence` 格式生成，支持最多 4096 个服务器，每个服务器 1048576 个玩家。ID 全局唯一，由 Game Server 在创建角色时生成。

## 架构设计

### ID 格式

```
[server_id: 12 bit][sequence: 20 bit]
```

- server_id: 0-4095，标识服务器
- sequence: 0-1048575，每个服务器内的自增序列
- 最终 ID = (server_id << 20) | sequence

### PlayerIdGenerator 类

```cpp
class PlayerIdGenerator {
public:
    uint64_t generate(uint32_t server_id);

private:
    std::mutex mutex_;
    std::unordered_map<uint32_t, uint64_t> sequences_;
};
```

- 使用 `std::mutex` 保证线程安全
- 每个 server_id 独立的 sequence 计数器
- 首次调用某个 server_id 时从 1 开始

## 关键流程

### ID 分配流程

1. 客户端发送 CreateRoleReq（包含 server_id、role_name）
2. Game Server 调用 `player_id_generator.generate(server_id)`
3. 生成唯一 player_id
4. 使用 player_id 创建角色数据并保存到 DBMgr
5. 返回 CreateRoleResp（包含 player_id）

### 唯一性保证

- 同一 Game Server 内：mutex 保证 sequence 原子递增
- 不同 Game Server：server_id 不同，高位不同
- 重启后：sequence 从 DBMgr 中已有数据的最大值 +1 开始（TODO: 当前实现未处理）

## 关键代码路径

- ID 生成器：`scripts/server/game_server/src/player_id_generator.h`
- 使用位置：`scripts/server/game_server/src/game_server.cpp`（CreateRoleReq 处理）

## 常见陷阱

### ID 冲突

不同服务器生成相同 ID：
- 确保每个服务器配置不同的 server_id
- server_id 必须在 0-4095 范围内

### sequence 溢出

单个服务器玩家数超过 1048576：
- 当前设计不处理溢出情况
- 需要在运维层面监控玩家数量

### server_id 范围越界

server_id 超过 12 bit 范围：
- 配置校验应在服务启动时执行
- 超出范围会导致 ID 重叠

## 扩展指南

### 扩展 ID 容量

如需支持更多服务器或玩家：
1. 调整 server_id 和 sequence 的 bit 分配
2. 更新 PlayerIdGenerator 的位移常量
3. 确保所有使用 player_id 的代码兼容新格式

## 相关 Skill

- [[server-architecture]] — 服务器架构与配置
- [[player-persistence]] — 玩家数据持久化
```

- [ ] **Step 4: 验证**

---

### Task 3: 创建 `etcd-service-discovery`

**Files:**
- Create: `.claude/skills/etcd-service-discovery/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-01-etcd-service-discovery-design.md`
- `docs/superpowers/plans/2026-06-01-etcd-service-discovery.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/etcd-service-discovery
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/etcd-service-discovery/SKILL.md`，内容包含：

```markdown
---
name: etcd-service-discovery
description: etcd 服务发现集成、Lease 注册、Watch 发现、与 JSON 配置共存策略。
metadata:
  type: reference
---

# etcd 服务发现

## 概述

引入 etcd 作为服务注册中心和配置中心，实现进程动态发现与配置统一管理。与现有 JSON 配置系统共存，etcd 优先、JSON 降级。

## 架构设计

### etcd Key 结构

```
/farm/
├── services/                    # 服务注册（绑定 lease，自动过期）
│   ├── gate/{instance_id}       → {"ip":"0.0.0.0","port":8080,"status":"online"}
│   ├── game/{server_id}         → {"ip":"0.0.0.0","port":9090,"server_id":1}
│   └── dbmgr/{index}            → {"ip":"0.0.0.0","port":5000,"index":0}
│
└── config/                      # 配置中心（持久化，不绑定 lease）
    └── servers/
        ├── gate/{instance_id}
        ├── game/{server_id}
        └── dbmgr/{index}
```

### EtcdManager 接口

```cpp
class EtcdManager {
    bool connect();
    void shutdown();
    bool register_service(service_type, instance_id, value_json);
    void deregister_service(service_type, instance_id);
    std::vector<ServiceInstance> discover_services(service_type);
    void watch_services(service_type, callback);
    bool put_config(key, value_json);
    std::optional<std::string> get_config(key);
    void watch_config(key_prefix, callback);
};
```

## 关键流程

### 服务注册流程

1. 进程启动，连接 etcd
2. 创建 Lease（TTL 15 秒）
3. 将自身信息写入 `/farm/services/{type}/{id}`，绑定 Lease
4. 启动定期续约（每 10 秒续约一次）
5. 进程关闭时主动删除 key

### 服务发现流程

1. 启动时 Watch `/farm/services/{type}/` 前缀
2. 收到 PUT 事件：添加/更新服务实例
3. 收到 DELETE 事件：移除服务实例
4. 本地维护服务实例列表缓存

### 降级策略

1. 启动时尝试连接 etcd
2. 连接失败：使用 JSON 配置文件中的静态配置
3. 运行时 etcd 断开：继续使用本地缓存，定期尝试重连

## 关键代码路径

- etcd 客户端封装：`scripts/server/common/include/etcd_manager.h`
- 服务注册：各服务的 `main.cpp` 启动流程
- 配置：`config/*.json` 中的 etcd 配置段

## 常见陷阱

### Lease 过期

服务注册信息被自动删除：
- 确认定约线程正常运行
- 检查网络连通性
- TTL 不要设置过短

### Watch 断开重连

Watch 连接断开后丢失事件：
- 实现 Watch 的自动重连机制
- 重连后重新获取全量数据作为基线

### etcd 集群不可用

etcd 完全不可用时的处理：
- 启动时降级到 JSON 配置
- 运行时使用本地缓存
- 记录错误日志，定期重试

## 扩展指南

### 添加新的服务类型

1. 定义新的 service_type 常量
2. 在对应服务的 main.cpp 中添加注册逻辑
3. 在需要发现该服务的进程中添加 Watch 逻辑

## 相关 Skill

- [[server-architecture]] — 服务器架构与配置系统
```

- [ ] **Step 4: 验证**

---

### Task 4: 创建 `dbmgr-redis-mongo-enhancement`

**Files:**
- Create: `.claude/skills/dbmgr-redis-mongo-enhancement/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-dbmgr-redis-mongo-enhancement-design.md`
- `docs/superpowers/specs/2026-05-31-dbmgr-redis-mongo-design.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/dbmgr-redis-mongo-enhancement
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/dbmgr-redis-mongo-enhancement/SKILL.md`，内容包含：

```markdown
---
name: dbmgr-redis-mongo-enhancement
description: DBMgr 数据层增强、MongoDB 集成、Redis 缓存层、ConnectionManager 状态机。
metadata:
  type: reference
---

# DBMgr 数据层增强

## 概述

在现有 DBMgr JSON 文件存储基础上，引入 MongoDB 作为持久化存储、Redis 作为缓存层。本 skill 覆盖 Redis/Mongo 的详细设计，与 [[dbmgr-data-layer]] 互补。

## 架构设计

### 整体架构

```
Game Server
    │ TCP + Protobuf
    ↓
DBMgr
    ├── Redis（缓存层）
    │   └── 读穿透缓存、写穿透失效
    │
    └── MongoDB（持久化层）
        └── 集合：players, accounts
```

### ConnectionManager 状态机

```
DISCONNECTED → CONNECTING → CONNECTED
                          → FAILED → FAILED_PERMANENT
```

- FAILED 状态：后台线程自动重连（间隔 5 秒）
- FAILED_PERMANENT：重连次数超过阈值，停止重试
- ready 回调：连接就绪时通知上层

### 数据路由

```cpp
// 玩家数据路由
int target_dbmgr = player_id % dbmgr_count;

// 账户数据路由
int target_dbmgr = std::hash<std::string>{}(account_id) % dbmgr_count;
```

## 关键流程

### 读穿透流程

1. Game 请求玩家数据
2. DBMgr 先查 Redis 缓存
3. 缓存命中：直接返回
4. 缓存未命中：查 MongoDB → 写入 Redis（TTL 5 分钟）→ 返回

### 写穿透流程

1. Game 请求写入玩家数据
2. DBMgr 写入 MongoDB
3. 失效 Redis 缓存（DEL key）
4. 返回写入结果

## 关键代码路径

- MongoDB 连接：`scripts/server/dbmgr/src/mongo_connection.h/cpp`
- Redis 连接：`scripts/server/dbmgr/src/redis_connection.h/cpp`
- ConnectionManager：`scripts/server/dbmgr/src/connection_manager.h/cpp`
- 数据路由：`scripts/server/dbmgr/src/data_router.h/cpp`

## 常见陷阱

### 连接池耗尽

MongoDB/Redis 连接数超过限制：
- 监控连接池使用率
- 合理设置最大连接数
- 连接用完后及时释放

### 缓存一致性

Redis 缓存与 MongoDB 数据不一致：
- 写入时必须失效缓存
- 使用 TTL 作为兜底策略
- 避免在缓存中存储未持久化的数据

### 索引缺失

MongoDB 查询性能差：
- 为常用查询字段创建索引
- player_id 和 account_id 必须有索引

## 扩展指南

### 添加新的数据集合

1. 在 MongoDB 中创建新集合
2. 在 DataManager 中添加对应的 CRUD 方法
3. 定义 Protobuf 消息
4. 在 Game Server 中添加使用逻辑

## 相关 Skill

- [[dbmgr-data-layer]] — DBMgr 基础架构（JSON 文件存储）
- [[player-persistence]] — 玩家数据持久化机制
- [[server-architecture]] — 服务器架构与连接管理
```

- [ ] **Step 4: 验证**

---

### Task 5: 创建 `chat-system`

**Files:**
- Create: `.claude/skills/chat-system/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-chat-system-design.md`
- `docs/superpowers/plans/2026-06-02-chat-system.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/chat-system
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/chat-system/SKILL.md`，内容包含：

```markdown
---
name: chat-system
description: 聊天系统架构、Chat Server、频道类型、消息路由、客户端 UI。
metadata:
  type: reference
---

# 聊天系统

## 概述

聊天系统采用独立 Chat Server 架构，支持世界频道、场景频道、私聊频道和系统频道。GateServer 将聊天消息（msg_id 6000-6999）转发到 ChatServer 处理。

## 架构设计

### 整体架构

```
Client → Gate Server → Chat Server → Redis（历史消息）
                         ↓
                    Channel Manager
                    ├── 世界频道
                    ├── 场景频道
                    ├── 私聊频道
                    └── 系统频道
```

### 频道类型

| 类型 | 说明 | 订阅方式 |
|------|------|----------|
| 世界频道 | 全服广播 | 自动订阅 |
| 场景频道 | 同场景玩家 | 进入场景时订阅 |
| 私聊频道 | 点对点 | 发起私聊时创建 |
| 系统频道 | 系统公告 | 自动订阅 |

## 关键流程

### 消息发送流程

1. 客户端发送 ChatMsgReq（channel_type、content）
2. GateServer 解析 PlayerMsg，提取 player_id
3. 打包为 ClientMessage 发送给 ChatServer
4. ChatServer 验证、存储、广播到频道
5. 返回 ChatMsgResp 给发送者

### 消息接收流程

1. ChatServer 将消息推送到频道所有订阅者
2. 通过 GameMessage 格式发送给 GateServer
3. GateServer 查找玩家 Session，转发消息
4. 客户端渲染消息到 ChatPanel

## 关键代码路径

- Chat Server：`scripts/server/chat_server/src/`
- GateServer 路由：`scripts/server/gate_server/src/gate_server.cpp`（route_message 6000+ 分支）
- 客户端 UI：`scripts/client/ui/chat_panel.py`
- 频道配置：`scripts/client/chat_channel.py`
- Protobuf：`scripts/common/proto/chat.proto`

## 常见陷阱

### 消息顺序

消息到达顺序与发送顺序不一致：
- 使用时间戳排序
- 服务端分配序列号

### 频道订阅遗漏

玩家切换场景后未订阅新频道：
- 场景切换时自动退订旧频道、订阅新频道

### 客户端线程安全

网络线程和渲染线程同时访问消息队列：
- 使用线程安全队列
- 渲染线程从队列取消息时加锁

## 扩展指南

### 添加新频道类型

1. 在 Channel Manager 中添加新频道类型枚举
2. 实现频道的订阅/退订逻辑
3. 定义频道的消息路由规则
4. 更新客户端 UI 支持新频道

## 相关 Skill

- [[server-architecture]] — 服务器架构与消息路由
- [[client-architecture]] — 客户端架构与 UI 系统
```

- [ ] **Step 4: 验证**

---

### Task 6: 创建 `friend-system`

**Files:**
- Create: `.claude/skills/friend-system/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-friend-system-design.md`
- `docs/superpowers/plans/2026-06-02-friend-system.md`
- `docs/superpowers/plans/2026-06-02-friend-system-remaining.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/friend-system
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/friend-system/SKILL.md`，内容包含：

```markdown
---
name: friend-system
description: 好友系统架构、好友关系模型、在线状态同步、好友请求流程。
metadata:
  type: reference
---

# 好友系统

## 概述

好友系统支持双向好友确认、好友列表管理、黑名单和在线状态同步。采用独立 Friend Server 或 Game Server 内置 Stub 模式实现。

## 架构设计

### 好友关系模型

```
Player A ←→ Player B（双向好友）
Player A → Player C（单向黑名单）
```

- 好友列表上限：50 人
- 黑名单上限：20 人
- 好友关系需要双方确认

### 在线状态同步

使用 Redis pub/sub 跨进程广播在线状态：
- 玩家上线：PUBLISH player:online {player_id}
- 玩家下线：PUBLISH player:offline {player_id}
- Friend Server 订阅并更新好友列表的在线状态

## 关键流程

### 好友请求流程

1. 玩家 A 发送 FriendReq（target_player_id）
2. 服务端验证目标玩家存在且不在黑名单
3. 创建待确认请求，通知目标玩家
4. 目标玩家接受：双向添加好友关系
5. 目标玩家拒绝：删除请求

### 好友数据持久化

好友数据存储在 DBMgr 的 player_data 中：
- friend_list: [player_id, ...]
- black_list: [player_id, ...]

## 关键代码路径

- Friend Server：`scripts/server/friend_server/src/`
- FriendManager：`scripts/server/friend_server/src/friend_manager.h/cpp`
- Protobuf：`scripts/common/proto/friend.proto`
- 消息 ID：`shared/message_ids.json`（friend 相关条目）

## 常见陷阱

### 并发请求处理

同时收到多个好友请求：
- 使用请求队列串行处理
- 验证当前状态再执行操作

### 在线状态延迟

好友上线后状态未及时更新：
- Redis pub/sub 有网络延迟
- 客户端定期拉取作为兜底

### 好友列表上限溢出

好友数超过 50 人限制：
- 添加前检查列表长度
- 返回 FRIEND_LIST_FULL 错误码

## 扩展指南

### 添加好友功能（如最近组队）

1. 在 player_data 中添加新字段
2. 在 FriendManager 中添加对应的查询/更新方法
3. 定义新的 Protobuf 消息
4. 更新客户端 UI

## 相关 Skill

- [[server-architecture]] — 服务器架构
- [[player-persistence]] — 玩家数据持久化
- [[team-system]] — 组队系统
```

- [ ] **Step 4: 验证**

---

### Task 7: 创建 `team-system`

**Files:**
- Create: `.claude/skills/team-system/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-team-system-design.md`
- `docs/superpowers/plans/2026-06-02-team-system.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/team-system
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/team-system/SKILL.md`，内容包含：

```markdown
---
name: team-system
description: 组队系统架构、Team Manager、组队流程、队伍状态管理。
metadata:
  type: reference
---

# 组队系统

## 概述

组队系统支持玩家创建队伍、邀请成员、管理队伍状态。Team Manager 管理所有队伍，Team 实体持有成员列表和队伍属性。

## 架构设计

### 组件结构

```
TeamManager
├── Team 1
│   ├── Leader (Player A)
│   ├── Member (Player B)
│   └── Member (Player C)
└── Team 2
    └── ...
```

### 队伍属性

- 队伍上限：5 人
- 队长转移：队长离开时自动转移给最早加入的成员
- 队伍解散：所有成员离开后自动解散

## 关键流程

### 创建队伍

1. 玩家发送 TeamCreateReq
2. TeamManager 创建 Team 实体
3. 创建者成为队长
4. 返回 TeamCreateResp（team_id）

### 邀请成员

1. 队长发送 TeamInviteReq（target_player_id）
2. 验证队伍未满、目标玩家不在其他队伍
3. 发送邀请通知给目标玩家
4. 目标玩家接受：加入队伍
5. 目标玩家拒绝：删除邀请

### 离开队伍

1. 成员发送 TeamLeaveReq
2. 如果是队长：转移队长权限
3. 如果是最后成员：解散队伍
4. 广播队伍状态更新

## 关键代码路径

- Team Manager：`scripts/server/game_server/src/team_manager.h/cpp`
- Team 实体：`scripts/server/game_server/src/team.h`
- Protobuf：`scripts/common/proto/team.proto`

## 常见陷阱

### 队长离线处理

队长掉线但队伍未解散：
- 实现队长超时转移机制
- 或允许其他成员申请队长

### 成员状态同步

成员状态变更未及时通知其他成员：
- 所有队伍操作都广播状态更新
- 使用统一的 TeamSync 消息

### 队伍数据持久化

服务器重启后队伍数据丢失：
- 当前设计队伍数据仅在内存中
- 服务器重启后队伍自动解散
- 如需持久化，需添加 DBMgr 存储

## 扩展指南

### 集成副本匹配

1. 在 Team 中添加副本类型字段
2. TeamManager 提供按副本类型查询队伍的接口
3. 副本系统调用 TeamManager 获取队伍信息

## 相关 Skill

- [[server-architecture]] — 服务器架构
- [[friend-system]] — 好友系统（组队邀请来源）
- [[monster-combat]] — 战斗系统（组队战斗）
```

- [ ] **Step 4: 验证**

---

### Task 8: 创建 `monster-combat`

**Files:**
- Create: `.claude/skills/monster-combat/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-monster-combat-system-design.md`
- `docs/superpowers/plans/2026-06-02-monster-combat-system.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/monster-combat
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/monster-combat/SKILL.md`，内容包含：

```markdown
---
name: monster-combat
description: 战斗系统架构、Monster 实体、伤害计算、客户端集成。
metadata:
  type: reference
---

# 战斗系统

## 概述

战斗系统支持玩家与 Monster 实体进行实时战斗。服务端负责伤害计算和状态管理，客户端负责表现和交互。

## 架构设计

### 战斗模型

```
Player                    Monster
├── HP: 100/100          ├── HP: 50/50
├── Attack: 10           ├── Attack: 5
├── Defense: 5           ├── Defense: 2
└── Combat Exp: 0        └── Drop Table: [...]
```

### 伤害公式

```
damage = max(1, attacker.attack - defender.defense + random(-2, 2))
```

## 关键流程

### 攻击流程

1. 客户端发送 AttackReq（monster_id）
2. 服务端验证玩家和怪物状态
3. 计算伤害（考虑攻击、防御、随机因素）
4. 更新怪物 HP
5. 如果怪物死亡：处理掉落、经验
6. 广播 CombatResult 给附近玩家

### 怪物死亡处理

1. 计算掉落物（根据掉落表）
2. 生成 DropItem 实体
3. 分配经验值给攻击者
4. 标记怪物为死亡状态
5. 启动刷新计时器

## 关键代码路径

- 战斗处理：`scripts/server/game_server/src/combat_handler.h/cpp`
- Monster 实体：`scripts/server/game_server/src/monster.h/cpp`
- 客户端战斗：`scripts/client/combat/` 目录
- Protobuf：`scripts/common/proto/combat.proto`

## 常见陷阱

### 伤害计算溢出

伤害值为负数或超过 HP：
- 使用 `max(1, damage)` 保证最低伤害
- 使用 `max(0, hp - damage)` 保证 HP 不为负

### 怪物刷新重叠

多个怪物刷新在同一位置：
- 刷新时检查目标位置是否已有怪物
- 使用 Spawn 点配置避免重叠

### 战斗状态同步延迟

客户端显示的 HP 与服务端不一致：
- 服务端计算后立即广播 CombatResult
- 客户端收到后更新本地状态

## 扩展指南

### 添加新怪物类型

1. 在配置文件中定义怪物属性（HP、攻击、防御、掉落表）
2. 在 MonsterManager 中注册新怪物类型
3. 在场景中配置 Spawn 点

## 相关 Skill

- [[server-architecture]] — 服务器架构
- [[scene-system]] — 场景管理（怪物刷新）
- [[item-interaction]] — 物品系统（掉落物）
```

- [ ] **Step 4: 验证**

---

### Task 9: 创建 `quest-system`

**Files:**
- Create: `.claude/skills/quest-system/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-quest-system-design.md`
- `docs/superpowers/plans/2026-06-02-quest-system.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/quest-system
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/quest-system/SKILL.md`，内容包含：

```markdown
---
name: quest-system
description: 任务系统架构、任务模型、任务类型、任务目标、任务奖励。
metadata:
  type: reference
---

# 任务系统

## 概述

任务系统支持主线任务、支线任务和日常任务。任务有明确的状态机和目标系统，完成后给予奖励。

## 架构设计

### 任务状态机

```
AVAILABLE → ACTIVE → COMPLETED → SUBMITTED
    ↑          ↓
    └──────────┘（放弃任务）
```

### 任务类型

| 类型 | 说明 | 重置 |
|------|------|------|
| 主线 | 推动剧情发展 | 不可重置 |
| 支线 | 丰富游戏内容 | 不可重置 |
| 日常 | 每日重复 | 每日重置 |

### 任务目标类型

- 收集物品：收集指定数量的物品
- 击杀怪物：击杀指定数量的怪物
- NPC 对话：与指定 NPC 对话
- 探索区域：到达指定区域

## 关键流程

### 任务接取

1. 玩家与 NPC 对话或到达触发区域
2. 检查任务前置条件（等级、前置任务）
3. 创建任务实例，状态设为 ACTIVE
4. 开始追踪任务目标进度

### 任务完成

1. 所有任务目标达成
2. 任务状态变为 COMPLETED
3. 通知客户端显示完成提示
4. 玩家提交任务（与 NPC 对话）
5. 发放奖励，状态变为 SUBMITTED

## 关键代码路径

- Quest Manager：`scripts/server/game_server/src/quest_manager.h/cpp`
- Quest 定义：`config/quests.json`
- 客户端 UI：`scripts/client/ui/quest_panel.py`
- Protobuf：`scripts/common/proto/quest.proto`

## 常见陷阱

### 任务状态持久化

任务进度未保存导致丢失：
- 任务数据存储在 PlayerBizData 中
- 脏标记触发保存

### 目标计数重置

任务目标进度异常重置：
- 避免在 update 中重复初始化进度
- 加载时从持久化数据恢复

### 任务链顺序错误

前置任务未完成就能接取后续任务：
- 接取时严格检查前置任务状态

## 扩展指南

### 添加新任务

1. 在 `config/quests.json` 中定义任务配置
2. 定义任务目标和奖励
3. 如需新目标类型，在 QuestManager 中添加处理逻辑

## 相关 Skill

- [[npc-dialog]] — NPC 对话系统（任务触发）
- [[item-interaction]] — 物品系统（任务奖励）
- [[monster-combat]] — 战斗系统（击杀目标）
```

- [ ] **Step 4: 验证**

---

### Task 10: 创建 `npc-dialog`

**Files:**
- Create: `.claude/skills/npc-dialog/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-npc-dialog-system-design.md`
- `docs/superpowers/plans/2026-06-02-npc-dialog-system.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/npc-dialog
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/npc-dialog/SKILL.md`，内容包含：

```markdown
---
name: npc-dialog
description: NPC 对话系统架构、DialogTree、对话触发、客户端 UI。
metadata:
  type: reference
---

# NPC 对话系统

## 概述

NPC 对话系统支持树状对话结构，玩家与 NPC 交互时触发对话，选择选项推进对话流程。与任务系统深度集成。

## 架构设计

### 对话树结构

```
DialogTree
├── DialogNode 1（NPC 对话）
│   ├── DialogOption A → DialogNode 2
│   └── DialogOption B → DialogNode 3
├── DialogNode 2（NPC 对话）
│   └── DialogOption → close
└── DialogNode 3（玩家选择）
    ├── Option X → DialogNode 4
    └── Option Y → close
```

### 对话效果

- 跳转节点：goto(node_id)
- 触发任务：start_quest(quest_id)
- 给予物品：give_item(item_id, count)
- 关闭对话：close()

## 关键流程

### 对话触发

1. 玩家站在 NPC 附近（距离 ≤ 1 格）
2. 按交互键或点击 NPC
3. 客户端发送 NpcInteractReq（npc_id）
4. 服务端返回对话树数据
5. 客户端渲染 DialogPanel

### 选项选择

1. 玩家点击对话选项
2. 客户端发送 DialogOptionReq（npc_id, option_id）
3. 服务端执行选项效果
4. 返回下一步对话内容或关闭对话

## 关键代码路径

- NPC 管理：`scripts/server/game_server/src/npc_manager.h/cpp`
- 对话树定义：`config/dialogs/` 目录
- 客户端 UI：`scripts/client/ui/dialog_panel.py`
- Protobuf：`scripts/common/proto/npc.proto`

## 常见陷阱

### 对话树循环引用

对话节点形成死循环：
- 加载时检测循环引用
- 设置最大跳转次数限制

### NPC 位置同步

客户端和服务端 NPC 位置不一致：
- NPC 位置在场景配置中定义
- 客户端加载场景时同步 NPC 位置

### 对话状态与任务状态不一致

对话中显示的任务状态与实际不符：
- 对话打开时重新查询任务状态
- 任务状态变更时刷新对话 UI

## 扩展指南

### 添加新 NPC

1. 在场景配置中定义 NPC 位置
2. 在 `config/dialogs/` 中创建对话树文件
3. 在 NPCManager 中注册 NPC

## 相关 Skill

- [[quest-system]] — 任务系统（对话触发任务）
- [[scene-system]] — 场景管理（NPC 位置）
```

- [ ] **Step 4: 验证**

---

### Task 11: 创建 `activity-system`

**Files:**
- Create: `.claude/skills/activity-system/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-activity-system-design.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/activity-system
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/activity-system/SKILL.md`，内容包含：

```markdown
---
name: activity-system
description: 活动系统架构、活动模型、活动类型、活动奖励。
metadata:
  type: reference
---

# 活动系统

## 概述

活动系统支持限时活动、周期活动和节日活动，有明确的时间窗口和状态管理。

## 架构设计

### 活动状态机

```
PENDING → ACTIVE → ENDED
```

### 活动类型

| 类型 | 说明 | 示例 |
|------|------|------|
| 限时 | 固定时间窗口 | 周末双倍经验 |
| 周期 | 重复出现 | 每日签到 |
| 节日 | 特定日期 | 春节活动 |

## 关键流程

### 活动启动

1. 定时器检查活动配置
2. 当前时间进入活动窗口
3. 活动状态变为 ACTIVE
4. 广播活动开始通知

### 活动结束

1. 当前时间超过活动窗口
2. 活动状态变为 ENDED
3. 结算排行榜
4. 发放奖励

## 关键代码路径

- Activity Manager：`scripts/server/game_server/src/activity_manager.h/cpp`
- 活动配置：`config/activities.json`
- Protobuf：`scripts/common/proto/activity.proto`

## 常见陷阱

### 时区处理

活动时间与服务器时区不一致：
- 使用 UTC 时间存储和比较
- 客户端显示时转换为本地时间

### 活动状态持久化

服务器重启后活动状态丢失：
- 从配置文件重新加载
- 根据当前时间重新计算状态

### 跨天活动边界

活动跨越午夜时的行为异常：
- 使用时间戳而非日期字符串
- 明确活动的开始和结束时间戳

## 扩展指南

### 添加新活动类型

1. 在 `config/activities.json` 中定义活动配置
2. 在 ActivityManager 中添加活动逻辑
3. 定义活动奖励规则

## 相关 Skill

- [[game-clock]] — 游戏时钟（活动时间基准）
```

- [ ] **Step 4: 验证**

---

### Task 12: 创建 `player-load-path`

**Files:**
- Create: `.claude/skills/player-load-path/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-player-load-path-fix-design.md`
- `docs/superpowers/plans/2026-06-02-player-load-path-fix.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/player-load-path
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/player-load-path/SKILL.md`，内容包含：

```markdown
---
name: player-load-path
description: 玩家加载路径修复、load_from_json() 方法、字段兼容策略。
metadata:
  type: reference
---

# 玩家加载路径修复

## 概述

修复现有玩家数据加载路径的缺陷，提供统一的 `load_from_json()` 方法，支持字段兼容和数据迁移。

## 架构设计

### 问题描述

现有加载路径的问题：
- 字段丢失时崩溃
- 格式不一致导致解析失败
- 缺少版本管理

### 解决方案

统一的 `load_from_json()` 方法：
- 缺失字段使用默认值
- 多余字段忽略
- 支持版本号字段实现数据迁移

## 关键流程

### 加载流程

1. 从 DBMgr 获取 JSON 数据
2. 调用 `load_from_json(json)`
3. 解析各字段，缺失则使用默认值
4. 检查版本号，必要时执行迁移
5. 填充 Player 对象

### 数据迁移

1. 检查 JSON 中的 version 字段
2. 如果版本低于当前版本，执行迁移函数
3. 迁移后更新版本号
4. 标记为脏以便保存新格式

## 关键代码路径

- Player 加载：`scripts/server/game_server/src/player.cpp`（load_from_json）
- Player 序列化：`scripts/server/game_server/src/player.cpp`（get_all_data_json）
- 测试：`scripts/server/game_server/tests/`

## 常见陷阱

### JSON 格式兼容性

旧格式数据无法解析：
- 使用 try-catch 包裹解析逻辑
- 解析失败时使用默认数据

### 字段缺失导致崩溃

直接访问不存在的 JSON 字段：
- 使用 `json.value("key", default_value)` 安全访问
- 检查字段存在性再使用

### 默认值不合理

默认值导致游戏逻辑异常：
- 默认值应符合游戏设计（如 level=1, gold=100）
- 测试覆盖默认值场景

## 扩展指南

### 添加新字段

1. 在 PlayerBizData 中添加新字段
2. 在 load_from_json 中添加解析逻辑（带默认值）
3. 在 get_all_data_json 中添加序列化逻辑
4. 更新版本号（如需要）

## 相关 Skill

- [[player-persistence]] — 玩家数据持久化机制
- [[dbmgr-data-layer]] — DBMgr 数据层
```

- [ ] **Step 4: 验证**

---

### Task 13: 创建 `player-movement`

**Files:**
- Create: `.claude/skills/player-movement/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-player-movement-completeness-design.md`
- `docs/superpowers/plans/2026-06-02-player-movement-completeness.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/player-movement
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/player-movement/SKILL.md`，内容包含：

```markdown
---
name: player-movement
description: 玩家移动模型、位置同步协议、碰撞检测、客户端预测。
metadata:
  type: reference
---

# 玩家移动系统

## 概述

玩家移动采用客户端预测 + 服务器权威模型。客户端立即响应输入，服务端验证并修正。

## 架构设计

### 移动模型

```
Client                    Server
├── 输入处理              ├── 位置验证
├── 客户端预测            ├── 碰撞检测
├── 位置上报 (100ms)      └── 位置修正 (200ms lerp)
└── 服务器修正
```

### 位置同步协议

- 客户端每 100ms 发送 PosSync（x, y, direction）
- 服务端验证位置合法性
- 服务端每 200ms 广播位置修正（如有偏差）

## 关键流程

### 移动流程

1. 玩家按下方向键
2. 客户端立即移动（预测）
3. 检查本地碰撞
4. 每 100ms 上报位置给服务端
5. 服务端验证位置和碰撞
6. 如有偏差，发送修正位置
7. 客户端 lerp 到修正位置

### 碰撞检测

客户端预判 + 服务端验证：
- 客户端：移动前检查目标位置是否可通行
- 服务端：收到位置后验证是否合法

## 关键代码路径

- 客户端移动：`scripts/client/player_controller.py`
- 服务端验证：`scripts/server/game_server/src/player_manager.cpp`
- 碰撞检测：`scripts/client/scene/tile_map.py`
- Protobuf：`scripts/common/proto/player.proto`（PosSync）

## 常见陷阱

### 位置抖动

客户端和服务端位置不一致导致抖动：
- lerp 平滑过渡，不要直接跳到修正位置
- 修正阈值设置合理（如 > 0.5 格才修正）

### 碰撞穿透

帧率差异导致碰撞检测遗漏：
- 服务端使用连续碰撞检测
- 客户端移动距离不超过 1 格/帧

### 网络延迟补偿

高延迟下移动卡顿：
- 客户端预测允许短暂偏差
- 服务端修正使用 lerp 平滑

## 扩展指南

### 添加新的移动类型

1. 定义新的移动模式（如跑步、骑乘）
2. 更新速度常量
3. 更新碰撞检测逻辑

## 相关 Skill

- [[scene-system]] — 场景管理（地图碰撞数据）
- [[client-architecture]] — 客户端架构（输入处理）
```

- [ ] **Step 4: 验证**

---

### Task 14: 创建 `cross-server`

**Files:**
- Create: `.claude/skills/cross-server/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-cross-server-design.md`
- `docs/superpowers/plans/2026-06-02-cross-server.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/cross-server
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/cross-server/SKILL.md`，内容包含：

```markdown
---
name: cross-server
description: 跨服架构、CrossServerService、GameConnection、RouteCache。
metadata:
  type: reference
---

# 跨服系统

## 概述

跨服系统支持玩家在不同服务器之间交互和迁移。CrossServerService 管理跨服连接，RouteCache 缓存路由信息。

## 架构设计

### 整体架构

```
Game Server A ←→ CrossServerService ←→ Game Server B
                    │
                    ├── GameConnection（连接池）
                    ├── GameSession（会话管理）
                    └── RouteCache（路由缓存）
```

### 组件职责

- CrossServerService：管理所有跨服连接和消息路由
- GameConnection：与远程 Game Server 的连接
- GameSession：跨服玩家会话状态
- RouteCache：玩家 → 目标服务器映射缓存

## 关键流程

### 跨服消息转发

1. 玩家 A 向跨服玩家 B 发送消息
2. 查询 RouteCache 获取 B 所在服务器
3. 通过 GameConnection 转发到目标服务器
4. 目标服务器投递给玩家 B

### 玩家迁移

1. 玩家请求迁移到目标服务器
2. 打包玩家数据
3. 发送到目标服务器
4. 目标服务器创建玩家实例
5. 更新 RouteCache

## 关键代码路径

- CrossServerService：`scripts/server/game_server/src/cross_server_service.h/cpp`
- GameConnection：`scripts/server/game_server/src/game_connection.h/cpp`
- GameSession：`scripts/server/game_server/src/game_session.h/cpp`
- RouteCache：`scripts/server/game_server/src/route_cache.h/cpp`

## 常见陷阱

### 路由缓存失效

玩家迁移后缓存未更新：
- 迁移成功后更新所有相关服务器的 RouteCache
- 缓存设置 TTL 定期刷新

### 会话状态丢失

跨服会话中断后状态丢失：
- 会话数据持久化到 DBMgr
- 重连后恢复会话

### 消息乱序

跨服消息到达顺序与发送顺序不一致：
- 使用消息序列号
- 接收端按序列号排序

## 扩展指南

### 添加新的跨服功能

1. 定义新的跨服消息类型
2. 在 CrossServerService 中添加路由逻辑
3. 在目标服务器中添加处理逻辑

## 相关 Skill

- [[server-architecture]] — 服务器架构
- [[player-persistence]] — 玩家数据持久化（迁移数据）
```

- [ ] **Step 4: 验证**

---

### Task 15: 创建 `client-notification`

**Files:**
- Create: `.claude/skills/client-notification/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-client-notification-system-design.md`
- `docs/superpowers/plans/2026-06-02-client-notification-system.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/client-notification
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/client-notification/SKILL.md`，内容包含：

```markdown
---
name: client-notification
description: 客户端通知系统架构、NotificationManager、通知类型、优先级。
metadata:
  type: reference
---

# 客户端通知系统

## 概述

通知系统统一管理客户端的各种提示信息，支持 Toast 轻提示、Banner 横幅和 Modal 模态框三种类型。

## 架构设计

### 通知类型

| 类型 | 说明 | 自动消失 | 输入阻塞 |
|------|------|----------|----------|
| Toast | 轻量提示 | 是（3秒） | 否 |
| Banner | 横幅通知 | 是（5秒） | 否 |
| Modal | 模态框 | 否（手动关闭） | 是 |

### 优先级

```
紧急（Modal）> 重要（Banner）> 普通（Toast）
```

## 关键流程

### 显示通知

1. 代码调用 NotificationManager.show(type, message)
2. 加入通知队列
3. 按优先级排序
4. 渲染最高优先级通知
5. 自动消失或手动关闭后显示下一个

### 事件处理

1. PyGame 事件循环先传递给 NotificationManager
2. 如果 Modal 可见，消费事件（阻止游戏输入）
3. Toast/Banner 不消费事件

## 关键代码路径

- NotificationManager：`scripts/client/ui/notification_manager.py`
- Toast 组件：`scripts/client/ui/toast.py`
- Banner 组件：`scripts/client/ui/banner.py`

## 常见陷阱

### 通知堆积

大量通知同时到达导致 UI 混乱：
- 限制同时显示的通知数量
- 使用队列排队显示

### 输入冲突

Modal 显示时游戏输入未屏蔽：
- 在事件处理中检查 Modal 状态
- Modal 可见时阻止游戏输入

### 多通知重叠

多个通知同时显示位置重叠：
- 计算通知位置时考虑已有通知
- 使用堆叠布局

## 扩展指南

### 添加新通知类型

1. 定义新的通知类型枚举
2. 创建对应的 UI 组件
3. 在 NotificationManager 中添加处理逻辑

## 相关 Skill

- [[client-architecture]] — 客户端架构与 UI 系统
```

- [ ] **Step 4: 验证**

---

### Task 16: 创建 `config-editor`

**Files:**
- Create: `.claude/skills/config-editor/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-config-editor-design.md`
- `docs/superpowers/plans/2026-06-02-config-editor.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/config-editor
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/config-editor/SKILL.md`，内容包含：

```markdown
---
name: config-editor
description: 配置编辑器架构、Web UI、JSON Schema 验证、热更新机制。
metadata:
  type: reference
---

# 配置编辑器

## 概述

配置编辑器提供 Web UI 界面管理游戏配置文件，支持 JSON Schema 验证和热更新。

## 架构设计

### 整体架构

```
Browser → Web UI (Vue.js) → REST API (Python) → config/*.json
                                                      ↓
                                                  File Watcher
                                                      ↓
                                                  Server Reload
```

### 功能模块

- 配置浏览：树形展示配置文件
- 在线编辑：表单化编辑配置项
- Schema 验证：编辑时实时校验
- 热更新：保存后自动通知服务重载

## 关键流程

### 编辑流程

1. 用户在 Web UI 选择配置文件
2. 加载配置内容和 Schema
3. 表单化展示可编辑字段
4. 用户修改并保存
5. API 验证 Schema
6. 写入配置文件
7. 通知相关服务重载

### 热更新流程

1. 配置文件被修改
2. File Watcher 检测到变更
3. 通知相关服务
4. 服务重新加载配置

## 关键代码路径

- Web UI：`tools/config-editor/` 目录
- API 服务：`tools/config-editor/server.py`
- 配置 Schema：`config/schema/` 目录

## 常见陷阱

### 配置格式校验遗漏

保存了格式错误的配置：
- 严格使用 JSON Schema 验证
- 保存前显示验证错误

### 并发修改冲突

多人同时编辑同一配置：
- 实现乐观锁（版本号检查）
- 冲突时提示用户刷新

### 热更新时机不当

服务正在处理请求时重载配置：
- 实现配置重载的队列机制
- 在空闲时执行重载

## 扩展指南

### 添加新配置文件支持

1. 创建对应的 JSON Schema
2. 在 Web UI 中添加配置文件入口
3. 在 API 中添加读写接口

## 相关 Skill

- [[server-architecture]] — 服务器架构（配置系统）
```

- [ ] **Step 4: 验证**

---

### Task 17: 创建 `code-split-refactor`

**Files:**
- Create: `.claude/skills/code-split-refactor/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-code-split-refactor-design.md`
- `docs/superpowers/plans/2026-06-02-code-split-refactor.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/code-split-refactor
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/code-split-refactor/SKILL.md`，内容包含：

```markdown
---
name: code-split-refactor
description: 代码拆分重构策略、拆分案例、依赖注入模式。
metadata:
  type: reference
---

# 代码拆分重构

## 概述

代码拆分重构的目标是将大文件拆分为职责单一的小模块，提升可维护性。

## 架构设计

### 拆分策略

- 按职责划分：每个模块单一职责
- 协调层保留：跨模块交互逻辑放在协调层
- 依赖注入：模块间通过回调解耦

### 已完成案例

game_scene.py (877行) → 4 个模块：
- PlayerController (303行)：玩家移动/位置同步
- NetworkDispatcher (176行)：网络消息分发
- GameRenderer (120行)：渲染逻辑
- GameScene (493行)：薄协调层

## 关键流程

### 拆分步骤

1. 识别大文件中的职责边界
2. 提取独立模块
3. 定义模块接口
4. 使用依赖注入连接模块
5. 验证功能不变
6. 更新导入路径

### 依赖注入模式

```python
class PlayerController:
    def __init__(self, connection, tmx_map, player_sprite):
        self.connection = connection
        self.tmx_map = tmx_map
        self.player_sprite = player_sprite
```

## 关键代码路径

- 拆分后的模块：`scripts/client/player_controller.py`, `network_dispatcher.py`, `game_renderer.py`, `game_scene.py`
- 设计文档：`docs/superpowers/specs/2026-06-02-code-split-refactor-design.md`

## 常见陷阱

### 循环导入

模块间相互导入导致启动失败：
- 使用依赖注入而非直接导入
- 协调层持有所有模块引用

### 依赖注入遗漏

模块创建时忘记注入依赖：
- 使用构造函数注入，遗漏时报错明显
- 编写初始化测试

### 拆分后接口不一致

拆分后模块接口与原代码不兼容：
- 保持原有公共接口不变
- 使用 adapter 模式适配

## 扩展指南

### 拆分新文件

1. 分析文件职责
2. 识别可独立的模块
3. 按上述步骤执行拆分
4. 编写测试验证

## 相关 Skill

- [[client-architecture]] — 客户端架构
```

- [ ] **Step 4: 验证**

---

### Task 18: 创建 `ai-knowledge-skills`

**Files:**
- Create: `.claude/skills/ai-knowledge-skills/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-02-ai-knowledge-skills-design.md`
- `docs/superpowers/plans/2026-06-02-ai-knowledge-skills.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建目录**

```bash
mkdir -p .claude/skills/ai-knowledge-skills
```

- [ ] **Step 3: 创建 SKILL.md**

创建 `.claude/skills/ai-knowledge-skills/SKILL.md`，内容包含：

```markdown
---
name: ai-knowledge-skills
description: Skill 体系架构、编写指南、标准章节模板、交叉引用规范。
metadata:
  type: reference
---

# AI Knowledge Skills 体系

## 概述

AI Knowledge Skills 是将项目设计知识从"需求文档"转化为"开发指南"的体系。每个 skill 聚焦一个子系统，供 AI 开发者按需加载。

## 架构设计

### 目录结构

```
.claude/skills/
├── server-architecture/SKILL.md
├── scene-system/SKILL.md
├── crop-system/SKILL.md
└── ...
```

### Frontmatter 规范

```yaml
---
name: skill-name
description: 一句话描述
metadata:
  type: reference
---
```

## 关键流程

### 创建新 Skill 的步骤

1. 确定子系统和源文档
2. 创建 `.claude/skills/{name}/` 目录
3. 创建 `SKILL.md` 文件
4. 按标准章节模板组织内容
5. 添加交叉引用
6. 验证格式和内容

### 内容来源

- `docs/superpowers/specs/` — 设计文档
- `docs/superpowers/plans/` — 实施计划
- `openspec/specs/` — 规范文档
- 实际代码

## 标准章节模板

```markdown
# 系统名称

## 概述
简要说明系统的定位和职责

## 架构设计
核心设计模式、数据结构、组件关系

## 关键流程
主要业务流程的步骤说明

## 关键代码路径
- 服务器：path/to/file.cpp/.h
- 客户端：path/to/file.py
- 配置：path/to/config

## 常见陷阱
从测试报告中提取的常见错误和解决方案

## 扩展指南
如何添加新功能的模板和步骤

## 相关 Skill
- [[other-skill]] — 关联说明
```

## 交叉引用规范

使用 `[[skill-name]]` 标记相关 skill：
- 指向存在的 skill
- 说明关联关系
- 不要创建不存在的引用

## 常见陷阱

### 内容与 spec 不一致

skill 内容过时或与设计文档矛盾：
- 从 spec 提炼而非复制
- 定期审查更新

### 交叉引用断裂

引用的 skill 不存在：
- 创建 skill 前检查依赖
- 使用 [[name]] 语法自动标记

### frontmatter 缺失

skill 文件没有 frontmatter：
- 必须包含 name、description、metadata.type

## 扩展指南

### 为新子系统创建 Skill

1. 确认子系统有完整的设计文档
2. 按标准模板创建 skill
3. 添加到相关 skill 的交叉引用中

## 相关 Skill

- [[server-architecture]] — 服务器架构（skill 示例）
```

- [ ] **Step 4: 验证**

---

### Task 19: 最终验证

- [ ] **Step 1: 检查所有 skill 文件的 frontmatter**

```bash
for f in .claude/skills/*/SKILL.md; do
  echo "=== $f ==="
  head -5 "$f"
done
```

确认每个文件都有完整的 name、description、metadata。

- [ ] **Step 2: 检查交叉引用**

```bash
grep -r "\[\[" .claude/skills/*/SKILL.md | grep -v "node_modules"
```

确认所有 `[[skill-name]]` 指向存在的 skill。

- [ ] **Step 3: 检查目录结构**

```bash
ls -la .claude/skills/
```

确认 18 个新目录都已创建。

- [ ] **Step 4: 提交**

```bash
git add .claude/skills/
git commit -m "docs: add 18 AI knowledge skills for new subsystems"
```
