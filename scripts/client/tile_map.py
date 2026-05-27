"""
TileMap 数据结构模块
维护二维网格地图数据，支持坐标转换
支持双层数据结构：地面层（Ground）和地物层（Objects）
"""
from typing import Optional, List

from .constants import TILE_SIZE, GroundType, ObjectType


class TileMap:
    """
    TileMap 二维网格地图
    存储地图的地面类型和地物类型数据（双层结构）
    """

    def __init__(self, width: int, height: int):
        """
        初始化 TileMap

        Args:
            width: 地图宽度（tile 数量）
            height: 地图高度（tile 数量）
        """
        self.width = width
        self.height = height
        # 地面层二维数组: ground[y][x]，默认填充 GRASS
        self.ground: List[List[int]] = [
            [GroundType.GRASS for _ in range(width)]
            for _ in range(height)
        ]
        # 地物层二维数组: objects[y][x]，默认填充 NONE
        self.objects: List[List[int]] = [
            [ObjectType.NONE for _ in range(width)]
            for _ in range(height)
        ]

    def get_ground(self, tile_x: int, tile_y: int) -> Optional[GroundType]:
        """
        获取指定位置的地面类型

        Args:
            tile_x: 网格 X 坐标
            tile_y: 网格 Y 坐标

        Returns:
            GroundType 枚举值，越界时返回 None
        """
        if tile_x < 0 or tile_x >= self.width:
            return None
        if tile_y < 0 or tile_y >= self.height:
            return None
        try:
            return GroundType(self.ground[tile_y][tile_x])
        except ValueError:
            return None

    def get_ground_type(self, tile_x: int, tile_y: int) -> Optional[GroundType]:
        """get_ground 的别名，与 TmxMapLoader 接口一致"""
        return self.get_ground(tile_x, tile_y)

    def set_ground(self, tile_x: int, tile_y: int, ground_type: GroundType) -> bool:
        """
        设置指定位置的地面类型

        Args:
            tile_x: 网格 X 坐标
            tile_y: 网格 Y 坐标
            ground_type: 地面类型

        Returns:
            是否设置成功（越界时返回 False）
        """
        if tile_x < 0 or tile_x >= self.width:
            return False
        if tile_y < 0 or tile_y >= self.height:
            return False
        self.ground[tile_y][tile_x] = int(ground_type)
        return True

    def get_object(self, tile_x: int, tile_y: int) -> Optional[ObjectType]:
        """
        获取指定位置的地物类型

        Args:
            tile_x: 网格 X 坐标
            tile_y: 网格 Y 坐标

        Returns:
            ObjectType 枚举值，越界时返回 None
        """
        if tile_x < 0 or tile_x >= self.width:
            return None
        if tile_y < 0 or tile_y >= self.height:
            return None
        try:
            return ObjectType(self.objects[tile_y][tile_x])
        except ValueError:
            return None

    def get_object_type(self, tile_x: int, tile_y: int) -> Optional[ObjectType]:
        """get_object 的别名，与 TmxMapLoader 接口一致"""
        return self.get_object(tile_x, tile_y)

    def set_object(self, tile_x: int, tile_y: int, obj_type: ObjectType) -> bool:
        """
        设置指定位置的地物类型

        Args:
            tile_x: 网格 X 坐标
            tile_y: 网格 Y 坐标
            obj_type: 地物类型

        Returns:
            是否设置成功（越界时返回 False）
        """
        if tile_x < 0 or tile_x >= self.width:
            return False
        if tile_y < 0 or tile_y >= self.height:
            return False
        self.objects[tile_y][tile_x] = int(obj_type)
        return True

    def fill_from_data(self, ground_data: List[List[int]], objects_data: Optional[List[List[int]]] = None):
        """
        从服务器数据填充地图（支持双层数据）

        Args:
            ground_data: 二维数组，ground_data[y][x] 为地面类型 ID
            objects_data: 二维数组，objects_data[y][x] 为地物类型 ID（可选）
        """
        # 填充地面层
        for y in range(min(len(ground_data), self.height)):
            row = ground_data[y]
            for x in range(min(len(row), self.width)):
                self.ground[y][x] = row[x]

        # 填充地物层（如果提供）
        if objects_data is not None:
            for y in range(min(len(objects_data), self.height)):
                row = objects_data[y]
                for x in range(min(len(row), self.width)):
                    self.objects[y][x] = row[x]

    @property
    def pixel_width(self) -> int:
        """地图像素宽度"""
        return self.width * TILE_SIZE

    @property
    def pixel_height(self) -> int:
        """地图像素高度"""
        return self.height * TILE_SIZE


def tile_to_pixel(tile_x: int, tile_y: int) -> tuple:
    """
    网格坐标转像素坐标（左上角）

    Args:
        tile_x: 网格 X 坐标
        tile_y: 网格 Y 坐标

    Returns:
        (pixel_x, pixel_y) 像素坐标
    """
    return (tile_x * TILE_SIZE, tile_y * TILE_SIZE)


def pixel_to_tile(pixel_x: int, pixel_y: int) -> tuple:
    """
    像素坐标转网格坐标

    Args:
        pixel_x: 像素 X 坐标
        pixel_y: 像素 Y 坐标

    Returns:
        (tile_x, tile_y) 网格坐标
    """
    return (pixel_x // TILE_SIZE, pixel_y // TILE_SIZE)
