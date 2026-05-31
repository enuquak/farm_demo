"""
地物精灵管理器模块
管理地物类型的像素精灵缓存，支持 16×16 像素精灵生成和拉伸
"""
import pygame
from typing import Dict, Optional, Tuple

from .constants import ObjectType, OBJECT_SPRITE_SIZE, TILE_SIZE
from .sprite_data import OBJECT_SPRITES


class ObjectSpriteManager:
    """
    地物精灵管理器
    负责生成和缓存地物类型的像素精灵
    """

    def __init__(self):
        """初始化精灵管理器"""
        # 精灵缓存：ObjectType -> pygame.Surface
        self._sprite_cache: Dict[ObjectType, pygame.Surface] = {}
        # 预生成所有精灵
        self._generate_all_sprites()

    def _generate_all_sprites(self):
        """预生成所有地物类型的精灵"""
        for obj_type in ObjectType:
            if obj_type == ObjectType.NONE:
                continue
            self._generate_sprite(obj_type)

    def _generate_sprite(self, obj_type: ObjectType):
        """
        生成指定地物类型的精灵

        Args:
            obj_type: 地物类型
        """
        sprite_data = OBJECT_SPRITES.get(obj_type)
        if not sprite_data:
            return
        size = sprite_data.get("size", OBJECT_SPRITE_SIZE)
        base_color = sprite_data.get("base_color", (255, 0, 255))
        pixels = sprite_data.get("pixels", [])
        color_map = sprite_data.get("color_map", {})

        # 创建 16×16 精灵表面（带 alpha 通道）
        sprite_surface = pygame.Surface((size, size), pygame.SRCALPHA)
        sprite_surface.fill((0, 0, 0, 0))  # 透明背景

        # 绘制像素
        for y in range(min(len(pixels), size)):
            row = pixels[y]
            for x in range(min(len(row), size)):
                pixel_value = row[x]
                if pixel_value == 0:
                    continue  # 透明像素

                # 获取颜色
                if pixel_value in color_map:
                    color = color_map[pixel_value]
                else:
                    color = base_color

                # 绘制像素（带 alpha 通道）
                sprite_surface.set_at((x, y), (*color, 255))

        # 拉伸到 TILE_SIZE（使用 NEAREST 采样）
        scaled_surface = pygame.transform.scale(
            sprite_surface,
            (TILE_SIZE, TILE_SIZE),
        )
        # 使用 NEAREST 采样保持像素风格
        scaled_surface = pygame.transform.scale(
            sprite_surface,
            (TILE_SIZE, TILE_SIZE),
        )

        # 缓存精灵
        self._sprite_cache[obj_type] = scaled_surface

    def get_sprite(self, obj_type: ObjectType) -> Optional[pygame.Surface]:
        """
        获取指定地物类型的精灵

        Args:
            obj_type: 地物类型

        Returns:
            pygame.Surface 精灵表面，无精灵时返回 None
        """
        return self._sprite_cache.get(obj_type)

    def has_sprite(self, obj_type: ObjectType) -> bool:
        """
        检查指定地物类型是否有精灵

        Args:
            obj_type: 地物类型

        Returns:
            是否有精灵
        """
        return obj_type in self._sprite_cache

    def clear_cache(self):
        """清除精灵缓存"""
        self._sprite_cache.clear()

    def reload_sprites(self):
        """重新加载所有精灵"""
        self.clear_cache()
        self._generate_all_sprites()


# 全局精灵管理器实例
_sprite_manager: Optional[ObjectSpriteManager] = None


def get_sprite_manager() -> ObjectSpriteManager:
    """
    获取全局精灵管理器实例

    Returns:
        ObjectSpriteManager 实例
    """
    global _sprite_manager
    if _sprite_manager is None:
        _sprite_manager = ObjectSpriteManager()
    return _sprite_manager


def reload_sprite_manager():
    """重新加载全局精灵管理器"""
    global _sprite_manager
    if _sprite_manager is not None:
        _sprite_manager.reload_sprites()
