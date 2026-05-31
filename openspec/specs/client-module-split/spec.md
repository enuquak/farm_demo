# 客户端模块拆分规格

## 概述

从 `GameScene`（game_scene.py 877 行）提取 3 个独立模块，并修复 constants.py 的结构问题。

## 模块 1：PlayerController

### 新文件
- `scripts/client/player_controller.py`

### 职责
- 玩家移动输入处理
- 碰撞检测与地图边界约束
- 朝向更新
- 服务器位置校正（lerp 插值）
- 位置更新定时发送

### 移入内容（从 GameScene 提取）
**方法**：
- `_handle_input(dt)` → `handle_input(dt, input_mgr)`
- `_update_facing_direction(dx, dy)` → 内部方法
- `_clamp_to_map_bounds(x, y)` → 内部方法
- `_check_walkable(x, y)` → 内部方法
- `_update_correction(dt)` → `update_correction(dt)`
- `_start_correction(server_x, server_y)` → `start_correction(server_x, server_y)`
- `_update_position_sending(dt)` → `update_position_sending(dt)`
- `_send_position_update()` → 内部方法

**状态**：
- `facing`：当前朝向
- `correction_target`：校正目标位置
- `correction_timer`：校正计时器
- `send_timer`：位置发送计时器

### 接口设计
```python
class PlayerController:
    def __init__(self, connection, tmx_map, player_sprite, scene_manager):
        self.connection = connection
        self.tmx_map = tmx_map
        self.player_sprite = player_sprite
        self.scene_mgr = scene_manager
        self.facing = Direction.DOWN
        self.correction_target = None
        self.correction_timer = 0.0
        self.send_timer = 0.0

    def handle_input(self, dt: float, input_mgr: InputManager):
        """处理移动输入、碰撞检测、朝向更新"""

    def update_correction(self, dt: float):
        """服务器位置校正插值"""

    def start_correction(self, server_x: float, server_y: float):
        """启动位置校正"""

    def update_position_sending(self, dt: float):
        """定时发送位置更新到服务器"""
```

---

## 模块 2：NetworkMessageDispatcher

### 新文件
- `scripts/client/network_dispatcher.py`

### 职责
- 从连接接收消息并分发到对应处理器
- 处理 6 种服务端消息类型

### 移入内容（从 GameScene 提取）
**方法**：
- `_process_network_messages()` → `dispatch_pending(connection)`
- `_handle_map_data(msg)` → 内部处理器
- `_handle_position_update(msg)` → 内部处理器
- `_handle_item_use_resp(msg)` → 内部处理器
- `_handle_scene_change_resp(msg)` → 内部处理器
- `_handle_clock_sync(msg)` → 内部处理器
- `_handle_force_sleep_notify(msg)` → 内部处理器

### 接口设计
```python
class NetworkMessageDispatcher:
    def __init__(self, scene_manager, player_controller, inventory,
                 energy_bar, time_hud, exhaustion_modal, connection):
        self.scene_mgr = scene_manager
        self.player_ctrl = player_controller
        self.inventory = inventory
        self.energy_bar = energy_bar
        self.time_hud = time_hud
        self.exhaustion_modal = exhaustion_modal
        self.connection = connection
        self.force_sleep_callback = None  # GameScene 注册

        self.handlers = {
            MSG_ID_MAP_DATA: self._handle_map_data,
            MSG_ID_POSITION_UPDATE: self._handle_position_update,
            MSG_ID_ITEM_USE_RESP: self._handle_item_use_resp,
            MSG_ID_SCENE_CHANGE_RESP: self._handle_scene_change_resp,
            MSG_ID_CLOCK_SYNC: self._handle_clock_sync,
            MSG_ID_FORCE_SLEEP_NOTIFY: self._handle_force_sleep_notify,
        }

    def dispatch_pending(self):
        """处理所有待处理的网络消息"""

    def _handle_map_data(self, msg): ...
    def _handle_position_update(self, msg): ...
    def _handle_item_use_resp(self, msg): ...
    def _handle_scene_change_resp(self, msg): ...
    def _handle_clock_sync(self, msg): ...
    def _handle_force_sleep_notify(self, msg): ...
```

---

## 模块 3：GameRenderer

### 新文件
- `scripts/client/game_renderer.py`

### 职责
- 协调渲染一帧的完整流程
- 管理 HUD、能量条、时间显示、体力耗尽弹窗

### 移入内容（从 GameScene 提取）
**方法**：
- `_render()` → `render(map_group, player_sprite, scene_transition=None)`

**管理的 UI 组件**：
- `HUD`（基本信息）
- `EnergyBar`（能量条）
- `TimeHUD`（时间显示）
- `ExhaustionModal`（体力耗尽弹窗）

### 接口设计
```python
class GameRenderer:
    def __init__(self, screen):
        self.screen = screen
        self.hud = HUD()
        self.energy_bar = EnergyBar(screen)
        self.time_hud = TimeHUD(screen)
        self.exhaustion_modal = ExhaustionModal(screen)

    def render(self, map_group, player_sprite, scene_transition=None):
        """渲染完整一帧"""

    def set_energy(self, current, max_val):
        self.energy_bar.set_energy(current, max_val)

    def set_hud_info(self, **kwargs):
        self.hud.set_info(**kwargs)

    def cleanup(self):
        """清理 UI 资源"""
```

---

## constants.py 拆分

### 新文件
- `scripts/client/sprite_data.py`

### 移入内容
将 `constants.py` 中的 `OBJECT_SPRITES` 字典（~300 行像素精灵数据）移到 `sprite_data.py`。

8 种物体的 16x16 像素数组和颜色映射：
- STONE, CROP_GROWING, CROP_READY, TREE
- DOOR_IN, DOOR_OUT, BED, TV, STOVE

### constants.py 保留内容（~120 行）
- 路径常量（`ASSETS_DIR`, `SPRITE_DIR` 等）
- 枚举（`GroundType`, `ObjectType`, `Direction`）
- 数值设置（`TILE_SIZE`, `MOVE_SPEED`, `ANIMATION_SPEED` 等）
- 新增缺失常量（见下方）

---

## 修复断开导入

在 `constants.py` 中补充以下缺失常量：

### icon_manager.py 需要
```python
ITEM_ICON_PALETTE = {
    # 根据 icon_manager.py 的渲染逻辑定义颜色映射
}
```

### ui/hotbar.py 需要
```python
HOTBAR_SLOT_COUNT = 10
HOTBAR_SLOT_SIZE = 40
HOTBAR_SLOT_GAP = 4
HOTBAR_MARGIN_BOTTOM = 10
HOTBAR_BG_COLOR = (40, 40, 40)
HOTBAR_BORDER_COLOR = (100, 100, 100)
HOTBAR_ACTIVE_COLOR = (255, 215, 0)
HOTBAR_TEXT_COLOR = (255, 255, 255)
HOTBAR_TEXT_SHADOW_COLOR = (0, 0, 0)
```

### ui/inventory_panel.py 需要
```python
PANEL_SLOT_SIZE = 48
PANEL_SLOT_GAP = 4
PANEL_COLS = 6
PANEL_ROWS = 5
PANEL_BG_COLOR = (30, 30, 30)
PANEL_BORDER_COLOR = (100, 100, 100)
PANEL_TITLE_COLOR = (255, 215, 0)
PANEL_CLOSE_BTN_COLOR = (200, 50, 50)
PANEL_OVERLAY_ALPHA = 180
```

---

## connection.py 清理

### 变更
- 移除 `send_heartbeat()` 方法 → `HeartbeatManager` 直接调用 `connection.send_message()`
- 移除 `send_login_request()` 方法 → `LoginFlowManager` 自己构造并发送

### 影响
- `heartbeat.py`：修改发送心跳的方式
- `login_flow.py`：修改发送登录请求的方式
- `connection.py`：只保留纯传输层职责

---

## 删除 Dead Code

### 删除文件
- `scripts/client/camera.py`（GameScene 使用 pyscroll 内置相机）
- `scripts/client/tile_renderer.py`（GameScene 使用 pyscroll.BufferedRenderer）
- `scripts/client/message_handler.py`（GameScene 和 LoginFlow 各自内联分发）

### 更新
- `scripts/client/__init__.py`：移除 `MessageHandler` 导出

---

## GameScene 重构

### 提取后的 GameScene 结构（~200 行）
```python
class GameScene:
    def __init__(self, connection, login_data):
        # PyGame 初始化
        self.screen = pygame.display.set_mode(...)
        self.clock = pygame.time.Clock()

        # 子系统初始化
        self.scene_mgr = SceneManager(...)
        self.player_ctrl = PlayerController(connection, tmx_map, player_sprite, self.scene_mgr)
        self.net_dispatcher = NetworkMessageDispatcher(...)
        self.renderer = GameRenderer(self.screen)
        self.input_mgr = InputManager()
        self.inventory = Inventory()

    def run(self):
        """主循环"""
        while self.running:
            dt = self.clock.tick(60) / 1000.0
            self._handle_pygame_events()
            self.player_ctrl.handle_input(dt, self.input_mgr)
            self.player_ctrl.update_correction(dt)
            self.player_ctrl.update_position_sending(dt)
            self.net_dispatcher.dispatch_pending()
            self.scene_mgr.update(dt)
            self.renderer.render(...)

    def _handle_pygame_events(self):
        """处理 PyGame 事件（退出、鼠标点击等）"""
        # 鼠标点击：物品使用、传送门交互
```
