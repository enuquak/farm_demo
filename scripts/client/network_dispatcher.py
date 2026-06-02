"""
网络消息分发器模块
使用 dict 分发表将收到的网络消息路由到对应的处理函数。
从 GameScene 中提取的网络消息处理逻辑。
"""
import logging
from typing import Callable, Dict, Any

from .message_ids import (
    MSG_ID_MAP_DATA_NOTIFY, MSG_ID_POSITION_CORRECT,
    MSG_ID_ITEM_USE_RESP, MSG_ID_SCENE_CHANGE_RESP,
    MSG_ID_CLOCK_SYNC, MSG_ID_FORCE_SLEEP_NOTIFY, MSG_ID_FORCE_SLEEP_READY,
    MSG_ID_DROP_ITEM_SYNC, MSG_ID_INVENTORY_SYNC,
    MSG_ID_NOTIFY_TOAST,
    MSG_ID_QUEST_ACCEPT_RESP, MSG_ID_QUEST_SUBMIT_RESP,
    MSG_ID_QUEST_ABANDON_RESP, MSG_ID_QUEST_SYNC_NOTIFY,
    MSG_ID_QUEST_PROGRESS_NOTIFY,
)

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'common', 'proto', 'generated'))
import player_pb2
import base_pb2

logger = logging.getLogger("client.network_dispatcher")


class NetworkMessageDispatcher:
    """
    网络消息分发器
    维护 msg_id -> handler 的分发表，每帧从连接中拉取消息并分发。
    """

    def __init__(
        self,
        connection,
        on_map_data_notify: Callable[[], None],
        on_position_correct: Callable[[float, float], None],
        on_item_use_resp: Callable[[Any], None],
        on_scene_change_resp: Callable[[Any], None],
        on_clock_sync: Callable[[int, int], None],
        on_force_sleep_notify: Callable[[int], None],
        on_drop_item_sync: Callable[[Any], None] = None,
        on_inventory_sync: Callable[[str], None] = None,
        on_notify_toast: Callable[[Any], None] = None,
        on_gift_resp: Callable[[Any], None] = None,
        on_affection_sync: Callable[[Any], None] = None,
        on_quest_accept_resp: Callable[[Any], None] = None,
        on_quest_submit_resp: Callable[[Any], None] = None,
        on_quest_abandon_resp: Callable[[Any], None] = None,
        on_quest_sync_notify: Callable[[Any], None] = None,
        on_quest_progress_notify: Callable[[Any], None] = None,
    ):
        """
        初始化消息分发器

        Args:
            connection: GateConnection 连接对象
            on_map_data_notify: 地图数据通知回调
            on_position_correct: 位置纠正回调 (target_x, target_y)
            on_item_use_resp: 物品使用响应回调 (item_resp)
            on_scene_change_resp: 场景切换响应回调 (resp)
            on_clock_sync: 时钟同步回调 (day, time_slot)
            on_force_sleep_notify: 强制睡觉通知回调 (day)
            on_drop_item_sync: 掉落物同步回调 (drop_sync)
            on_inventory_sync: 背包同步回调 (inventory_json)
            on_gift_resp: 送礼响应回调 (gift_resp)
            on_affection_sync: 好感度同步回调 (affection_sync)
            on_quest_accept_resp: 任务接受响应回调 (resp)
            on_quest_submit_resp: 任务提交响应回调 (resp)
            on_quest_abandon_resp: 任务放弃响应回调 (resp)
            on_quest_sync_notify: 任务同步通知回调 (notify)
            on_quest_progress_notify: 任务进度通知回调 (notify)
        """
        self._connection = connection
        self._callbacks = {
            "on_map_data_notify": on_map_data_notify,
            "on_position_correct": on_position_correct,
            "on_item_use_resp": on_item_use_resp,
            "on_scene_change_resp": on_scene_change_resp,
            "on_clock_sync": on_clock_sync,
            "on_force_sleep_notify": on_force_sleep_notify,
            "on_drop_item_sync": on_drop_item_sync,
            "on_inventory_sync": on_inventory_sync,
            "on_notify_toast": on_notify_toast,
            "on_gift_resp": on_gift_resp,
            "on_affection_sync": on_affection_sync,
            "on_quest_accept_resp": on_quest_accept_resp,
            "on_quest_submit_resp": on_quest_submit_resp,
            "on_quest_abandon_resp": on_quest_abandon_resp,
            "on_quest_sync_notify": on_quest_sync_notify,
            "on_quest_progress_notify": on_quest_progress_notify,
        }

        # 分发表: msg_id -> handler 方法
        self._dispatch_table: Dict[int, Callable] = {
            MSG_ID_MAP_DATA_NOTIFY: self._handle_map_data_notify,
            MSG_ID_POSITION_CORRECT: self._handle_position_correct,
            MSG_ID_ITEM_USE_RESP: self._handle_item_use_resp,
            MSG_ID_SCENE_CHANGE_RESP: self._handle_scene_change_resp,
            MSG_ID_CLOCK_SYNC: self._handle_clock_sync,
            MSG_ID_FORCE_SLEEP_NOTIFY: self._handle_force_sleep_notify,
            MSG_ID_DROP_ITEM_SYNC: self._handle_drop_item_sync,
            MSG_ID_INVENTORY_SYNC: self._handle_inventory_sync,
            MSG_ID_NOTIFY_TOAST: self._handle_notify_toast,
            # NPC 对话相关 (msg_ids.py auto-generated, using integer values directly)
            3202: self._handle_gift_resp,          # MSG_ID_GIFT_RESP
            3203: self._handle_affection_sync,     # MSG_ID_AFFECTION_SYNC
            MSG_ID_QUEST_ACCEPT_RESP: self._handle_quest_accept_resp,
            MSG_ID_QUEST_SUBMIT_RESP: self._handle_quest_submit_resp,
            MSG_ID_QUEST_ABANDON_RESP: self._handle_quest_abandon_resp,
            MSG_ID_QUEST_SYNC_NOTIFY: self._handle_quest_sync_notify,
            MSG_ID_QUEST_PROGRESS_NOTIFY: self._handle_quest_progress_notify,
        }

    def dispatch_pending(self, connection=None):
        """
        分发所有待处理的网络消息

        Args:
            connection: 可选的连接对象，未提供时使用初始化时的连接
        """
        conn = connection or self._connection
        if not conn:
            return

        messages = conn.recv_all_messages()
        for msg_id, payload in messages:
            handler = self._dispatch_table.get(msg_id)
            if handler:
                handler(payload)
            else:
                logger.debug(f"[NetworkDispatcher]Unhandled msg_id={msg_id}")

    def _handle_map_data_notify(self, payload: bytes):
        """处理地图数据通知"""
        logger.info("[NetworkDispatcher]MapDataNotify received (TMX map is used, ignoring server map data)")
        self._callbacks["on_map_data_notify"]()

    def _handle_position_correct(self, payload: bytes):
        """处理位置纠正消息"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            pos_correct = player_pb2.PositionCorrect()
            pos_correct.ParseFromString(player_msg.payload)

            self._callbacks["on_position_correct"](pos_correct.pos_x, pos_correct.pos_y)

            logger.info(f"[NetworkDispatcher]PositionCorrect received: ({pos_correct.pos_x:.1f}, {pos_correct.pos_y:.1f})")

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse PositionCorrect: {e}")

    def _handle_item_use_resp(self, payload: bytes):
        """处理物品使用响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            item_resp = player_pb2.ItemUseResp()
            item_resp.ParseFromString(player_msg.payload)

            logger.info(f"[NetworkDispatcher]ItemUseResp received: code={item_resp.code}, msg={item_resp.msg}")

            self._callbacks["on_item_use_resp"](item_resp)

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse ItemUseResp: {e}")

    def _handle_scene_change_resp(self, payload: bytes):
        """处理场景切换响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            resp = player_pb2.SceneChangeResp()
            resp.ParseFromString(player_msg.payload)

            logger.info(f"[NetworkDispatcher]SceneChangeResp received: code={resp.code}, "
                       f"target={resp.target_scene}, spawn=({resp.spawn_x},{resp.spawn_y})")

            self._callbacks["on_scene_change_resp"](resp)

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse SceneChangeResp: {e}")

    def _handle_clock_sync(self, payload: bytes):
        """处理时钟同步消息"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            clock_sync = player_pb2.ClockSync()
            clock_sync.ParseFromString(player_msg.payload)

            self._callbacks["on_clock_sync"](clock_sync.day, clock_sync.time_slot)

            logger.debug(f"[NetworkDispatcher]ClockSync received: day={clock_sync.day}, "
                        f"slot={clock_sync.time_slot}, paused={clock_sync.paused}")

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse ClockSync: {e}")

    def _handle_force_sleep_notify(self, payload: bytes):
        """处理强制睡觉通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            notify = player_pb2.ForceSleepNotify()
            notify.ParseFromString(player_msg.payload)

            logger.info(f"[NetworkDispatcher]ForceSleepNotify received: day={notify.day}")

            self._callbacks["on_force_sleep_notify"](notify.day)

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse ForceSleepNotify: {e}")

    def _handle_drop_item_sync(self, payload: bytes):
        """处理掉落物同步消息"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            drop_sync = player_pb2.DropItemSync()
            drop_sync.ParseFromString(player_msg.payload)

            logger.debug(f"[NetworkDispatcher]DropItemSync received: drop_id={drop_sync.drop_id}, "
                        f"item_id={drop_sync.item_id}, action={drop_sync.action}")

            if self._callbacks["on_drop_item_sync"]:
                self._callbacks["on_drop_item_sync"](drop_sync)

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse DropItemSync: {e}")

    def _handle_inventory_sync(self, payload: bytes):
        """处理背包同步消息"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            inv_sync = player_pb2.InventorySync()
            inv_sync.ParseFromString(player_msg.payload)

            logger.info(f"[NetworkDispatcher]InventorySync received, json_len={len(inv_sync.inventory_json)}")

            if self._callbacks["on_inventory_sync"]:
                self._callbacks["on_inventory_sync"](inv_sync.inventory_json)

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse InventorySync: {e}")

    def _handle_notify_toast(self, payload: bytes):
        """处理服务器通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            notify = player_pb2.NotifyToast()
            notify.ParseFromString(player_msg.payload)

            logger.info(f"[NetworkDispatcher]NotifyToast received: type={notify.type}, "
                       f"title={notify.title}, priority={notify.priority}")

            if self._callbacks["on_notify_toast"]:
                self._callbacks["on_notify_toast"](notify)

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse NotifyToast: {e}")

    def _handle_gift_resp(self, payload: bytes):
        """处理送礼响应。"""
        pass  # Placeholder - will be connected when server is ready

    def _handle_affection_sync(self, payload: bytes):
        """处理好感度同步。"""
        pass  # Placeholder - will be connected when server is ready

    def _handle_quest_accept_resp(self, payload: bytes):
        """处理任务接受响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            resp = player_pb2.QuestAcceptResp()
            resp.ParseFromString(player_msg.payload)

            logger.info(f"[NetworkDispatcher]QuestAcceptResp received: code={resp.code}, msg={resp.msg}")

            if self._callbacks["on_quest_accept_resp"]:
                self._callbacks["on_quest_accept_resp"](resp)

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse QuestAcceptResp: {e}")

    def _handle_quest_submit_resp(self, payload: bytes):
        """处理任务提交响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            resp = player_pb2.QuestSubmitResp()
            resp.ParseFromString(player_msg.payload)

            logger.info(f"[NetworkDispatcher]QuestSubmitResp received: code={resp.code}, msg={resp.msg}")

            if self._callbacks["on_quest_submit_resp"]:
                self._callbacks["on_quest_submit_resp"](resp)

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse QuestSubmitResp: {e}")

    def _handle_quest_abandon_resp(self, payload: bytes):
        """处理任务放弃响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            resp = player_pb2.QuestAbandonResp()
            resp.ParseFromString(player_msg.payload)

            logger.info(f"[NetworkDispatcher]QuestAbandonResp received: code={resp.code}, msg={resp.msg}")

            if self._callbacks["on_quest_abandon_resp"]:
                self._callbacks["on_quest_abandon_resp"](resp)

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse QuestAbandonResp: {e}")

    def _handle_quest_sync_notify(self, payload: bytes):
        """处理任务同步通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            notify = player_pb2.QuestSyncNotify()
            notify.ParseFromString(player_msg.payload)

            logger.info(f"[NetworkDispatcher]QuestSyncNotify received: {len(notify.task_infos)} quests")

            if self._callbacks["on_quest_sync_notify"]:
                self._callbacks["on_quest_sync_notify"](notify)

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse QuestSyncNotify: {e}")

    def _handle_quest_progress_notify(self, payload: bytes):
        """处理任务进度通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            notify = player_pb2.QuestProgressNotify()
            notify.ParseFromString(player_msg.payload)

            logger.info(f"[NetworkDispatcher]QuestProgressNotify received: quest_id={notify.quest_id}")

            if self._callbacks["on_quest_progress_notify"]:
                self._callbacks["on_quest_progress_notify"](notify)

        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse QuestProgressNotify: {e}")
