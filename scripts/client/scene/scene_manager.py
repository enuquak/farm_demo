"""
客户端场景管理器
管理当前场景的渲染和切换，支持 Iris 过渡动效
"""
import logging
from typing import Optional, Dict, Any, Tuple

import pygame

from ..constants import TILE_SIZE, ZOOM_FACTOR, ObjectType
from ..tile_map import TileMap
from .scene_defs import SCENE_DEFS, get_portal_at, get_spawn_for_portal
from .scene_transition import SceneTransition, TransitionState

logger = logging.getLogger("client.scene_manager")


class SceneManager:
    """
    客户端场景管理器

    职责:
    - 持有当前场景的 TileMap
    - 管理场景切换流程（Iris 过渡动效）
    - 处理 Portal 检测（客户端触发，发送请求给服务器）
    - 在过渡期间屏蔽游戏输入
    """

    def __init__(self, screen_width: int, screen_height: int, connection=None):
        """
        初始化场景管理器

        Args:
            screen_width: 屏幕宽度（像素）
            screen_height: 屏幕高度（像素）
            connection: GateConnection 连接对象（用于发送消息）
        """
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._connection = connection

        # 当前场景
        self._current_scene_id: Optional[str] = None
        self._current_tile_map: Optional[TileMap] = None

        # 过渡动效
        self._transition = SceneTransition(screen_width, screen_height)

        # 待切换的目标场景（在 IRIS_CLOSE 完成后执行）
        self._pending_switch: Optional[Dict[str, Any]] = None

        logger.info("SceneManager initialized")

    @property
    def current_scene_id(self) -> Optional[str]:
        """当前场景 ID"""
        return self._current_scene_id

    @property
    def tile_map(self) -> Optional[TileMap]:
        """当前场景的 TileMap"""
        return self._current_tile_map

    @property
    def is_transitioning(self) -> bool:
        """是否正在过渡中"""
        return self._transition.is_active

    def is_input_blocked(self) -> bool:
        """是否应屏蔽游戏输入（过渡期间返回 True）"""
        return self._transition.is_active

    def load_scene(self, scene_id: str) -> bool:
        """
        加载指定场景

        Args:
            scene_id: 场景 ID

        Returns:
            是否加载成功
        """
        scene_def = SCENE_DEFS.get(scene_id)
        if scene_def is None:
            logger.error(f"load_scene, unknown scene_id={scene_id}")
            return False

        width = scene_def["width"]
        height = scene_def["height"]
        generate = scene_def["generate"]

        # 生成地图数据
        map_data = generate()

        # 创建 TileMap 并填充数据
        tile_map = TileMap(width, height)
        tile_map.fill_from_data(map_data["ground"], map_data["objects"])

        self._current_scene_id = scene_id
        self._current_tile_map = tile_map

        logger.info(f"load_scene, loaded scene={scene_id}, size={width}x{height}")
        return True

    def load_scene_from_data(self, scene_id: str, ground_data: list, objects_data: list) -> bool:
        """
        从服务器数据加载场景

        Args:
            scene_id: 场景 ID
            ground_data: 地面层数据
            objects_data: 地物层数据

        Returns:
            是否加载成功
        """
        scene_def = SCENE_DEFS.get(scene_id)
        if scene_def is None:
            logger.error(f"load_scene_from_data, unknown scene_id={scene_id}")
            return False

        width = scene_def["width"]
        height = scene_def["height"]

        tile_map = TileMap(width, height)
        tile_map.fill_from_data(ground_data, objects_data)

        self._current_scene_id = scene_id
        self._current_tile_map = tile_map

        logger.info(f"load_scene_from_data, loaded scene={scene_id}, size={width}x{height}")
        return True

    def check_portal_trigger(self, tile_x: int, tile_y: int, direction: str) -> Optional[Dict[str, Any]]:
        """
        检查当前位置和方向是否触发传送门

        Args:
            tile_x: 玩家当前 tile X
            tile_y: 玩家当前 tile Y
            direction: 玩家移动方向

        Returns:
            Portal 配置字典，无匹配时返回 None
        """
        if self._current_scene_id is None:
            return None

        # 检查当前位置的地物类型
        if self._current_tile_map is not None:
            obj_type = self._current_tile_map.get_object(tile_x, tile_y)
            if obj_type is None or obj_type == ObjectType.NONE:
                return None

            # 检查是否是 portal 类型的地物
            from ..constants import OBJECT_PROPERTIES
            props = OBJECT_PROPERTIES.get(obj_type)
            if props is None or props.get("interact_type") != "portal":
                return None

        return get_portal_at(self._current_scene_id, tile_x, tile_y, direction)

    def request_scene_change(self, target_scene: str, target_portal_id: str, player_screen_pos: Tuple[int, int]):
        """
        请求场景切换（发送 SceneChangeReq 并启动过渡动效）

        Args:
            target_scene: 目标场景 ID
            target_portal_id: 目标 Portal ID
            player_screen_pos: 玩家屏幕位置（用于 Iris 动效中心点）
        """
        if self._transition.is_active:
            logger.warning("request_scene_change, transition already active")
            return

        # 发送 SceneChangeReq 给服务器
        if self._connection and self._connection.is_connected:
            self._send_scene_change_req(target_scene, target_portal_id)

        # 记录待切换信息
        self._pending_switch = {
            "target_scene": target_scene,
            "target_portal_id": target_portal_id,
        }

        # 启动 Iris 关闭动效
        self._transition.start(player_screen_pos, self._execute_switch)
        logger.info(f"request_scene_change, target={target_scene}, portal={target_portal_id}")

    def _execute_switch(self):
        """执行实际的场景切换（在 IRIS_CLOSE 完成后调用）"""
        if self._pending_switch is None:
            return

        target_scene = self._pending_switch["target_scene"]
        target_portal_id = self._pending_switch["target_portal_id"]

        # 加载目标场景
        success = self.load_scene(target_scene)
        if not success:
            logger.error(f"_execute_switch, failed to load scene={target_scene}")
            self._pending_switch = None
            return

        self._pending_switch = None
        logger.info(f"_execute_switch, switched to scene={target_scene}")

    def handle_scene_change_resp(self, target_scene: str, spawn_x: int, spawn_y: int) -> Optional[Tuple[int, int]]:
        """
        处理服务器的场景切换响应

        Args:
            target_scene: 目标场景 ID
            spawn_x: 出生点 X (tile)
            spawn_y: 出生点 Y (tile)

        Returns:
            (spawn_pixel_x, spawn_pixel_y) 或 None（如果场景加载失败）
        """
        # 如果过渡动效还没开始（比如服务器先响应），直接加载场景
        if not self._transition.is_active:
            success = self.load_scene(target_scene)
            if not success:
                return None

        # 返回出生点的像素坐标
        spawn_pixel_x = float(spawn_x * TILE_SIZE)
        spawn_pixel_y = float(spawn_y * TILE_SIZE)

        logger.info(f"handle_scene_change_resp, scene={target_scene}, "
                    f"spawn=({spawn_x},{spawn_y}) pixel=({spawn_pixel_x},{spawn_pixel_y})")

        return (spawn_pixel_x, spawn_pixel_y)

    def _send_scene_change_req(self, target_scene: str, target_portal_id: str):
        """
        发送 SceneChangeReq 消息

        Args:
            target_scene: 目标场景 ID
            target_portal_id: 目标 Portal ID
        """
        try:
            import sys
            import os
            sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'common', 'proto', 'generated'))
            import player_pb2
            import base_pb2

            from ..msg_ids import MSG_ID_SCENE_CHANGE_REQ

            req = player_pb2.SceneChangeReq()
            req.target_scene = target_scene
            req.target_portal_id = target_portal_id

            payload = req.SerializeToString()

            player_msg = base_pb2.PlayerMsg()
            player_msg.msg_id = MSG_ID_SCENE_CHANGE_REQ
            player_msg.payload = payload

            self._connection.send_message(MSG_ID_SCENE_CHANGE_REQ, player_msg.SerializeToString())
            logger.info(f"_send_scene_change_req, sent: target={target_scene}, portal={target_portal_id}")

        except Exception as e:
            logger.error(f"_send_scene_change_req, failed: {e}")

    def update(self, dt: float):
        """
        更新场景管理器（每帧调用）

        Args:
            dt: 距离上一帧的时间（秒）
        """
        self._transition.update(dt)
