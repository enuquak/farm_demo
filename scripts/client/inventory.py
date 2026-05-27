"""
Inventory Module - Player backpack data model.

Manages 30-slot backpack (10 hotbar + 20 extended) with item stacking,
activeSlot management, and serialization support.
"""
import logging
from typing import Optional, Dict, List, Any

# Import item registry for max_stack values
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'server'))
from item_registry import get_item_def

logger = logging.getLogger("client.inventory")

# Constants
TOTAL_SLOTS = 30
HOTBAR_SLOTS = 10
EXTENDED_SLOTS = 20
DEFAULT_MAX_STACK = 99

# Default initial items for new players: (slot_index, item_id, count)
DEFAULT_ITEMS = [
    (0, 3, 1),   # slot[0] = axe x1
    (1, 4, 1),   # slot[1] = hoe x1
    (2, 5, 5),   # slot[2] = seeds x5
    (3, 6, 3),   # slot[3] = bread x3
]


class Slot:
    """Represents a single inventory slot."""

    __slots__ = ('item_id', 'count')

    def __init__(self, item_id: int, count: int):
        self.item_id = item_id
        self.count = count

    def to_dict(self) -> Dict[str, int]:
        """Serialize slot to dict."""
        return {"item_id": self.item_id, "count": self.count}

    @classmethod
    def from_dict(cls, data: Dict[str, int]) -> 'Slot':
        """Deserialize slot from dict."""
        return cls(item_id=data["item_id"], count=data["count"])

    def __repr__(self) -> str:
        return f"Slot(item_id={self.item_id}, count={self.count})"


class Inventory:
    """
    Player inventory management.

    Manages 30 slots (10 hotbar + 20 extended) with item stacking,
    activeSlot pointer (0-9), and serialization support.
    """

    def __init__(self):
        """Initialize empty inventory."""
        # 30 slots, each is either None or a Slot object
        self._slots: List[Optional[Slot]] = [None] * TOTAL_SLOTS
        # Active slot index (0-9, points to hotbar)
        self._active_slot: int = 0

    @property
    def active_slot(self) -> int:
        """Get current active slot index."""
        return self._active_slot

    @property
    def slots(self) -> List[Optional[Slot]]:
        """Get all slots (read-only view)."""
        return self._slots

    def get_slot(self, index: int) -> Optional[Slot]:
        """
        Get slot at index.

        Args:
            index: Slot index (0-29)

        Returns:
            Slot object or None if empty/invalid
        """
        if 0 <= index < TOTAL_SLOTS:
            return self._slots[index]
        return None

    def add_item(self, item_id: int, count: int) -> int:
        """
        Add items to inventory with automatic stacking.

        Args:
            item_id: Item ID to add
            count: Number of items to add

        Returns:
            Number of items that could not be added (0 = all added)
        """
        if count <= 0:
            return 0

        # Get max_stack from item registry
        item_def = get_item_def(item_id)
        if item_def is None:
            logger.warning(f"[Inventory]Unknown item_id={item_id}, cannot add")
            return count
        max_stack = item_def.get("max_stack", DEFAULT_MAX_STACK)

        remaining = count

        # Phase 1: Try to stack into existing slots with same item
        for i in range(TOTAL_SLOTS):
            if remaining <= 0:
                break
            slot = self._slots[i]
            if slot is not None and slot.item_id == item_id and slot.count < max_stack:
                can_add = min(remaining, max_stack - slot.count)
                slot.count += can_add
                remaining -= can_add
                logger.debug(f"[Inventory]Stacked {can_add} of item {item_id} into slot {i}, now {slot.count}")

        # Phase 2: Fill empty slots
        for i in range(TOTAL_SLOTS):
            if remaining <= 0:
                break
            if self._slots[i] is None:
                can_add = min(remaining, max_stack)
                self._slots[i] = Slot(item_id=item_id, count=can_add)
                remaining -= can_add
                logger.debug(f"[Inventory]Added {can_add} of item {item_id} to empty slot {i}")

        if remaining > 0:
            logger.info(f"[Inventory]Could not add {remaining} of item {item_id} (inventory full)")

        return remaining

    def remove_item(self, item_id: int, count: int) -> bool:
        """
        Remove items from inventory.

        Args:
            item_id: Item ID to remove
            count: Number of items to remove

        Returns:
            True if successfully removed, False if insufficient items
        """
        if count <= 0:
            return True

        # First check if we have enough
        total = self.get_count(item_id)
        if total < count:
            return False

        # Remove items from slots
        remaining = count
        for i in range(TOTAL_SLOTS):
            if remaining <= 0:
                break
            slot = self._slots[i]
            if slot is not None and slot.item_id == item_id:
                if slot.count <= remaining:
                    # Remove entire slot
                    remaining -= slot.count
                    self._slots[i] = None
                    logger.debug(f"[Inventory]Removed entire slot {i} (item {item_id})")
                else:
                    # Partial removal
                    slot.count -= remaining
                    remaining = 0
                    logger.debug(f"[Inventory]Removed {count} of item {item_id} from slot {i}, remaining {slot.count}")

        return True

    def get_count(self, item_id: int) -> int:
        """
        Get total count of an item across all slots.

        Args:
            item_id: Item ID to count

        Returns:
            Total count of the item
        """
        total = 0
        for slot in self._slots:
            if slot is not None and slot.item_id == item_id:
                total += slot.count
        return total

    def set_active_slot(self, slot_index: int) -> bool:
        """
        Set active slot index.

        Args:
            slot_index: Slot index (0-9 for hotbar)

        Returns:
            True if set successfully, False if invalid range
        """
        if 0 <= slot_index < HOTBAR_SLOTS:
            self._active_slot = slot_index
            logger.debug(f"[Inventory]Active slot set to {slot_index}")
            return True
        logger.warning(f"[Inventory]Invalid active slot index: {slot_index} (must be 0-9)")
        return False

    def get_active_item(self) -> Optional[Dict[str, int]]:
        """
        Get the item in the active slot.

        Returns:
            Dict with item_id and count, or None if slot is empty
        """
        slot = self._slots[self._active_slot]
        if slot is None:
            return None
        return {"item_id": slot.item_id, "count": slot.count}

    def serialize(self) -> Dict[str, Any]:
        """
        Serialize inventory to dict.

        Returns:
            Dict with slots list and active_slot
        """
        slots_data = []
        for slot in self._slots:
            if slot is None:
                slots_data.append(None)
            else:
                slots_data.append(slot.to_dict())

        return {
            "slots": slots_data,
            "active_slot": self._active_slot
        }

    @classmethod
    def deserialize(cls, data: Dict[str, Any]) -> 'Inventory':
        """
        Deserialize inventory from dict.

        Args:
            data: Dict with slots list and active_slot

        Returns:
            Restored Inventory instance
        """
        inv = cls()

        # Restore slots
        slots_data = data.get("slots", [])
        for i, slot_data in enumerate(slots_data):
            if i >= TOTAL_SLOTS:
                break
            if slot_data is None:
                inv._slots[i] = None
            else:
                inv._slots[i] = Slot.from_dict(slot_data)

        # Restore active slot
        active_slot = data.get("active_slot", 0)
        if 0 <= active_slot < HOTBAR_SLOTS:
            inv._active_slot = active_slot

        logger.info(f"[Inventory]Deserialized inventory with {sum(1 for s in inv._slots if s is not None)} items")
        return inv

    @classmethod
    def create_default(cls) -> 'Inventory':
        """
        Create inventory with default initial items for new players.

        Returns:
            Inventory with default items
        """
        inv = cls()
        for slot_index, item_id, count in DEFAULT_ITEMS:
            inv._slots[slot_index] = Slot(item_id=item_id, count=count)
        logger.info("[Inventory]Created default inventory with initial items")
        return inv

    @classmethod
    def from_save_data(cls, save_data: Optional[Dict[str, Any]]) -> 'Inventory':
        """
        Create inventory from save data, handling legacy compatibility.

        Args:
            save_data: Player save data dict (may or may not contain 'inventory')

        Returns:
            Inventory instance (default if no inventory data)
        """
        if save_data is None or "inventory" not in save_data:
            logger.info("[Inventory]No inventory data in save, creating default")
            return cls.create_default()

        inventory_data = save_data["inventory"]
        if not inventory_data or inventory_data == "{}":
            logger.info("[Inventory]Empty inventory data, creating default")
            return cls.create_default()

        try:
            import json
            if isinstance(inventory_data, str):
                inventory_data = json.loads(inventory_data)
            return cls.deserialize(inventory_data)
        except (json.JSONDecodeError, KeyError, TypeError) as e:
            logger.error(f"[Inventory]Failed to parse inventory data: {e}")
            return cls.create_default()

    def __repr__(self) -> str:
        item_count = sum(1 for s in self._slots if s is not None)
        return f"Inventory(slots_used={item_count}/{TOTAL_SLOTS}, active_slot={self._active_slot})"
