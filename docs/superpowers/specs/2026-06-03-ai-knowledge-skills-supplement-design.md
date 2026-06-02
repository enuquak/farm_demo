# AI Knowledge Skills 补充设计文档

## 背景

项目已有 10 个 AI Knowledge Skill 文件（server-architecture、scene-system、crop-system 等），覆盖了核心子系统。但自 2026-06-02 以来，项目新增了 18 个子系统的设计文档（specs）和实施计划（plans），其中 6 个正在活跃开发中，尚无对应的 skill 文件。

## 目标

1. 为全部 18 个新子系统创建独立的 skill 文件
2. 保持与现有 skill 一致的细粒度风格（一对一映射）
3. 遵循标准 skill 结构（frontmatter → 概述 → 架构 → 流程 → 代码路径 → 陷阱 → 扩展 → 相关 skill）
4. 完成后 skill 总数从 10 增加到 28

## 设计决策

### 1. 组织方案：一对一映射

每个 spec 对应一个独立 skill，原因：
- 与现有 10 个 skill 保持一致的细粒度风格
- AI 按需加载精准上下文，不引入无关信息
- 与 openspec/specs 一一对应，便于溯源和维护

### 2. 语言：中文

与现有 skills 保持一致。

### 3. 内容来源

从以下来源提炼，以"开发指南"形式重新组织：
- `docs/superpowers/specs/` — 设计文档
- `docs/superpowers/plans/` — 实施计划
- `openspec/specs/` — 规范文档
- 实际代码

## Skill 清单

### 总计：18 个新 skill

| # | Skill | 定位 | 源文档 |
|---|-------|------|--------|
| 1 | `shared-constants` | 共享常量系统、代码生成 | specs: shared-constants-design |
| 2 | `global-player-id` | Player ID 生成与管理 | specs: global-player-id-design |
| 3 | `etcd-service-discovery` | etcd 服务发现集成 | specs: etcd-service-discovery-design + plan |
| 4 | `dbmgr-redis-mongo-enhancement` | DBMgr 数据层增强 | specs: dbmgr-redis-mongo-enhancement-design |
| 5 | `chat-system` | 聊天系统架构与实现 | specs: chat-system-design + plan |
| 6 | `friend-system` | 好友系统架构与实现 | specs: friend-system-design + 2 plans |
| 7 | `team-system` | 组队系统架构与实现 | specs: team-system-design + plan |
| 8 | `monster-combat` | 战斗系统架构与实现 | specs: monster-combat-system-design + plan |
| 9 | `quest-system` | 任务系统架构与实现 | specs: quest-system-design + plan |
| 10 | `npc-dialog` | NPC 对话系统 | specs: npc-dialog-system-design + plan |
| 11 | `activity-system` | 活动系统架构 | specs: activity-system-design |
| 12 | `player-load-path` | 玩家加载路径修复 | specs: player-load-path-fix-design + plan |
| 13 | `player-movement` | 玩家移动完整性 | specs: player-movement-completeness-design + plan |
| 14 | `cross-server` | 跨服架构与实现 | specs: cross-server-design + plan |
| 15 | `client-notification` | 客户端通知系统 | specs: client-notification-system-design + plan |
| 16 | `config-editor` | 配置编辑器 | specs: config-editor-design + plan |
| 17 | `code-split-refactor` | 代码拆分重构 | specs: code-split-refactor-design + plan |
| 18 | `ai-knowledge-skills` | Skill 体系元信息 | specs: ai-knowledge-skills-design + plan |

---

## 详细设计

### 1. `shared-constants/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-01-shared-constants-design.md`

**内容：**
1. 共享常量系统架构（shared/*.json → 自动生成 C++/Python）
2. 消息 ID 管理（message_ids.json 格式、分区规则：1000-1999 客户端、2000-2999 登录、3000-3299 Gate-Game、4000-4299 Game-DBMgr、5000-5999 管理、6000-6999 聊天）
3. 错误码管理（error_codes.json 格式、命名约定）
4. 代码生成脚本（tools/generate_constants.py 的使用方法）
5. 添加新常量的完整步骤
6. 常见陷阱：常量不同步、ID 冲突、生成脚本未运行

**关键代码路径：**
- 共享常量 JSON：`shared/message_ids.json`, `shared/error_codes.json`
- 生成脚本：`tools/generate_constants.py`
- 生成的 C++ 头文件：`scripts/server/common/include/message_ids.h`, `error_codes.h`
- 生成的 Python 模块：`scripts/client/message_ids.py`, `scripts/client/error_codes.py`

---

### 2. `global-player-id/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-01-global-player-id-design.md`

**内容：**
1. Player ID 格式：`(server_id << 20) | sequence`，支持最多 4096 个服务器，每个服务器 1048576 个玩家
2. PlayerIdGenerator 类（std::mutex 线程安全、std::atomic 计数器）
3. ID 分配流程（CreateRoleReq 时生成、唯一性保证）
4. 容量规划与扩展策略
5. 常见陷阱：ID 冲突、sequence 溢出、server_id 范围越界

**关键代码路径：**
- ID 生成器：`scripts/server/game_server/src/player_id_generator.h`
- 使用位置：`scripts/server/game_server/src/game_server.cpp`（CreateRoleReq 处理）

---

### 3. `etcd-service-discovery/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-01-etcd-service-discovery-design.md` + plan

**内容：**
1. etcd 集成架构（Lease 注册、Watch 发现）
2. 服务注册流程（启动时注册、定期续约、TTL 配置）
3. 服务发现流程（Watch 前缀、变更回调、本地缓存）
4. 与现有 JSON 配置系统的共存策略（etcd 优先、JSON 降级）
5. 健康检查与故障转移
6. 常见陷阱：Lease 过期、Watch 断开重连、etcd 集群不可用时的降级

**关键代码路径：**
- etcd 客户端封装：`scripts/server/common/src/etcd_client.h/cpp`
- 服务注册：各服务的 `main.cpp` 启动流程
- 配置：`config/*.json` 中的 etcd 配置段

---

### 4. `dbmgr-redis-mongo-enhancement/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-dbmgr-redis-mongo-enhancement-design.md`

**内容：**
1. MongoDB 集成（连接管理、集合设计、索引策略）
2. Redis 缓存层（读穿透缓存、写穿透失效、TTL 配置）
3. ConnectionManager 状态机（DISCONNECTED → CONNECTING → CONNECTED / FAILED / FAILED_PERMANENT）
4. 数据路由（player_id % dbmgr_count、hash(account_id) % dbmgr_count）
5. 与现有 `dbmgr-data-layer` skill 的关系（增强而非替代，新 skill 覆盖 Redis/Mongo 细节）
6. 常见陷阱：连接池耗尽、缓存一致性、索引缺失、MongoDB ObjectId 处理

**关键代码路径：**
- MongoDB 连接：`scripts/server/dbmgr/src/mongo_connection.h/cpp`
- Redis 连接：`scripts/server/dbmgr/src/redis_connection.h/cpp`
- ConnectionManager：`scripts/server/dbmgr/src/connection_manager.h/cpp`
- 数据路由：`scripts/server/dbmgr/src/data_router.h/cpp`

---

### 5. `chat-system/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-chat-system-design.md` + plan

**内容：**
1. Chat Server 架构（独立进程、Channel 管理器）
2. 频道类型（世界频道、场景频道、私聊频道、系统频道）
3. 消息路由（GateServer 将 msg_id 6000-6999 转发到 ChatServer）
4. 消息协议（ChatMsgReq/Resp、频道订阅/退订）
5. 客户端 UI（ChatPanel 组件、频道切换 Tab、消息滚动、输入框）
6. 历史消息（Redis 存储最近 50 条、新玩家加入时加载）
7. 常见陷阱：消息顺序、频道订阅遗漏、客户端线程安全、消息过滤

**关键代码路径：**
- Chat Server：`scripts/server/chat_server/src/`
- GateServer 路由：`scripts/server/gate_server/src/gate_server.cpp`（route_message 6000+ 分支）
- 客户端 UI：`scripts/client/ui/chat_panel.py`
- 频道配置：`scripts/client/chat_channel.py`
- Protobuf：`scripts/common/proto/chat.proto`

---

### 6. `friend-system/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-friend-system-design.md` + 2 plans

**内容：**
1. Friend Server 架构（独立进程或 Game Server 内置 Stub）
2. 好友关系模型（双向确认、好友列表上限、黑名单）
3. 在线状态同步（Redis pub/sub 跨进程广播）
4. 好友请求流程（发送 → 接受/拒绝 → 双向添加）
5. 好友数据持久化（DBMgr 存储、friend_list / black_list 字段）
6. 常见陷阱：并发请求处理、在线状态延迟、好友列表上限溢出

**关键代码路径：**
- Friend Server：`scripts/server/friend_server/src/`
- FriendManager：`scripts/server/friend_server/src/friend_manager.h/cpp`
- Protobuf：`scripts/common/proto/friend.proto`
- 消息 ID：`shared/message_ids.json`（friend 相关条目）

---

### 7. `team-system/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-team-system-design.md` + plan

**内容：**
1. 组队架构（Team Manager 管理所有队伍、Team 实体持有成员列表）
2. 组队流程（创建队伍 → 邀请成员 → 接受/拒绝 → 加入 → 离开/解散）
3. 队伍状态管理（成员列表、队长转移规则、队伍上限）
4. 组队消息广播（成员状态变更通知）
5. 与其他系统的集成入口（副本匹配、战斗组队）
6. 常见陷阱：队长离线处理、成员状态同步、队伍数据持久化时机

**关键代码路径：**
- Team Manager：`scripts/server/game_server/src/team_manager.h/cpp`
- Team 实体：`scripts/server/game_server/src/team.h`
- Protobuf：`scripts/common/proto/team.proto`

---

### 8. `monster-combat/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-monster-combat-system-design.md` + plan

**内容：**
1. 战斗模型（HP/MaxHP、Attack、Defense、经验值、伤害公式）
2. Monster 实体（Spawn 点配置、AI 状态机、死亡掉落表）
3. 战斗流程（客户端 AttackReq → 服务端伤害计算 → CombatResult 广播）
4. 客户端集成（GameScene 战斗逻辑、MonsterSprite、HP 条渲染）
5. 战利品系统（DropItem 复用现有机制）
6. 常见陷阱：伤害计算溢出、怪物刷新重叠、战斗状态同步延迟、死亡动画与实体清理时序

**关键代码路径：**
- 战斗处理：`scripts/server/game_server/src/combat_handler.h/cpp`
- Monster 实体：`scripts/server/game_server/src/monster.h/cpp`
- 客户端战斗：`scripts/client/combat/` 目录
- Protobuf：`scripts/common/proto/combat.proto`

---

### 9. `quest-system/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-quest-system-design.md` + plan

**内容：**
1. 任务模型（Quest 定义、任务状态机：AVAILABLE → ACTIVE → COMPLETED → SUBMITTED）
2. 任务类型（主线任务、支线任务、日常任务）
3. 任务目标类型（收集物品、击杀怪物、NPC 对话、探索区域）
4. 任务奖励（经验值、金币、物品、解锁功能）
5. 任务数据持久化（玩家任务进度存储在 PlayerBizData）
6. 常见陷阱：任务状态持久化遗漏、目标计数重置、任务链顺序错误

**关键代码路径：**
- Quest Manager：`scripts/server/game_server/src/quest_manager.h/cpp`
- Quest 定义：`config/quests.json`
- 客户端 UI：`scripts/client/ui/quest_panel.py`
- Protobuf：`scripts/common/proto/quest.proto`

---

### 10. `npc-dialog/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-npc-dialog-system-design.md` + plan

**内容：**
1. NPC 实体模型（NPC 定义、位置配置、对话树绑定）
2. 对话系统架构（DialogTree → DialogNode → DialogOption）
3. 对话触发（交互按键检测、距离检测、NPC 面向）
4. 客户端 UI（DialogPanel 组件、NPC 名称、对话文本、选项按钮）
5. 对话效果（跳转节点、触发任务、给予物品、关闭对话）
6. 与任务系统的集成（任务触发对话、任务提交对话）
7. 常见陷阱：对话树循环引用、NPC 位置同步、对话状态与任务状态不一致

**关键代码路径：**
- NPC 管理：`scripts/server/game_server/src/npc_manager.h/cpp`
- 对话树定义：`config/dialogs/` 目录
- 客户端 UI：`scripts/client/ui/dialog_panel.py`
- Protobuf：`scripts/common/proto/npc.proto`

---

### 11. `activity-system/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-activity-system-design.md`

**内容：**
1. 活动模型（Activity 定义、时间窗口、状态机：PENDING → ACTIVE → ENDED）
2. 活动类型（限时活动、周期活动、节日活动）
3. 活动奖励（排行榜、参与奖励、成就奖励）
4. 活动配置（JSON 配置文件、cron 表达式）
5. 常见陷阱：时区处理、活动状态持久化、跨天活动边界

**关键代码路径：**
- Activity Manager：`scripts/server/game_server/src/activity_manager.h/cpp`
- 活动配置：`config/activities.json`
- Protobuf：`scripts/common/proto/activity.proto`

---

### 12. `player-load-path/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-player-load-path-fix-design.md` + plan

**内容：**
1. 问题描述（现有加载路径的缺陷：字段丢失、格式不一致）
2. load_from_json() 新方法（统一的 JSON 反序列化入口）
3. 字段兼容策略（缺失字段使用默认值、多余字段忽略）
4. 数据迁移（旧格式自动升级、版本号字段）
5. 常见陷阱：JSON 格式兼容性、字段缺失导致崩溃、默认值不合理

**关键代码路径：**
- Player 加载：`scripts/server/game_server/src/player.cpp`（load_from_json）
- Player 序列化：`scripts/server/game_server/src/player.cpp`（get_all_data_json）
- 测试：`scripts/server/game_server/tests/`

---

### 13. `player-movement/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-player-movement-completeness-design.md` + plan

**内容：**
1. 移动模型（客户端预测 + 服务器权威）
2. 位置同步协议（客户端每 100ms 上报 PosSync、服务器 200ms lerp 修正）
3. 碰撞检测（客户端预判 + 服务端验证、双层地图碰撞）
4. 移动速度与方向（标准化对角线移动、速度常量）
5. 常见陷阱：位置抖动（客户端/服务器不一致）、碰撞穿透（帧率差异）、网络延迟补偿

**关键代码路径：**
- 客户端移动：`scripts/client/player_controller.py`
- 服务端验证：`scripts/server/game_server/src/player_manager.cpp`
- 碰撞检测：`scripts/client/scene/tile_map.py`
- Protobuf：`scripts/common/proto/player.proto`（PosSync）

---

### 14. `cross-server/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-cross-server-design.md` + plan

**内容：**
1. 跨服架构（CrossServerService 管理跨服连接）
2. GameConnection / GameSession 管理（连接池、会话复用）
3. RouteCache 路由缓存（玩家 → 目标服务器映射）
4. 消息路由（跨服消息转发、玩家迁移流程）
5. 数据同步（跨服状态一致性、迁移数据打包）
6. 常见陷阱：路由缓存失效、会话状态丢失、消息乱序、迁移中断恢复

**关键代码路径：**
- CrossServerService：`scripts/server/game_server/src/cross_server_service.h/cpp`
- GameConnection：`scripts/server/game_server/src/game_connection.h/cpp`
- GameSession：`scripts/server/game_server/src/game_session.h/cpp`
- RouteCache：`scripts/server/game_server/src/route_cache.h/cpp`

---

### 15. `client-notification/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-client-notification-system-design.md` + plan

**内容：**
1. 通知系统架构（NotificationManager 统一管理）
2. 通知类型（Toast 轻提示、Banner 横幅、Modal 模态框）
3. 通知队列与优先级（紧急 > 重要 > 普通）
4. 客户端 UI 实现（动画效果、自动消失、手动关闭）
5. 常见陷阱：通知堆积、输入冲突、多通知重叠显示

**关键代码路径：**
- NotificationManager：`scripts/client/ui/notification_manager.py`
- Toast 组件：`scripts/client/ui/toast.py`
- Banner 组件：`scripts/client/ui/banner.py`

---

### 16. `config-editor/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-config-editor-design.md` + plan

**内容：**
1. 配置编辑器架构（Web UI + REST API）
2. 配置文件格式（JSON Schema 验证、类型约束）
3. 热更新机制（文件监听、变更通知、服务重载）
4. 权限控制（只读/读写角色）
5. 常见陷阱：配置格式校验遗漏、并发修改冲突、热更新时机不当

**关键代码路径：**
- Web UI：`tools/config-editor/` 目录
- API 服务：`tools/config-editor/server.py`
- 配置 Schema：`config/schema/` 目录

---

### 17. `code-split-refactor/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-code-split-refactor-design.md` + plan

**内容：**
1. 重构目标（大文件拆分、职责分离、可维护性提升）
2. 拆分策略（按职责划分、单一职责原则）
3. 已完成的拆分案例（game_scene.py → PlayerController + NetworkDispatcher + GameRenderer + GameScene 协调层）
4. 依赖注入模式（构造函数注入、回调函数解耦）
5. 常见陷阱：循环导入、依赖注入遗漏、拆分后接口不一致

**关键代码路径：**
- 拆分后的模块：`scripts/client/player_controller.py`, `network_dispatcher.py`, `game_renderer.py`, `game_scene.py`
- 设计文档：`docs/superpowers/specs/2026-06-02-code-split-refactor-design.md`

---

### 18. `ai-knowledge-skills/SKILL.md`

**源文档：** `docs/superpowers/specs/2026-06-02-ai-knowledge-skills-design.md` + plan

**内容：**
1. Skill 体系架构（.claude/skills/ 目录结构、frontmatter 规范）
2. Skill 编写指南（内容来源：openspec/specs + docs/superpowers/specs + 代码）
3. 标准章节模板（概述 → 架构 → 流程 → 代码路径 → 陷阱 → 扩展 → 相关 skill）
4. 交叉引用规范（[[skill-name]] 标记、指向存在的 skill）
5. 常见陷阱：内容与 spec 不一致、交叉引用断裂、frontmatter 缺失

**关键代码路径：**
- Skill 目录：`.claude/skills/`
- 设计文档：`docs/superpowers/specs/2026-06-02-ai-knowledge-skills-design.md`
- 实施计划：`docs/superpowers/plans/2026-06-02-ai-knowledge-skills.md`

---

## 实施顺序

按依赖关系和优先级排序：

**第一批（基础设施，无依赖）：**
1. `shared-constants`
2. `global-player-id`
3. `etcd-service-discovery`

**第二批（数据层增强）：**
4. `dbmgr-redis-mongo-enhancement`

**第三批（社交系统，依赖基础设施）：**
5. `chat-system`
6. `friend-system`
7. `team-system`

**第四批（战斗与内容，依赖核心系统）：**
8. `monster-combat`
9. `quest-system`
10. `npc-dialog`
11. `activity-system`

**第五批（玩家系统修复）：**
12. `player-load-path`
13. `player-movement`

**第六批（跨服与客户端）：**
14. `cross-server`
15. `client-notification`

**第七批（工具与流程）：**
16. `config-editor`
17. `code-split-refactor`

**第八批（元信息）：**
18. `ai-knowledge-skills`

## 验证标准

- [ ] 每个 skill 文件有完整的 frontmatter（name、description、metadata.type: reference）
- [ ] 每个 skill 包含关键代码路径章节
- [ ] 交叉引用正确指向存在的 skill（使用 [[skill-name]] 标记）
- [ ] 内容与 docs/superpowers/specs 一致
- [ ] 中文撰写，无英文段落混杂
- [ ] 常见陷阱章节覆盖已知问题
- [ ] 与现有 10 个 skill 风格一致
