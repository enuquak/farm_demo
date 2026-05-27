"""
游戏场景模块
使用 pytmx + pyscroll 渲染瓦片地图，支持玩家精灵动画、摄像机跟随和 HUD
"""
import pygame
import sys
import os
import time
import math
import logging
from typing import Optional, Dict, Any

import pyscroll

from .constants import (
    TILE_SIZE, ZOOM_FACTOR, TARGET_FPS,
    PLAYER_SPEED, POSITION_UPDATE_INTERVAL, POSITION_CORRECT_DURATION,
    DEFAULT_MAP_PATH, PLAYER_SPRITE_PATH,
    Direction,
)
from .tmx_map import TmxMapLoader
from .player_sprite import PlayerSprite
from .hud import HUD
from .msg_ids import (
    MSG_ID_MAP_DATA_NOTIFY, MSG_ID_POSITION_UPDATE, MSG_ID_POSITION_CORRECT,
    MSG_ID_ITEM_USE_RESP
)
from .input_manager import InputManager, check_distance, get_interact_range
from .interaction import ITEM_EFFECTS, match_item_effect
from .ui.energy_bar import EnergyBar
from .ui.exhaustion_modal import ExhaustionModal

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'common', 'proto', 'generated'))
import player_pb2
import base_pb2

logger = logging.getLogger("client.game_scene")


class GameScene:
    """
    游戏场景
    使用 pyscroll 渲染 TMX 地图，管理玩家精灵、摄像机和 HUD
    """

    # 窗口尺寸（屏幕像素）
    WINDOW_WIDTH = 800
    WINDOW_HEIGHT = 600

    def __init__(self, connection, player_data: Dict[str, Any]):
        """
        初始化游戏场景

        Args:
            connection: GateConnection 连接对象
            player_data: 玩家数据字典（来自登录成功结果）
        """
        self._connection = connection
        self._player_data = player_data

        # PyGame 初始化
        pygame.init()
        self.screen = pygame.display.set_mode((self.WINDOW_WIDTH, self.WINDOW_HEIGHT))
        pygame.display.set_caption("Farm Demo - Game")
        self.clock = pygame.time.Clock()

        # 加载 TMX 地图
        self._tmx_map = TmxMapLoader(DEFAULT_MAP_PATH)
        logger.info(f"[GameScene]TMX map loaded: {self._tmx_map.width}x{self._tmx_map.height}")

        # 创建 pyscroll 渲染器
        map_data = pyscroll.data.TiledMapData(self._tmx_map.tmx_data)
        self._map_renderer = pyscroll.BufferedRenderer(
            map_data,
            (self.WINDOW_WIDTH, self.WINDOW_HEIGHT),
            clamp_camera=True,
            zoom=ZOOM_FACTOR,
        )
        self._map_renderer.clamp_camera = True

        # 创建 pyscroll 精灵组（管理地图 + 精灵混合渲染）
        self._group = pyscroll.PyscrollGroup(
            map_layer=self._map_renderer,
            default_layer=0,
        )

        # 玩家初始位置（tile 坐标转像素坐标）
        init_tile_x = player_data.get('pos_x', 5)
        init_tile_y = player_data.get('pos_y', 5)
        init_pixel_x = float(init_tile_x) * TILE_SIZE
        init_pixel_y = float(init_tile_y) * TILE_SIZE

        # 创建玩家精灵
        self._player_sprite = PlayerSprite(
            init_pixel_x, init_pixel_y,
            PLAYER_SPRITE_PATH,
            zoom=ZOOM_FACTOR,
        )
        self._group.add(self._player_sprite)

        # 输入管理器
        self._input_manager = InputManager()

        # HUD
        self._hud = HUD(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)

        # 能量条
        self._energy_bar = EnergyBar(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)
        self._energy_bar.set_energy(
            player_data.get('energy_current', 100),
            player_data.get('energy_max', 100)
        )

        # 精疲力尽弹窗
        self._exhaustion_modal = ExhaustionModal(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)

        # 玩家朝向
        self._facing_direction: str = Direction.DOWN

        # 位置更新计时器
        self._position_update_timer: float = 0.0
        self._last_sent_x: float = init_pixel_x
        self._last_sent_y: float = init_pixel_y

        # 位置纠正状态
        self._correcting: bool = False
        self._correct_start_x: float = 0.0
        self._correct_start_y: float = 0.0
        self._correct_target_x: float = 0.0
        self._correct_target_y: float = 0.0
        self._correct_elapsed: float = 0.0

        # 是否收到地图数据
        self._map_data_received = False

        # 上一帧时间（用于 deltaTime）
        self._last_frame_time: float = time.time()

        # 初始居中相机
        self._map_renderer.center = (int(init_pixel_x), int(init_pixel_y))

        logger.info(f"[GameScene]Initialized with player at ({init_pixel_x}, {init_pixel_y})")
        logger.info(f"[GameScene]Map size: {self._tmx_map.width}x{self._tmx_map.height}")

    def run(self):
        """运行游戏主循环"""
        running = True
        logger.info("[GameScene]Game loop started")

        while running:
            # 计算 deltaTime（秒）
            current_time = time.time()
            dt = current_time - self._last_frame_time
            self._last_frame_time = current_time

            # 限制 dt 上限，避免长时间暂停导致跳跃
            dt = min(dt, 0.1)

            # 处理事件
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                    break

                # 弹窗优先处理事件
                if self._exhaustion_modal.handle_event(event):
                    continue

                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        running = False
                        break

            if not running:
                break

            # 更新输入状态
            self._input_manager.update()

            # 处理网络消息
            self._process_network_messages()

            # 处理玩家输入（帧率无关）
            self._handle_input(dt)

            # 处理位置纠正插值
            self._update_correction(dt)

            # 更新玩家动画
            self._player_sprite.update_animation(dt)

            # 定期发送位置更新
            self._update_position_sending(dt)

            # 更新相机跟随（pyscroll center）
            player_cx = self._player_sprite.world_x + TILE_SIZE // 2
            player_cy = self._player_sprite.world_y + TILE_SIZE // 2
            self._map_renderer.center = (int(player_cx), int(player_cy))

            # 渲染
            self._render(dt)

            # 控制帧率
            self.clock.tick(TARGET_FPS)

        logger.info("[GameScene]Game loop ended")
        pygame.quit()

    # ========== 输入处理 ==========

    def _handle_input(self, dt: float):
        """
        处理玩家输入（帧率无关移动）

        Args:
            dt: 距离上一帧的时间（秒）
        """
        # 弹窗显示时屏蔽游戏输入
        if self._exhaustion_modal.is_visible:
            self._player_sprite.set_moving(False)
            return

        # 处理鼠标点击交互
        self._handle_mouse_click()

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

    def _handle_mouse_click(self):
        """
        处理鼠标点击交互
        鼠标左键点击世界中的 tile 触发物品交互
        """
        if self._input_manager.is_ui_blocking():
            return

        if not self._input_manager.is_left_mouse_just_pressed():
            return

        screen_x, screen_y = self._input_manager.get_mouse_pos()

        # 屏幕坐标转世界坐标（考虑 pyscroll zoom）
        world_x = self._map_renderer.x + screen_x / ZOOM_FACTOR
        world_y = self._map_renderer.y + screen_y / ZOOM_FACTOR
        tile_x = int(world_x) // TILE_SIZE
        tile_y = int(world_y) // TILE_SIZE

        if not (0 <= tile_x < self._tmx_map.width and 0 <= tile_y < self._tmx_map.height):
            return

        # 获取玩家当前 tile 坐标
        player_tile_x = int(self._player_sprite.world_x) // TILE_SIZE
        player_tile_y = int(self._player_sprite.world_y) // TILE_SIZE

        from .interaction import get_interaction_key
        effect_key = get_interaction_key(self._tmx_map, tile_x, tile_y)
        interact_range = get_interact_range(ITEM_EFFECTS, effect_key)

        if not check_distance(player_tile_x, player_tile_y, tile_x, tile_y, interact_range):
            logger.debug(f"[GameScene]Click out of range: player=({player_tile_x},{player_tile_y}), "
                        f"target=({tile_x},{tile_y}), range={interact_range}")
            return

        current_tool = None
        effect, description = match_item_effect(current_tool, self._tmx_map, tile_x, tile_y)

        logger.info(f"[GameScene]Mouse click interaction: tile=({tile_x},{tile_y}), "
                    f"effect={effect}, description={description}")

        dx = tile_x - player_tile_x
        dy = tile_y - player_tile_y
        self._update_facing_direction(dx, dy)
        self._player_sprite.set_direction(self._facing_direction)

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

    # ========== 位置纠正 ==========

    def _update_correction(self, dt: float):
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
            logger.debug(f"[GameScene]Position correction completed at ({x:.1f}, {y:.1f})")

    def _start_correction(self, target_x: float, target_y: float):
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
        logger.info(f"[GameScene]Position correction started: "
                    f"({self._correct_start_x:.1f}, {self._correct_start_y:.1f}) -> ({target_x:.1f}, {target_y:.1f})")

    # ========== 位置更新发送 ==========

    def _update_position_sending(self, dt: float):
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

            logger.debug(f"[GameScene]PositionUpdate sent: ({self._player_sprite.world_x:.1f}, "
                        f"{self._player_sprite.world_y:.1f}, {self._facing_direction})")

        except Exception as e:
            logger.error(f"[GameScene]Failed to send PositionUpdate: {e}")

    # ========== 网络消息处理 ==========

    def _process_network_messages(self):
        """处理网络消息"""
        if not self._connection:
            return

        messages = self._connection.recv_all_messages()
        for msg_id, payload in messages:
            if msg_id == MSG_ID_MAP_DATA_NOTIFY:
                self._handle_map_data_notify(payload)
            elif msg_id == MSG_ID_POSITION_CORRECT:
                self._handle_position_correct(payload)
            elif msg_id == MSG_ID_ITEM_USE_RESP:
                self._handle_item_use_resp(payload)

    def _handle_map_data_notify(self, payload: bytes):
        """
        处理地图数据通知

        Args:
            payload: 消息负载
        """
        logger.info("[GameScene]MapDataNotify received (TMX map is used, ignoring server map data)")
        self._map_data_received = True

    def _handle_position_correct(self, payload: bytes):
        """
        处理位置纠正消息

        Args:
            payload: 消息负载
        """
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            pos_correct = player_pb2.PositionCorrect()
            pos_correct.ParseFromString(player_msg.payload)

            self._start_correction(pos_correct.x, pos_correct.y)

            logger.info(f"[GameScene]PositionCorrect received: ({pos_correct.x:.1f}, {pos_correct.y:.1f})")

        except Exception as e:
            logger.error(f"[GameScene]Failed to parse PositionCorrect: {e}")

    def _handle_item_use_resp(self, payload: bytes):
        """
        处理物品使用响应

        Args:
            payload: 消息负载（PlayerMsg 包装）
        """
        try:
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            item_resp = player_pb2.ItemUseResp()
            item_resp.ParseFromString(player_msg.payload)

            logger.info(f"[GameScene]ItemUseResp received: code={item_resp.code}, msg={item_resp.msg}")

            # 更新能量数据
            if item_resp.HasField("energy"):
                self._energy_bar.set_energy(item_resp.energy.current, item_resp.energy.max)
                logger.debug(f"[GameScene]Energy updated: {item_resp.energy.current}/{item_resp.energy.max}")

            # 处理精疲力尽
            if item_resp.code == 3:  # ENERGY_EXHAUSTED
                self._exhaustion_modal.show()
                logger.info("[GameScene]Energy exhausted, showing modal")

        except Exception as e:
            logger.error(f"[GameScene]Failed to parse ItemUseResp: {e}")

    # ========== 渲染 ==========

    def _render(self, dt: float):
        """
        渲染一帧

        Args:
            dt: 距离上一帧的时间（秒）
        """
        # 清屏
        self.screen.fill((0, 0, 0))

        # pyscroll 渲染地图和精灵（自动处理图层遮挡）
        self._group.draw(self.screen)

        # HUD 渲染（在游戏画面上方）
        tile_x = self._player_sprite.world_x / TILE_SIZE
        tile_y = self._player_sprite.world_y / TILE_SIZE
        self._hud.set_info(
            role_name=self._player_data.get('role_name', 'Unknown'),
            pos_x=tile_x,
            pos_y=tile_y,
            scene_id=self._player_data.get('scene_id', 'farm'),
        )
        self._hud.draw(self.screen)

        # 能量条渲染（右下角）
        self._energy_bar.draw(self.screen)

        # 精疲力尽弹窗渲染（最顶层）
        self._exhaustion_modal.draw(self.screen)

        # 刷新显示
        pygame.display.flip()
