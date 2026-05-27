"""
Unit tests for Inventory module.
"""
import unittest
import sys
import os

# Add parent directories to path for imports
sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'server'))

from inventory import Inventory, Slot, TOTAL_SLOTS, HOTBAR_SLOTS, DEFAULT_ITEMS


class TestInventoryInit(unittest.TestCase):
    """Test inventory initialization."""

    def test_empty_inventory(self):
        """New inventory should have 30 empty slots and active_slot=0."""
        inv = Inventory()
        self.assertEqual(len(inv.slots), TOTAL_SLOTS)
        self.assertTrue(all(slot is None for slot in inv.slots))
        self.assertEqual(inv.active_slot, 0)

    def test_default_inventory(self):
        """Default inventory should have initial items."""
        inv = Inventory.create_default()
        # slot[0] = axe x1
        self.assertIsNotNone(inv.get_slot(0))
        self.assertEqual(inv.get_slot(0).item_id, 3)  # axe
        self.assertEqual(inv.get_slot(0).count, 1)
        # slot[1] = hoe x1
        self.assertIsNotNone(inv.get_slot(1))
        self.assertEqual(inv.get_slot(1).item_id, 4)  # hoe
        self.assertEqual(inv.get_slot(1).count, 1)
        # slot[2] = seeds x5
        self.assertIsNotNone(inv.get_slot(2))
        self.assertEqual(inv.get_slot(2).item_id, 5)  # seeds
        self.assertEqual(inv.get_slot(2).count, 5)
        # slot[3] = bread x3
        self.assertIsNotNone(inv.get_slot(3))
        self.assertEqual(inv.get_slot(3).item_id, 6)  # bread
        self.assertEqual(inv.get_slot(3).count, 3)
        # Other slots should be empty
        for i in range(4, TOTAL_SLOTS):
            self.assertIsNone(inv.get_slot(i))


class TestAddItem(unittest.TestCase):
    """Test add_item method."""

    def test_add_to_empty_slot(self):
        """Adding item to empty inventory should find empty slot."""
        inv = Inventory()
        remaining = inv.add_item(1, 5)  # wood x5
        self.assertEqual(remaining, 0)
        # Find the slot with wood
        found = False
        for slot in inv.slots:
            if slot is not None and slot.item_id == 1:
                self.assertEqual(slot.count, 5)
                found = True
                break
        self.assertTrue(found)

    def test_stack_to_existing_slot(self):
        """Adding item should stack to existing slot with same item."""
        inv = Inventory()
        inv._slots[2] = Slot(item_id=5, count=3)  # seeds x3
        remaining = inv.add_item(5, 2)  # add seeds x2
        self.assertEqual(remaining, 0)
        self.assertEqual(inv.get_slot(2).count, 5)  # 3 + 2 = 5

    def test_stack_overflow_to_new_slot(self):
        """Adding items should overflow to new slot when max_stack reached."""
        inv = Inventory()
        inv._slots[2] = Slot(item_id=5, count=98)  # seeds x98 (max_stack=99)
        remaining = inv.add_item(5, 5)  # add seeds x5
        self.assertEqual(remaining, 0)
        # slot[2] should be full (99)
        self.assertEqual(inv.get_slot(2).count, 99)
        # Overflow should be in another slot (4 seeds)
        total = inv.get_count(5)
        self.assertEqual(total, 103)  # 98 + 5 = 103
        # Verify slot[2] is maxed and overflow exists
        self.assertEqual(inv.get_slot(2).count, 99)

    def test_inventory_full(self):
        """Adding item when inventory is full should return remaining count."""
        inv = Inventory()
        # Fill all slots with different items
        for i in range(TOTAL_SLOTS):
            inv._slots[i] = Slot(item_id=i+100, count=1)
        remaining = inv.add_item(2, 3)  # try to add stone x3
        self.assertEqual(remaining, 3)  # all 3 should remain

    def test_partial_add(self):
        """Adding more than available space should return remaining."""
        inv = Inventory()
        # Fill 29 slots, leave 1 empty
        for i in range(TOTAL_SLOTS - 1):
            inv._slots[i] = Slot(item_id=i+100, count=1)
        # Empty slot can hold max_stack=99 of wood
        remaining = inv.add_item(1, 100)  # wood x100
        self.assertEqual(remaining, 1)  # 1 remaining (99 in slot)
        # Find the wood slot
        for slot in inv.slots:
            if slot is not None and slot.item_id == 1:
                self.assertEqual(slot.count, 99)
                break


class TestRemoveItem(unittest.TestCase):
    """Test remove_item method."""

    def test_remove_from_single_slot(self):
        """Removing items from single slot should work."""
        inv = Inventory()
        inv._slots[3] = Slot(item_id=6, count=3)  # bread x3
        result = inv.remove_item(6, 1)  # remove 1 bread
        self.assertTrue(result)
        self.assertEqual(inv.get_slot(3).count, 2)  # 3 - 1 = 2

    def test_remove_across_slots(self):
        """Removing items should work across multiple slots."""
        inv = Inventory()
        inv._slots[3] = Slot(item_id=6, count=2)  # bread x2
        inv._slots[7] = Slot(item_id=6, count=1)  # bread x1
        result = inv.remove_item(6, 3)  # remove 3 bread
        self.assertTrue(result)
        self.assertIsNone(inv.get_slot(3))  # empty
        self.assertIsNone(inv.get_slot(7))  # empty

    def test_remove_insufficient(self):
        """Removing more than available should return False."""
        inv = Inventory()
        inv._slots[3] = Slot(item_id=6, count=2)  # bread x2
        result = inv.remove_item(6, 3)  # try to remove 3
        self.assertFalse(result)
        # Inventory should be unchanged
        self.assertEqual(inv.get_slot(3).count, 2)

    def test_remove_nonexistent(self):
        """Removing item that doesn't exist should return False."""
        inv = Inventory()
        result = inv.remove_item(1, 1)  # try to remove wood
        self.assertFalse(result)


class TestGetCount(unittest.TestCase):
    """Test get_count method."""

    def test_count_single_slot(self):
        """Counting item in single slot should work."""
        inv = Inventory()
        inv._slots[2] = Slot(item_id=5, count=5)  # seeds x5
        self.assertEqual(inv.get_count(5), 5)

    def test_count_multiple_slots(self):
        """Counting item across multiple slots should sum correctly."""
        inv = Inventory()
        inv._slots[2] = Slot(item_id=5, count=5)  # seeds x5
        inv._slots[8] = Slot(item_id=5, count=3)  # seeds x3
        self.assertEqual(inv.get_count(5), 8)

    def test_count_nonexistent(self):
        """Counting item that doesn't exist should return 0."""
        inv = Inventory()
        self.assertEqual(inv.get_count(1), 0)  # no wood


class TestActiveSlot(unittest.TestCase):
    """Test activeSlot management."""

    def test_set_active_slot(self):
        """Setting active slot should work for valid range."""
        inv = Inventory()
        self.assertTrue(inv.set_active_slot(3))
        self.assertEqual(inv.active_slot, 3)

    def test_set_active_slot_invalid(self):
        """Setting active slot outside 0-9 should fail."""
        inv = Inventory()
        inv.set_active_slot(5)  # set to 5 first
        self.assertFalse(inv.set_active_slot(15))  # invalid
        self.assertEqual(inv.active_slot, 5)  # unchanged

    def test_get_active_item(self):
        """Getting active item should return item in active slot."""
        inv = Inventory()
        inv._slots[2] = Slot(item_id=5, count=5)  # seeds x5
        inv.set_active_slot(2)
        active = inv.get_active_item()
        self.assertIsNotNone(active)
        self.assertEqual(active["item_id"], 5)
        self.assertEqual(active["count"], 5)

    def test_get_active_item_empty(self):
        """Getting active item from empty slot should return None."""
        inv = Inventory()
        inv.set_active_slot(4)
        self.assertIsNone(inv.get_active_item())


class TestSerialization(unittest.TestCase):
    """Test serialization and deserialization."""

    def test_serialize_empty(self):
        """Serializing empty inventory should produce correct dict."""
        inv = Inventory()
        data = inv.serialize()
        self.assertEqual(len(data["slots"]), TOTAL_SLOTS)
        self.assertTrue(all(slot is None for slot in data["slots"]))
        self.assertEqual(data["active_slot"], 0)

    def test_serialize_with_items(self):
        """Serializing inventory with items should preserve data."""
        inv = Inventory()
        inv._slots[0] = Slot(item_id=3, count=1)  # axe
        inv._slots[2] = Slot(item_id=5, count=5)  # seeds
        inv.set_active_slot(2)

        data = inv.serialize()
        self.assertEqual(data["slots"][0], {"item_id": 3, "count": 1})
        self.assertEqual(data["slots"][2], {"item_id": 5, "count": 5})
        self.assertIsNone(data["slots"][1])
        self.assertEqual(data["active_slot"], 2)

    def test_deserialize(self):
        """Deserializing should restore inventory state."""
        data = {
            "slots": [
                {"item_id": 3, "count": 1},  # axe
                None,
                {"item_id": 5, "count": 5},  # seeds
            ] + [None] * 27,
            "active_slot": 2
        }
        inv = Inventory.deserialize(data)
        self.assertEqual(inv.get_slot(0).item_id, 3)
        self.assertEqual(inv.get_slot(0).count, 1)
        self.assertIsNone(inv.get_slot(1))
        self.assertEqual(inv.get_slot(2).item_id, 5)
        self.assertEqual(inv.get_slot(2).count, 5)
        self.assertEqual(inv.active_slot, 2)

    def test_roundtrip(self):
        """Serialize then deserialize should preserve state."""
        inv = Inventory.create_default()
        inv.set_active_slot(3)
        data = inv.serialize()
        restored = Inventory.deserialize(data)

        # Check all slots match
        for i in range(TOTAL_SLOTS):
            orig = inv.get_slot(i)
            rest = restored.get_slot(i)
            if orig is None:
                self.assertIsNone(rest)
            else:
                self.assertEqual(orig.item_id, rest.item_id)
                self.assertEqual(orig.count, rest.count)
        self.assertEqual(restored.active_slot, 3)


class TestLegacyCompatibility(unittest.TestCase):
    """Test legacy save compatibility."""

    def test_no_inventory_field(self):
        """Missing inventory field should create default."""
        inv = Inventory.from_save_data({"level": 1, "gold": 100})
        # Should have default items
        self.assertIsNotNone(inv.get_slot(0))
        self.assertEqual(inv.get_slot(0).item_id, 3)  # axe

    def test_none_save_data(self):
        """None save data should create default."""
        inv = Inventory.from_save_data(None)
        self.assertIsNotNone(inv.get_slot(0))

    def test_empty_inventory_string(self):
        """Empty inventory string should create default."""
        inv = Inventory.from_save_data({"inventory": "{}"})
        self.assertIsNotNone(inv.get_slot(0))

    def test_valid_inventory_string(self):
        """Valid JSON inventory string should deserialize correctly."""
        import json
        inventory_data = {
            "slots": [{"item_id": 1, "count": 10}] + [None] * 29,
            "active_slot": 0
        }
        save_data = {"inventory": json.dumps(inventory_data)}
        inv = Inventory.from_save_data(save_data)
        self.assertEqual(inv.get_slot(0).item_id, 1)
        self.assertEqual(inv.get_slot(0).count, 10)


if __name__ == "__main__":
    unittest.main()
