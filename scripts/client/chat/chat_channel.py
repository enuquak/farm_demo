"""Chat channel configuration and message storage."""
import logging
import time
from dataclasses import dataclass, field
from enum import Enum, unique
from typing import List, Optional

logger = logging.getLogger("client.chat.chat_channel")


@unique
class ChannelType(Enum):
    """Chat channel type identifiers."""
    WORLD = 0
    PARTY = 1
    WHISPER = 2


@dataclass
class ChannelConfig:
    """Configuration for a chat channel."""
    channel_type: ChannelType
    max_msg_length: int
    cooldown_sec: float
    max_history: int = 100


# Default channel configurations
CHANNEL_CONFIGS = {
    ChannelType.WORLD: ChannelConfig(
        channel_type=ChannelType.WORLD,
        max_msg_length=100,
        cooldown_sec=5.0,
        max_history=50,
    ),
    ChannelType.PARTY: ChannelConfig(
        channel_type=ChannelType.PARTY,
        max_msg_length=200,
        cooldown_sec=3.0,
        max_history=100,
    ),
    ChannelType.WHISPER: ChannelConfig(
        channel_type=ChannelType.WHISPER,
        max_msg_length=500,
        cooldown_sec=1.0,
        max_history=200,
    ),
}


@dataclass
class ChatMessage:
    """A single chat message."""
    channel_type: ChannelType
    sender_id: int
    sender_name: str
    content: str
    timestamp: int  # milliseconds
    target_id: int = 0


class ChatChannel:
    """A chat channel that stores messages."""

    def __init__(self, config: ChannelConfig):
        self.config = config
        self.messages: List[ChatMessage] = []
        self._last_send_time: float = 0.0

    def add_message(self, msg: ChatMessage) -> None:
        """Add a message to the channel, enforcing max history."""
        self.messages.append(msg)
        if len(self.messages) > self.config.max_history:
            self.messages.pop(0)
        logger.debug(
            "[%s] %s: %s (total=%d)",
            self.config.channel_type.name,
            msg.sender_name,
            msg.content,
            len(self.messages),
        )

    def get_messages(self) -> List[ChatMessage]:
        """Get all messages in this channel."""
        return self.messages

    def can_send(self) -> bool:
        """Check if the player can send a message (rate limit)."""
        now = time.time()
        return (now - self._last_send_time) >= self.config.cooldown_sec

    def record_send(self) -> None:
        """Record that a message was sent (for rate limiting)."""
        self._last_send_time = time.time()

    def validate_message(self, content: str) -> Optional[str]:
        """Validate a message before sending. Returns error string or None."""
        if not content.strip():
            return "Message cannot be empty"
        if len(content) > self.config.max_msg_length:
            return f"Message too long ({len(content)}/{self.config.max_msg_length})"
        return None