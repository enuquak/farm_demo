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
    MSG_ID_ATTACK_NOTIFY, MSG_ID_MONSTER_SPAWN_NOTIFY,
    MSG_ID_MONSTER_DEATH_NOTIFY, MSG_ID_MONSTER_MOVE_NOTIFY,
    MSG_ID_MONSTER_ATTACK_NOTIFY, MSG_ID_PLAYER_HP_UPDATE,
    MSG_ID_PLAYER_DEATH_NOTIFY,
    MSG_ID_CHAT_MESSAGE, MSG_ID_CHAT_SEND_RESP,
    MSG_ID_TEAM_CREATE_RESP, MSG_ID_TEAM_DISBAND_RESP,
    MSG_ID_TEAM_INVITE_RESP, MSG_ID_TEAM_INVITE_NOTIFY,
    MSG_ID_TEAM_ACCEPT_RESP, MSG_ID_TEAM_REJECT_RESP,
    MSG_ID_TEAM_LEAVE_RESP, MSG_ID_TEAM_KICK_RESP,
    MSG_ID_TEAM_INFO_RESP, MSG_ID_TEAM_MEMBER_UPDATE,
    MSG_ID_TEAM_LEADER_CHANGE, MSG_ID_TEAM_STATUS_UPDATE,
    MSG_ID_FRIEND_SEARCH_RESP, MSG_ID_FRIEND_ADD_RESP, MSG_ID_FRIEND_ADD_NOTIFY,
    MSG_ID_FRIEND_ACCEPT_RESP, MSG_ID_FRIEND_REJECT_RESP, MSG_ID_FRIEND_DELETE_RESP,
    MSG_ID_FRIEND_LIST_RESP, MSG_ID_FRIEND_ONLINE_NOTIFY, MSG_ID_FRIEND_OFFLINE_NOTIFY,
    MSG_ID_FRIEND_CHAT_RESP, MSG_ID_FRIEND_CHAT_NOTIFY, MSG_ID_FRIEND_CHAT_HISTORY_RESP,
    MSG_ID_FRIEND_GIFT_RESP, MSG_ID_FRIEND_GIFT_NOTIFY,
    MSG_ID_FRIEND_VISIT_RESP, MSG_ID_FRIEND_VISIT_ACTION_RESP,
    MSG_ID_FRIEND_RECOMMEND_RESP, MSG_ID_FRIEND_BLOCK_RESP, MSG_ID_FRIEND_UNBLOCK_RESP,
)

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'common', 'proto', 'generated'))
import player_pb2
import base_pb2
import friend_pb2

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
        on_attack_notify=None,
        on_monster_spawn=None,
        on_monster_death=None,
        on_monster_move=None,
        on_monster_attack=None,
        on_player_hp_update=None,
        on_player_death=None,
        on_chat_message: Callable[[bytes], None] = None,
        on_chat_send_resp: Callable[[bytes], None] = None,
        on_team_create_resp: Callable[[bytes], None] = None,
        on_team_disband_resp: Callable[[bytes], None] = None,
        on_team_invite_resp: Callable[[bytes], None] = None,
        on_team_invite_notify: Callable[[bytes], None] = None,
        on_team_accept_resp: Callable[[bytes], None] = None,
        on_team_reject_resp: Callable[[bytes], None] = None,
        on_team_leave_resp: Callable[[bytes], None] = None,
        on_team_kick_resp: Callable[[bytes], None] = None,
        on_team_info_resp: Callable[[bytes], None] = None,
        on_team_member_update: Callable[[bytes], None] = None,
        on_team_leader_change: Callable[[bytes], None] = None,
        on_team_status_update: Callable[[bytes], None] = None,
        on_friend_search_resp: Callable[[Any], None] = None,
        on_friend_add_resp: Callable[[Any], None] = None,
        on_friend_add_notify: Callable[[Any], None] = None,
        on_friend_accept_resp: Callable[[Any], None] = None,
        on_friend_reject_resp: Callable[[Any], None] = None,
        on_friend_delete_resp: Callable[[Any], None] = None,
        on_friend_list_resp: Callable[[Any], None] = None,
        on_friend_online_notify: Callable[[Any], None] = None,
        on_friend_offline_notify: Callable[[Any], None] = None,
        on_friend_chat_resp: Callable[[Any], None] = None,
        on_friend_chat_notify: Callable[[Any], None] = None,
        on_friend_chat_history_resp: Callable[[Any], None] = None,
        on_friend_gift_resp: Callable[[Any], None] = None,
        on_friend_gift_notify: Callable[[Any], None] = None,
        on_friend_visit_resp: Callable[[Any], None] = None,
        on_friend_visit_action_resp: Callable[[Any], None] = None,
        on_friend_recommend_resp: Callable[[Any], None] = None,
        on_friend_block_resp: Callable[[Any], None] = None,
        on_friend_unblock_resp: Callable[[Any], None] = None,
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
            "on_attack_notify": on_attack_notify,
            "on_monster_spawn": on_monster_spawn,
            "on_monster_death": on_monster_death,
            "on_monster_move": on_monster_move,
            "on_monster_attack": on_monster_attack,
            "on_player_hp_update": on_player_hp_update,
            "on_player_death": on_player_death,
            "on_chat_message": on_chat_message,
            "on_chat_send_resp": on_chat_send_resp,
            "on_team_create_resp": on_team_create_resp,
            "on_team_disband_resp": on_team_disband_resp,
            "on_team_invite_resp": on_team_invite_resp,
            "on_team_invite_notify": on_team_invite_notify,
            "on_team_accept_resp": on_team_accept_resp,
            "on_team_reject_resp": on_team_reject_resp,
            "on_team_leave_resp": on_team_leave_resp,
            "on_team_kick_resp": on_team_kick_resp,
            "on_team_info_resp": on_team_info_resp,
            "on_team_member_update": on_team_member_update,
            "on_team_leader_change": on_team_leader_change,
            "on_team_status_update": on_team_status_update,
            "on_friend_search_resp": on_friend_search_resp,
            "on_friend_add_resp": on_friend_add_resp,
            "on_friend_add_notify": on_friend_add_notify,
            "on_friend_accept_resp": on_friend_accept_resp,
            "on_friend_reject_resp": on_friend_reject_resp,
            "on_friend_delete_resp": on_friend_delete_resp,
            "on_friend_list_resp": on_friend_list_resp,
            "on_friend_online_notify": on_friend_online_notify,
            "on_friend_offline_notify": on_friend_offline_notify,
            "on_friend_chat_resp": on_friend_chat_resp,
            "on_friend_chat_notify": on_friend_chat_notify,
            "on_friend_chat_history_resp": on_friend_chat_history_resp,
            "on_friend_gift_resp": on_friend_gift_resp,
            "on_friend_gift_notify": on_friend_gift_notify,
            "on_friend_visit_resp": on_friend_visit_resp,
            "on_friend_visit_action_resp": on_friend_visit_action_resp,
            "on_friend_recommend_resp": on_friend_recommend_resp,
            "on_friend_block_resp": on_friend_block_resp,
            "on_friend_unblock_resp": on_friend_unblock_resp,
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
            MSG_ID_ATTACK_NOTIFY: self._handle_attack_notify,
            MSG_ID_MONSTER_SPAWN_NOTIFY: self._handle_monster_spawn,
            MSG_ID_MONSTER_DEATH_NOTIFY: self._handle_monster_death,
            MSG_ID_MONSTER_MOVE_NOTIFY: self._handle_monster_move,
            MSG_ID_MONSTER_ATTACK_NOTIFY: self._handle_monster_attack,
            MSG_ID_PLAYER_HP_UPDATE: self._handle_player_hp_update,
            MSG_ID_PLAYER_DEATH_NOTIFY: self._handle_player_death,
            MSG_ID_CHAT_MESSAGE: self._handle_chat_message,
            MSG_ID_CHAT_SEND_RESP: self._handle_chat_send_resp,
            MSG_ID_TEAM_CREATE_RESP: self._handle_team_create_resp,
            MSG_ID_TEAM_DISBAND_RESP: self._handle_team_disband_resp,
            MSG_ID_TEAM_INVITE_RESP: self._handle_team_invite_resp,
            MSG_ID_TEAM_INVITE_NOTIFY: self._handle_team_invite_notify,
            MSG_ID_TEAM_ACCEPT_RESP: self._handle_team_accept_resp,
            MSG_ID_TEAM_REJECT_RESP: self._handle_team_reject_resp,
            MSG_ID_TEAM_LEAVE_RESP: self._handle_team_leave_resp,
            MSG_ID_TEAM_KICK_RESP: self._handle_team_kick_resp,
            MSG_ID_TEAM_INFO_RESP: self._handle_team_info_resp,
            MSG_ID_TEAM_MEMBER_UPDATE: self._handle_team_member_update,
            MSG_ID_TEAM_LEADER_CHANGE: self._handle_team_leader_change,
            MSG_ID_TEAM_STATUS_UPDATE: self._handle_team_status_update,
            MSG_ID_FRIEND_SEARCH_RESP: self._handle_friend_search_resp,
            MSG_ID_FRIEND_ADD_RESP: self._handle_friend_add_resp,
            MSG_ID_FRIEND_ADD_NOTIFY: self._handle_friend_add_notify,
            MSG_ID_FRIEND_ACCEPT_RESP: self._handle_friend_accept_resp,
            MSG_ID_FRIEND_REJECT_RESP: self._handle_friend_reject_resp,
            MSG_ID_FRIEND_DELETE_RESP: self._handle_friend_delete_resp,
            MSG_ID_FRIEND_LIST_RESP: self._handle_friend_list_resp,
            MSG_ID_FRIEND_ONLINE_NOTIFY: self._handle_friend_online_notify,
            MSG_ID_FRIEND_OFFLINE_NOTIFY: self._handle_friend_offline_notify,
            MSG_ID_FRIEND_CHAT_RESP: self._handle_friend_chat_resp,
            MSG_ID_FRIEND_CHAT_NOTIFY: self._handle_friend_chat_notify,
            MSG_ID_FRIEND_CHAT_HISTORY_RESP: self._handle_friend_chat_history_resp,
            MSG_ID_FRIEND_GIFT_RESP: self._handle_friend_gift_resp,
            MSG_ID_FRIEND_GIFT_NOTIFY: self._handle_friend_gift_notify,
            MSG_ID_FRIEND_VISIT_RESP: self._handle_friend_visit_resp,
            MSG_ID_FRIEND_VISIT_ACTION_RESP: self._handle_friend_visit_action_resp,
            MSG_ID_FRIEND_RECOMMEND_RESP: self._handle_friend_recommend_resp,
            MSG_ID_FRIEND_BLOCK_RESP: self._handle_friend_block_resp,
            MSG_ID_FRIEND_UNBLOCK_RESP: self._handle_friend_unblock_resp,
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

    # ========== 战斗系统消息处理 ==========

    def _handle_attack_notify(self, payload: bytes):
        """处理攻击结果通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_attack_notify"):
                self._callbacks["on_attack_notify"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse AttackNotify: {e}")

    def _handle_monster_spawn(self, payload: bytes):
        """处理怪物刷新通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_monster_spawn"):
                self._callbacks["on_monster_spawn"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse MonsterSpawnNotify: {e}")

    def _handle_monster_death(self, payload: bytes):
        """处理怪物死亡通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_monster_death"):
                self._callbacks["on_monster_death"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse MonsterDeathNotify: {e}")

    def _handle_monster_move(self, payload: bytes):
        """处理怪物移动通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_monster_move"):
                self._callbacks["on_monster_move"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse MonsterMoveNotify: {e}")

    def _handle_monster_attack(self, payload: bytes):
        """处理怪物攻击通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_monster_attack"):
                self._callbacks["on_monster_attack"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse MonsterAttackNotify: {e}")

    def _handle_player_hp_update(self, payload: bytes):
        """处理玩家HP更新"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_player_hp_update"):
                self._callbacks["on_player_hp_update"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse PlayerHpUpdate: {e}")

    def _handle_player_death(self, payload: bytes):
        """处理玩家死亡通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            import json
            data = json.loads(player_msg.payload)
            if self._callbacks.get("on_player_death"):
                self._callbacks["on_player_death"](data)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse PlayerDeathNotify: {e}")

    # ========== 聊天系统消息处理 ==========

    def _handle_chat_message(self, payload: bytes):
        """处理聊天消息推送"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_chat_message"):
                self._callbacks["on_chat_message"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse ChatMessage: {e}")

    def _handle_chat_send_resp(self, payload: bytes):
        """处理聊天发送响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_chat_send_resp"):
                self._callbacks["on_chat_send_resp"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse ChatSendResp: {e}")

    # ========== 组队系统消息处理 ==========

    def _handle_team_create_resp(self, payload: bytes):
        """处理创建队伍响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_team_create_resp"):
                self._callbacks["on_team_create_resp"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse TeamCreateResp: {e}")

    def _handle_team_disband_resp(self, payload: bytes):
        """处理解散队伍响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_team_disband_resp"):
                self._callbacks["on_team_disband_resp"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse TeamDisbandResp: {e}")

    def _handle_team_invite_resp(self, payload: bytes):
        """处理邀请响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_team_invite_resp"):
                self._callbacks["on_team_invite_resp"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse TeamInviteResp: {e}")

    def _handle_team_invite_notify(self, payload: bytes):
        """处理收到邀请通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_team_invite_notify"):
                self._callbacks["on_team_invite_notify"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse TeamInviteNotify: {e}")

    def _handle_team_accept_resp(self, payload: bytes):
        """处理接受邀请响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_team_accept_resp"):
                self._callbacks["on_team_accept_resp"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse TeamAcceptResp: {e}")

    def _handle_team_reject_resp(self, payload: bytes):
        """处理拒绝邀请响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_team_reject_resp"):
                self._callbacks["on_team_reject_resp"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse TeamRejectResp: {e}")

    def _handle_team_leave_resp(self, payload: bytes):
        """处理离开队伍响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_team_leave_resp"):
                self._callbacks["on_team_leave_resp"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse TeamLeaveResp: {e}")

    def _handle_team_kick_resp(self, payload: bytes):
        """处理踢出响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_team_kick_resp"):
                self._callbacks["on_team_kick_resp"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse TeamKickResp: {e}")

    def _handle_team_info_resp(self, payload: bytes):
        """处理队伍信息响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_team_info_resp"):
                self._callbacks["on_team_info_resp"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse TeamInfoResp: {e}")

    def _handle_team_member_update(self, payload: bytes):
        """处理成员变更通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_team_member_update"):
                self._callbacks["on_team_member_update"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse TeamMemberUpdate: {e}")

    def _handle_team_leader_change(self, payload: bytes):
        """处理队长变更通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_team_leader_change"):
                self._callbacks["on_team_leader_change"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse TeamLeaderChange: {e}")

    def _handle_team_status_update(self, payload: bytes):
        """处理队伍状态变更"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            if self._callbacks.get("on_team_status_update"):
                self._callbacks["on_team_status_update"](player_msg.payload)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse TeamStatusUpdate: {e}")

    # ========== 好友系统消息处理 ==========

    def _handle_friend_search_resp(self, payload: bytes):
        """处理搜索玩家响应"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendSearchResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendSearchResp: code={resp.code}, {len(resp.results)} results")
            if self._callbacks.get("on_friend_search_resp"):
                self._callbacks["on_friend_search_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendSearchResp: {e}")

    def _handle_friend_add_resp(self, payload: bytes):
        """处理好友请求结果"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendAddResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendAddResp: code={resp.code}, msg={resp.msg}")
            if self._callbacks.get("on_friend_add_resp"):
                self._callbacks["on_friend_add_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendAddResp: {e}")

    def _handle_friend_add_notify(self, payload: bytes):
        """处理收到好友请求通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            notify = friend_pb2.FriendAddNotify()
            notify.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendAddNotify: sender={notify.request.sender_name}")
            if self._callbacks.get("on_friend_add_notify"):
                self._callbacks["on_friend_add_notify"](notify)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendAddNotify: {e}")

    def _handle_friend_accept_resp(self, payload: bytes):
        """处理接受好友结果"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendAcceptResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendAcceptResp: code={resp.code}")
            if self._callbacks.get("on_friend_accept_resp"):
                self._callbacks["on_friend_accept_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendAcceptResp: {e}")

    def _handle_friend_reject_resp(self, payload: bytes):
        """处理拒绝好友结果"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendRejectResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendRejectResp: code={resp.code}")
            if self._callbacks.get("on_friend_reject_resp"):
                self._callbacks["on_friend_reject_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendRejectResp: {e}")

    def _handle_friend_delete_resp(self, payload: bytes):
        """处理删除好友结果"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendDeleteResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendDeleteResp: code={resp.code}")
            if self._callbacks.get("on_friend_delete_resp"):
                self._callbacks["on_friend_delete_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendDeleteResp: {e}")

    def _handle_friend_list_resp(self, payload: bytes):
        """处理好友列表数据"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendListResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendListResp: {len(resp.friends)} friends, {len(resp.pending_requests)} pending")
            if self._callbacks.get("on_friend_list_resp"):
                self._callbacks["on_friend_list_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendListResp: {e}")

    def _handle_friend_online_notify(self, payload: bytes):
        """处理好友上线通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            notify = friend_pb2.FriendOnlineNotify()
            notify.ParseFromString(player_msg.payload)
            logger.debug(f"[NetworkDispatcher]FriendOnlineNotify: player_id={notify.player_id}, name={notify.role_name}")
            if self._callbacks.get("on_friend_online_notify"):
                self._callbacks["on_friend_online_notify"](notify)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendOnlineNotify: {e}")

    def _handle_friend_offline_notify(self, payload: bytes):
        """处理好友下线通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            notify = friend_pb2.FriendOfflineNotify()
            notify.ParseFromString(player_msg.payload)
            logger.debug(f"[NetworkDispatcher]FriendOfflineNotify: player_id={notify.player_id}")
            if self._callbacks.get("on_friend_offline_notify"):
                self._callbacks["on_friend_offline_notify"](notify)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendOfflineNotify: {e}")

    def _handle_friend_chat_resp(self, payload: bytes):
        """处理私聊发送结果"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendChatResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendChatResp: code={resp.code}")
            if self._callbacks.get("on_friend_chat_resp"):
                self._callbacks["on_friend_chat_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendChatResp: {e}")

    def _handle_friend_chat_notify(self, payload: bytes):
        """处理收到私聊消息"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            notify = friend_pb2.FriendChatNotify()
            notify.ParseFromString(player_msg.payload)
            logger.debug(f"[NetworkDispatcher]FriendChatNotify: from={notify.sender_name}, content={notify.content[:20]}")
            if self._callbacks.get("on_friend_chat_notify"):
                self._callbacks["on_friend_chat_notify"](notify)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendChatNotify: {e}")

    def _handle_friend_chat_history_resp(self, payload: bytes):
        """处理聊天记录数据"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendChatHistoryResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendChatHistoryResp: code={resp.code}, {len(resp.messages)} messages")
            if self._callbacks.get("on_friend_chat_history_resp"):
                self._callbacks["on_friend_chat_history_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendChatHistoryResp: {e}")

    def _handle_friend_gift_resp(self, payload: bytes):
        """处理赠送物品结果"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendGiftResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendGiftResp: code={resp.code}, msg={resp.msg}")
            if self._callbacks.get("on_friend_gift_resp"):
                self._callbacks["on_friend_gift_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendGiftResp: {e}")

    def _handle_friend_gift_notify(self, payload: bytes):
        """处理收到礼物通知"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            notify = friend_pb2.FriendGiftNotify()
            notify.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendGiftNotify: from={notify.sender_name}, item={notify.item_id}x{notify.count}")
            if self._callbacks.get("on_friend_gift_notify"):
                self._callbacks["on_friend_gift_notify"](notify)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendGiftNotify: {e}")

    def _handle_friend_visit_resp(self, payload: bytes):
        """处理访问农场结果"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendVisitResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendVisitResp: code={resp.code}, owner={resp.owner_name}, scene={resp.scene_id}")
            if self._callbacks.get("on_friend_visit_resp"):
                self._callbacks["on_friend_visit_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendVisitResp: {e}")

    def _handle_friend_visit_action_resp(self, payload: bytes):
        """处理农场操作结果"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendVisitActionResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendVisitActionResp: code={resp.code}, msg={resp.msg}")
            if self._callbacks.get("on_friend_visit_action_resp"):
                self._callbacks["on_friend_visit_action_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendVisitActionResp: {e}")

    def _handle_friend_recommend_resp(self, payload: bytes):
        """处理推荐好友列表"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendRecommendResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendRecommendResp: code={resp.code}, {len(resp.recommendations)} recommendations")
            if self._callbacks.get("on_friend_recommend_resp"):
                self._callbacks["on_friend_recommend_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendRecommendResp: {e}")

    def _handle_friend_block_resp(self, payload: bytes):
        """处理拉黑结果"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendBlockResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendBlockResp: code={resp.code}")
            if self._callbacks.get("on_friend_block_resp"):
                self._callbacks["on_friend_block_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendBlockResp: {e}")

    def _handle_friend_unblock_resp(self, payload: bytes):
        """处理取消拉黑结果"""
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)
            resp = friend_pb2.FriendUnblockResp()
            resp.ParseFromString(player_msg.payload)
            logger.info(f"[NetworkDispatcher]FriendUnblockResp: code={resp.code}")
            if self._callbacks.get("on_friend_unblock_resp"):
                self._callbacks["on_friend_unblock_resp"](resp)
        except Exception as e:
            logger.error(f"[NetworkDispatcher]Failed to parse FriendUnblockResp: {e}")
