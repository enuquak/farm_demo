"""
交互系统模块
实现物品交互的双层匹配逻辑：优先匹配地物层，无地物时匹配地面层
"""
from typing import Optional, Dict, Any, Tuple

from .constants import ObjectType, GroundType, OBJECT_PROPERTIES, GROUND_PROPERTIES
from .tile_map import TileMap


# 物品效果匹配表
# 键格式：
#   - "obj:ObjectType" 匹配地物层
#   - "gnd:GroundType" 匹配地面层
#   - "ANY" 兜底匹配
ITEM_EFFECTS: Dict[str, Dict[str, Any]] = {
    # 地物层匹配
    "obj:STONE": {
        "tool": "pickaxe",
        "effect": "break_stone",
        "description": "使用镐子破碎石头",
        "interactRange": 1,
    },
    "obj:TREE": {
        "tool": "axe",
        "effect": "chop_tree",
        "description": "使用斧头砍树",
        "interactRange": 1,
    },
    "obj:CROP_READY": {
        "tool": None,  # 空手即可
        "effect": "harvest",
        "description": "收获成熟作物",
        "interactRange": 1,
    },
    "obj:DOOR_IN": {
        "tool": None,
        "effect": "enter_portal",
        "description": "进入门",
        "interactRange": 1,
    },
    "obj:DOOR_OUT": {
        "tool": None,
        "effect": "exit_portal",
        "description": "离开门",
        "interactRange": 1,
    },
    "obj:BED": {
        "tool": None,
        "effect": "sleep",
        "description": "睡觉",
        "interactRange": 1,
    },
    "obj:TV": {
        "tool": None,
        "effect": "watch",
        "description": "看电视",
        "interactRange": 1,
    },
    "obj:STOVE": {
        "tool": None,
        "effect": "cook",
        "description": "烹饪",
        "interactRange": 1,
    },

    # 地面层匹配
    "gnd:GRASS": {
        "tool": "hoe",
        "effect": "till",
        "description": "使用锄头翻耕草地",
        "interactRange": 1,
    },
    "gnd:TILLED": {
        "tool": "seed",
        "effect": "plant",
        "description": "种植种子",
        "interactRange": 1,
    },
    "gnd:WATER": {
        "tool": "watering_can",
        "effect": "water",
        "description": "浇水",
        "interactRange": 1,
    },

    # 兜底匹配
    "ANY": {
        "tool": None,
        "effect": "none",
        "description": "无效果",
        "interactRange": -1,  # 食物无距离限制
    },
}


def get_interaction_key(tile_map: TileMap, tile_x: int, tile_y: int) -> str:
    """
    获取指定位置的交互匹配键

    Args:
        tile_map: TileMap 数据
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        交互匹配键（格式："obj:ObjectType" 或 "gnd:GroundType"）
    """
    # 优先匹配地物层
    obj_type = tile_map.get_object(tile_x, tile_y)
    if obj_type is not None and obj_type != ObjectType.NONE:
        return f"obj:{obj_type.name}"

    # 无地物时匹配地面层
    ground_type = tile_map.get_ground(tile_x, tile_y)
    if ground_type is not None:
        return f"gnd:{ground_type.name}"

    # 兜底
    return "ANY"


def match_item_effect(
    item_tool: Optional[str],
    tile_map: TileMap,
    tile_x: int,
    tile_y: int,
) -> Tuple[Optional[str], str]:
    """
    匹配物品效果

    Args:
        item_tool: 当前手持工具名称（如 "pickaxe", "axe", "hoe" 等）
        tile_map: TileMap 数据
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        (effect, description) 元组
        - effect: 效果名称（如 "break_stone", "till" 等），无匹配时为 "none"
        - description: 效果描述
    """
    # 获取交互键
    interaction_key = get_interaction_key(tile_map, tile_x, tile_y)

    # 查找匹配的效果
    effect_data = ITEM_EFFECTS.get(interaction_key)
    if effect_data is None:
        # 兜底到 ANY
        effect_data = ITEM_EFFECTS.get("ANY", {"effect": "none", "description": "无效果"})

    # 检查工具匹配
    required_tool = effect_data.get("tool")
    if required_tool is not None and item_tool != required_tool:
        # 工具不匹配，尝试兜底
        effect_data = ITEM_EFFECTS.get("ANY", {"effect": "none", "description": "无效果"})

    return effect_data.get("effect", "none"), effect_data.get("description", "无效果")


def get_object_properties(tile_map: TileMap, tile_x: int, tile_y: int) -> Optional[Dict[str, Any]]:
    """
    获取指定位置的地物属性

    Args:
        tile_map: TileMap 数据
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        地物属性字典，无地物时返回 None
    """
    obj_type = tile_map.get_object(tile_x, tile_y)
    if obj_type is None or obj_type == ObjectType.NONE:
        return None
    return OBJECT_PROPERTIES.get(obj_type)


def is_interactable(tile_map: TileMap, tile_x: int, tile_y: int) -> bool:
    """
    检查指定位置是否可交互

    Args:
        tile_map: TileMap 数据
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        是否可交互
    """
    props = get_object_properties(tile_map, tile_x, tile_y)
    if props is None:
        return False
    return props.get("interactable", False)


def get_interact_type(tile_map: TileMap, tile_x: int, tile_y: int) -> Optional[str]:
    """
    获取指定位置的交互类型

    Args:
        tile_map: TileMap 数据
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        交互类型字符串，无交互时返回 None
    """
    props = get_object_properties(tile_map, tile_x, tile_y)
    if props is None:
        return None
    return props.get("interact_type")
