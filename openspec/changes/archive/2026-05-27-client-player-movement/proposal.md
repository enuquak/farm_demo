## Why

Farm Demo 客户端登录后能看到游戏场景，但玩家角色还无法在地图上移动。需要实现客户端玩家移动系统，让玩家可以通过键盘（WASD）控制角色在网格地图上自由移动，同时实现客户端预测 + 服务器权威的位置同步架构。

## What Changes

- **新增客户端 Player 实体**：PyGame 渲染的玩家角色精灵，支持四向朝向
- **实现客户端移动逻辑**：WASD 键盘输入驱动的自由像素级移动，帧率无关
- **实现客户端预测**：客户端先移动再发包给服务器，减少延迟感
- **实现位置同步协议**：客户端定期发送位置更新，服务器验证后广播
- **实现服务器端位置验证**：Game Server 校验玩家位置合法性（速度、碰撞）
- **实现相机跟随**：相机中心锁定玩家位置，地图边缘约束

## Capabilities

### New Capabilities

- `player-movement`: 客户端玩家移动系统——WASD 自由像素级移动、四向朝向、客户端预测、位置同步协议

### Modified Capabilities

- `scene-grid`: 集成玩家渲染到场景渲染管线
- `game-server`: 新增玩家位置验证与广播逻辑
- `player-entity`: 新增 position 字段的运行时更新

## Impact

- **新增文件**: `scripts/client/player.py`（玩家实体 + 移动逻辑）
- **修改文件**: `scripts/client/main.py`（输入处理 + 玩家更新）、`scripts/server/`（位置验证）
- **协议变更**: 新增位置同步消息（MsgID 待分配）
