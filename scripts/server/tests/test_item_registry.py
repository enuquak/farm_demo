"""
Unit tests for item_registry module.

Tests:
TC-001: Item type constants defined
TC-002: All 7 items defined in ITEM_DEFS
TC-003: get_item_def returns correct dict for existing item
TC-004: get_item_def returns None for non-existing item
TC-005: Axe effect on stone (obj:STONE)
TC-006: Hoe effect on grass (gnd:GRASS)
TC-007: Hoe effect on dirt (gnd:DIRT)
TC-008: Seed effect on tilled ground (gnd:TILLED)
TC-009: Bread effect fallback (ANY)
TC-010: No effect for wood on grass
TC-011: get_item_effect priority: obj over gnd
TC-012: get_item_effect with no obj, match gnd
TC-013: get_item_effect fallback to ANY
TC-014: energy_cost default is 0
TC-015: interact_range default is 1
TC-016: energy_cost from effect (axe on stone = 4)
TC-017: interact_range from effect (bread = -1)
"""

import os
import sys

# Add scripts/server to path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SERVER_DIR = os.path.join(SCRIPT_DIR, "..")
sys.path.insert(0, SERVER_DIR)

from item_registry import ItemType, ITEM_DEFS, ITEM_EFFECTS, get_item_def, get_item_effect


def test_item_type_constants():
    """TC-001: Item type constants defined."""
    print("TC-001: Item type constants defined ... ", end="")
    assert ItemType.RESOURCE == "RESOURCE"
    assert ItemType.TOOL == "TOOL"
    assert ItemType.SEED == "SEED"
    assert ItemType.FOOD == "FOOD"
    print("PASS")


def test_all_items_defined():
    """TC-002: All 7 items defined in ITEM_DEFS."""
    print("TC-002: All 7 items defined in ITEM_DEFS ... ", end="")
    expected_ids = [1, 2, 3, 4, 5, 6, 7]
    for item_id in expected_ids:
        assert item_id in ITEM_DEFS, f"Item {item_id} missing from ITEM_DEFS"
    assert len(ITEM_DEFS) == 7, f"Expected 7 items, got {len(ITEM_DEFS)}"
    print("PASS")


def test_item_defs_values():
    """TC-002b: Item definitions have correct values."""
    print("TC-002b: Item definitions have correct values ... ", end="")
    expected = [
        (1, "木材", "RESOURCE", 99),
        (2, "石头", "RESOURCE", 99),
        (3, "斧头", "TOOL", 1),
        (4, "锄头", "TOOL", 1),
        (5, "种子", "SEED", 99),
        (6, "面包", "FOOD", 20),
        (7, "作物", "RESOURCE", 99),
    ]
    for item_id, name, item_type, max_stack in expected:
        item = ITEM_DEFS[item_id]
        assert item["name"] == name, f"Item {item_id}: expected name={name}, got {item['name']}"
        assert item["type"] == item_type, f"Item {item_id}: expected type={item_type}, got {item['type']}"
        assert item["max_stack"] == max_stack, f"Item {item_id}: expected max_stack={max_stack}, got {item['max_stack']}"
    print("PASS")


def test_get_item_def_existing():
    """TC-003: get_item_def returns correct dict for existing item."""
    print("TC-003: get_item_def returns correct dict for existing item ... ", end="")
    result = get_item_def(3)
    assert result is not None, "get_item_def(3) returned None"
    assert result["name"] == "斧头"
    assert result["type"] == "TOOL"
    assert result["max_stack"] == 1
    print("PASS")


def test_get_item_def_non_existing():
    """TC-004: get_item_def returns None for non-existing item."""
    print("TC-004: get_item_def returns None for non-existing item ... ", end="")
    result = get_item_def(999)
    assert result is None, f"get_item_def(999) returned {result}, expected None"
    print("PASS")


def test_axe_on_stone():
    """TC-005: Axe effect on stone (obj:STONE)."""
    print("TC-005: Axe effect on stone (obj:STONE) ... ", end="")
    effect = get_item_effect(3, obj_type="STONE", ground_type="GRASS")
    assert effect is not None, "Axe on stone returned None"
    assert effect["remove_object"] is True
    assert len(effect["drops"]) == 1
    assert effect["drops"][0]["id"] == 2
    assert effect["drops"][0]["min"] == 1
    assert effect["drops"][0]["max"] == 3
    assert effect["energy_cost"] == 4
    print("PASS")


def test_hoe_on_grass():
    """TC-006: Hoe effect on grass (gnd:GRASS)."""
    print("TC-006: Hoe effect on grass (gnd:GRASS) ... ", end="")
    effect = get_item_effect(4, obj_type=None, ground_type="GRASS")
    assert effect is not None, "Hoe on grass returned None"
    assert effect["set_ground"] == "TILLED"
    print("PASS")


def test_hoe_on_dirt():
    """TC-007: Hoe effect on dirt (gnd:DIRT)."""
    print("TC-007: Hoe effect on dirt (gnd:DIRT) ... ", end="")
    effect = get_item_effect(4, obj_type=None, ground_type="DIRT")
    assert effect is not None, "Hoe on dirt returned None"
    assert effect["set_ground"] == "TILLED"
    print("PASS")


def test_seed_on_tilled():
    """TC-008: Seed effect on tilled ground (gnd:TILLED)."""
    print("TC-008: Seed effect on tilled ground (gnd:TILLED) ... ", end="")
    effect = get_item_effect(5, obj_type=None, ground_type="TILLED")
    assert effect is not None, "Seed on tilled returned None"
    assert effect["place_object"] == "CROP_GROWING"
    assert effect["consume_self"] is True
    print("PASS")


def test_bread_any():
    """TC-009: Bread effect fallback (ANY)."""
    print("TC-009: Bread effect fallback (ANY) ... ", end="")
    effect = get_item_effect(6, obj_type=None, ground_type="GRASS")
    assert effect is not None, "Bread ANY returned None"
    assert effect["consume_self"] is True
    assert effect["energy_restore"] == 15
    print("PASS")


def test_no_effect_wood_on_grass():
    """TC-010: No effect for wood on grass."""
    print("TC-010: No effect for wood on grass ... ", end="")
    effect = get_item_effect(1, obj_type=None, ground_type="GRASS")
    assert effect is None, f"Wood on grass returned {effect}, expected None"
    print("PASS")


def test_priority_obj_over_gnd():
    """TC-011: get_item_effect priority: obj over gnd."""
    print("TC-011: get_item_effect priority: obj over gnd ... ", end="")
    # Axe with both obj:STONE and gnd:GRASS -> should match obj:STONE
    effect = get_item_effect(3, obj_type="STONE", ground_type="GRASS")
    assert effect is not None
    assert effect["remove_object"] is True  # obj:STONE effect
    assert "set_ground" not in effect       # not gnd:GRASS effect
    print("PASS")


def test_no_obj_match_gnd():
    """TC-012: get_item_effect with no obj, match gnd."""
    print("TC-012: get_item_effect with no obj, match gnd ... ", end="")
    # Hoe with no obj, only ground -> should match gnd:GRASS
    effect = get_item_effect(4, obj_type=None, ground_type="GRASS")
    assert effect is not None
    assert effect["set_ground"] == "TILLED"
    print("PASS")


def test_fallback_to_any():
    """TC-013: get_item_effect fallback to ANY."""
    print("TC-013: get_item_effect fallback to ANY ... ", end="")
    # Bread with no matching obj or gnd -> should match ANY
    effect = get_item_effect(6, obj_type=None, ground_type=None)
    assert effect is not None
    assert effect["consume_self"] is True
    assert effect["energy_restore"] == 15
    print("PASS")


def test_energy_cost_default():
    """TC-014: energy_cost default is 0."""
    print("TC-014: energy_cost default is 0 ... ", end="")
    # Hoe on grass has no energy_cost in definition
    effect = get_item_effect(4, obj_type=None, ground_type="GRASS")
    assert effect is not None
    assert effect["energy_cost"] == 0, f"Expected energy_cost=0, got {effect['energy_cost']}"
    print("PASS")


def test_interact_range_default():
    """TC-015: interact_range default is 1."""
    print("TC-015: interact_range default is 1 ... ", end="")
    # Hoe on grass has no interact_range in definition
    effect = get_item_effect(4, obj_type=None, ground_type="GRASS")
    assert effect is not None
    assert effect["interact_range"] == 1, f"Expected interact_range=1, got {effect['interact_range']}"
    print("PASS")


def test_energy_cost_from_effect():
    """TC-016: energy_cost from effect (axe on stone = 4)."""
    print("TC-016: energy_cost from effect (axe on stone = 4) ... ", end="")
    effect = get_item_effect(3, obj_type="STONE")
    assert effect is not None
    assert effect["energy_cost"] == 4, f"Expected energy_cost=4, got {effect['energy_cost']}"
    print("PASS")


def test_interact_range_from_effect():
    """TC-017: interact_range from effect (bread = -1)."""
    print("TC-017: interact_range from effect (bread = -1) ... ", end="")
    effect = get_item_effect(6, obj_type=None, ground_type=None)
    assert effect is not None
    assert effect["interact_range"] == -1, f"Expected interact_range=-1, got {effect['interact_range']}"
    print("PASS")


def main():
    print("=" * 60)
    print("Item Registry Unit Tests")
    print("=" * 60)

    tests = [
        test_item_type_constants,
        test_all_items_defined,
        test_item_defs_values,
        test_get_item_def_existing,
        test_get_item_def_non_existing,
        test_axe_on_stone,
        test_hoe_on_grass,
        test_hoe_on_dirt,
        test_seed_on_tilled,
        test_bread_any,
        test_no_effect_wood_on_grass,
        test_priority_obj_over_gnd,
        test_no_obj_match_gnd,
        test_fallback_to_any,
        test_energy_cost_default,
        test_interact_range_default,
        test_energy_cost_from_effect,
        test_interact_range_from_effect,
    ]

    passed = 0
    failed = 0
    errors = []

    for test_fn in tests:
        try:
            test_fn()
            passed += 1
        except Exception as e:
            failed += 1
            errors.append((test_fn.__doc__, str(e)))
            print(f"FAIL: {e}")

    print("=" * 60)
    print(f"Results: {passed} passed, {failed} failed out of {len(tests)} tests")
    if errors:
        print("\nFailed tests:")
        for name, err in errors:
            print(f"  {name}: {err}")
    print("=" * 60)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
