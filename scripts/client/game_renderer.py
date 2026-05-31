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

    def render(self, dt: float, group, player_sprite: PlayerSprite,
               scene_manager, map_renderer):
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

        # 精疲力尽弹窗渲染（最顶层）
        self._exhaustion_modal.draw(self._screen)

        # 场景过渡 Iris 遮罩（最顶层）
        scene_manager._transition.apply(self._screen)

        # 刷新显示
        pygame.display.flip()
