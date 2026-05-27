## Why

Farm Demo 客户端当前只有登录界面和网络连接能力，缺少游戏世界的可视化渲染。需要实现客户端场景网格系统，让玩家登录后能看到可视化的游戏世界——基于方形网格的 TileMap 渲染，支持多种地形类型（草地、泥土、水面、石头等），为后续的玩家移动、物品交互、种植等系统提供可视化的场景基础。

## What Changes

- **新增 PyGame TileMap 渲染模块**：基于 PyGame 实现方形网格地图渲染，支持多种地形类型（纯色占位符，预留纹理替换扩展点）
- **新增场景数据结构**：客户端本地维护 TileMap 二维数组，从服务器同步地图数据
- **新增相机系统**：以玩家为中心的正交投影相机，支持地图边缘约束
- **新增地图数据同步协议**：客户端从服务器接收地图数据（Ground + Objects 双层），本地渲染
- **扩展 Game Server**：地图数据管理，玩家进入时下发地图数据

## Capabilities

### New Capabilities

- `scene-grid`: 客户端场景网格系统——PyGame TileMap 渲染、地形类型定义、网格坐标系、相机跟随、地图数据从服务器同步

### Modified Capabilities

- `client-login-screen`: 登录成功后切换到游戏场景渲染状态
- `game-server`: 新增地图数据管理与下发逻辑
- `player-entity`: 新增 scene_id 字段用于场景数据路由

## Impact

- **新增文件**: `scripts/client/scene/` 目录（tile_map.py、camera.py、renderer.py、constants.py）
- **修改文件**: `scripts/client/main.py`（游戏主循环集成场景渲染）、`scripts/server/`（地图数据管理）
- **新增依赖**: PyGame（客户端渲染）
- **协议变更**: 新增地图数据同步消息（MsgID 待分配）
