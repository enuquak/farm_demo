## Why

当前客户端只有网络测试代码（纯 TCP + Protobuf 收发），没有图形界面和游戏场景。玩家无法直观地登录、进入游戏世界、看到自己的角色。需要补充完整的 PyGame 客户端，实现"输入账号 → 进入场景 → 看到角色 → 可以移动"的完整链路，为后续游戏玩法（种田、背包、NPC）提供可视化基础。

## What Changes

- 新增 PyGame 客户端入口和游戏主循环（状态机：LOGIN → CONNECTING → PLAYING）
- 新增登录界面：输入 account_id，点击按钮或回车连接服务器
- 新增自动角色管理：查询角色列表，无角色则自动创建（role_name = account_id），有角色则直接进入
- 新增 2D 场景渲染：基于 pytmx + pyscroll 的瓦片地图、角色精灵（4方向行走动画）、摄像机跟随、HUD 叠加
- 新增美术资源：16×16 像素风瓦片集、角色精灵表、Tiled 地图文件
- 复用现有 `GateConnection` 网络层，串联 LoginReq → QueryRoles → CreateRole/EnterGame 协议流程
- 无需修改服务端代码，现有协议完全支持

## Capabilities

### New Capabilities

- `client-login-screen`: 登录界面 UI 和网络连接流程。输入 account_id，自动执行登录 → 查询角色 → 无角色则自动创建 → 进入游戏的完整流程。包含连接中/失败状态的 UI 反馈。

- `client-scene-rendering`: PyGame 2D 渲染系统。包含瓦片地图加载与渲染（pytmx + pyscroll）、玩家精灵（4方向行走动画）、摄像机跟随（平滑 lerp + 边界钳制）、碰撞检测、游戏内 HUD（角色名、坐标、场景名）。

### Modified Capabilities

（无现有 spec 需要修改）

## Impact

- **新增文件**: `scripts/client/` 下新增 PyGame 相关模块（game.py, camera.py, tilemap.py, spritesheet.py, entities/player.py, ui/login_screen.py, ui/hud.py, states/*.py）
- **新增资源**: `scripts/client/assets/` 下新增瓦片集、精灵表、地图文件、字体
- **新增依赖**: pygame, pytmx, pyscroll（Python 包）
- **复用代码**: `scripts/client/connection.py`（GateConnection）、`scripts/client/heartbeat.py`、`scripts/client/message_handler.py`
- **协议兼容**: 使用现有 MsgID 3(登录)、1001(查角色)、1003(创角色)、1005(进游戏)，服务端零改动
- **无破坏性变更**: 现有测试客户端和服务器代码不受影响
