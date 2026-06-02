"""
组队邀请通知 UI 组件
收到邀请时弹出通知，60 秒后自动消失。
"""
import pygame
import time
from typing import Optional, List
from .team_manager import TeamManager, TeamInviteNotify


class TeamInviteNotifyUI:
    """组队邀请通知 UI"""

    WIDTH = 280
    HEIGHT = 80
    MARGIN_RIGHT = 20
    MARGIN_BOTTOM = 20
    TIMEOUT_SEC = 60

    # Colors
    BG_COLOR = (40, 40, 60, 220)
    BORDER_COLOR = (100, 150, 255)
    TEXT_COLOR = (255, 255, 255)
    ACCEPT_COLOR = (60, 180, 60)
    ACCEPT_HOVER = (80, 220, 80)
    REJECT_COLOR = (180, 60, 60)
    REJECT_HOVER = (220, 80, 80)

    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._notifications: List[dict] = []  # {invite, timestamp, accept_rect, reject_rect}

    def add_invite(self, invite: TeamInviteNotify) -> None:
        """添加邀请通知"""
        self._notifications.append({
            "invite": invite,
            "timestamp": time.time(),
            "accept_rect": None,
            "reject_rect": None,
        })

    def handle_event(self, event: pygame.event.Event, team_mgr: TeamManager) -> bool:
        """处理输入事件"""
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            for notif in self._notifications[:]:
                if notif["accept_rect"] and notif["accept_rect"].collidepoint(event.pos):
                    team_mgr.accept_invite(notif["invite"].team_id)
                    self._notifications.remove(notif)
                    return True
                if notif["reject_rect"] and notif["reject_rect"].collidepoint(event.pos):
                    team_mgr.reject_invite(notif["invite"].team_id)
                    self._notifications.remove(notif)
                    return True
        return False

    def update(self) -> None:
        """更新通知列表，移除过期通知"""
        now = time.time()
        self._notifications = [
            n for n in self._notifications
            if now - n["timestamp"] < self.TIMEOUT_SEC
        ]

    def render(self, screen: pygame.Surface) -> None:
        """渲染邀请通知"""
        font = pygame.font.SysFont(None, 18)
        small_font = pygame.font.SysFont(None, 14)

        for i, notif in enumerate(self._notifications):
            invite = notif["invite"]

            # Position (bottom-right, stacked)
            x = self._screen_width - self.WIDTH - self.MARGIN_RIGHT
            y = (self._screen_height - self.HEIGHT - self.MARGIN_BOTTOM -
                 i * (self.HEIGHT + 10))

            # Background
            surface = pygame.Surface((self.WIDTH, self.HEIGHT), pygame.SRCALPHA)
            surface.fill(self.BG_COLOR)
            screen.blit(surface, (x, y))

            # Border
            pygame.draw.rect(screen, self.BORDER_COLOR,
                             (x, y, self.WIDTH, self.HEIGHT), 1)

            # Text
            title = font.render(
                f"{invite.inviter_name} 邀请你加入队伍", True, self.TEXT_COLOR)
            screen.blit(title, (x + 10, y + 10))

            level_text = small_font.render(
                f"等级: {invite.inviter_level}", True, self.TEXT_COLOR)
            screen.blit(level_text, (x + 10, y + 30))

            # Accept button
            accept_rect = pygame.Rect(x + 10, y + 52, 60, 22)
            pygame.draw.rect(screen, self.ACCEPT_COLOR, accept_rect)
            accept_text = small_font.render("接受", True, self.TEXT_COLOR)
            screen.blit(accept_text, (x + 22, y + 56))
            notif["accept_rect"] = accept_rect

            # Reject button
            reject_rect = pygame.Rect(x + 80, y + 52, 60, 22)
            pygame.draw.rect(screen, self.REJECT_COLOR, reject_rect)
            reject_text = small_font.render("拒绝", True, self.TEXT_COLOR)
            screen.blit(reject_text, (x + 92, y + 56))
            notif["reject_rect"] = reject_rect

            # Timeout progress bar
            elapsed = time.time() - notif["timestamp"]
            remaining = max(0, self.TIMEOUT_SEC - elapsed)
            bar_width = int((remaining / self.TIMEOUT_SEC) * (self.WIDTH - 20))
            pygame.draw.rect(screen, (60, 60, 80),
                             (x + 10, y + self.HEIGHT - 6, self.WIDTH - 20, 4))
            pygame.draw.rect(screen, self.BORDER_COLOR,
                             (x + 10, y + self.HEIGHT - 6, bar_width, 4))
