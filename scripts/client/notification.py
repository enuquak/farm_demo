"""Notification data structures for the client notification system.

Supports Toast (popup) and Marquee (scrolling) notifications with priority management.
"""

from dataclasses import dataclass, field
from enum import Enum, unique


@unique
class NotificationType(Enum):
    """Notification type identifiers."""

    DAWN = "dawn"              # 天亮了
    DUSK = "dusk"              # 天黑了
    SEASON_CHANGE = "season"   # 季节变化
    ITEM_USE_SUCCESS = "item_ok"   # 物品使用成功
    ITEM_USE_FAIL = "item_fail"    # 物品使用失败
    ENERGY_LOW = "energy_low"      # 能量不足
    INVENTORY_FULL = "inv_full"    # 背包已满
    SYSTEM_NOTICE = "system"       # 系统公告
    EVENT_NOTICE = "event"         # 活动通知


@unique
class NotificationPriority(Enum):
    """Notification priority levels (ascending)."""

    LOW = 0
    NORMAL = 1
    HIGH = 2
    CRITICAL = 3


@dataclass
class Notification:
    """A single notification to be displayed in the client.

    Attributes:
        type: The category of this notification.
        priority: Display/importance priority.
        title: Short heading shown to the player.
        content: Optional longer body text.
        duration: How many seconds the notification stays visible.
        channel: Display channel -- "toast" for popup, "marquee" for scrolling.
    """

    type: NotificationType
    priority: NotificationPriority
    title: str
    content: str = ""
    duration: float = 3.0
    channel: str = "toast"

    def __post_init__(self) -> None:
        if not isinstance(self.type, NotificationType):
            raise TypeError(
                f"type must be a NotificationType, got {type(self.type).__name__}"
            )
        if not isinstance(self.priority, NotificationPriority):
            raise TypeError(
                f"priority must be a NotificationPriority, got {type(self.priority).__name__}"
            )
        if not self.title:
            raise ValueError("title must not be empty")
        if self.duration < 0:
            raise ValueError(f"duration must be non-negative, got {self.duration}")
        if self.channel not in ("toast", "marquee"):
            raise ValueError(
                f"channel must be 'toast' or 'marquee', got {self.channel!r}"
            )
