"""
游戏场景模块
使用 pytmx + pyscroll 渲染瓦片地图，协调 PlayerController、NetworkMessageDispatcher、GameRenderer 等子模块。
"""
import pygame
import sys
import os
import time
import logging
from typing import Optional, Dict, Any

import pyscroll

from .constants import (
    TILE_SIZE, ZOOM_FACTOR, TARGET_FPS,
    DEFAULT_MAP_PATH, PLAYER_SPRITE_PATH,
    Direction, NPC_INTERACT_RANGE,
)
from .tmx_map import TmxMapLoader
from .player_sprite import PlayerSprite
from .input_manager import InputManager, check_distance, get_interact_range
from .interaction import ITEM_EFFECTS, match_item_effect
from .scene import SceneManager
from .player_controller import PlayerController
from .network_dispatcher import NetworkMessageDispatcher
from .game_renderer import GameRenderer
from .drop_item_renderer import DropItemRenderer
from .notification import Notification, NotificationType, NotificationPriority
from .ui.notification_manager import NotificationManager
from .npc_manager import NPCManager
from .dialog_engine import DialogEngine
from .affection_system import AffectionSystem
from .bubble_ui import BubbleUI
from .quest_handler import QuestHandler
from .chat import ChatManager, ChatPanel

# 战斗系统（可选模块）
try:
    from .monster_manager import MonsterManager
    from .weapon_manager import WeaponManager
    from .combat_system import CombatSystem
    from .battle_ui import BattleUI
    _COMBAT_AVAILABLE = True
except ImportError:
    _COMBAT_AVAILABLE = False

from .message_ids import MSG_ID_FORCE_SLEEP_READY

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'common', 'proto', 'generated'))
import player_pb2
import base_pb2

logger = logging.getLogger("client.game_scene")


class GameScene:
    """
    游戏场景
    作为薄协调层，组合 PlayerController、NetworkMessageDispatcher、GameRenderer 等子模块。
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

        # 场景管理器
        self._scene_manager = SceneManager(
            self.WINDOW_WIDTH, self.WINDOW_HEIGHT, connection
        )

        # 加载初始场景（默认 farm）
        initial_scene = player_data.get('scene_id', 'farm')
        if not self._scene_manager.load_scene(initial_scene):
            logger.warning(f"[GameScene]Failed to load scene {initial_scene}, falling back to TMX")
            self._scene_manager.load_scene('farm')

        # TMX 地图（用于 pyscroll 渲染）
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

        # 渲染器
        self._renderer = GameRenderer(
            self.screen, self.WINDOW_WIDTH, self.WINDOW_HEIGHT, player_data
        )

        # 掉落物渲染器
        self._drop_item_renderer = DropItemRenderer()

        # 通知管理器
        self._notification_manager = NotificationManager(
            self.WINDOW_WIDTH, self.WINDOW_HEIGHT
        )

        # NPC 系统
        self._npc_manager = NPCManager(zoom=ZOOM_FACTOR)
        self._dialog_engine = DialogEngine()
        self._affection_system = AffectionSystem()
        self._bubble_ui = BubbleUI()
        self._current_time_slot = "08:00"
        self._npc_manager.set_current_scene(self._scene_manager.current_scene)
        self._update_npc_sprites_in_group()

        # 任务系统
        self._quest_handler = QuestHandler(connection)

        # 聊天系统
        self._chat_manager = ChatManager(connection, player_data)
        self._chat_panel = ChatPanel(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)

        # 战斗系统（可选）
        if _COMBAT_AVAILABLE:
            self._weapon_manager = WeaponManager()
            self._monster_manager = MonsterManager()
            self._combat_system = CombatSystem(self._weapon_manager, self._monster_manager)
            self._battle_ui = BattleUI(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)
            self._player_hp = player_data.get('current_hp', 100)
            self._player_max_hp = player_data.get('max_hp', 100)
            self._player_invincible_until = 0.0
        else:
            self._weapon_manager = None
            self._monster_manager = None
            self._combat_system = None
            self._battle_ui = None
            self._player_hp = 100
            self._player_max_hp = 100

        # 玩家控制器
        self._player_controller = PlayerController(
            connection=connection,
            tmx_map=self._tmx_map,
            player_sprite=self._player_sprite,
            scene_manager=self._scene_manager,
            player_data=player_data,
            input_manager=self._input_manager,
        )

        # 网络消息分发器
        self._network_dispatcher = NetworkMessageDispatcher(
            connection=connection,
            on_map_data_notify=self._on_map_data_notify,
            on_position_correct=self._on_position_correct,
            on_item_use_resp=self._on_item_use_resp,
            on_scene_change_resp=self._on_scene_change_resp,
            on_clock_sync=self._on_clock_sync,
            on_force_sleep_notify=self._on_force_sleep_notify,
            on_notify_toast=self._on_notify_toast,
            on_quest_accept_resp=self._on_quest_accept_resp,
            on_quest_submit_resp=self._on_quest_submit_resp,
            on_quest_abandon_resp=self._on_quest_abandon_resp,
            on_quest_sync_notify=self._on_quest_sync_notify,
            on_quest_progress_notify=self._on_quest_progress_notify,
            # 战斗系统回调
            on_monster_spawn=lambda data: self._monster_manager.handle_monster_spawn(
                data.get('monster_id', 0), data.get('monster_type', ''),
                data.get('x', 0), data.get('y', 0),
                data.get('hp', 0), data.get('max_hp', 0)
            ) if self._monster_manager else None,
            on_monster_death=lambda data: self._monster_manager.handle_monster_death(
                data.get('monster_id', 0)
            ) if self._monster_manager else None,
            on_monster_move=lambda data: self._monster_manager.handle_monster_move(
                data.get('monster_id', 0), data.get('x', 0), data.get('y', 0),
                data.get('state', '')
            ) if self._monster_manager else None,
            on_player_hp_update=lambda data: self._handle_player_hp_update(data),
            on_player_death=lambda data: self._handle_player_death(data),
            # 聊天系统回调
            on_chat_message=self._chat_manager.on_chat_message,
            on_chat_send_resp=self._chat_manager.on_chat_send_resp,
        )

        # 强制睡觉状态
        self._force_sleep_pending: bool = False

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
                if self._renderer.exhaustion_modal.handle_event(event):
                    continue

                # 聊天面板事件（输入激活时优先处理）
                if self._chat_panel.handle_event(event, self._chat_manager):
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
            self._network_dispatcher.dispatch_pending()

            # 更新场景管理器（过渡动效）
            self._scene_manager.update(dt)

            # 处理玩家输入（帧率无关）
            self._handle_mouse_click()
            self._handle_portal_interaction()
            self._handle_npc_interaction()
            self._dialog_engine.update(dt)
            self._npc_manager.update_animations(dt)
            self._bubble_ui.update(dt)

            # 战斗系统更新
            if _COMBAT_AVAILABLE and self._monster_manager:
                self._monster_manager.update(dt, self._player_sprite.world_x, self._player_sprite.world_y)

            if _COMBAT_AVAILABLE and self._battle_ui:
                self._battle_ui.update(dt)

            # 攻击检测
            if (_COMBAT_AVAILABLE and self._combat_system
                    and self._input_manager.is_attack_pressed() and self._combat_system.can_attack()):
                player_screen_x = self.WINDOW_WIDTH // 2
                player_screen_y = self.WINDOW_HEIGHT // 2
                attack_dir = self._input_manager.get_attack_direction(player_screen_x, player_screen_y)
                hits = self._combat_system.try_attack(
                    self._player_sprite.world_x,
                    self._player_sprite.world_y,
                    attack_dir
                )
                for monster_id, damage, is_kill in hits:
                    monster = self._monster_manager.get_monster(monster_id)
                    if monster:
                        self._battle_ui.add_damage_number(monster.world_x, monster.world_y, damage)
            self._player_controller.handle_input(
                dt,
                self._renderer.exhaustion_modal.is_visible,
                self._map_renderer,
            )

            # 检查 Portal 触发
            self._check_portal_trigger()

            # 处理位置纠正插值
            self._player_controller.update_correction(dt)

            # 更新玩家动画
            self._player_sprite.update_animation(dt)

            # 定期发送位置更新
            self._player_controller.update_position_sending(dt)

            # 更新相机跟随（pyscroll center）
            player_cx = self._player_sprite.world_x + TILE_SIZE // 2
            player_cy = self._player_sprite.world_y + TILE_SIZE // 2
            self._map_renderer.center = (int(player_cx), int(player_cy))

            # 处理对话效果
            effects = self._dialog_engine.pending_effects
            for effect in effects:
                if 'affection' in effect:
                    npc_id = self._dialog_engine.current_npc_id
                    if npc_id:
                        self._affection_system.add_affection(npc_id, effect['affection'])

            if not self._dialog_engine.is_active and self._input_manager.is_ui_blocking():
                self._input_manager.set_ui_blocking(False)

            # NPC 可交互状态更新
            player_tile_x = self._player_sprite.world_x // (TILE_SIZE * ZOOM_FACTOR)
            player_tile_y = self._player_sprite.world_y // (TILE_SIZE * ZOOM_FACTOR)
            for npc in self._npc_manager.get_npcs_in_scene():
                dx = abs(npc.tile_x - player_tile_x)
                dy = abs(npc.tile_y - player_tile_y)
                is_near = max(dx, dy) <= NPC_INTERACT_RANGE
                npc.set_interactable(is_near)
                if is_near:
                    self._bubble_ui.show_dots(npc.npc_id)
                else:
                    self._bubble_ui.hide_dots(npc.npc_id)

            # 渲染
            self._renderer.render(
                dt, self._group, self._player_sprite,
                self._scene_manager, self._map_renderer,
                drop_item_renderer=self._drop_item_renderer,
                notification_manager=self._notification_manager,
                npc_manager=self._npc_manager,
                bubble_ui=self._bubble_ui,
                dialog_engine=self._dialog_engine,
                affection_system=self._affection_system,
                monster_manager=self._monster_manager if _COMBAT_AVAILABLE else None,
                battle_ui=self._battle_ui if _COMBAT_AVAILABLE else None,
                player_hp=self._player_hp,
                player_max_hp=self._player_max_hp,
                chat_panel=self._chat_panel,
                chat_manager=self._chat_manager,
            )

            # 控制帧率
            self.clock.tick(TARGET_FPS)

        logger.info("[GameScene]Game loop ended")
        pygame.quit()

    # ========== 鼠标交互（保留在 GameScene 中，因为涉及多模块协调） ==========

    def _handle_mouse_click(self):
        """
        处理鼠标点击交互
        鼠标左键点击世界中的 tile 触发物品交互
        """
        if self._dialog_engine.is_active:
            return
        if self._chat_panel.is_input_active:
            return
        if self._input_manager.is_ui_blocking():
            return

        # 过渡期间屏蔽鼠标交互
        if self._scene_manager.is_input_blocked():
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

        # 检查是否点击了 Portal（门）
        from .interaction import is_portal, get_portal_scene
        if is_portal(self._tmx_map, tile_x, tile_y):
            target_scene = get_portal_scene(self._tmx_map, tile_x, tile_y)
            if target_scene:
                # 计算玩家屏幕位置（用于 Iris 动效中心点）
                player_cx = int(self._player_sprite.world_x + TILE_SIZE // 2)
                player_cy = int(self._player_sprite.world_y + TILE_SIZE // 2)
                scr_x = int((player_cx - self._map_renderer.x) * ZOOM_FACTOR)
                scr_y = int((player_cy - self._map_renderer.y) * ZOOM_FACTOR)

                logger.info(f"[GameScene]Portal clicked: tile=({tile_x},{tile_y}), target={target_scene}")

                # 确定目标 portal ID
                from .constants import ObjectType
                obj_type = self._tmx_map.get_object_type(tile_x, tile_y)
                target_portal_id = "door_out" if obj_type == ObjectType.DOOR_IN else "door_in"

                self._scene_manager.request_scene_change(
                    target_scene, target_portal_id, (scr_x, scr_y)
                )
                return

        # 检查是否点击了 NPC
        npc = self._npc_manager.get_npc_at_tile(tile_x, tile_y)
        if npc is not None:
            self._npc_manager.face_player(
                self._player_sprite.world_x // (TILE_SIZE * ZOOM_FACTOR),
                self._player_sprite.world_y // (TILE_SIZE * ZOOM_FACTOR)
            )
            started = self._dialog_engine.start_dialog(
                npc_id=npc.npc_id,
                affection=self._affection_system.get_affection(npc.npc_id),
            )
            if started:
                self._input_manager.set_ui_blocking(True)
            return

        current_tool = None
        effect, description = match_item_effect(current_tool, self._tmx_map, tile_x, tile_y)

        logger.info(f"[GameScene]Mouse click interaction: tile=({tile_x},{tile_y}), "
                    f"effect={effect}, description={description}")

        dx = tile_x - player_tile_x
        dy = tile_y - player_tile_y
        self._player_controller._update_facing_direction(dx, dy)
        self._player_sprite.set_direction(self._player_controller.facing_direction)

    def _handle_portal_interaction(self):
        """
        处理空格键 Portal 交互
        玩家站在 Portal 旁（切比雪夫距离=1），面前 tile 是 Portal，按空格触发场景切换
        """
        if self._dialog_engine.is_active:
            return
        if self._chat_panel.is_input_active:
            return
        if not self._input_manager.is_action_pressed("interact"):
            return

        # 过渡期间不处理
        if self._scene_manager.is_input_blocked():
            return

        # 计算玩家当前 tile
        player_tile_x = int(self._player_sprite.world_x) // TILE_SIZE
        player_tile_y = int(self._player_sprite.world_y) // TILE_SIZE

        # 根据朝向计算面前的 tile
        facing = self._player_controller.facing_direction
        face_x, face_y = player_tile_x, player_tile_y
        if facing == Direction.UP:
            face_y -= 1
        elif facing == Direction.DOWN:
            face_y += 1
        elif facing == Direction.LEFT:
            face_x -= 1
        elif facing == Direction.RIGHT:
            face_x += 1

        # 检查面前的 tile 是否是 Portal
        from .interaction import is_portal, get_portal_scene
        if not is_portal(self._tmx_map, face_x, face_y):
            return

        target_scene = get_portal_scene(self._tmx_map, face_x, face_y)
        if not target_scene:
            return

        # 计算玩家屏幕位置（用于 Iris 动效中心点）
        player_cx = int(self._player_sprite.world_x + TILE_SIZE // 2)
        player_cy = int(self._player_sprite.world_y + TILE_SIZE // 2)
        screen_x = int((player_cx - self._map_renderer.x) * ZOOM_FACTOR)
        screen_y = int((player_cy - self._map_renderer.y) * ZOOM_FACTOR)

        # 确定目标 portal ID
        from .constants import ObjectType
        obj_type = self._tmx_map.get_object_type(face_x, face_y)
        target_portal_id = "door_out" if obj_type == ObjectType.DOOR_IN else "door_in"

        logger.info(f"[GameScene]Portal interact via SPACE: face_tile=({face_x},{face_y}), "
                    f"target={target_scene}, portal={target_portal_id}")

        self._scene_manager.request_scene_change(
            target_scene, target_portal_id, (screen_x, screen_y)
        )

    def _check_portal_trigger(self):
        """检查玩家当前位置是否触发 Portal"""
        # 过渡中不检查
        if self._scene_manager.is_transitioning:
            return

        # 计算 tile 坐标
        player_x = self._player_sprite.world_x
        player_y = self._player_sprite.world_y
        tile_x = int(player_x) // TILE_SIZE
        tile_y = int(player_y) // TILE_SIZE

        # 检查当前位置是否有 Portal
        portal = self._scene_manager.check_portal_trigger(
            tile_x, tile_y, self._player_controller.facing_direction
        )

        if portal is None:
            return

        target_scene = portal.get("target")
        target_portal_id = portal.get("target_portal")

        if not target_scene:
            return

        # 计算玩家屏幕位置（用于 Iris 动效中心点）
        player_cx = int(self._player_sprite.world_x + TILE_SIZE // 2)
        player_cy = int(self._player_sprite.world_y + TILE_SIZE // 2)
        screen_x = int((player_cx - self._map_renderer.x) * ZOOM_FACTOR)
        screen_y = int((player_cy - self._map_renderer.y) * ZOOM_FACTOR)

        logger.info(f"[GameScene]Portal triggered: tile=({tile_x},{tile_y}), "
                    f"target={target_scene}, portal={target_portal_id}")

        # 请求场景切换
        self._scene_manager.request_scene_change(
            target_scene, target_portal_id, (screen_x, screen_y)
        )

    # ========== NPC 交互 ==========

    def _update_npc_sprites_in_group(self):
        """将当前场景的 NPC 精灵加入 pyscroll group。"""
        for npc in list(self._group):
            if hasattr(npc, 'npc_id'):
                self._group.remove(npc)
        for npc in self._npc_manager.get_npcs_in_scene():
            self._group.add(npc)

    def _on_affection_sync(self, payload):
        """处理好感度同步消息。"""
        pass  # Will be connected to network dispatcher later

    def _handle_npc_interaction(self):
        """处理 NPC 交互（空格键触发）。"""
        if self._input_manager.is_ui_blocking():
            return
        if self._scene_manager.is_input_blocked():
            return
        if not self._input_manager.is_action_pressed('interact'):
            return

        player_tile_x = self._player_sprite.world_x // (TILE_SIZE * ZOOM_FACTOR)
        player_tile_y = self._player_sprite.world_y // (TILE_SIZE * ZOOM_FACTOR)
        facing = self._player_sprite.direction

        if self._dialog_engine.is_active:
            self._dialog_engine.advance()
            self._input_manager.set_ui_blocking(self._dialog_engine.is_active)
            return

        npc = self._npc_manager.get_interactable_npc(player_tile_x, player_tile_y, facing)
        if npc is None:
            npc = self._npc_manager.get_nearby_npc(player_tile_x, player_tile_y)

        if npc is not None:
            self._npc_manager.face_player(player_tile_x, player_tile_y)

            active_item = self._inventory.get_active_item() if hasattr(self, '_inventory') else None
            if active_item:
                item_id = str(active_item.get('item_id', ''))
                reaction = self._dialog_engine.get_gift_reaction(npc.npc_id, item_id)
                if self._affection_system.can_gift(npc.npc_id):
                    self._affection_system.record_gift(npc.npc_id)
                    self._affection_system.add_affection(npc.npc_id, reaction['affection'])
                    self._dialog_engine.start_gift_dialog(npc.npc_id, reaction)
                    self._input_manager.set_ui_blocking(True)
                    return

            started = self._dialog_engine.start_dialog(
                npc_id=npc.npc_id,
                affection=self._affection_system.get_affection(npc.npc_id),
                time_slot=getattr(self, '_current_time_slot', '08:00'),
            )
            if started:
                self._input_manager.set_ui_blocking(True)

    # ========== 网络回调 ==========

    def _on_map_data_notify(self):
        """地图数据通知回调"""
        self._map_data_received = True

    def _on_position_correct(self, target_x: float, target_y: float):
        """位置纠正回调"""
        self._player_controller.start_correction(target_x, target_y)

    def _on_item_use_resp(self, item_resp):
        """物品使用响应回调"""
        # 更新能量数据
        if item_resp.HasField("energy"):
            self._renderer.energy_bar.set_energy(item_resp.energy.current, item_resp.energy.max)
            logger.debug(f"[GameScene]Energy updated: {item_resp.energy.current}/{item_resp.energy.max}")

        # 处理精疲力尽
        if item_resp.code == 3:  # ENERGY_EXHAUSTED
            self._renderer.exhaustion_modal.show()
            logger.info("[GameScene]Energy exhausted, showing modal")

    def _on_scene_change_resp(self, resp):
        """场景切换响应回调"""
        if resp.code != 0:
            logger.error(f"[GameScene]Scene change failed: {resp.msg}")
            return

        # 处理场景切换
        spawn_pos = self._scene_manager.handle_scene_change_resp(
            resp.target_scene, resp.spawn_x, resp.spawn_y
        )

        if spawn_pos:
            # 更新玩家位置
            spawn_x, spawn_y = spawn_pos
            self._player_sprite.set_position(spawn_x, spawn_y)

            # 更新玩家数据中的场景 ID
            self._player_data['scene_id'] = resp.target_scene

            logger.info(f"[GameScene]Scene changed to {resp.target_scene}, "
                       f"player at ({spawn_x}, {spawn_y})")

    def _on_clock_sync(self, day: int, time_slot: int):
        """时钟同步回调"""
        self._renderer.time_hud.update(day, time_slot)

    def _on_force_sleep_notify(self, day: int):
        """强制睡觉通知回调"""
        # 标记强制睡觉待处理
        self._force_sleep_pending = True

        # 计算玩家屏幕位置（用于 Iris 动效中心点）
        player_cx = int(self._player_sprite.world_x + TILE_SIZE // 2)
        player_cy = int(self._player_sprite.world_y + TILE_SIZE // 2)
        screen_x = int((player_cx - self._map_renderer.x) * ZOOM_FACTOR)
        screen_y = int((player_cy - self._map_renderer.y) * ZOOM_FACTOR)

        # 启动 Iris 收缩过渡，完成后发送 ForceSleepReady
        self._scene_manager._transition.start(
            (screen_x, screen_y),
            self._on_force_sleep_iris_complete
        )

    def _on_force_sleep_iris_complete(self):
        """强制睡觉 Iris 收缩完成回调"""
        logger.info("[GameScene]Force sleep Iris complete, sending ForceSleepReady")

        # 发送 ForceSleepReady 给服务器
        if self._connection and self._connection.is_connected:
            try:
                ready = player_pb2.ForceSleepReady()
                ready.day = self._renderer.time_hud._day

                payload = ready.SerializeToString()

                player_msg = base_pb2.PlayerMsg()
                player_msg.player_id = self._player_data.get('player_id', 0)
                player_msg.server_id = self._player_data.get('server_id', 1)
                player_msg.msg_id = MSG_ID_FORCE_SLEEP_READY
                player_msg.payload = payload
                msg_payload = player_msg.SerializeToString()

                self._connection.send_message(MSG_ID_FORCE_SLEEP_READY, msg_payload)
                logger.info("[GameScene]ForceSleepReady sent")

            except Exception as e:
                logger.error(f"[GameScene]Failed to send ForceSleepReady: {e}")

        # 不在这里切换场景，等待服务器的 SceneChangeResp
        # Iris 展开会在 handle_scene_change_resp 中通过场景管理器处理

    def _on_notify_toast(self, notify):
        """通知提示回调"""
        notification = Notification(
            type=NotificationType(notify.type),
            priority=NotificationPriority(notify.priority),
            title=notify.title,
            content=notify.content,
            duration=notify.duration_ms / 1000.0,
            channel="marquee" if notify.is_marquee else "toast",
        )
        self._notification_manager.add(notification)
        logger.info(f"[GameScene]Notification received: {notify.title}")

    # ========== 任务系统回调 ==========

    def _on_quest_accept_resp(self, resp):
        """任务接受响应回调"""
        self._quest_handler.handle_quest_accept_resp(resp)

    def _on_quest_submit_resp(self, resp):
        """任务提交响应回调"""
        self._quest_handler.handle_quest_submit_resp(resp)

    def _on_quest_abandon_resp(self, resp):
        """任务放弃响应回调"""
        self._quest_handler.handle_quest_abandon_resp(resp)

    def _on_quest_sync_notify(self, notify):
        """任务同步通知回调"""
        self._quest_handler.handle_quest_sync_notify(notify)

    def _on_quest_progress_notify(self, notify):
        """任务进度通知回调"""
        self._quest_handler.handle_quest_progress_notify(notify)

    # ========== 战斗系统回调 ==========

    def _handle_player_hp_update(self, data: dict):
        """处理玩家HP更新"""
        self._player_hp = data.get('current_hp', self._player_hp)
        self._player_max_hp = data.get('max_hp', self._player_max_hp)
        damage = data.get('damage', 0)
        if damage > 0 and _COMBAT_AVAILABLE and self._battle_ui:
            self._battle_ui.add_damage_number(
                self._player_sprite.world_x, self._player_sprite.world_y, damage
            )

    def _handle_player_death(self, data: dict):
        """处理玩家死亡"""
        logger.info("[GameScene]Player died!")
        # TODO: 显示死亡UI，传送回农场
