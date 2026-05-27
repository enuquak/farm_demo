"""
场景注册表
定义所有场景的配置：尺寸、地图生成函数、传送门、出生点
"""
import random
from typing import Dict, List, Any, Optional, Tuple

from ..constants import GroundType, ObjectType


def _generate_farm_map(width: int, height: int) -> Dict[str, List[List[int]]]:
    """
    生成农场地图数据

    Args:
        width: 地图宽度（tile 数量）
        height: 地图高度（tile 数量）

    Returns:
        {"ground": [[int]], "objects": [[int]]}
    """
    random.seed(42)  # 固定种子，保证客户端和服务端生成一致

    ground = [[int(GroundType.GRASS) for _ in range(width)] for _ in range(height)]
    objects = [[int(ObjectType.NONE) for _ in range(width)] for _ in range(height)]

    # 放置一些石头
    for _ in range(30):
        x = random.randint(2, width - 3)
        y = random.randint(2, height - 3)
        objects[y][x] = int(ObjectType.STONE)

    # 放置一些树
    for _ in range(20):
        x = random.randint(1, width - 2)
        y = random.randint(1, height - 2)
        if objects[y][x] == int(ObjectType.NONE):
            objects[y][x] = int(ObjectType.TREE)

    # 农田区域（中央偏上）
    for y in range(10, 20):
        for x in range(15, 35):
            if random.random() < 0.7:
                ground[y][x] = int(GroundType.TILLED)

    # 门口区域（底部中央）- 确保门的位置可通行
    # 门在 (25,48) 和 (26,48)，周围留空
    for x in range(24, 28):
        for y in range(47, 50):
            if y < height and x < width:
                objects[y][x] = int(ObjectType.NONE)

    # 放置门（DOOR_IN = 进入房屋的门）
    if height > 48 and width > 26:
        objects[48][25] = int(ObjectType.DOOR_IN)
        objects[48][26] = int(ObjectType.DOOR_IN)

    return {"ground": ground, "objects": objects}


def _generate_house_map(width: int, height: int) -> Dict[str, List[List[int]]]:
    """
    生成房屋内部地图数据

    Args:
        width: 地图宽度（tile 数量）
        height: 地图高度（tile 数量）

    Returns:
        {"ground": [[int]], "objects": [[int]]}
    """
    ground = [[int(GroundType.WOOD_FLOOR) for _ in range(width)] for _ in range(height)]
    objects = [[int(ObjectType.NONE) for _ in range(width)] for _ in range(height)]

    # 四周墙壁
    for x in range(width):
        ground[0][x] = int(GroundType.WALL)
        ground[height - 1][x] = int(GroundType.WALL)
    for y in range(height):
        ground[y][0] = int(GroundType.WALL)
        ground[y][width - 1] = int(GroundType.WALL)

    # 门（底部中央）- DOOR_OUT = 离开房屋的门
    if width > 5 and height > 7:
        ground[height - 1][5] = int(GroundType.WOOD_FLOOR)  # 门位置用木地板
        objects[height - 1][5] = int(ObjectType.DOOR_OUT)

    # 家具
    # 床（左上角）
    if width > 2 and height > 2:
        objects[1][2] = int(ObjectType.BED)

    # 电视（右上角）
    if width > 7 and height > 2:
        objects[1][width - 2] = int(ObjectType.TV)

    # 炉灶（右侧中间）
    if width > 8 and height > 4:
        objects[4][width - 2] = int(ObjectType.STOVE)

    return {"ground": ground, "objects": objects}


# 场景注册表
SCENE_DEFS: Dict[str, Dict[str, Any]] = {
    "farm": {
        "width": 60,
        "height": 50,
        "generate": lambda: _generate_farm_map(60, 50),
        "portals": [
            {
                "pos": (25, 48),
                "trigger_dir": "down",
                "target": "house",
                "target_portal": "door_out",
            },
            {
                "pos": (26, 48),
                "trigger_dir": "down",
                "target": "house",
                "target_portal": "door_out",
            },
        ],
        "player_spawn": (30, 25),
    },
    "house": {
        "width": 10,
        "height": 8,
        "generate": lambda: _generate_house_map(10, 8),
        "portals": [
            {
                "pos": (5, 7),
                "trigger_dir": "down",
                "target": "farm",
                "target_portal": "door_in",
            },
        ],
        "player_spawn": (5, 6),
    },
}


def get_portal_at(scene_id: str, tile_x: int, tile_y: int, direction: str) -> Optional[Dict[str, Any]]:
    """
    检查指定位置和方向是否有传送门

    Args:
        scene_id: 当前场景 ID
        tile_x: 玩家所在 tile X
        tile_y: 玩家所在 tile Y
        direction: 玩家移动方向

    Returns:
        Portal 配置字典，无匹配时返回 None
    """
    scene_def = SCENE_DEFS.get(scene_id)
    if scene_def is None:
        return None

    for portal in scene_def.get("portals", []):
        px, py = portal["pos"]
        if px == tile_x and py == tile_y and portal["trigger_dir"] == direction:
            return portal

    return None


def get_spawn_for_portal(scene_id: str, portal_id: str) -> Tuple[int, int]:
    """
    获取传送到目标场景后的出生点

    Args:
        scene_id: 目标场景 ID
        portal_id: 目标 Portal ID

    Returns:
        (spawn_x, spawn_y) tile 坐标
    """
    scene_def = SCENE_DEFS.get(scene_id)
    if scene_def is None:
        return (0, 0)

    # 查找目标 portal 附近的出生点
    for portal in scene_def.get("portals", []):
        if portal.get("target_portal") == portal_id:
            # 出生点在 portal 旁边（根据 trigger_dir 偏移）
            px, py = portal["pos"]
            trigger_dir = portal["trigger_dir"]
            if trigger_dir == "up":
                return (px, py + 1)
            elif trigger_dir == "down":
                return (px, py - 1)
            elif trigger_dir == "left":
                return (px + 1, py)
            elif trigger_dir == "right":
                return (px - 1, py)

    # 默认使用场景的 player_spawn
    return scene_def.get("player_spawn", (5, 5))
