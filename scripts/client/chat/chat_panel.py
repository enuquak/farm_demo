"""Chat panel UI component for rendering and input handling."""
import logging
from typing import Optional, List

import pygame

from .chat_channel import ChatMessage, ChannelType
from .chat_manager import ChatManager

logger = logging.getLogger("client.chat.chat_panel")


class ChatPanel:
    """Left-bottom chat panel with channel filtering and input."""

    # Layout constants
    PANEL_WIDTH = 350
    PANEL_HEIGHT = 200
    MARGIN = 10
    INPUT_HEIGHT = 25
    TAB_HEIGHT = 22
    MSG_LINE_HEIGHT = 18
    MAX_VISIBLE_MESSAGES = 8

    # Colors
    BG_COLOR = (0, 0, 0, 140)        # Semi-transparent black
    BORDER_COLOR = (80, 80, 80, 200)
    TEXT_COLOR = (220, 220, 220)      # Light gray
    INPUT_BG = (30, 30, 30, 200)
    INPUT_ACTIVE_BG = (40, 40, 40, 220)
    TAB_ACTIVE_COLOR = (100, 180, 255)  # Blue highlight
    TAB_INACTIVE_COLOR = (120, 120, 120)
    CHANNEL_COLORS = {
        "world": (100, 200, 100),    # Green
        "party": (100, 180, 255),    # Blue
        "whisper": (255, 180, 100),  # Orange
    }

    # Channel tabs
    TAB_LABELS = ["全部", "世界", "队伍", "私聊"]
    TAB_FILTERS = ["all", "world", "party", "whisper"]

    def __init__(self, screen_width: int, screen_height: int):
        self._x = self.MARGIN
        self._y = screen_height - self.PANEL_HEIGHT - self.MARGIN
        self._screen_width = screen_width
        self._screen_height = screen_height

        # Input state
        self._input_active = False
        self._input_text = ""

        # Filter state
        self._selected_filter = "all"

        # Font
        pygame.font.init()
        self._font = pygame.font.SysFont("microsoftyahei", 14)
        if self._font is None:
            self._font = pygame.font.Font(None, 14)
        self._tab_font = pygame.font.SysFont("microsoftyahei", 12)
        if self._tab_font is None:
            self._tab_font = pygame.font.Font(None, 12)

        # Cached surface
        self._surface: Optional[pygame.Surface] = None
        self._needs_redraw = True

        logger.info("ChatPanel initialized at (%d, %d)", self._x, self._y)

    @property
    def is_input_active(self) -> bool:
        """Whether the chat input is currently focused."""
        return self._input_active

    def handle_event(self, event: pygame.event.Event, chat_manager: ChatManager) -> bool:
        """Handle a pygame event. Returns True if the event was consumed."""

        # Enter key: toggle input
        if event.type == pygame.KEYDOWN and event.key == pygame.K_RETURN:
            if self._input_active:
                # Send message
                if self._input_text.strip():
                    error = chat_manager.send_message(self._input_text.strip())
                    if error:
                        logger.warning("Chat send error: %s", error)
                self._input_text = ""
                self._input_active = False
                self._needs_redraw = True
            else:
                self._input_active = True
                self._needs_redraw = True
            return True

        # Esc key: close input
        if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
            if self._input_active:
                self._input_active = False
                self._input_text = ""
                self._needs_redraw = True
                return True

        # Tab key: switch channel filter
        if event.type == pygame.KEYDOWN and event.key == pygame.K_TAB:
            if not self._input_active:
                idx = self.TAB_FILTERS.index(self._selected_filter)
                self._selected_filter = self.TAB_FILTERS[(idx + 1) % len(self.TAB_FILTERS)]
                self._needs_redraw = True
                return True

        # Text input when active
        if event.type == pygame.KEYDOWN and self._input_active:
            if event.key == pygame.K_BACKSPACE:
                self._input_text = self._input_text[:-1]
                self._needs_redraw = True
                return True
            elif event.unicode and event.unicode.isprintable():
                # Check max length
                config = chat_manager.get_current_channel().config
                if len(self._input_text) < config.max_msg_length:
                    self._input_text += event.unicode
                    self._needs_redraw = True
                return True

        # Mouse click on tabs
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            mx, my = event.pos
            tab_y = self._y + self.PANEL_HEIGHT - self.INPUT_HEIGHT - self.TAB_HEIGHT
            if tab_y <= my <= tab_y + self.TAB_HEIGHT:
                tab_x = self._x + 5
                for i, label in enumerate(self.TAB_LABELS):
                    tab_surface = self._tab_font.render(label, True, (255, 255, 255))
                    tab_w = tab_surface.get_width() + 12
                    if tab_x <= mx <= tab_x + tab_w:
                        self._selected_filter = self.TAB_FILTERS[i]
                        self._needs_redraw = True
                        return True
                    tab_x += tab_w + 4

        return False

    def render(self, screen: pygame.Surface, chat_manager: ChatManager) -> None:
        """Render the chat panel."""
        # Always redraw (messages change frequently)
        self._surface = pygame.Surface(
            (self.PANEL_WIDTH, self.PANEL_HEIGHT), pygame.SRCALPHA
        )

        # Background
        bg_rect = pygame.Rect(0, 0, self.PANEL_WIDTH, self.PANEL_HEIGHT)
        pygame.draw.rect(self._surface, self.BG_COLOR, bg_rect)
        pygame.draw.rect(self._surface, self.BORDER_COLOR, bg_rect, 1)

        # Messages area
        messages = self._get_filtered_messages(chat_manager)
        visible = messages[-self.MAX_VISIBLE_MESSAGES:]
        msg_y = 5
        for msg in visible:
            channel_name = msg.channel_type.name.lower()
            color = self.CHANNEL_COLORS.get(channel_name, self.TEXT_COLOR)
            prefix = f"[{channel_name}]"
            text = f"{prefix} {msg.sender_name}: {msg.content}"
            # Truncate if too wide
            text_surface = self._font.render(text, True, color)
            if text_surface.get_width() > self.PANEL_WIDTH - 10:
                while self._font.render(text + "...", True, color).get_width() > self.PANEL_WIDTH - 10 and len(text) > 0:
                    text = text[:-1]
                text = text + "..."
                text_surface = self._font.render(text, True, color)
            self._surface.blit(text_surface, (5, msg_y))
            msg_y += self.MSG_LINE_HEIGHT

        # Tab bar
        tab_y = self.PANEL_HEIGHT - self.INPUT_HEIGHT - self.TAB_HEIGHT
        tab_x = 5
        for i, label in enumerate(self.TAB_LABELS):
            is_active = self.TAB_FILTERS[i] == self._selected_filter
            color = self.TAB_ACTIVE_COLOR if is_active else self.TAB_INACTIVE_COLOR
            tab_surface = self._tab_font.render(label, True, color)
            tab_w = tab_surface.get_width() + 12
            tab_rect = pygame.Rect(tab_x, tab_y, tab_w, self.TAB_HEIGHT)
            pygame.draw.rect(self._surface, (40, 40, 40, 150), tab_rect)
            if is_active:
                pygame.draw.rect(self._surface, (*self.TAB_ACTIVE_COLOR, 100), tab_rect, 1)
            self._surface.blit(tab_surface, (tab_x + 6, tab_y + 4))
            tab_x += tab_w + 4

        # Input box
        input_y = self.PANEL_HEIGHT - self.INPUT_HEIGHT
        input_rect = pygame.Rect(2, input_y, self.PANEL_WIDTH - 4, self.INPUT_HEIGHT)
        bg = self.INPUT_ACTIVE_BG if self._input_active else self.INPUT_BG
        pygame.draw.rect(self._surface, bg, input_rect)
        pygame.draw.rect(self._surface, self.BORDER_COLOR, input_rect, 1)

        # Input text
        display_text = self._input_text
        if not self._input_active and not display_text:
            display_text = "Press Enter to chat..."
            text_color = (100, 100, 100)
        else:
            text_color = self.TEXT_COLOR
            if self._input_active:
                display_text = display_text + "_"  # Cursor

        text_surface = self._font.render(display_text, True, text_color)
        self._surface.blit(text_surface, (6, input_y + 5))

        # Blit to screen
        screen.blit(self._surface, (self._x, self._y))

    def _get_filtered_messages(self, chat_manager: ChatManager) -> List[ChatMessage]:
        """Get messages filtered by the selected tab."""
        if self._selected_filter == "all":
            # Merge all channels, sorted by timestamp
            all_msgs = []
            for channel in chat_manager._channels.values():
                all_msgs.extend(channel.get_messages())
            all_msgs.sort(key=lambda m: m.timestamp)
            return all_msgs
        else:
            channel_type = ChannelType[self._selected_filter.upper()]
            channel = chat_manager.get_channel(channel_type)
            return channel.get_messages() if channel else []

    def cleanup(self) -> None:
        """Release resources."""
        if self._font:
            self._font = None
        if self._tab_font:
            self._tab_font = None
