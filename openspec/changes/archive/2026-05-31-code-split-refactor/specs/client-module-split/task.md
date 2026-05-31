# 客户端模块拆分任务分解

## 依赖顺序

```
Task 1: constants.py 修复 + sprite_data.py 拆分（基础设施）
    ↓
Task 2: PlayerController 提取（独立）
Task 3: NetworkMessageDispatcher 提取（独立）
Task 4: GameRenderer 提取（独立）
    ↓
Task 5: GameScene 重构（依赖上述所有）
    ↓
Task 6: connection.py 清理 + dead code 删除
```

## Task 1：修复 constants.py + 拆分 sprite_data.py

### 步骤
1. 创建 `sprite_data.py`，将 `OBJECT_SPRITES` 从 constants.py 移入
2. 更新 `object_sprite_manager.py` 的 import
3. 在 constants.py 中补充缺失常量：
   - `ITEM_ICON_PALETTE`（读取 icon_manager.py 确定格式）
   - `HOTBAR_*` 系列（读取 ui/hotbar.py 确定值）
   - `PANEL_*` 系列（读取 ui/inventory_panel.py 确定值）

### 验证
- Python 导入无报错：`python -c "from scripts.client.constants import *"`

## Task 2：提取 PlayerController

### 步骤
1. 创建 `player_controller.py`
2. 从 GameScene 移入 8 个方法（handle_input, update_facing_direction, clamp_to_map_bounds, check_walkable, update_correction, start_correction, update_position_sending, send_position_update）
3. 移入相关状态变量（facing, correction_target, correction_timer, send_timer）
4. 构造函数接收 connection, tmx_map, player_sprite, scene_manager

### 验证
- Python 导入无报错
- 移动/碰撞/位置校正逻辑不变

## Task 3：提取 NetworkMessageDispatcher

### 步骤
1. 创建 `network_dispatcher.py`
2. 从 GameScene 移入 dispatch_pending 和 6 个消息处理器
3. 使用 dict 分发表替代 if/elif 链
4. force_sleep 回调通过构造函数注入

### 验证
- Python 导入无报错
- 消息分发逻辑不变

## Task 4：提取 GameRenderer

### 步骤
1. 创建 `game_renderer.py`
2. 从 GameScene 移入 render 方法
3. 管理 HUD, EnergyBar, TimeHUD, ExhaustionModal

### 验证
- Python 导入无报错
- 渲染逻辑不变

## Task 5：GameScene 重构

### 步骤
1. 删除 GameScene 中已移出的方法
2. 在 __init__ 中初始化 PlayerController, NetworkMessageDispatcher, GameRenderer
3. 简化 run() 为协调循环
4. 保留 _handle_pygame_events（鼠标点击处理）

### 验证
- Python 导入无报错
- 游戏主循环功能不变

## Task 6：connection.py 清理 + dead code 删除

### 步骤
1. 从 connection.py 移除 `send_heartbeat()` 方法
2. 更新 heartbeat.py 使用 `connection.send_message()`
3. 从 connection.py 移除 `send_login_request()` 方法
4. 更新 login_flow.py 自己构造并发送
5. 删除 `camera.py`, `tile_renderer.py`, `message_handler.py`
6. 更新 `__init__.py` 移除 MessageHandler 导出

### 验证
- Python 导入无报错
- 心跳/登录流程不变

## 最终验证

```bash
cd scripts/client
python -c "from scripts.client.game_scene import GameScene; print('OK')"
python -c "from scripts.client.player_controller import PlayerController; print('OK')"
python -c "from scripts.client.network_dispatcher import NetworkMessageDispatcher; print('OK')"
python -c "from scripts.client.game_renderer import GameRenderer; print('OK')"
python -c "from scripts.client.constants import *; print('OK')"
python -c "from scripts.client.sprite_data import OBJECT_SPRITES; print('OK')"
```
