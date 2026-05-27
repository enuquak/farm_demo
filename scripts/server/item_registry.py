"""
Item Registry - Static item definitions and effect mappings.

Data-driven design: new items only need configuration, no logic code changes.
"""


# Item type constants
class ItemType:
    RESOURCE = "RESOURCE"
    TOOL = "TOOL"
    SEED = "SEED"
    FOOD = "FOOD"


# Item definitions: id -> {name, type, max_stack}
ITEM_DEFS = {
    1: {"name": "木材", "type": ItemType.RESOURCE, "max_stack": 99},
    2: {"name": "石头", "type": ItemType.RESOURCE, "max_stack": 99},
    3: {"name": "斧头", "type": ItemType.TOOL, "max_stack": 1},
    4: {"name": "锄头", "type": ItemType.TOOL, "max_stack": 1},
    5: {"name": "种子", "type": ItemType.SEED, "max_stack": 99},
    6: {"name": "面包", "type": ItemType.FOOD, "max_stack": 20},
    7: {"name": "作物", "type": ItemType.RESOURCE, "max_stack": 99},
}

# Item effects: item_id -> {target_key -> effect_dict}
# Target keys:
#   "obj:TYPE"  - match when interacting with an object of TYPE
#   "gnd:TYPE"  - match when interacting with ground of TYPE
#   "ANY"       - fallback match for any target
ITEM_EFFECTS = {
    3: {
        "obj:STONE": {
            "remove_object": True,
            "drops": [{"id": 2, "min": 1, "max": 3}],
            "energy_cost": 4,
        },
    },
    4: {
        "gnd:GRASS": {"set_ground": "TILLED"},
        "gnd:DIRT": {"set_ground": "TILLED"},
    },
    5: {
        "gnd:TILLED": {"place_object": "CROP_GROWING", "consume_self": True},
    },
    6: {
        "ANY": {"consume_self": True, "energy_restore": 15, "interact_range": -1},
    },
}


def get_item_def(item_id):
    """
    Get item definition by ID.

    Args:
        item_id: The item ID to look up.

    Returns:
        dict with keys {name, type, max_stack} if found, None otherwise.
    """
    item = ITEM_DEFS.get(item_id)
    if item is None:
        return None
    return {"name": item["name"], "type": item["type"], "max_stack": item["max_stack"]}


def get_item_effect(item_id, obj_type=None, ground_type=None):
    """
    Get the effect of using an item on a target.

    Lookup priority:
        1. obj:<obj_type>  - if obj_type is provided
        2. gnd:<ground_type> - if ground_type is provided
        3. ANY - fallback

    Args:
        item_id: The item ID being used.
        obj_type: The object type being interacted with (e.g. "STONE"), or None.
        ground_type: The ground type being interacted with (e.g. "GRASS"), or None.

    Returns:
        dict with effect config if found, None otherwise.
        Returned effects have energy_cost defaulting to 0 and interact_range defaulting to 1.
    """
    effects = ITEM_EFFECTS.get(item_id)
    if effects is None:
        return None

    # Priority 1: match object type
    if obj_type is not None:
        key = "obj:" + obj_type
        if key in effects:
            return _apply_defaults(effects[key])

    # Priority 2: match ground type
    if ground_type is not None:
        key = "gnd:" + ground_type
        if key in effects:
            return _apply_defaults(effects[key])

    # Priority 3: fallback to ANY
    if "ANY" in effects:
        return _apply_defaults(effects["ANY"])

    return None


def _apply_defaults(effect):
    """
    Apply default values for optional fields in an effect dict.

    Args:
        effect: The raw effect dict from ITEM_EFFECTS.

    Returns:
        A new dict with energy_cost (default 0) and interact_range (default 1) filled in.
    """
    result = dict(effect)
    if "energy_cost" not in result:
        result["energy_cost"] = 0
    if "interact_range" not in result:
        result["interact_range"] = 1
    return result
