# AI Knowledge Skills 设计文档

## 背景

项目已有一定规模（37 个 openspec spec、9 个 superpowers 设计文档），但这些文档是面向需求的规范格式（SHALL 语句、场景验收标准），不适合 AI 开发者直接作为"开发指南"使用。需要将项目的设计知识和实现细节提炼为 AI 可直接加载的 skill 文件，供后续 AI 开发使用。

## 目标

1. 将项目的设计知识从"需求文档"转化为"开发指南"
2. 按职责域拆分 skill，AI 按需加载精准上下文
3. 覆盖项目全部子系统，不留知识盲区
4. 与现有 skill 体系（openspec workflow、coding conventions）兼容

## 设计决策

### 1. Skill 组织方案：细粒度分离

选择按职责域拆分而非按层分组，原因：
- 每个子系统后续都会扩展，独立文件便于维护
- AI 开发场景通常是针对单个子系统，不需要加载全部上下文
- 与 openspec 的 spec 文件一一对应，便于溯源

### 2. 语言：中文

与现有 skills（cpp-ai-coding-conventions、project-context）保持一致。

### 3. 内容来源

从以下来源提炼，但以"开发指南"形式重新组织：
- `openspec/specs/` — 37 个规范文档
- `docs/superpowers/specs/` — 9 个设计文档
- `docs/superpowers/plans/` — 9 个实现计划
- 实际代码

## Skill 清单

### 总计：10 个新 skill + 1 个更新

| # | Skill | 定位 | 内容来源 |
|---|-------|------|----------|
| 0 | `cpp-ai-coding-conventions` (更新) | 补充项目通用设计模式 | docs/superpowers/specs 中的设计模式 |
| 1 | `server-architecture` (新建) | 三进程架构、协议、生命周期 | openspec: gate-server, game-server, dbmgr, server-config, server-lifecycle, logging |
| 2 | `scene-system` (新建) | 场景管理、冻结/解冻、传送门 | openspec: scene-management, scene-grid, tile-layers, portal-system, scene-transition, house-scene |
| 3 | `crop-system` (新建) | 作物生命周期、生长定时器 | openspec: crop-system |
| 4 | `item-interaction` (新建) | 物品注册、交互效果、DropItem | openspec: item-registry, item-interaction |
| 5 | `player-persistence` (新建) | 脏字段追踪、保存路径 | docs/superpowers/specs: player-persistence-redesign |
| 6 | `energy-system` (新建) | 能量模型、消耗/恢复 | openspec: energy-system |
| 7 | `game-clock` (新建) | 时钟模型、日循环、强制睡眠 | openspec: game-clock, forced-sleep |
| 8 | `gm-system` (新建) | GM 命令、HTTP API、批量模板 | docs/superpowers/specs: gm-platform-design |
| 9 | `client-architecture` (新建) | PyGame 客户端、渲染、网络、UI | openspec: client-connection, client-scene-rendering, inventory-ui, input-mapping, time-hud, energy-system(client) |
| 10 | `dbmgr-data-layer` (新建) | MongoDB/Redis、连接管理、数据路由 | docs/superpowers/specs: dbmgr-redis-mongo, global-player-id; openspec: dbmgr |

### 0. `cpp-ai-coding-conventions` 更新

在现有编码规范基础上，新增"项目通用设计模式"章节：

1. **Stub 模式** — Game Server 内部单点业务组件（LoginStub、OnlineStub、GMStub），持有自己的依赖，通过回调注入
2. **回调注入** — 模块间通信使用 `std::function` 回调，不直接引用其他模块
3. **ConnectionManager 状态机** — DISCONNECTED → CONNECTING → CONNECTED / FAILED / FAILED_PERMANENT，后台重试
4. **异步请求-响应** — request_id 匹配，pending_requests map，单线程事件循环中不阻塞
5. **脏字段追踪** — dirty_ + dirty_fields_ 双重标记，三种保存路径
6. **实体自治** — Player 持有 DBMgrConnectionManager*，自主管理保存定时器
7. **数据路由** — player_id % dbmgr_count / hash(account_id) % dbmgr_count

### 1. `server-architecture/SKILL.md`

内容：
1. 三进程架构概览（Gate:8080、Game:9090、DBMgr:5000）
2. 消息协议规范（线格式、MsgID 分区、Protobuf 文件位置）
3. 服务生命周期管理（启动顺序、PID 文件、优雅/强制关闭）
4. 配置系统（JSON 配置、nlohmann/json）
5. 服务发现（etcd、Lease 注册、Watch 发现）
6. 共享常量系统（shared/*.json、自动生成）
7. 构建系统（C++17、MSVC、CMake、依赖路径）

### 2. `scene-system/SKILL.md`

内容：
1. 双层地图架构（ground layer + object layer）
2. 场景注册与管理（scene_defs.py、GameSceneManager）
3. 冻结/解冻机制（时间戳、经过时间模拟）
4. 传送门系统（双触发检测）
5. 场景切换流程（SceneChangeReq/Resp、Iris 动画）
6. 关键代码路径

### 3. `crop-system/SKILL.md`

内容：
1. 作物生命周期（TILLED → CROP_GROWING → CROP_READY）
2. 60 秒实时生长
3. 冻结/解冻支持
4. 交互与收获
5. 持久化
6. 关键代码路径

### 4. `item-interaction/SKILL.md`

内容：
1. 物品注册表（7 个物品、4 种类型）
2. 交互效果系统（ITEM_EFFECTS 表）
3. DropItem 实体（自动拾取、生命周期）
4. 交互流程（ItemUseReq、验证、执行）
5. 关键代码路径

### 5. `player-persistence/SKILL.md`

内容：
1. 脏字段追踪机制
2. 三种保存路径（save_full、save、save_field）
3. JSON 序列化（nlohmann::json）
4. 定时保存（libevent timer、5 分钟）
5. 实体自治
6. 关键代码路径

### 6. `energy-system/SKILL.md`

内容：
1. 能量模型（current=100、max=100）
2. 消耗/恢复规则
3. 耗尽处理（ENERGY_EXHAUSTED、模态对话框）
4. 同步机制（EnergySync）
5. 客户端 UI（能量条、颜色变化）
6. 关键代码路径

### 7. `game-clock/SKILL.md`

内容：
1. 时钟模型（time_slot 0-39、每 slot 30 游戏分钟）
2. 日循环（40 slot × 60 ticks）
3. 时钟同步（ClockSync 广播）
4. 强制睡眠流程（slot 40 → ForceSleepNotify → Iris → 切换场景）
5. 关键代码路径

### 8. `gm-system/SKILL.md`

内容：
1. GM 命令注册系统（GMStub、handler 注册）
2. HTTP API（evhttp、端点列表）
3. 在线/离线处理
4. 批量模板
5. 前端面板（Vue.js 3 SPA）
6. 添加新 GM 命令的步骤
7. 关键代码路径

### 9. `client-architecture/SKILL.md`

内容：
1. 客户端整体架构（PyGame 主循环 + 网络线程）
2. 网络层（GateConnection、心跳、NetworkDispatcher）
3. 场景管理（SceneManager、TMX 地图）
4. 玩家控制器（输入、客户端预测、服务器修正）
5. 渲染协调（GameRenderer、双层渲染、摄像机）
6. UI 系统（能量条、时间 HUD、快捷栏、背包）
7. 输入映射（Action Map、鼠标状态）
8. 关键代码路径

### 10. `dbmgr-data-layer/SKILL.md`

内容：
1. DBMgr 架构（纯数据代理、零业务逻辑）
2. MongoDB 集成（MongoConnection、集合、索引）
3. Redis 集成（RedisConnection、读穿透缓存、写穿透失效）
4. ConnectionManager 状态机
5. 数据路由（player_id/account_id hash）
6. 全局 ID 分配（MongoDB counters、批量预分配）
7. 关键代码路径

## 设计原则

1. **元数据块** — 每个 skill 使用标准 frontmatter（name、description、metadata）
2. **开发指南形式** — 从"需求 SHALL"转化为"开发时需要注意什么"
3. **关键代码路径** — 每个 skill 包含具体的文件路径，便于 AI 直接定位
4. **交叉引用** — 使用 `[[skill-name]]` 标记相关 skill
5. **常见陷阱** — 每个 skill 必须包含"常见陷阱"章节，从 agent_workspace_data/ 的 40+ 测试报告中提取该子系统的常见错误
6. **可扩展** — 每个 skill 预留"扩展"章节，便于后续添加新功能

## Skill 文件标准结构

每个 skill 文件应包含以下章节（按需调整顺序）：

```markdown
---
name: <skill-name>
description: <一句话描述>
metadata:
  type: reference
---

# <系统名称>

## 概述
简要说明系统的定位和职责

## 架构设计
核心设计模式、数据结构、组件关系

## 关键流程
主要业务流程的步骤说明

## 关键代码路径
- 服务器：`path/to/file.cpp/.h`
- 客户端：`path/to/file.py`
- 配置：`path/to/config`

## 常见陷阱
从测试报告中提取的常见错误和解决方案

## 扩展指南
如何添加新功能的模板和步骤

## 相关 Skill
- [[other-skill]] — 关联说明
```

## 实施顺序

1. 更新 `cpp-ai-coding-conventions`（通用基础）
2. 创建 `server-architecture`（架构基础）
3. 创建 `dbmgr-data-layer`（数据基础）
4. 创建 `player-persistence`（核心实体）
5. 创建 `scene-system`（核心场景）
6. 创建 `crop-system`、`item-interaction`、`energy-system`、`game-clock`（游戏系统）
7. 创建 `gm-system`（运维工具）
8. 创建 `client-architecture`（客户端）

## 验证标准

- [ ] 每个 skill 文件有完整的 frontmatter
- [ ] 每个 skill 包含关键代码路径
- [ ] 交叉引用正确指向存在的 skill
- [ ] 内容与 openspec/specs 和 docs/superpowers/specs 一致
- [ ] 中文撰写，无英文段落混杂
- [ ] 常见陷阱章节覆盖已知问题
