"""
能量条 UI 模块
在游戏画面右下角显示竖条能量条，颜色随能量比例变化
"""
import logging
from typing import Tuple

import pygame

logger = logging.getLogger("client.ui.energy_bar")


class EnergyBar:
    """
    能量条 UI 组件
    右下角 40x120px 竖条，从下往上填充，颜色随比例变化
    """

    # 布局配置
    BAR_WIDTH = 40
    BAR_HEIGHT = 120
    MARGIN = 10          # 距屏幕右下角的边距
    BORDER_WIDTH = 2     # 边框宽度
    FONT_SIZE = 14       # 字体大小

    # 颜色配置
    BG_COLOR = (40, 40, 40)              # 深灰色背景
    BORDER_COLOR = (80, 80, 80)          # 灰色边框
    TEXT_COLOR = (255, 255, 255)         # 白色文字
    EMPTY_COLOR = (60, 60, 60)           # 空白部分颜色

    # 能量条颜色梯度
    COLOR_HIGH = (76, 175, 80)           # 绿色 (ratio >= 0.6)
    COLOR_MEDIUM = (255, 152, 0)         # 橙黄色 (0.3 <= ratio < 0.6)
    COLOR_LOW = (244, 67, 54)            # 红色 (ratio < 0.3)

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化能量条

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._screen_width = screen_width
        self._screen_height = screen_height

        # 能量数据
        self._current: int = 100
        self._max_energy: int = 100

        # 初始化字体
        pygame.font.init()
        self._font = pygame.font.SysFont("monospace", self.FONT_SIZE)
        if self._font is None:
            self._font = pygame.font.Font(None, self.FONT_SIZE)

        # 计算位置（右下角）
        self._x = screen_width - self.BAR_WIDTH - self.MARGIN
        self._y = screen_height - self.BAR_HEIGHT - self.MARGIN

        logger.info(f"[EnergyBar]Initialized at ({self._x}, {self._y}), "
                    f"size={self.BAR_WIDTH}x{self.BAR_HEIGHT}")

    def set_energy(self, current: int, max_energy: int):
        """
        更新能量数据

        Args:
            current: 当前能量值
            max_energy: 最大能量值
        """
        self._current = max(0, min(current, max_energy))
        self._max_energy = max(1, max_energy)

    def draw(self, screen: pygame.Surface):
        """
        绘制能量条到屏幕

        Args:
            screen: 目标屏幕表面
        """
        # 计算能量比例
        ratio = self._current / self._max_energy if self._max_energy > 0 else 0.0

        # 1. 绘制背景
        bg_rect = pygame.Rect(self._x, self._y, self.BAR_WIDTH, self.BAR_HEIGHT)
        pygame.draw.rect(screen, self.BG_COLOR, bg_rect)

        # 2. 绘制填充部分（从下往上）
        fill_height = int(self.BAR_HEIGHT * ratio)
        if fill_height > 0:
            fill_color = self._get_color(ratio)
            fill_rect = pygame.Rect(
                self._x + self.BORDER_WIDTH,
                self._y + self.BAR_HEIGHT - fill_height,
                self.BAR_WIDTH - self.BORDER_WIDTH * 2,
                fill_height
            )
            pygame.draw.rect(screen, fill_color, fill_rect)

        # 3. 绘制空白部分（填充上方）
        empty_height = self.BAR_HEIGHT - fill_height
        if empty_height > 0:
            empty_rect = pygame.Rect(
                self._x + self.BORDER_WIDTH,
                self._y,
                self.BAR_WIDTH - self.BORDER_WIDTH * 2,
                empty_height
            )
            pygame.draw.rect(screen, self.EMPTY_COLOR, empty_rect)

        # 4. 绘制边框
        pygame.draw.rect(screen, self.BORDER_COLOR, bg_rect, self.BORDER_WIDTH)

        # 5. 绘制文字（居中显示 "current/max"）
        text = f"{self._current}/{self._max_energy}"
        text_surface = self._font.render(text, True, self.TEXT_COLOR)
        text_x = self._x + (self.BAR_WIDTH - text_surface.get_width()) // 2
        text_y = self._y + (self.BAR_HEIGHT - text_surface.get_height()) // 2
        screen.blit(text_surface, (text_x, text_y))

    def _get_color(self, ratio: float) -> Tuple[int, int, int]:
        """
        根据能量比例返回颜色

        Args:
            ratio: 能量比例 (0.0 ~ 1.0)

        Returns:
            RGB 颜色元组
        """
        if ratio >= 0.6:
            return self.COLOR_HIGH
        elif ratio >= 0.3:
            return self.COLOR_MEDIUM
        else:
            return self.COLOR_LOW

    def cleanup(self):
        """清理资源"""
        logger.info("[EnergyBar]Cleaned up")
