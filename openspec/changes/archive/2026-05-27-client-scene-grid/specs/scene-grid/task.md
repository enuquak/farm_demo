# Task: scene-grid

## 1. GroundType 枚举与常量定义 [已完成 ✅]
- 1.1 创建 `scripts/client/constants.py`，定义 `TILE_SIZE = 32` 和 `GroundType` 枚举（GRASS=0, DIRT=1, WATER=2, SAND=3, TILLED=4）
- 1.2 为每个 GroundType 定义属性：color（颜色元组）、walkable（布尔值）

## 2. TileMap 数据结构 [已完成 ✅]
- 2.1 创建 `scripts/client/tile_map.py`，实现 `TileMap` 类
- 2.2 实现 `__init__(self, width, height)` 构造函数，初始化二维数组
- 2.3 实现 `get_ground(tile_x, tile_y)` 方法，含越界检查

## 3. 坐标转换工具函数 [已完成 ✅]
- 3.1 在 `tile_map.py` 中实现 `tile_to_pixel(tile_x, tile_y)` 返回像素坐标
- 3.2 在 `tile_map.py` 中实现 `pixel_to_tile(pixel_x, pixel_y)` 返回网格坐标

## 4. 正交投影相机 [已完成 ✅]
- 4.1 创建 `scripts/client/camera.py`，实现 `Camera` 类
- 4.2 实现 `follow(player_pixel_x, player_pixel_y)` 方法，使相机居中于玩家
- 4.3 实现地图边缘约束（左上角和右下角 clamp）
- 4.4 实现 `screen_to_world(screen_x, screen_y)` 和 `screen_to_tile(screen_x, screen_y)` 方法

## 5. TileMap 渲染器 [已完成 ✅]
- 5.1 创建 `scripts/client/tile_renderer.py`，实现 `TileRenderer` 类
- 5.2 实现视口裁剪，仅渲染可见区域内的 tile
- 5.3 实现地形颜色映射（GRASS=#4a7c2e 等）
- 5.4 实现 2.5D 高光/阴影效果（顶部左侧高光，底部右侧阴影）

## 6. 地图数据同步 [已完成 ✅]
- 6.1 在 `msg_ids.py` 中添加 MapDataNotify 消息 ID
- 6.2 实现 MapDataNotify 处理逻辑，解析 width、height、ground[][] 数据
- 6.3 支持空地图数据时使用默认生成逻辑

## 7. 游戏主循环集成 [已完成 ✅]
- 7.1 创建 `scripts/client/game_scene.py`，实现 `GameScene` 类
- 7.2 集成 TileMap、Camera、TileRenderer
- 7.3 实现主循环：清屏 -> 更新相机 -> 渲染 TileMap -> 渲染玩家 -> 刷新显示
- 7.4 使用 `pygame.time.Clock()` 控制 60 FPS
- 7.5 修改 `main.py`，登录成功后进入 GameScene
