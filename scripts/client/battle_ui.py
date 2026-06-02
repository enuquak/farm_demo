"""
战斗UI模块
渲染玩家HP条、伤害数字。
"""
import time
import logging
from typing import List, Tuple

import pygame

from .constants import (
    PLAYER_HP_BAR_WIDTH, PLAYER_HP_BAR_HEIGHT,
    PLAYER_HP_BAR_MARGIN_TOP, DAMAGE_NUMBER_DURATION,
    DAMAGE_NUMBER_RISE_SPEED, ZOOM_FACTOR,
)

logger = logging.getLogger("client.battle_ui")


class DamageNumber:
    """浮动伤害数字"""

    def __init__(self, x: float, y: float, damage: int, is_heal: bool = False):
        self.x = x
        self.y = y
        self.damage = damage
        self.is_heal = is_heal
        self.spawn_time = time.time()

    @property
    def is_expired(self) -> bool:
        return (time.time() - self.spawn_time) >= DAMAGE_NUMBER_DURATION

    def update(self, dt: float):
        self.y -= DAMAGE_NUMBER_RISE_SPEED * dt


class BattleUI:
    """战斗UI渲染器"""

    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height

        # 伤害数字列表
        self._damage_numbers: List[DamageNumber] = []

        # 字体
        self._font = pygame.font.SysFont("arial", 14, bold=True)
        self._hp_font = pygame.font.SysFont("arial", 10)

    def add_damage_number(self, world_x: float, world_y: float, damage: int,
                          is_heal: bool = False):
        """添加伤害数字"""
        self._damage_numbers.append(
            DamageNumber(world_x, world_y, damage, is_heal)
        )

    def update(self, dt: float):
        """更新伤害数字"""
        for dn in self._damage_numbers:
            dn.update(dt)
        self._damage_numbers = [dn for dn in self._damage_numbers if not dn.is_expired]

    def render(self, screen: pygame.Surface, camera_x: float, camera_y: float,
               player_hp: int, player_max_hp: int):
        """
        渲染战斗UI

        Args:
            screen: PyGame 屏幕
            camera_x: 相机X偏移
            camera_y: 相机Y偏移
            player_hp: 玩家当前HP
            player_max_hp: 玩家最大HP
        """
        # 玩家HP条（左上角）
        self._render_player_hp(screen, player_hp, player_max_hp)

        # 伤害数字
        for dn in self._damage_numbers:
            self._render_damage_number(screen, dn, camera_x, camera_y)

    def _render_player_hp(self, screen: pygame.Surface, hp: int, max_hp: int):
        """绘制玩家HP条"""
        x = 10
        y = PLAYER_HP_BAR_MARGIN_TOP + 20  # 在时间HUD下方

        # 标签
        label = self._hp_font.render("HP", True, (200, 200, 200))
        screen.blit(label, (x, y - 12))

        # 背景
        bg_rect = pygame.Rect(x, y, PLAYER_HP_BAR_WIDTH, PLAYER_HP_BAR_HEIGHT)
        pygame.draw.rect(screen, (60, 60, 60), bg_rect)
        pygame.draw.rect(screen, (100, 100, 100), bg_rect, 1)

        # HP
        ratio = max(0, hp / max_hp) if max_hp > 0 else 0
        hp_rect = pygame.Rect(x, y, int(PLAYER_HP_BAR_WIDTH * ratio), PLAYER_HP_BAR_HEIGHT)
        hp_color = (50, 200, 50) if ratio > 0.5 else (200, 200, 50) if ratio > 0.25 else (200, 50, 50)
        pygame.draw.rect(screen, hp_color, hp_rect)

        # 数值
        hp_text = self._hp_font.render(f"{hp}/{max_hp}", True, (255, 255, 255))
        screen.blit(hp_text, (x + PLAYER_HP_BAR_WIDTH + 5, y - 2))

    def _render_damage_number(self, screen: pygame.Surface, dn: DamageNumber,
                              camera_x: float, camera_y: float):
        """绘制伤害数字"""
        screen_x = (dn.x - camera_x) * ZOOM_FACTOR
        screen_y = (dn.y - camera_y) * ZOOM_FACTOR

        if dn.is_heal:
            color = (50, 255, 50)
            text = f"+{dn.damage}"
        else:
            color = (255, 50, 50)
            text = f"-{dn.damage}"

        # 淡出效果
        elapsed = time.time() - dn.spawn_time
        alpha = max(0, 255 - int(255 * elapsed / DAMAGE_NUMBER_DURATION))

        surf = self._font.render(text, True, color)
        if alpha < 255:
            surf.set_alpha(alpha)

        rect = surf.get_rect(center=(int(screen_x), int(screen_y)))
        screen.blit(surf, rect)
