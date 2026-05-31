# 服务器端模块提取任务分解

## 依赖顺序

```
GameSceneManager (无依赖)
    ↓
GameClock (依赖 GameSceneManager)
    ↓
AccountMessageHandler (独立，可与上述并行)
    ↓
AdminHandler (依赖所有上述)
    ↓
GameServer 重构 (最后)
```

## Task 1：提取 GameSceneManager

### 步骤
1. 创建 `game_scene_manager.h` 和 `game_scene_manager.cpp`
2. 从 game_server.cpp 移入 `scenes_` 成员变量
3. 移入 6 个方法：`get_or_create_scene`, `handle_scene_change_req`, `save_all_scenes`, `save_scene_data`, `load_scene_data`, `make_scene_data_key`
4. 定义 `SendGameMsgFunc` 回调类型
5. 更新 game_server.h：添加 `#include "game_scene_manager.h"`，用 `unique_ptr<GameSceneManager>` 替换 `scenes_`
6. 更新 game_server.cpp：在 start() 中初始化，route 中委托
7. 更新 CMakeLists.txt 添加新源文件

### 验证
- CMake Release 构建通过
- 场景创建/切换/持久化功能不变

## Task 2：提取 GameClock

### 步骤
1. 创建 `game_clock.h` 和 `game_clock.cpp`
2. 移入 6 个时钟成员变量
3. 移入 7 个方法：`update_clock`, `broadcast_clock_sync`, `on_day_end`, `handle_force_sleep_ready`, `complete_force_sleep`, `save_clock_data`, `load_clock_data`
4. 构造函数接收 `PlayerManager*`, `GameSceneManager*`, `DBMgrConnectionManager*`
5. `complete_force_sleep` 通过注入的指针访问场景和玩家
6. 更新 game_server.h/cpp
7. 更新 CMakeLists.txt

### 验证
- CMake Release 构建通过
- 时钟推进、强制睡眠流程不变

## Task 3：提取 AccountMessageHandler

### 步骤
1. 创建 `account_message_handler.h` 和 `account_message_handler.cpp`
2. 将 `handle_account_msg()`（265 行）拆分为 5 个子方法
3. 构造函数接收 `DBMgrConnectionManager*`, `PlayerManager*`, `PlayerIdGenerator*`
4. 定义 `SendToGateFunc` 和 `SendGameMsgFunc` 回调
5. `player_id_gen_` 从 GameServer 移入此类
6. 更新 game_server.h/cpp
7. 更新 CMakeLists.txt

### 验证
- CMake Release 构建通过
- 账号查询/创建/进入游戏流程不变

## Task 4：提取 AdminHandler

### 步骤
1. 创建 `admin_handler.h` 和 `admin_handler.cpp`
2. 移入 3 个方法：`handle_admin_message`, `handle_shutdown`, `handle_shutdown_resp`
3. 构造函数接收所有子系统指针
4. `handle_shutdown` 通过注入的指针保存所有数据
5. 设置 stop 回调用于停止服务器
6. 更新 game_server.h/cpp
7. 更新 CMakeLists.txt

### 验证
- CMake Release 构建通过
- 停服流程不变

## Task 5：GameServer 重构 + 遗留清理

### 步骤
1. 从 GameServer 移除 `world_state_`, `drop_manager_`, `crop_system_` 成员
2. auto-pickup 中的 `drop_manager_` 改为 `item_handler_.drops()`
3. 更新 `update_game_logic()` 委托给子系统
4. 更新 `route_internal_message()` 委托给子系统
5. 清理不再需要的 include
6. 更新 CMakeLists.txt（如有需要）

### 验证
- CMake Release 构建通过
- 完整游戏流程测试

## 最终验证

对三个服务器执行 Release 构建：
```bash
cd scripts/server/gate_server && cmake --build build2 --config Release
cd scripts/server/game_server && cmake --build build2 --config Release
cd scripts/server/dbmgr && cmake --build build --config Release
```
