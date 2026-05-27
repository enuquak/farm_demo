"""
时间 HUD 模块
在游戏画面左上角显示当前游戏时间和天数
"""
import logging
from typing import Tuple

import pygame

logger = logging.getLogger("client.ui.time_hud")


class TimeHUD:
    """
    时间 HUD 组件

    左上角 (8, 8) 显示 "AM 6:00  Day 1"
    24px 白色字体，半透明黑色背景
    """

    # 布局配置
    MARGIN_X = 8
    MARGIN_Y = 8
    PADDING_X = 8
    PADDING_Y = 4
    FONT_SIZE = 24

    # 颜色配置
    TEXT_COLOR = (255, 255, 255)         # 白色文字
    BG_COLOR = (0, 0, 0, 128)           # 半透明黑色背景

    def __init__(self):
        """初始化时间 HUD"""
        # 时钟数据
        self._day: int = 1
        self._time_slot: int = 0

        # 初始化字体
        pygame.font.init()
        self._font = pygame.font.SysFont("monospace", self.FONT_SIZE)
        if self._font is None:
            self._font = pygame.font.Font(None, self.FONT_SIZE)

        logger.info("[TimeHUD]Initialized")

    def update(self, day: int, time_slot: int):
        """
        更新时钟显示数据

        Args:
            day: 天数
            time_slot: 时间槽 (0~39)
        """
        self._day = day
        self._time_slot = time_slot

    @property
    def display_time(self) -> str:
        """
        返回格式化的时间字符串

        Returns:
            如 "AM 6:00", "PM 12:00"
        """
        raw_hour = 6 + self._time_slot // 2
        hour = raw_hour % 24
        minute = (self._time_slot % 2) * 30
        period = "AM" if hour < 12 else "PM"
        display_hour = hour if hour in (0, 12) else hour % 12
        return f"{period} {display_hour}:{minute:02d}"

    def draw(self, screen: pygame.Surface):
        """
        绘制时间 HUD 到屏幕

        Args:
            screen: 目标屏幕表面
        """
        # 构建显示文本
        text = f"{self.display_time}  Day {self._day}"

        # 渲染文字
        text_surface = self._font.render(text, True, self.TEXT_COLOR)

        # 计算背景尺寸
        bg_width = text_surface.get_width() + self.PADDING_X * 2
        bg_height = text_surface.get_height() + self.PADDING_Y * 2

        # 绘制半透明背景
        bg_surface = pygame.Surface((bg_width, bg_height), pygame.SRCALPHA)
        bg_surface.fill(self.BG_COLOR)
        screen.blit(bg_surface, (self.MARGIN_X, self.MARGIN_Y))

        # 绘制文字
        text_x = self.MARGIN_X + self.PADDING_X
        text_y = self.MARGIN_Y + self.PADDING_Y
        screen.blit(text_surface, (text_x, text_y))

    def cleanup(self):
        """清理资源"""
        logger.info("[TimeHUD]Cleaned up")
