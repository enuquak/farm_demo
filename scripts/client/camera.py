"""
正交投影相机模块
以玩家为中心，支持地图边缘约束
"""
from .constants import TILE_SIZE
from .tile_map import pixel_to_tile


class Camera:
    """
    正交投影相机
    跟随玩家移动，约束在地图边界内
    """

    def __init__(self, viewport_width: int, viewport_height: int):
        """
        初始化相机

        Args:
            viewport_width: 视口宽度（像素）
            viewport_height: 视口高度（像素）
        """
        self.viewport_width = viewport_width
        self.viewport_height = viewport_height

        # 相机左上角的世界像素坐标
        self.x: float = 0.0
        self.y: float = 0.0

        # 地图边界（像素），由 set_map_bounds 设置
        self._map_width: int = 0
        self._map_height: int = 0

    def set_map_bounds(self, map_pixel_width: int, map_pixel_height: int):
        """
        设置地图边界

        Args:
            map_pixel_width: 地图像素宽度
            map_pixel_height: 地图像素高度
        """
        self._map_width = map_pixel_width
        self._map_height = map_pixel_height

    def follow(self, player_pixel_x: float, player_pixel_y: float):
        """
        使相机居中于玩家，带地图边缘约束

        Args:
            player_pixel_x: 玩家世界像素 X 坐标
            player_pixel_y: 玩家世界像素 Y 坐标
        """
        # 计算相机左上角，使玩家居中
        self.x = player_pixel_x - self.viewport_width / 2
        self.y = player_pixel_y - self.viewport_height / 2

        # 地图边缘约束
        self._clamp_to_bounds()

    def _clamp_to_bounds(self):
        """将相机约束在地图边界内"""
        # 左上角约束
        if self.x < 0:
            self.x = 0
        if self.y < 0:
            self.y = 0

        # 右下角约束
        max_x = self._map_width - self.viewport_width
        max_y = self._map_height - self.viewport_height

        if max_x < 0:
            # 地图比视口窄，居中显示
            self.x = max_x / 2
        elif self.x > max_x:
            self.x = max_x

        if max_y < 0:
            # 地图比视口矮，居中显示
            self.y = max_y / 2
        elif self.y > max_y:
            self.y = max_y

    def screen_to_world(self, screen_x: int, screen_y: int) -> tuple:
        """
        屏幕像素坐标转世界像素坐标

        Args:
            screen_x: 屏幕 X 坐标
            screen_y: 屏幕 Y 坐标

        Returns:
            (world_x, world_y) 世界像素坐标
        """
        world_x = screen_x + self.x
        world_y = screen_y + self.y
        return (world_x, world_y)

    def screen_to_tile(self, screen_x: int, screen_y: int) -> tuple:
        """
        屏幕像素坐标转世界网格坐标

        Args:
            screen_x: 屏幕 X 坐标
            screen_y: 屏幕 Y 坐标

        Returns:
            (tile_x, tile_y) 网格坐标
        """
        world_x, world_y = self.screen_to_world(screen_x, screen_y)
        return pixel_to_tile(int(world_x), int(world_y))

    def world_to_screen(self, world_x: float, world_y: float) -> tuple:
        """
        世界像素坐标转屏幕像素坐标

        Args:
            world_x: 世界 X 坐标
            world_y: 世界 Y 坐标

        Returns:
            (screen_x, screen_y) 屏幕像素坐标
        """
        screen_x = world_x - self.x
        screen_y = world_y - self.y
        return (screen_x, screen_y)

    def get_visible_tile_range(self) -> tuple:
        """
        获取可见区域的 tile 范围

        Returns:
            (start_x, start_y, end_x, end_y) tile 坐标范围（不含 end）
        """
        from .tile_map import pixel_to_tile
        start_x, start_y = pixel_to_tile(int(self.x), int(self.y))
        end_x, end_y = pixel_to_tile(
            int(self.x + self.viewport_width),
            int(self.y + self.viewport_height)
        )

        # 确保范围有效
        start_x = max(0, start_x)
        start_y = max(0, start_y)
        end_x = max(0, end_x)
        end_y = max(0, end_y)

        return (start_x, start_y, end_x, end_y)
