"""
NPC 精灵模块
支持 4 方向动画，使用 sprite sheet 实现帧动画
"""
import os
import logging
from typing import Dict, List, Optional

import pygame

from .constants import TILE_SIZE, Direction, PLAYER_ANIM_FRAME_DURATION

logger = logging.getLogger("client.npc_sprite")

# 方向到 sprite sheet 行号的映射
DIRECTION_ROW = {
    Direction.DOWN: 0,
    Direction.LEFT: 1,
    Direction.RIGHT: 2,
    Direction.UP: 3,
}


class NPCSprite(pygame.sprite.Sprite):
    """
    NPC 精灵，渲染动画和状态管理。

    支持 4 方向动画（每方向 4 帧），与 PlayerSprite 类似的 sprite sheet 格式。
    当 sprite sheet 文件不存在时，会自动生成占位精灵。
    """

    def __init__(self, npc_id: str, name: str, pos_x: float, pos_y: float,
                 sprite_sheet_path: str, zoom: int = 1):
        """
        初始化 NPC 精灵

        Args:
            npc_id: NPC 唯一标识符
            name: NPC 显示名称
            pos_x: 初始 X 坐标（地图像素）
            pos_y: 初始 Y 坐标（地图像素）
            sprite_sheet_path: 精灵表 PNG 路径
            zoom: 渲染缩放倍数（pyscroll 不自动缩放精灵，需预缩放）
        """
        super().__init__()

        self.npc_id = npc_id
        self.name = name
        self._zoom = zoom

        # 加载并预缩放精灵表
        self._frames: Dict[str, List[pygame.Surface]] = self._load_frames(sprite_sheet_path)

        # 当前状态
        self._direction = Direction.DOWN
        self._frame_index = 0
        self._anim_timer = 0.0

        # 精灵属性
        self.image = self._frames[self._direction][0]
        self.rect = self.image.get_rect(topleft=(int(pos_x), int(pos_y)))

        # NPC 特有属性
        self._is_interactable = False
        self._scene_id: Optional[str] = None

        # 渲染层级（地面层之上，地物层之下）
        self._layer = 1

        logger.info(f"[NPCSprite]Initialized NPC '{name}' (id={npc_id}) at ({pos_x}, {pos_y}), zoom={zoom}")

    def _load_frames(self, sprite_sheet_path: str) -> Dict[str, List[pygame.Surface]]:
        """
        加载精灵表并按方向分帧

        Args:
            sprite_sheet_path: 精灵表 PNG 路径

        Returns:
            {direction: [frame0, frame1, frame2, frame3]}
        """
        if not os.path.exists(sprite_sheet_path):
            logger.warning(f"[NPCSprite]Sprite sheet not found: {sprite_sheet_path}, using placeholder")
            return self._generate_placeholder()

        sheet = pygame.image.load(sprite_sheet_path).convert_alpha()
        frame_w = sheet.get_width() // 4
        frame_h = sheet.get_height() // 4

        frames = {}
        for row, direction in enumerate(DIRECTION_ROW.keys()):
            direction_frames = []
            for col in range(4):
                rect = pygame.Rect(col * frame_w, row * frame_h, frame_w, frame_h)
                frame = sheet.subsurface(rect).copy()

                # 预缩放
                if self._zoom != 1:
                    scaled_size = (frame_w * self._zoom, frame_h * self._zoom)
                    frame = pygame.transform.scale(frame, scaled_size)

                direction_frames.append(frame)
            frames[direction] = direction_frames

        logger.info(f"[NPCSprite]Loaded sprite sheet: {sprite_sheet_path}")
        return frames

    def _generate_placeholder(self) -> Dict[str, List[pygame.Surface]]:
        """
        生成占位精灵帧（当 sprite sheet 文件不存在时使用）

        Returns:
            {direction: [frame0, frame1, frame2, frame3]}
        """
        size = 32 * self._zoom
        color = (70, 130, 180)  # 蓝色调，与玩家区分
        frames = {}

        for direction in DIRECTION_ROW.keys():
            direction_frames = []
            for i in range(4):
                surface = pygame.Surface((size, size), pygame.SRCALPHA)
                surface.fill((0, 0, 0, 0))

                # 绘制简单方块角色
                margin = 2 * self._zoom
                body_rect = pygame.Rect(margin, margin, size - 2 * margin, size - 2 * margin)
                pygame.draw.rect(surface, color, body_rect)

                # 绘制头部（浅色圆形）
                head_color = (255, 220, 180)
                head_radius = size // 6
                head_center = (size // 2, size // 4 - 2)
                pygame.draw.circle(surface, head_color, head_center, head_radius)

                # 绘制身体
                body_rect = pygame.Rect(
                    size // 2 - 4 * self._zoom,
                    size // 4 - 2 + head_radius,
                    8 * self._zoom,
                    12 * self._zoom
                )
                pygame.draw.rect(surface, color, body_rect)

                direction_frames.append(surface)
            frames[direction] = direction_frames

        logger.info("[NPCSprite]Using placeholder sprites")
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
        if direction in self._frames:
            self._direction = direction
            self._frame_index = 0
            self.image = self._frames[self._direction][0]

    def set_interactable(self, interactable: bool):
        """
        设置 NPC 是否可交互

        Args:
            interactable: 是否可交互
        """
        self._is_interactable = interactable

    @property
    def is_interactable(self) -> bool:
        """NPC 是否可交互"""
        return self._is_interactable

    @property
    def world_x(self) -> float:
        """地图像素 X 坐标"""
        return float(self.rect.x)

    @property
    def world_y(self) -> float:
        """地图像素 Y 坐标"""
        return float(self.rect.y)

    @property
    def tile_x(self) -> int:
        """所在瓦片 X 坐标"""
        return self.rect.x // (TILE_SIZE * self._zoom)

    @property
    def tile_y(self) -> int:
        """所在瓦片 Y 坐标"""
        return self.rect.y // (TILE_SIZE * self._zoom)

    @property
    def direction(self) -> str:
        """当前朝向"""
        return self._direction

    def update_animation(self, dt: float):
        """
        更新动画帧

        Args:
            dt: 距上一帧的时间（秒）
        """
        self._anim_timer += dt
        if self._anim_timer >= PLAYER_ANIM_FRAME_DURATION * 2:
            self._anim_timer = 0.0
            self._frame_index = (self._frame_index + 1) % len(self._frames[self._direction])
            self.image = self._frames[self._direction][self._frame_index]
