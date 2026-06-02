# 矿洞场景系统实现计划（Phase 2）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现矿洞场景系统，包含程序化地图生成、多层光照、怪物刷新、场景切换，让玩家从农场进入矿洞逐层探索战斗。

**Architecture:** 客户端 CaveManager 协调地图生成和光照渲染，CaveGenerator 使用房间+走廊算法程序化生成地图，CaveLighting 实现多层光照效果。服务器端 CaveSpawner 管理矿洞怪物生命周期，扩展 GameSceneManager 支持矿洞场景切换。复用现有 SceneChangeReq/Resp 消息协议。

**Tech Stack:** Python + PyGame (客户端), C++ + libevent + nlohmann/json (服务器), JSON 数据配置

---

## 文件结构

### 新增文件

| 文件 | 职责 |
|------|------|
| `scripts/client/data/cave_levels.json` | 矿洞层配置（层数、怪物、光照、刷新参数） |
| `scripts/client/cave_generator.py` | 程序化地图生成器（房间+走廊算法） |
| `scripts/client/cave_lighting.py` | 矿洞光照系统（环境光、玩家光照、静态光源、动态效果） |
| `scripts/client/cave_manager.py` | 矿洞场景管理器（协调生成、光照、场景状态） |
| `scripts/server/game_server/src/cave_spawner.h` | 服务器怪物刷新器头文件 |
| `scripts/server/game_server/src/cave_spawner.cpp` | 服务器怪物刷新器实现 |

### 修改文件

| 文件 | 修改内容 |
|------|---------|
| `scripts/client/scene/scene_defs.py` | 添加矿洞场景定义和传送门 |
| `scripts/client/game_scene.py` | 集成 CaveManager，场景切换时清空怪物 |
| `scripts/client/game_renderer.py` | 矿洞场景光照渲染 |
| `scripts/client/network_dispatcher.py` | 场景切换时清空怪物 |
| `scripts/client/interaction.py` | 矿洞传送门场景映射 |
| `scripts/server/game_server/src/scene_state.h` | 添加矿洞场景数据字段 |
| `scripts/server/game_server/src/scene_state.cpp` | 矿洞场景序列化 |
| `scripts/server/game_server/src/game_scene_manager.h` | 添加 CaveSpawner 成员 |
| `scripts/server/game_server/src/game_scene_manager.cpp` | 支持矿洞场景创建和切换 |
| `scripts/server/game_server/src/game_server.h` | 添加 CaveSpawner 成员 |
| `scripts/server/game_server/src/game_server.cpp` | 初始化 CaveSpawner，注册消息 handler |
| `scripts/server/game_server/src/monster_manager.h` | 添加多场景怪物管理方法 |
| `scripts/server/game_server/src/monster_manager.cpp` | 多场景怪物管理实现 |
| `scripts/server/game_server/src/player.h` | PlayerBizData 添加 cave_level 字段 |
| `scripts/server/game_server/src/player.cpp` | cave_level 序列化 |
| `scripts/server/game_server/CMakeLists.txt` | 添加 cave_spawner.cpp |
| `scripts/client/test_cave_generator.py` | CaveGenerator 单元测试 |

---

## Phase 2: 矿洞场景系统

### Task 1: 矿洞层配置数据文件

**Files:**
- Create: `scripts/client/data/cave_levels.json`

- [ ] **Step 1: 创建 cave_levels.json**

创建文件 `scripts/client/data/cave_levels.json`：

```json
{
  "cave_levels": [
    {
      "level": 1,
      "name": "矿洞入口",
      "scene_id": "cave_1",
      "max_monsters": 5,
      "monster_types": ["slime"],
      "spawn_points": [
        {"x": 10, "y": 8},
        {"x": 15, "y": 12},
        {"x": 8, "y": 15}
      ],
      "entrance": {"x": 2, "y": 18},
      "exit_down": {"x": 18, "y": 2},
      "map_size": [20, 20],
      "room_count": 4,
      "ambient_color": [40, 30, 30],
      "light_radius": 5,
      "boss_lighting": null,
      "spawn_config": {
        "respawn_delay": 10.0,
        "spawn_check_interval": 5.0,
        "spawn_radius": 2
      }
    },
    {
      "level": 2,
      "name": "矿洞深层",
      "scene_id": "cave_2",
      "max_monsters": 8,
      "monster_types": ["slime", "bat"],
      "spawn_points": [
        {"x": 5, "y": 5},
        {"x": 15, "y": 8},
        {"x": 10, "y": 15},
        {"x": 18, "y": 12}
      ],
      "entrance": {"x": 2, "y": 18},
      "exit_down": {"x": 18, "y": 2},
      "exit_up": {"x": 2, "y": 18},
      "map_size": [25, 25],
      "room_count": 6,
      "ambient_color": [30, 25, 25],
      "light_radius": 4,
      "boss_lighting": null,
      "spawn_config": {
        "respawn_delay": 8.0,
        "spawn_check_interval": 4.0,
        "spawn_radius": 2
      }
    },
    {
      "level": 3,
      "name": "骷髅大厅",
      "scene_id": "cave_3",
      "max_monsters": 10,
      "monster_types": ["bat", "skeleton"],
      "spawn_points": [
        {"x": 8, "y": 8},
        {"x": 12, "y": 8},
        {"x": 10, "y": 12},
        {"x": 15, "y": 15},
        {"x": 5, "y": 15}
      ],
      "entrance": {"x": 2, "y": 18},
      "exit_down": {"x": 18, "y": 2},
      "exit_up": {"x": 2, "y": 18},
      "map_size": [30, 30],
      "room_count": 8,
      "ambient_color": [25, 20, 20],
      "light_radius": 3,
      "boss_lighting": null,
      "spawn_config": {
        "respawn_delay": 6.0,
        "spawn_check_interval": 3.0,
        "spawn_radius": 3
      }
    },
    {
      "level": 10,
      "name": "骷髅王座",
      "scene_id": "cave_boss",
      "max_monsters": 1,
      "monster_types": ["boss_skeleton_king"],
      "spawn_points": [
        {"x": 10, "y": 5}
      ],
      "entrance": {"x": 10, "y": 18},
      "exit_up": {"x": 10, "y": 18},
      "map_size": [20, 20],
      "room_count": 1,
      "is_boss_room": true,
      "ambient_color": [20, 15, 15],
      "light_radius": 6,
      "boss_lighting": {
        "color": [150, 50, 50],
        "pulse_speed": 0.5,
        "pulse_range": [0.8, 1.2]
      },
      "spawn_config": {
        "respawn_delay": 0,
        "spawn_check_interval": 0,
        "spawn_radius": 0
      }
    }
  ]
}
```

- [ ] **Step 2: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/client/data/cave_levels.json
git commit -m "feat(cave): add cave level configuration data

Add cave_levels.json with 4 cave levels (entrance, deep, skeleton
hall, boss throne). Each level defines monsters, spawn points,
lighting, and map generation parameters."
```

---

### Task 2: 程序化地图生成器

**Files:**
- Create: `scripts/client/cave_generator.py`
- Create: `scripts/client/test_cave_generator.py`

- [ ] **Step 1: 创建 cave_generator.py**

创建文件 `scripts/client/cave_generator.py`：

```python
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
            # 垂直走廊
            for y in range(min(self.y1, self.y2), max(self.y1, self.y2) + 1):
                pts.append((self.x1, y))
        else:
            # 水平走廊
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
        """
        生成矿洞地图

        Args:
            level_config: 矿洞层配置（来自 cave_levels.json）

        Returns:
            CaveMap 对象
        """
        level = level_config["level"]
        map_width, map_height = level_config.get("map_size", [20, 20])
        room_count = level_config.get("room_count", 4)
        is_boss = level_config.get("is_boss_room", False)

        if is_boss:
            return self._generate_boss_room(map_width, map_height, level_config)

        # 生成房间
        rooms = self._generate_rooms(map_width, map_height, room_count)

        # 连接房间
        corridors = self._connect_rooms(rooms)

        # 放置楼梯
        stairs_up, stairs_down = self._place_stairs(rooms, level_config)

        # 放置刷新点
        spawn_points = self._place_spawn_points(rooms, level_config)

        # 放置光源
        lights = self._place_lights(rooms)

        # 构建地图数据
        map_data = self._build_map_data(map_width, map_height, rooms, corridors)

        # 标记楼梯为可行走
        if stairs_up:
            map_data[stairs_up[1]][stairs_up[0]] = 0
        if stairs_down:
            map_data[stairs_down[1]][stairs_down[0]] = 0

        cave_map = CaveMap(
            width=map_width,
            height=map_height,
            map_data=map_data,
            spawn_points=spawn_points,
            lights=lights,
            stairs_up=stairs_up,
            stairs_down=stairs_down,
        )

        logger.info(f"[CaveGenerator]Generated level {level}: "
                    f"{map_width}x{map_height}, {len(rooms)} rooms, "
                    f"{len(spawn_points)} spawn points")

        return cave_map

    def _generate_boss_room(self, width: int, height: int,
                            level_config: Dict[str, Any]) -> CaveMap:
        """生成 Boss 房间（单个大房间）"""
        # 全部为地板
        map_data = [[0 for _ in range(width)] for _ in range(height)]

        # 四周围墙
        for x in range(width):
            map_data[0][x] = 1
            map_data[height - 1][x] = 1
        for y in range(height):
            map_data[y][0] = 1
            map_data[y][width - 1] = 1

        # 入口在底部中央
        entrance = level_config.get("entrance", {"x": width // 2, "y": height - 2})
        stairs_up = (entrance["x"], entrance["y"])

        # Boss 刷新点在中央
        spawn_points = level_config.get("spawn_points", [{"x": width // 2, "y": height // 3}])

        # 无光源（Boss 房间用特殊光照）
        lights = []

        return CaveMap(
            width=width,
            height=height,
            map_data=map_data,
            spawn_points=spawn_points,
            lights=lights,
            stairs_up=stairs_up,
            stairs_down=None,
        )

    def _generate_rooms(self, map_width: int, map_height: int,
                        room_count: int) -> List[Room]:
        """生成随机不重叠房间"""
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

        # 按位置排序（左上到右下），便于走廊连接
        rooms.sort(key=lambda r: (r.x + r.y))

        return rooms

    def _connect_rooms(self, rooms: List[Room]) -> List[Corridor]:
        """使用 L 形走廊连接相邻房间"""
        corridors: List[Corridor] = []

        for i in range(len(rooms) - 1):
            ax, ay = rooms[i].center
            bx, by = rooms[i + 1].center

            if self._rng.random() < 0.5:
                # 先水平后垂直
                corridors.append(Corridor(ax, ay, bx, ay))
                corridors.append(Corridor(bx, ay, bx, by))
            else:
                # 先垂直后水平
                corridors.append(Corridor(ax, ay, ax, by))
                corridors.append(Corridor(ax, by, bx, by))

        return corridors

    def _place_stairs(self, rooms: List[Room],
                      level_config: Dict[str, Any]) -> Tuple[Optional[Tuple[int, int]], Optional[Tuple[int, int]]]:
        """放置楼梯，返回 (stairs_up, stairs_down)"""
        if not rooms:
            return None, None

        stairs_up = None
        stairs_down = None

        # 入口楼梯（向上）放在第一个房间中央
        if "exit_up" in level_config or "entrance" in level_config:
            first_room = rooms[0]
            sx, sy = first_room.center
            stairs_up = (sx, sy)

        # 出口楼梯（向下）放在最后一个房间中央
        if "exit_down" in level_config and len(rooms) > 1:
            last_room = rooms[-1]
            sx, sy = last_room.center
            stairs_down = (sx, sy)

        return stairs_up, stairs_down

    def _place_spawn_points(self, rooms: List[Room],
                            level_config: Dict[str, Any]) -> List[Dict[str, int]]:
        """在房间内放置怪物刷新点"""
        spawn_points: List[Dict[str, int]] = []
        max_monsters = level_config.get("max_monsters", 5)

        # 跳过第一个房间（入口房间）
        for i, room in enumerate(rooms):
            if i == 0:
                continue

            # 每个房间放 1-2 个刷新点
            count = min(2, max_monsters - len(spawn_points))
            for _ in range(count):
                if len(spawn_points) >= max_monsters:
                    break
                x = self._rng.randint(room.x + 1, room.x + room.w - 2)
                y = self._rng.randint(room.y + 1, room.y + room.h - 2)
                spawn_points.append({"x": x, "y": y})

        return spawn_points

    def _place_lights(self, rooms: List[Room]) -> List[Dict[str, Any]]:
        """在房间墙壁附近放置火把光源"""
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

                lights.append({
                    "x": x, "y": y,
                    "radius": 3,
                    "color": [255, 200, 100],
                    "type": "torch",
                })

        return lights

    def _build_map_data(self, map_width: int, map_height: int,
                        rooms: List[Room],
                        corridors: List[Corridor]) -> List[List[int]]:
        """构建二维地图数据（1=墙壁, 0=地板）"""
        # 初始化为墙壁
        map_data = [[1 for _ in range(map_width)] for _ in range(map_height)]

        # 挖掘房间
        for room in rooms:
            for y in range(room.y, room.y + room.h):
                for x in range(room.x, room.x + room.w):
                    if 0 <= x < map_width and 0 <= y < map_height:
                        map_data[y][x] = 0

        # 挖掘走廊
        for corridor in corridors:
            for x, y in corridor.points():
                if 0 <= x < map_width and 0 <= y < map_height:
                    map_data[y][x] = 0

        return map_data
```

- [ ] **Step 2: 创建 test_cave_generator.py**

创建文件 `scripts/client/test_cave_generator.py`：

```python
"""
CaveGenerator 单元测试
"""
import unittest
from scripts.client.cave_generator import CaveGenerator, Room, CaveMap


class TestRoom(unittest.TestCase):
    def test_center(self):
        room = Room(10, 10, 6, 4)
        self.assertEqual(room.center, (13, 12))

    def test_intersects_overlapping(self):
        r1 = Room(5, 5, 4, 4)
        r2 = Room(7, 7, 4, 4)
        self.assertTrue(r1.intersects(r2))

    def test_intersects_apart(self):
        r1 = Room(5, 5, 3, 3)
        r2 = Room(20, 20, 3, 3)
        self.assertFalse(r1.intersects(r2))

    def test_contains(self):
        room = Room(10, 10, 5, 5)
        self.assertTrue(room.contains(12, 12))
        self.assertFalse(room.contains(9, 12))
        self.assertFalse(room.contains(15, 12))


class TestCaveGenerator(unittest.TestCase):
    def test_generate_returns_cave_map(self):
        gen = CaveGenerator(seed=42)
        config = {
            "level": 1,
            "map_size": [20, 20],
            "room_count": 4,
            "max_monsters": 5,
            "entrance": {"x": 2, "y": 18},
            "exit_down": {"x": 18, "y": 2},
        }
        cave_map = gen.generate(config)
        self.assertIsInstance(cave_map, CaveMap)
        self.assertEqual(cave_map.width, 20)
        self.assertEqual(cave_map.height, 20)

    def test_generate_deterministic_with_seed(self):
        config = {
            "level": 1,
            "map_size": [20, 20],
            "room_count": 4,
            "max_monsters": 5,
            "entrance": {"x": 2, "y": 18},
            "exit_down": {"x": 18, "y": 2},
        }
        map1 = CaveGenerator(seed=123).generate(config)
        map2 = CaveGenerator(seed=123).generate(config)
        self.assertEqual(map1.map_data, map2.map_data)
        self.assertEqual(map1.stairs_up, map2.stairs_up)
        self.assertEqual(map1.stairs_down, map2.stairs_down)

    def test_generate_has_walkable_tiles(self):
        gen = CaveGenerator(seed=42)
        config = {
            "level": 1,
            "map_size": [20, 20],
            "room_count": 4,
            "max_monsters": 5,
            "entrance": {"x": 2, "y": 18},
            "exit_down": {"x": 18, "y": 2},
        }
        cave_map = gen.generate(config)
        walkable_count = sum(
            1 for y in range(cave_map.height)
            for x in range(cave_map.width)
            if cave_map.map_data[y][x] == 0
        )
        self.assertGreater(walkable_count, 50)

    def test_generate_stairs_present(self):
        gen = CaveGenerator(seed=42)
        config = {
            "level": 2,
            "map_size": [25, 25],
            "room_count": 6,
            "max_monsters": 8,
            "entrance": {"x": 2, "y": 18},
            "exit_down": {"x": 18, "y": 2},
            "exit_up": {"x": 2, "y": 18},
        }
        cave_map = gen.generate(config)
        self.assertIsNotNone(cave_map.stairs_up)
        self.assertIsNotNone(cave_map.stairs_down)

    def test_generate_boss_room(self):
        gen = CaveGenerator(seed=42)
        config = {
            "level": 10,
            "map_size": [20, 20],
            "room_count": 1,
            "max_monsters": 1,
            "is_boss_room": True,
            "entrance": {"x": 10, "y": 18},
            "exit_up": {"x": 10, "y": 18},
            "spawn_points": [{"x": 10, "y": 5}],
        }
        cave_map = gen.generate(config)
        self.assertEqual(cave_map.width, 20)
        self.assertIsNotNone(cave_map.stairs_up)
        self.assertIsNone(cave_map.stairs_down)
        # Boss 房间中央应该是地板
        self.assertTrue(cave_map.is_walkable(10, 10))

    def test_is_walkable(self):
        gen = CaveGenerator(seed=42)
        config = {
            "level": 1,
            "map_size": [20, 20],
            "room_count": 4,
            "max_monsters": 5,
            "entrance": {"x": 2, "y": 18},
            "exit_down": {"x": 18, "y": 2},
        }
        cave_map = gen.generate(config)
        # 边界外不可行走
        self.assertFalse(cave_map.is_walkable(-1, 0))
        self.assertFalse(cave_map.is_walkable(0, 20))


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 3: 运行测试验证**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest scripts/client/test_cave_generator.py -v
```

- [ ] **Step 4: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/client/cave_generator.py scripts/client/test_cave_generator.py
git commit -m "feat(cave): add procedural map generator

Add CaveGenerator with room+corridor algorithm for procedural cave
map generation. Supports variable room counts, L-shaped corridors,
stair placement, and boss room generation. Includes unit tests."
```

---

### Task 3: 矿洞光照系统

**Files:**
- Create: `scripts/client/cave_lighting.py`

- [ ] **Step 1: 创建 cave_lighting.py**

创建文件 `scripts/client/cave_lighting.py`：

```python
"""
矿洞光照系统
实现多层光照效果：环境光、玩家光照、静态光源、动态效果。
"""
import time
import math
import random
import logging
from typing import List, Dict, Any, Optional, Tuple

import pygame

from .constants import TILE_SIZE, ZOOM_FACTOR

logger = logging.getLogger("client.cave_lighting")


class CaveLighting:
    """矿洞光照系统"""

    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._light_mask = pygame.Surface((screen_width, screen_height), pygame.SRCALPHA)
        self._ambient_surface = pygame.Surface((screen_width, screen_height), pygame.SRCALPHA)

        # 动态效果状态
        self._torch_flicker_timer = 0.0
        self._torch_flicker_offsets: Dict[int, float] = {}

        # 受伤红光
        self._damage_flash_until = 0.0
        self._damage_flash_intensity = 0

        # Boss 光照
        self._boss_pulse_timer = 0.0
        self._boss_lighting_config: Optional[Dict[str, Any]] = None

    def trigger_damage_flash(self, intensity: int = 100):
        """触发受伤红光效果"""
        self._damage_flash_until = time.time() + 0.3
        self._damage_flash_intensity = intensity

    def set_boss_lighting(self, config: Optional[Dict[str, Any]]):
        """设置 Boss 房间光照配置"""
        self._boss_lighting_config = config
        self._boss_pulse_timer = 0.0

    def update(self, dt: float):
        """更新动态效果"""
        # 火把闪烁
        self._torch_flicker_timer += dt
        if self._torch_flicker_timer >= 0.1:
            self._torch_flicker_timer = 0.0
            for light_id in list(self._torch_flicker_offsets.keys()):
                self._torch_flicker_offsets[light_id] = random.uniform(-0.2, 0.2)

        # Boss 光照脉冲
        if self._boss_lighting_config:
            self._boss_pulse_timer += dt

    def render(self, screen: pygame.Surface, camera_x: float, camera_y: float,
               player_world_x: float, player_world_y: float,
               light_radius: int, ambient_color: Tuple[int, int, int],
               static_lights: Optional[List[Dict[str, Any]]] = None):
        """
        渲染光照效果

        Args:
            screen: 主屏幕 Surface
            camera_x: 相机 X 偏移（世界像素）
            camera_y: 相机 Y 偏移（世界像素）
            player_world_x: 玩家世界坐标 X
            player_world_y: 玩家世界坐标 Y
            light_radius: 玩家光照半径（格数）
            ambient_color: 环境光颜色 (R, G, B)
            static_lights: 静态光源列表
        """
        # 1. 填充黑色（全暗）
        self._light_mask.fill((0, 0, 0, 255))

        # 2. 绘制玩家光照
        player_screen_x = int((player_world_x - camera_x) * ZOOM_FACTOR)
        player_screen_y = int((player_world_y - camera_y) * ZOOM_FACTOR)
        radius_px = int(light_radius * TILE_SIZE * ZOOM_FACTOR)

        self._draw_light_circle(self._light_mask, player_screen_x, player_screen_y,
                                radius_px, (255, 255, 255, 0))

        # 3. 绘制静态光源
        if static_lights:
            for i, light in enumerate(static_lights):
                lx = int((light["x"] * TILE_SIZE - camera_x) * ZOOM_FACTOR)
                ly = int((light["y"] * TILE_SIZE - camera_y) * ZOOM_FACTOR)
                lr = int(light["radius"] * TILE_SIZE * ZOOM_FACTOR)

                # 火把闪烁效果
                if light.get("type") == "torch":
                    flicker = self._torch_flicker_offsets.get(i, 0) * TILE_SIZE * ZOOM_FACTOR
                    lr += int(flicker)

                # 初始化闪烁偏移
                if i not in self._torch_flicker_offsets:
                    self._torch_flicker_offsets[i] = 0.0

                color = tuple(light.get("color", [255, 200, 100]))
                self._draw_light_circle(self._light_mask, lx, ly, lr, (*color, 0))

        # 4. 叠加光照遮罩
        screen.blit(self._light_mask, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)

        # 5. 环境光色调
        self._ambient_surface.fill((*ambient_color, 30))
        screen.blit(self._ambient_surface, (0, 0))

        # 6. Boss 光照脉冲效果
        if self._boss_lighting_config:
            self._render_boss_lighting(screen)

        # 7. 受伤红光效果
        if time.time() < self._damage_flash_until:
            self._render_damage_flash(screen)

    def _draw_light_circle(self, surface: pygame.Surface, x: int, y: int,
                           radius: int, center_color: Tuple[int, int, int, int]):
        """绘制渐变光照圆形"""
        if radius <= 0:
            return
        # 使用多层同心圆模拟渐变
        step = max(2, radius // 15)
        for r in range(radius, 0, -step):
            alpha = int(255 * (1 - r / radius))
            color = (*center_color[:3], alpha)
            pygame.draw.circle(surface, color, (x, y), r)

    def _render_boss_lighting(self, screen: pygame.Surface):
        """渲染 Boss 房间特殊光照"""
        config = self._boss_lighting_config
        if not config:
            return

        pulse_speed = config.get("pulse_speed", 0.5)
        pulse_range = config.get("pulse_range", [0.8, 1.2])

        pulse = math.sin(self._boss_pulse_timer * pulse_speed * math.pi * 2)
        pulse = pulse_range[0] + (pulse_range[1] - pulse_range[0]) * (pulse + 1) / 2

        boss_surface = pygame.Surface((self._screen_width, self._screen_height), pygame.SRCALPHA)
        color = config.get("color", [150, 50, 50])
        alpha = int(20 * pulse)
        boss_surface.fill((*color, alpha))
        screen.blit(boss_surface, (0, 0))

    def _render_damage_flash(self, screen: pygame.Surface):
        """渲染受伤红光效果"""
        remaining = self._damage_flash_until - time.time()
        alpha = int(self._damage_flash_intensity * (remaining / 0.3))
        alpha = max(0, min(255, alpha))

        flash_surface = pygame.Surface((self._screen_width, self._screen_height), pygame.SRCALPHA)
        flash_surface.fill((255, 0, 0, alpha))
        screen.blit(flash_surface, (0, 0))
```

- [ ] **Step 2: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/client/cave_lighting.py
git commit -m "feat(cave): add cave lighting system

Add CaveLighting with multi-layer rendering: dark ambient overlay,
player light circle, static torch/lantern lights with flicker,
boss room pulse effect, and damage flash effect."
```

---

### Task 4: 矿洞场景管理器（客户端）

**Files:**
- Create: `scripts/client/cave_manager.py`

- [ ] **Step 1: 创建 cave_manager.py**

创建文件 `scripts/client/cave_manager.py`：

```python
"""
矿洞场景管理器
协调地图生成、光照渲染、矿洞层级状态。
"""
import json
import os
import logging
from typing import Dict, Any, Optional, List

import pygame

from .cave_generator import CaveGenerator, CaveMap
from .cave_lighting import CaveLighting
from .constants import TILE_SIZE, ZOOM_FACTOR

logger = logging.getLogger("client.cave_manager")

DATA_DIR = os.path.join(os.path.dirname(__file__), "data")


class CaveManager:
    """矿洞场景管理器"""

    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height

        # 加载配置
        self._levels_config: Dict[int, Dict[str, Any]] = {}
        self._scene_to_level: Dict[str, int] = {}
        self._load_config()

        # 子系统
        self._generator = CaveGenerator()
        self._lighting = CaveLighting(screen_width, screen_height)

        # 当前状态
        self._current_level: Optional[int] = None
        self._current_map: Optional[CaveMap] = None
        self._in_cave: bool = False

    def _load_config(self):
        """加载矿洞层配置"""
        path = os.path.join(DATA_DIR, "cave_levels.json")
        if not os.path.exists(path):
            logger.warning(f"[CaveManager]cave_levels.json not found at {path}")
            return

        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)

        for level_data in data.get("cave_levels", []):
            level = level_data["level"]
            self._levels_config[level] = level_data
            self._scene_to_level[level_data["scene_id"]] = level

        logger.info(f"[CaveManager]Loaded {len(self._levels_config)} cave levels")

    @property
    def in_cave(self) -> bool:
        return self._in_cave

    @property
    def current_level(self) -> Optional[int]:
        return self._current_level

    @property
    def current_map(self) -> Optional[CaveMap]:
        return self._current_map

    @property
    def lighting(self) -> CaveLighting:
        return self._lighting

    def get_level_config(self, level: int) -> Optional[Dict[str, Any]]:
        return self._levels_config.get(level)

    def get_level_for_scene(self, scene_id: str) -> Optional[int]:
        return self._scene_to_level.get(scene_id)

    def enter_cave(self, level: int = 1):
        """进入矿洞"""
        config = self._levels_config.get(level)
        if not config:
            logger.error(f"[CaveManager]No config for cave level {level}")
            return

        self._current_level = level
        self._in_cave = True

        # 生成地图
        self._current_map = self._generator.generate(config)

        # 设置 Boss 光照
        self._lighting.set_boss_lighting(config.get("boss_lighting"))

        logger.info(f"[CaveManager]Entered cave level {level}: {config['name']}")

    def change_level(self, new_level: int):
        """切换矿洞层级"""
        if not self._in_cave:
            logger.warning("[CaveManager]Not in cave, cannot change level")
            return

        config = self._levels_config.get(new_level)
        if not config:
            logger.error(f"[CaveManager]No config for cave level {new_level}")
            return

        self._current_level = new_level
        self._current_map = self._generator.generate(config)
        self._lighting.set_boss_lighting(config.get("boss_lighting"))

        logger.info(f"[CaveManager]Changed to cave level {new_level}: {config['name']}")

    def exit_cave(self):
        """离开矿洞"""
        self._current_level = None
        self._current_map = None
        self._in_cave = False
        self._lighting.set_boss_lighting(None)
        logger.info("[CaveManager]Exited cave")

    def is_stairs_at(self, tile_x: int, tile_y: int) -> Optional[str]:
        """检查指定位置是否有楼梯，返回 'up' 或 'down' 或 None"""
        if not self._current_map:
            return None

        if self._current_map.stairs_up == (tile_x, tile_y):
            return "up"
        if self._current_map.stairs_down == (tile_x, tile_y):
            return "down"
        return None

    def get_entrance_pos(self) -> Optional[tuple]:
        """获取当前层的入口位置（像素坐标）"""
        if not self._current_map or not self._current_map.stairs_up:
            return None
        x, y = self._current_map.stairs_up
        return (x * TILE_SIZE, y * TILE_SIZE)

    def get_target_scene_for_stairs(self, stairs_direction: str) -> Optional[str]:
        """获取楼梯目标场景 ID"""
        if not self._current_level:
            return None

        config = self._levels_config.get(self._current_level)
        if not config:
            return None

        if stairs_direction == "up":
            # 向上：回到上一层或农场
            if self._current_level == 1:
                return "farm"  # 回农场
            # 找上一层
            for level, cfg in self._levels_config.items():
                if cfg.get("exit_down") and level < self._current_level:
                    # 简单逻辑：level - 1
                    prev_scene = self._levels_config.get(self._current_level - 1)
                    if prev_scene:
                        return prev_scene["scene_id"]
            return "farm"

        elif stairs_direction == "down":
            # 向下：进入下一层
            for level, cfg in self._levels_config.items():
                if cfg.get("exit_up") and level > self._current_level:
                    return cfg["scene_id"]
            return None

        return None

    def update(self, dt: float):
        """更新动态效果"""
        if self._in_cave:
            self._lighting.update(dt)

    def render_lighting(self, screen: pygame.Surface, camera_x: float, camera_y: float,
                        player_world_x: float, player_world_y: float):
        """渲染光照效果"""
        if not self._in_cave or not self._current_map:
            return

        config = self._levels_config.get(self._current_level, {})
        light_radius = config.get("light_radius", 5)
        ambient_color = tuple(config.get("ambient_color", [30, 25, 25]))
        static_lights = self._current_map.lights

        self._lighting.render(screen, camera_x, camera_y,
                              player_world_x, player_world_y,
                              light_radius, ambient_color, static_lights)

    def render_minimap(self, screen: pygame.Surface, x: int, y: int):
        """渲染小地图（调试用）"""
        if not self._current_map:
            return

        scale = 3
        mw = self._current_map.width * scale
        mh = self._current_map.height * scale

        minimap = pygame.Surface((mw, mh), pygame.SRCALPHA)
        minimap.fill((0, 0, 0, 180))

        for ty in range(self._current_map.height):
            for tx in range(self._current_map.width):
                if self._current_map.map_data[ty][tx] == 0:
                    color = (80, 70, 60)
                else:
                    color = (30, 25, 20)
                pygame.draw.rect(minimap, color,
                                 (tx * scale, ty * scale, scale, scale))

        # 标记楼梯
        if self._current_map.stairs_up:
            sx, sy = self._current_map.stairs_up
            pygame.draw.rect(minimap, (0, 200, 0),
                             (sx * scale, sy * scale, scale, scale))
        if self._current_map.stairs_down:
            sx, sy = self._current_map.stairs_down
            pygame.draw.rect(minimap, (200, 200, 0),
                             (sx * scale, sy * scale, scale, scale))

        screen.blit(minimap, (x, y))
```

- [ ] **Step 2: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/client/cave_manager.py
git commit -m "feat(cave): add CaveManager for cave scene management

Add CaveManager that coordinates cave map generation, lighting
rendering, level transitions, and stair detection. Manages cave
level configuration and state."
```

---

### Task 5: 矿洞场景定义与传送门

**Files:**
- Modify: `scripts/client/scene/scene_defs.py`
- Modify: `scripts/client/interaction.py`

- [ ] **Step 1: 在 scene_defs.py 中添加矿洞场景定义**

在 `scripts/client/scene/scene_defs.py` 的 `SCENE_DEFS` 字典中，在 `"house"` 条目之后添加：

```python
    "cave_1": {
        "width": 20,
        "height": 20,
        "generate": lambda: _generate_cave_placeholder(20, 20),
        "portals": [
            {
                "pos": (10, 0),
                "trigger_dir": "up",
                "target": "farm",
                "target_portal": "cave_entrance",
            },
        ],
        "player_spawn": (10, 18),
        "is_cave": True,
    },
    "cave_2": {
        "width": 25,
        "height": 25,
        "generate": lambda: _generate_cave_placeholder(25, 25),
        "portals": [],
        "player_spawn": (12, 23),
        "is_cave": True,
    },
    "cave_3": {
        "width": 30,
        "height": 30,
        "generate": lambda: _generate_cave_placeholder(30, 30),
        "portals": [],
        "player_spawn": (15, 28),
        "is_cave": True,
    },
    "cave_boss": {
        "width": 20,
        "height": 20,
        "generate": lambda: _generate_cave_placeholder(20, 20),
        "portals": [],
        "player_spawn": (10, 18),
        "is_cave": True,
        "is_boss_room": True,
    },
```

在文件末尾（`get_spawn_for_portal` 函数之后）添加占位地图生成函数：

```python
def _generate_cave_placeholder(width: int, height: int) -> Dict[str, Any]:
    """
    矿洞占位地图生成（实际由 CaveGenerator 程序化生成）
    这里返回一个全地板的地图作为占位。
    """
    ground = [[0 for _ in range(width)] for _ in range(height)]
    objects = [[0 for _ in range(width)] for _ in range(height)]

    # 四周围墙
    for x in range(width):
        ground[0][x] = 1
        ground[height - 1][x] = 1
    for y in range(height):
        ground[y][0] = 1
        ground[y][width - 1] = 1

    return {"ground": ground, "objects": objects}
```

在 farm 场景的 `portals` 列表中添加矿洞入口传送门：

```python
            {
                "pos": (55, 25),
                "trigger_dir": "right",
                "target": "cave_1",
                "target_portal": "cave_entrance",
            },
```

- [ ] **Step 2: 在 interaction.py 中添加矿洞传送门场景映射**

在 `scripts/client/interaction.py` 的 `get_portal_scene()` 函数中，添加矿洞入口映射。找到现有的 portal 映射逻辑，在其中添加：

```python
    # 矿洞入口
    if obj_type == ObjectType.CAVE_ENTRANCE:
        return "cave_1"
```

- [ ] **Step 3: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/client/scene/scene_defs.py scripts/client/interaction.py
git commit -m "feat(cave): add cave scene definitions and portal mapping

Add cave_1/cave_2/cave_3/cave_boss scene definitions to SCENE_DEFS.
Add farm cave entrance portal. Add cave entrance interaction mapping
in interaction.py."
```

---

### Task 6: 服务器玩家数据扩展

**Files:**
- Modify: `scripts/server/game_server/src/player.h`
- Modify: `scripts/server/game_server/src/player.cpp`

- [ ] **Step 1: 在 PlayerBizData 中添加矿洞字段**

在 `scripts/server/game_server/src/player.h` 的 `PlayerBizData` 结构体中，在 `equipped_weapon` 字段之后添加：

```cpp
    int32_t cave_level = 0;           // 当前矿洞层级（0=不在矿洞）
    int32_t max_cave_level = 0;       // 已到达的最深层级
```

- [ ] **Step 2: 在 Player 类中添加访问方法**

在 `scripts/server/game_server/src/player.h` 的 Player 类中，在 `set_equipped_weapon` 方法之后添加：

```cpp
    int32_t get_cave_level() const { return player_data_.cave_level; }
    void set_cave_level(int32_t cave_level);

    int32_t get_max_cave_level() const { return player_data_.max_cave_level; }
    void set_max_cave_level(int32_t max_cave_level);
```

- [ ] **Step 3: 在 player.cpp 中实现 setter**

在 `scripts/server/game_server/src/player.cpp` 中，在现有 setter 方法之后添加：

```cpp
void Player::set_cave_level(int32_t cave_level) {
    player_data_.cave_level = cave_level;
    mark_dirty("cave_level");
}

void Player::set_max_cave_level(int32_t max_cave_level) {
    player_data_.max_cave_level = max_cave_level;
    mark_dirty("max_cave_level");
}
```

- [ ] **Step 4: 更新序列化方法**

在 `player.cpp` 的 `get_field_json` 方法中添加：

```cpp
    if (field == "cave_level") return std::to_string(player_data_.cave_level);
    if (field == "max_cave_level") return std::to_string(player_data_.max_cave_level);
```

在 `get_all_data_json` 方法中添加：

```cpp
    result += ",\"cave_level\":" + std::to_string(player_data_.cave_level);
    result += ",\"max_cave_level\":" + std::to_string(player_data_.max_cave_level);
```

- [ ] **Step 5: 编译验证**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build
cmake --build . --config Release
```

- [ ] **Step 6: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/player.h scripts/server/game_server/src/player.cpp
git commit -m "feat(cave): add cave_level fields to PlayerBizData

Add cave_level and max_cave_level to player data for tracking
cave exploration progress. Includes serialization support."
```

---

### Task 7: 服务器场景状态扩展

**Files:**
- Modify: `scripts/server/game_server/src/scene_state.h`
- Modify: `scripts/server/game_server/src/scene_state.cpp`

- [ ] **Step 1: 在 SceneState 中添加矿洞字段**

在 `scripts/server/game_server/src/scene_state.h` 的 SceneState 类中，在 `frozen_at_` 字段之后添加：

```cpp
    // 矿洞场景数据
    bool is_cave_ = false;
    int cave_level_ = 0;
    bool boss_defeated_ = false;
```

在 public 方法区域添加：

```cpp
    // 矿洞场景
    bool is_cave() const { return is_cave_; }
    void set_cave(bool is_cave, int level = 0);

    int cave_level() const { return cave_level_; }
    bool boss_defeated() const { return boss_defeated_; }
    void set_boss_defeated(bool defeated);
```

- [ ] **Step 2: 在 scene_state.cpp 中实现新方法**

在 `scripts/server/game_server/src/scene_state.cpp` 中添加：

```cpp
void SceneState::set_cave(bool is_cave, int level) {
    is_cave_ = is_cave;
    cave_level_ = level;
}

void SceneState::set_boss_defeated(bool defeated) {
    boss_defeated_ = defeated;
}
```

- [ ] **Step 3: 编译验证**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build
cmake --build . --config Release
```

- [ ] **Step 4: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/scene_state.h scripts/server/game_server/src/scene_state.cpp
git commit -m "feat(cave): extend SceneState with cave fields

Add is_cave, cave_level, boss_defeated fields to SceneState for
tracking cave scene state and boss progress."
```

---

### Task 8: 服务器怪物刷新器

**Files:**
- Create: `scripts/server/game_server/src/cave_spawner.h`
- Create: `scripts/server/game_server/src/cave_spawner.cpp`
- Modify: `scripts/server/game_server/CMakeLists.txt`

- [ ] **Step 1: 创建 cave_spawner.h**

创建文件 `scripts/server/game_server/src/cave_spawner.h`：

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <random>

namespace farm {

class ServerMonsterManager;

struct SpawnPoint {
    float x;
    float y;
};

struct MonsterDeathRecord {
    uint32_t monster_id;
    time_t death_time;
};

using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;

class CaveSpawner {
public:
    CaveSpawner(ServerMonsterManager* monster_mgr);

    // 设置矿洞层配置
    void configure(int level, int max_monsters,
                   const std::vector<std::string>& monster_types,
                   const std::vector<SpawnPoint>& spawn_points,
                   float respawn_delay, float spawn_check_interval,
                   float spawn_radius);

    // 初始化怪物（进入场景时）
    void init_monsters(uint64_t player_id, float player_x, float player_y,
                       SendGameMsgFunc send_msg);

    // 更新刷新逻辑
    void update(float dt, uint64_t player_id, float player_x, float player_y,
                SendGameMsgFunc send_msg);

    // 怪物死亡时调用
    void on_monster_death(uint32_t monster_id);

    // 冻结/解冻
    void freeze();
    void thaw();
    bool is_frozen() const { return frozen_; }

    // 获取当前存活怪物数
    int alive_count() const;

private:
    ServerMonsterManager* monster_mgr_;

    int level_ = 0;
    int max_monsters_ = 0;
    std::vector<std::string> monster_types_;
    std::vector<SpawnPoint> spawn_points_;
    float respawn_delay_ = 10.0f;
    float spawn_check_interval_ = 5.0f;
    float spawn_radius_ = 2.0f;

    std::vector<uint32_t> active_monster_ids_;
    std::vector<MonsterDeathRecord> death_records_;
    float spawn_timer_ = 0.0f;
    bool frozen_ = false;

    std::mt19937 rng_{std::random_device{}()};

    void try_spawn_monster(uint64_t player_id, float player_x, float player_y,
                           SendGameMsgFunc send_msg);
    SpawnPoint select_spawn_point(float player_x, float player_y);
};

}  // namespace farm
```

- [ ] **Step 2: 创建 cave_spawner.cpp**

创建文件 `scripts/server/game_server/src/cave_spawner.cpp`：

```cpp
#include "cave_spawner.h"
#include "monster_manager.h"
#include "message_ids.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>
#include <ctime>
#include <cmath>
#include <algorithm>

namespace farm {

CaveSpawner::CaveSpawner(ServerMonsterManager* monster_mgr)
    : monster_mgr_(monster_mgr) {}

void CaveSpawner::configure(int level, int max_monsters,
                            const std::vector<std::string>& monster_types,
                            const std::vector<SpawnPoint>& spawn_points,
                            float respawn_delay, float spawn_check_interval,
                            float spawn_radius) {
    level_ = level;
    max_monsters_ = max_monsters;
    monster_types_ = monster_types;
    spawn_points_ = spawn_points;
    respawn_delay_ = respawn_delay;
    spawn_check_interval_ = spawn_check_interval;
    spawn_radius_ = spawn_radius;
}

void CaveSpawner::init_monsters(uint64_t player_id, float player_x, float player_y,
                                SendGameMsgFunc send_msg) {
    active_monster_ids_.clear();
    death_records_.clear();
    spawn_timer_ = 0.0f;

    // 生成初始怪物
    int initial_count = std::min(max_monsters_, static_cast<int>(spawn_points_.size()));
    for (int i = 0; i < initial_count; ++i) {
        if (monster_types_.empty()) break;

        const auto& sp = spawn_points_[i % spawn_points_.size()];
        const auto& type = monster_types_[i % monster_types_.size()];

        uint32_t id = monster_mgr_->spawn_monster(type, sp.x * 16.0f, sp.y * 16.0f);
        if (id > 0) {
            active_monster_ids_.push_back(id);

            // 通知客户端
            const auto* m = monster_mgr_->get_monster(id);
            if (m) {
                nlohmann::json notify;
                notify["monster_id"] = id;
                notify["monster_type"] = m->monster_type;
                notify["x"] = m->x;
                notify["y"] = m->y;
                notify["hp"] = m->hp;
                notify["max_hp"] = m->max_hp;

                std::string data = notify.dump();
                send_msg(player_id, MSG_ID_MONSTER_SPAWN_NOTIFY,
                         reinterpret_cast<const uint8_t*>(data.data()), data.size());
            }
        }
    }

    SPDLOG_INFO("[CaveSpawner]Initialized level {} with {} monsters",
                level_, active_monster_ids_.size());
}

void CaveSpawner::update(float dt, uint64_t player_id, float player_x, float player_y,
                         SendGameMsgFunc send_msg) {
    if (frozen_) return;

    spawn_timer_ += dt;
    if (spawn_timer_ < spawn_check_interval_) return;
    spawn_timer_ = 0.0f;

    // 清理已死亡的活跃怪物 ID
    active_monster_ids_.erase(
        std::remove_if(active_monster_ids_.begin(), active_monster_ids_.end(),
                       [this](uint32_t id) { return monster_mgr_->is_monster_dead(id); }),
        active_monster_ids_.end());

    // 尝试刷新
    try_spawn_monster(player_id, player_x, player_y, send_msg);
}

void CaveSpawner::on_monster_death(uint32_t monster_id) {
    death_records_.push_back({monster_id, std::time(nullptr)});
    SPDLOG_INFO("[CaveSpawner]Monster {} died in level {}", monster_id, level_);
}

void CaveSpawner::freeze() {
    frozen_ = true;
    SPDLOG_INFO("[CaveSpawner]Level {} frozen", level_);
}

void CaveSpawner::thaw() {
    frozen_ = false;
    spawn_timer_ = 0.0f;
    SPDLOG_INFO("[CaveSpawner]Level {} thawed", level_);
}

int CaveSpawner::alive_count() const {
    int count = 0;
    for (uint32_t id : active_monster_ids_) {
        if (!monster_mgr_->is_monster_dead(id)) {
            ++count;
        }
    }
    return count;
}

void CaveSpawner::try_spawn_monster(uint64_t player_id, float player_x, float player_y,
                                    SendGameMsgFunc send_msg) {
    if (alive_count() >= max_monsters_) return;
    if (monster_types_.empty() || spawn_points_.empty()) return;

    // 检查死亡记录，找到已过延迟的
    time_t now = std::time(nullptr);
    bool can_spawn = false;
    for (auto it = death_records_.begin(); it != death_records_.end(); ++it) {
        if (difftime(now, it->death_time) >= respawn_delay_) {
            death_records_.erase(it);
            can_spawn = true;
            break;
        }
    }

    if (!can_spawn) return;

    // 选择刷新点
    SpawnPoint sp = select_spawn_point(player_x, player_y);

    // 随机怪物类型
    std::uniform_int_distribution<size_t> type_dist(0, monster_types_.size() - 1);
    const auto& type = monster_types_[type_dist(rng_)];

    uint32_t id = monster_mgr_->spawn_monster(type, sp.x * 16.0f, sp.y * 16.0f);
    if (id > 0) {
        active_monster_ids_.push_back(id);

        const auto* m = monster_mgr_->get_monster(id);
        if (m) {
            nlohmann::json notify;
            notify["monster_id"] = id;
            notify["monster_type"] = m->monster_type;
            notify["x"] = m->x;
            notify["y"] = m->y;
            notify["hp"] = m->hp;
            notify["max_hp"] = m->max_hp;

            std::string data = notify.dump();
            send_msg(player_id, MSG_ID_MONSTER_SPAWN_NOTIFY,
                     reinterpret_cast<const uint8_t*>(data.data()), data.size());

            SPDLOG_INFO("[CaveSpawner]Spawned monster {} type={} at ({},{})",
                        id, type, sp.x, sp.y);
        }
    }
}

SpawnPoint CaveSpawner::select_spawn_point(float player_x, float player_y) {
    // 选择距离玩家最远的刷新点
    float max_dist = 0;
    SpawnPoint best = spawn_points_[0];

    for (const auto& sp : spawn_points_) {
        float dx = sp.x * 16.0f - player_x;
        float dy = sp.y * 16.0f - player_y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > max_dist) {
            max_dist = dist;
            best = sp;
        }
    }

    // 加一点随机偏移
    std::uniform_real_distribution<float> offset(-spawn_radius_, spawn_radius_);
    best.x += offset(rng_);
    best.y += offset(rng_);

    return best;
}

}  // namespace farm
```

- [ ] **Step 3: 更新 CMakeLists.txt**

在 `scripts/server/game_server/CMakeLists.txt` 的 `SOURCES` 列表中，在 `src/combat_handler.cpp` 之后添加：

```cmake
    src/cave_spawner.cpp
```

在 `TEST_SOURCES` 列表中同样添加（如果存在）。

- [ ] **Step 4: 编译验证**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build
cmake --build . --config Release
```

- [ ] **Step 5: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/cave_spawner.h scripts/server/game_server/src/cave_spawner.cpp scripts/server/game_server/CMakeLists.txt
git commit -m "feat(cave): add CaveSpawner for server-side monster spawning

Add CaveSpawner with delayed respawn, spawn point selection (avoids
player), freeze/thaw support, and MonsterSpawnNotify broadcasting."
```

---

### Task 9: 服务器场景切换扩展

**Files:**
- Modify: `scripts/server/game_server/src/monster_manager.h`
- Modify: `scripts/server/game_server/src/monster_manager.cpp`
- Modify: `scripts/server/game_server/src/game_scene_manager.h`
- Modify: `scripts/server/game_server/src/game_scene_manager.cpp`
- Modify: `scripts/server/game_server/src/game_server.h`
- Modify: `scripts/server/game_server/src/game_server.cpp`

- [ ] **Step 1: 扩展 ServerMonsterManager 支持多场景**

在 `scripts/server/game_server/src/monster_manager.h` 的 ServerMonsterManager 类的 public 区域添加：

```cpp
    // 多场景怪物管理
    void set_scene_monsters(const std::string& scene_id,
                           const std::vector<uint32_t>& monster_ids);
    std::vector<uint32_t> get_scene_monster_ids(const std::string& scene_id) const;
    void clear_scene_monsters(const std::string& scene_id);
```

在 private 区域添加：

```cpp
    std::unordered_map<std::string, std::vector<uint32_t>> scene_monsters_;
```

在 `scripts/server/game_server/src/monster_manager.cpp` 中实现：

```cpp
void ServerMonsterManager::set_scene_monsters(const std::string& scene_id,
                                              const std::vector<uint32_t>& monster_ids) {
    scene_monsters_[scene_id] = monster_ids;
}

std::vector<uint32_t> ServerMonsterManager::get_scene_monster_ids(const std::string& scene_id) const {
    auto it = scene_monsters_.find(scene_id);
    return it != scene_monsters_.end() ? it->second : std::vector<uint32_t>{};
}

void ServerMonsterManager::clear_scene_monsters(const std::string& scene_id) {
    auto it = scene_monsters_.find(scene_id);
    if (it != scene_monsters_.end()) {
        for (uint32_t id : it->second) {
            remove_monster(id);
        }
        scene_monsters_.erase(it);
    }
}
```

- [ ] **Step 2: 扩展 GameSceneManager 支持矿洞场景**

在 `scripts/server/game_server/src/game_scene_manager.h` 中添加 CaveSpawner 头文件引用和成员：

在顶部添加：
```cpp
#include "cave_spawner.h"
```

在 GameSceneManager 类的 private 区域添加：

```cpp
    std::unordered_map<int, std::unique_ptr<CaveSpawner>> cave_spawners_;
```

在 public 区域添加：

```cpp
    // 矿洞场景
    CaveSpawner* get_or_create_cave_spawner(int cave_level);
    void update_cave_spawners(float dt, uint64_t player_id, float player_x, float player_y);
```

在 `scripts/server/game_server/src/game_scene_manager.cpp` 中：

在 `get_or_create_scene` 方法中，扩展场景尺寸支持：

```cpp
    // Scene dimensions (matching client scene_defs.py)
    int width = 60;
    int height = 50;
    if (scene_id == "house") {
        width = 10;
        height = 8;
    } else if (scene_id == "cave_1") {
        width = 20;
        height = 20;
    } else if (scene_id == "cave_2") {
        width = 25;
        height = 25;
    } else if (scene_id == "cave_3") {
        width = 30;
        height = 30;
    } else if (scene_id == "cave_boss") {
        width = 20;
        height = 20;
    }
```

在 `handle_scene_change_req` 方法中，扩展场景验证白名单：

```cpp
    // Validate target scene exists in our scene definitions
    if (target_scene != "farm" && target_scene != "house" &&
        target_scene != "cave_1" && target_scene != "cave_2" &&
        target_scene != "cave_3" && target_scene != "cave_boss") {
```

扩展出生点逻辑：

```cpp
    // Determine spawn position
    int spawn_x = 5;
    int spawn_y = 5;
    if (target_scene == "farm") {
        spawn_x = 30;
        spawn_y = 25;
    } else if (target_scene == "house") {
        spawn_x = 5;
        spawn_y = 6;
    } else if (target_scene == "cave_1") {
        spawn_x = 10;
        spawn_y = 18;
    } else if (target_scene == "cave_2") {
        spawn_x = 12;
        spawn_y = 23;
    } else if (target_scene == "cave_3") {
        spawn_x = 15;
        spawn_y = 28;
    } else if (target_scene == "cave_boss") {
        spawn_x = 10;
        spawn_y = 18;
    }
```

在文件末尾添加矿洞 spawner 方法：

```cpp
CaveSpawner* GameSceneManager::get_or_create_cave_spawner(int cave_level) {
    auto it = cave_spawners_.find(cave_level);
    if (it != cave_spawners_.end()) {
        return it->second.get();
    }
    // 创建新的 spawner（需要 monster_mgr，但这里没有直接引用）
    // 实际实现在 GameServer 中通过 monster_mgr_ 创建
    return nullptr;
}

void GameSceneManager::update_cave_spawners(float dt, uint64_t player_id,
                                             float player_x, float player_y) {
    // 由 GameServer 调用，遍历活跃的 cave spawners
}
```

- [ ] **Step 3: 在 GameServer 中集成 CaveSpawner**

在 `scripts/server/game_server/src/game_server.h` 的 private 区域添加：

```cpp
    std::unique_ptr<CaveSpawner> cave_spawner_;
```

在 `scripts/server/game_server/src/game_server.cpp` 的 `start()` 方法中，在 `combat_handler_` 初始化之后添加：

```cpp
    cave_spawner_ = std::make_unique<CaveSpawner>(monster_mgr_.get());
```

在 `update_game_logic()` 方法中，添加矿洞 spawner 更新：

```cpp
    // 更新矿洞怪物刷新
    if (cave_spawner_ && !cave_spawner_->is_frozen()) {
        for (auto& [pid, session] : sessions_) {
            auto player_opt = player_mgr_.get_player(pid);
            if (player_opt.has_value()) {
                Player* player = player_opt.value();
                if (player->get_cave_level() > 0) {
                    auto send_msg = [this](uint64_t p, uint32_t m, const uint8_t* d, size_t l) {
                        send_game_msg(p, m, d, l);
                    };
                    cave_spawner_->update(1.0f, pid, player->get_pos_x(), player->get_pos_y(), send_msg);
                }
            }
        }
    }
```

- [ ] **Step 4: 编译验证**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build
cmake --build . --config Release
```

- [ ] **Step 5: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/monster_manager.h scripts/server/game_server/src/monster_manager.cpp scripts/server/game_server/src/game_scene_manager.h scripts/server/game_server/src/game_scene_manager.cpp scripts/server/game_server/src/game_server.h scripts/server/game_server/src/game_server.cpp
git commit -m "feat(cave): integrate cave scene switching on server

Extend GameSceneManager to support cave_1/cave_2/cave_3/cave_boss
scenes. Add multi-scene monster management to ServerMonsterManager.
Integrate CaveSpawner into GameServer update loop."
```

---

### Task 10: 客户端集成

**Files:**
- Modify: `scripts/client/game_scene.py`
- Modify: `scripts/client/game_renderer.py`
- Modify: `scripts/client/network_dispatcher.py`

- [ ] **Step 1: 在 game_scene.py 中集成 CaveManager**

在 `scripts/client/game_scene.py` 的导入部分添加（在 combat 导入附近）：

```python
try:
    from .cave_manager import CaveManager
    _CAVE_AVAILABLE = True
except ImportError:
    _CAVE_AVAILABLE = False
```

在 `GameScene.__init__` 中，在战斗系统初始化之后添加：

```python
        # 矿洞系统
        self._cave_manager = None
        if _CAVE_AVAILABLE:
            self._cave_manager = CaveManager(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)
```

在主循环的更新部分，在怪物更新之后添加：

```python
        # 矿洞系统更新
        if self._cave_manager:
            self._cave_manager.update(dt)
```

在场景切换响应处理中，添加矿洞进入/退出逻辑。找到 `handle_scene_change_resp` 调用处，添加：

```python
            # 矿洞场景检测
            if self._cave_manager:
                cave_level = self._cave_manager.get_level_for_scene(target_scene)
                if cave_level is not None:
                    self._cave_manager.enter_cave(cave_level)
                    # 清空怪物（新场景）
                    if self._monster_manager:
                        self._monster_manager.clear()
                elif self._cave_manager.in_cave:
                    self._cave_manager.exit_cave()
                    if self._monster_manager:
                        self._monster_manager.clear()
```

在渲染调用中，添加 `cave_manager` 参数：

```python
        self._renderer.render(
            # ... 现有参数 ...
            cave_manager=self._cave_manager,
        )
```

- [ ] **Step 2: 在 game_renderer.py 中添加矿洞光照渲染**

在 `scripts/client/game_renderer.py` 的 `render` 方法签名中添加参数：

```python
           cave_manager=None)
```

在怪物渲染之后、Bubble UI 渲染之前，添加矿洞光照渲染：

```python
        # 矿洞光照渲染
        if cave_manager and cave_manager.in_cave:
            cave_manager.render_lighting(
                self._screen,
                map_renderer.x, map_renderer.y,
                player_sprite.world_x, player_sprite.world_y
            )
```

- [ ] **Step 3: 在 network_dispatcher.py 中添加场景切换怪物清空**

在 `scripts/client/network_dispatcher.py` 的 `_handle_scene_change_resp` 方法中，在成功处理响应后添加怪物清空回调：

找到现有的 scene change resp 处理逻辑，在成功分支中添加：

```python
            # 清空怪物（场景切换时）
            if self._callbacks.get("on_scene_change_clear_monsters"):
                self._callbacks["on_scene_change_clear_monsters"]()
```

在 `__init__` 的回调参数中添加：

```python
        on_scene_change_clear_monsters=None,
```

在 `GameScene.__init__` 中注册回调：

```python
            on_scene_change_clear_monsters=lambda: self._monster_manager.clear() if self._monster_manager else None,
```

- [ ] **Step 4: 运行测试**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest scripts/client/test_cave_generator.py -v
```

- [ ] **Step 5: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/client/game_scene.py scripts/client/game_renderer.py scripts/client/network_dispatcher.py
git commit -m "feat(cave): integrate cave system into GameScene and renderer

Wire up CaveManager into game loop for lighting updates. Add cave
lighting rendering step in GameRenderer. Clear monsters on scene
switch via NetworkDispatcher callback."
```

---

### Task 11: 端到端验证

**Files:**
- None (verification only)

- [ ] **Step 1: 编译服务器**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build
cmake --build . --config Release
```

- [ ] **Step 2: 运行客户端单元测试**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest scripts/client/test_cave_generator.py -v
python -m pytest scripts/client/test_combat_system.py -v
```

- [ ] **Step 3: 启动服务器和客户端进行手动测试**

启动服务器：
```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build
./game_server
```

启动客户端：
```bash
cd D:/mb_workspace/farm_demo
python scripts/client/main.py
```

手动验证：
1. 在农场走到矿洞入口位置，确认自动切换到矿洞场景
2. 确认矿洞内有光照效果（暗色环境 + 玩家周围亮光）
3. 确认怪物正常刷新和战斗
4. 走到楼梯位置，确认层级切换
5. 确认矿洞内死亡后传回农场

- [ ] **Step 4: Commit（如果有修复）**

```bash
cd D:/mb_workspace/farm_demo
git add -A
git commit -m "fix(cave): fix issues found during end-to-end testing

Fix any integration issues discovered during manual testing of the
cave scene system."
```
