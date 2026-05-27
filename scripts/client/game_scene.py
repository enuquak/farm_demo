"""
游戏场景模块
集成 TileMap、Camera、TileRenderer，实现游戏主循环
支持 WASD 帧率无关移动、四向朝向、水面碰撞、客户端预测 + 服务器位置纠正
"""
import pygame
import sys
import os
import time
import math
import logging
from typing import Optional, Dict, Any

from .constants import (
    TILE_SIZE, TARGET_FPS, DEFAULT_MAP_WIDTH, DEFAULT_MAP_HEIGHT,
    GROUND_PROPERTIES, OBJECT_PROPERTIES, Direction, ObjectType,
    PLAYER_SPEED, POSITION_UPDATE_INTERVAL, POSITION_CORRECT_DURATION
)
from .tile_map import TileMap, tile_to_pixel, pixel_to_tile
from .camera import Camera
from .tile_renderer import TileRenderer
from .msg_ids import MSG_ID_MAP_DATA_NOTIFY, MSG_ID_POSITION_UPDATE, MSG_ID_POSITION_CORRECT
from .input_manager import InputManager, check_distance, get_interact_range
from .interaction import ITEM_EFFECTS, match_item_effect

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'common', 'proto', 'generated'))
import player_pb2
import base_pb2

logger = logging.getLogger("client.game_scene")


class GameScene:
    """
    游戏场景
    管理 TileMap、Camera、渲染和游戏主循环
    """

    # 窗口尺寸
    WINDOW_WIDTH = 800
    WINDOW_HEIGHT = 600

    # 玩家绘制颜色
    PLAYER_COLOR = (255, 50, 50)       # 红色主体
    PLAYER_OUTLINE_COLOR = (255, 255, 255)  # 白色边框
    ARROW_COLOR = (255, 255, 0)        # 黄色朝向箭头

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

        # 创建默认 TileMap
        self._tile_map = TileMap(DEFAULT_MAP_WIDTH, DEFAULT_MAP_HEIGHT)

        # 生成默认地图（草地为主，加一些变化）
        self._generate_default_map()

        # 相机
        self._camera = Camera(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)
        self._camera.set_map_bounds(
            self._tile_map.pixel_width,
            self._tile_map.pixel_height
        )

        # 渲染器
        self._renderer = TileRenderer(self.screen)

        # 输入管理器
        self._input_manager = InputManager()

        # 玩家世界像素坐标（从登录数据初始化，tile 坐标转像素坐标）
        init_tile_x = player_data.get('pos_x', 5)
        init_tile_y = player_data.get('pos_y', 5)
        self._player_world_x: float = float(init_tile_x) * TILE_SIZE
        self._player_world_y: float = float(init_tile_y) * TILE_SIZE

        # 玩家朝向
        self._facing_direction: str = Direction.DOWN

        # 位置更新计时器
        self._position_update_timer: float = 0.0
        self._last_sent_x: float = self._player_world_x
        self._last_sent_y: float = self._player_world_y

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

        logger.info(f"[GameScene]Initialized with player at ({self._player_world_x}, {self._player_world_y})")
        logger.info(f"[GameScene]Map size: {DEFAULT_MAP_WIDTH}x{DEFAULT_MAP_HEIGHT}")

    def _generate_default_map(self):
        """生成默认地图（含多种地形和地物）"""
        import random
        random.seed(42)  # 固定种子，保证一致性

        for y in range(self._tile_map.height):
            for x in range(self._tile_map.width):
                r = random.random()
                if r < 0.70:
                    ground_id = 0  # GRASS
                elif r < 0.80:
                    ground_id = 1  # DIRT
                elif r < 0.88:
                    ground_id = 3  # SAND
                elif r < 0.93:
                    ground_id = 4  # TILLED
                else:
                    ground_id = 2  # WATER
                self._tile_map.ground[y][x] = ground_id

        # 添加示例地物
        # 石头（散布在草地上）
        stone_positions = [
            (5, 5), (10, 8), (15, 12), (20, 3), (25, 15),
            (30, 10), (35, 20), (40, 5), (45, 18), (50, 8),
        ]
        for sx, sy in stone_positions:
            if 0 <= sx < self._tile_map.width and 0 <= sy < self._tile_map.height:
                self._tile_map.objects[sy][sx] = ObjectType.STONE

        # 树木（分布在边缘）
        tree_positions = [
            (2, 2), (3, 2), (4, 2), (2, 3), (3, 3), (4, 3),
            (self._tile_map.width - 3, 2), (self._tile_map.width - 2, 2),
            (self._tile_map.width - 3, 3), (self._tile_map.width - 2, 3),
        ]
        for tx, ty in tree_positions:
            if 0 <= tx < self._tile_map.width and 0 <= ty < self._tile_map.height:
                self._tile_map.objects[ty][tx] = ObjectType.TREE

        # 门（场景切换点）
        door_positions = [(8, 8), (52, 42)]
        for dx, dy in door_positions:
            if 0 <= dx < self._tile_map.width and 0 <= dy < self._tile_map.height:
                self._tile_map.objects[dy][dx] = ObjectType.DOOR_IN

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

            # 定期发送位置更新
            self._update_position_sending(dt)

            # 更新相机
            self._camera.follow(self._player_world_x, self._player_world_y)

            # 渲染
            self._render()

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
            return

        # 对角线归一化（确保对角速度与单方向一致）
        length = math.sqrt(input_dx * input_dx + input_dy * input_dy)
        if length > 0:
            input_dx /= length
            input_dy /= length

        # 计算位移（像素/秒 * 秒 = 像素）
        move_distance = PLAYER_SPEED * dt
        dx = input_dx * move_distance
        dy = input_dy * move_distance

        # 更新朝向（优先级：水平方向优先，取最后按下的方向）
        self._update_facing_direction(input_dx, input_dy)

        # 计算新位置
        new_x = self._player_world_x + dx
        new_y = self._player_world_y + dy

        # 地图边界碰撞检测
        new_x, new_y = self._clamp_to_map_bounds(new_x, new_y)

        # 水面 tile 碰撞检测
        new_x, new_y = self._check_walkable(new_x, new_y)

        # 应用移动
        self._player_world_x = new_x
        self._player_world_y = new_y

    def _update_facing_direction(self, dx: float, dy: float):
        """
        根据移动方向更新朝向

        Args:
            dx: 归一化后的 X 方向（-1, 0, 1）
            dy: 归一化后的 Y 方向（-1, 0, 1）
        """
        # 优先水平方向，其次垂直方向
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
        # 检查 UI 层是否屏蔽
        if self._input_manager.is_ui_blocking():
            return

        # 检查鼠标左键是否在本帧按下
        if not self._input_manager.is_left_mouse_just_pressed():
            return

        # 获取鼠标屏幕坐标
        screen_x, screen_y = self._input_manager.get_mouse_pos()

        # 转换为 tile 坐标
        tile_x, tile_y = self._camera.screen_to_tile(screen_x, screen_y)

        # 检查 tile 坐标是否在地图范围内
        if not (0 <= tile_x < self._tile_map.width and 0 <= tile_y < self._tile_map.height):
            return

        # 获取玩家当前 tile 坐标
        player_tile_x, player_tile_y = pixel_to_tile(
            int(self._player_world_x), int(self._player_world_y)
        )

        # 获取交互键
        from .interaction import get_interaction_key
        effect_key = get_interaction_key(self._tile_map, tile_x, tile_y)

        # 获取交互范围
        interact_range = get_interact_range(ITEM_EFFECTS, effect_key)

        # 距离校验（切比雪夫距离）
        if not check_distance(player_tile_x, player_tile_y, tile_x, tile_y, interact_range):
            logger.debug(f"[GameScene]Click out of range: player=({player_tile_x},{player_tile_y}), "
                        f"target=({tile_x},{tile_y}), range={interact_range}")
            return

        # 获取当前手持工具（暂为空手）
        current_tool = None

        # 匹配物品效果
        effect, description = match_item_effect(current_tool, self._tile_map, tile_x, tile_y)

        logger.info(f"[GameScene]Mouse click interaction: tile=({tile_x},{tile_y}), "
                    f"effect={effect}, description={description}")

        # TODO: 发送 ItemUseReq 到服务器
        # 暂时只记录日志，后续实现网络发送

        # 更新玩家朝向（基于点击方向）
        dx = tile_x - player_tile_x
        dy = tile_y - player_tile_y
        self._update_facing_direction(dx, dy)

    def _clamp_to_map_bounds(self, x: float, y: float) -> tuple:
        """
        将坐标限制在地图边界内

        Args:
            x: 目标 X 坐标
            y: 目标 Y 坐标

        Returns:
            (clamped_x, clamped_y) 限制后的坐标
        """
        # 左边界
        x = max(0.0, x)
        # 上边界
        y = max(0.0, y)
        # 右边界（playerWidth = TILE_SIZE）
        x = min(x, self._tile_map.pixel_width - TILE_SIZE)
        # 下边界
        y = min(y, self._tile_map.pixel_height - TILE_SIZE)
        return (x, y)

    def _check_walkable(self, new_x: float, new_y: float) -> tuple:
        """
        检测目标位置是否可通行（水面 tile 不可通行，地物层碰撞检测）
        使用玩家的四角进行检测

        Args:
            new_x: 目标 X 坐标
            new_y: 目标 Y 坐标

        Returns:
            (safe_x, safe_y) 安全的坐标（如果目标不可通行，返回原位置）
        """
        # 检测玩家四角所在的 tile
        corners = [
            (new_x, new_y),                              # 左上
            (new_x + TILE_SIZE - 1, new_y),              # 右上
            (new_x, new_y + TILE_SIZE - 1),              # 左下
            (new_x + TILE_SIZE - 1, new_y + TILE_SIZE - 1),  # 右下
        ]

        for corner_x, corner_y in corners:
            tile_x, tile_y = pixel_to_tile(int(corner_x), int(corner_y))

            # 检查地面层
            ground = self._tile_map.get_ground(tile_x, tile_y)
            if ground is not None:
                props = GROUND_PROPERTIES.get(ground)
                if props and not props.get("walkable", True):
                    # 目标位置有不可通行地面，返回原位置
                    return (self._player_world_x, self._player_world_y)

            # 检查地物层
            obj = self._tile_map.get_object(tile_x, tile_y)
            if obj is not None and obj != ObjectType.NONE:
                obj_props = OBJECT_PROPERTIES.get(obj)
                if obj_props and not obj_props.get("walkable", True):
                    # 目标位置有不可通行地物，返回原位置
                    return (self._player_world_x, self._player_world_y)

        return (new_x, new_y)

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
        self._player_world_x = self._correct_start_x + (self._correct_target_x - self._correct_start_x) * t
        self._player_world_y = self._correct_start_y + (self._correct_target_y - self._correct_start_y) * t

        if t >= 1.0:
            self._correcting = False
            logger.debug(f"[GameScene]Position correction completed at ({self._player_world_x:.1f}, {self._player_world_y:.1f})")

    def _start_correction(self, target_x: float, target_y: float):
        """
        开始位置纠正插值

        Args:
            target_x: 目标 X 坐标
            target_y: 目标 Y 坐标
        """
        self._correcting = True
        self._correct_start_x = self._player_world_x
        self._correct_start_y = self._player_world_y
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

        # 检查是否移动了（静止时不发包）
        moved = (
            abs(self._player_world_x - self._last_sent_x) > 0.5 or
            abs(self._player_world_y - self._last_sent_y) > 0.5
        )

        if not moved:
            return

        # 构建并发送 PositionUpdate
        self._send_position_update()

    def _send_position_update(self):
        """发送 PositionUpdate 消息到服务器"""
        try:
            pos_update = player_pb2.PositionUpdate()
            pos_update.x = self._player_world_x
            pos_update.y = self._player_world_y
            pos_update.direction = self._facing_direction
            pos_update.timestamp = int(time.time() * 1000)  # 毫秒时间戳

            payload = pos_update.SerializeToString()

            # 通过 PlayerMsg 封装发送
            player_msg = base_pb2.PlayerMsg()
            player_msg.player_id = self._player_data.get('player_id', 0)
            player_msg.server_id = self._player_data.get('server_id', 1)
            player_msg.msg_id = MSG_ID_POSITION_UPDATE
            player_msg.payload = payload
            msg_payload = player_msg.SerializeToString()

            self._connection.send_message(MSG_ID_POSITION_UPDATE, msg_payload)

            # 记录已发送位置
            self._last_sent_x = self._player_world_x
            self._last_sent_y = self._player_world_y

            logger.debug(f"[GameScene]PositionUpdate sent: ({self._player_world_x:.1f}, {self._player_world_y:.1f}, {self._facing_direction})")

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

    def _handle_map_data_notify(self, payload: bytes):
        """
        处理地图数据通知（支持双层数据）

        Args:
            payload: 消息负载（简单二进制格式）
            格式: width(4B) + height(4B) + ground_data(width*height bytes) + objects_data(width*height bytes)
        """
        try:
            import struct
            # 解析: width(4B) + height(4B) + ground_data(width*height bytes) + objects_data(width*height bytes)
            if len(payload) < 8:
                logger.warning("[GameScene]MapDataNotify payload too short")
                return

            width, height = struct.unpack('!II', payload[:8])
            data_bytes = payload[8:]

            expected_ground_size = width * height
            expected_objects_size = width * height
            total_expected = expected_ground_size + expected_objects_size

            if len(data_bytes) < expected_ground_size:
                logger.warning(f"[GameScene]MapDataNotify ground data incomplete: "
                             f"expected {expected_ground_size}, got {len(data_bytes)}")
                return

            # 如果地图尺寸变化，重新创建 TileMap
            if width != self._tile_map.width or height != self._tile_map.height:
                self._tile_map = TileMap(width, height)
                self._camera.set_map_bounds(
                    self._tile_map.pixel_width,
                    self._tile_map.pixel_height
                )

            # 填充地面层数据
            ground_bytes = data_bytes[:expected_ground_size]
            for y in range(height):
                for x in range(width):
                    self._tile_map.ground[y][x] = ground_bytes[y * width + x]

            # 填充地物层数据（如果存在）
            if len(data_bytes) >= total_expected:
                objects_bytes = data_bytes[expected_ground_size:total_expected]
                for y in range(height):
                    for x in range(width):
                        self._tile_map.objects[y][x] = objects_bytes[y * width + x]
                logger.info(f"[GameScene]Map data received (dual-layer): {width}x{height}")
            else:
                # 兼容旧格式（只有地面层）
                logger.info(f"[GameScene]Map data received (single-layer): {width}x{height}")

            self._map_data_received = True

        except Exception as e:
            logger.error(f"[GameScene]Failed to parse MapDataNotify: {e}")

    def _handle_position_correct(self, payload: bytes):
        """
        处理位置纠正消息

        Args:
            payload: 消息负载（需要先解包 PlayerMsg）
        """
        try:
            # 解包 PlayerMsg
            player_msg = base_pb2.PlayerMsg()
            player_msg.ParseFromString(payload)

            # 解包 PositionCorrect
            pos_correct = player_pb2.PositionCorrect()
            pos_correct.ParseFromString(player_msg.payload)

            # 开始平滑纠正
            self._start_correction(pos_correct.x, pos_correct.y)

            logger.info(f"[GameScene]PositionCorrect received: ({pos_correct.x:.1f}, {pos_correct.y:.1f})")

        except Exception as e:
            logger.error(f"[GameScene]Failed to parse PositionCorrect: {e}")

    # ========== 渲染 ==========

    def _render(self):
        """渲染一帧"""
        # 清屏
        self.screen.fill((0, 0, 0))

        # 渲染 TileMap
        self._renderer.render(self._tile_map, self._camera)

        # 渲染玩家
        self._render_player()

        # 刷新显示
        pygame.display.flip()

    def _render_player(self):
        """
        渲染玩家
        绘制 TILE_SIZE x TILE_SIZE 纯色方块 + 朝向三角箭头
        """
        # 转换到屏幕坐标
        screen_x, screen_y = self._camera.world_to_screen(
            self._player_world_x, self._player_world_y
        )
        sx = int(screen_x)
        sy = int(screen_y)

        # 绘制主体方块（TILE_SIZE x TILE_SIZE）
        player_rect = pygame.Rect(sx, sy, TILE_SIZE, TILE_SIZE)
        pygame.draw.rect(self.screen, self.PLAYER_COLOR, player_rect)
        pygame.draw.rect(self.screen, self.PLAYER_OUTLINE_COLOR, player_rect, 2)

        # 绘制朝向三角箭头
        self._render_direction_arrow(sx, sy)

    def _render_direction_arrow(self, sx: int, sy: int):
        """
        在角色方块上绘制朝向三角箭头

        Args:
            sx: 屏幕 X 坐标（角色左上角）
            sy: 屏幕 Y 坐标（角色左上角）
        """
        cx = sx + TILE_SIZE // 2  # 中心 X
        cy = sy + TILE_SIZE // 2  # 中心 Y
        half = TILE_SIZE // 2
        arrow_size = 6  # 箭头大小

        if self._facing_direction == Direction.UP:
            # 向上箭头：三角形顶点在上方
            points = [
                (cx, sy + 2),                        # 顶点
                (cx - arrow_size, sy + 2 + arrow_size),  # 左下
                (cx + arrow_size, sy + 2 + arrow_size),  # 右下
            ]
        elif self._facing_direction == Direction.DOWN:
            # 向下箭头：三角形顶点在下方
            points = [
                (cx, sy + TILE_SIZE - 2),                     # 顶点
                (cx - arrow_size, sy + TILE_SIZE - 2 - arrow_size),  # 左上
                (cx + arrow_size, sy + TILE_SIZE - 2 - arrow_size),  # 右上
            ]
        elif self._facing_direction == Direction.LEFT:
            # 向左箭头：三角形顶点在左侧
            points = [
                (sx + 2, cy),                        # 顶点
                (sx + 2 + arrow_size, cy - arrow_size),  # 右上
                (sx + 2 + arrow_size, cy + arrow_size),  # 右下
            ]
        elif self._facing_direction == Direction.RIGHT:
            # 向右箭头：三角形顶点在右侧
            points = [
                (sx + TILE_SIZE - 2, cy),                     # 顶点
                (sx + TILE_SIZE - 2 - arrow_size, cy - arrow_size),  # 左上
                (sx + TILE_SIZE - 2 - arrow_size, cy + arrow_size),  # 左下
            ]
        else:
            return

        pygame.draw.polygon(self.screen, self.ARROW_COLOR, points)
