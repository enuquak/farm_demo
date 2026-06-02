"""
战斗系统模块
处理玩家挥砍攻击、伤害计算、击退效果。
"""
import math
import time
import random
import logging
from typing import Optional, Dict, Any, List, Tuple

from .constants import TILE_SIZE, Direction, ATTACK_COOLDOWN_DEFAULT
from .monster_manager import MonsterManager
from .monster_sprite import MonsterSprite
from .weapon_manager import WeaponManager

logger = logging.getLogger("client.combat_system")


class CombatSystem:
    """
    战斗系统
    处理攻击判定、伤害计算、击退效果。
    """

    def __init__(self, weapon_manager: WeaponManager, monster_manager: MonsterManager):
        self._weapon_mgr = weapon_manager
        self._monster_mgr = monster_manager

        # 攻击冷却
        self._last_attack_time = 0.0

        # 当前装备的武器
        self._equipped_weapon_id: Optional[str] = None

    @property
    def equipped_weapon(self) -> Optional[Dict[str, Any]]:
        """获取当前装备的武器"""
        if self._equipped_weapon_id:
            return self._weapon_mgr.get_weapon(self._equipped_weapon_id)
        return None

    def equip_weapon(self, weapon_id: Optional[str]):
        """装备武器"""
        self._equipped_weapon_id = weapon_id
        logger.info(f"[Combat]Equipped weapon: {weapon_id}")

    def equip_weapon_by_item(self, item_id: int):
        """通过物品ID装备武器"""
        weapon = self._weapon_mgr.get_weapon_by_item(item_id)
        if weapon:
            self._equipped_weapon_id = weapon["id"]
            logger.info(f"[Combat]Equipped weapon by item: {item_id} -> {weapon['id']}")
        else:
            self._equipped_weapon_id = None

    def can_attack(self) -> bool:
        """是否可以攻击"""
        weapon = self.equipped_weapon
        if not weapon:
            return False

        now = time.time()
        cooldown = weapon.get("attack_speed", ATTACK_COOLDOWN_DEFAULT)
        return (now - self._last_attack_time) >= cooldown

    def try_attack(self, player_x: float, player_y: float,
                   direction: str, player_attack_power: int = 0
                   ) -> List[Tuple[int, int, bool]]:
        """
        尝试攻击

        Args:
            player_x: 玩家世界坐标X（像素）
            player_y: 玩家世界坐标Y（像素）
            direction: 攻击方向
            player_attack_power: 玩家额外攻击力

        Returns:
            [(monster_id, damage, is_kill), ...] 命中的怪物列表
        """
        if not self.can_attack():
            return []

        weapon = self.equipped_weapon
        if not weapon:
            return []

        self._last_attack_time = time.time()

        # 计算攻击方向向量
        dir_vectors = {
            Direction.UP: (0, -1),
            Direction.DOWN: (0, 1),
            Direction.LEFT: (-1, 0),
            Direction.RIGHT: (1, 0),
        }
        dir_vec = dir_vectors.get(direction, (0, 1))

        # 攻击范围（像素）
        attack_range = weapon.get("attack_range", 1.5) * TILE_SIZE
        weapon_attack = weapon.get("attack", 10)
        knockback = weapon.get("knockback", 2.0)

        # 攻击判定区域（面向方向的矩形）
        # 攻击中心点在玩家前方
        attack_cx = player_x + dir_vec[0] * attack_range * 0.6
        attack_cy = player_y + dir_vec[1] * attack_range * 0.6

        # 检测范围内怪物
        hits = []
        for monster in self._monster_mgr.get_all_monsters().values():
            if monster.hp <= 0:
                continue

            dx = monster.world_x - attack_cx
            dy = monster.world_y - attack_cy
            dist = math.sqrt(dx * dx + dy * dy)

            if dist <= attack_range:
                # 计算伤害
                monster_def = monster.config.get("defense", 0)
                base_damage = weapon_attack + player_attack_power - monster_def
                random_factor = random.uniform(0.9, 1.1)
                damage = max(1, int(base_damage * random_factor))

                # 应用伤害
                monster.hp = max(0, monster.hp - damage)

                # 计算击退方向
                knockback_dist = knockback * TILE_SIZE
                if dist > 0:
                    kb_dx = (dx / dist) * knockback_dist * 0.3
                    kb_dy = (dy / dist) * knockback_dist * 0.3
                else:
                    kb_dx = dir_vec[0] * knockback_dist * 0.3
                    kb_dy = dir_vec[1] * knockback_dist * 0.3

                monster.apply_hit(kb_dx, kb_dy)

                is_kill = monster.hp <= 0
                hits.append((monster.monster_id, damage, is_kill))

                logger.info(f"[Combat]Hit monster_id={monster.monster_id}, "
                           f"damage={damage}, hp={monster.hp}, kill={is_kill}")

        return hits

    def get_attack_info(self) -> Optional[Dict[str, Any]]:
        """获取当前攻击信息（用于发送给服务器）"""
        weapon = self.equipped_weapon
        if not weapon:
            return None
        return {
            "weapon_id": weapon["id"],
            "attack": weapon.get("attack", 10),
            "attack_range": weapon.get("attack_range", 1.5),
            "knockback": weapon.get("knockback", 2.0),
        }
