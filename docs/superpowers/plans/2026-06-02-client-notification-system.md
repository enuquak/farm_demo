# 客户端提示系统实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现一个完整的客户端提示系统，支持 Toast 弹出提示和 Marquee 跑马灯，具有优先级管理功能。

**Architecture:** 使用 NotificationManager 作为统一入口，内部包含 ToastChannel 和 MarqueeChannel 两个独立通道，每个通道有自己的渲染器。服务器通过 NotifyToast protobuf 消息触发提示。

**Tech Stack:** Python, PyGame, Protobuf

---

## 文件结构

**新增文件：**
- `scripts/client/notification.py` - 通知数据结构定义
- `scripts/client/ui/notification_manager.py` - 通知管理器
- `scripts/client/ui/toast_channel.py` - Toast 通道
- `scripts/client/ui/toast_renderer.py` - Toast 渲染器
- `scripts/client/ui/marquee_channel.py` - Marquee 通道
- `scripts/client/ui/marquee_renderer.py` - Marquee 渲染器
- `scripts/client/ui/test_notification_system.py` - 单元测试

**修改文件：**
- `scripts/common/proto/player.proto` - 新增 NotifyToast 消息定义
- `scripts/client/message_ids.py` - 新增 MSG_ID_NOTIFY_TOAST
- `scripts/client/network_dispatcher.py` - 新增 _handle_notify_toast
- `scripts/client/game_scene.py` - 集成通知管理器
- `scripts/client/game_renderer.py` - 集成通知渲染

---

### Task 1: 定义通知数据结构

**Files:**
- Create: `scripts/client/notification.py`

- [ ] **Step 1: 创建 notification.py 文件**

```python
"""
通知数据结构模块
定义通知类型、优先级和通知数据结构
"""
import logging
from enum import Enum
from dataclasses import dataclass
from typing import Optional

logger = logging.getLogger("client.notification")


class NotificationType(Enum):
    """通知类型枚举"""
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


class NotificationPriority(Enum):
    """通知优先级枚举"""
    LOW = 0       # 普通提示（物品使用反馈）
    NORMAL = 1    # 一般提示（时间事件）
    HIGH = 2      # 重要提示（系统公告）
    CRITICAL = 3  # 紧急提示（强制打断）


@dataclass
class Notification:
    """通知数据结构"""
    type: NotificationType
    priority: NotificationPriority
    title: str                    # 标题（如"天亮了"）
    content: str = ""             # 内容（可选）
    duration: float = 3.0         # 显示时长（秒）
    channel: str = "toast"        # 显示通道：toast / marquee

    def __post_init__(self):
        """验证数据有效性"""
        if not self.title:
            logger.warning("[Notification]Empty title provided")
        if self.duration <= 0:
            logger.warning(f"[Notification]Invalid duration: {self.duration}, using default 3.0")
            self.duration = 3.0
```

- [ ] **Step 2: 运行测试验证文件语法**

Run: `python -c "from scripts.client.notification import NotificationType, NotificationPriority, Notification; print('Import OK')"`
Expected: 输出 "Import OK"

- [ ] **Step 3: 提交代码**

```bash
git add scripts/client/notification.py
git commit -m "feat: add notification data structures"
```

---

### Task 2: 实现 Toast 渲染器

**Files:**
- Create: `scripts/client/ui/toast_renderer.py`

- [ ] **Step 1: 创建 toast_renderer.py 文件**

```python
"""
Toast 渲染器模块
负责渲染弹出提示的视觉效果和动画
"""
import logging
import math
from typing import Optional, Tuple

import pygame

logger = logging.getLogger("client.ui.toast_renderer")


class ToastRenderer:
    """
    Toast 渲染器
    负责渲染弹出提示，支持缩放动画效果
    """

    # 布局配置
    TOAST_WIDTH = 300           # 宽度
    TOAST_HEIGHT = 60           # 高度
    MARGIN_TOP = 50             # 距顶部距离
    PADDING_X = 15              # 水平内边距
    PADDING_Y = 10              # 垂直内边距

    # 字体配置
    TITLE_FONT_SIZE = 20
    CONTENT_FONT_SIZE = 14

    # 颜色配置
    BG_COLOR = (0, 0, 0, 200)           # 半透明黑色背景
    BORDER_COLOR = (100, 100, 100, 200) # 灰色边框
    TITLE_COLOR = (255, 255, 255)       # 白色标题
    CONTENT_COLOR = (200, 200, 200)     # 浅灰色内容

    # 动画配置
    APPEAR_DURATION = 0.3       # 出现动画时长（秒）
    DISAPPEAR_DURATION = 0.2    # 消失动画时长（秒）
    SCALE_OVERSHOOT = 1.1       # 过冲缩放比例

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化 Toast 渲染器

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._screen_width = screen_width
        self._screen_height = screen_height

        # 初始化字体
        pygame.font.init()
        self._title_font = pygame.font.SysFont("simhei", self.TITLE_FONT_SIZE)
        self._content_font = pygame.font.SysFont("simhei", self.CONTENT_FONT_SIZE)

        # 计算居中位置
        self._center_x = (screen_width - self.TOAST_WIDTH) // 2
        self._base_y = self.MARGIN_TOP

        logger.info(f"[ToastRenderer]Initialized: screen={screen_width}x{screen_height}")

    def render(self, screen: pygame.Surface, title: str, content: str,
               progress: float, state: str):
        """
        渲染 Toast

        Args:
            screen: 目标屏幕表面
            title: 标题文字
            content: 内容文字
            progress: 动画进度 (0.0 ~ 1.0)
            state: 动画状态 (appearing/showing/disappearing)
        """
        # 计算缩放比例
        scale = self._calculate_scale(progress, state)

        if scale <= 0:
            return

        # 计算实际尺寸
        actual_width = int(self.TOAST_WIDTH * scale)
        actual_height = int(self.TOAST_HEIGHT * scale)

        # 计算位置（保持居中）
        x = self._center_x + (self.TOAST_WIDTH - actual_width) // 2
        y = self._base_y

        # 创建 Toast 表面
        toast_surface = pygame.Surface((actual_width, actual_height), pygame.SRCALPHA)

        # 绘制背景
        bg_rect = pygame.Rect(0, 0, actual_width, actual_height)
        pygame.draw.rect(toast_surface, self.BG_COLOR, bg_rect)
        pygame.draw.rect(toast_surface, self.BORDER_COLOR, bg_rect, 2)

        # 绘制文字（根据缩放调整）
        if scale >= 0.5:  # 缩放太小时不绘制文字
            self._render_text(toast_surface, title, content, scale)

        # 绘制到屏幕
        screen.blit(toast_surface, (x, y))

    def _calculate_scale(self, progress: float, state: str) -> float:
        """
        计算动画缩放比例

        Args:
            progress: 动画进度 (0.0 ~ 1.0)
            state: 动画状态

        Returns:
            缩放比例 (0.0 ~ SCALE_OVERSHOOT)
        """
        if state == "appearing":
            # 出现动画：从 0 到 SCALE_OVERSHOOT 再到 1.0
            if progress < 0.7:
                # 0 -> 0.7: 从 0 到 SCALE_OVERSHOOT
                return self.SCALE_OVERSHOOT * (progress / 0.7)
            else:
                # 0.7 -> 1.0: 从 SCALE_OVERSHOOT 到 1.0
                overshoot_progress = (progress - 0.7) / 0.3
                return self.SCALE_OVERSHOOT - (self.SCALE_OVERSHOOT - 1.0) * overshoot_progress

        elif state == "showing":
            return 1.0

        elif state == "disappearing":
            # 消失动画：从 1.0 到 0
            return 1.0 - progress

        return 0.0

    def _render_text(self, surface: pygame.Surface, title: str, content: str, scale: float):
        """
        渲染文字

        Args:
            surface: 目标表面
            title: 标题
            content: 内容
            scale: 缩放比例
        """
        # 渲染标题
        title_surface = self._title_font.render(title, True, self.TITLE_COLOR)
        title_x = (surface.get_width() - title_surface.get_width()) // 2
        title_y = self.PADDING_Y
        surface.blit(title_surface, (title_x, title_y))

        # 渲染内容（如果有）
        if content:
            content_surface = self._content_font.render(content, True, self.CONTENT_COLOR)
            content_x = (surface.get_width() - content_surface.get_width()) // 2
            content_y = title_y + title_surface.get_height() + 5
            surface.blit(content_surface, (content_x, content_y))

    def cleanup(self):
        """清理资源"""
        logger.info("[ToastRenderer]Cleaned up")
```

- [ ] **Step 2: 运行测试验证文件语法**

Run: `python -c "from scripts.client.ui.toast_renderer import ToastRenderer; print('Import OK')"`
Expected: 输出 "Import OK"

- [ ] **Step 3: 提交代码**

```bash
git add scripts/client/ui/toast_renderer.py
git commit -m "feat: add Toast renderer with scale animation"
```

---

### Task 3: 实现 Toast 通道

**Files:**
- Create: `scripts/client/ui/toast_channel.py`

- [ ] **Step 1: 创建 toast_channel.py 文件**

```python
"""
Toast 通道模块
管理弹出提示的优先级队列和显示逻辑
"""
import logging
from typing import Optional, List
from enum import Enum

import pygame

from ..notification import Notification, NotificationPriority
from .toast_renderer import ToastRenderer

logger = logging.getLogger("client.ui.toast_channel")


class ToastState(Enum):
    """Toast 动画状态"""
    IDLE = "idle"
    APPEARING = "appearing"
    SHOWING = "showing"
    DISAPPEARING = "disappearing"


class ToastChannel:
    """
    Toast 通道
    管理弹出提示的优先级队列和动画状态
    """

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化 Toast 通道

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._renderer = ToastRenderer(screen_width, screen_height)

        # 队列和状态
        self._queue: List[Notification] = []
        self._current: Optional[Notification] = None
        self._state = ToastState.IDLE

        # 动画计时器
        self._timer: float = 0.0
        self._show_duration: float = 0.0

        logger.info("[ToastChannel]Initialized")

    @property
    def is_idle(self) -> bool:
        """通道是否空闲"""
        return self._state == ToastState.IDLE and len(self._queue) == 0

    def add(self, notification: Notification):
        """
        添加提示到队列

        Args:
            notification: 通知对象
        """
        # CRITICAL 优先级直接打断当前提示
        if notification.priority == NotificationPriority.CRITICAL:
            self._interrupt_current(notification)
        else:
            self._queue.append(notification)
            # 按优先级排序（高优先级在前）
            self._queue.sort(key=lambda n: n.priority.value, reverse=True)

        logger.info(f"[ToastChannel]Added: {notification.title}, priority={notification.priority.name}")

    def _interrupt_current(self, notification: Notification):
        """
        打断当前提示

        Args:
            notification: 新的通知（必须是 CRITICAL 优先级）
        """
        if self._current:
            logger.info(f"[ToastChannel]Interrupting '{self._current.title}' with '{notification.title}'")

        self._current = notification
        self._state = ToastState.APPEARING
        self._timer = 0.0
        self._show_duration = notification.duration

    def update(self, dt: float):
        """
        更新动画状态

        Args:
            dt: 距上一帧的时间（秒）
        """
        if self._state == ToastState.IDLE:
            self._show_next()

        elif self._state == ToastState.APPEARING:
            self._timer += dt
            progress = min(self._timer / ToastRenderer.APPEAR_DURATION, 1.0)

            if progress >= 1.0:
                self._state = ToastState.SHOWING
                self._timer = 0.0

        elif self._state == ToastState.SHOWING:
            self._timer += dt

            if self._timer >= self._show_duration:
                self._state = ToastState.DISAPPEARING
                self._timer = 0.0

        elif self._state == ToastState.DISAPPEARING:
            self._timer += dt
            progress = min(self._timer / ToastRenderer.DISAPPEAR_DURATION, 1.0)

            if progress >= 1.0:
                self._current = None
                self._state = ToastState.IDLE

    def _show_next(self):
        """显示下一条提示"""
        if self._queue:
            self._current = self._queue.pop(0)
            self._state = ToastState.APPEARING
            self._timer = 0.0
            self._show_duration = self._current.duration
            logger.info(f"[ToastChannel]Showing: {self._current.title}")

    def render(self, screen: pygame.Surface):
        """
        渲染 Toast

        Args:
            screen: 目标屏幕表面
        """
        if not self._current:
            return

        # 计算动画进度
        progress = 0.0
        if self._state == ToastState.APPEARING:
            progress = min(self._timer / ToastRenderer.APPEAR_DURATION, 1.0)
        elif self._state == ToastState.SHOWING:
            progress = 1.0
        elif self._state == ToastState.DISAPPEARING:
            progress = min(self._timer / ToastRenderer.DISAPPEAR_DURATION, 1.0)

        # 渲染
        self._renderer.render(
            screen,
            self._current.title,
            self._current.content,
            progress,
            self._state.value
        )

    def cleanup(self):
        """清理资源"""
        self._renderer.cleanup()
        logger.info("[ToastChannel]Cleaned up")
```

- [ ] **Step 2: 运行测试验证文件语法**

Run: `python -c "from scripts.client.ui.toast_channel import ToastChannel, ToastState; print('Import OK')"`
Expected: 输出 "Import OK"

- [ ] **Step 3: 提交代码**

```bash
git add scripts/client/ui/toast_channel.py
git commit -m "feat: add Toast channel with priority queue"
```

---

### Task 4: 实现 Marquee 渲染器

**Files:**
- Create: `scripts/client/ui/marquee_renderer.py`

- [ ] **Step 1: 创建 marquee_renderer.py 文件**

```python
"""
Marquee 渲染器模块
负责渲染跑马灯文字的视觉效果
"""
import logging
from typing import Tuple

import pygame

logger = logging.getLogger("client.ui.marquee_renderer")


class MarqueeRenderer:
    """
    Marquee 渲染器
    负责渲染跑马灯文字，支持横向滚动
    """

    # 布局配置
    MARQUEE_HEIGHT = 30         # 高度
    MARGIN_TOP = 8              # 距顶部距离
    PADDING_X = 15              # 水平内边距

    # 字体配置
    FONT_SIZE = 18

    # 颜色配置
    BG_COLOR = (0, 0, 0, 150)           # 半透明黑色背景
    TEXT_COLOR = (255, 255, 0)           # 黄色文字（醒目）

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化 Marquee 渲染器

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._screen_width = screen_width
        self._screen_height = screen_height

        # 初始化字体
        pygame.font.init()
        self._font = pygame.font.SysFont("simhei", self.FONT_SIZE)

        logger.info(f"[MarqueeRenderer]Initialized: screen={screen_width}x{screen_height}")

    def get_text_width(self, text: str) -> int:
        """
        获取文字宽度

        Args:
            text: 文字内容

        Returns:
            文字宽度（像素）
        """
        if not text:
            return 0
        text_surface = self._font.render(text, True, self.TEXT_COLOR)
        return text_surface.get_width()

    def render(self, screen: pygame.Surface, text: str, x: float):
        """
        渲染跑马灯文字

        Args:
            screen: 目标屏幕表面
            text: 文字内容
            x: 当前 X 坐标
        """
        if not text:
            return

        # 渲染文字
        text_surface = self._font.render(text, True, self.TEXT_COLOR)
        text_width = text_surface.get_width()
        text_height = text_surface.get_height()

        # 计算背景区域
        bg_width = text_width + self.PADDING_X * 2
        bg_height = self.MARQUEE_HEIGHT
        bg_x = x - self.PADDING_X
        bg_y = self.MARGIN_TOP

        # 绘制背景
        bg_surface = pygame.Surface((bg_width, bg_height), pygame.SRCALPHA)
        bg_surface.fill(self.BG_COLOR)
        screen.blit(bg_surface, (bg_x, bg_y))

        # 绘制文字
        text_y = bg_y + (bg_height - text_height) // 2
        screen.blit(text_surface, (x, text_y))

    def cleanup(self):
        """清理资源"""
        logger.info("[MarqueeRenderer]Cleaned up")
```

- [ ] **Step 2: 运行测试验证文件语法**

Run: `python -c "from scripts.client.ui.marquee_renderer import MarqueeRenderer; print('Import OK')"`
Expected: 输出 "Import OK"

- [ ] **Step 3: 提交代码**

```bash
git add scripts/client/ui/marquee_renderer.py
git commit -m "feat: add Marquee renderer"
```

---

### Task 5: 实现 Marquee 通道

**Files:**
- Create: `scripts/client/ui/marquee_channel.py`

- [ ] **Step 1: 创建 marquee_channel.py 文件**

```python
"""
Marquee 通道模块
管理跑马灯文字的滚动显示逻辑
"""
import logging
from typing import List, Optional
from enum import Enum

import pygame

from .marquee_renderer import MarqueeRenderer

logger = logging.getLogger("client.ui.marquee_channel")


class MarqueeState(Enum):
    """Marquee 滚动状态"""
    IDLE = "idle"
    SCROLLING = "scrolling"


class MarqueeChannel:
    """
    Marquee 通道
    管理跑马灯文字的队列和滚动逻辑
    """

    # 滚动速度（像素/秒）
    SCROLL_SPEED = 100

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化 Marquee 通道

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._screen_width = screen_width
        self._renderer = MarqueeRenderer(screen_width, screen_height)

        # 队列和状态
        self._queue: List[str] = []
        self._current_text: str = ""
        self._current_x: float = 0.0
        self._current_text_width: int = 0
        self._state = MarqueeState.IDLE

        logger.info("[MarqueeChannel]Initialized")

    @property
    def is_idle(self) -> bool:
        """通道是否空闲"""
        return self._state == MarqueeState.IDLE and len(self._queue) == 0

    def add(self, text: str):
        """
        添加跑马灯文字

        Args:
            text: 文字内容
        """
        if not text:
            logger.warning("[MarqueeChannel]Empty text provided")
            return

        self._queue.append(text)

        if self._state == MarqueeState.IDLE:
            self._start_next()

        logger.info(f"[MarqueeChannel]Added: '{text}', queue size={len(self._queue)}")

    def _start_next(self):
        """开始下一条消息"""
        if self._queue:
            self._current_text = self._queue.pop(0)
            self._current_x = float(self._screen_width)  # 从右侧进入
            self._current_text_width = self._renderer.get_text_width(self._current_text)
            self._state = MarqueeState.SCROLLING
            logger.info(f"[MarqueeChannel]Started: '{self._current_text}'")
        else:
            self._current_text = ""
            self._state = MarqueeState.IDLE

    def update(self, dt: float):
        """
        更新滚动位置

        Args:
            dt: 距上一帧的时间（秒）
        """
        if self._state == MarqueeState.SCROLLING:
            self._current_x -= self.SCROLL_SPEED * dt

            # 当文字完全滚出左侧时，显示下一条
            if self._current_x < -self._current_text_width:
                self._start_next()

    def render(self, screen: pygame.Surface):
        """
        渲染跑马灯

        Args:
            screen: 目标屏幕表面
        """
        if self._state == MarqueeState.SCROLLING and self._current_text:
            self._renderer.render(screen, self._current_text, self._current_x)

    def cleanup(self):
        """清理资源"""
        self._renderer.cleanup()
        logger.info("[MarqueeChannel]Cleaned up")
```

- [ ] **Step 2: 运行测试验证文件语法**

Run: `python -c "from scripts.client.ui.marquee_channel import MarqueeChannel, MarqueeState; print('Import OK')"`
Expected: 输出 "Import OK"

- [ ] **Step 3: 提交代码**

```bash
git add scripts/client/ui/marquee_channel.py
git commit -m "feat: add Marquee channel with scrolling logic"
```

---

### Task 6: 实现通知管理器

**Files:**
- Create: `scripts/client/ui/notification_manager.py`

- [ ] **Step 1: 创建 notification_manager.py 文件**

```python
"""
通知管理器模块
统一管理 Toast 和 Marquee 两个通道
"""
import logging
from typing import Optional

import pygame

from ..notification import Notification, NotificationType, NotificationPriority
from .toast_channel import ToastChannel
from .marquee_channel import MarqueeChannel

logger = logging.getLogger("client.ui.notification_manager")


class NotificationManager:
    """
    通知管理器
    统一管理 Toast 和 Marquee 两个通道
    """

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化通知管理器

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._toast_channel = ToastChannel(screen_width, screen_height)
        self._marquee_channel = MarqueeChannel(screen_width, screen_height)

        logger.info("[NotificationManager]Initialized")

    def add(self, notification: Notification):
        """
        添加通知

        Args:
            notification: 通知对象
        """
        if notification.channel == "marquee":
            self._marquee_channel.add(notification.title)
        else:
            self._toast_channel.add(notification)

        logger.info(f"[NotificationManager]Added notification: {notification.title}, channel={notification.channel}")

    def show_toast(self, title: str, content: str = "",
                   priority: NotificationPriority = NotificationPriority.NORMAL,
                   duration: float = 3.0):
        """
        显示 Toast 提示

        Args:
            title: 标题
            content: 内容（可选）
            priority: 优先级
            duration: 显示时长（秒）
        """
        notification = Notification(
            type=NotificationType.SYSTEM_NOTICE,
            priority=priority,
            title=title,
            content=content,
            duration=duration,
            channel="toast"
        )
        self.add(notification)

    def show_marquee(self, text: str):
        """
        显示跑马灯

        Args:
            text: 文字内容
        """
        self._marquee_channel.add(text)

    def update(self, dt: float):
        """
        更新动画状态

        Args:
            dt: 距上一帧的时间（秒）
        """
        self._toast_channel.update(dt)
        self._marquee_channel.update(dt)

    def render(self, screen: pygame.Surface, dt: float):
        """
        渲染通知

        Args:
            screen: 目标屏幕表面
            dt: 距上一帧的时间（秒）
        """
        # 先更新状态
        self.update(dt)

        # 渲染 Marquee（在顶部）
        self._marquee_channel.render(screen)

        # 渲染 Toast（在 Marquee 下方）
        self._toast_channel.render(screen)

    def cleanup(self):
        """清理资源"""
        self._toast_channel.cleanup()
        self._marquee_channel.cleanup()
        logger.info("[NotificationManager]Cleaned up")
```

- [ ] **Step 2: 运行测试验证文件语法**

Run: `python -c "from scripts.client.ui.notification_manager import NotificationManager; print('Import OK')"`
Expected: 输出 "Import OK"

- [ ] **Step 3: 提交代码**

```bash
git add scripts/client/ui/notification_manager.py
git commit -m "feat: add NotificationManager"
```

---

### Task 7: 更新 Protobuf 定义

**Files:**
- Modify: `scripts/common/proto/player.proto`

- [ ] **Step 1: 在 player.proto 末尾添加 NotifyToast 消息定义**

在文件末尾添加：

```protobuf
// ===========================================
// 通知提示协议
// ===========================================

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

- [ ] **Step 2: 提交代码**

```bash
git add scripts/common/proto/player.proto
git commit -m "feat: add NotifyToast protobuf definition"
```

---

### Task 8: 更新 message_ids.py

**Files:**
- Modify: `scripts/client/message_ids.py`

- [ ] **Step 1: 在 MessageIds 类中添加新消息 ID**

在 `MSG_ID_ACTIVE_SLOT_CHANGE = 3101` 之后添加：

```python
    MSG_ID_NOTIFY_TOAST = 4001  # 服务器通知提示
```

- [ ] **Step 2: 提交代码**

```bash
git add scripts/client/message_ids.py
git commit -m "feat: add MSG_ID_NOTIFY_TOAST message ID"
```

---

### Task 9: 更新 NetworkDispatcher

**Files:**
- Modify: `scripts/client/network_dispatcher.py`

- [ ] **Step 1: 添加 import 和回调参数**

在文件顶部的 import 部分添加：

```python
from .message_ids import (
    MSG_ID_MAP_DATA_NOTIFY, MSG_ID_POSITION_CORRECT,
    MSG_ID_ITEM_USE_RESP, MSG_ID_SCENE_CHANGE_RESP,
    MSG_ID_CLOCK_SYNC, MSG_ID_FORCE_SLEEP_NOTIFY, MSG_ID_FORCE_SLEEP_READY,
    MSG_ID_DROP_ITEM_SYNC, MSG_ID_INVENTORY_SYNC,
    MSG_ID_NOTIFY_TOAST,  # 新增
)
```

- [ ] **Step 2: 更新 __init__ 方法签名**

在 `__init__` 方法的参数列表中添加：

```python
    on_notify_toast: Callable[[Any], None] = None,
```

在 `self._callbacks` 字典中添加：

```python
            "on_notify_toast": on_notify_toast,
```

- [ ] **Step 3: 更新分发表**

在 `self._dispatch_table` 中添加：

```python
            MSG_ID_NOTIFY_TOAST: self._handle_notify_toast,
```

- [ ] **Step 4: 添加处理方法**

在类的末尾添加新方法：

```python
    def _handle_notify_toast(self, payload: bytes):
        """处理服务器通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            notify = player_pb2.NotifyToast()
            notify.ParseFromString(player_msg.payload)

            logger.info(f"[NetworkDispatcher]NotifyToast received: type={notify.type}, "
                       f"title={notify.title}, priority={notify.priority}")

            if self._callbacks["on_notify_toast"]:
                self._callbacks["on_notify_toast"](notify)

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse NotifyToast: {e}")
```

- [ ] **Step 5: 提交代码**

```bash
git add scripts/client/network_dispatcher.py
git commit -m "feat: add NotifyToast handler to NetworkDispatcher"
```

---

### Task 10: 更新 GameScene

**Files:**
- Modify: `scripts/client/game_scene.py`

- [ ] **Step 1: 添加 import**

在文件顶部添加：

```python
from .notification import Notification, NotificationType, NotificationPriority
from .ui.notification_manager import NotificationManager
```

- [ ] **Step 2: 在 __init__ 方法中创建 NotificationManager**

在 `self._drop_item_renderer = DropItemRenderer()` 之后添加：

```python
        # 通知管理器
        self._notification_manager = NotificationManager(
            self.WINDOW_WIDTH, self.WINDOW_HEIGHT
        )
```

- [ ] **Step 3: 更新 NetworkMessageDispatcher 初始化**

在 `NetworkMessageDispatcher` 初始化的参数中添加：

```python
            on_notify_toast=self._on_notify_toast,
```

- [ ] **Step 4: 添加回调方法**

在 `_on_inventory_sync` 方法之后添加：

```python
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
        logger.info(f"[GameScene]Notification received: {notify.title}")
```

- [ ] **Step 5: 更新 render 调用**

在 `self._renderer.render(...)` 调用中添加 `notification_manager` 参数：

```python
            self._renderer.render(
                dt, self._group, self._player_sprite,
                self._scene_manager, self._map_renderer,
                self._drop_item_renderer,
                self._notification_manager,  # 新增
            )
```

- [ ] **Step 6: 提交代码**

```bash
git add scripts/client/game_scene.py
git commit -m "feat: integrate NotificationManager into GameScene"
```

---

### Task 11: 更新 GameRenderer

**Files:**
- Modify: `scripts/client/game_renderer.py`

- [ ] **Step 1: 添加 import**

在文件顶部添加：

```python
from .ui.notification_manager import NotificationManager
```

- [ ] **Step 2: 更新 render 方法签名**

将 `render` 方法签名更新为：

```python
    def render(self, dt: float, group, player_sprite: PlayerSprite,
               scene_manager, map_renderer, drop_item_renderer: DropItemRenderer = None,
               notification_manager: NotificationManager = None):
```

- [ ] **Step 3: 添加通知渲染逻辑**

在 `# 时间 HUD 渲染（左上角）` 之前添加：

```python
        # 通知提示渲染（在 HUD 上方，弹窗下方）
        if notification_manager:
            notification_manager.render(self._screen, dt)
```

- [ ] **Step 4: 提交代码**

```bash
git add scripts/client/game_renderer.py
git commit -m "feat: integrate notification rendering into GameRenderer"
```

---

### Task 12: 单元测试

**Files:**
- Create: `scripts/client/ui/test_notification_system.py`

- [ ] **Step 1: 创建测试文件**

```python
"""
通知系统单元测试
"""
import unittest
import pygame

from scripts.client.notification import Notification, NotificationType, NotificationPriority
from scripts.client.ui.notification_manager import NotificationManager
from scripts.client.ui.toast_channel import ToastChannel, ToastState
from scripts.client.ui.marquee_channel import MarqueeChannel, MarqueeState


class TestNotification(unittest.TestCase):
    """测试通知数据结构"""

    def test_notification_creation(self):
        """测试通知创建"""
        notification = Notification(
            type=NotificationType.DAWN,
            priority=NotificationPriority.NORMAL,
            title="天亮了"
        )
        self.assertEqual(notification.type, NotificationType.DAWN)
        self.assertEqual(notification.priority, NotificationPriority.NORMAL)
        self.assertEqual(notification.title, "天亮了")
        self.assertEqual(notification.content, "")
        self.assertEqual(notification.duration, 3.0)
        self.assertEqual(notification.channel, "toast")

    def test_notification_with_content(self):
        """测试带内容的通知"""
        notification = Notification(
            type=NotificationType.SYSTEM_NOTICE,
            priority=NotificationPriority.HIGH,
            title="系统公告",
            content="服务器将在 10 分钟后维护",
            duration=5.0,
            channel="marquee"
        )
        self.assertEqual(notification.content, "服务器将在 10 分钟后维护")
        self.assertEqual(notification.duration, 5.0)
        self.assertEqual(notification.channel, "marquee")


class TestToastChannel(unittest.TestCase):
    """测试 Toast 通道"""

    def setUp(self):
        """初始化测试环境"""
        pygame.init()
        self.screen = pygame.display.set_mode((800, 600))
        self.channel = ToastChannel(800, 600)

    def tearDown(self):
        """清理测试环境"""
        self.channel.cleanup()
        pygame.quit()

    def test_add_notification(self):
        """测试添加通知"""
        notification = Notification(
            type=NotificationType.DAWN,
            priority=NotificationPriority.NORMAL,
            title="天亮了"
        )
        self.channel.add(notification)
        self.assertEqual(len(self.channel._queue), 1)

    def test_priority_ordering(self):
        """测试优先级排序"""
        low = Notification(
            type=NotificationType.ITEM_USE_SUCCESS,
            priority=NotificationPriority.LOW,
            title="低优先级"
        )
        high = Notification(
            type=NotificationType.SYSTEM_NOTICE,
            priority=NotificationPriority.HIGH,
            title="高优先级"
        )
        self.channel.add(low)
        self.channel.add(high)

        # 高优先级应该在前面
        self.assertEqual(self.channel._queue[0].title, "高优先级")
        self.assertEqual(self.channel._queue[1].title, "低优先级")

    def test_critical_interrupts(self):
        """测试 CRITICAL 优先级打断"""
        normal = Notification(
            type=NotificationType.DAWN,
            priority=NotificationPriority.NORMAL,
            title="普通提示"
        )
        critical = Notification(
            type=NotificationType.SYSTEM_NOTICE,
            priority=NotificationPriority.CRITICAL,
            title="紧急提示"
        )

        self.channel.add(normal)
        self.channel.update(0.1)  # 触发显示

        self.channel.add(critical)
        # CRITICAL 应该立即打断
        self.assertEqual(self.channel._current.title, "紧急提示")


class TestMarqueeChannel(unittest.TestCase):
    """测试 Marquee 通道"""

    def setUp(self):
        """初始化测试环境"""
        pygame.init()
        self.screen = pygame.display.set_mode((800, 600))
        self.channel = MarqueeChannel(800, 600)

    def tearDown(self):
        """清理测试环境"""
        self.channel.cleanup()
        pygame.quit()

    def test_add_text(self):
        """测试添加文字"""
        self.channel.add("测试跑马灯")
        self.assertEqual(len(self.channel._queue), 1)

    def test_scrolling_state(self):
        """测试滚动状态"""
        self.channel.add("测试文字")
        self.assertEqual(self.channel._state, MarqueeState.SCROLLING)

    def test_multiple_texts(self):
        """测试多条文字排队"""
        self.channel.add("第一条")
        self.channel.add("第二条")
        self.assertEqual(len(self.channel._queue), 1)  # 一条在显示，一条在队列


class TestNotificationManager(unittest.TestCase):
    """测试通知管理器"""

    def setUp(self):
        """初始化测试环境"""
        pygame.init()
        self.screen = pygame.display.set_mode((800, 600))
        self.manager = NotificationManager(800, 600)

    def tearDown(self):
        """清理测试环境"""
        self.manager.cleanup()
        pygame.quit()

    def test_show_toast(self):
        """测试显示 Toast"""
        self.manager.show_toast("测试标题", "测试内容")
        # 不应该抛出异常

    def test_show_marquee(self):
        """测试显示跑马灯"""
        self.manager.show_marquee("测试跑马灯文字")
        # 不应该抛出异常

    def test_add_notification(self):
        """测试添加通知"""
        notification = Notification(
            type=NotificationType.DAWN,
            priority=NotificationPriority.NORMAL,
            title="天亮了"
        )
        self.manager.add(notification)
        # 不应该抛出异常


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 运行测试**

Run: `python -m pytest scripts/client/ui/test_notification_system.py -v`
Expected: 所有测试通过

- [ ] **Step 3: 提交代码**

```bash
git add scripts/client/ui/test_notification_system.py
git commit -m "test: add notification system unit tests"
```

---

### Task 13: 更新 UI __init__.py

**Files:**
- Modify: `scripts/client/ui/__init__.py`

- [ ] **Step 1: 更新 __init__.py 导出新模块**

```python
"""
UI 模块
包含游戏内所有 UI 组件
"""
from .energy_bar import EnergyBar
from .exhaustion_modal import ExhaustionModal
from .hotbar import Hotbar
from .inventory_panel import InventoryPanel
from .time_hud import TimeHUD
from .notification_manager import NotificationManager
from .toast_channel import ToastChannel
from .marquee_channel import MarqueeChannel
from .toast_renderer import ToastRenderer
from .marquee_renderer import MarqueeRenderer

__all__ = [
    'EnergyBar',
    'ExhaustionModal',
    'Hotbar',
    'InventoryPanel',
    'TimeHUD',
    'NotificationManager',
    'ToastChannel',
    'MarqueeChannel',
    'ToastRenderer',
    'MarqueeRenderer',
]
```

- [ ] **Step 2: 提交代码**

```bash
git add scripts/client/ui/__init__.py
git commit -m "feat: update UI __init__.py to export notification modules"
```

---

## 自检清单

**1. Spec 覆盖检查：**
- ✅ 通知类型枚举（NotificationType）
- ✅ 优先级定义（NotificationPriority）
- ✅ 通知数据结构（Notification）
- ✅ Toast 通道实现（ToastChannel + ToastRenderer）
- ✅ Marquee 通道实现（MarqueeChannel + MarqueeRenderer）
- ✅ 通知管理器（NotificationManager）
- ✅ Protobuf 定义（NotifyToast）
- ✅ 网络集成（NetworkDispatcher）
- ✅ GameScene 集成
- ✅ GameRenderer 集成

**2. 占位符扫描：**
- ✅ 无 TBD、TODO 或模糊要求
- ✅ 所有代码步骤都包含完整代码
- ✅ 所有测试步骤都包含具体命令

**3. 类型一致性：**
- ✅ NotificationType 枚举值一致
- ✅ NotificationPriority 枚举值一致
- ✅ 方法签名在所有任务中一致
- ✅ 属性名称在所有任务中一致

---

## 执行选项

**Plan complete and saved to `docs/superpowers/plans/2026-06-02-client-notification-system.md`. Two execution options:**

**1. Subagent-Driven (recommended)** - 我为每个任务分发一个新的子代理，任务之间进行审查，快速迭代

**2. Inline Execution** - 在当前会话中使用 executing-plans 执行任务，批量执行并设置检查点

**选择哪种方式？**
