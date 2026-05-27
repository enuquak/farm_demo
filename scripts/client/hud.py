"""
游戏内 HUD 模块
显示玩家信息覆盖层（角色名、坐标、场景名）
"""
import logging
from typing import Tuple

import pygame

logger = logging.getLogger("client.hud")


class HUD:
    """
    游戏内 HUD（Head-Up Display）
    在游戏画面上叠加显示玩家信息
    """

    # HUD 配置
    MARGIN = 10          # 边距
    LINE_HEIGHT = 20     # 行高
    FONT_SIZE = 16       # 字体大小
    BG_ALPHA = 128       # 背景透明度（0-255）
    TEXT_COLOR = (255, 255, 255)       # 白色文字
    BG_COLOR = (0, 0, 0)              # 黑色背景
    BORDER_COLOR = (100, 100, 100)    # 灰色边框

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化 HUD

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._screen_width = screen_width
        self._screen_height = screen_height

        # 初始化字体
        pygame.font.init()
        self._font = pygame.font.SysFont("monospace", self.FONT_SIZE)
        if self._font is None:
            self._font = pygame.font.Font(None, self.FONT_SIZE)

        # HUD 表面（带 alpha 通道）
        self._hud_surface: pygame.Surface = None
        self._needs_update = True

        # 缓存的信息
        self._role_name = ""
        self._pos_x = 0.0
        self._pos_y = 0.0
        self._scene_id = ""

        logger.info(f"[HUD]Initialized: {screen_width}x{screen_height}")

    def set_info(self, role_name: str, pos_x: float, pos_y: float, scene_id: str):
        """
        更新 HUD 显示信息

        Args:
            role_name: 角色名
            pos_x: 当前 X 坐标（tile 坐标）
            pos_y: 当前 Y 坐标（tile 坐标）
            scene_id: 场景 ID
        """
        if (role_name != self._role_name or
            abs(pos_x - self._pos_x) > 0.1 or
            abs(pos_y - self._pos_y) > 0.1 or
            scene_id != self._scene_id):
            self._role_name = role_name
            self._pos_x = pos_x
            self._pos_y = pos_y
            self._scene_id = scene_id
            self._needs_update = True

    def draw(self, screen: pygame.Surface):
        """
        绘制 HUD 到屏幕

        Args:
            screen: 目标屏幕表面
        """
        if self._needs_update:
            self._render_hud()
            self._needs_update = False

        if self._hud_surface is not None:
            screen.blit(self._hud_surface, (self.MARGIN, self.MARGIN))

    def _render_hud(self):
        """渲染 HUD 表面"""
        lines = [
            f"Name: {self._role_name}",
            f"Pos:  ({self._pos_x:.1f}, {self._pos_y:.1f})",
            f"Scene: {self._scene_id}",
        ]

        # 计算 HUD 尺寸
        max_width = 0
        for line in lines:
            text_surface = self._font.render(line, True, self.TEXT_COLOR)
            max_width = max(max_width, text_surface.get_width())

        hud_width = max_width + self.MARGIN * 2
        hud_height = len(lines) * self.LINE_HEIGHT + self.MARGIN * 2

        # 创建带 alpha 的表面
        self._hud_surface = pygame.Surface((hud_width, hud_height), pygame.SRCALPHA)

        # 绘制半透明背景
        bg_rect = pygame.Rect(0, 0, hud_width, hud_height)
        pygame.draw.rect(self._hud_surface, (*self.BG_COLOR, self.BG_ALPHA), bg_rect)
        pygame.draw.rect(self._hud_surface, (*self.BORDER_COLOR, self.BG_ALPHA), bg_rect, 1)

        # 绘制文字
        for i, line in enumerate(lines):
            text_surface = self._font.render(line, True, self.TEXT_COLOR)
            text_x = self.MARGIN
            text_y = self.MARGIN + i * self.LINE_HEIGHT
            self._hud_surface.blit(text_surface, (text_x, text_y))

    def cleanup(self):
        """清理资源"""
        pygame.font.quit()
        logger.info("[HUD]Cleaned up")
