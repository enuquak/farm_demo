# scripts/client/affection_system.py
from typing import Dict


class AffectionSystem:
    """好感度管理，送礼逻辑。"""

    def __init__(self):
        self._affection: Dict[str, int] = {}
        self._daily_gifts: Dict[str, bool] = {}

    def get_affection(self, npc_id: str) -> int:
        return self._affection.get(npc_id, 0)

    def set_affection(self, npc_id: str, value: int):
        self._affection[npc_id] = max(0, min(100, value))

    def add_affection(self, npc_id: str, amount: int) -> int:
        current = self._affection.get(npc_id, 0)
        new_value = max(0, min(100, current + amount))
        self._affection[npc_id] = new_value
        return new_value

    def can_gift(self, npc_id: str) -> bool:
        return not self._daily_gifts.get(npc_id, False)

    def record_gift(self, npc_id: str):
        self._daily_gifts[npc_id] = True

    def reset_daily_gifts(self):
        self._daily_gifts.clear()

    def sync_affection(self, affection_map: Dict[str, int]):
        for npc_id, value in affection_map.items():
            self._affection[npc_id] = max(0, min(100, value))

    def get_all(self) -> Dict[str, int]:
        return self._affection.copy()
