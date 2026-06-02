"""
武器管理器模块
加载武器定义，管理武器装备状态。
"""
import json
import os
import logging
from typing import Dict, Optional, Any, List

logger = logging.getLogger("client.weapon_manager")

DATA_DIR = os.path.join(os.path.dirname(__file__), "data")


class WeaponManager:
    """武器管理器"""

    def __init__(self):
        self._weapons: Dict[str, Dict[str, Any]] = {}
        self._item_to_weapon: Dict[int, str] = {}  # item_id -> weapon_id
        self._load_weapons()

    def _load_weapons(self):
        """加载武器定义"""
        path = os.path.join(DATA_DIR, "weapon_defs.json")
        if not os.path.exists(path):
            logger.warning(f"[WeaponManager]weapon_defs.json not found at {path}")
            return

        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)

        for w in data.get("weapons", []):
            self._weapons[w["id"]] = w
            if "item_id" in w:
                self._item_to_weapon[w["item_id"]] = w["id"]

        logger.info(f"[WeaponManager]Loaded {len(self._weapons)} weapons")

    def get_weapon(self, weapon_id: str) -> Optional[Dict[str, Any]]:
        """获取武器定义"""
        return self._weapons.get(weapon_id)

    def get_weapon_by_item(self, item_id: int) -> Optional[Dict[str, Any]]:
        """通过物品ID获取武器定义"""
        weapon_id = self._item_to_weapon.get(item_id)
        if weapon_id:
            return self._weapons.get(weapon_id)
        return None

    def is_weapon(self, item_id: int) -> bool:
        """检查物品是否是武器"""
        return item_id in self._item_to_weapon

    def get_all_weapons(self) -> List[Dict[str, Any]]:
        """获取所有武器定义"""
        return list(self._weapons.values())
