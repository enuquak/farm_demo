"""
玩家控制器模块
负责玩家移动、位置纠正插值和定期位置同步。
从 GameScene 中提取的玩家相关逻辑。
"""
import math
import time
import logging
from typing import Optional, Dict, Any

import pygame

from .constants import (
    TILE_SIZE, ZOOM_FACTOR,
    PLAYER_SPEED, POSITION_UPDATE_INTERVAL, POSITION_CORRECT_DURATION,
    Direction,
)
from .tmx_map import TmxMapLoader
from .player_sprite import PlayerSprite
from .input_manager import InputManager, check_distance, get_interact_range
from .interaction import ITEM_EFFECTS, match_item_effect
from .scene import SceneManager

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'common', 'proto', 'generated'))
import player_pb2
import base_pb2

from .message_ids import MSG_ID_POSITION_UPDATE

logger = logging.getLogger("client.player_controller")


class PlayerController:
    """
    玩家控制器
    管理玩家移动输入、碰撞检测、位置纠正插值和定期位置同步。
    """

    def __init__(
        self,
        connection,
        tmx_map: TmxMapLoader,
        player_sprite: PlayerSprite,
        scene_manager: SceneManager,
        player_data: Dict[str, Any],
        input_manager: InputManager,
    ):
        """
        初始化玩家控制器

        Args:
            connection: GateConnection 连接对象
            tmx_map: TMX 地图加载器
            player_sprite: 玩家精灵
            scene_manager: 场景管理器
            player_data: 玩家数据字典
            input_manager: 输入管理器
        """
        self._connection = connection
        self._tmx_map = tmx_map
        self._player_sprite = player_sprite
        self._scene_manager = scene_manager
        self._player_data = player_data
        self._input_manager = input_manager

        # 玩家朝向
        self._facing_direction: str = Direction.DOWN

        # 位置更新计时器
        self._position_update_timer: float = 0.0
        self._last_sent_x: float = player_sprite.world_x
        self._last_sent_y: float = player_sprite.world_y

        # 位置纠正状态
        self._correcting: bool = False
        self._correct_start_x: float = 0.0
        self._correct_start_y: float = 0.0
        self._correct_target_x: float = 0.0
        self._correct_target_y: float = 0.0
        self._correct_elapsed: float = 0.0

    @property
    def facing_direction(self) -> str:
        """当前朝向"""
        return self._facing_direction

    def handle_input(
        self,
        dt: float,
        exhaustion_visible: bool,
        map_renderer,
    ):
        """
        处理玩家输入（帧率无关移动）

        Args:
            dt: 距离上一帧的时间（秒）
            exhaustion_visible: 精疲力尽弹窗是否可见
            map_renderer: pyscroll 渲染器（用于鼠标坐标转换）
        """
        # 弹窗显示时或场景过渡时屏蔽游戏输入
        if exhaustion_visible or self._scene_manager.is_input_blocked():
            self._player_sprite.set_moving(False)
            return

        # 使用 InputManager 的 Action Map 处理移动
        input_dx = 0.0
        input_dy = 0.0

        if self._input_manager.is_action_pressed("move_left"):
            input_dx -= 1.0
        if self._input_manager.is_action_pressed("move_right"):
            input_dx += 1.0
        if self._input_manager.is_action_pressed("move_up"):
            input_dy -= 1.0
        if self._input_manager.is_action_pressed("move_down"):
            input_dy += 1.0

        # 无输入时静止
        if input_dx == 0.0 and input_dy == 0.0:
            self._player_sprite.set_moving(False)
            return

        # 标记为移动中
        self._player_sprite.set_moving(True)

        # 对角线归一化（确保对角速度与单方向一致）
        length = math.sqrt(input_dx * input_dx + input_dy * input_dy)
        if length > 0:
            input_dx /= length
            input_dy /= length

        # 计算位移（像素/秒 * 秒 = 像素）
        move_distance = PLAYER_SPEED * dt
        dx = input_dx * move_distance
        dy = input_dy * move_distance

        # 更新朝向
        self._update_facing_direction(input_dx, input_dy)
        self._player_sprite.set_direction(self._facing_direction)

        # 计算新位置
        new_x = self._player_sprite.world_x + dx
        new_y = self._player_sprite.world_y + dy

        # 地图边界碰撞检测
        new_x, new_y = self._clamp_to_map_bounds(new_x, new_y)

        # 瓦片碰撞检测（使用 TMX 地图数据）
        new_x, new_y = self._check_walkable(new_x, new_y)

        # 应用移动
        self._player_sprite.set_position(new_x, new_y)

    def _update_facing_direction(self, dx: float, dy: float):
        """
        根据移动方向更新朝向

        Args:
            dx: 归一化后的 X 方向（-1, 0, 1）
            dy: 归一化后的 Y 方向（-1, 0, 1）
        """
        if abs(dx) >= abs(dy):
            if dx > 0:
                self._facing_direction = Direction.RIGHT
            elif dx < 0:
                self._facing_direction = Direction.LEFT
        else:
            if dy > 0:
                self._facing_direction = Direction.DOWN
            elif dy < 0:
                self._facing_direction = Direction.UP

    def _clamp_to_map_bounds(self, x: float, y: float) -> tuple:
        """
        将坐标限制在地图边界内

        Args:
            x: 目标 X 坐标
            y: 目标 Y 坐标

        Returns:
            (clamped_x, clamped_y)
        """
        x = max(0.0, x)
        y = max(0.0, y)
        x = min(x, self._tmx_map.pixel_width - TILE_SIZE)
        y = min(y, self._tmx_map.pixel_height - TILE_SIZE)
        return (x, y)

    def _check_walkable(self, new_x: float, new_y: float) -> tuple:
        """
        检测目标位置是否可通行（基于 TMX 地图碰撞数据）

        Args:
            new_x: 目标 X 坐标
            new_y: 目标 Y 坐标

        Returns:
            (safe_x, safe_y)
        """
        if self._tmx_map.check_walkable_rect(new_x, new_y, TILE_SIZE, TILE_SIZE):
            return (new_x, new_y)

        # 不可通行，返回原位置
        return (self._player_sprite.world_x, self._player_sprite.world_y)

    def update_correction(self, dt: float):
        """
        更新位置纠正插值

        Args:
            dt: 距离上一帧的时间（秒）
        """
        if not self._correcting:
            return

        self._correct_elapsed += dt
        t = min(self._correct_elapsed / POSITION_CORRECT_DURATION, 1.0)

        # 线性插值（lerp）
        x = self._correct_start_x + (self._correct_target_x - self._correct_start_x) * t
        y = self._correct_start_y + (self._correct_target_y - self._correct_start_y) * t
        self._player_sprite.set_position(x, y)

        if t >= 1.0:
            self._correcting = False
            logger.debug(f"[PlayerController]Position correction completed at ({x:.1f}, {y:.1f})")

    def start_correction(self, target_x: float, target_y: float):
        """
        开始位置纠正插值

        Args:
            target_x: 目标 X 坐标
            target_y: 目标 Y 坐标
        """
        self._correcting = True
        self._correct_start_x = self._player_sprite.world_x
        self._correct_start_y = self._player_sprite.world_y
        self._correct_target_x = target_x
        self._correct_target_y = target_y
        self._correct_elapsed = 0.0
        logger.info(f"[PlayerController]Position correction started: "
                    f"({self._correct_start_x:.1f}, {self._correct_start_y:.1f}) -> ({target_x:.1f}, {target_y:.1f})")

    def update_position_sending(self, dt: float):
        """
        定期发送位置更新（每 100ms，静止时不发送）

        Args:
            dt: 距离上一帧的时间（秒）
        """
        if not self._connection or not self._connection.is_connected:
            return

        self._position_update_timer += dt

        if self._position_update_timer < POSITION_UPDATE_INTERVAL:
            return

        self._position_update_timer = 0.0

        moved = (
            abs(self._player_sprite.world_x - self._last_sent_x) > 0.5 or
            abs(self._player_sprite.world_y - self._last_sent_y) > 0.5
        )

        if not moved:
            return

        self._send_position_update()

    def _send_position_update(self):
        """发送 PositionUpdate 消息到服务器"""
        try:
            pos_update = player_pb2.PositionUpdate()
            pos_update.x = self._player_sprite.world_x
            pos_update.y = self._player_sprite.world_y
            pos_update.direction = self._facing_direction
            pos_update.timestamp = int(time.time() * 1000)

            payload = pos_update.SerializeToString()

            player_msg = base_pb2.PlayerMsg()
            player_msg.player_id = self._player_data.get('player_id', 0)
            player_msg.server_id = self._player_data.get('server_id', 1)
            player_msg.msg_id = MSG_ID_POSITION_UPDATE
            player_msg.payload = payload
            msg_payload = player_msg.SerializeToString()

            self._connection.send_message(MSG_ID_POSITION_UPDATE, msg_payload)

            self._last_sent_x = self._player_sprite.world_x
            self._last_sent_y = self._player_sprite.world_y

            logger.debug(f"[PlayerController]PositionUpdate sent: ({self._player_sprite.world_x:.1f}, "
                        f"{self._player_sprite.world_y:.1f}, {self._facing_direction})")

        except Exception as e:
            logger.error(f"[PlayerController]Failed to send PositionUpdate: {e}")
