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
        config = {"level": 1, "map_size": [20, 20], "room_count": 4,
                  "max_monsters": 5, "entrance": {"x": 2, "y": 18}, "exit_down": {"x": 18, "y": 2}}
        cave_map = gen.generate(config)
        self.assertIsInstance(cave_map, CaveMap)
        self.assertEqual(cave_map.width, 20)
        self.assertEqual(cave_map.height, 20)

    def test_generate_deterministic_with_seed(self):
        config = {"level": 1, "map_size": [20, 20], "room_count": 4,
                  "max_monsters": 5, "entrance": {"x": 2, "y": 18}, "exit_down": {"x": 18, "y": 2}}
        map1 = CaveGenerator(seed=123).generate(config)
        map2 = CaveGenerator(seed=123).generate(config)
        self.assertEqual(map1.map_data, map2.map_data)
        self.assertEqual(map1.stairs_up, map2.stairs_up)
        self.assertEqual(map1.stairs_down, map2.stairs_down)

    def test_generate_has_walkable_tiles(self):
        gen = CaveGenerator(seed=42)
        config = {"level": 1, "map_size": [20, 20], "room_count": 4,
                  "max_monsters": 5, "entrance": {"x": 2, "y": 18}, "exit_down": {"x": 18, "y": 2}}
        cave_map = gen.generate(config)
        walkable_count = sum(1 for y in range(cave_map.height) for x in range(cave_map.width) if cave_map.map_data[y][x] == 0)
        self.assertGreater(walkable_count, 50)

    def test_generate_stairs_present(self):
        gen = CaveGenerator(seed=42)
        config = {"level": 2, "map_size": [25, 25], "room_count": 6,
                  "max_monsters": 8, "entrance": {"x": 2, "y": 18},
                  "exit_down": {"x": 18, "y": 2}, "exit_up": {"x": 2, "y": 18}}
        cave_map = gen.generate(config)
        self.assertIsNotNone(cave_map.stairs_up)
        self.assertIsNotNone(cave_map.stairs_down)

    def test_generate_boss_room(self):
        gen = CaveGenerator(seed=42)
        config = {"level": 10, "map_size": [20, 20], "room_count": 1,
                  "max_monsters": 1, "is_boss_room": True,
                  "entrance": {"x": 10, "y": 18}, "exit_up": {"x": 10, "y": 18},
                  "spawn_points": [{"x": 10, "y": 5}]}
        cave_map = gen.generate(config)
        self.assertEqual(cave_map.width, 20)
        self.assertIsNotNone(cave_map.stairs_up)
        self.assertIsNone(cave_map.stairs_down)
        self.assertTrue(cave_map.is_walkable(10, 10))

    def test_is_walkable(self):
        gen = CaveGenerator(seed=42)
        config = {"level": 1, "map_size": [20, 20], "room_count": 4,
                  "max_monsters": 5, "entrance": {"x": 2, "y": 18}, "exit_down": {"x": 18, "y": 2}}
        cave_map = gen.generate(config)
        self.assertFalse(cave_map.is_walkable(-1, 0))
        self.assertFalse(cave_map.is_walkable(0, 20))


if __name__ == "__main__":
    unittest.main()
