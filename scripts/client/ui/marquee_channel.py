"""
Marquee (滚动公告) Channel
负责滚动公告的消息队列管理和滚动状态控制
"""
import logging
from collections import deque
from enum import Enum, auto

from .marquee_renderer import MarqueeRenderer

logger = logging.getLogger("client.ui.marquee_channel")


class MarqueeState(Enum):
    """Marquee 滚动状态"""
    IDLE = auto()
    SCROLLING = auto()


class MarqueeChannel:
    """
    Marquee 滚动公告通道
    管理消息队列，控制滚动逻辑，委托渲染器进行绘制
    """

    SCROLL_SPEED = 100  # pixels per second

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化 Marquee 通道

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._renderer = MarqueeRenderer(screen_width, screen_height)
        self._screen_width = screen_width
        self._queue: deque[str] = deque()
        self._state = MarqueeState.IDLE

        self._current_text: str = ""
        self._current_x: float = 0.0

        logger.info(
            f"[MarqueeChannel]Initialized, screen={screen_width}x{screen_height}"
        )

    @property
    def is_idle(self) -> bool:
        """Return True if channel is idle and no messages waiting."""
        return self._state == MarqueeState.IDLE and len(self._queue) == 0

    def add(self, text: str) -> None:
        """
        Add a message to the scroll queue.

        If the channel is idle, scrolling starts immediately.

        Args:
            text: Text message to display
        """
        self._queue.append(text)
        logger.debug(f"[MarqueeChannel]Queued: {text} (queue size={len(self._queue)})")

        if self._state == MarqueeState.IDLE:
            self._start_next()

    def _start_next(self) -> None:
        """Pop the next message from the queue and begin scrolling."""
        if not self._queue:
            self._state = MarqueeState.IDLE
            return

        self._current_text = self._queue.popleft()
        self._current_x = float(self._screen_width)
        self._state = MarqueeState.SCROLLING

        logger.debug(
            f"[MarqueeChannel]Start scrolling: {self._current_text} "
            f"(remaining queue={len(self._queue)})"
        )

    def update(self, dt: float) -> None:
        """
        Update scroll position each frame.

        Args:
            dt: Delta time in seconds since last frame
        """
        if self._state != MarqueeState.SCROLLING:
            return

        self._current_x -= self.SCROLL_SPEED * dt

        text_width = self._renderer.get_text_width(self._current_text)
        if self._current_x < -text_width:
            self._start_next()

    def render(self, screen) -> None:
        """
        Render the current scrolling message.

        Args:
            screen: Pygame screen surface
        """
        if self._state == MarqueeState.SCROLLING:
            self._renderer.render(screen, self._current_text, int(self._current_x))

    def cleanup(self) -> None:
        """Cleanup renderer resources."""
        self._renderer.cleanup()
        logger.info("[MarqueeChannel]Cleaned up")
