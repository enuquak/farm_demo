"""
Unit tests for the notification system.

Tests cover Notification data structures, ToastChannel, MarqueeChannel,
and NotificationManager.
"""

import sys
import os
import unittest

# Add project root to path so the scripts.client package can be resolved
_project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
if _project_root not in sys.path:
    sys.path.insert(0, _project_root)

import pygame

from scripts.client.notification import Notification, NotificationType, NotificationPriority
from scripts.client.ui.toast_channel import ToastChannel
from scripts.client.ui.marquee_channel import MarqueeChannel
from scripts.client.ui.notification_manager import NotificationManager


class TestNotification(unittest.TestCase):
    """Test Notification dataclass creation and validation."""

    def test_notification_creation(self):
        """Create notification with DAWN type, verify all fields."""
        n = Notification(
            type=NotificationType.DAWN,
            priority=NotificationPriority.HIGH,
            title="Dawn",
            content="The sun rises.",
            duration=5.0,
            channel="toast",
        )
        self.assertEqual(n.type, NotificationType.DAWN)
        self.assertEqual(n.priority, NotificationPriority.HIGH)
        self.assertEqual(n.title, "Dawn")
        self.assertEqual(n.content, "The sun rises.")
        self.assertEqual(n.duration, 5.0)
        self.assertEqual(n.channel, "toast")

    def test_notification_with_content(self):
        """Create notification with content and marquee channel."""
        n = Notification(
            type=NotificationType.SYSTEM_NOTICE,
            priority=NotificationPriority.NORMAL,
            title="Server Announcement",
            content="Double XP this weekend!",
            duration=10.0,
            channel="marquee",
        )
        self.assertEqual(n.type, NotificationType.SYSTEM_NOTICE)
        self.assertEqual(n.priority, NotificationPriority.NORMAL)
        self.assertEqual(n.title, "Server Announcement")
        self.assertEqual(n.content, "Double XP this weekend!")
        self.assertEqual(n.duration, 10.0)
        self.assertEqual(n.channel, "marquee")


class TestToastChannel(unittest.TestCase):
    """Test ToastChannel queue management and priority logic."""

    def setUp(self):
        """Initialize pygame and create a ToastChannel."""
        pygame.init()
        self.screen = pygame.display.set_mode((800, 600))
        self.channel = ToastChannel(800, 600)

    def tearDown(self):
        """Cleanup pygame."""
        self.channel.cleanup()
        pygame.quit()

    def test_add_notification(self):
        """Add notification, verify queue length."""
        n = Notification(
            type=NotificationType.DAWN,
            priority=NotificationPriority.NORMAL,
            title="Test",
        )
        self.channel.add(n)
        self.assertEqual(len(self.channel._queue), 1)

    def test_priority_ordering(self):
        """Add LOW then HIGH, verify HIGH is first in queue."""
        low = Notification(
            type=NotificationType.DAWN,
            priority=NotificationPriority.LOW,
            title="Low",
        )
        high = Notification(
            type=NotificationType.DUSK,
            priority=NotificationPriority.HIGH,
            title="High",
        )
        self.channel.add(low)
        self.channel.add(high)
        self.assertEqual(self.channel._queue[0].title, "High")

    def test_critical_interrupts(self):
        """Add NORMAL, update to show it, then add CRITICAL; CRITICAL becomes current."""
        normal = Notification(
            type=NotificationType.DAWN,
            priority=NotificationPriority.NORMAL,
            title="Normal",
        )
        critical = Notification(
            type=NotificationType.SYSTEM_NOTICE,
            priority=NotificationPriority.CRITICAL,
            title="Critical",
        )
        self.channel.add(normal)
        # Update to let the channel pick up the normal notification
        self.channel.update(0.01)
        self.assertEqual(self.channel._current.title, "Normal")

        self.channel.add(critical)
        self.assertEqual(self.channel._current.title, "Critical")


class TestMarqueeChannel(unittest.TestCase):
    """Test MarqueeChannel text queue and scrolling state."""

    def setUp(self):
        """Initialize pygame and create a MarqueeChannel."""
        pygame.init()
        self.screen = pygame.display.set_mode((800, 600))
        self.channel = MarqueeChannel(800, 600)

    def tearDown(self):
        """Cleanup pygame."""
        self.channel.cleanup()
        pygame.quit()

    def test_add_text(self):
        """Add text, verify queue length."""
        self.channel.add("Hello world")
        # First text is immediately popped into scrolling, so queue should be 0
        self.assertEqual(len(self.channel._queue), 0)

    def test_scrolling_state(self):
        """Add text, verify state is SCROLLING."""
        from scripts.client.ui.marquee_channel import MarqueeState
        self.channel.add("Scroll me")
        self.assertEqual(self.channel._state, MarqueeState.SCROLLING)

    def test_multiple_texts(self):
        """Add two texts, verify one in queue."""
        self.channel.add("First")
        self.channel.add("Second")
        # First is being scrolled, second is in queue
        self.assertEqual(len(self.channel._queue), 1)
        self.assertEqual(self.channel._queue[0], "Second")


class TestNotificationManager(unittest.TestCase):
    """Test NotificationManager routing and convenience methods."""

    def setUp(self):
        """Initialize pygame and create a NotificationManager."""
        pygame.init()
        self.screen = pygame.display.set_mode((800, 600))
        self.manager = NotificationManager(800, 600)

    def tearDown(self):
        """Cleanup pygame."""
        self.manager.cleanup()
        pygame.quit()

    def test_show_toast(self):
        """Call show_toast, no exception."""
        self.manager.show_toast("Test Toast", "Content here")

    def test_show_marquee(self):
        """Call show_marquee, no exception."""
        self.manager.show_marquee("Scrolling text")

    def test_add_notification(self):
        """Add notification, no exception."""
        n = Notification(
            type=NotificationType.DAWN,
            priority=NotificationPriority.NORMAL,
            title="Dawn",
            content="A new day begins.",
        )
        self.manager.add(n)


if __name__ == "__main__":
    unittest.main()
