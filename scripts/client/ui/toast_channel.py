"""Toast notification channel.

Manages a priority queue of notifications and drives the ToastRenderer
through an appearance / display / disappearance animation state machine.
"""

import logging
from enum import Enum, unique
from typing import List, Optional

from ..notification import Notification, NotificationPriority
from .toast_renderer import ToastRenderer

logger = logging.getLogger("client.ui.toast_channel")


@unique
class ToastState(Enum):
    """Animation states for a single toast notification."""

    IDLE = "idle"
    APPEARING = "appearing"
    SHOWING = "showing"
    DISAPPEARING = "disappearing"


class ToastChannel:
    """Priority-driven toast notification channel.

    Maintains a priority-sorted queue and renders one notification at a
    time through an appear -> show -> disappear cycle.  CRITICAL
    notifications interrupt whatever is currently being shown.
    """

    def __init__(self, screen_width: int, screen_height: int):
        self._renderer = ToastRenderer(screen_width, screen_height)
        self._queue: List[Notification] = []
        self._current: Optional[Notification] = None
        self._state: ToastState = ToastState.IDLE
        self._timer: float = 0.0

    # -- public properties ---------------------------------------------------

    @property
    def is_idle(self) -> bool:
        """True when nothing is being shown and the queue is empty."""
        return self._state == ToastState.IDLE and not self._queue

    # -- public interface ----------------------------------------------------

    def add(self, notification: Notification) -> None:
        """Enqueue a notification.

        If *notification* has CRITICAL priority and something is currently
        showing, it interrupts immediately.
        """
        if (
            notification.priority == NotificationPriority.CRITICAL
            and self._current is not None
            and self._state != ToastState.IDLE
        ):
            self._interrupt_current(notification)
            return

        self._queue.append(notification)
        # Keep highest-priority items first (descending by enum value).
        self._queue.sort(key=lambda n: n.priority.value, reverse=True)
        logger.debug(
            "Enqueued %s (priority=%s), queue size=%d",
            notification.title,
            notification.priority.name,
            len(self._queue),
        )

    def update(self, dt: float) -> None:
        """Advance the animation state machine by *dt* seconds."""
        if self._state == ToastState.IDLE:
            self._show_next()
            return

        if self._current is None:
            return

        self._timer += dt

        if self._state == ToastState.APPEARING:
            if self._timer >= ToastRenderer.APPEAR_DURATION:
                self._state = ToastState.SHOWING
                self._timer = 0.0
                logger.debug("Transition -> SHOWING for %s", self._current.title)

        elif self._state == ToastState.SHOWING:
            if self._timer >= self._current.duration:
                self._state = ToastState.DISAPPEARING
                self._timer = 0.0
                logger.debug("Transition -> DISAPPEARING for %s", self._current.title)

        elif self._state == ToastState.DISAPPEARING:
            if self._timer >= ToastRenderer.DISAPPEAR_DURATION:
                logger.debug("Transition -> IDLE (finished %s)", self._current.title)
                self._current = None
                self._state = ToastState.IDLE
                self._timer = 0.0

    def render(self, screen: object) -> None:
        """Render the current toast (if any) to *screen*."""
        if self._current is None or self._state == ToastState.IDLE:
            return

        progress = self._calc_progress()
        self._renderer.render(
            screen,
            self._current.title,
            self._current.content,
            progress,
            self._state.value,
        )

    def cleanup(self) -> None:
        """Release renderer resources."""
        self._renderer.cleanup()
        self._queue.clear()
        self._current = None
        self._state = ToastState.IDLE
        self._timer = 0.0

    # -- private helpers -----------------------------------------------------

    def _interrupt_current(self, notification: Notification) -> None:
        """Replace the currently-shown notification immediately."""
        logger.info(
            "CRITICAL interrupt: replacing %s with %s",
            self._current.title if self._current else "<none>",
            notification.title,
        )
        self._current = notification
        self._state = ToastState.APPEARING
        self._timer = 0.0

    def _show_next(self) -> None:
        """Pop the highest-priority notification from the queue and show it."""
        if not self._queue:
            return
        self._current = self._queue.pop(0)
        self._state = ToastState.APPEARING
        self._timer = 0.0
        logger.debug("Showing next: %s", self._current.title)

    def _calc_progress(self) -> float:
        """Return a 0.0-1.0 progress value for the current animation phase."""
        if self._state == ToastState.APPEARING:
            return min(self._timer / ToastRenderer.APPEAR_DURATION, 1.0)
        if self._state == ToastState.SHOWING:
            if self._current.duration <= 0:
                return 1.0
            return min(self._timer / self._current.duration, 1.0)
        if self._state == ToastState.DISAPPEARING:
            return min(self._timer / ToastRenderer.DISAPPEAR_DURATION, 1.0)
        return 0.0
