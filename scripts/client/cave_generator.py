"""
矿洞地图程序化生成器
使用房间+走廊算法随机生成矿洞地图。
"""
import random
import logging
from typing import List, Tuple, Dict, Any, Optional
from dataclasses import dataclass, field

logger = logging.getLogger("client.cave_generator")


@dataclass
class Room:
    """房间"""
    x: int
    y: int
    w: int
    h: int
    objects: List[Dict[str, Any]] = field(default_factory=list)

    @property
    def center(self) -> Tuple[int, int]:
        return (self.x + self.w // 2, self.y + self.h // 2)

    def intersects(self, other: 'Room', margin: int = 1) -> bool:
        """检查是否与另一个房间重叠（含边距）"""
        return (self.x - margin < other.x + other.w and
                self.x + self.w + margin > other.x and
                self.y - margin < other.y + other.h and
                self.y + self.h + margin > other.y)

    def contains(self, px: int, py: int) -> bool:
        """检查点是否在房间内"""
        return self.x <= px < self.x + self.w and self.y <= py < self.y + self.h

    def add_object(self, obj_type: str, x: int, y: int):
        """添加对象到房间"""
        self.objects.append({"type": obj_type, "x": x, "y": y})


@dataclass
class Corridor:
    """走廊（水平或垂直线段）"""
    x1: int
    y1: int
    x2: int
    y2: int

    def points(self) -> List[Tuple[int, int]]:
        """获取走廊上的所有点"""
        pts = []
        if self.x1 == self.x2:
            for y in range(min(self.y1, self.y2), max(self.y1, self.y2) + 1):
                pts.append((self.x1, y))
        else:
            for x in range(min(self.x1, self.x2), max(self.x1, self.x2) + 1):
                pts.append((x, self.y1))
        return pts


@dataclass
class CaveMap:
    """矿洞地图数据"""
    width: int
    height: int
    map_data: List[List[int]]  # 0=地板, 1=墙壁
    spawn_points: List[Dict[str, int]]
    lights: List[Dict[str, Any]]
    stairs_up: Optional[Tuple[int, int]] = None
    stairs_down: Optional[Tuple[int, int]] = None

    def is_walkable(self, x: int, y: int) -> bool:
        """检查位置是否可行走"""
        if 0 <= x < self.width and 0 <= y < self.height:
            return self.map_data[y][x] == 0
        return False


class CaveGenerator:
    """矿洞地图程序化生成器"""

    def __init__(self, seed: int = None):
        self._rng = random.Random(seed)

    def generate(self, level_config: Dict[str, Any]) -> CaveMap:
        level = level_config["level"]
        map_width, map_height = level_config.get("map_size", [20, 20])
        room_count = level_config.get("room_count", 4)
        is_boss = level_config.get("is_boss_room", False)

        if is_boss:
            return self._generate_boss_room(map_width, map_height, level_config)

        rooms = self._generate_rooms(map_width, map_height, room_count)
        corridors = self._connect_rooms(rooms)
        stairs_up, stairs_down = self._place_stairs(rooms, level_config)
        spawn_points = self._place_spawn_points(rooms, level_config)
        lights = self._place_lights(rooms)
        map_data = self._build_map_data(map_width, map_height, rooms, corridors)

        if stairs_up:
            map_data[stairs_up[1]][stairs_up[0]] = 0
        if stairs_down:
            map_data[stairs_down[1]][stairs_down[0]] = 0

        cave_map = CaveMap(
            width=map_width, height=map_height, map_data=map_data,
            spawn_points=spawn_points, lights=lights,
            stairs_up=stairs_up, stairs_down=stairs_down,
        )

        logger.info(f"[CaveGenerator]Generated level {level}: "
                    f"{map_width}x{map_height}, {len(rooms)} rooms, "
                    f"{len(spawn_points)} spawn points")
        return cave_map

    def _generate_boss_room(self, width: int, height: int,
                            level_config: Dict[str, Any]) -> CaveMap:
        map_data = [[0 for _ in range(width)] for _ in range(height)]
        for x in range(width):
            map_data[0][x] = 1
            map_data[height - 1][x] = 1
        for y in range(height):
            map_data[y][0] = 1
            map_data[y][width - 1] = 1

        entrance = level_config.get("entrance", {"x": width // 2, "y": height - 2})
        stairs_up = (entrance["x"], entrance["y"])
        spawn_points = level_config.get("spawn_points", [{"x": width // 2, "y": height // 3}])
        lights = []

        return CaveMap(width=width, height=height, map_data=map_data,
                       spawn_points=spawn_points, lights=lights,
                       stairs_up=stairs_up, stairs_down=None)

    def _generate_rooms(self, map_width: int, map_height: int,
                        room_count: int) -> List[Room]:
        rooms: List[Room] = []
        max_attempts = 200
        for _ in range(max_attempts):
            if len(rooms) >= room_count:
                break
            w = self._rng.randint(4, 8)
            h = self._rng.randint(4, 8)
            x = self._rng.randint(1, map_width - w - 1)
            y = self._rng.randint(1, map_height - h - 1)
            new_room = Room(x, y, w, h)
            if not any(new_room.intersects(r, margin=2) for r in rooms):
                rooms.append(new_room)
        rooms.sort(key=lambda r: (r.x + r.y))
        return rooms

    def _connect_rooms(self, rooms: List[Room]) -> List[Corridor]:
        corridors: List[Corridor] = []
        for i in range(len(rooms) - 1):
            ax, ay = rooms[i].center
            bx, by = rooms[i + 1].center
            if self._rng.random() < 0.5:
                corridors.append(Corridor(ax, ay, bx, ay))
                corridors.append(Corridor(bx, ay, bx, by))
            else:
                corridors.append(Corridor(ax, ay, ax, by))
                corridors.append(Corridor(ax, by, bx, by))
        return corridors

    def _place_stairs(self, rooms: List[Room],
                      level_config: Dict[str, Any]) -> Tuple[Optional[Tuple[int, int]], Optional[Tuple[int, int]]]:
        if not rooms:
            return None, None
        stairs_up = None
        stairs_down = None
        if "exit_up" in level_config or "entrance" in level_config:
            first_room = rooms[0]
            sx, sy = first_room.center
            stairs_up = (sx, sy)
        if "exit_down" in level_config and len(rooms) > 1:
            last_room = rooms[-1]
            sx, sy = last_room.center
            stairs_down = (sx, sy)
        return stairs_up, stairs_down

    def _place_spawn_points(self, rooms: List[Room],
                            level_config: Dict[str, Any]) -> List[Dict[str, int]]:
        spawn_points: List[Dict[str, int]] = []
        max_monsters = level_config.get("max_monsters", 5)
        for i, room in enumerate(rooms):
            if i == 0:
                continue
            count = min(2, max_monsters - len(spawn_points))
            for _ in range(count):
                if len(spawn_points) >= max_monsters:
                    break
                x = self._rng.randint(room.x + 1, room.x + room.w - 2)
                y = self._rng.randint(room.y + 1, room.y + room.h - 2)
                spawn_points.append({"x": x, "y": y})
        return spawn_points

    def _place_lights(self, rooms: List[Room]) -> List[Dict[str, Any]]:
        lights: List[Dict[str, Any]] = []
        for room in rooms:
            count = self._rng.randint(1, 2)
            for _ in range(count):
                side = self._rng.choice(["top", "bottom", "left", "right"])
                if side == "top":
                    x = self._rng.randint(room.x + 1, room.x + room.w - 2)
                    y = room.y
                elif side == "bottom":
                    x = self._rng.randint(room.x + 1, room.x + room.w - 2)
                    y = room.y + room.h - 1
                elif side == "left":
                    x = room.x
                    y = self._rng.randint(room.y + 1, room.y + room.h - 2)
                else:
                    x = room.x + room.w - 1
                    y = self._rng.randint(room.y + 1, room.y + room.h - 2)
                lights.append({"x": x, "y": y, "radius": 3, "color": [255, 200, 100], "type": "torch"})
        return lights

    def _build_map_data(self, map_width: int, map_height: int,
                        rooms: List[Room], corridors: List[Corridor]) -> List[List[int]]:
        map_data = [[1 for _ in range(map_width)] for _ in range(map_height)]
        for room in rooms:
            for y in range(room.y, room.y + room.h):
                for x in range(room.x, room.x + room.w):
                    if 0 <= x < map_width and 0 <= y < map_height:
                        map_data[y][x] = 0
        for corridor in corridors:
            for x, y in corridor.points():
                if 0 <= x < map_width and 0 <= y < map_height:
                    map_data[y][x] = 0
        return map_data
