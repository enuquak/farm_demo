"""
背包面板 UI 模块
显示完整 30 格背包面板（10x3 网格），支持关闭按钮和点击检测。
"""
import pygame
from typing import Optional, Tuple

from ..constants import (
    TILE_SIZE, PANEL_SLOT_SIZE, PANEL_SLOT_GAP, PANEL_COLS, PANEL_ROWS,
    PANEL_BG_COLOR, PANEL_BORDER_COLOR, PANEL_TITLE_COLOR,
    PANEL_CLOSE_BTN_COLOR, PANEL_OVERLAY_ALPHA,
    HOTBAR_BG_COLOR, HOTBAR_BORDER_COLOR, HOTBAR_ACTIVE_COLOR,
    HOTBAR_TEXT_COLOR, HOTBAR_TEXT_SHADOW_COLOR,
)
from ..inventory import Inventory, Slot, TOTAL_SLOTS
from ..icon_manager import get_icon_manager


class InventoryPanel:
    """
    背包面板
    显示完整 30 格背包（10x3 网格），覆盖在游戏画面上。
    支持关闭按钮点击。
    """

    # 面板内边距
    PADDING = 12
    # 标题高度
    TITLE_HEIGHT = 28
    # 关闭按钮大小
    CLOSE_BTN_SIZE = 20

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化背包面板

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._screen_width = screen_width
        self._screen_height = screen_height

        # 计算面板内容区域尺寸
        content_width = (
            PANEL_COLS * PANEL_SLOT_SIZE
            + (PANEL_COLS - 1) * PANEL_SLOT_GAP
        )
        content_height = (
            PANEL_ROWS * PANEL_SLOT_SIZE
            + (PANEL_ROWS - 1) * PANEL_SLOT_GAP
        )

        # 面板总尺寸（含内边距和标题）
        self._panel_width = content_width + self.PADDING * 2
        self._panel_height = content_height + self.PADDING * 2 + self.TITLE_HEIGHT

        # 面板位置（居中）
        self._panel_x = (screen_width - self._panel_width) // 2
        self._panel_y = (screen_height - self._panel_height) // 2

        # 内容区域起始位置
        self._content_x = self._panel_x + self.PADDING
        self._content_y = self._panel_y + self.PADDING + self.TITLE_HEIGHT

        # 关闭按钮位置（面板右上角）
        self._close_btn_x = self._panel_x + self._panel_width - self.PADDING - self.CLOSE_BTN_SIZE
        self._close_btn_y = self._panel_y + self.PADDING // 2 + 4
        self._close_btn_rect = pygame.Rect(
            self._close_btn_x, self._close_btn_y,
            self.CLOSE_BTN_SIZE, self.CLOSE_BTN_SIZE
        )

        # 字体
        self._title_font: Optional[pygame.font.Font] = None
        self._count_font: Optional[pygame.font.Font] = None
        self._close_font: Optional[pygame.font.Font] = None
        self._init_fonts()

        # 覆盖层 Surface（半透明黑色）
        self._overlay = pygame.Surface((screen_width, screen_height), pygame.SRCALPHA)
        self._overlay.fill((0, 0, 0, PANEL_OVERLAY_ALPHA))

    def _init_fonts(self):
        """初始化字体"""
        try:
            self._title_font = pygame.font.SysFont("simhei", 18, bold=True)
            self._count_font = pygame.font.SysFont("arial", 12, bold=True)
            self._close_font = pygame.font.SysFont("arial", 14, bold=True)
        except Exception:
            self._title_font = pygame.font.Font(None, 22)
            self._count_font = pygame.font.Font(None, 14)
            self._close_font = pygame.font.Font(None, 18)

    def render(self, screen: pygame.Surface, inventory: Inventory):
        """
        渲染背包面板

        Args:
            screen: PyGame 屏幕 Surface
            inventory: 玩家背包数据
        """
        # 绘制半透明覆盖层
        screen.blit(self._overlay, (0, 0))

        # 绘制面板背景
        panel_rect = pygame.Rect(
            self._panel_x, self._panel_y,
            self._panel_width, self._panel_height
        )
        pygame.draw.rect(screen, PANEL_BG_COLOR, panel_rect)
        pygame.draw.rect(screen, PANEL_BORDER_COLOR, panel_rect, 2)

        # 绘制标题
        self._render_title(screen)

        # 绘制关闭按钮
        self._render_close_button(screen)

        # 绘制所有 slot
        self._render_slots(screen, inventory)

    def _render_title(self, screen: pygame.Surface):
        """
        绘制面板标题

        Args:
            screen: PyGame 屏幕 Surface
        """
        if self._title_font is None:
            return

        title_surface = self._title_font.render("背包", True, PANEL_TITLE_COLOR)
        title_x = self._panel_x + self.PADDING
        title_y = self._panel_y + self.PADDING // 2 + 4
        screen.blit(title_surface, (title_x, title_y))

    def _render_close_button(self, screen: pygame.Surface):
        """
        绘制关闭按钮 [x]

        Args:
            screen: PyGame 屏幕 Surface
        """
        # 按钮背景
        pygame.draw.rect(screen, PANEL_CLOSE_BTN_COLOR, self._close_btn_rect)

        # 按钮文字 "x"
        if self._close_font is not None:
            x_text = self._close_font.render("x", True, (255, 255, 255))
            text_x = self._close_btn_x + (self.CLOSE_BTN_SIZE - x_text.get_width()) // 2
            text_y = self._close_btn_y + (self.CLOSE_BTN_SIZE - x_text.get_height()) // 2
            screen.blit(x_text, (text_x, text_y))

    def _render_slots(self, screen: pygame.Surface, inventory: Inventory):
        """
        绘制所有 30 格 slot

        Args:
            screen: PyGame 屏幕 Surface
            inventory: 玩家背包数据
        """
        icon_mgr = get_icon_manager()

        for row in range(PANEL_ROWS):
            for col in range(PANEL_COLS):
                slot_index = row * PANEL_COLS + col

                # 计算 slot 位置
                x = self._content_x + col * (PANEL_SLOT_SIZE + PANEL_SLOT_GAP)
                y = self._content_y + row * (PANEL_SLOT_SIZE + PANEL_SLOT_GAP)

                # 绘制 slot 背景
                slot_rect = pygame.Rect(x, y, PANEL_SLOT_SIZE, PANEL_SLOT_SIZE)
                pygame.draw.rect(screen, HOTBAR_BG_COLOR, slot_rect)

                # 第一行（快捷栏）显示选中高亮
                is_active = (row == 0 and slot_index == inventory.active_slot)
                if is_active:
                    pygame.draw.rect(screen, HOTBAR_ACTIVE_COLOR, slot_rect, 2)
                else:
                    pygame.draw.rect(screen, HOTBAR_BORDER_COLOR, slot_rect, 1)

                # 绘制物品图标和数量
                slot = inventory.get_slot(slot_index)
                if slot is not None:
                    icon = icon_mgr.get_icon(slot.item_id)
                    if icon is not None:
                        screen.blit(icon, (x, y))

                    if slot.count > 1:
                        self._render_count(screen, x, y, slot.count)

    def _render_count(self, screen: pygame.Surface, slot_x: int, slot_y: int, count: int):
        """
        在 slot 右下角渲染数量文字

        Args:
            screen: PyGame 屏幕 Surface
            slot_x: slot 左上角 X
            slot_y: slot 左上角 Y
            count: 物品数量
        """
        if self._count_font is None:
            return

        text = str(count)
        shadow_surface = self._count_font.render(text, True, HOTBAR_TEXT_SHADOW_COLOR)
        text_surface = self._count_font.render(text, True, HOTBAR_TEXT_COLOR)

        text_x = slot_x + PANEL_SLOT_SIZE - text_surface.get_width() - 2
        text_y = slot_y + PANEL_SLOT_SIZE - text_surface.get_height() - 1

        screen.blit(shadow_surface, (text_x + 1, text_y + 1))
        screen.blit(text_surface, (text_x, text_y))

    def is_close_button_clicked(self, mouse_pos: Tuple[int, int]) -> bool:
        """
        检测鼠标点击是否在关闭按钮上

        Args:
            mouse_pos: 鼠标屏幕坐标 (x, y)

        Returns:
            True 表示点击了关闭按钮
        """
        return self._close_btn_rect.collidepoint(mouse_pos)

    def is_panel_area(self, mouse_pos: Tuple[int, int]) -> bool:
        """
        检测鼠标位置是否在面板区域内

        Args:
            mouse_pos: 鼠标屏幕坐标 (x, y)

        Returns:
            True 表示在面板区域内
        """
        panel_rect = pygame.Rect(
            self._panel_x, self._panel_y,
            self._panel_width, self._panel_height
        )
        return panel_rect.collidepoint(mouse_pos)
