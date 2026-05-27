"""
物品图标管理器模块
负责为每种物品生成 8x8 像素图标 Surface，并缓存供 UI 渲染使用。
"""
import pygame
from typing import Dict, Optional

from .constants import ITEM_ICON_PALETTE, TILE_SIZE

# 图标原始尺寸
ICON_SIZE = 8

# 缩放后尺寸（与 TILE_SIZE 一致）
ICON_SCALE_SIZE = TILE_SIZE


class IconManager:
    """
    物品图标管理器
    在初始化时为每种物品预生成 8x8 像素图标 Surface 并缩放到 32x32。
    使用缓存机制，多个 slot 共享同一个 Surface 实例。
    """

    def __init__(self):
        """初始化图标管理器，预生成所有物品图标"""
        # 图标缓存：item_id -> pygame.Surface (32x32)
        self._icon_cache: Dict[int, pygame.Surface] = {}
        # 预生成所有图标
        self._generate_all_icons()

    def _generate_all_icons(self):
        """为所有定义了图标数据的物品生成 Surface"""
        for item_id, icon_data in ITEM_ICON_PALETTE.items():
            self._generate_icon(item_id, icon_data)

    def _generate_icon(self, item_id: int, icon_data: dict):
        """
        生成指定物品的图标 Surface

        Args:
            item_id: 物品 ID
            icon_data: 图标数据字典，包含 colors 和 pixels
        """
        colors = icon_data.get("colors", {})
        pixels = icon_data.get("pixels", [])

        # 创建 8x8 Surface（带 alpha 通道）
        surface = pygame.Surface((ICON_SIZE, ICON_SIZE), pygame.SRCALPHA)
        surface.fill((0, 0, 0, 0))  # 透明背景

        # 绘制像素
        for y in range(min(len(pixels), ICON_SIZE)):
            row = pixels[y]
            for x in range(min(len(row), ICON_SIZE)):
                pixel_value = row[x]
                if pixel_value == 0:
                    continue  # 透明像素
                color = colors.get(pixel_value, (255, 0, 255))  # 品红兜底
                surface.set_at((x, y), (*color, 255))

        # 缩放到 32x32，使用 NEAREST 保持像素风格
        scaled = pygame.transform.scale(surface, (ICON_SCALE_SIZE, ICON_SCALE_SIZE))

        # 缓存
        self._icon_cache[item_id] = scaled

    def get_icon(self, item_id: int) -> Optional[pygame.Surface]:
        """
        获取指定物品的图标 Surface

        Args:
            item_id: 物品 ID

        Returns:
            32x32 pygame.Surface，未定义图标时返回 None
        """
        return self._icon_cache.get(item_id)

    def has_icon(self, item_id: int) -> bool:
        """
        检查指定物品是否有图标

        Args:
            item_id: 物品 ID

        Returns:
            是否有图标
        """
        return item_id in self._icon_cache


# 全局单例
_icon_manager: Optional[IconManager] = None


def get_icon_manager() -> IconManager:
    """
    获取全局 IconManager 实例（惰性初始化）

    Returns:
        IconManager 实例
    """
    global _icon_manager
    if _icon_manager is None:
        _icon_manager = IconManager()
    return _icon_manager
