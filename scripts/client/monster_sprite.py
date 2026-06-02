"""
怪物精灵模块
渲染怪物实体，支持动画、HP条、受击闪白效果。
"""
import math
import time
import logging
from typing import Optional, Dict, Any, Tuple

import pygame

from .constants import TILE_SIZE, ZOOM_FACTOR, Direction

logger = logging.getLogger("client.monster_sprite")


class MonsterState:
    """怪物状态常量"""
    IDLE = "idle"
    WANDER = "wander"
    CHASE = "chase"
    ATTACK = "attack"
    HIT = "hit"
    KNOCKBACK = "knockback"
    DEAD = "dead"


class MonsterSprite:
    """
    怪物精灵
    渲染单个怪物实体，支持动画、HP条、受击闪白。
    """

    def __init__(self, monster_id: int, monster_type: str, x: float, y: float,
                 hp: int, max_hp: int, config: Dict[str, Any]):
        self.monster_id = monster_id
        self.monster_type = monster_type
        self.world_x = x  # 世界坐标（像素）
        self.world_y = y
        self.hp = hp
        self.max_hp = max_hp
        self.config = config

        self.state = MonsterState.IDLE
        self.direction = Direction.DOWN

        # 受击闪白
        self._hit_flash_until = 0.0

        # 击退
        self._knockback_until = 0.0
        self._knockback_dx = 0.0
        self._knockback_dy = 0.0

        # 动画
        self._anim_frame = 0
        self._anim_timer = 0.0

        # 颜色渲染（临时方案，后续替换为精灵图）
        color = config.get("color", [128, 128, 128])
        self._color = tuple(color)
        size = config.get("size", [12, 12])
        self._width = size[0]
        self._height = size[1]

    @property
    def tile_x(self) -> int:
        return int(self.world_x / TILE_SIZE)

    @property
    def tile_y(self) -> int:
        return int(self.world_y / TILE_SIZE)

    @property
    def is_hit(self) -> bool:
        return time.time() < self._hit_flash_until

    @property
    def is_knockback(self) -> bool:
        return time.time() < self._knockback_until

    def apply_hit(self, knockback_dx: float = 0.0, knockback_dy: float = 0.0):
        """应用受击效果"""
        self._hit_flash_until = time.time() + 0.15
        if knockback_dx != 0 or knockback_dy != 0:
            self._knockback_until = time.time() + 0.2
            self._knockback_dx = knockback_dx
            self._knockback_dy = knockback_dy
        self.state = MonsterState.HIT

    def update(self, dt: float):
        """更新怪物状态"""
        now = time.time()

        # 击退移动
        if self.is_knockback:
            self.world_x += self._knockback_dx * dt * 5 * TILE_SIZE
            self.world_y += self._knockback_dy * dt * 5 * TILE_SIZE

        # 动画更新
        self._anim_timer += dt
        if self._anim_timer >= 0.2:
            self._anim_timer = 0.0
            self._anim_frame = (self._anim_frame + 1) % 4

    def render(self, screen: pygame.Surface, camera_x: float, camera_y: float):
        """
        渲染怪物到屏幕

        Args:
            screen: PyGame 屏幕 Surface
            camera_x: 相机 X 偏移（世界像素）
            camera_y: 相机 Y 偏移（世界像素）
        """
        # 计算屏幕坐标
        screen_x = (self.world_x - camera_x) * ZOOM_FACTOR
        screen_y = (self.world_y - camera_y) * ZOOM_FACTOR

        # 怪物尺寸（缩放后）
        w = self._width * ZOOM_FACTOR
        h = self._height * ZOOM_FACTOR

        # 受击闪白效果
        color = (255, 255, 255) if self.is_hit else self._color

        # 绘制怪物矩形（临时方案）
        rect = pygame.Rect(
            int(screen_x - w // 2),
            int(screen_y - h // 2),
            w, h
        )
        pygame.draw.rect(screen, color, rect)
        pygame.draw.rect(screen, (0, 0, 0), rect, 1)

        # 绘制眼睛（朝向指示）
        eye_size = max(2, ZOOM_FACTOR)
        if self.direction == Direction.DOWN:
            eye_y = int(screen_y - h // 4)
            pygame.draw.circle(screen, (0, 0, 0), (int(screen_x - w // 4), eye_y), eye_size)
            pygame.draw.circle(screen, (0, 0, 0), (int(screen_x + w // 4), eye_y), eye_size)
        elif self.direction == Direction.UP:
            eye_y = int(screen_y + h // 4)
            pygame.draw.circle(screen, (0, 0, 0), (int(screen_x - w // 4), eye_y), eye_size)
            pygame.draw.circle(screen, (0, 0, 0), (int(screen_x + w // 4), eye_y), eye_size)
        elif self.direction == Direction.LEFT:
            eye_x = int(screen_x - w // 4)
            pygame.draw.circle(screen, (0, 0, 0), (eye_x, int(screen_y)), eye_size)
        else:
            eye_x = int(screen_x + w // 4)
            pygame.draw.circle(screen, (0, 0, 0), (eye_x, int(screen_y)), eye_size)

        # 绘制 HP 条（受伤时显示）
        if self.hp < self.max_hp and self.hp > 0:
            self._render_hp_bar(screen, int(screen_x), int(screen_y - h // 2 - 6))

    def _render_hp_bar(self, screen: pygame.Surface, x: int, y: int):
        """绘制 HP 条"""
        bar_w = 24 * ZOOM_FACTOR // 2
        bar_h = 3 * ZOOM_FACTOR // 2

        # 背景
        bg_rect = pygame.Rect(x - bar_w // 2, y, bar_w, bar_h)
        pygame.draw.rect(screen, (60, 60, 60), bg_rect)

        # HP
        ratio = max(0, self.hp / self.max_hp)
        hp_rect = pygame.Rect(x - bar_w // 2, y, int(bar_w * ratio), bar_h)
        hp_color = (50, 200, 50) if ratio > 0.5 else (200, 200, 50) if ratio > 0.25 else (200, 50, 50)
        pygame.draw.rect(screen, hp_color, hp_rect)
