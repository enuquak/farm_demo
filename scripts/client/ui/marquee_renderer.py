"""
Marquee (滚动公告) 渲染模块
负责 Marquee 滚动文字的视觉渲染，显示在屏幕顶部
"""
import logging
from typing import Optional

import pygame

logger = logging.getLogger("client.ui.marquee_renderer")


class MarqueeRenderer:
    """
    Marquee 滚动公告渲染器
    处理滚动文字的视觉渲染，支持水平滚动效果
    """

    # 布局配置
    MARQUEE_HEIGHT = 30
    MARGIN_TOP = 8
    PADDING_X = 15

    # 字体配置
    FONT_SIZE = 18

    # 颜色配置
    BG_COLOR = (0, 0, 0, 150)
    TEXT_COLOR = (255, 255, 0)

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化 Marquee 渲染器

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._screen_width = screen_width
        self._screen_height = screen_height

        # 初始化字体（支持中文）
        pygame.font.init()
        self._font = pygame.font.SysFont("simhei", self.FONT_SIZE)

        logger.info(
            f"[MarqueeRenderer]Initialized, screen={screen_width}x{screen_height}"
        )

    def get_text_width(self, text: str) -> int:
        """
        获取文本渲染后的像素宽度

        Args:
            text: 要测量的文本

        Returns:
            文本的像素宽度
        """
        return self._font.size(text)[0]

    def render(self, screen: pygame.Surface, text: str, x: int) -> None:
        """
        渲染滚动文字到屏幕指定位置

        Args:
            screen: 目标屏幕表面
            text: 要显示的文本
            x: 文本的起始 x 坐标
        """
        text_width = self.get_text_width(text)
        bg_width = text_width + self.PADDING_X * 2

        # 创建带透明通道的背景表面
        bg_surface = pygame.Surface((bg_width, self.MARQUEE_HEIGHT), pygame.SRCALPHA)

        # 绘制半透明背景
        bg_rect = pygame.Rect(0, 0, bg_width, self.MARQUEE_HEIGHT)
        pygame.draw.rect(bg_surface, self.BG_COLOR, bg_rect)

        # 渲染文字（垂直居中）
        text_surface = self._font.render(text, True, self.TEXT_COLOR)
        text_y = (self.MARQUEE_HEIGHT - text_surface.get_height()) // 2
        bg_surface.blit(text_surface, (self.PADDING_X, text_y))

        # 绘制到屏幕
        bg_x = x - self.PADDING_X
        screen.blit(bg_surface, (bg_x, self.MARGIN_TOP))

    def cleanup(self) -> None:
        """清理资源"""
        logger.info("[MarqueeRenderer]Cleaned up")
