"""TeamManager 单元测试"""
import unittest
from unittest.mock import MagicMock

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from scripts.client.team_manager import TeamManager, TeamMember, TeamInviteNotify


class MockConnection:
    def __init__(self):
        self.sent_messages = []

    def send_message(self, msg_id, data):
        self.sent_messages.append((msg_id, data))


class TestTeamManager(unittest.TestCase):
    def setUp(self):
        self.conn = MockConnection()
        self.mgr = TeamManager(self.conn, player_id=1001)

    def test_initial_state(self):
        """初始状态：不在队伍中"""
        self.assertFalse(self.mgr.in_team)
        self.assertFalse(self.mgr.is_leader)
        self.assertEqual(self.mgr.team_id, 0)
        self.assertEqual(self.mgr.member_count, 0)

    def test_create_team_sends_req(self):
        """创建队伍发送正确消息"""
        self.mgr.create_team()
        self.assertEqual(len(self.conn.sent_messages), 1)
        self.assertEqual(self.conn.sent_messages[0][0], 7001)

    def test_leave_team_when_not_in_team(self):
        """不在队伍中时离开无效"""
        self.mgr.leave_team()
        self.assertEqual(len(self.conn.sent_messages), 0)

    def test_invite_when_not_in_team(self):
        """不在队伍中时邀请无效"""
        self.mgr.invite_player(2001)
        self.assertEqual(len(self.conn.sent_messages), 0)


if __name__ == '__main__':
    unittest.main()
