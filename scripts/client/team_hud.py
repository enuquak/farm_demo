"""
队伍 HUD UI 组件
在战斗场景中显示队伍成员信息（HP、等级等）。
"""
import pygame
from typing import Optional
from .team_manager import TeamManager


class TeamHUD:
    """队伍 HUD 显示"""

    # 布局常量
    PANEL_WIDTH = 200
    PANEL_HEIGHT_PER_MEMBER = 28
    PANEL_PADDING = 8
    MARGIN_TOP = 10
    MARGIN_LEFT = 10

    # 颜色
    BG_COLOR = (0, 0, 0, 160)      # 半透明黑色
    BORDER_COLOR = (100, 100, 100)
    TEXT_COLOR = (255, 255, 255)
    LEADER_COLOR = (255, 215, 0)    # 金色
    HP_BAR_COLOR = (0, 200, 0)      # 绿色
    HP_BAR_BG = (80, 80, 80)
    LEAVE_BTN_COLOR = (180, 60, 60)
    LEAVE_BTN_HOVER = (220, 80, 80)

    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._visible = False
        self._leave_btn_rect: Optional[pygame.Rect] = None
        self._leave_btn_hover = False

    def set_visible(self, visible: bool) -> None:
        self._visible = visible

    def handle_event(self, event: pygame.event.Event, team_mgr: TeamManager) -> bool:
        """处理输入事件，返回 True 表示事件已消费"""
        if not self._visible:
            return False

        if event.type == pygame.MOUSEMOTION:
            if self._leave_btn_rect:
                self._leave_btn_hover = self._leave_btn_rect.collidepoint(event.pos)

        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            if self._leave_btn_rect and self._leave_btn_rect.collidepoint(event.pos):
                team_mgr.leave_team()
                return True

        return False

    def render(self, screen: pygame.Surface, team_mgr: TeamManager) -> None:
        """渲染队伍 HUD"""
        if not self._visible or not team_mgr.in_team:
            return

        members = team_mgr.members
        if not members:
            return

        panel_height = (self.PANEL_PADDING * 2 +
                        len(members) * self.PANEL_HEIGHT_PER_MEMBER +
                        30)  # 30 for header + leave button

        # Panel background
        panel_surface = pygame.Surface(
            (self.PANEL_WIDTH, panel_height), pygame.SRCALPHA)
        panel_surface.fill(self.BG_COLOR)
        screen.blit(panel_surface, (self.MARGIN_LEFT, self.MARGIN_TOP))

        # Border
        pygame.draw.rect(screen, self.BORDER_COLOR,
                         (self.MARGIN_LEFT, self.MARGIN_TOP,
                          self.PANEL_WIDTH, panel_height), 1)

        # Header
        font = pygame.font.SysFont(None, 20)
        header = font.render(f"队伍 ({len(members)}/4)", True, self.TEXT_COLOR)
        screen.blit(header, (self.MARGIN_LEFT + self.PANEL_PADDING,
                             self.MARGIN_TOP + self.PANEL_PADDING))

        # Leave button
        btn_x = self.MARGIN_LEFT + self.PANEL_WIDTH - 60
        btn_y = self.MARGIN_TOP + self.PANEL_PADDING
        btn_rect = pygame.Rect(btn_x, btn_y, 50, 18)
        btn_color = self.LEAVE_BTN_HOVER if self._leave_btn_hover else self.LEAVE_BTN_COLOR
        pygame.draw.rect(screen, btn_color, btn_rect)
        btn_text = font.render("离开", True, self.TEXT_COLOR)
        screen.blit(btn_text, (btn_x + 8, btn_y + 2))
        self._leave_btn_rect = btn_rect

        # Members
        y = self.MARGIN_TOP + self.PANEL_PADDING + 24
        small_font = pygame.font.SysFont(None, 16)
        for member in members:
            # Leader icon
            name_color = self.LEADER_COLOR if member.is_leader else self.TEXT_COLOR
            prefix = "👑 " if member.is_leader else "   "

            # Name + level
            name_text = f"{prefix}{member.role_name} Lv.{member.level}"
            name_surface = small_font.render(name_text, True, name_color)
            screen.blit(name_surface, (self.MARGIN_LEFT + self.PANEL_PADDING, y))

            # HP bar
            hp_x = self.MARGIN_LEFT + self.PANEL_PADDING + 120
            hp_y = y + 2
            hp_width = 60
            hp_height = 12
            pygame.draw.rect(screen, self.HP_BAR_BG,
                             (hp_x, hp_y, hp_width, hp_height))
            if member.max_hp > 0:
                fill_width = int(hp_width * member.current_hp / member.max_hp)
                pygame.draw.rect(screen, self.HP_BAR_COLOR,
                                 (hp_x, hp_y, fill_width, hp_height))

            y += self.PANEL_HEIGHT_PER_MEMBER
