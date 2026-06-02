# scripts/client/npc_manager.py
import json
import os
from typing import Dict, List, Optional, Tuple
from .npc_sprite import NPCSprite
from .constants import TILE_SIZE, ZOOM_FACTOR, NPC_INTERACT_RANGE

def _load_json(relative_path: str) -> dict:
    """加载 JSON 数据文件。"""
    base_dir = os.path.dirname(os.path.abspath(__file__))
    full_path = os.path.join(base_dir, relative_path)
    with open(full_path, 'r', encoding='utf-8') as f:
        return json.load(f)


class NPCManager:
    """管理所有 NPC 实例、调度更新、交互检测。"""

    def __init__(self, zoom: int = ZOOM_FACTOR):
        self._zoom = zoom
        self._npc_defs: dict = _load_json('data/npc_defs.json')
        self._schedule: dict = _load_json('data/npc_schedule.json')
        self._npcs: Dict[str, NPCSprite] = {}
        self._active_npcs: List[NPCSprite] = []
        self._current_scene: Optional[str] = None
        self._init_npcs()

    def _init_npcs(self):
        base_dir = os.path.dirname(os.path.abspath(__file__))
        sprites_dir = os.path.join(base_dir, '..', '..', 'assets', 'sprites', 'npc')
        for npc_def in self._npc_defs.get('npcs', []):
            npc_id = npc_def['id']
            name = npc_def['name']
            sprite_path = os.path.join(sprites_dir, npc_def['sprite_sheet'])
            default_pos = npc_def.get('default_position', {'x': 0, 'y': 0})
            pixel_x = default_pos['x'] * TILE_SIZE * self._zoom
            pixel_y = default_pos['y'] * TILE_SIZE * self._zoom
            sprite = NPCSprite(npc_id=npc_id, name=name, pos_x=pixel_x, pos_y=pixel_y,
                             sprite_sheet_path=sprite_path, zoom=self._zoom)
            sprite._scene_id = npc_def.get('initial_scene', 'farm')
            self._npcs[npc_id] = sprite

    def set_current_scene(self, scene_id: str):
        self._current_scene = scene_id
        self._update_active_npcs()

    def _update_active_npcs(self):
        self._active_npcs = [npc for npc in self._npcs.values() if npc._scene_id == self._current_scene]

    def update_schedule(self, time_slot: str):
        for npc_id, schedule_list in self._schedule.items():
            if npc_id not in self._npcs:
                continue
            for entry in schedule_list:
                start, end = entry['time_range']
                if self._time_in_range(time_slot, start, end):
                    npc = self._npcs[npc_id]
                    new_scene = entry['scene']
                    pos = entry['position']
                    facing = entry.get('facing', 'down')
                    pixel_x = pos['x'] * TILE_SIZE * self._zoom
                    pixel_y = pos['y'] * TILE_SIZE * self._zoom
                    npc.set_position(pixel_x, pixel_y)
                    npc.set_direction(facing)
                    if npc._scene_id != new_scene:
                        npc._scene_id = new_scene
                    break
        self._update_active_npcs()

    def _time_in_range(self, current: str, start: str, end: str) -> bool:
        cur_h, cur_m = map(int, current.split(':'))
        start_h, start_m = map(int, start.split(':'))
        end_h, end_m = map(int, end.split(':'))
        cur_min = cur_h * 60 + cur_m
        start_min = start_h * 60 + start_m
        end_min = end_h * 60 + end_m
        if start_min <= end_min:
            return start_min <= cur_min < end_min
        else:
            return cur_min >= start_min or cur_min < end_min

    def get_npcs_in_scene(self) -> List[NPCSprite]:
        return self._active_npcs

    def get_npc_at_tile(self, tile_x: int, tile_y: int) -> Optional[NPCSprite]:
        for npc in self._active_npcs:
            if npc.tile_x == tile_x and npc.tile_y == tile_y:
                return npc
        return None

    def get_interactable_npc(self, player_tile_x: int, player_tile_y: int, facing: str) -> Optional[NPCSprite]:
        dx, dy = 0, 0
        if facing == 'up': dy = -1
        elif facing == 'down': dy = 1
        elif facing == 'left': dx = -1
        elif facing == 'right': dx = 1
        target_x = player_tile_x + dx
        target_y = player_tile_y + dy
        return self.get_npc_at_tile(target_x, target_y)

    def get_nearby_npc(self, player_tile_x: int, player_tile_y: int, range_tiles: int = NPC_INTERACT_RANGE) -> Optional[NPCSprite]:
        best_npc = None
        best_dist = float('inf')
        for npc in self._active_npcs:
            dx = abs(npc.tile_x - player_tile_x)
            dy = abs(npc.tile_y - player_tile_y)
            dist = max(dx, dy)
            if dist <= range_tiles and dist < best_dist:
                best_dist = dist
                best_npc = npc
        return best_npc

    def get_npc_by_id(self, npc_id: str) -> Optional[NPCSprite]:
        return self._npcs.get(npc_id)

    def update_animations(self, dt: float):
        for npc in self._active_npcs:
            npc.update_animation(dt)

    def face_player(self, player_tile_x: int, player_tile_y: int):
        for npc in self._active_npcs:
            dx = player_tile_x - npc.tile_x
            dy = player_tile_y - npc.tile_y
            if abs(dx) <= 1 and abs(dy) <= 1:
                if abs(dx) > abs(dy):
                    npc.set_direction('left' if dx < 0 else 'right')
                else:
                    npc.set_direction('up' if dy < 0 else 'down')
