"""
好友列表面板
显示好友列表、在线状态、操作按钮。
"""
import pygame
import logging
from typing import List, Any, Optional, Callable

from ..constants import (
    FRIEND_PANEL_WIDTH, FRIEND_PANEL_HEIGHT,
    FRIEND_PANEL_BG_COLOR, FRIEND_PANEL_BORDER_COLOR,
    FRIEND_PANEL_ROW_HEIGHT, FRIEND_PANEL_ONLINE_COLOR,
    FRIEND_PANEL_OFFLINE_COLOR, FRIEND_PANEL_HOVER_COLOR,
    FRIEND_PANEL_BUTTON_COLOR, FRIEND_PANEL_TEXT_COLOR,
    FRIEND_PANEL_TITLE_COLOR,
)

logger = logging.getLogger("client.ui.friend_list_panel")


class FriendListPanel:
    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._visible = False
        self._hovered_index = -1
        self._scroll_offset = 0

        # Layout
        self._panel_x = (screen_width - FRIEND_PANEL_WIDTH) // 2
        self._panel_y = (screen_height - FRIEND_PANEL_HEIGHT) // 2
        self._panel_rect = pygame.Rect(self._panel_x, self._panel_y,
                                        FRIEND_PANEL_WIDTH, FRIEND_PANEL_HEIGHT)

        # Close button
        self._close_rect = pygame.Rect(
            self._panel_x + FRIEND_PANEL_WIDTH - 30,
            self._panel_y + 5, 24, 24)

        # Fonts
        try:
            self._title_font = pygame.font.SysFont("microsoftyahei", 18, bold=True)
            self._font = pygame.font.SysFont("microsoftyahei", 14)
            self._small_font = pygame.font.SysFont("microsoftyahei", 12)
        except Exception:
            self._title_font = pygame.font.Font(None, 20)
            self._font = pygame.font.Font(None, 16)
            self._small_font = pygame.font.Font(None, 14)

    def toggle(self):
        self._visible = not self._visible

    def is_visible(self):
        return self._visible

    def is_panel_area(self, pos) -> bool:
        return self._panel_rect.collidepoint(pos)

    def is_close_clicked(self, pos) -> bool:
        return self._close_rect.collidepoint(pos)

    def get_clicked_friend_index(self, pos) -> int:
        if not self._visible:
            return -1
        x, y = pos
        list_start_y = self._panel_y + 50
        if x < self._panel_x or x > self._panel_x + FRIEND_PANEL_WIDTH:
            return -1
        if y < list_start_y or y > self._panel_y + FRIEND_PANEL_HEIGHT - 50:
            return -1
        index = (y - list_start_y + self._scroll_offset * FRIEND_PANEL_ROW_HEIGHT) // FRIEND_PANEL_ROW_HEIGHT
        return index

    def render(self, screen: pygame.Surface, friends: List[Any], pending_count: int = 0):
        if not self._visible:
            return

        # Panel background
        overlay = pygame.Surface((FRIEND_PANEL_WIDTH, FRIEND_PANEL_HEIGHT), pygame.SRCALPHA)
        overlay.fill(FRIEND_PANEL_BG_COLOR)
        screen.blit(overlay, (self._panel_x, self._panel_y))

        # Border
        pygame.draw.rect(screen, FRIEND_PANEL_BORDER_COLOR, self._panel_rect, 2)

        # Title
        title = self._title_font.render(f"好友列表 ({len(friends)}/50)", True, FRIEND_PANEL_TITLE_COLOR)
        screen.blit(title, (self._panel_x + 10, self._panel_y + 10))

        # Close button
        close_text = self._font.render("×", True, (255, 100, 100))
        screen.blit(close_text, (self._close_rect.x + 6, self._close_rect.y + 2))

        # Friend list
        list_start_y = self._panel_y + 50
        visible_count = (FRIEND_PANEL_HEIGHT - 100) // FRIEND_PANEL_ROW_HEIGHT

        for i in range(min(visible_count, len(friends))):
            idx = i + self._scroll_offset
            if idx >= len(friends):
                break

            friend = friends[idx]
            row_y = list_start_y + i * FRIEND_PANEL_ROW_HEIGHT

            # Hover highlight
            if idx == self._hovered_index:
                hover_surf = pygame.Surface((FRIEND_PANEL_WIDTH - 20, FRIEND_PANEL_ROW_HEIGHT), pygame.SRCALPHA)
                hover_surf.fill(FRIEND_PANEL_HOVER_COLOR)
                screen.blit(hover_surf, (self._panel_x + 10, row_y))

            # Online indicator
            color = FRIEND_PANEL_ONLINE_COLOR if friend.online else FRIEND_PANEL_OFFLINE_COLOR
            pygame.draw.circle(screen, color, (self._panel_x + 25, row_y + 20), 6)

            # Name and level
            name_text = self._font.render(f"{friend.role_name}  Lv.{friend.level}", True, FRIEND_PANEL_TEXT_COLOR)
            screen.blit(name_text, (self._panel_x + 40, row_y + 10))

            # Scene
            scene_text = self._small_font.render(friend.scene_id if friend.online else "离线", True, color)
            screen.blit(scene_text, (self._panel_x + 250, row_y + 12))

        # Pending requests indicator
        if pending_count > 0:
            req_text = self._font.render(f"好友请求 ({pending_count})", True, (255, 200, 100))
            screen.blit(req_text, (self._panel_x + 10, self._panel_y + FRIEND_PANEL_HEIGHT - 35))

    def update_hover(self, pos):
        if not self._visible:
            self._hovered_index = -1
            return
        self._hovered_index = self.get_clicked_friend_index(pos)
