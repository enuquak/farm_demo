"""
任务系统客户端处理器
"""
import logging
from typing import Dict, List, Optional, Any

from .message_ids import (
    MSG_ID_QUEST_ACCEPT_REQ, MSG_ID_QUEST_SUBMIT_REQ,
    MSG_ID_QUEST_ABANDON_REQ,
)

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'common', 'proto', 'generated'))
import player_pb2
import base_pb2

logger = logging.getLogger("client.quest_handler")


class QuestHandler:
    """客户端任务处理器，负责发送任务请求和处理任务响应。"""

    def __init__(self, connection):
        self._connection = connection
        self._active_quests: Dict[str, Any] = {}

    def request_accept_quest(self, quest_id: str):
        """发送任务接受请求。"""
        try:
            req = player_pb2.QuestAcceptReq()
            req.quest_id = quest_id

            player_msg = base_pb2.PlayerMsg()
            player_msg.msg_id = MSG_ID_QUEST_ACCEPT_REQ
            player_msg.payload = req.SerializeToString()

            self._connection.send_message(MSG_ID_QUEST_ACCEPT_REQ, player_msg.SerializeToString())
            logger.info(f"[QuestHandler]QuestAcceptReq sent: quest_id={quest_id}")
        except Exception as e:
            logger.error(f"[QuestHandler]Failed to send QuestAcceptReq: {e}")

    def request_submit_quest(self, quest_id: str):
        """发送任务提交请求。"""
        try:
            req = player_pb2.QuestSubmitReq()
            req.quest_id = quest_id

            player_msg = base_pb2.PlayerMsg()
            player_msg.msg_id = MSG_ID_QUEST_SUBMIT_REQ
            player_msg.payload = req.SerializeToString()

            self._connection.send_message(MSG_ID_QUEST_SUBMIT_REQ, player_msg.SerializeToString())
            logger.info(f"[QuestHandler]QuestSubmitReq sent: quest_id={quest_id}")
        except Exception as e:
            logger.error(f"[QuestHandler]Failed to send QuestSubmitReq: {e}")

    def request_abandon_quest(self, quest_id: str):
        """发送任务放弃请求。"""
        try:
            req = player_pb2.QuestAbandonReq()
            req.quest_id = quest_id

            player_msg = base_pb2.PlayerMsg()
            player_msg.msg_id = MSG_ID_QUEST_ABANDON_REQ
            player_msg.payload = req.SerializeToString()

            self._connection.send_message(MSG_ID_QUEST_ABANDON_REQ, player_msg.SerializeToString())
            logger.info(f"[QuestHandler]QuestAbandonReq sent: quest_id={quest_id}")
        except Exception as e:
            logger.error(f"[QuestHandler]Failed to send QuestAbandonReq: {e}")

    def handle_quest_accept_resp(self, resp):
        """处理任务接受响应，更新活跃任务列表。"""
        if resp.code == 0:
            quest_id = resp.task_info.task_id
            self._active_quests[quest_id] = resp.task_info
            logger.info(f"[QuestHandler]Quest accepted: quest_id={quest_id}, state={resp.task_info.state}")
        else:
            logger.warning(f"[QuestHandler]Quest accept failed: code={resp.code}, msg={resp.msg}")

    def handle_quest_submit_resp(self, resp):
        """处理任务提交响应，从活跃任务中移除。"""
        if resp.code == 0:
            logger.info(f"[QuestHandler]Quest submitted successfully, rewards: gold={resp.rewards.gold}, exp={resp.rewards.exp}")
        else:
            logger.warning(f"[QuestHandler]Quest submit failed: code={resp.code}, msg={resp.msg}")

    def handle_quest_abandon_resp(self, resp):
        """处理任务放弃响应。"""
        if resp.code == 0:
            logger.info(f"[QuestHandler]Quest abandoned successfully")
        else:
            logger.warning(f"[QuestHandler]Quest abandon failed: code={resp.code}, msg={resp.msg}")

    def handle_quest_sync_notify(self, notify):
        """同步所有任务数据。"""
        self._active_quests.clear()
        for task_info in notify.task_infos:
            self._active_quests[task_info.task_id] = task_info
            logger.debug(f"[QuestHandler]Synced quest: {task_info.task_id}, state={task_info.state}")
        logger.info(f"[QuestHandler]Quest sync completed: {len(self._active_quests)} quests")

    def handle_quest_progress_notify(self, notify):
        """更新任务进度。"""
        quest_id = notify.quest_id
        if quest_id in self._active_quests:
            task_info = self._active_quests[quest_id]
            for key, value in notify.progress.items():
                task_info.progress[key] = value
            logger.info(f"[QuestHandler]Quest progress updated: quest_id={quest_id}")
        else:
            logger.warning(f"[QuestHandler]Progress update for unknown quest: quest_id={quest_id}")

    @property
    def active_quests(self) -> Dict[str, Any]:
        """返回当前活跃的任务字典。"""
        return self._active_quests
