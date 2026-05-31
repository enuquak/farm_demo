"""PlayerController unit tests"""
import unittest
from unittest.mock import MagicMock
import sys
import os

# 确保 client 包可导入
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from scripts.client.constants import TILE_SIZE, Direction


class TestPlayerController(unittest.TestCase):
    """Test PlayerController module structure."""

    def test_import(self):
        """PlayerController can be imported"""
        from scripts.client.player_controller import PlayerController
        self.assertTrue(callable(PlayerController))

    def test_has_methods(self):
        """PlayerController has expected methods"""
        from scripts.client.player_controller import PlayerController
        self.assertTrue(hasattr(PlayerController, 'handle_input'))
        self.assertTrue(hasattr(PlayerController, 'update_position_sending'))
        self.assertTrue(hasattr(PlayerController, 'start_correction'))

    def test_tile_size(self):
        """TILE_SIZE constant is defined and is a positive integer"""
        self.assertIsInstance(TILE_SIZE, int)
        self.assertGreater(TILE_SIZE, 0)

    def test_direction_exists(self):
        """Direction enum exists"""
        self.assertIsNotNone(Direction)


if __name__ == '__main__':
    unittest.main()
