"""
矿洞光照系统
实现多层光照效果：环境光、玩家光照、静态光源、动态效果。
"""
import time
import math
import random
import logging
from typing import List, Dict, Any, Optional, Tuple

import pygame

from .constants import TILE_SIZE, ZOOM_FACTOR

logger = logging.getLogger("client.cave_lighting")


class CaveLighting:
    """矿洞光照系统"""

    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._light_mask = pygame.Surface((screen_width, screen_height), pygame.SRCALPHA)
        self._ambient_surface = pygame.Surface((screen_width, screen_height), pygame.SRCALPHA)

        self._torch_flicker_timer = 0.0
        self._torch_flicker_offsets: Dict[int, float] = {}

        self._damage_flash_until = 0.0
        self._damage_flash_intensity = 0

        self._boss_pulse_timer = 0.0
        self._boss_lighting_config: Optional[Dict[str, Any]] = None

    def trigger_damage_flash(self, intensity: int = 100):
        self._damage_flash_until = time.time() + 0.3
        self._damage_flash_intensity = intensity

    def set_boss_lighting(self, config: Optional[Dict[str, Any]]):
        self._boss_lighting_config = config
        self._boss_pulse_timer = 0.0

    def update(self, dt: float):
        self._torch_flicker_timer += dt
        if self._torch_flicker_timer >= 0.1:
            self._torch_flicker_timer = 0.0
            for light_id in list(self._torch_flicker_offsets.keys()):
                self._torch_flicker_offsets[light_id] = random.uniform(-0.2, 0.2)
        if self._boss_lighting_config:
            self._boss_pulse_timer += dt

    def render(self, screen: pygame.Surface, camera_x: float, camera_y: float,
               player_world_x: float, player_world_y: float,
               light_radius: int, ambient_color: Tuple[int, int, int],
               static_lights: Optional[List[Dict[str, Any]]] = None):
        self._light_mask.fill((0, 0, 0, 255))

        player_screen_x = int((player_world_x - camera_x) * ZOOM_FACTOR)
        player_screen_y = int((player_world_y - camera_y) * ZOOM_FACTOR)
        radius_px = int(light_radius * TILE_SIZE * ZOOM_FACTOR)

        self._draw_light_circle(self._light_mask, player_screen_x, player_screen_y,
                                radius_px, (255, 255, 255, 0))

        if static_lights:
            for i, light in enumerate(static_lights):
                lx = int((light["x"] * TILE_SIZE - camera_x) * ZOOM_FACTOR)
                ly = int((light["y"] * TILE_SIZE - camera_y) * ZOOM_FACTOR)
                lr = int(light["radius"] * TILE_SIZE * ZOOM_FACTOR)
                if light.get("type") == "torch":
                    flicker = self._torch_flicker_offsets.get(i, 0) * TILE_SIZE * ZOOM_FACTOR
                    lr += int(flicker)
                if i not in self._torch_flicker_offsets:
                    self._torch_flicker_offsets[i] = 0.0
                color = tuple(light.get("color", [255, 200, 100]))
                self._draw_light_circle(self._light_mask, lx, ly, lr, (*color, 0))

        screen.blit(self._light_mask, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)

        self._ambient_surface.fill((*ambient_color, 30))
        screen.blit(self._ambient_surface, (0, 0))

        if self._boss_lighting_config:
            self._render_boss_lighting(screen)
        if time.time() < self._damage_flash_until:
            self._render_damage_flash(screen)

    def _draw_light_circle(self, surface: pygame.Surface, x: int, y: int,
                           radius: int, center_color: Tuple[int, int, int, int]):
        if radius <= 0:
            return
        step = max(2, radius // 15)
        for r in range(radius, 0, -step):
            alpha = int(255 * (1 - r / radius))
            color = (*center_color[:3], alpha)
            pygame.draw.circle(surface, color, (x, y), r)

    def _render_boss_lighting(self, screen: pygame.Surface):
        config = self._boss_lighting_config
        if not config:
            return
        pulse_speed = config.get("pulse_speed", 0.5)
        pulse_range = config.get("pulse_range", [0.8, 1.2])
        pulse = math.sin(self._boss_pulse_timer * pulse_speed * math.pi * 2)
        pulse = pulse_range[0] + (pulse_range[1] - pulse_range[0]) * (pulse + 1) / 2
        boss_surface = pygame.Surface((self._screen_width, self._screen_height), pygame.SRCALPHA)
        color = config.get("color", [150, 50, 50])
        alpha = int(20 * pulse)
        boss_surface.fill((*color, alpha))
        screen.blit(boss_surface, (0, 0))

    def _render_damage_flash(self, screen: pygame.Surface):
        remaining = self._damage_flash_until - time.time()
        alpha = int(self._damage_flash_intensity * (remaining / 0.3))
        alpha = max(0, min(255, alpha))
        flash_surface = pygame.Surface((self._screen_width, self._screen_height), pygame.SRCALPHA)
        flash_surface.fill((255, 0, 0, alpha))
        screen.blit(flash_surface, (0, 0))
