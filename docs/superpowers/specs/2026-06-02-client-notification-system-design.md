# 客户端提示系统设计文档

**日期**：2026-06-02
**状态**：已批准
**作者**：AI Assistant

---

## 1. 概述

### 1.1 目标

为农场游戏客户端实现一个完整的提示系统，支持两种主要类型的提示：
- **Toast（弹出提示）**：用于时间事件、操作反馈、系统公告等
- **Marquee（跑马灯）**：用于滚动公告、活动通知等

### 1.2 设计原则

- **像素风格**：参考《星露谷物语》的视觉风格
- **优先级管理**：重要提示可以打断普通提示
- **多通道支持**：Toast 和 Marquee 独立运行
- **易于扩展**：支持添加新的通知类型和通道

---

## 2. 整体架构

### 2.1 系统分层

```
┌─────────────────────────────────────────────────────┐
│                   GameScene                          │
│                      │                               │
│                      ▼                               │
│            NotificationManager                       │
│            (统一管理入口)                              │
│         ┌─────────┴─────────┐                       │
│         ▼                   ▼                       │
│    ToastChannel      MarqueeChannel                 │
│    (弹出提示)         (跑马灯)                        │
│         │                   │                       │
│         ▼                   ▼                       │
│    ToastRenderer     MarqueeRenderer                │
│    (渲染+动画)        (渲染+滚动)                     │
└─────────────────────────────────────────────────────┘
```

### 2.2 核心组件

- **NotificationManager**：统一管理入口，接收通知请求，分发到对应通道
- **ToastChannel**：管理弹出提示的优先级队列和显示逻辑
- **MarqueeChannel**：管理跑马灯文字的滚动显示
- **ToastRenderer**：负责弹出提示的渲染和动画
- **MarqueeRenderer**：负责跑马灯文字的渲染

### 2.3 数据流向

1. 服务器发送 `NotifyToast` 消息
2. `NetworkDispatcher` 接收并解析
3. `GameScene` 调用 `NotificationManager.show_toast()` 或 `show_marquee()`
4. 对应通道处理优先级和显示逻辑
5. 渲染器在每帧绘制到屏幕

---

## 3. 通知类型与数据结构

### 3.1 通知类型枚举

```python
class NotificationType(Enum):
    # 时间事件
    DAWN = "dawn"              # 天亮了
    DUSK = "dusk"              # 天黑了
    SEASON_CHANGE = "season"   # 季节变化
    
    # 操作反馈
    ITEM_USE_SUCCESS = "item_ok"    # 物品使用成功
    ITEM_USE_FAIL = "item_fail"     # 物品使用失败
    ENERGY_LOW = "energy_low"       # 能量不足
    INVENTORY_FULL = "inv_full"     # 背包已满
    
    # 系统公告
    SYSTEM_NOTICE = "system"        # 系统公告
    EVENT_NOTICE = "event"          # 活动通知
```

### 3.2 优先级定义

```python
class NotificationPriority(Enum):
    LOW = 0       # 普通提示（物品使用反馈）
    NORMAL = 1    # 一般提示（时间事件）
    HIGH = 2      # 重要提示（系统公告）
    CRITICAL = 3  # 紧急提示（强制打断）
```

### 3.3 通知数据结构

```python
@dataclass
class Notification:
    type: NotificationType
    priority: NotificationPriority
    title: str                    # 标题（如"天亮了"）
    content: str = ""             # 内容（可选）
    duration: float = 3.0         # 显示时长（秒）
    channel: str = "toast"        # 显示通道：toast / marquee
```

---

## 4. Toast 通道实现

### 4.1 显示位置

屏幕上方居中，参考《星露谷物语》。

### 4.2 动画效果

弹出效果（缩放动画）：
- 从 0 缩放到 1.1（略微过冲）
- 回弹到 1.0
- 停留指定时长
- 缩放到 0 并消失

### 4.3 优先级处理逻辑

```python
class ToastChannel:
    def __init__(self):
        self._queue = []  # 优先级队列
        self._current = None  # 当前显示的提示
        self._state = "idle"  # idle / appearing / showing / disappearing
    
    def add(self, notification: Notification):
        """添加提示到队列"""
        # CRITICAL 优先级直接打断当前提示
        if notification.priority == Priority.CRITICAL:
            self._interrupt_current(notification)
        else:
            self._queue.append(notification)
            self._queue.sort(key=lambda n: n.priority.value, reverse=True)
    
    def update(self, dt: float):
        """更新动画状态"""
        if self._state == "idle":
            self._show_next()
        elif self._state == "appearing":
            self._update_appear_animation(dt)
        elif self._state == "showing":
            self._update_show_duration(dt)
        elif self._state == "disappearing":
            self._update_disappear_animation(dt)
```

### 4.4 渲染参数

```python
# Toast 配置
TOAST_WIDTH = 300           # 宽度
TOAST_HEIGHT = 60           # 高度
TOAST_MARGIN_TOP = 50       # 距顶部距离
TOAST_FONT_SIZE = 20        # 字体大小
TOAST_BG_COLOR = (0, 0, 0, 200)  # 半透明黑色背景
TOAST_TEXT_COLOR = (255, 255, 255)  # 白色文字

# 动画参数
APPEAR_DURATION = 0.3       # 出现动画时长（秒）
DISAPPEAR_DURATION = 0.2    # 消失动画时长（秒）
SCALE_OVERSHOOT = 1.1       # 过冲缩放比例
```

---

## 5. Marquee 通道实现

### 5.1 显示位置

屏幕顶部横向滚动。

### 5.2 滚动逻辑

- 文字从右侧屏幕外进入
- 向左滚动直到完全消失在左侧
- 支持多条消息排队显示

### 5.3 实现方式

```python
class MarqueeChannel:
    def __init__(self, screen_width: int):
        self._screen_width = screen_width
        self._queue = []           # 消息队列
        self._current_text = ""    # 当前显示的文字
        self._current_x = 0       # 当前 X 坐标
        self._speed = 100          # 滚动速度（像素/秒）
        self._state = "idle"       # idle / scrolling
    
    def add(self, text: str):
        """添加跑马灯文字"""
        self._queue.append(text)
        if self._state == "idle":
            self._start_next()
    
    def _start_next(self):
        """开始下一条消息"""
        if self._queue:
            self._current_text = self._queue.pop(0)
            self._current_x = self._screen_width  # 从右侧进入
            self._state = "scrolling"
        else:
            self._state = "idle"
    
    def update(self, dt: float):
        """更新滚动位置"""
        if self._state == "scrolling":
            self._current_x -= self._speed * dt
            # 当文字完全滚出左侧时，显示下一条
            if self._current_x < -len(self._current_text) * 16:  # 假设每个字符 16 像素
                self._start_next()
```

### 5.4 渲染参数

```python
# Marquee 配置
MARQUEE_HEIGHT = 30         # 高度
MARQUEE_MARGIN_TOP = 8      # 距顶部距离
MARQUEE_FONT_SIZE = 18      # 字体大小
MARQUEE_BG_COLOR = (0, 0, 0, 150)  # 半透明黑色背景
MARQUEE_TEXT_COLOR = (255, 255, 0)  # 黄色文字（醒目）
MARQUEE_SPEED = 100         # 滚动速度（像素/秒）
```

### 5.5 多条消息处理

- 新消息进入队列等待
- 当前消息滚完后自动显示下一条
- 支持紧急消息插队（通过优先级）

---

## 6. 网络协议与集成

### 6.1 新增消息 ID

```python
# message_ids.py 新增
MSG_ID_NOTIFY_TOAST = 4001  # 服务器通知提示
```

### 6.2 Protobuf 定义

```protobuf
// 通知类型枚举
enum NotifyType {
    NOTIFY_DAWN = 0;           // 天亮了
    NOTIFY_DUSK = 1;           // 天黑了
    NOTIFY_ITEM_SUCCESS = 2;   // 物品使用成功
    NOTIFY_ITEM_FAIL = 3;      // 物品使用失败
    NOTIFY_SYSTEM = 4;         // 系统公告
}

// 服务器通知 (Game -> Client)
message NotifyToast {
    NotifyType type = 1;       // 通知类型
    string title = 2;          // 标题
    string content = 3;        // 内容（可选）
    int32 priority = 4;        // 优先级：0=LOW, 1=NORMAL, 2=HIGH, 3=CRITICAL
    int32 duration_ms = 5;     // 显示时长（毫秒）
    bool is_marquee = 6;       // 是否为跑马灯
}
```

### 6.3 NetworkDispatcher 集成

```python
# network_dispatcher.py 新增
def _handle_notify_toast(self, payload: bytes):
    """处理服务器通知"""
    try:
        player_msg = base_pb2.PlayerMsg()
        player_msg.ParseFromString(payload)
        
        notify = player_pb2.NotifyToast()
        notify.ParseFromString(player_msg.payload)
        
        # 调用回调
        self._callbacks["on_notify_toast"](notify)
        
    except Exception as e:
        logger.error(f"[NetworkDispatcher]Failed to parse NotifyToast: {e}")
```

### 6.4 GameScene 集成

```python
# game_scene.py 新增
def _on_notify_toast(self, notify):
    """通知提示回调"""
    notification = Notification(
        type=NotificationType(notify.type),
        priority=NotificationPriority(notify.priority),
        title=notify.title,
        content=notify.content,
        duration=notify.duration_ms / 1000.0,
        channel="marquee" if notify.is_marquee else "toast",
    )
    self._notification_manager.add(notification)
```

### 6.5 GameRenderer 集成

```python
# game_renderer.py 新增
def render(self, dt, group, player_sprite, scene_manager, map_renderer, 
           drop_item_renderer=None, notification_manager=None):
    # ... 现有渲染逻辑 ...
    
    # 通知提示渲染（在 HUD 上方，弹窗下方）
    if notification_manager:
        notification_manager.render(self._screen, dt)
    
    # ... 现有弹窗和过渡渲染 ...
```

---

## 7. 文件结构与实现计划

### 7.1 新增文件

```
scripts/client/
├── ui/
│   ├── notification_manager.py    # 通知管理器
│   ├── toast_channel.py          # Toast 通道
│   ├── toast_renderer.py         # Toast 渲染器
│   ├── marquee_channel.py        # Marquee 通道
│   └── marquee_renderer.py       # Marquee 渲染器
└── notification.py               # 通知数据结构定义
```

### 7.2 修改文件

```
scripts/client/
├── message_ids.py                # 新增 MSG_ID_NOTIFY_TOAST
├── network_dispatcher.py         # 新增 _handle_notify_toast
├── game_scene.py                 # 集成通知管理器
└── game_renderer.py              # 集成通知渲染

scripts/common/proto/
└── player.proto                  # 新增 NotifyToast 消息定义
```

### 7.3 实现顺序

1. **Phase 1：基础框架**
   - 定义通知数据结构 (`notification.py`)
   - 实现 NotificationManager (`notification_manager.py`)

2. **Phase 2：Toast 通道**
   - 实现 ToastChannel (`toast_channel.py`)
   - 实现 ToastRenderer (`toast_renderer.py`)

3. **Phase 3：Marquee 通道**
   - 实现 MarqueeChannel (`marquee_channel.py`)
   - 实现 MarqueeRenderer (`marquee_renderer.py`)

4. **Phase 4：网络集成**
   - 更新 protobuf 定义
   - 更新 message_ids.py
   - 更新 NetworkDispatcher
   - 更新 GameScene 和 GameRenderer

5. **Phase 5：测试与调试**
   - 单元测试
   - 集成测试
   - 性能优化

---

## 8. 附录

### 8.1 参考资料

- 《星露谷物语》UI 设计
- PyGame 动画实现
- 优先级队列算法

### 8.2 术语表

- **Toast**：弹出提示，通常显示在屏幕上方，停留几秒后消失
- **Marquee**：跑马灯，横向滚动的文字公告
- **优先级**：决定提示是否可以打断其他提示的等级
- **通道**：提示的显示方式（Toast 或 Marquee）
