---
name: scene-system
description: 场景系统：双层地图、场景注册、冻结/解冻、传送门、场景切换。
metadata:
  type: reference
---

## 概述

多场景管理系统，支持场景冻结/解冻、传送门切换。客户端基于 PyGame 实现方形网格地图渲染，服务端维护每个场景的独立数据（TileMap、CropSystem、DropItems），支持场景无人时冻结、有人进入时恢复（补帧模拟经过时间）。

## 架构设计

### 双层地图模型

TileMap 维护两个独立二维数组：`_ground[y][x]`（地面层）和 `_objects[y][x]`（地物层）。渲染时先绘制地面层（纯色 + 2.5D 高光/阴影效果），再绘制地物层（16x16 像素精灵，带透明通道）。

**GroundType 枚举**（定义于 `constants.py`）：

| 枚举值 | ID | 说明 | walkable |
|---|---|---|---|
| GRASS | 0 | 草地 (#4a7c2e) | True |
| DIRT | 1 | 泥土 | True |
| WATER | 2 | 水面 | False |
| SAND | 3 | 沙地 | True |
| TILLED | 4 | 翻耕 | True |
| WALL | - | 墙壁（house 场景） | False |
| WOOD_FLOOR | - | 木地板（house 场景） | True |

**ObjectType 枚举**（定义于 `constants.py`）：

| 枚举值 | ID | walkable | interactable | interact_type |
|---|---|---|---|---|
| NONE | 0 | - | - | - |
| STONE | 1 | False | False | - |
| CROP_GROWING | 2 | False | True | crop |
| CROP_READY | 3 | False | True | crop |
| TREE | 4 | False | False | - |
| DOOR_IN | 10 | True | True | portal |
| DOOR_OUT | 11 | True | True | portal |
| BED | 12 | False | True | furniture |
| TV | 13 | False | True | furniture |
| STOVE | 14 | False | True | furniture |

交互双层匹配规则：优先匹配地物层（`obj:ObjectType`），无地物时匹配地面层（`gnd:GroundType`），兜底使用 `ANY`。

### 场景注册表

定义于 `scripts/client/scene/scene_defs.py`，`SCENE_DEFS` 字典注册所有场景配置：

```python
SCENE_DEFS = {
    "farm": {
        "width": 60, "height": 50,
        "generate": generate_farm_map,  # 返回 {ground[][], objects[][]}
        "portals": [...],
        "player_spawn": (x, y)
    },
    "house": {
        "width": 10, "height": 8,
        "generate": generate_house_map,
        "portals": [...],
        "player_spawn": (x, y)
    }
}
```

每个 Portal 定义包含：`pos(x, y)`、`trigger_dir`（触发方向）、`target_scene`（目标场景）、`target_portal_id`（目标 Portal ID）。

### GameSceneManager

服务端核心管理器，负责：
- 场景数据隔离：每个场景独立的 TileMap、CropSystem、DropItems
- 场景首次加载：调用 `SceneDef.generate()` 创建默认地图数据
- 场景数据持久化：保存到 DBMgr
- 存档格式：`{"active_scene": "farm", "scenes": {"farm": {...}, "house": {...}}}`

## 关键流程

### 冻结/解冻机制

1. **冻结**：最后一个玩家离开场景时，记录 `frozen_at` 时间戳，暂停该场景的 CropSystem 更新
2. **解冻**：玩家进入已冻结场景时，计算 `elapsed = now - frozen_at`，调用 `crop_system.simulate_elapsed(elapsed)` 补帧
3. **补帧示例**：场景冻结 120 秒，某作物在冻结前 30 秒种植 → 补帧后总经过 150 秒，若成熟时间 60 秒则立即成熟

### 传送门系统（双触发检测）

**触发方式一：方向触发**
- 玩家站在 Portal tile 上，按 `trigger_dir` 方向移动时触发
- 方向不匹配则不触发，正常移动

**触发方式二：交互触发**
- 玩家站在 Portal 旁（切比雪夫距离=1），面前 tile 是 Portal，按空格键触发
- 鼠标左键点击 Portal tile，检测 `interact_type='portal'` 触发

**触发后流程**：客户端发送 `SceneChangeReq {target_scene, target_portal_id}` → 服务端验证 Portal 合法性 → 冻结当前场景 → 加载目标场景 → 设置玩家位置 → 返回 `SceneChangeResp {target_scene, spawn_x, spawn_y}`

### 场景切换流程（Iris 动画状态机）

状态机：`IDLE → IRIS_CLOSE → SWITCHING → IRIS_OPEN → IDLE`

1. **IRIS_CLOSE**（300ms）：以玩家屏幕位置为圆心，圆形遮罩从最大半径收缩到 0
2. **SWITCHING**（1 帧）：屏幕全黑，SceneManager 执行场景数据替换（替换 TileMap、更新玩家位置）
3. **IRIS_OPEN**（300ms）：圆形遮罩从 0 展开到最大半径
4. 最大半径 = `sqrt(screen_w² + screen_h²) / 2`

过渡期间屏蔽所有游戏输入（移动、交互、鼠标点击），状态回到 IDLE 后恢复。

## 关键代码路径

| 文件 | 说明 |
|---|---|
| `scripts/server/game_server/src/game_scene_manager.cpp` | 服务端场景管理器实现 |
| `scripts/server/game_server/src/game_scene_manager.h` | 服务端场景管理器头文件 |
| `scripts/server/game_server/src/scene_state.cpp` | 场景状态（冻结/解冻逻辑） |
| `scripts/server/game_server/src/scene_state.h` | 场景状态头文件 |
| `scripts/client/scene/scene_defs.py` | 场景注册表（SCENE_DEFS 字典） |
| `scripts/client/scene/scene_manager.py` | 客户端 SceneManager |
| `scripts/client/scene/scene_transition.py` | Iris 过渡动效状态机 |

## 常见陷阱

- **场景数据未持久化**：服务器关闭或场景数据变更时需保存到 DBMgr，否则玩家修改丢失
- **传送门方向错误**：`trigger_dir` 必须与 Portal 期望的"走出方向"一致，设反会导致站在门上无法触发
- **冻结时间计算错误**：`elapsed = now - frozen_at`，注意时区和时间戳精度（秒级 vs 毫秒级）
- **旧存档迁移**：单层存档需自动拆分为 ground + objects 双层结构（STONE → ground:GRASS + obj:STONE）
- **补帧性能**：长时间冻结后解冻，`simulate_elapsed` 可能触发大量作物状态计算，需注意性能

## 扩展指南

### 添加新场景

1. 在 `scene_defs.py` 的 `SCENE_DEFS` 中添加新场景条目（width、height、generate、portals、player_spawn）
2. 实现 `generate_xxx_map()` 函数，返回 `{ground[][], objects[][]}`
3. 在相关场景中添加对应的 Portal 定义（双向连接）
4. 服务端 GameSceneManager 会自动处理新场景的注册和数据管理

### 添加新物体类型

1. 在 `constants.py` 的 `ObjectType` 枚举中添加新类型（ID、walkable、interactable、interact_type）
2. 在 `constants.py` 中为新类型定义 16x16 像素精灵数据
3. 如果是可交互类型，在交互系统中添加对应的处理逻辑
4. 在地图生成函数中使用新类型

## 相关 Skill

- [[crop-system]] — 作物系统，依赖场景冻结/解冻的补帧机制
- [[item-interaction]] — 物品交互系统，使用双层匹配规则
- [[game-clock]] — 游戏时钟，与场景冻结时间计算相关
