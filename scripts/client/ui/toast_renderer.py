"""
Toast 通知渲染模块
负责 Toast 通知的视觉渲染，包含缩放动画效果
"""
import logging
from typing import Tuple

import pygame

logger = logging.getLogger("client.ui.toast_renderer")


class ToastRenderer:
    """
    Toast 通知渲染器
    处理 Toast 通知的视觉渲染，支持出现/消失缩放动画
    """

    # 布局配置
    TOAST_WIDTH = 300
    TOAST_HEIGHT = 60
    MARGIN_TOP = 50
    PADDING_X = 15
    PADDING_Y = 10

    # 字体配置
    TITLE_FONT_SIZE = 20
    CONTENT_FONT_SIZE = 14

    # 颜色配置
    BG_COLOR = (0, 0, 0, 200)
    BORDER_COLOR = (100, 100, 100, 200)
    TITLE_COLOR = (255, 255, 255)
    CONTENT_COLOR = (200, 200, 200)

    # 动画配置
    APPEAR_DURATION = 0.3
    DISAPPEAR_DURATION = 0.2
    SCALE_OVERSHOOT = 1.1

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化 Toast 渲染器

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._screen_width = screen_width
        self._screen_height = screen_height

        # 初始化字体（支持中文）
        pygame.font.init()
        self._title_font = pygame.font.SysFont("simhei", self.TITLE_FONT_SIZE)
        self._content_font = pygame.font.SysFont("simhei", self.CONTENT_FONT_SIZE)

        # 计算居中位置
        self._center_x = (screen_width - self.TOAST_WIDTH) // 2
        self._y = self.MARGIN_TOP

        logger.info(
            f"[ToastRenderer]Initialized at ({self._center_x}, {self._y}), "
            f"size={self.TOAST_WIDTH}x{self.TOAST_HEIGHT}"
        )

    def render(
        self,
        screen: pygame.Surface,
        title: str,
        content: str,
        progress: float,
        state: str,
    ) -> None:
        """
        渲染 Toast 通知到屏幕

        Args:
            screen: 目标屏幕表面
            title: 标题文本
            content: 内容文本
            progress: 动画进度 (0.0 ~ 1.0)
            state: 动画状态 ("appearing", "showing", "disappearing")
        """
        # 计算缩放比例
        scale = self._calculate_scale(progress, state)

        # 如果缩放为 0，不渲染
        if scale <= 0:
            return

        # 计算缩放后的尺寸
        scaled_width = int(self.TOAST_WIDTH * scale)
        scaled_height = int(self.TOAST_HEIGHT * scale)

        # 计算缩放后的位置（保持居中）
        x = self._center_x + (self.TOAST_WIDTH - scaled_width) // 2
        y = self._y + (self.TOAST_HEIGHT - scaled_height) // 2

        # 创建临时表面用于缩放渲染
        toast_surface = pygame.Surface(
            (self.TOAST_WIDTH, self.TOAST_HEIGHT), pygame.SRCALPHA
        )

        # 绘制背景
        bg_rect = pygame.Rect(0, 0, self.TOAST_WIDTH, self.TOAST_HEIGHT)
        pygame.draw.rect(toast_surface, self.BG_COLOR, bg_rect)

        # 绘制边框
        pygame.draw.rect(toast_surface, self.BORDER_COLOR, bg_rect, 2)

        # 渲染文字
        self._render_text(toast_surface, title, content, scale)

        # 缩放并绘制到屏幕
        if scale != 1.0:
            scaled_surface = pygame.transform.scale(
                toast_surface, (scaled_width, scaled_height)
            )
            screen.blit(scaled_surface, (x, y))
        else:
            screen.blit(toast_surface, (x, y))

    def _calculate_scale(self, progress: float, state: str) -> float:
        """
        根据动画状态和进度计算缩放比例

        Args:
            progress: 动画进度 (0.0 ~ 1.0)
            state: 动画状态 ("appearing", "showing", "disappearing")

        Returns:
            缩放比例 (0.0 ~ SCALE_OVERSHOOT)
        """
        if state == "appearing":
            if progress < 0.7:
                # 0.0-0.7: 从 0 缩放到 SCALE_OVERSHOOT
                return (progress / 0.7) * self.SCALE_OVERSHOOT
            else:
                # 0.7-1.0: 从 SCALE_OVERSHOOT 缩放回 1.0
                overshoot_progress = (progress - 0.7) / 0.3
                return self.SCALE_OVERSHOOT - overshoot_progress * (
                    self.SCALE_OVERSHOOT - 1.0
                )
        elif state == "showing":
            return 1.0
        elif state == "disappearing":
            # 从 1.0 缩放到 0
            return 1.0 - progress
        else:
            return 1.0

    def _render_text(
        self, surface: pygame.Surface, title: str, content: str, scale: float
    ) -> None:
        """
        渲染标题和内容文本

        Args:
            surface: 目标表面
            title: 标题文本
            content: 内容文本
            scale: 当前缩放比例
        """
        # 渲染标题
        title_surface = self._title_font.render(title, True, self.TITLE_COLOR)
        title_x = self.PADDING_X
        title_y = self.PADDING_Y
        surface.blit(title_surface, (title_x, title_y))

        # 渲染内容（如果有）
        if content:
            content_surface = self._content_font.render(
                content, True, self.CONTENT_COLOR
            )
            content_x = self.PADDING_X
            content_y = title_y + title_surface.get_height() + 4
            surface.blit(content_surface, (content_x, content_y))

    def cleanup(self) -> None:
        """清理资源"""
        logger.info("[ToastRenderer]Cleaned up")
