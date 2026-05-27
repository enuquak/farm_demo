"""
精疲力尽弹窗模块
当能量不足时显示模态弹窗，暂停游戏输入
"""
import logging
from typing import Optional

import pygame

logger = logging.getLogger("client.ui.exhaustion_modal")


class ExhaustionModal:
    """
    精疲力尽弹窗
    模态覆盖层，暂停游戏输入，点击确定或按 Enter/Escape 关闭
    """

    # 弹窗配置
    MODAL_WIDTH = 320
    MODAL_HEIGHT = 180
    BUTTON_WIDTH = 100
    BUTTON_HEIGHT = 36
    BUTTON_PADDING = 20     # 按钮距底部的距离

    # 颜色配置
    OVERLAY_COLOR = (0, 0, 0, 128)       # 半透明黑色遮罩
    MODAL_BG_COLOR = (50, 50, 50)        # 弹窗背景
    MODAL_BORDER_COLOR = (100, 100, 100) # 弹窗边框
    TITLE_COLOR = (244, 67, 54)          # 红色标题
    TEXT_COLOR = (200, 200, 200)         # 灰白色正文
    BUTTON_COLOR = (70, 70, 70)          # 按钮背景
    BUTTON_HOVER_COLOR = (90, 90, 90)    # 按钮悬停颜色
    BUTTON_TEXT_COLOR = (255, 255, 255)  # 按钮文字颜色
    BUTTON_BORDER_COLOR = (120, 120, 120)  # 按钮边框

    # 字体配置
    TITLE_FONT_SIZE = 24
    TEXT_FONT_SIZE = 16
    BUTTON_FONT_SIZE = 16

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化弹窗

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._visible: bool = False

        # 初始化字体
        pygame.font.init()
        self._title_font = pygame.font.SysFont("simhei", self.TITLE_FONT_SIZE)
        self._text_font = pygame.font.SysFont("simhei", self.TEXT_FONT_SIZE)
        self._button_font = pygame.font.SysFont("simhei", self.BUTTON_FONT_SIZE)

        # 计算弹窗位置（居中）
        self._modal_x = (screen_width - self.MODAL_WIDTH) // 2
        self._modal_y = (screen_height - self.MODAL_HEIGHT) // 2

        # 计算按钮位置
        self._button_x = self._modal_x + (self.MODAL_WIDTH - self.BUTTON_WIDTH) // 2
        self._button_y = self._modal_y + self.MODAL_HEIGHT - self.BUTTON_HEIGHT - self.BUTTON_PADDING

        # 按钮矩形
        self._button_rect = pygame.Rect(
            self._button_x, self._button_y,
            self.BUTTON_WIDTH, self.BUTTON_HEIGHT
        )

        logger.info(f"[ExhaustionModal]Initialized at ({self._modal_x}, {self._modal_y})")

    @property
    def is_visible(self) -> bool:
        """弹窗是否可见"""
        return self._visible

    def show(self):
        """显示弹窗"""
        self._visible = True
        logger.info("[ExhaustionModal]Shown")

    def hide(self):
        """隐藏弹窗"""
        self._visible = False
        logger.info("[ExhaustionModal]Hidden")

    def handle_event(self, event: pygame.event.Event) -> bool:
        """
        处理事件

        Args:
            event: PyGame 事件

        Returns:
            True 表示事件被消费（弹窗关闭），False 表示未处理
        """
        if not self._visible:
            return False

        if event.type == pygame.KEYDOWN:
            if event.key in (pygame.K_RETURN, pygame.K_ESCAPE):
                self.hide()
                return True

        if event.type == pygame.MOUSEBUTTONDOWN:
            if event.button == 1:  # 左键
                if self._button_rect.collidepoint(event.pos):
                    self.hide()
                    return True

        return False

    def draw(self, screen: pygame.Surface):
        """
        绘制弹窗到屏幕

        Args:
            screen: 目标屏幕表面
        """
        if not self._visible:
            return

        # 1. 绘制半透明遮罩
        overlay = pygame.Surface((self._screen_width, self._screen_height), pygame.SRCALPHA)
        overlay.fill(self.OVERLAY_COLOR)
        screen.blit(overlay, (0, 0))

        # 2. 绘制弹窗背景
        modal_rect = pygame.Rect(self._modal_x, self._modal_y, self.MODAL_WIDTH, self.MODAL_HEIGHT)
        pygame.draw.rect(screen, self.MODAL_BG_COLOR, modal_rect)
        pygame.draw.rect(screen, self.MODAL_BORDER_COLOR, modal_rect, 2)

        # 3. 绘制标题
        title_surface = self._title_font.render("精疲力尽！", True, self.TITLE_COLOR)
        title_x = self._modal_x + (self.MODAL_WIDTH - title_surface.get_width()) // 2
        title_y = self._modal_y + 30
        screen.blit(title_surface, (title_x, title_y))

        # 4. 绘制正文
        text_surface = self._text_font.render("能量不足，无法执行此操作", True, self.TEXT_COLOR)
        text_x = self._modal_x + (self.MODAL_WIDTH - text_surface.get_width()) // 2
        text_y = self._modal_y + 70
        screen.blit(text_surface, (text_x, text_y))

        # 5. 绘制按钮
        mouse_pos = pygame.mouse.get_pos()
        is_hover = self._button_rect.collidepoint(mouse_pos)
        button_color = self.BUTTON_HOVER_COLOR if is_hover else self.BUTTON_COLOR

        pygame.draw.rect(screen, button_color, self._button_rect)
        pygame.draw.rect(screen, self.BUTTON_BORDER_COLOR, self._button_rect, 1)

        button_text = self._button_font.render("确定", True, self.BUTTON_TEXT_COLOR)
        button_text_x = self._button_x + (self.BUTTON_WIDTH - button_text.get_width()) // 2
        button_text_y = self._button_y + (self.BUTTON_HEIGHT - button_text.get_height()) // 2
        screen.blit(button_text, (button_text_x, button_text_y))

    def cleanup(self):
        """清理资源"""
        logger.info("[ExhaustionModal]Cleaned up")
