## Purpose

Portal 传送门系统：定义 Portal 数据结构，支持双触发检测（站在门上往外走 + 门旁按空格/鼠标交互），实现场景之间的连接。

## Requirements

### Requirement: Portal 数据结构
每个 Portal SHALL 包含位置、触发方向、目标场景和目标 Portal ID。

#### Scenario: Portal 定义
- **WHEN** 场景注册表中定义 Portal
- **THEN** 包含以下字段：pos(x,y)、trigger_dir（触发方向）、target_scene（目标场景）、target_portal_id（目标 Portal ID）

#### Scenario: Farm 场景的 Portal
- **WHEN** 查询 SCENE_DEFS["farm"]["portals"]
- **THEN** 返回包含至少 1 个 Portal：位置在房屋入口，target_scene="house"

#### Scenario: House 场景的 Portal
- **WHEN** 查询 SCENE_DEFS["house"]["portals"]
- **THEN** 返回包含至少 1 个 Portal：位置在门口，target_scene="farm"

### Requirement: Portal 可通行
Portal tile SHALL 是 walkable 的，玩家可以站在上面。

#### Scenario: 玩家走到 Portal 上
- **WHEN** Portal 位于 tile (25, 48)，玩家移动到该 tile
- **THEN** 玩家可以正常站在 Portal tile 上，不被阻挡

### Requirement: 方向触发检测
系统 SHALL 在玩家位于 Portal tile 上且按触发方向移动时检测到触发。

#### Scenario: 站在 Portal 上往外走
- **WHEN** 玩家在 Portal tile (25, 48)，trigger_dir="down"，玩家按 S 键向下移动
- **THEN** 检测到 Portal 触发，发送 SceneChangeReq

#### Scenario: 方向不匹配不触发
- **WHEN** 玩家在 Portal tile (25, 48)，trigger_dir="down"，玩家按 W 键向上移动
- **THEN** 不触发 Portal，正常移动

### Requirement: 空格/鼠标交互触发
玩家 SHALL 能在 Portal 旁对 Portal 按空格或鼠标点击触发场景切换。

#### Scenario: 门旁按空格
- **WHEN** 玩家站在 Portal 旁（切比雪夫距离=1），面前 tile 是 Portal
- **THEN** 按空格键触发场景切换

#### Scenario: 鼠标点击 Portal
- **WHEN** 玩家鼠标左键点击 Portal tile
- **THEN** 检测 interact_type='portal'，发送 SceneChangeReq

### Requirement: Portal 与地物类型关联
DOOR_IN 和 DOOR_OUT 地物类型 SHALL 标记为 portal 交互类型。

#### Scenario: DOOR_IN 交互
- **WHEN** 交互目标是 DOOR_IN 地物
- **THEN** interact_type='portal'，触发场景切换到室内

#### Scenario: DOOR_OUT 交互
- **WHEN** 交互目标是 DOOR_OUT 地物
- **THEN** interact_type='portal'，触发场景切换到室外

### Requirement: 目标位置映射
场景切换后玩家 SHALL 出现在目标 Portal 的对应位置。

#### Scenario: Portal 对应关系
- **WHEN** 从 farm 的 door_in Portal 切换到 house
- **THEN** 玩家出现在 house 的 door_out Portal 旁边

#### Scenario: 指定出生点
- **WHEN** SceneChangeResp 包含 spawn_x, spawn_y
- **THEN** 玩家出现在指定坐标
