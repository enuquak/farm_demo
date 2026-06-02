"""Client-side chat manager handling send/recv and channel routing."""
import logging
import time
import sys
import os
from typing import Dict, Optional, Callable

from .chat_channel import (
    ChatChannel, ChatMessage, ChannelType, ChannelConfig, CHANNEL_CONFIGS,
)

# Add proto generated path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'common', 'proto', 'generated'))
import chat_pb2
import base_pb2

from ..message_ids import MSG_ID_CHAT_SEND_REQ, MSG_ID_CHAT_SEND_RESP, MSG_ID_CHAT_MESSAGE

logger = logging.getLogger("client.chat.chat_manager")


class ChatManager:
    """Manages chat channels, sending, and receiving."""

    def __init__(self, connection, player_data: dict):
        self._connection = connection
        self._player_data = player_data
        self._player_id = player_data.get('player_id', 0)
        self._player_name = player_data.get('role_name', 'Unknown')

        # Initialize channels
        self._channels: Dict[ChannelType, ChatChannel] = {}
        for channel_type, config in CHANNEL_CONFIGS.items():
            self._channels[channel_type] = ChatChannel(config)

        # Current active channel for sending
        self._current_channel: ChannelType = ChannelType.WORLD

        # Callback for UI to know when messages arrive
        self._on_message_received: Optional[Callable[[], None]] = None

        logger.info("ChatManager initialized for player %d (%s)",
                     self._player_id, self._player_name)

    @property
    def current_channel(self) -> ChannelType:
        return self._current_channel

    @current_channel.setter
    def current_channel(self, channel_type: ChannelType) -> None:
        self._current_channel = channel_type

    def get_channel(self, channel_type: ChannelType) -> Optional[ChatChannel]:
        return self._channels.get(channel_type)

    def get_current_channel(self) -> ChatChannel:
        return self._channels[self._current_channel]

    def switch_channel(self) -> None:
        types = list(ChannelType)
        idx = types.index(self._current_channel)
        self._current_channel = types[(idx + 1) % len(types)]
        logger.debug("Switched to channel: %s", self._current_channel.name)

    def set_on_message_received(self, callback: Callable[[], None]) -> None:
        self._on_message_received = callback

    def send_message(self, content: str, target_id: int = 0) -> Optional[str]:
        """Send a chat message. Returns error string or None on success."""
        channel = self._channels[self._current_channel]

        error = channel.validate_message(content)
        if error:
            return error

        if not channel.can_send():
            return "Sending too fast"

        # Build protobuf
        req = chat_pb2.ChatSendReq()
        req.channel_type = self._current_channel.value
        req.content = content
        req.target_id = target_id

        # Wrap in PlayerMsg
        player_msg = base_pb2.PlayerMsg()
        player_msg.player_id = self._player_id
        player_msg.server_id = self._player_data.get('server_id', 1)
        player_msg.msg_id = MSG_ID_CHAT_SEND_REQ
        player_msg.payload = req.SerializeToString()

        try:
            self._connection.send_message(MSG_ID_CHAT_SEND_REQ, player_msg.SerializeToString())
            channel.record_send()
            logger.debug("Sent chat message to %s: %s", self._current_channel.name, content)
            return None
        except Exception as e:
            logger.error("Failed to send chat message: %s", e)
            return "Failed to send"

    def on_chat_message(self, payload: bytes) -> None:
        """Handle incoming ChatMessage from server."""
        try:
            msg = chat_pb2.ChatMessage()
            msg.ParseFromString(payload)

            channel_type = ChannelType(msg.channel_type)
            chat_msg = ChatMessage(
                channel_type=channel_type,
                sender_id=msg.sender_id,
                sender_name=msg.sender_name,
                content=msg.content,
                timestamp=msg.timestamp,
                target_id=msg.target_id,
            )

            channel = self._channels.get(channel_type)
            if channel:
                channel.add_message(chat_msg)
                if self._on_message_received:
                    self._on_message_received()
            else:
                logger.warning("Received message for unknown channel: %s", channel_type)

        except Exception as e:
            logger.error("Failed to parse ChatMessage: %s", e)

    def on_chat_send_resp(self, payload: bytes) -> None:
        """Handle ChatSendResp from server."""
        try:
            resp = chat_pb2.ChatSendResp()
            resp.ParseFromString(payload)
            if resp.code != 0:
                logger.warning("Chat send failed: code=%d, msg=%s", resp.code, resp.msg)
        except Exception as e:
            logger.error("Failed to parse ChatSendResp: %s", e)
