"""
搜索/添加好友面板
"""
import pygame
import logging
from typing import List, Any, Optional

from ..constants import (
    FRIEND_PANEL_WIDTH, FRIEND_PANEL_BG_COLOR, FRIEND_PANEL_BORDER_COLOR,
    FRIEND_PANEL_TEXT_COLOR, FRIEND_PANEL_TITLE_COLOR, FRIEND_PANEL_BUTTON_COLOR,
    FRIEND_PANEL_ROW_HEIGHT,
)

logger = logging.getLogger("client.ui.friend_search_panel")


class FriendSearchPanel:
    def __init__(self, screen_width: int, screen_height: int):
        self._visible = False
        self._input_text = ""
        self._input_active = False
        self._results: List[Any] = []

        panel_w = 350
        panel_h = 300
        self._panel_x = (screen_width - panel_w) // 2
        self._panel_y = (screen_height - panel_h) // 2
        self._panel_rect = pygame.Rect(self._panel_x, self._panel_y, panel_w, panel_h)

        self._input_rect = pygame.Rect(self._panel_x + 10, self._panel_y + 40, panel_w - 20, 30)
        self._search_rect = pygame.Rect(self._panel_x + panel_w - 80, self._panel_y + 75, 70, 28)
        self._close_rect = pygame.Rect(self._panel_x + panel_w - 30, self._panel_y + 5, 24, 24)

        try:
            self._font = pygame.font.SysFont("microsoftyahei", 13)
            self._title_font = pygame.font.SysFont("microsoftyahei", 16, bold=True)
        except Exception:
            self._font = pygame.font.Font(None, 15)
            self._title_font = pygame.font.Font(None, 18)

    def toggle(self):
        self._visible = not self._visible
        self._input_active = self._visible

    def is_visible(self):
        return self._visible

    def is_close_clicked(self, pos):
        return self._close_rect.collidepoint(pos)

    def is_search_clicked(self, pos):
        return self._search_rect.collidepoint(pos)

    def is_input_area(self, pos):
        return self._input_rect.collidepoint(pos)

    def get_clicked_result_index(self, pos) -> int:
        x = pos[0]
        y = pos[1]
        start_y = self._panel_y + 110
        if x < self._panel_x or x > self._panel_x + self._panel_rect.w:
            return -1
        if y < start_y:
            return -1
        idx = (y - start_y) // FRIEND_PANEL_ROW_HEIGHT
        if idx < len(self._results):
            return idx
        return -1

    def get_search_text(self) -> str:
        return self._input_text.strip()

    def set_results(self, results: List[Any]):
        self._results = results

    def handle_key(self, event):
        if not self._input_active:
            return
        if event.key == pygame.K_BACKSPACE:
            self._input_text = self._input_text[:-1]
        elif event.key == pygame.K_RETURN:
            return True  # trigger search
        elif len(self._input_text) < 50:
            self._input_text += event.unicode
        return False

    def render(self, screen: pygame.Surface):
        if not self._visible:
            return

        overlay = pygame.Surface((self._panel_rect.w, self._panel_rect.h), pygame.SRCALPHA)
        overlay.fill(FRIEND_PANEL_BG_COLOR)
        screen.blit(overlay, (self._panel_x, self._panel_y))
        pygame.draw.rect(screen, FRIEND_PANEL_BORDER_COLOR, self._panel_rect, 2)

        title = self._title_font.render("搜索玩家", True, FRIEND_PANEL_TITLE_COLOR)
        screen.blit(title, (self._panel_x + 10, self._panel_y + 10))

        close_text = self._font.render("×", True, (255, 100, 100))
        screen.blit(close_text, (self._close_rect.x + 6, self._close_rect.y + 2))

        # Input
        pygame.draw.rect(screen, (40, 40, 50), self._input_rect)
        pygame.draw.rect(screen, (80, 80, 100), self._input_rect, 1)
        input_surf = self._font.render(self._input_text, True, FRIEND_PANEL_TEXT_COLOR)
        screen.blit(input_surf, (self._input_rect.x + 5, self._input_rect.y + 8))

        # Search button
        pygame.draw.rect(screen, FRIEND_PANEL_BUTTON_COLOR, self._search_rect)
        btn_text = self._font.render("搜索", True, (255, 255, 255))
        screen.blit(btn_text, (self._search_rect.x + 15, self._search_rect.y + 6))

        # Results
        visible_count = (self._panel_rect.h - 110) // FRIEND_PANEL_ROW_HEIGHT
        for i, result in enumerate(self._results[:visible_count]):
            row_y = self._panel_y + 110 + i * FRIEND_PANEL_ROW_HEIGHT
            name_text = self._font.render(f"{result.role_name} Lv.{result.level}", True, FRIEND_PANEL_TEXT_COLOR)
            screen.blit(name_text, (self._panel_x + 15, row_y + 10))
            add_text = self._font.render("[添加]", True, (100, 200, 100))
            screen.blit(add_text, (self._panel_x + 250, row_y + 10))
