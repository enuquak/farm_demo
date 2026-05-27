"""
TileMap 渲染器模块
使用 PyGame 渲染网格地图，支持视口裁剪、2.5D 效果和双层渲染
"""
import pygame
from typing import Tuple

from .constants import (
    TILE_SIZE, GroundType, ObjectType, GROUND_PROPERTIES,
    HIGHLIGHT_ALPHA, SHADOW_ALPHA
)
from .tile_map import TileMap, tile_to_pixel
from .camera import Camera
from .object_sprite_manager import get_sprite_manager


class TileRenderer:
    """
    TileMap 渲染器
    将 TileMap 数据渲染到 PyGame 屏幕
    支持双层渲染：先地面层，再地物层
    """

    def __init__(self, screen: pygame.Surface):
        """
        初始化渲染器

        Args:
            screen: PyGame 屏幕表面
        """
        self.screen = screen
        # 获取精灵管理器
        self._sprite_manager = get_sprite_manager()

    def render(self, tile_map: TileMap, camera: Camera):
        """
        渲染 TileMap 到屏幕（双层渲染）

        Args:
            tile_map: TileMap 数据
            camera: 相机对象
        """
        # 获取可见 tile 范围
        start_x, start_y, end_x, end_y = camera.get_visible_tile_range()

        # 限制到地图范围
        end_x = min(end_x, tile_map.width)
        end_y = min(end_y, tile_map.height)

        # 第一遍：渲染地面层
        for tile_y in range(start_y, end_y):
            for tile_x in range(start_x, end_x):
                ground_type = tile_map.get_ground(tile_x, tile_y)
                if ground_type is None:
                    continue

                # 计算屏幕坐标
                world_x, world_y = tile_to_pixel(tile_x, tile_y)
                screen_x, screen_y = camera.world_to_screen(world_x, world_y)

                # 绘制地面 tile
                self._draw_ground_tile(
                    int(screen_x), int(screen_y),
                    ground_type
                )

        # 第二遍：渲染地物层
        for tile_y in range(start_y, end_y):
            for tile_x in range(start_x, end_x):
                obj_type = tile_map.get_object(tile_x, tile_y)
                if obj_type is None or obj_type == ObjectType.NONE:
                    continue

                # 计算屏幕坐标
                world_x, world_y = tile_to_pixel(tile_x, tile_y)
                screen_x, screen_y = camera.world_to_screen(world_x, world_y)

                # 绘制地物精灵
                self._draw_object_sprite(
                    int(screen_x), int(screen_y),
                    obj_type
                )

    def _draw_ground_tile(self, screen_x: int, screen_y: int, ground_type: GroundType):
        """
        绘制单个地面 tile（含 2.5D 高光/阴影效果）

        Args:
            screen_x: 屏幕 X 坐标
            screen_y: 屏幕 Y 坐标
            ground_type: 地面类型
        """
        props = GROUND_PROPERTIES.get(ground_type)
        if not props:
            return

        base_color = props["color"]
        rect = pygame.Rect(screen_x, screen_y, TILE_SIZE, TILE_SIZE)

        # 绘制基础颜色
        pygame.draw.rect(self.screen, base_color, rect)

        # 2.5D 高光效果（顶部和左侧）
        highlight_color = self._adjust_color(base_color, HIGHLIGHT_ALPHA)
        # 顶部高光条
        pygame.draw.line(
            self.screen, highlight_color,
            (screen_x, screen_y),
            (screen_x + TILE_SIZE - 1, screen_y),
            2
        )
        # 左侧高光条
        pygame.draw.line(
            self.screen, highlight_color,
            (screen_x, screen_y),
            (screen_x, screen_y + TILE_SIZE - 1),
            2
        )

        # 2.5D 阴影效果（底部和右侧）
        shadow_color = self._adjust_color(base_color, -SHADOW_ALPHA)
        # 底部阴影条
        pygame.draw.line(
            self.screen, shadow_color,
            (screen_x, screen_y + TILE_SIZE - 1),
            (screen_x + TILE_SIZE - 1, screen_y + TILE_SIZE - 1),
            2
        )
        # 右侧阴影条
        pygame.draw.line(
            self.screen, shadow_color,
            (screen_x + TILE_SIZE - 1, screen_y),
            (screen_x + TILE_SIZE - 1, screen_y + TILE_SIZE - 1),
            2
        )

    def _draw_object_sprite(self, screen_x: int, screen_y: int, obj_type: ObjectType):
        """
        绘制地物精灵

        Args:
            screen_x: 屏幕 X 坐标
            screen_y: 屏幕 Y 坐标
            obj_type: 地物类型
        """
        sprite = self._sprite_manager.get_sprite(obj_type)
        if sprite is None:
            return

        # 绘制精灵（带透明通道）
        self.screen.blit(sprite, (screen_x, screen_y))

    @staticmethod
    def _adjust_color(color: Tuple[int, int, int], amount: int) -> Tuple[int, int, int]:
        """
        调整颜色亮度

        Args:
            color: 原始颜色 (R, G, B)
            amount: 亮度调整值（正数变亮，负数变暗）

        Returns:
            调整后的颜色
        """
        r = max(0, min(255, color[0] + amount))
        g = max(0, min(255, color[1] + amount))
        b = max(0, min(255, color[2] + amount))
        return (r, g, b)
