"""
玩家精灵模块
支持 4 方向行走动画，使用 sprite sheet 实现帧动画
"""
import os
import logging
from typing import Dict, List, Optional

import pygame

from .constants import TILE_SIZE, Direction, PLAYER_ANIM_FRAME_DURATION

logger = logging.getLogger("client.player_sprite")

# 方向到 sprite sheet 行号的映射
DIRECTION_ROW = {
    Direction.DOWN: 0,
    Direction.LEFT: 1,
    Direction.RIGHT: 2,
    Direction.UP: 3,
}


class PlayerSprite(pygame.sprite.Sprite):
    """
    玩家精灵
    支持 4 方向行走动画（每方向 4 帧）
    """

    def __init__(self, pos_x: float, pos_y: float, sprite_sheet_path: str, zoom: int = 1):
        """
        初始化玩家精灵

        Args:
            pos_x: 初始 X 坐标（地图像素）
            pos_y: 初始 Y 坐标（地图像素）
            sprite_sheet_path: 精灵表 PNG 路径
            zoom: 渲染缩放倍数（pyscroll 不自动缩放精灵，需预缩放）
        """
        super().__init__()

        self._zoom = zoom
        self._frame_width = TILE_SIZE
        self._frame_height = TILE_SIZE

        # 加载并预缩放精灵表
        self._frames = self._load_sprite_sheet(sprite_sheet_path)

        # 当前状态
        self._direction = Direction.DOWN
        self._frame_index = 0  # 0=静止, 1-3=行走
        self._anim_timer = 0.0
        self._is_moving = False

        # 精灵属性
        self.image = self._frames[self._direction][0]
        self.rect = self.image.get_rect()
        self.rect.topleft = (int(pos_x), int(pos_y))

        # 渲染层级（地面层之上，地物层之下）
        self._layer = 1

        logger.info(f"[PlayerSprite]Initialized at ({pos_x}, {pos_y}), zoom={zoom}")

    def _load_sprite_sheet(self, path: str) -> Dict[str, List[pygame.Surface]]:
        """
        加载精灵表并按方向分帧

        Args:
            path: 精灵表 PNG 路径

        Returns:
            {direction: [frame0, frame1, frame2, frame3]}
        """
        if not os.path.exists(path):
            logger.warning(f"[PlayerSprite]Sprite sheet not found: {path}, using fallback")
            return self._create_fallback_frames()

        sheet = pygame.image.load(path).convert_alpha()
        frames = {}

        for direction, row in DIRECTION_ROW.items():
            direction_frames = []
            for col in range(4):
                # 从精灵表裁剪帧
                frame_rect = pygame.Rect(
                    col * self._frame_width,
                    row * self._frame_height,
                    self._frame_width,
                    self._frame_height,
                )
                frame = sheet.subsurface(frame_rect).copy()

                # 预缩放
                if self._zoom != 1:
                    scaled_size = (
                        self._frame_width * self._zoom,
                        self._frame_height * self._zoom,
                    )
                    frame = pygame.transform.scale(frame, scaled_size)

                direction_frames.append(frame)
            frames[direction] = direction_frames

        logger.info(f"[PlayerSprite]Loaded sprite sheet: {path}")
        return frames

    def _create_fallback_frames(self) -> Dict[str, List[pygame.Surface]]:
        """
        创建后备精灵帧（当精灵表文件不存在时使用）

        Returns:
            {direction: [frame0, frame1, frame2, frame3]}
        """
        size = self._frame_width * self._zoom
        frames = {}

        # 颜色方案
        body_color = (50, 120, 200)
        outline_color = (255, 255, 255)

        for direction in DIRECTION_ROW:
            direction_frames = []
            for frame_idx in range(4):
                surface = pygame.Surface((size, size), pygame.SRCALPHA)
                surface.fill((0, 0, 0, 0))

                # 绘制简单方块角色
                margin = 2 * self._zoom
                body_rect = pygame.Rect(margin, margin, size - 2 * margin, size - 2 * margin)
                pygame.draw.rect(surface, body_color, body_rect)
                pygame.draw.rect(surface, outline_color, body_rect, self._zoom)

                # 行走动画：轻微偏移
                if frame_idx == 1:
                    surface.scroll(dx=-self._zoom)
                elif frame_idx == 3:
                    surface.scroll(dx=self._zoom)

                # 方向指示
                cx, cy = size // 2, size // 2
                arrow_size = 3 * self._zoom
                if direction == Direction.UP:
                    pygame.draw.polygon(surface, (255, 255, 0), [
                        (cx, margin + self._zoom),
                        (cx - arrow_size, margin + arrow_size + self._zoom),
                        (cx + arrow_size, margin + arrow_size + self._zoom),
                    ])
                elif direction == Direction.DOWN:
                    pygame.draw.polygon(surface, (255, 255, 0), [
                        (cx, size - margin - self._zoom),
                        (cx - arrow_size, size - margin - arrow_size - self._zoom),
                        (cx + arrow_size, size - margin - arrow_size - self._zoom),
                    ])
                elif direction == Direction.LEFT:
                    pygame.draw.polygon(surface, (255, 255, 0), [
                        (margin + self._zoom, cy),
                        (margin + arrow_size + self._zoom, cy - arrow_size),
                        (margin + arrow_size + self._zoom, cy + arrow_size),
                    ])
                elif direction == Direction.RIGHT:
                    pygame.draw.polygon(surface, (255, 255, 0), [
                        (size - margin - self._zoom, cy),
                        (size - margin - arrow_size - self._zoom, cy - arrow_size),
                        (size - margin - arrow_size - self._zoom, cy + arrow_size),
                    ])

                direction_frames.append(surface)
            frames[direction] = direction_frames

        logger.info("[PlayerSprite]Using fallback sprites")
        return frames

    def set_position(self, x: float, y: float):
        """
        设置精灵位置

        Args:
            x: 地图像素 X 坐标
            y: 地图像素 Y 坐标
        """
        self.rect.topleft = (int(x), int(y))

    def set_direction(self, direction: str):
        """
        设置朝向

        Args:
            direction: 方向常量（Direction.UP/DOWN/LEFT/RIGHT）
        """
        if direction != self._direction:
            self._direction = direction
            self._update_image()

    def set_moving(self, is_moving: bool):
        """
        设置移动状态

        Args:
            is_moving: 是否在移动
        """
        if is_moving != self._is_moving:
            self._is_moving = is_moving
            if not is_moving:
                # 停止时重置到静止帧
                self._frame_index = 0
                self._anim_timer = 0.0
                self._update_image()

    def update_animation(self, dt: float):
        """
        更新动画帧

        Args:
            dt: 距上一帧的时间（秒）
        """
        if not self._is_moving:
            return

        self._anim_timer += dt
        if self._anim_timer >= PLAYER_ANIM_FRAME_DURATION:
            self._anim_timer -= PLAYER_ANIM_FRAME_DURATION
            # 循环 1-3 帧（0 是静止帧）
            self._frame_index = (self._frame_index % 3) + 1
            self._update_image()

    def _update_image(self):
        """更新当前显示的帧"""
        frames = self._frames.get(self._direction)
        if frames and self._frame_index < len(frames):
            self.image = frames[self._frame_index]

    @property
    def direction(self) -> str:
        """当前朝向"""
        return self._direction

    @property
    def world_x(self) -> float:
        """地图像素 X 坐标"""
        return float(self.rect.x)

    @property
    def world_y(self) -> float:
        """地图像素 Y 坐标"""
        return float(self.rect.y)
