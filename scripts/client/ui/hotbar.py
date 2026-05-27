"""
快捷栏 UI 模块
在屏幕底部固定显示 10 格快捷栏，渲染物品图标、数量和选中高亮。
"""
import pygame
from typing import Optional, List

from ..constants import (
    TILE_SIZE, HOTBAR_SLOT_COUNT, HOTBAR_SLOT_SIZE, HOTBAR_SLOT_GAP,
    HOTBAR_MARGIN_BOTTOM, HOTBAR_BG_COLOR, HOTBAR_BORDER_COLOR,
    HOTBAR_ACTIVE_COLOR, HOTBAR_TEXT_COLOR, HOTBAR_TEXT_SHADOW_COLOR,
)
from ..inventory import Inventory, Slot
from ..icon_manager import get_icon_manager


class HotbarRenderer:
    """
    快捷栏渲染器
    负责在屏幕底部绘制 10 格快捷栏 UI。
    """

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化快捷栏渲染器

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
        """
        self._screen_width = screen_width
        self._screen_height = screen_height

        # 计算快捷栏总宽度
        self._total_width = (
            HOTBAR_SLOT_COUNT * HOTBAR_SLOT_SIZE
            + (HOTBAR_SLOT_COUNT - 1) * HOTBAR_SLOT_GAP
        )

        # 计算起始 X（居中）
        self._start_x = (screen_width - self._total_width) // 2

        # 计算起始 Y（底部对齐）
        self._start_y = screen_height - HOTBAR_SLOT_SIZE - HOTBAR_MARGIN_BOTTOM

        # 字体（用于显示数量）
        self._font: Optional[pygame.font.Font] = None
        self._init_font()

    def _init_font(self):
        """初始化字体"""
        try:
            # 尝试使用系统字体
            self._font = pygame.font.SysFont("arial", 12, bold=True)
        except Exception:
            # 回退到默认字体
            self._font = pygame.font.Font(None, 14)

    def render(self, screen: pygame.Surface, inventory: Inventory):
        """
        渲染快捷栏

        Args:
            screen: PyGame 屏幕 Surface
            inventory: 玩家背包数据
        """
        icon_mgr = get_icon_manager()

        for i in range(HOTBAR_SLOT_COUNT):
            # 计算 slot 位置
            x = self._start_x + i * (HOTBAR_SLOT_SIZE + HOTBAR_SLOT_GAP)
            y = self._start_y

            # 绘制 slot 背景
            slot_rect = pygame.Rect(x, y, HOTBAR_SLOT_SIZE, HOTBAR_SLOT_SIZE)
            pygame.draw.rect(screen, HOTBAR_BG_COLOR, slot_rect)

            # 绘制 slot 边框
            is_active = (i == inventory.active_slot)
            if is_active:
                # 选中高亮：黄色边框，2px 宽
                pygame.draw.rect(screen, HOTBAR_ACTIVE_COLOR, slot_rect, 2)
            else:
                # 普通边框
                pygame.draw.rect(screen, HOTBAR_BORDER_COLOR, slot_rect, 1)

            # 获取 slot 数据
            slot = inventory.get_slot(i)
            if slot is not None:
                # 绘制物品图标
                icon = icon_mgr.get_icon(slot.item_id)
                if icon is not None:
                    screen.blit(icon, (x, y))

                # 绘制数量文字（右下角）
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
        if self._font is None:
            return

        text = str(count)
        # 黑色阴影
        shadow_surface = self._font.render(text, True, HOTBAR_TEXT_SHADOW_COLOR)
        # 白色文字
        text_surface = self._font.render(text, True, HOTBAR_TEXT_COLOR)

        # 定位到右下角
        text_x = slot_x + HOTBAR_SLOT_SIZE - text_surface.get_width() - 2
        text_y = slot_y + HOTBAR_SLOT_SIZE - text_surface.get_height() - 1

        # 绘制阴影（偏移 1px）
        screen.blit(shadow_surface, (text_x + 1, text_y + 1))
        # 绘制文字
        screen.blit(text_surface, (text_x, text_y))

    def get_slot_rect(self, slot_index: int) -> pygame.Rect:
        """
        获取指定 slot 的屏幕矩形（用于点击检测）

        Args:
            slot_index: slot 索引 (0-9)

        Returns:
            pygame.Rect
        """
        x = self._start_x + slot_index * (HOTBAR_SLOT_SIZE + HOTBAR_SLOT_GAP)
        y = self._start_y
        return pygame.Rect(x, y, HOTBAR_SLOT_SIZE, HOTBAR_SLOT_SIZE)
