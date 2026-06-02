"""
怪物管理器模块
管理怪物实例，加载怪物定义，驱动怪物AI状态机。
处理服务器同步消息。
"""
import json
import os
import time
import math
import random
import logging
from typing import Dict, Optional, List, Any, Tuple

from .constants import TILE_SIZE, Direction
from .monster_sprite import MonsterSprite, MonsterState

logger = logging.getLogger("client.monster_manager")

DATA_DIR = os.path.join(os.path.dirname(__file__), "data")


class MonsterManager:
    """
    怪物管理器
    管理场景中的所有怪物实例。
    """

    def __init__(self):
        self._monsters: Dict[int, MonsterSprite] = {}
        self._monster_defs: Dict[str, Dict[str, Any]] = {}
        self._load_monster_defs()

    def _load_monster_defs(self):
        """加载怪物定义"""
        path = os.path.join(DATA_DIR, "monster_defs.json")
        if not os.path.exists(path):
            logger.warning(f"[MonsterManager]monster_defs.json not found at {path}")
            return

        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)

        for m in data.get("monsters", []):
            self._monster_defs[m["id"]] = m

        logger.info(f"[MonsterManager]Loaded {len(self._monster_defs)} monster types")

    def get_monster_def(self, monster_type: str) -> Optional[Dict[str, Any]]:
        """获取怪物定义"""
        return self._monster_defs.get(monster_type)

    def spawn_monster(self, monster_id: int, monster_type: str,
                      x: float, y: float, hp: int, max_hp: int) -> Optional[MonsterSprite]:
        """生成怪物"""
        config = self._monster_defs.get(monster_type)
        if not config:
            logger.warning(f"[MonsterManager]Unknown monster type: {monster_type}")
            return None

        monster = MonsterSprite(monster_id, monster_type, x, y, hp, max_hp, config)
        self._monsters[monster_id] = monster
        logger.debug(f"[MonsterManager]Spawned monster_id={monster_id}, type={monster_type}")
        return monster

    def remove_monster(self, monster_id: int):
        """移除怪物"""
        if monster_id in self._monsters:
            del self._monsters[monster_id]
            logger.debug(f"[MonsterManager]Removed monster_id={monster_id}")

    def get_monster(self, monster_id: int) -> Optional[MonsterSprite]:
        """获取怪物"""
        return self._monsters.get(monster_id)

    def get_all_monsters(self) -> Dict[int, MonsterSprite]:
        """获取所有怪物"""
        return self._monsters

    def get_monsters_in_range(self, world_x: float, world_y: float,
                              range_tiles: float) -> List[MonsterSprite]:
        """获取范围内的怪物"""
        result = []
        range_px = range_tiles * TILE_SIZE
        for m in self._monsters.values():
            if m.hp <= 0:
                continue
            dx = m.world_x - world_x
            dy = m.world_y - world_y
            dist = math.sqrt(dx * dx + dy * dy)
            if dist <= range_px:
                result.append(m)
        return result

    def update(self, dt: float, player_x: float, player_y: float):
        """
        更新所有怪物（客户端AI预测）

        Args:
            dt: 帧间隔
            player_x: 玩家世界坐标X
            player_y: 玩家世界坐标Y
        """
        for monster in self._monsters.values():
            if monster.hp <= 0:
                continue
            self._update_monster_ai(monster, dt, player_x, player_y)
            monster.update(dt)

    def _update_monster_ai(self, monster: MonsterSprite, dt: float,
                           player_x: float, player_y: float):
        """更新怪物AI（客户端预测）"""
        config = monster.config
        aggro_range = config.get("aggro_range", 5) * TILE_SIZE
        speed = config.get("speed", 30.0)

        dx = player_x - monster.world_x
        dy = player_y - monster.world_y
        dist = math.sqrt(dx * dx + dy * dy)

        if monster.is_knockback:
            return

        if dist < aggro_range and dist > TILE_SIZE * 0.5:
            # 追击玩家
            monster.state = MonsterState.CHASE
            if dist > 0:
                nx = dx / dist
                ny = dy / dist
                monster.world_x += nx * speed * dt
                monster.world_y += ny * speed * dt

                # 更新朝向
                if abs(dx) > abs(dy):
                    monster.direction = Direction.RIGHT if dx > 0 else Direction.LEFT
                else:
                    monster.direction = Direction.DOWN if dy > 0 else Direction.UP
        else:
            # 游荡
            if monster.state != MonsterState.WANDER:
                monster.state = MonsterState.IDLE

    def handle_monster_spawn(self, monster_id: int, monster_type: str,
                             x: float, y: float, hp: int, max_hp: int):
        """处理怪物刷新消息"""
        self.spawn_monster(monster_id, monster_type, x, y, hp, max_hp)

    def handle_monster_death(self, monster_id: int):
        """处理怪物死亡消息"""
        monster = self._monsters.get(monster_id)
        if monster:
            monster.hp = 0
            # 延迟移除（播放死亡动画）
            # 简单方案：立即移除
            self.remove_monster(monster_id)

    def handle_monster_move(self, monster_id: int, x: float, y: float, state: str):
        """处理怪物移动同步消息"""
        monster = self._monsters.get(monster_id)
        if monster:
            # 插值到目标位置
            monster.world_x = x
            monster.world_y = y
            if state:
                monster.state = state

    def handle_monster_attack(self, monster_id: int, damage: int):
        """处理怪物攻击通知"""
        monster = self._monsters.get(monster_id)
        if monster:
            monster.state = MonsterState.ATTACK

    def clear(self):
        """清空所有怪物（场景切换时）"""
        self._monsters.clear()
        logger.info("[MonsterManager]Cleared all monsters")

    @property
    def monster_count(self) -> int:
        """当前怪物数量"""
        return len(self._monsters)
