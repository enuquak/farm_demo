"""NetworkMessageDispatcher unit tests"""
import unittest
from unittest.mock import MagicMock
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from scripts.client.network_dispatcher import NetworkMessageDispatcher
from scripts.client.message_ids import (
    MSG_ID_MAP_DATA_NOTIFY, MSG_ID_CLOCK_SYNC, MSG_ID_FORCE_SLEEP_NOTIFY,
)


class TestNetworkMessageDispatcher(unittest.TestCase):
    """Test NetworkMessageDispatcher module structure."""

    def test_import(self):
        """NetworkMessageDispatcher can be imported"""
        self.assertTrue(callable(NetworkMessageDispatcher))

    def test_has_methods(self):
        """NetworkMessageDispatcher has expected methods"""
        self.assertTrue(hasattr(NetworkMessageDispatcher, 'dispatch_pending'))

    def test_handler_registry(self):
        """Constructor registers handlers for expected message IDs"""
        dispatcher = NetworkMessageDispatcher(
            connection=MagicMock(),
            on_map_data_notify=MagicMock(),
            on_position_correct=MagicMock(),
            on_item_use_resp=MagicMock(),
            on_scene_change_resp=MagicMock(),
            on_clock_sync=MagicMock(),
            on_force_sleep_notify=MagicMock(),
        )
        self.assertIn(MSG_ID_MAP_DATA_NOTIFY, dispatcher._dispatch_table)
        self.assertIn(MSG_ID_CLOCK_SYNC, dispatcher._dispatch_table)
        self.assertIn(MSG_ID_FORCE_SLEEP_NOTIFY, dispatcher._dispatch_table)

    def test_dispatch_empty(self):
        """dispatch_pending with no messages does nothing"""
        conn = MagicMock()
        conn.recv_all_messages.return_value = []

        dispatcher = NetworkMessageDispatcher(
            connection=conn,
            on_map_data_notify=MagicMock(),
            on_position_correct=MagicMock(),
            on_item_use_resp=MagicMock(),
            on_scene_change_resp=MagicMock(),
            on_clock_sync=MagicMock(),
            on_force_sleep_notify=MagicMock(),
        )
        # Should not raise
        dispatcher.dispatch_pending()


if __name__ == '__main__':
    unittest.main()
