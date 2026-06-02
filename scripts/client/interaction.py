"""
交互系统模块
实现物品交互的双层匹配逻辑：优先匹配地物层，无地物时匹配地面层
"""
import logging
from typing import Optional, Dict, Any, Tuple, Union

from .constants import ObjectType, GroundType, OBJECT_PROPERTIES, GROUND_PROPERTIES

logger = logging.getLogger("client.interaction")


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
    "obj:NPC": {
        "tool": None,
        "effect": "start_dialog",
        "description": "与 NPC 对话",
        "interactRange": 1,
    },
    "obj:CAVE_ENTRANCE": {
        "tool": None,
        "effect": "enter_cave",
        "description": "进入矿洞",
        "interactRange": 1,
    },
    "obj:STAIRS_DOWN": {
        "tool": None,
        "effect": "stairs_down",
        "description": "下楼",
        "interactRange": 1,
    },
    "obj:STAIRS_UP": {
        "tool": None,
        "effect": "stairs_up",
        "description": "上楼",
        "interactRange": 1,
    },
    "obj:ANVIL": {
        "tool": None,
        "effect": "craft",
        "description": "打开合成台",
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


def get_interaction_key(map_data, tile_x: int, tile_y: int) -> str:
    """
    获取指定位置的交互匹配键

    Args:
        map_data: 地图数据对象（TileMap 或 TmxMapLoader）
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        交互匹配键（格式："obj:ObjectType" 或 "gnd:GroundType"）
    """
    # 优先匹配地物层
    obj_type = map_data.get_object_type(tile_x, tile_y)
    if obj_type is not None and obj_type != ObjectType.NONE:
        return f"obj:{obj_type.name}"

    # 无地物时匹配地面层
    ground_type = map_data.get_ground_type(tile_x, tile_y)
    if ground_type is not None:
        return f"gnd:{ground_type.name}"

    # 兜底
    return "ANY"


def match_item_effect(
    item_tool: Optional[str],
    map_data,
    tile_x: int,
    tile_y: int,
) -> Tuple[Optional[str], str]:
    """
    匹配物品效果

    Args:
        item_tool: 当前手持工具名称（如 "pickaxe", "axe", "hoe" 等）
        map_data: 地图数据对象（TileMap 或 TmxMapLoader）
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        (effect, description) 元组
        - effect: 效果名称（如 "break_stone", "till" 等），无匹配时为 "none"
        - description: 效果描述
    """
    # 获取交互键
    interaction_key = get_interaction_key(map_data, tile_x, tile_y)

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


def get_object_properties(map_data, tile_x: int, tile_y: int) -> Optional[Dict[str, Any]]:
    """
    获取指定位置的地物属性

    Args:
        map_data: 地图数据对象（TileMap 或 TmxMapLoader）
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        地物属性字典，无地物时返回 None
    """
    obj_type = map_data.get_object_type(tile_x, tile_y)
    if obj_type is None or obj_type == ObjectType.NONE:
        return None
    return OBJECT_PROPERTIES.get(obj_type)


def is_interactable(map_data, tile_x: int, tile_y: int) -> bool:
    """
    检查指定位置是否可交互

    Args:
        map_data: 地图数据对象（TileMap 或 TmxMapLoader）
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        是否可交互
    """
    props = get_object_properties(map_data, tile_x, tile_y)
    if props is None:
        return False
    return props.get("interactable", False)


def get_interact_type(map_data, tile_x: int, tile_y: int) -> Optional[str]:
    """
    获取指定位置的交互类型

    Args:
        map_data: 地图数据对象（TileMap 或 TmxMapLoader）
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        交互类型字符串，无交互时返回 None
    """
    props = get_object_properties(map_data, tile_x, tile_y)
    if props is None:
        return None
    return props.get("interact_type")


def is_portal(map_data, tile_x: int, tile_y: int) -> bool:
    """
    检查指定位置是否是 Portal（门）

    Args:
        map_data: 地图数据对象（TileMap 或 TmxMapLoader）
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        是否是 Portal
    """
    interact_type = get_interact_type(map_data, tile_x, tile_y)
    return interact_type == "portal"


def get_portal_scene(map_data, tile_x: int, tile_y: int) -> Optional[str]:
    """
    获取 Portal 的目标场景

    Args:
        map_data: 地图数据对象（TileMap 或 TmxMapLoader）
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        目标场景 ID，非 Portal 时返回 None
    """
    obj_type = map_data.get_object_type(tile_x, tile_y)
    if obj_type is None:
        return None

    # 根据门类型确定目标场景
    if obj_type == ObjectType.DOOR_IN:
        return "house"
    elif obj_type == ObjectType.DOOR_OUT:
        return "farm"

    return None


def is_npc(map_data, tile_x: int, tile_y: int) -> bool:
    """检查格子上是否有 NPC。"""
    return get_interact_type(map_data, tile_x, tile_y) == "dialog"


def get_npc_at_tile(npc_manager, tile_x: int, tile_y: int):
    """获取格子上的 NPC（需要 NPCManager 实例）。"""
    if npc_manager is None:
        return None
    return npc_manager.get_npc_at_tile(tile_x, tile_y)
