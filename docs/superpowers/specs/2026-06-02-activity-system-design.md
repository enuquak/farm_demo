# 运营活动系统设计文档

> 日期：2026-06-02
> 状态：待实现

## 概述

新增运营活动功能，包含两个核心子系统：

1. **通用触发器系统（TriggerEngine）**：支持布尔表达式（AND/OR/NOT + 括号）的条件评估引擎，供活动系统和任务系统共用
2. **活动系统（Activity System）**：支持时间窗口、Buff 增益、条件达成领奖的运营活动

同时对现有任务系统进行改造，将 objectives 替换为通用触发器表达式。

## 设计决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 触发器归属 | 通用模块，活动和任务共用 | 统一条件评估逻辑，减少重复代码 |
| 活动配置方式 | 静态 JSON 文件 | 与任务系统风格一致，简单可靠 |
| 触发器表达式格式 | 树结构（AST），单条件简写 | 不需要额外解析器，灵活且直观 |
| 活动效果类型 | Buff 增益 + 条件达成领奖 | 覆盖运营活动的常见需求 |
| 任务系统改造 | objectives 替换为 TriggerNode | 彻底统一，避免概念重复 |

---

## 一、通用触发器系统

### 1.1 核心数据结构

```cpp
// 触发项（叶子节点）—— 对应一个可量化的游戏行为
struct TriggerItem {
    std::string id;            // 唯一标识
    std::string type;          // 触发类型：CollectItem, PlantCrop, HarvestCrop, TalkToNPC, VisitScene, CraftItem, UseItem, ReachLevel, OwnGold, Custom
    std::string target;        // 触发目标，如 "wheat", "wood"
    int32_t count = 0;         // 目标数量
    std::string description;   // 描述文本（给客户端展示用）
};

// 逻辑运算符
enum class LogicOp { AND, OR, NOT };

// 触发器节点（树结构）
struct TriggerNode {
    bool is_leaf = true;

    // 叶子节点字段
    TriggerItem item;

    // 逻辑节点字段
    LogicOp op = LogicOp::AND;
    std::vector<TriggerNode> children;  // AND/OR: 2+ 个子节点; NOT: 恰好 1 个子节点
};
```

### 1.2 运行时进度

```cpp
struct TriggerProgress {
    std::string trigger_id;                                    // 所属活动/任务 ID
    std::unordered_map<std::string, int32_t> item_progress;   // item_id -> 当前计数
    bool is_complete = false;
};
```

### 1.3 引擎接口

```cpp
class TriggerEngine {
public:
    // 评估触发器表达式
    bool evaluate(const TriggerNode& root, const TriggerProgress& progress) const;

    // 处理游戏事件，更新进度，返回是否有变化
    bool on_game_event(TriggerProgress& progress, const TriggerNode& root,
                       EventType event_type, const std::string& target, int32_t count = 1);

    // 收集所有叶子节点（用于初始化进度）
    void collect_items(const TriggerNode& root, std::vector<TriggerItem>& out) const;
};
```

### 1.4 评估逻辑

- **AND**：所有子节点都为 true → true
- **OR**：任一子节点为 true → true
- **NOT**：唯一子节点为 false → true
- **叶子节点**：`item_progress[item.id] >= item.target_count` → true

### 1.5 事件匹配

TriggerEngine 订阅 EventBus 的游戏事件。收到事件时：

1. 遍历 TriggerNode 树的所有叶子节点
2. 匹配条件：`item.type` 对应的 EventType == 事件类型 且 `item.target` == 事件中的 target
3. 匹配成功则累加 `item_progress[item.id]`
4. 重新评估整棵树，更新 `is_complete`

### 1.6 JSON 格式

单条件（简写，直接是 TriggerItem）：

```json
{ "type": "HarvestCrop", "target": "wheat", "count": 100 }
```

多条件组合（TriggerNode）：

```json
{
  "op": "AND",
  "children": [
    { "id": "item_1", "type": "HarvestCrop", "target": "wheat", "count": 100 },
    { "op": "OR", "children": [
      { "id": "item_2", "type": "CollectItem", "target": "wood", "count": 50 },
      { "op": "NOT", "children": [
        { "id": "item_3", "type": "CollectItem", "target": "stone", "count": 30 }
      ]}
    ]}
  ]
}
```

---

## 二、活动系统

### 2.1 活动数据结构

```cpp
enum class ActivityState : int32_t {
    NOT_STARTED = 0,
    ACTIVE      = 1,
    ENDED       = 2,
};

struct ActivityBuffDef {
    std::string buff_type;       // 如 "harvest_multiplier", "exp_multiplier"
    std::string target;          // 作用目标（空=全局）
    double multiplier = 1.0;     // 倍率
};

struct ActivityRewardDef {
    std::vector<std::string> items;
    int32_t gold = 0;
    int32_t exp = 0;
};

struct ActivityDef {
    std::string id;
    std::string name;
    std::string description;
    int64_t start_time;                  // Unix 时间戳
    int64_t end_time;
    TriggerNode trigger;                 // 完成条件
    std::vector<ActivityBuffDef> buffs;  // Buff 增益
    ActivityRewardDef rewards;           // 条件达成奖励
    bool auto_claim = false;             // true=自动发放, false=手动领取
};
```

### 2.2 活动配置示例

```json
{
  "activities": {
    "act_001": {
      "id": "act_001",
      "name": "丰收节",
      "description": "活动期间，小麦收获数量翻倍！累计收获10个小麦可领取额外奖励。",
      "start_time": 1748880000,
      "end_time": 1749052800,
      "trigger": {
        "type": "HarvestCrop",
        "target": "wheat",
        "count": 10
      },
      "buffs": [
        { "buff_type": "harvest_multiplier", "target": "wheat", "multiplier": 2.0 }
      ],
      "rewards": {
        "items": ["fertilizer"],
        "gold": 200,
        "exp": 100
      },
      "auto_claim": false
    }
  }
}
```

### 2.3 ActivityManager

```cpp
class ActivityManager {
public:
    bool load_from_file(const std::string& file_path);

    // 定时检查活动状态（每秒调用）
    void tick(int64_t now);

    // 获取当前活跃活动列表
    std::vector<const ActivityDef*> get_active_activities() const;

    // 获取玩家的活动进度
    const TriggerProgress* get_progress(uint64_t player_id, const std::string& activity_id) const;

    // 处理游戏事件
    void on_game_event(uint64_t player_id, EventType type, const std::string& target, int32_t count);

    // 领取奖励
    bool claim_reward(uint64_t player_id, const std::string& activity_id);

    // 查询 buff 倍率（供游戏逻辑调用）
    double get_buff_multiplier(uint64_t player_id, const std::string& buff_type,
                               const std::string& target) const;

    // 持久化
    std::string get_progress_json(uint64_t player_id) const;
    void load_progress(uint64_t player_id, const std::string& json_str);
};
```

### 2.4 Buff 生效机制

游戏逻辑中执行产出操作时，查询 ActivityManager：

```cpp
// 示例：收获时
int actual_count = base_count * activity_mgr_->get_buff_multiplier(player_id, "harvest_multiplier", crop_id);
```

Buff 查询逻辑：
1. 遍历所有 ACTIVE 状态的活动
2. 检查活动的 buffs 列表
3. 匹配 `buff_type` 和 `target`
4. 返回乘数（多个 buff 叠乘）

### 2.5 活动生命周期

```
NOT_STARTED ──(时间到达 start_time)──> ACTIVE ──(时间超过 end_time)──> ENDED
                                            │
                                  (玩家完成触发条件)
                                            │
                                  (领取奖励 / 自动发放)
```

`tick()` 方法每秒检查所有活动，根据当前时间更新状态。状态变更时推送 `ActivitySyncNotify` 给所有在线玩家。

---

## 三、任务系统改造

### 3.1 结构变更

**QuestDef：**
```cpp
struct QuestDef {
    std::string id;
    std::string name;
    std::string description;
    std::string type;
    TriggerDef trigger;              // 接取条件（保持不变：Auto / NPC）
    TriggerNode condition;           // ★ 替换原 objectives，用触发器表达式描述完成条件
    QuestRewardDef rewards;
    std::vector<std::string> prerequisites;
    std::string branch_group;
};
```

**TaskInfo：**
```cpp
struct TaskInfo {
    std::string task_id;
    TaskState state = TaskState::NOT_ACCEPTED;
    TriggerProgress trigger_progress;   // ★ 替换原 progress map
    int64_t accept_time = 0;
    int64_t complete_time = 0;
};
```

### 3.2 QuestManager 改造

- 移除 `ObjectiveType` 枚举和 `ObjectiveDef` 结构
- 移除 `objective_type_to_event_type()` 映射
- `on_game_event()` 改为调用 `TriggerEngine::on_game_event()` 更新 `TaskInfo::trigger_progress`
- `check_quest_completion()` 改为调用 `TriggerEngine::evaluate()`
- `handle_quest_submit()` 中的完成检查改为调用 `TriggerEngine::evaluate()`
- 序列化/反序列化适配新的 `TriggerProgress` 结构

### 3.3 配置文件迁移

现有 `quest_data.json` 的 `objectives` 数组迁移为 `trigger` 结构：

```json
// 改造前
"objectives": [
  { "id": "obj_001", "type": "PlantCrop", "target": "wheat", "count": 3, "description": "种植3个小麦" },
  { "id": "obj_002", "type": "HarvestCrop", "target": "wheat", "count": 3, "description": "收获3个小麦" }
]

// 改造后（隐式 AND → 显式 AND）
"trigger": {
  "op": "AND",
  "children": [
    { "id": "obj_001", "type": "PlantCrop", "target": "wheat", "count": 3, "description": "种植3个小麦" },
    { "id": "obj_002", "type": "HarvestCrop", "target": "wheat", "count": 3, "description": "收获3个小麦" }
  ]
}
```

### 3.4 影响范围

| 文件 | 改动 |
|------|------|
| `quest_types.h` | 移除 ObjectiveType/ObjectiveDef，QuestDef 改用 TriggerNode，TaskInfo 改用 TriggerProgress |
| `quest_types.cpp` | 移除 ObjectiveType 转换函数 |
| `quest_config.cpp` | 解析逻辑改为读取 trigger 字段 |
| `quest_manager.cpp` | 事件处理改为调用 TriggerEngine |
| `config/quest_data.json` | 数据格式迁移 |
| `tools/quest_editor/` | 编辑器 UI 适配新触发器结构 |

---

## 四、客户端 UI

### 4.1 活动图标（ActivityIcon）

位置：右上角，固定边距。

- 有活动进行中：图标高亮，右上角小红点提示
- 无活动时：图标灰色或不显示
- 点击：打开活动面板

新增文件：`scripts/client/ui/activity_icon.py`

### 4.2 活动面板（ActivityPanel）

点击图标后弹出的居中面板，显示：

- 活动名称和描述
- 活动时间窗口和剩余时间
- Buff 效果说明
- 触发条件进度（每个 TriggerItem 的当前/目标计数）
- 奖励内容
- 领取按钮（条件满足且未领取时可点击）

新增文件：`scripts/client/ui/activity_panel.py`

### 4.3 ActivityHandler

网络消息处理器，管理活动数据和进度。

新增文件：`scripts/client/activity_handler.py`

### 4.4 集成点

- `GameRenderer`：新增 ActivityIcon 渲染（在 Iris 遮罩之前）
- `GameScene`：新增 ActivityHandler，处理图标点击和面板交互
- 活动面板打开时屏蔽游戏输入（类似对话框）

---

## 五、网络协议

### 5.1 Message ID 分配

活动系统使用 7001-7010 区间：

| ID | 名称 | 方向 | 说明 |
|----|------|------|------|
| 7001 | MSG_ID_ACTIVITY_SYNC_NOTIFY | S→C | 活动列表同步 |
| 7002 | MSG_ID_ACTIVITY_PROGRESS_NOTIFY | S→C | 活动进度更新 |
| 7003 | MSG_ID_ACTIVITY_CLAIM_REQ | C→S | 领取奖励请求 |
| 7004 | MSG_ID_ACTIVITY_CLAIM_RESP | S→C | 领取结果响应 |

### 5.2 Protobuf 定义

新增 `activity.proto`：

```protobuf
syntax = "proto3";

message ActivityBuffInfo {
    string buff_type = 1;
    string target = 2;
    double multiplier = 3;
}

message ActivityRewardInfo {
    repeated string items = 1;
    int32 gold = 2;
    int32 exp = 3;
}

message ActivityInfo {
    string activity_id = 1;
    string name = 2;
    string description = 3;
    int64 start_time = 4;
    int64 end_time = 5;
    repeated ActivityBuffInfo buffs = 6;
    ActivityRewardInfo rewards = 7;
    bool can_claim = 8;
    bool claimed = 9;
    int32 state = 10;
}

message ActivitySyncNotify {
    repeated ActivityInfo activities = 1;
}

message TriggerItemProgress {
    string item_id = 1;
    string type = 2;
    string target = 3;
    int32 target_count = 4;
    int32 current_count = 5;
    string description = 6;
}

message ActivityProgressNotify {
    string activity_id = 1;
    repeated TriggerItemProgress items = 2;
    bool is_complete = 3;
}

message ActivityClaimReq {
    string activity_id = 1;
}

message ActivityClaimResp {
    int32 code = 1;
    string msg = 2;
    string activity_id = 3;
}
```

### 5.3 数据流

```
登录:
  Server → Client: ActivitySyncNotify

活动进行中（每次事件触发后）:
  Server → Client: ActivityProgressNotify

领取奖励:
  Client → Server: ActivityClaimReq
  Server → Client: ActivityClaimResp
  Server → Client: ActivitySyncNotify (更新状态)

活动开始/结束:
  Server → Client: ActivitySyncNotify
```

---

## 六、文件清单

### 新增文件

| 文件 | 说明 |
|------|------|
| `scripts/server/game_server/src/trigger_engine.h` | 通用触发器引擎头文件 |
| `scripts/server/game_server/src/trigger_engine.cpp` | 通用触发器引擎实现 |
| `scripts/server/game_server/src/activity_types.h` | 活动数据结构 |
| `scripts/server/game_server/src/activity_config.h/cpp` | 活动配置加载 |
| `scripts/server/game_server/src/activity_manager.h/cpp` | 活动管理器 |
| `scripts/common/proto/activity.proto` | 活动 Protobuf 定义 |
| `scripts/client/ui/activity_icon.py` | 活动图标组件 |
| `scripts/client/ui/activity_panel.py` | 活动详情面板 |
| `scripts/client/activity_handler.py` | 活动网络消息处理 |
| `config/activity_data.json` | 活动配置文件 |

### 修改文件

| 文件 | 改动 |
|------|------|
| `scripts/server/game_server/src/quest_types.h` | 移除 ObjectiveType/ObjectiveDef，QuestDef/TaskInfo 改用触发器结构 |
| `scripts/server/game_server/src/quest_types.cpp` | 移除 ObjectiveType 转换 |
| `scripts/server/game_server/src/quest_config.h/cpp` | 解析逻辑适配 |
| `scripts/server/game_server/src/quest_manager.h/cpp` | 核心逻辑改用 TriggerEngine |
| `scripts/server/game_server/src/game_server.h/cpp` | 新增 ActivityManager、TriggerEngine |
| `scripts/server/game_server/src/event_bus.h` | 如需新增事件类型 |
| `scripts/common/proto/player.proto` | 新增活动消息引用（或使用独立 activity.proto） |
| `scripts/client/game_renderer.py` | 新增 ActivityIcon 渲染 |
| `scripts/client/game_scene.py` | 集成 ActivityHandler |
| `scripts/client/network_dispatcher.py` | 新增活动消息路由 |
| `shared/message_ids.json` | 新增 7001-7004 |
| `tools/quest_editor/property_panel.py` | 触发器编辑 UI 适配 |
| `config/quest_data.json` | 数据格式迁移 |

---

## 七、测试要点

1. **TriggerEngine 单元测试**：AND/OR/NOT/嵌套组合的评估正确性
2. **活动生命周期**：时间窗口正确切换状态
3. **Buff 生效**：收获倍率正确叠加
4. **进度同步**：事件触发后客户端收到正确的进度更新
5. **奖励领取**：满足条件可领取，领取后状态更新，不可重复领取
6. **任务系统回归**：现有任务功能不受影响（配置迁移后行为等价）
7. **任务系统新能力**：验证 OR/NOT 条件的任务可以正常完成
