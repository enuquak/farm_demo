"""
农场访问 HUD
在访问好友农场时显示的操作面板。
"""
import pygame
import logging
from typing import Optional

from ..constants import (
    FRIEND_PANEL_BG_COLOR, FRIEND_PANEL_BORDER_COLOR,
    FRIEND_PANEL_TEXT_COLOR, FRIEND_PANEL_TITLE_COLOR,
    FRIEND_PANEL_BUTTON_COLOR,
)

logger = logging.getLogger("client.ui.friend_visit_panel")


class FriendVisitPanel:
    def __init__(self, screen_width: int, screen_height: int):
        self._visible = False
        self._owner_name = ""
        self._actions_remaining = 5

        panel_w = 250
        panel_h = 180
        self._panel_x = 20
        self._panel_y = screen_height - panel_h - 80
        self._panel_rect = pygame.Rect(self._panel_x, self._panel_y, panel_w, panel_h)

        btn_w = 90
        btn_h = 30
        self._water_rect = pygame.Rect(self._panel_x + 10, self._panel_y + 70, btn_w, btn_h)
        self._weed_rect = pygame.Rect(self._panel_x + 110, self._panel_y + 70, btn_w, btn_h)
        self._steal_rect = pygame.Rect(self._panel_x + 10, self._panel_y + 110, btn_w, btn_h)
        self._return_rect = pygame.Rect(self._panel_x + 110, self._panel_y + 110, btn_w, btn_h)

        try:
            self._font = pygame.font.SysFont("microsoftyahei", 13)
            self._title_font = pygame.font.SysFont("microsoftyahei", 15, bold=True)
        except Exception:
            self._font = pygame.font.Font(None, 15)
            self._title_font = pygame.font.Font(None, 17)

    def open(self, owner_id: int, owner_name: str):
        self._visible = True
        self._owner_id = owner_id
        self._owner_name = owner_name
        self._actions_remaining = 5

    def close(self):
        self._visible = False

    def is_visible(self):
        return self._visible

    def is_water_clicked(self, pos):
        return self._water_rect.collidepoint(pos)

    def is_weed_clicked(self, pos):
        return self._weed_rect.collidepoint(pos)

    def is_steal_clicked(self, pos):
        return self._steal_rect.collidepoint(pos)

    def is_return_clicked(self, pos):
        return self._return_rect.collidepoint(pos)

    def use_action(self):
        if self._actions_remaining > 0:
            self._actions_remaining -= 1
            return True
        return False

    def render(self, screen: pygame.Surface):
        if not self._visible:
            return

        overlay = pygame.Surface((self._panel_rect.w, self._panel_rect.h), pygame.SRCALPHA)
        overlay.fill(FRIEND_PANEL_BG_COLOR)
        screen.blit(overlay, (self._panel_x, self._panel_y))
        pygame.draw.rect(screen, FRIEND_PANEL_BORDER_COLOR, self._panel_rect, 2)

        title = self._title_font.render(f"访问 {self._owner_name} 的农场", True, FRIEND_PANEL_TITLE_COLOR)
        screen.blit(title, (self._panel_x + 10, self._panel_y + 10))

        remain_text = self._font.render(f"剩余操作: {self._actions_remaining}/5", True, FRIEND_PANEL_TEXT_COLOR)
        screen.blit(remain_text, (self._panel_x + 10, self._panel_y + 40))

        # Buttons
        for rect, label in [
            (self._water_rect, "浇水"),
            (self._weed_rect, "除草"),
            (self._steal_rect, "偷菜"),
            (self._return_rect, "返回"),
        ]:
            pygame.draw.rect(screen, FRIEND_PANEL_BUTTON_COLOR, rect)
            btn_text = self._font.render(label, True, (255, 255, 255))
            screen.blit(btn_text, (rect.x + 25, rect.y + 7))
