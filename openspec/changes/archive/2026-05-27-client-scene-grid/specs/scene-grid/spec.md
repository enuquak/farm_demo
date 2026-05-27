## Purpose

客户端场景网格系统：基于 PyGame 实现方形网格地图渲染，支持多种地形类型，提供以玩家为中心的正交投影相机，以及从服务器同步地图数据的能力。作为所有客户端游戏可视化功能的基础。

## Requirements

### Requirement: TileMap 数据结构
客户端 SHALL 维护本地 TileMap 二维数组，存储地图数据。

#### Scenario: 初始化 TileMap
- **WHEN** 客户端收到服务器下发的 MapDataNotify（width=60, height=50）
- **THEN** 创建 60×50 的二维数组，填充地面类型 ID

#### Scenario: 获取指定位置的地面类型
- **WHEN** 调用 `tile_map.get_ground(tile_x, tile_y)`
- **THEN** 返回该位置的 GroundType 枚举值

#### Scenario: 坐标越界
- **WHEN** 调用 `tile_map.get_ground(-1, 50)` 或 `tile_map.get_ground(60, 0)`
- **THEN** 返回 None 或抛出异常（不允许越界访问）

### Requirement: 地形类型定义
系统 SHALL 在 `constants.py` 中定义 GroundType 枚举，支持至少 5 种地形。

#### Scenario: 地形类型枚举
- **WHEN** 系统初始化
- **THEN** 定义以下地形类型：GRASS(0, 草地), DIRT(1, 泥土), WATER(2, 水面), SAND(3, 沙地), TILLED(4, 翻耕)

#### Scenario: 地形关联属性
- **WHEN** 查询 GroundType.GRASS 的属性
- **THEN** 返回包含 color（颜色值）、walkable（是否可通行）的属性集

#### Scenario: 水面不可通行
- **WHEN** 查询 GroundType.WATER 的 walkable 属性
- **THEN** 返回 False

### Requirement: 网格坐标与像素坐标互转
系统 SHALL 支持网格坐标（tileX, tileY）与像素坐标（pixelX, pixelY）之间的转换。

#### Scenario: 网格坐标转像素坐标
- **WHEN** 调用 `tile_to_pixel(3, 5)` 且 TILE_SIZE=32
- **THEN** 返回 (96, 160)

#### Scenario: 像素坐标转网格坐标
- **WHEN** 调用 `pixel_to_tile(100, 165)` 且 TILE_SIZE=32
- **THEN** 返回 (3, 5)

### Requirement: TileMap 渲染
系统 SHALL 使用 PyGame 将 TileMap 渲染到屏幕上，仅渲染相机视口内的可见区域。

#### Scenario: 渲染可见区域
- **WHEN** 相机视口覆盖 tile (10,10) 到 (35,29)
- **THEN** 仅渲染该范围内的 tile，每个 tile 绘制为 TILE_SIZE×TILE_SIZE 的纯色方块

#### Scenario: 地形颜色映射
- **WHEN** 渲染 GRASS 类型的 tile
- **THEN** 绘制为绿色 (#4a7c2e) 方块

#### Scenario: 2.5D 高光/阴影效果
- **WHEN** 渲染任意地面 tile
- **THEN** tile 顶部和左侧绘制高光色，底部和右侧绘制阴影色，产生立体感

### Requirement: 正交投影相机
系统 SHALL 实现以玩家为中心的正交投影相机，支持地图边缘约束。

#### Scenario: 相机跟随玩家
- **WHEN** 玩家位于像素坐标 (960, 800)，视口大小 800×600
- **THEN** 相机左上角位于 (560, 500)，玩家居中于视口

#### Scenario: 地图左上角约束
- **WHEN** 玩家位于像素坐标 (100, 100)，视口大小 800×600
- **THEN** 相机左上角位于 (0, 0)，不出现地图外的空白区域

#### Scenario: 地图右下角约束
- **WHEN** 玩家位于地图右下角附近
- **THEN** 相机左上角不超过 (mapPixelW - viewportW, mapPixelH - viewportH)

### Requirement: 屏幕坐标转世界坐标
Camera SHALL 支持将屏幕像素坐标转换为世界网格坐标。

#### Scenario: 屏幕中心转世界坐标
- **WHEN** 相机位于 (560, 500)，调用 `camera.screen_to_world(400, 300)`
- **THEN** 返回世界像素坐标 (960, 800)

#### Scenario: 屏幕坐标转 tile 坐标
- **WHEN** 调用 `camera.screen_to_tile(400, 300)` 且 TILE_SIZE=32
- **THEN** 返回 tile 坐标 (30, 25)

### Requirement: 地图数据同步
客户端 SHALL 在登录成功后从服务器接收完整地图数据。

#### Scenario: 登录后接收地图数据
- **WHEN** 客户端发送 EnterGameResp 成功后
- **THEN** 服务器发送 MapDataNotify，包含 width、height、ground[][]、objects[][] 数据

#### Scenario: 客户端构建 TileMap
- **WHEN** 客户端收到 MapDataNotify
- **THEN** 根据数据构建本地 TileMap 并开始渲染

#### Scenario: 地图数据为空（新场景）
- **WHEN** 服务器下发的 MapDataNotify 中 ground 数据为空
- **THEN** 客户端使用 SceneDef 的 generate() 函数生成默认地图

### Requirement: 游戏主循环集成
场景渲染 SHALL 集成到 PyGame 游戏主循环中。

#### Scenario: 主循环渲染顺序
- **WHEN** 每帧渲染
- **THEN** 按以下顺序执行：清屏 → 更新相机 → 渲染 TileMap → 渲染玩家 → 刷新显示

#### Scenario: 帧率控制
- **WHEN** 游戏运行
- **THEN** 帧率 SHALL 维持在 60 FPS，使用 `pygame.time.Clock()` 控制
