"""
掉落物渲染器模块
在世界中渲染掉落物实体，支持 sin 波浮动动画。
处理 DropItemSync 消息（spawn、remove、pickup）。
"""
import math
import time
import logging
from typing import Dict, Optional, Any

import pygame

from .constants import TILE_SIZE, ZOOM_FACTOR
from .icon_manager import get_icon_manager

logger = logging.getLogger("client.drop_item_renderer")

# 掉落物浮动动画参数
FLOAT_AMPLITUDE = 2.0   # 浮动幅度（像素）
FLOAT_SPEED = 3.0       # 浮动速度（弧度/秒）

# 掉落物渲染尺寸（比物品图标稍小）
DROP_ICON_SCALE = 0.75


class DropItem:
    """掉落物实体"""

    __slots__ = ('drop_id', 'item_id', 'count', 'x', 'y', 'spawn_time')

    def __init__(self, drop_id: int, item_id: int, count: int, x: float, y: float):
        self.drop_id = drop_id
        self.item_id = item_id
        self.count = count
        self.x = x  # 世界坐标（像素）
        self.y = y  # 世界坐标（像素）
        self.spawn_time = time.time()


class DropItemRenderer:
    """
    掉落物渲染器
    管理世界中的掉落物实体，处理同步消息并渲染到屏幕。
    """

    def __init__(self):
        """初始化掉落物渲染器"""
        # 掉落物字典：drop_id -> DropItem
        self._drops: Dict[int, DropItem] = {}

        # 缩放后的图标缓存：item_id -> pygame.Surface
        self._scaled_icon_cache: Dict[int, pygame.Surface] = {}

    def handle_drop_sync(self, drop_sync) -> None:
        """
        处理 DropItemSync 消息

        Args:
            drop_sync: DropItemSync protobuf 消息
        """
        drop_id = drop_sync.drop_id
        action = drop_sync.action

        if action == 0:  # spawn
            self._drops[drop_id] = DropItem(
                drop_id=drop_id,
                item_id=drop_sync.item_id,
                count=drop_sync.count,
                x=drop_sync.x,
                y=drop_sync.y,
            )
            logger.debug(f"[DropRenderer]Spawned drop_id={drop_id}, item={drop_sync.item_id}, "
                        f"pos=({drop_sync.x:.1f},{drop_sync.y:.1f})")

        elif action == 1:  # remove (expired)
            if drop_id in self._drops:
                del self._drops[drop_id]
                logger.debug(f"[DropRenderer]Removed drop_id={drop_id} (expired)")

        elif action == 2:  # pickup
            if drop_id in self._drops:
                del self._drops[drop_id]
                logger.debug(f"[DropRenderer]Removed drop_id={drop_id} (pickup)")

    def render(self, screen: pygame.Surface, camera_x: float, camera_y: float) -> None:
        """
        渲染所有掉落物

        Args:
            screen: PyGame 屏幕 Surface
            camera_x: 相机 X 偏移（世界像素）
            camera_y: 相机 Y 偏移（世界像素）
        """
        icon_mgr = get_icon_manager()
        current_time = time.time()

        for drop in self._drops.values():
            # 获取物品图标
            icon = self._get_scaled_icon(drop.item_id, icon_mgr)
            if icon is None:
                continue

            # 计算屏幕坐标
            screen_x = (drop.x - camera_x) * ZOOM_FACTOR
            screen_y = (drop.y - camera_y) * ZOOM_FACTOR

            # 应用 sin 波浮动动画
            elapsed = current_time - drop.spawn_time
            float_offset = math.sin(elapsed * FLOAT_SPEED) * FLOAT_AMPLITUDE
            screen_y += float_offset

            # 居中绘制图标
            icon_rect = icon.get_rect()
            icon_rect.centerx = int(screen_x)
            icon_rect.centery = int(screen_y)

            screen.blit(icon, icon_rect)

    def _get_scaled_icon(self, item_id: int, icon_mgr) -> Optional[pygame.Surface]:
        """
        获取缩放后的掉落物图标（带缓存）

        Args:
            item_id: 物品 ID
            icon_mgr: 图标管理器

        Returns:
            缩放后的 pygame.Surface，或 None
        """
        if item_id in self._scaled_icon_cache:
            return self._scaled_icon_cache[item_id]

        original = icon_mgr.get_icon(item_id)
        if original is None:
            return None

        # 缩放到 75% 大小
        size = int(TILE_SIZE * DROP_ICON_SCALE)
        scaled = pygame.transform.scale(original, (size, size))
        self._scaled_icon_cache[item_id] = scaled
        return scaled

    def clear(self) -> None:
        """清空所有掉落物（场景切换时使用）"""
        self._drops.clear()
        logger.info("[DropRenderer]Cleared all drop items")

    @property
    def drop_count(self) -> int:
        """当前掉落物数量"""
        return len(self._drops)

    def get_drop(self, drop_id: int) -> Optional[DropItem]:
        """
        获取指定掉落物

        Args:
            drop_id: 掉落物 ID

        Returns:
            DropItem 实例，或 None
        """
        return self._drops.get(drop_id)
