"""
私聊面板
显示聊天记录、输入框、表情按钮。
"""
import pygame
import logging
from typing import List, Any, Optional

from ..constants import (
    CHAT_PANEL_WIDTH, CHAT_PANEL_HEIGHT,
    CHAT_PANEL_BG_COLOR, CHAT_PANEL_MSG_SELF_COLOR,
    CHAT_PANEL_MSG_OTHER_COLOR, FRIEND_PANEL_TEXT_COLOR,
    FRIEND_PANEL_TITLE_COLOR, FRIEND_PANEL_BORDER_COLOR,
)

logger = logging.getLogger("client.ui.friend_chat_panel")


class FriendChatPanel:
    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._visible = False
        self._target_id = 0
        self._target_name = ""
        self._input_text = ""
        self._input_active = False
        self._scroll_offset = 0

        self._panel_x = screen_width - CHAT_PANEL_WIDTH - 20
        self._panel_y = (screen_height - CHAT_PANEL_HEIGHT) // 2
        self._panel_rect = pygame.Rect(self._panel_x, self._panel_y,
                                        CHAT_PANEL_WIDTH, CHAT_PANEL_HEIGHT)

        self._input_rect = pygame.Rect(
            self._panel_x + 10,
            self._panel_y + CHAT_PANEL_HEIGHT - 40,
            CHAT_PANEL_WIDTH - 80, 30)

        self._send_rect = pygame.Rect(
            self._panel_x + CHAT_PANEL_WIDTH - 65,
            self._panel_y + CHAT_PANEL_HEIGHT - 40, 55, 30)

        self._close_rect = pygame.Rect(
            self._panel_x + CHAT_PANEL_WIDTH - 30,
            self._panel_y + 5, 24, 24)

        try:
            self._font = pygame.font.SysFont("microsoftyahei", 13)
            self._title_font = pygame.font.SysFont("microsoftyahei", 16, bold=True)
            self._input_font = pygame.font.SysFont("microsoftyahei", 14)
        except Exception:
            self._font = pygame.font.Font(None, 15)
            self._title_font = pygame.font.Font(None, 18)
            self._input_font = pygame.font.Font(None, 16)

    def open(self, target_id: int, target_name: str):
        self._target_id = target_id
        self._target_name = target_name
        self._visible = True
        self._input_active = True

    def close(self):
        self._visible = False
        self._input_active = False

    def is_visible(self):
        return self._visible

    def is_panel_area(self, pos) -> bool:
        return self._panel_rect.collidepoint(pos)

    def is_close_clicked(self, pos) -> bool:
        return self._close_rect.collidepoint(pos)

    def is_input_area(self, pos) -> bool:
        return self._input_rect.collidepoint(pos)

    def is_send_clicked(self, pos) -> bool:
        return self._send_rect.collidepoint(pos)

    def handle_key(self, event) -> Optional[str]:
        if not self._input_active:
            return None
        if event.key == pygame.K_BACKSPACE:
            self._input_text = self._input_text[:-1]
        elif event.key == pygame.K_RETURN:
            if self._input_text.strip():
                msg = self._input_text.strip()
                self._input_text = ""
                return msg
        elif len(self._input_text) < 200:
            self._input_text += event.unicode
        return None

    def get_input_text(self) -> str:
        return self._input_text

    def clear_input(self):
        self._input_text = ""

    def render(self, screen: pygame.Surface, messages: List[Any], my_player_id: int):
        if not self._visible:
            return

        # Background
        overlay = pygame.Surface((CHAT_PANEL_WIDTH, CHAT_PANEL_HEIGHT), pygame.SRCALPHA)
        overlay.fill(CHAT_PANEL_BG_COLOR)
        screen.blit(overlay, (self._panel_x, self._panel_y))
        pygame.draw.rect(screen, FRIEND_PANEL_BORDER_COLOR, self._panel_rect, 2)

        # Title
        title = self._title_font.render(f"与 {self._target_name} 私聊", True, FRIEND_PANEL_TITLE_COLOR)
        screen.blit(title, (self._panel_x + 10, self._panel_y + 10))

        # Close
        close_text = self._font.render("×", True, (255, 100, 100))
        screen.blit(close_text, (self._close_rect.x + 6, self._close_rect.y + 2))

        # Messages
        msg_y = self._panel_y + 40
        max_msg_y = self._panel_y + CHAT_PANEL_HEIGHT - 50
        visible_msgs = messages[-20:] if len(messages) > 20 else messages

        for msg in visible_msgs:
            if msg_y > max_msg_y:
                break
            is_self = (msg.sender_id == my_player_id)
            bg_color = CHAT_PANEL_MSG_SELF_COLOR if is_self else CHAT_PANEL_MSG_OTHER_COLOR

            # Message bubble
            text_surf = self._font.render(msg.content, True, FRIEND_PANEL_TEXT_COLOR)
            bubble_w = min(text_surf.get_width() + 20, CHAT_PANEL_WIDTH - 40)
            bubble_x = self._panel_x + CHAT_PANEL_WIDTH - bubble_w - 15 if is_self else self._panel_x + 15

            bubble = pygame.Surface((bubble_w, 28), pygame.SRCALPHA)
            bubble.fill(bg_color)
            screen.blit(bubble, (bubble_x, msg_y))
            screen.blit(text_surf, (bubble_x + 10, msg_y + 6))

            msg_y += 35

        # Input box
        pygame.draw.rect(screen, (40, 40, 50), self._input_rect)
        pygame.draw.rect(screen, (80, 80, 100), self._input_rect, 1)
        input_surf = self._input_font.render(self._input_text, True, FRIEND_PANEL_TEXT_COLOR)
        screen.blit(input_surf, (self._input_rect.x + 5, self._input_rect.y + 7))

        # Send button
        pygame.draw.rect(screen, (60, 120, 60), self._send_rect)
        send_text = self._font.render("发送", True, (255, 255, 255))
        screen.blit(send_text, (self._send_rect.x + 10, self._send_rect.y + 7))
