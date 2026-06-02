"""
矿洞场景管理器
协调地图生成、光照渲染、矿洞层级状态。
"""
import json
import os
import logging
from typing import Dict, Any, Optional, List

import pygame

from .cave_generator import CaveGenerator, CaveMap
from .cave_lighting import CaveLighting
from .constants import TILE_SIZE, ZOOM_FACTOR

logger = logging.getLogger("client.cave_manager")

DATA_DIR = os.path.join(os.path.dirname(__file__), "data")


class CaveManager:
    """矿洞场景管理器"""

    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height

        self._levels_config: Dict[int, Dict[str, Any]] = {}
        self._scene_to_level: Dict[str, int] = {}
        self._load_config()

        self._generator = CaveGenerator()
        self._lighting = CaveLighting(screen_width, screen_height)

        self._current_level: Optional[int] = None
        self._current_map: Optional[CaveMap] = None
        self._in_cave: bool = False

    def _load_config(self):
        path = os.path.join(DATA_DIR, "cave_levels.json")
        if not os.path.exists(path):
            logger.warning(f"[CaveManager]cave_levels.json not found at {path}")
            return
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        for level_data in data.get("cave_levels", []):
            level = level_data["level"]
            self._levels_config[level] = level_data
            self._scene_to_level[level_data["scene_id"]] = level
        logger.info(f"[CaveManager]Loaded {len(self._levels_config)} cave levels")

    @property
    def in_cave(self) -> bool:
        return self._in_cave

    @property
    def current_level(self) -> Optional[int]:
        return self._current_level

    @property
    def current_map(self) -> Optional[CaveMap]:
        return self._current_map

    @property
    def lighting(self) -> CaveLighting:
        return self._lighting

    def get_level_config(self, level: int) -> Optional[Dict[str, Any]]:
        return self._levels_config.get(level)

    def get_level_for_scene(self, scene_id: str) -> Optional[int]:
        return self._scene_to_level.get(scene_id)

    def enter_cave(self, level: int = 1):
        config = self._levels_config.get(level)
        if not config:
            logger.error(f"[CaveManager]No config for cave level {level}")
            return
        self._current_level = level
        self._in_cave = True
        self._current_map = self._generator.generate(config)
        self._lighting.set_boss_lighting(config.get("boss_lighting"))
        logger.info(f"[CaveManager]Entered cave level {level}: {config['name']}")

    def change_level(self, new_level: int):
        if not self._in_cave:
            logger.warning("[CaveManager]Not in cave, cannot change level")
            return
        config = self._levels_config.get(new_level)
        if not config:
            logger.error(f"[CaveManager]No config for cave level {new_level}")
            return
        self._current_level = new_level
        self._current_map = self._generator.generate(config)
        self._lighting.set_boss_lighting(config.get("boss_lighting"))
        logger.info(f"[CaveManager]Changed to cave level {new_level}: {config['name']}")

    def exit_cave(self):
        self._current_level = None
        self._current_map = None
        self._in_cave = False
        self._lighting.set_boss_lighting(None)
        logger.info("[CaveManager]Exited cave")

    def is_stairs_at(self, tile_x: int, tile_y: int) -> Optional[str]:
        if not self._current_map:
            return None
        if self._current_map.stairs_up == (tile_x, tile_y):
            return "up"
        if self._current_map.stairs_down == (tile_x, tile_y):
            return "down"
        return None

    def get_entrance_pos(self) -> Optional[tuple]:
        if not self._current_map or not self._current_map.stairs_up:
            return None
        x, y = self._current_map.stairs_up
        return (x * TILE_SIZE, y * TILE_SIZE)

    def get_target_scene_for_stairs(self, stairs_direction: str) -> Optional[str]:
        if not self._current_level:
            return None
        config = self._levels_config.get(self._current_level)
        if not config:
            return None
        if stairs_direction == "up":
            if self._current_level == 1:
                return "farm"
            prev_scene = self._levels_config.get(self._current_level - 1)
            if prev_scene:
                return prev_scene["scene_id"]
            return "farm"
        elif stairs_direction == "down":
            for level, cfg in self._levels_config.items():
                if cfg.get("exit_up") and level > self._current_level:
                    return cfg["scene_id"]
            return None
        return None

    def update(self, dt: float):
        if self._in_cave:
            self._lighting.update(dt)

    def render_lighting(self, screen: pygame.Surface, camera_x: float, camera_y: float,
                        player_world_x: float, player_world_y: float):
        if not self._in_cave or not self._current_map:
            return
        config = self._levels_config.get(self._current_level, {})
        light_radius = config.get("light_radius", 5)
        ambient_color = tuple(config.get("ambient_color", [30, 25, 25]))
        static_lights = self._current_map.lights
        self._lighting.render(screen, camera_x, camera_y,
                              player_world_x, player_world_y,
                              light_radius, ambient_color, static_lights)

    def render_minimap(self, screen: pygame.Surface, x: int, y: int):
        if not self._current_map:
            return
        scale = 3
        mw = self._current_map.width * scale
        mh = self._current_map.height * scale
        minimap = pygame.Surface((mw, mh), pygame.SRCALPHA)
        minimap.fill((0, 0, 0, 180))
        for ty in range(self._current_map.height):
            for tx in range(self._current_map.width):
                if self._current_map.map_data[ty][tx] == 0:
                    color = (80, 70, 60)
                else:
                    color = (30, 25, 20)
                pygame.draw.rect(minimap, color, (tx * scale, ty * scale, scale, scale))
        if self._current_map.stairs_up:
            sx, sy = self._current_map.stairs_up
            pygame.draw.rect(minimap, (0, 200, 0), (sx * scale, sy * scale, scale, scale))
        if self._current_map.stairs_down:
            sx, sy = self._current_map.stairs_down
            pygame.draw.rect(minimap, (200, 200, 0), (sx * scale, sy * scale, scale, scale))
        screen.blit(minimap, (x, y))
