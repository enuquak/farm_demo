"""
场景过渡动效模块
实现 Iris 圆形遮罩过渡效果
"""
import math
import logging
from typing import Tuple

import pygame

logger = logging.getLogger("client.scene_transition")


class TransitionState:
    """过渡状态常量"""
    IDLE = "IDLE"
    IRIS_CLOSE = "IRIS_CLOSE"
    SWITCHING = "SWITCHING"
    IRIS_OPEN = "IRIS_OPEN"


class SceneTransition:
    """
    场景过渡动效管理器
    使用 PyGame Surface 实现 Iris 圆形遮罩效果

    状态机: IDLE -> IRIS_CLOSE(300ms) -> SWITCHING(1帧) -> IRIS_OPEN(300ms) -> IDLE
    """

    # 过渡动画时长（秒）
    IRIS_DURATION = 0.3

    def __init__(self, screen_width: int, screen_height: int):
        """
        初始化过渡动效

        Args:
            screen_width: 屏幕宽度（像素）
            screen_height: 屏幕高度（像素）
        """
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._state = TransitionState.IDLE
        self._center = (0, 0)
        self._radius = 0.0
        self._max_radius = 0.0
        self._switch_callback = None

    @property
    def state(self) -> str:
        """当前过渡状态"""
        return self._state

    @property
    def is_active(self) -> bool:
        """是否正在过渡中"""
        return self._state != TransitionState.IDLE

    def start(self, player_screen_pos: Tuple[int, int], switch_callback=None):
        """
        开始 Iris 关闭过渡

        Args:
            player_screen_pos: 玩家在屏幕上的位置 (x, y)
            switch_callback: 切换场景的回调函数（在 IRIS_CLOSE 完成时调用）
        """
        if self._state != TransitionState.IDLE:
            logger.warning(f"SceneTransition.start called but state is {self._state}")
            return

        self._state = TransitionState.IRIS_CLOSE
        self._center = player_screen_pos
        self._max_radius = math.sqrt(
            self._screen_width ** 2 + self._screen_height ** 2
        ) / 2
        self._radius = self._max_radius
        self._switch_callback = switch_callback

        logger.info(f"SceneTransition started: center={player_screen_pos}, "
                    f"max_radius={self._max_radius:.1f}")

    def update(self, dt: float):
        """
        更新过渡动效

        Args:
            dt: 距离上一帧的时间（秒）
        """
        if self._state == TransitionState.IDLE:
            return

        if self._state == TransitionState.IRIS_CLOSE:
            self._radius -= self._max_radius * dt / self.IRIS_DURATION
            if self._radius <= 0:
                self._radius = 0
                self._state = TransitionState.SWITCHING
                logger.info("SceneTransition: IRIS_CLOSE -> SWITCHING")

        elif self._state == TransitionState.SWITCHING:
            # 执行场景切换回调
            if self._switch_callback:
                self._switch_callback()
                self._switch_callback = None
            self._state = TransitionState.IRIS_OPEN
            logger.info("SceneTransition: SWITCHING -> IRIS_OPEN")

        elif self._state == TransitionState.IRIS_OPEN:
            self._radius += self._max_radius * dt / self.IRIS_DURATION
            if self._radius >= self._max_radius:
                self._radius = self._max_radius
                self._state = TransitionState.IDLE
                logger.info("SceneTransition: IRIS_OPEN -> IDLE")

    def apply(self, surface: pygame.Surface):
        """
        在渲染表面应用 Iris 遮罩

        Args:
            surface: 要应用遮罩的 PyGame Surface
        """
        if self._state == TransitionState.IDLE:
            return

        # 创建遮罩 surface（带 alpha 通道）
        mask = pygame.Surface(
            (self._screen_width, self._screen_height), pygame.SRCALPHA
        )
        mask.fill((0, 0, 0, 255))

        # 在中心位置画一个透明圆（半径为当前 radius）
        radius_int = max(0, int(self._radius))
        if radius_int > 0:
            pygame.draw.circle(
                mask, (0, 0, 0, 0), self._center, radius_int
            )

        # 将遮罩覆盖到画面上
        surface.blit(mask, (0, 0))
