## Why

服务器端 `game_server.cpp`（1645 行）和客户端 `game_scene.py`（877 行）是两个最大的单体文件，各自承担了过多职责：
- **game_server.cpp**：连接管理、消息路由、玩家生命周期、场景管理、时钟系统、账号消息处理、管理消息处理、物品交互——全部耦合在一个类中
- **game_scene.py**：游戏循环、输入处理、玩家移动、碰撞检测、网络消息分发、位置校正、场景切换、渲染协调——全部耦合在一个类中

单体文件导致：代码难以定位、修改容易引入回归、无法独立测试子系统。

## What Changes

### 服务器端（C++）

从 `GameServer` 类提取 4 个独立子系统：

| 新模块 | 职责 | 估计行数 |
|--------|------|----------|
| `GameSceneManager` | 场景创建/查找/切换/持久化 | ~180 |
| `GameClock` | 游戏时钟、天数推进、强制睡眠 | ~260 |
| `AccountMessageHandler` | 账号相关消息处理（查询角色、创建角色、进入游戏等） | ~280 |
| `AdminHandler` | 管理消息处理（停服请求/响应） | ~90 |

重构后 `GameServer` 缩减为纯基础设施编排器（~500 行），只保留：
- libevent 生命周期
- 连接管理（accept/read/disconnect/heartbeat）
- 消息路由（route_internal_message）
- 消息发送辅助
- update_game_logic 定时器协调

同时清理遗留成员：`world_state_`、`drop_manager_`、`crop_system_`（已由 ItemInteractionHandler 管理）。

### 客户端（Python）

从 `GameScene` 类提取 3 个独立模块：

| 新模块 | 职责 | 估计行数 |
|--------|------|----------|
| `PlayerController` | 玩家移动、碰撞检测、位置校正、位置发送 | ~200 |
| `NetworkMessageDispatcher` | 网络消息分发（6 种消息类型） | ~150 |
| `GameRenderer` | 渲染协调（HUD、能量条、时间显示、弹窗） | ~120 |

重构后 `GameScene` 缩减为游戏循环编排器（~200 行）。

额外清理：
- **constants.py 拆分**：将 ~300 行像素精灵数据移到 `sprite_data.py`
- **修复断开导入**：补充 `ITEM_ICON_PALETTE`、`HOTBAR_*`、`PANEL_*` 常量
- **删除 dead code**：`camera.py`、`tile_renderer.py`、`message_handler.py`
- **connection.py 清理**：将 `send_heartbeat`、`send_login_request` 移到调用方

## Capabilities

### New Capabilities

- `server-module-extraction`: 服务器端 GameServer 按功能拆分为 GameSceneManager、GameClock、AccountMessageHandler、AdminHandler 四个独立模块
- `client-module-split`: 客户端 GameScene 按功能拆分为 PlayerController、NetworkMessageDispatcher、GameRenderer 三个独立模块

### Modified Capabilities

- `game-server`: GameServer 重构为纯基础设施编排器，持有上述子系统的 unique_ptr
- `scene-management`: 服务器端场景管理逻辑迁移到 GameSceneManager
- `game-clock`: 时钟逻辑迁移到 GameClock 类
- `login-flow`: 客户端 LoginFlowManager 不变，但 connection.py 协议方法迁移

## Impact

### 服务器端
- **新增文件**: `game_clock.h/.cpp`、`game_scene_manager.h/.cpp`、`account_message_handler.h/.cpp`、`admin_handler.h/.cpp`（8 个文件）
- **修改文件**: `game_server.h`、`game_server.cpp`、`CMakeLists.txt`
- **不修改**: gate_server、dbmgr、scripts/server/common

### 客户端
- **新增文件**: `player_controller.py`、`network_dispatcher.py`、`game_renderer.py`、`sprite_data.py`（4 个文件）
- **修改文件**: `game_scene.py`、`constants.py`、`connection.py`、`__init__.py`
- **删除文件**: `camera.py`、`tile_renderer.py`、`message_handler.py`
- **不修改**: scene/、ui/、login_flow.py、login_screen.py
