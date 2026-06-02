"""Notification manager that routes notifications to the appropriate display channel."""

import logging
from typing import Optional

from ..notification import Notification, NotificationPriority, NotificationType
from .toast_channel import ToastChannel
from .marquee_channel import MarqueeChannel

logger = logging.getLogger("client.ui.notification_manager")


class NotificationManager:
    """Central manager that routes notifications to toast or marquee channels."""

    def __init__(self, screen_width: int, screen_height: int):
        self._toast_channel = ToastChannel(screen_width, screen_height)
        self._marquee_channel = MarqueeChannel(screen_width, screen_height)

    def add(self, notification: Notification) -> None:
        """Route a notification to the appropriate channel.

        If the notification's channel is "marquee", the title is sent to the
        marquee channel.  Otherwise it is forwarded to the toast channel.
        """
        if notification.channel == "marquee":
            self._marquee_channel.add(notification.title)
        else:
            self._toast_channel.add(notification)

    def show_toast(
        self,
        title: str,
        content: str = "",
        priority: NotificationPriority = NotificationPriority.NORMAL,
        duration: float = 3.0,
    ) -> None:
        """Convenience method to display a toast notification."""
        notification = Notification(
            type=NotificationType.SYSTEM_NOTICE,
            priority=priority,
            title=title,
            content=content,
            duration=duration,
            channel="toast",
        )
        self.add(notification)

    def show_marquee(self, text: str) -> None:
        """Convenience method to display a marquee (scrolling) notification."""
        self._marquee_channel.add(text)

    def update(self, dt: float) -> None:
        """Update both channels by *dt* seconds."""
        self._marquee_channel.update(dt)
        self._toast_channel.update(dt)

    def render(self, screen: object, dt: float) -> None:
        """Update then render both channels (marquee first, then toast)."""
        self.update(dt)
        self._marquee_channel.render(screen)
        self._toast_channel.render(screen)

    def cleanup(self) -> None:
        """Release resources held by both channels."""
        self._marquee_channel.cleanup()
        self._toast_channel.cleanup()
