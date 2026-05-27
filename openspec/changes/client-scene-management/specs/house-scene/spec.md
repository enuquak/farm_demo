## Purpose

"家"室内场景定义：定义 house 场景的地图生成（10×8 小地图）、家具布局、新地面/地物类型（WALL、WOOD_FLOOR、BED、TV、STOVE、DOOR_IN、DOOR_OUT），以及对应的像素精灵。

## Requirements

### Requirement: 场景尺寸与生成
house 场景 SHALL 为 10×8 的小地图。

#### Scenario: 地图生成
- **WHEN** 调用 generate_house_map()
- **THEN** 返回 10×8 的 ground[][] 和 objects[][] 数组

#### Scenario: 墙壁包围
- **WHEN** house 地图生成
- **THEN** 四周一圈 ground=WALL（不可通行），内部 ground=WOOD_FLOOR

#### Scenario: 门口
- **WHEN** house 地图生成
- **THEN** 底部中央有 1~2 格 ground=WOOD_FLOOR + object=DOOR_OUT（可通行，通向 farm）

### Requirement: 家具布局
house 场景 SHALL 包含床、电视、炉子等家具地物。

#### Scenario: 床位置
- **WHEN** house 地图生成
- **THEN** 左上区域放置 BED 地物（占 1 格）

#### Scenario: 电视位置
- **WHEN** house 地图生成
- **THEN** 中间区域放置 TV 地物（占 1 格）

#### Scenario: 炉子位置
- **WHEN** house 地图生成
- **THEN** 右侧区域放置 STOVE 地物（占 1 格）

### Requirement: 新 Ground 类型
系统 SHALL 新增 WALL 和 WOOD_FLOOR 两种地面类型。

#### Scenario: WALL 地面
- **WHEN** 查询 GroundType.WALL 的属性
- **THEN** 返回 walkable=False, color=米白色

#### Scenario: WOOD_FLOOR 地面
- **WHEN** 查询 GroundType.WOOD_FLOOR 的属性
- **THEN** 返回 walkable=True, color=棕色, 有 2.5D 高光/阴影效果

### Requirement: 新 Object 类型
系统 SHALL 新增 DOOR_IN、DOOR_OUT、BED、TV、STOVE 地物类型。

#### Scenario: DOOR_IN 地物
- **WHEN** 查询 ObjectType.DOOR_IN 的属性
- **THEN** 返回 walkable=True, interactable=True, interact_type='portal'

#### Scenario: DOOR_OUT 地物
- **WHEN** 查询 ObjectType.DOOR_OUT 的属性
- **THEN** 返回 walkable=True, interactable=True, interact_type='portal'

#### Scenario: BED 地物
- **WHEN** 查询 ObjectType.BED 的属性
- **THEN** 返回 walkable=False, interactable=True, interact_type='furniture'

#### Scenario: TV 地物
- **WHEN** 查询 ObjectType.TV 的属性
- **THEN** 返回 walkable=False, interactable=True, interact_type='furniture'

#### Scenario: STOVE 地物
- **WHEN** 查询 ObjectType.STOVE 的属性
- **THEN** 返回 walkable=False, interactable=True, interact_type='furniture'

### Requirement: 家具交互预留
家具地物 SHALL 标记为可交互，但本版本不实现实际功能。

#### Scenario: 家具交互触发
- **WHEN** 玩家对 BED 按空格
- **THEN** 触发 furniture 交互回调，本版本为空实现（记录日志 "家具交互: BED"）

#### Scenario: 后续扩展点
- **WHEN** 后续版本需要实现睡觉功能
- **THEN** 只需在 furniture 交互回调中添加 BED 的处理逻辑

### Requirement: 地物像素精灵
新增的 5 种 Object 类型 SHALL 各有 16×16 像素精灵。

#### Scenario: 精灵生成
- **WHEN** 系统初始化
- **THEN** 为 DOOR_IN、DOOR_OUT、BED、TV、STOVE 各生成一个 16×16 PyGame Surface

#### Scenario: 精灵缓存
- **WHEN** 地图中有多个同类型家具
- **THEN** 共享同一个精灵 Surface 实例

### Requirement: Farm 场景房屋入口
farm 场景 SHALL 在对应位置添加房屋外观和室外门。

#### Scenario: Farm 地图中的门
- **WHEN** farm 地图生成
- **THEN** 在指定位置放置 DOOR_IN 地物（通向 house）

#### Scenario: 门的 walkable
- **WHEN** 玩家走到 DOOR_IN 位置
- **THEN** 可以正常站在上面（walkable=True）
