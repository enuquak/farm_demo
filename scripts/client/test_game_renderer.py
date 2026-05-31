"""GameRenderer unit tests"""
import unittest
from unittest.mock import MagicMock, patch
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from scripts.client.game_renderer import GameRenderer


class TestGameRenderer(unittest.TestCase):
    """Test GameRenderer module structure."""

    def test_import(self):
        """GameRenderer can be imported"""
        self.assertTrue(callable(GameRenderer))

    def test_has_methods(self):
        """GameRenderer has expected methods"""
        self.assertTrue(hasattr(GameRenderer, 'render'))

    def test_constructor(self):
        """GameRenderer can be constructed with mock screen"""
        screen = MagicMock()
        player_data = {
            'energy_current': 100,
            'energy_max': 100,
            'player_name': 'TestPlayer',
            'scene_id': 'farm',
        }
        renderer = GameRenderer(screen, 800, 600, player_data)
        self.assertIsNotNone(renderer)


if __name__ == '__main__':
    unittest.main()
