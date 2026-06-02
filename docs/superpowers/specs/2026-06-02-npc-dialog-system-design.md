# NPC 对话交互系统设计

**日期**: 2026-06-02  
**状态**: 已批准  
**范围**: MVP 1-3 个 NPC，完整对话系统 + 好感度 + 送礼

---

## 1. 概述

为种田游戏添加 NPC 对话交互系统，支持分支对话、好感度、送礼、按时间移动的 NPC 行为。采用数据驱动架构，所有 NPC 数据通过 JSON 配置定义，代码为通用引擎。

### 1.1 功能清单

| 功能 | 描述 |
|------|------|
| NPC 对话 | 分支对话树，支持条件判断（好感度、时间、季节） |
| 好感度系统 | 送礼增减好感度，好感度影响对话内容和事件解锁 |
| 送礼系统 | 手持物品靠近 NPC 触发送礼，NPC 有喜好/厌恶 |
| NPC 调度 | NPC 按时间段出现在不同场景/位置 |
| 对话 UI | 底部对话框（头像+名字+文本+选项）+ 头顶气泡 |
| 交互触发 | 空格键（面向 NPC）和鼠标点击（点击 NPC 格子） |

---

## 2. 整体架构

```
┌─────────────────────────────────────────────────┐
│                   GameScene                      │
│                                                  │
│  ┌──────────┐    ┌──────────────┐    ┌────────┐ │
│  │ NPC      │───▶│ Dialog       │───▶│ Dialog │ │
│  │ Manager  │    │ Engine       │    │ UI     │ │
│  └──────────┘    └──────────────┘    └────────┘ │
│       │                │                    │    │
│       ▼                ▼                    ▼    │
│  ┌──────────┐    ┌──────────────┐    ┌────────┐ │
│  │ NPC      │    │ Affection   │    │ Bubble │ │
│  │ Sprite   │    │ System      │    │ UI     │ │
│  └──────────┘    └──────────────┘    └────────┘ │
└─────────────────────────────────────────────────┘
```

| 模块 | 职责 |
|------|------|
| **NPCManager** | 管理所有 NPC 实例，根据时间更新位置，处理交互触发 |
| **NPCSprite** | NPC 的视觉表现（精灵图动画、名字标签、头顶气泡） |
| **DialogEngine** | 加载对话 JSON，驱动对话树状态机，处理分支和条件判断 |
| **DialogUI** | 底部对话框渲染（头像 + 名字 + 文本 + 选项） |
| **AffectionSystem** | 好感度存储、送礼逻辑、事件解锁判定 |

---

## 3. 数据结构（JSON Schema）

### 3.1 npc_defs.json — NPC 基础定义

```json
{
  "npcs": [
    {
      "id": "merchant",
      "name": "商人老李",
      "sprite_sheet": "npc_merchant.png",
      "portrait": "portrait_merchant.png",
      "bubble_color": [70, 130, 180],
      "initial_scene": "farm",
      "default_position": {"x": 12, "y": 8}
    }
  ]
}
```

### 3.2 npc_schedule.json — 时间调度

```json
{
  "merchant": [
    {
      "time_range": ["06:00", "12:00"],
      "scene": "farm",
      "position": {"x": 12, "y": 8},
      "facing": "down"
    },
    {
      "time_range": ["12:00", "18:00"],
      "scene": "house",
      "position": {"x": 5, "y": 3},
      "facing": "left"
    },
    {
      "time_range": ["18:00", "06:00"],
      "scene": "house",
      "position": {"x": 8, "y": 6},
      "facing": "down"
    }
  ]
}
```

### 3.3 npc_dialog.json — 对话树

```json
{
  "merchant": {
    "greeting": {
      "condition": {"affection": {"min": 0}},
      "time_condition": {"morning": true},
      "lines": [
        {"speaker": "merchant", "text": "早上好啊！今天天气不错。"},
        {"speaker": "merchant", "text": "有什么需要的吗？"}
      ],
      "responses": [
        {"text": "看看有什么", "next": "browse_shop"},
        {"text": "没什么事", "next": "farewell"}
      ]
    },
    "browse_shop": {
      "lines": [
        {"speaker": "merchant", "text": "我这里种子和工具都有，慢慢挑。"}
      ],
      "effect": {"action": "open_shop"},
      "next": "farewell"
    },
    "farewell": {
      "lines": [
        {"speaker": "merchant", "text": "下次再来啊！"}
      ],
      "effect": {"affection": 1}
    },
    "high_affection": {
      "condition": {"affection": {"min": 50}},
      "lines": [
        {"speaker": "merchant", "text": "老朋友来了！今天给你打折。"}
      ],
      "responses": [
        {"text": "太好了！", "next": "browse_shop"},
        {"text": "谢谢老李", "next": "farewell"}
      ]
    }
  }
}
```

**对话节点字段**：

| 字段 | 类型 | 描述 |
|------|------|------|
| `condition` | object | 好感度门槛（`affection.min/max`） |
| `time_condition` | object | 时间窗口限制（`morning/afternoon/evening`） |
| `season` | string | 季节限制（`spring/summer/fall/winter`） |
| `lines` | array | 台词序列，支持打字机效果逐字显示 |
| `responses` | array | 玩家选项，`text` 显示文本，`next` 指向下一节点 |
| `next` | string | 无选项时自动跳转的下一节点 |
| `effect` | object | 副作用：`affection`（好感度变化）、`action`（触发动作） |

**节点选择逻辑**：按好感度从高到低排序节点，取第一个条件满足的。高好感度特殊对话自动优先。

### 3.4 npc_gifts.json — 送礼偏好

```json
{
  "merchant": {
    "loved": {"items": ["gold_ore", "diamond"], "affection": 10},
    "liked": {"items": ["iron_ore", "coal"], "affection": 5},
    "neutral": {"default": true, "affection": 1},
    "disliked": {"items": ["weed", "trash"], "affection": -3}
  }
}
```

---

## 4. 对话引擎（DialogEngine）

### 4.1 状态机

```
IDLE ──[交互触发]──▶ TYPING ──[文本完成]──▶ WAITING_INPUT
  ▲                                              │
  │                    ┌─────────────────────────┘
  │                    ▼
  │              [有选项?]──yes──▶ CHOOSING ──[选择]──▶ TYPING
  │                    │
  │                   no
  │                    ▼
  │              [有 next?]──yes──▶ TYPING
  │                    │
  │                   no
  │                    ▼
  └──────────────[关闭对话]
```

| 状态 | 行为 |
|------|------|
| **IDLE** | 无对话，游戏正常运行 |
| **TYPING** | 文字逐字显示（打字机效果），按空格跳过全部 |
| **WAITING_INPUT** | 文本显示完毕，等待玩家按空格继续 |
| **CHOOSING** | 显示选项列表，方向键选择，空格确认 |

### 4.2 条件评估器

```python
def evaluate_condition(condition, context):
    # context 包含: affection, time_slot, season, inventory
    if "affection" in condition:
        if context.affection < condition["affection"].get("min", 0):
            return False
    if "time_condition" in condition:
        if not match_time(context.time_slot, condition["time_condition"]):
            return False
    if "season" in condition:
        if context.season != condition["season"]:
            return False
    return True
```

### 4.3 送礼流程

```
玩家手持物品 + 靠近 NPC + 按空格
    │
    ▼
检查 npc_gifts.json 中该物品的分类
    │
    ▼
好感度 ±N，显示反馈气泡（❤️/💔/😐）
    │
    ▼
播放对应台词（"太喜欢了！"/"这个...不太行"）
```

---

## 5. UI 系统

### 5.1 底部对话框

```
┌─────────────────────────────────────────────────────────┐
│  ┌──────┐                                                │
│  │      │  商人老李                                        │
│  │ 头像  │  ─────────────────────────────────────          │
│  │      │  早上好啊！今天天气不错。                          │
│  └──────┘  有什么需要的吗？                                │
│                                                          │
│    ▶ 看看有什么                                           │
│      没什么事                                             │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

- **位置**：屏幕底部，宽度 80%，水平居中
- **背景**：半透明深色圆角矩形，类似现有 inventory_panel 风格
- **头像**：左侧 64x64 NPC 头像
- **名字**：头像右侧，NPC 名字，用 `bubble_color` 着色
- **文本**：名字下方，白色文字，打字机效果逐字显示
- **选项**：文本下方，`▶` 标记当前选中项，方向键上下选择
- **按键提示**：右下角小字 "空格继续" 或 "↑↓选择 空格确认"

### 5.2 头顶气泡

```
    ┌─────────┐
    │ ❤️ +5   │   ← 好感度变化反馈
    └────┬────┘
         │
    ┌────┴────┐
    │  ...    │   ← NPC 有话想说时显示省略号
    └─────────┘
    ┌─────────┐
    │ 你好！   │   ← 简短台词气泡
    └─────────┘
         │
       [NPC]
```

- **位置**：NPC 头顶上方，跟随 NPC 移动
- **类型**：
  - **省略号气泡**：NPC 可交互时默认显示 `...`，提示玩家可以对话
  - **台词气泡**：对话开始前显示一句简短招呼
  - **反馈气泡**：送礼后显示 `❤️ +5` 或 `💔 -3`，1.5 秒后消失
- **样式**：白色圆角气泡，三角箭头指向 NPC，文字居中

### 5.3 UI 层级（渲染顺序）

```
地图层 → NPC 精灵层 → 气泡层 → HUD 层 → 对话框层
```

对话框在最顶层，打开时设置 `_ui_blocking = True` 阻止游戏交互（与现有背包面板逻辑一致）。

---

## 6. NPC 精灵系统

### 6.1 精灵图规格

32x32 像素 NPC 精灵图，格式与现有 `player.png` 一致：

```
精灵图布局（每行一个方向，每列一帧）：
┌────┬────┬────┬────┐
│ D1 │ D2 │ D3 │ D4 │  ← Down（面朝下）
├────┼────┼────┼────┤
│ U1 │ U2 │ U3 │ U4 │  ← Up（面朝上）
├────┼────┼────┼────┤
│ L1 │ L2 │ L3 │ L4 │  ← Left（面朝左）
├────┼────┼────┼────┤
│ R1 │ R2 │ R3 │ R4 │  ← Right（面朝右）
└────┴────┴────┴────┘
```

每个 NPC 一张独立精灵图，存放在 `assets/sprites/npc/` 目录。

### 6.2 NPC 视觉状态

| 状态 | 表现 |
|------|------|
| **空闲** | 面朝固定方向，无动画或缓慢呼吸动画 |
| **可交互** | 头顶显示 `...` 气泡 |
| **对话中** | 面朝玩家方向，头顶无气泡 |

---

## 7. 交互集成

### 7.1 交互触发流程

```
每帧更新：
  1. 根据当前时间 + schedule 更新 NPC 位置/场景
  2. 检测玩家与 NPC 距离（Chebyshev ≤ 1）
  3. 距离内 → NPC 状态 = 可交互，显示 ... 气泡

玩家操作：
  空格键 ──▶ 检查 facing 方向格子是否有 NPC ──▶ 触发对话
  鼠标点击 ──▶ 检查点击格子是否有 NPC ──▶ 触发对话
  
  对话中：
  手持物品 + 空格 ──▶ 尝试送礼（优先于对话）
```

### 7.2 与现有系统的对接点

| 现有模块 | 对接方式 |
|----------|---------|
| `interaction.py` | 新增 `obj:NPC` 类型，效果为 `start_dialog` |
| `game_scene.py` | `_handle_mouse_click` 和 `_handle_portal_interaction` 增加 NPC 检测 |
| `input_manager.py` | 无需修改，现有 `interact` 动作直接复用 |
| `game_renderer.py` | 渲染管线增加 NPC 精灵层 + 气泡层 + 对话框层 |
| `network_dispatcher.py` | 新增 `DIALOG_START_NOTIFY` 和 `AFFECTION_SYNC` 消息类型 |
| 服务端 | 新增 `npc_handler` 处理对话触发、好感度变更、送礼验证 |

### 7.3 服务端消息

```protobuf
// player.proto 新增
message DialogStartNotify {
    string npc_id = 1;
    string dialog_node = 2;
}

message GiftReq {
    string npc_id = 1;
    string item_id = 2;
}

message GiftResp {
    int32 code = 1;           // 0=成功
    int32 affection_change = 2;
    string reaction = 3;      // loved/liked/neutral/disliked
}

message AffectionSync {
    map<string, int32> affection_map = 1;  // npc_id -> value
}
```

---

## 8. 文件清单

### 新增文件

| 文件 | 描述 |
|------|------|
| `scripts/client/npc_manager.py` | NPC 实例管理、调度更新、交互检测 |
| `scripts/client/npc_sprite.py` | NPC 精灵渲染、动画、状态管理 |
| `scripts/client/dialog_engine.py` | 对话树状态机、条件评估 |
| `scripts/client/dialog_ui.py` | 底部对话框 UI 渲染 |
| `scripts/client/bubble_ui.py` | 头顶气泡渲染 |
| `scripts/client/affection_system.py` | 好感度管理、送礼逻辑 |
| `scripts/client/data/npc_defs.json` | NPC 基础定义 |
| `scripts/client/data/npc_schedule.json` | NPC 时间调度 |
| `scripts/client/data/npc_dialog.json` | 对话树数据 |
| `scripts/client/data/npc_gifts.json` | 送礼偏好数据 |
| `assets/sprites/npc/*.png` | NPC 精灵图 |
| `assets/sprites/npc/portrait_*.png` | NPC 头像 |

### 修改文件

| 文件 | 修改内容 |
|------|---------|
| `scripts/client/interaction.py` | 新增 `obj:NPC` 匹配规则 |
| `scripts/client/game_scene.py` | 集成 NPCManager，增加 NPC 交互处理 |
| `scripts/client/game_renderer.py` | 渲染管线增加 NPC 层、气泡层、对话框层 |
| `scripts/client/network_dispatcher.py` | 新增消息处理器 |
| `scripts/client/constants.py` | 新增 NPC 相关常量 |
| `scripts/common/proto/player.proto` | 新增消息定义 |

---

## 9. 实现优先级

| 阶段 | 内容 | 依赖 |
|------|------|------|
| P1 | NPC 精灵渲染 + 固定位置 + 简单对话（无分支） | 无 |
| P2 | 对话引擎（分支、条件）+ 底部对话框 UI | P1 |
| P3 | 好感度系统 + 送礼 | P2 |
| P4 | NPC 时间调度 + 位置移动 | P1 |
| P5 | 头顶气泡 + 服务端消息同步 | P2 |
| P6 | 生成 NPC 精灵图素材 | 无（可并行） |
