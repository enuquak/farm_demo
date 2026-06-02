# AI Knowledge Skills 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建 10 个新 skill 文件 + 更新 1 个现有 skill，将项目的设计知识从"需求文档"转化为 AI 可直接加载的"开发指南"。

**Architecture:** 按职责域拆分 skill，每个 skill 聚焦一个子系统，包含概述、架构设计、关键流程、代码路径、常见陷阱、扩展指南。内容从 openspec/specs 和 docs/superpowers/specs 提炼。

**Tech Stack:** Markdown, Claude Code Skills

---

## 文件结构

```
.claude/skills/
├── cpp-ai-coding-conventions/SKILL.md  ← 更新
├── server-architecture/SKILL.md        ← 新建
├── scene-system/SKILL.md               ← 新建
├── crop-system/SKILL.md                ← 新建
├── item-interaction/SKILL.md           ← 新建
├── player-persistence/SKILL.md         ← 新建
├── energy-system/SKILL.md              ← 新建
├── game-clock/SKILL.md                 ← 新建
├── gm-system/SKILL.md                  ← 新建
├── client-architecture/SKILL.md        ← 新建
└── dbmgr-data-layer/SKILL.md           ← 新建
```

---

### Task 1: 更新 `cpp-ai-coding-conventions`

**Files:**
- Modify: `.claude/skills/cpp-ai-coding-conventions/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-05-31-stub-design.md`
- `docs/superpowers/specs/2026-06-01-player-persistence-redesign.md`
- `docs/superpowers/specs/2026-05-31-dbmgr-redis-mongo-design.md`
- `docs/superpowers/specs/2026-06-01-global-player-id-design.md`

- [ ] **Step 1: 读取源文档，提取设计模式**

读取上述 4 个设计文档，提取以下模式的详细实现：
- Stub 模式（LoginStub、OnlineStub、GMStub 的构造和依赖注入方式）
- 回调注入（std::function 的典型签名和使用场景）
- ConnectionManager 状态机（状态转换、重试逻辑）
- 异步请求-响应（request_id 生成、pending_requests map）
- 脏字段追踪（dirty_ + dirty_fields_ 的 setter 模式）
- 实体自治（Player 持有 DBMgrConnectionManager*）
- 数据路由（hash 函数、取模逻辑）

- [ ] **Step 2: 在 SKILL.md 末尾添加"项目通用设计模式"章节**

在现有"十二、功能开发 Checklist"之后，添加新章节：

```markdown
---

## 十三、项目通用设计模式

> 以下模式是本项目的核心架构约定，所有 C++ 开发都必须遵循。

### 13.1 Stub 模式

Stub 是 Game Server 内部的单点业务组件，每个 Stub 持有自己的依赖，通过回调注入与其他模块通信。

**已有 Stub：**
- `LoginStub` — 登录流程管理，依赖 RedisConnection、DBMgrConnectionManager
- `OnlineStub` — 在线状态查询，依赖 RedisConnection
- `GMStub` — GM 命令处理，依赖 DBMgrConnectionManager

**创建新 Stub 的模板：**
```cpp
// my_stub.h
class MyStub {
public:
    MyStub(RedisConnection* redis, DBMgrConnectionManager* dbmgr);
    void init();

private:
    RedisConnection* m_redis;
    DBMgrConnectionManager* m_dbmgr;
};

// game_server.cpp 构造函数中
m_my_stub = std::make_unique<MyStub>(m_redis.get(), m_dbmgr_conn_mgr.get());
```

### 13.2 回调注入

模块间通信使用 `std::function` 回调，不直接引用其他模块。

**典型签名：**
```cpp
using SendToGateFunc = std::function<void(uint32_t player_id, const std::string& data)>;
using AllocPlayerIdCallback = std::function<void(uint64_t player_id)>;
using OnlineQueryCallback = std::function<void(bool is_online)>;
```

**注入方式：** 构造函数或 setter 方法注入，运行时通过回调调用。

### 13.3 ConnectionManager 状态机

统一的数据库连接管理模式：

```
DISCONNECTED → CONNECTING → CONNECTED
                          → FAILED → FAILED_PERMANENT
```

- 后台重试线程在 FAILED 状态自动重连
- ready 回调通知上层连接就绪
- 应用于 MongoDB 和 Redis 连接

### 13.4 异步请求-响应

DBMgr 通信使用 request_id 匹配：

```cpp
// 发送时
uint64_t request_id = generate_request_id();
m_pending_requests[request_id] = callback;
send_to_dbmgr(request_id, data);

// 响应回来时
auto it = m_pending_requests.find(response.request_id());
if (it != m_pending_requests.end()) {
    it->second(response);
    m_pending_requests.erase(it);
}
```

单线程事件循环中不阻塞等待。

### 13.5 脏字段追踪

Player 实体使用双重标记：

```cpp
bool dirty_ = false;
std::unordered_set<std::string> dirty_fields_;

void set_gold(int gold) {
    m_gold = gold;
    dirty_ = true;
    dirty_fields_.insert("gold");
}
```

三种保存路径：
- `save_full()` — SET_ALL，定时器/断线时使用
- `save()` — 逐字段 SET，业务刷新时使用
- `save_field("gold")` — 单字段 SET，精确保存

脏标记在保存失败时不清除，下次定时器重试。

### 13.6 实体自治

Player 持有 `DBMgrConnectionManager*` 直接引用，自主管理保存定时器（libevent timer，5 分钟间隔），不依赖 PlayerManager 代为保存。

### 13.7 数据路由

```cpp
// 玩家数据路由
int target_dbmgr = player_id % dbmgr_count;

// 账户数据路由
int target_dbmgr = std::hash<std::string>{}(account_id) % dbmgr_count;
```
```

- [ ] **Step 3: 更新 frontmatter description**

将 description 更新为包含"项目设计模式"的描述：

```yaml
---
name: cpp-ai-coding-conventions
description: C++ 通用 AI 编码规范 + 项目设计模式：格式、命名、错误处理、日志、内存管理、Stub 模式、回调注入、ConnectionManager 等。
---
```

- [ ] **Step 4: 验证**

检查文件格式正确，Markdown 渲染正常。

---

### Task 2: 创建 `server-architecture`

**Files:**
- Create: `.claude/skills/server-architecture/SKILL.md`

**Source Documents:**
- `openspec/specs/gate-server/spec.md`
- `openspec/specs/game-server/spec.md`
- `openspec/specs/dbmgr/spec.md`
- `openspec/specs/server-config/spec.md`
- `openspec/specs/server-lifecycle/spec.md`
- `openspec/specs/logging/spec.md`
- `docs/superpowers/specs/2026-06-01-etcd-service-discovery-design.md`
- `docs/superpowers/specs/2026-06-01-shared-constants-design.md`

- [ ] **Step 1: 读取源文档**

读取上述 8 个文档，提取架构、协议、生命周期、配置、服务发现、共享常量的关键信息。

- [ ] **Step 2: 创建 SKILL.md**

```markdown
---
name: server-architecture
description: 服务器三进程架构、消息协议、服务生命周期、配置系统、服务发现、共享常量、构建系统。
metadata:
  type: reference
---

# 服务器架构

## 概述

项目采用三进程架构：Gate Server（客户端网关）、Game Server（游戏逻辑）、DBMgr（数据代理）。三者通过 TCP + Protobuf 通信，共享统一的消息协议和服务生命周期管理。

## 架构设计

### 三进程架构

```
Client (Python/PyGame)
    | TCP + Protobuf
    v
Gate Server (C++17/libevent) -- 端口 8080
    | TCP + 内部 Protobuf
    v
Game Server (C++17/libevent) -- 端口 9090
    | TCP + dbmgr Protobuf
    v
DBMgr (C++17/libevent) -- 端口 5000
    |
    v
MongoDB + Redis
```

### 消息协议

**线格式：**
```
[4 字节 Length][4 字节 MsgID][变长 Protobuf Payload]
```

**MsgID 分区：**
| 范围 | 用途 |
|------|------|
| 1000-1999 | 客户端心跳/账户消息 |
| 2000-2999 | 登录/玩家消息 |
| 3000-3299 | Gate-Game 内部消息 |
| 4000-4299 | Game-DBMgr 消息 |
| 5000-5999 | 管理消息（shutdown） |

### 启动顺序

dbmgr → game_server → gate_server（间隔 2 秒）

### 优雅关闭

MSG_ID_SHUTDOWN (5001) 级联传播：gate → game → dbmgr

## 关键流程

### 服务启动

1. dbmgr 启动，监听端口 5000
2. 等待 2 秒
3. game_server 启动，监听端口 9090，连接所有 dbmgr
4. 等待 2 秒
5. gate_server 启动，监听端口 8080，连接所有 game_server

### PID 文件管理

每个服务在 `runtimeData/` 目录下创建 PID 文件，用于进程管理和强制关闭。

## 关键代码路径

- Gate Server: `scripts/server/gate_server/src/`
- Game Server: `scripts/server/game_server/src/`
- DBMgr: `scripts/server/dbmgr/src/`
- 公共模块: `scripts/server/common/`
- 配置文件: `config/gate_server.json`, `config/game_server.json`, `config/dbmgr.json`
- Protobuf 定义: `scripts/common/proto/`
- 共享常量: `shared/message_ids.json`, `shared/error_codes.json`
- 构建脚本: `tools/build_all.bat`, `tool/build_cpp14.bat`

## 常见陷阱

1. **DLL 缺失** — exe 启动报"找不到 xxx.dll"，检查 bin/ 目录是否包含所有 DLL（event.dll, event_core.dll, event_extra.dll）
2. **端口冲突** — 启动前检查端口是否被占用：`netstat -ano | grep <port>`
3. **启动顺序错误** — 必须按 dbmgr → game_server → gate_server 顺序启动
4. **MsgID 不匹配** — 客户端和服务器的 MsgID 必须一致，使用共享常量系统自动生成

## 扩展指南

### 添加新的服务器进程

1. 在 `scripts/server/` 下创建新目录
2. 创建 `CMakeLists.txt`（参考现有服务）
3. 创建 `src/main.cpp`（使用 server_main_helper）
4. 在 `config/` 下添加配置文件
5. 更新 `tools/build_all.bat` 和 `tools/start_all.bat`

### 添加新的 MsgID

1. 在 `shared/message_ids.json` 添加新条目
2. 运行 `tools/generate_constants.py` 自动生成 C++ 和 Python 常量
3. 在对应服务中注册消息处理器

## 相关 Skill

- [[dbmgr-data-layer]] — DBMgr 数据层详细设计
- [[player-persistence]] — 玩家持久化机制
- [[gm-system]] — GM 系统 HTTP API
```

- [ ] **Step 3: 验证**

检查 frontmatter、章节完整性、交叉引用。

---

### Task 3: 创建 `dbmgr-data-layer`

**Files:**
- Create: `.claude/skills/dbmgr-data-layer/SKILL.md`

**Source Documents:**
- `openspec/specs/dbmgr/spec.md`
- `docs/superpowers/specs/2026-05-31-dbmgr-redis-mongo-design.md`
- `docs/superpowers/specs/2026-06-01-global-player-id-design.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建 SKILL.md**

内容包括：
- DBMgr 架构（纯数据代理、零业务逻辑）
- MongoDB 集成（MongoConnection、集合、索引配置）
- Redis 集成（RedisConnection、读穿透缓存、写穿透失效）
- ConnectionManager 状态机（详细状态转换图）
- 数据路由（player_id/account_id hash）
- 全局 ID 分配（MongoDB counters、批量预分配 64 个）
- 关键代码路径
- 常见陷阱（连接失败处理、索引未创建、ID 耗尽）

- [ ] **Step 3: 验证**

---

### Task 4: 创建 `player-persistence`

**Files:**
- Create: `.claude/skills/player-persistence/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-01-player-persistence-redesign.md`
- `openspec/specs/dbmgr/spec.md`（PlayerDataReq/Resp 协议）

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建 SKILL.md**

内容包括：
- 脏字段追踪机制（dirty_ + dirty_fields_）
- 三种保存路径（save_full、save、save_field）的使用场景
- JSON 序列化（nlohmann::json 格式）
- 定时保存（libevent timer、5 分钟间隔）
- 实体自治（Player 持有 DBMgrConnectionManager*）
- 断线保存流程
- 关键代码路径
- 常见陷阱（脏标记未清除、JSON 格式不一致、定时器泄漏）

- [ ] **Step 3: 验证**

---

### Task 5: 创建 `scene-system`

**Files:**
- Create: `.claude/skills/scene-system/SKILL.md`

**Source Documents:**
- `openspec/specs/scene-management/spec.md`
- `openspec/specs/scene-grid/spec.md`
- `openspec/specs/tile-layers/spec.md`
- `openspec/specs/portal-system/spec.md`
- `openspec/specs/scene-transition/spec.md`
- `openspec/specs/house-scene/spec.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建 SKILL.md**

内容包括：
- 双层地图架构（ground layer + object layer、GroundType/ObjectType 枚举）
- 场景注册与管理（scene_defs.py、GameSceneManager）
- 冻结/解冻机制（时间戳记录、经过时间模拟）
- 传送门系统（双触发检测、DOOR_IN/DOOR_OUT）
- 场景切换流程（SceneChangeReq/Resp、Iris 动画状态机）
- 关键代码路径
- 常见陷阱（场景数据未持久化、传送门方向错误、冻结时间计算错误）

- [ ] **Step 3: 验证**

---

### Task 6: 创建 `crop-system`

**Files:**
- Create: `.claude/skills/crop-system/SKILL.md`

**Source Documents:**
- `openspec/specs/crop-system/spec.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建 SKILL.md**

内容包括：
- 作物生命周期（TILLED → CROP_GROWING → CROP_READY）
- 60 秒实时生长
- 冻结/解冻支持（与 scene-system 集成）
- 交互与收获（播种、成熟收获）
- 持久化（序列化到场景数据）
- 关键代码路径
- 常见陷阱（作物状态未同步、冻结后生长计算错误）

- [ ] **Step 3: 验证**

---

### Task 7: 创建 `item-interaction`

**Files:**
- Create: `.claude/skills/item-interaction/SKILL.md`

**Source Documents:**
- `openspec/specs/item-registry/spec.md`
- `openspec/specs/item-interaction/spec.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建 SKILL.md**

内容包括：
- 物品注册表（7 个物品、4 种类型、ITEM_EFFECTS 表）
- 交互效果系统（remove_object、set_ground、place_object 等）
- DropItem 实体（48px 自动拾取、300 秒生命周期）
- 交互流程（ItemUseReq → 验证 → 执行 → 300ms 冷却）
- 关键代码路径
- 常见陷阱（物品 ID 不匹配、效果未触发、DropItem 内存泄漏）

- [ ] **Step 3: 验证**

---

### Task 8: 创建 `energy-system`

**Files:**
- Create: `.claude/skills/energy-system/SKILL.md`

**Source Documents:**
- `openspec/specs/energy-system/spec.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建 SKILL.md**

内容包括：
- 能量模型（current=100、max=100）
- 消耗/恢复规则（按 ITEM_EFFECTS 配置）
- 耗尽处理（ENERGY_EXHAUSTED 错误码、客户端模态对话框）
- 同步机制（EnergySync 消息）
- 客户端 UI（40x120px 能量条、颜色变化阈值）
- 关键代码路径
- 常见陷阱（能量值未同步、耗尽状态未重置）

- [ ] **Step 3: 验证**

---

### Task 9: 创建 `game-clock`

**Files:**
- Create: `.claude/skills/game-clock/SKILL.md`

**Source Documents:**
- `openspec/specs/game-clock/spec.md`
- `openspec/specs/forced-sleep/spec.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建 SKILL.md**

内容包括：
- 时钟模型（time_slot 0-39、每 slot 30 游戏分钟、现实 1 分钟 = 游戏 30 分钟）
- 日循环（40 slot × 60 ticks、帧率无关）
- 时钟同步（ClockSync 广播）
- 强制睡眠流程（slot 40 → 暂停时钟 → ForceSleepNotify → Iris → 切换场景 → 恢复 50% 能量 → 推进一天）
- 关键代码路径
- 常见陷阱（时钟暂停后未恢复、强制睡眠超时、能量恢复计算错误）

- [ ] **Step 3: 验证**

---

### Task 10: 创建 `gm-system`

**Files:**
- Create: `.claude/skills/gm-system/SKILL.md`

**Source Documents:**
- `docs/superpowers/specs/2026-06-01-gm-platform-design.md`
- `docs/superpowers/plans/2026-06-01-gm-platform.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建 SKILL.md**

内容包括：
- GM 命令注册系统（GMStub、map<string, GmHandler>、handler 注册）
- HTTP API（evhttp 端口 7070、/api/gm/commands、/api/gm/execute、/api/gm/players）
- 在线/离线处理（在线直接操作内存、离线通过 DBMgr 查询/修改 MongoDB）
- 批量模板（gm_templates.h、player_id 注入）
- 前端面板（Vue.js 3 SPA、CDN 加载、动态参数渲染）
- 添加新 GM 命令的完整步骤
- 关键代码路径
- 常见陷阱（HTTP 端口冲突、离线玩家数据未同步、模板参数类型错误）

- [ ] **Step 3: 验证**

---

### Task 11: 创建 `client-architecture`

**Files:**
- Create: `.claude/skills/client-architecture/SKILL.md`

**Source Documents:**
- `openspec/specs/client-connection_2/spec.md`
- `openspec/specs/client-scene-rendering/spec.md`
- `openspec/specs/inventory-ui/spec.md`
- `openspec/specs/input-mapping/spec.md`
- `openspec/specs/player-movement/spec.md`
- `openspec/specs/client-login-screen/spec.md`

- [ ] **Step 1: 读取源文档**

- [ ] **Step 2: 创建 SKILL.md**

内容包括：
- 客户端整体架构（PyGame 主循环 + 独立网络线程 + 线程安全队列）
- 网络层（GateConnection、心跳 5s/15s、NetworkDispatcher dict 映射）
- 场景管理（SceneManager、TMX 地图、Iris 过渡动画）
- 玩家控制器（WASD 输入、客户端预测、100ms 位置更新、服务器 200ms lerp 修正）
- 渲染协调（GameRenderer、双层渲染、摄像机 lerp + 边界约束）
- UI 系统（能量条、时间 HUD、快捷栏、背包面板、耗尽模态框）
- 输入映射（Action Map、鼠标状态、屏幕到世界坐标转换）
- 关键代码路径
- 常见陷阱（线程安全问题、网络断开未重连、位置预测抖动、UI 层未阻塞游戏输入）

- [ ] **Step 3: 验证**

---

### Task 12: 最终验证

- [ ] **Step 1: 检查所有 skill 文件的 frontmatter**

确认每个文件都有完整的 name、description、metadata。

- [ ] **Step 2: 检查交叉引用**

确认所有 `[[skill-name]]` 指向存在的 skill。

- [ ] **Step 3: 检查内容一致性**

确认每个 skill 的内容与 openspec/specs 和 docs/superpowers/specs 一致。

- [ ] **Step 4: 提交**

```bash
git add .claude/skills/
git commit -m "docs: add AI knowledge skills for all subsystems"
```
