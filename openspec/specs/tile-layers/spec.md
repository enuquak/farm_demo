## Purpose

地图双层模型：将单层 TileMap 拆分为地面层（Ground）和地物层（Object），地物使用像素精灵渲染（带透明通道），实现"物体放在地面上"的自然视觉效果。同时适配交互系统的双层匹配。

## Requirements

### Requirement: 双层数据结构
TileMap SHALL 维护两个独立二维数组：`_ground[y][x]` 和 `_objects[y][x]`。

#### Scenario: 初始化双层 TileMap
- **WHEN** 收到 MapDataNotify，包含 ground[][] 和 objects[][] 两层数组
- **THEN** 分别存储到 `_ground` 和 `_objects` 两个二维数组中

#### Scenario: 获取指定位置的地物
- **WHEN** 调用 `tile_map.get_object(tx, ty)` 且该位置有石头
- **THEN** 返回 ObjectType.STONE

#### Scenario: 无地物位置
- **WHEN** 调用 `tile_map.get_object(tx, ty)` 且该位置无地物
- **THEN** 返回 None

#### Scenario: 修改地物
- **WHEN** 调用 `tile_map.set_object(tx, ty, ObjectType.CROP_GROWING)`
- **THEN** 该位置的地物更新为 CROP_GROWING

### Requirement: 地物类型定义
系统 SHALL 在 `constants.py` 中定义 ObjectType 枚举，包含像素精灵数据。

#### Scenario: 基础地物类型
- **WHEN** 系统初始化
- **THEN** 定义以下地物类型：NONE(0), STONE(1), CROP_GROWING(2), CROP_READY(3), TREE(4)

#### Scenario: 场景切换地物类型
- **WHEN** 系统初始化
- **THEN** 定义以下地物类型：DOOR_IN(10), DOOR_OUT(11), BED(12), TV(13), STOVE(14)

#### Scenario: 地物关联属性
- **WHEN** 查询 ObjectType.STONE 的属性
- **THEN** 返回包含 walkable（False）、interactable（False）、sprite（16×16 像素数据）的属性集

#### Scenario: 可交互地物属性
- **WHEN** 查询 ObjectType.DOOR_IN 的属性
- **THEN** 返回包含 walkable（True）、interactable（True）、interact_type（'portal'）的属性集

### Requirement: 地物像素精灵
每种地物类型 SHALL 关联一个 16×16 像素精灵，渲染时拉伸到 TILE_SIZE×TILE_SIZE。

#### Scenario: 地物精灵生成
- **WHEN** 系统初始化
- **THEN** 为每种 ObjectType 生成 16×16 PyGame Surface，使用 NEAREST 采样拉伸到 TILE_SIZE

#### Scenario: 透明通道
- **WHEN** 渲染 STONE 地物精灵
- **THEN** 精灵中透明像素不遮挡下层地面

#### Scenario: 精灵缓存
- **WHEN** 多个位置都有 STONE 地物
- **THEN** 所有 STONE 地物共享同一个精灵 Surface 实例（不重复创建）

### Requirement: 分层渲染
TileMap SHALL 分两遍渲染：先地面层，再地物层。

#### Scenario: 渲染顺序
- **WHEN** 每帧渲染 TileMap
- **THEN** 第一遍渲染所有可见 tile 的地面层（纯色 + 2.5D 效果），第二遍渲染所有非 None 的地物层（精灵）

#### Scenario: 地物覆盖在地面之上
- **WHEN** 位置 (10, 5) 的地面是 GRASS，地物是 STONE
- **THEN** 先绘制绿色草地，再在上面绘制石头精灵，草地从石头透明部分透出

### Requirement: 交互双层匹配
物品交互系统 SHALL 优先匹配地物层，无地物时匹配地面层。

#### Scenario: 有地物时匹配地物
- **WHEN** 目标位置有 STONE 地物，玩家手持斧头
- **THEN** 使用 `obj:STONE` 作为 ITEM_EFFECTS 匹配键

#### Scenario: 无地物时匹配地面
- **WHEN** 目标位置无地物，地面是 GRASS，玩家手持锄头
- **THEN** 使用 `gnd:GRASS` 作为 ITEM_EFFECTS 匹配键

#### Scenario: 兜底匹配
- **WHEN** 目标位置无地物，地面类型无匹配效果，玩家手持面包
- **THEN** 使用 `ANY` 作为兜底匹配键

### Requirement: 旧存档迁移
服务端 SHALL 在加载旧格式存档时自动拆分为双层结构。

#### Scenario: 旧单层存档迁移
- **WHEN** 服务端加载存档，发现 `mapData` 为单层数组（旧格式）
- **THEN** 自动拆分为 ground + objects 双层：STONE→ground:GRASS+obj:STONE，TILLED→ground:TILLED+obj:None

#### Scenario: 新格式存档不迁移
- **WHEN** 服务端加载存档，已有 `ground` 和 `objects` 字段
- **THEN** 直接使用，不执行迁移
