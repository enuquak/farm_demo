"""
游戏渲染器模块
负责渲染游戏画面的各个层：地图、HUD、能量条、时间 HUD、弹窗和场景过渡。
从 GameScene 中提取的渲染逻辑。
"""
import logging
from typing import Optional, Dict, Any

import pygame

from .constants import TILE_SIZE, ZOOM_FACTOR
from .player_sprite import PlayerSprite
from .hud import HUD
from .ui.energy_bar import EnergyBar
from .ui.exhaustion_modal import ExhaustionModal
from .ui.time_hud import TimeHUD
from .dialog_ui import DialogUI

logger = logging.getLogger("client.game_renderer")


class GameRenderer:
    """
    游戏渲染器
    负责将游戏状态渲染到屏幕上，包括 pyscroll 地图、HUD、UI 组件和场景过渡。
    """

    def __init__(self, screen: pygame.Surface, screen_width: int, screen_height: int,
                 player_data: Dict[str, Any]):
        """
        初始化渲染器

        Args:
            screen: PyGame 屏幕 Surface
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
            player_data: 玩家数据字典
        """
        self._screen = screen
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._player_data = player_data

        # HUD
        self._hud = HUD(screen_width, screen_height)

        # 能量条
        self._energy_bar = EnergyBar(screen_width, screen_height)
        self._energy_bar.set_energy(
            player_data.get('energy_current', 100),
            player_data.get('energy_max', 100)
        )

        # 精疲力尽弹窗
        self._exhaustion_modal = ExhaustionModal(screen_width, screen_height)

        # 时间 HUD
        self._time_hud = TimeHUD()
        self._time_hud.update(
            player_data.get('day', 1),
            player_data.get('time_slot', 0)
        )

        # 对话 UI
        self._dialog_ui = DialogUI(screen_width, screen_height)

    @property
    def exhaustion_modal(self) -> ExhaustionModal:
        """获取精疲力尽弹窗对象（用于事件处理）"""
        return self._exhaustion_modal

    @property
    def energy_bar(self) -> EnergyBar:
        """获取能量条对象（用于数据更新）"""
        return self._energy_bar

    @property
    def time_hud(self) -> TimeHUD:
        """获取时间 HUD 对象（用于数据更新）"""
        return self._time_hud

    @property
    def dialog_ui(self) -> DialogUI:
        return self._dialog_ui

    def render(self, dt: float, group, player_sprite: PlayerSprite,
               scene_manager, map_renderer,
               drop_item_renderer=None, notification_manager=None,
               npc_manager=None, bubble_ui=None,
               dialog_engine=None, affection_system=None,
               monster_manager=None, battle_ui=None,
               player_hp=100, player_max_hp=100):
        """
        渲染一帧

        Args:
            dt: 距离上一帧的时间（秒）
            group: pyscroll 精灵组
            player_sprite: 玩家精灵
            scene_manager: 场景管理器
            map_renderer: pyscroll 渲染器
        """
        # 清屏
        self._screen.fill((0, 0, 0))

        # pyscroll 渲染地图和精灵（自动处理图层遮挡）
        group.draw(self._screen)

        # 掉落物渲染
        if drop_item_renderer is not None:
            camera_x = map_renderer.x
            camera_y = map_renderer.y
            drop_item_renderer.render(self._screen, camera_x, camera_y)

        # 怪物渲染
        if monster_manager is not None:
            camera_x = map_renderer.x
            camera_y = map_renderer.y
            for monster in monster_manager.get_all_monsters().values():
                monster.render(self._screen, camera_x, camera_y)

        # 头顶气泡渲染
        if bubble_ui and npc_manager:
            camera_x, camera_y = map_renderer.center
            for npc in npc_manager.get_npcs_in_scene():
                screen_x = int(npc.world_x + npc.rect.width // 2)
                screen_y = int(npc.world_y)
                bubble_ui.render(self._screen, npc.npc_id, screen_x, screen_y)

        # HUD 渲染（在游戏画面上方）
        tile_x = player_sprite.world_x / TILE_SIZE
        tile_y = player_sprite.world_y / TILE_SIZE
        self._hud.set_info(
            role_name=self._player_data.get('role_name', 'Unknown'),
            pos_x=tile_x,
            pos_y=tile_y,
            scene_id=self._player_data.get('scene_id', 'farm'),
        )
        self._hud.draw(self._screen)

        # 能量条渲染（右下角）
        self._energy_bar.draw(self._screen)

        # 时间 HUD 渲染（左上角）
        self._time_hud.draw(self._screen)

        # 对话框渲染
        if dialog_engine and dialog_engine.is_active:
            npc_id = dialog_engine.current_npc_id
            npc_name = ""
            bubble_color = (70, 130, 180)
            if npc_manager:
                npc = npc_manager.get_npc_by_id(npc_id)
                if npc:
                    npc_name = npc.name
            self._dialog_ui.render(
                screen=self._screen,
                npc_id=npc_id,
                npc_name=npc_name,
                displayed_text=dialog_engine.displayed_text,
                is_text_complete=dialog_engine.is_text_complete,
                responses=dialog_engine.responses,
                selected_option=dialog_engine.selected_option,
                is_choosing=dialog_engine.state.value == 'choosing',
                bubble_color=bubble_color,
            )

        # 战斗UI渲染
        if battle_ui is not None:
            camera_x = map_renderer.x
            camera_y = map_renderer.y
            battle_ui.render(self._screen, camera_x, camera_y,
                           player_hp, player_max_hp)

        # 精疲力尽弹窗渲染（最顶层）
        self._exhaustion_modal.draw(self._screen)

        # 通知渲染
        if notification_manager is not None:
            notification_manager.render(self._screen, dt)

        # 场景过渡 Iris 遮罩（最顶层）
        scene_manager._transition.apply(self._screen)

        # 刷新显示
        pygame.display.flip()
