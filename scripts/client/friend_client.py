"""
好友系统客户端模块
封装好友相关的网络请求发送和状态管理。
"""
import logging
from typing import List, Any

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'common', 'proto', 'generated'))
import base_pb2
import friend_pb2

from .message_ids import (
    MSG_ID_FRIEND_SEARCH_REQ, MSG_ID_FRIEND_ADD_REQ, MSG_ID_FRIEND_ACCEPT_REQ,
    MSG_ID_FRIEND_REJECT_REQ, MSG_ID_FRIEND_DELETE_REQ, MSG_ID_FRIEND_LIST_REQ,
    MSG_ID_FRIEND_BLOCK_REQ, MSG_ID_FRIEND_UNBLOCK_REQ,
    MSG_ID_FRIEND_CHAT_REQ, MSG_ID_FRIEND_CHAT_HISTORY_REQ,
    MSG_ID_FRIEND_GIFT_REQ,
    MSG_ID_FRIEND_VISIT_REQ, MSG_ID_FRIEND_VISIT_ACTION_REQ,
    MSG_ID_FRIEND_RECOMMEND_REQ,
)

logger = logging.getLogger("client.friend_client")


class FriendClient:
    """好友系统网络客户端"""

    def __init__(self, connection, player_id: int, server_id: int):
        self._connection = connection
        self._player_id = player_id
        self._server_id = server_id

        # State
        self.friends: List[Any] = []
        self.pending_requests: List[Any] = []
        self.chat_history: dict = {}  # target_id -> [messages]
        self.recommendations: List[Any] = []

    def _send(self, msg_id: int, inner_msg):
        """发送消息到服务器"""
        payload = inner_msg.SerializeToString()
        player_msg = base_pb2.PlayerMsg()
        player_msg.player_id = self._player_id
        player_msg.server_id = self._server_id
        player_msg.msg_id = msg_id
        player_msg.payload = payload
        msg_payload = player_msg.SerializeToString()
        self._connection.send_message(msg_id, msg_payload)

    def search(self, keyword: str):
        """搜索玩家"""
        req = friend_pb2.FriendSearchReq()
        req.keyword = keyword
        self._send(MSG_ID_FRIEND_SEARCH_REQ, req)

    def send_add_request(self, target_id: int):
        """发送好友请求"""
        req = friend_pb2.FriendAddReq()
        req.target_id = target_id
        self._send(MSG_ID_FRIEND_ADD_REQ, req)

    def accept_request(self, sender_id: int):
        """接受好友请求"""
        req = friend_pb2.FriendAcceptReq()
        req.sender_id = sender_id
        self._send(MSG_ID_FRIEND_ACCEPT_REQ, req)

    def reject_request(self, sender_id: int):
        """拒绝好友请求"""
        req = friend_pb2.FriendRejectReq()
        req.sender_id = sender_id
        self._send(MSG_ID_FRIEND_REJECT_REQ, req)

    def delete_friend(self, target_id: int):
        """删除好友"""
        req = friend_pb2.FriendDeleteReq()
        req.target_id = target_id
        self._send(MSG_ID_FRIEND_DELETE_REQ, req)

    def request_list(self):
        """请求好友列表"""
        req = friend_pb2.FriendListReq()
        self._send(MSG_ID_FRIEND_LIST_REQ, req)

    def send_chat(self, receiver_id: int, msg_type: int, content: str):
        """发送私聊消息"""
        req = friend_pb2.FriendChatReq()
        req.receiver_id = receiver_id
        req.msg_type = msg_type
        req.content = content
        self._send(MSG_ID_FRIEND_CHAT_REQ, req)

    def request_history(self, target_id: int):
        """请求聊天记录"""
        req = friend_pb2.FriendChatHistoryReq()
        req.target_id = target_id
        self._send(MSG_ID_FRIEND_CHAT_HISTORY_REQ, req)

    def send_gift(self, receiver_id: int, item_id: int, count: int):
        """赠送物品"""
        req = friend_pb2.FriendGiftReq()
        req.receiver_id = receiver_id
        req.item_id = item_id
        req.count = count
        self._send(MSG_ID_FRIEND_GIFT_REQ, req)

    def visit_farm(self, owner_id: int):
        """访问好友农场"""
        req = friend_pb2.FriendVisitReq()
        req.owner_id = owner_id
        self._send(MSG_ID_FRIEND_VISIT_REQ, req)

    def visit_action(self, owner_id: int, action_type: int, target_x: int, target_y: int):
        """农场访问操作"""
        req = friend_pb2.FriendVisitActionReq()
        req.owner_id = owner_id
        req.action_type = action_type
        req.target_x = target_x
        req.target_y = target_y
        self._send(MSG_ID_FRIEND_VISIT_ACTION_REQ, req)

    def request_recommendations(self):
        """请求推荐好友"""
        req = friend_pb2.FriendRecommendReq()
        self._send(MSG_ID_FRIEND_RECOMMEND_REQ, req)

    def block_player(self, target_id: int):
        """拉黑玩家"""
        req = friend_pb2.FriendBlockReq()
        req.target_id = target_id
        self._send(MSG_ID_FRIEND_BLOCK_REQ, req)

    def unblock_player(self, target_id: int):
        """取消拉黑"""
        req = friend_pb2.FriendUnblockReq()
        req.target_id = target_id
        self._send(MSG_ID_FRIEND_UNBLOCK_REQ, req)
