"""
TMX 地图加载模块
使用 pytmx 加载 .tmx 地图文件，提供碰撞检测和地图查询接口
"""
import os
import logging
from typing import Optional, Tuple, List

import pytmx
from pytmx.util_pygame import load_pygame

from .constants import GroundType, ObjectType

logger = logging.getLogger("client.tmx_map")


class TmxMapLoader:
    """
    TMX 地图加载器
    使用 pytmx 加载 .tmx 文件，提供地图数据和碰撞查询
    """

    def __init__(self, tmx_path: str):
        """
        加载 TMX 地图文件

        Args:
            tmx_path: .tmx 文件路径（相对于客户端目录或绝对路径）
        """
        self._tmx_path = tmx_path
        self._tmx: Optional[pytmx.TiledMap] = None
        self._ground_layer: Optional[pytmx.TiledTileLayer] = None
        self._objects_layer: Optional[pytmx.TiledTileLayer] = None
        self._load()

    def _load(self):
        """加载 TMX 文件"""
        if not os.path.exists(self._tmx_path):
            logger.error(f"[TmxMapLoader]TMX file not found: {self._tmx_path}")
            raise FileNotFoundError(f"TMX file not found: {self._tmx_path}")

        self._tmx = load_pygame(self._tmx_path)
        logger.info(f"[TmxMapLoader]Loaded TMX: {self._tmx.width}x{self._tmx.height}, "
                    f"tile_size={self._tmx.tilewidth}x{self._tmx.tileheight}")

        # 获取图层
        for layer in self._tmx.visible_layers:
            if layer.name == "Ground":
                self._ground_layer = layer
            elif layer.name == "Objects":
                self._objects_layer = layer

        if self._ground_layer is None:
            logger.warning("[TmxMapLoader]Ground layer not found")
        else:
            logger.info(f"[TmxMapLoader]Ground layer: {self._ground_layer.width}x{self._ground_layer.height}")

        if self._objects_layer is None:
            logger.warning("[TmxMapLoader]Objects layer not found")
        else:
            logger.info(f"[TmxMapLoader]Objects layer: {self._objects_layer.width}x{self._objects_layer.height}")

    @property
    def width(self) -> int:
        """地图宽度（tile 数量）"""
        return self._tmx.width

    @property
    def height(self) -> int:
        """地图高度（tile 数量）"""
        return self._tmx.height

    @property
    def tile_width(self) -> int:
        """瓦片宽度（像素）"""
        return self._tmx.tilewidth

    @property
    def tile_height(self) -> int:
        """瓦片高度（像素）"""
        return self._tmx.tileheight

    @property
    def pixel_width(self) -> int:
        """地图像素宽度"""
        return self.width * self.tile_width

    @property
    def pixel_height(self) -> int:
        """地图像素高度"""
        return self.height * self.tile_height

    @property
    def tmx_data(self) -> pytmx.TiledMap:
        """获取 pytmx TiledMap 对象（供 pyscroll 使用）"""
        return self._tmx

    def get_ground_gid(self, tile_x: int, tile_y: int) -> int:
        """
        获取地面层指定位置的内部 GID

        Args:
            tile_x: 网格 X 坐标
            tile_y: 网格 Y 坐标

        Returns:
            内部 GID 值，0 表示空
        """
        if self._ground_layer is None:
            return 0
        if tile_x < 0 or tile_x >= self.width:
            return 0
        if tile_y < 0 or tile_y >= self.height:
            return 0
        return self._ground_layer.data[tile_y][tile_x]

    def get_object_gid(self, tile_x: int, tile_y: int) -> int:
        """
        获取地物层指定位置的内部 GID

        Args:
            tile_x: 网格 X 坐标
            tile_y: 网格 Y 坐标

        Returns:
            内部 GID 值，0 表示空
        """
        if self._objects_layer is None:
            return 0
        if tile_x < 0 or tile_x >= self.width:
            return 0
        if tile_y < 0 or tile_y >= self.height:
            return 0
        return self._objects_layer.data[tile_y][tile_x]

    def _get_tile_walkable(self, tile_x: int, tile_y: int, layer_index: int) -> Optional[bool]:
        """
        通过 tile 属性获取可通行状态

        Args:
            tile_x: 网格 X 坐标
            tile_y: 网格 Y 坐标
            layer_index: 图层索引（0=Ground, 1=Objects）

        Returns:
            True=可通行, False=不可通行, None=无属性（默认可通行）
        """
        props = self._tmx.get_tile_properties(tile_x, tile_y, layer_index)
        if props is None:
            return None
        walkable = props.get("walkable")
        if walkable is None:
            return None
        return str(walkable).lower() != "false"

    def is_walkable(self, tile_x: int, tile_y: int) -> bool:
        """
        检查指定 tile 是否可通行
        使用 tile 属性中的 walkable 字段判断

        Args:
            tile_x: 网格 X 坐标
            tile_y: 网格 Y 坐标

        Returns:
            True 表示可通行，False 表示不可通行或越界
        """
        # 越界不可通行
        if tile_x < 0 or tile_x >= self.width:
            return False
        if tile_y < 0 or tile_y >= self.height:
            return False

        # 检查地物层（优先级更高）
        obj_walkable = self._get_tile_walkable(tile_x, tile_y, 1)
        if obj_walkable is not None and not obj_walkable:
            return False

        # 检查地面层
        ground_walkable = self._get_tile_walkable(tile_x, tile_y, 0)
        if ground_walkable is not None and not ground_walkable:
            return False

        return True

    def get_ground_type(self, tile_x: int, tile_y: int) -> Optional[GroundType]:
        """
        获取地面类型枚举
        基于 tile 在 tileset 中的位置推断类型

        Args:
            tile_x: 网格 X 坐标
            tile_y: 网格 Y 坐标

        Returns:
            GroundType 枚举值，无数据时返回 None
        """
        gid = self.get_ground_gid(tile_x, tile_y)
        if gid == 0:
            return None

        props = self._tmx.get_tile_properties(tile_x, tile_y, 0)
        if props is None:
            return GroundType.GRASS  # 默认草地

        # 基于 tile ID 推断类型
        tile_id = props.get("id", 0)
        ground_map = {
            0: GroundType.GRASS,
            1: GroundType.DIRT,
            2: GroundType.WATER,
            3: GroundType.SAND,
            4: GroundType.TILLED,
        }
        return ground_map.get(tile_id, GroundType.GRASS)

    def get_object_type(self, tile_x: int, tile_y: int) -> Optional[ObjectType]:
        """
        获取地物类型枚举
        基于 tile 属性推断类型

        Args:
            tile_x: 网格 X 坐标
            tile_y: 网格 Y 坐标

        Returns:
            ObjectType 枚举值，无数据或空地物时返回 None
        """
        gid = self.get_object_gid(tile_x, tile_y)
        if gid == 0:
            return None

        props = self._tmx.get_tile_properties(tile_x, tile_y, 1)
        if props is None:
            return ObjectType.NONE

        tile_id = props.get("id", 0)
        object_map = {
            8: ObjectType.STONE,
            9: ObjectType.CROP_GROWING,
            10: ObjectType.CROP_READY,
            11: ObjectType.TREE,
            12: ObjectType.TREE,
            13: ObjectType.DOOR_IN,
            14: ObjectType.BED,
            15: ObjectType.TV,
            16: ObjectType.STOVE,
        }
        return object_map.get(tile_id, ObjectType.NONE)

    def check_walkable_rect(self, x: float, y: float, width: int, height: int) -> bool:
        """
        检查矩形区域是否可通行（使用四角检测）

        Args:
            x: 像素 X 坐标
            y: 像素 Y 坐标
            width: 宽度（像素）
            height: 高度（像素）

        Returns:
            True 表示可通行
        """
        tile_w = self.tile_width
        tile_h = self.tile_height

        # 四角检测
        corners = [
            (int(x), int(y)),                              # 左上
            (int(x) + width - 1, int(y)),                  # 右上
            (int(x), int(y) + height - 1),                 # 左下
            (int(x) + width - 1, int(y) + height - 1),     # 右下
        ]

        for px, py in corners:
            tx = px // tile_w
            ty = py // tile_h
            if not self.is_walkable(tx, ty):
                return False

        return True
