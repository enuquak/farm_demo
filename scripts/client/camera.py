"""
正交投影相机模块
以玩家为中心，支持 lerp 平滑跟随和地图边缘约束
"""
from .constants import TILE_SIZE

# lerp 插值系数（0-1，越大跟随越快）
CAMERA_LERP_FACTOR = 0.1


class Camera:
    """
    正交投影相机
    使用 lerp 插值平滑跟随玩家移动，约束在地图边界内
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

    def follow(self, player_pixel_x: float, player_pixel_y: float, lerp: bool = True):
        """
        使相机跟随玩家，带 lerp 平滑插值和地图边缘约束

        Args:
            player_pixel_x: 玩家世界像素 X 坐标
            player_pixel_y: 玩家世界像素 Y 坐标
            lerp: 是否使用 lerp 插值（True=平滑跟随，False=直接居中）
        """
        # 目标位置：使玩家居中
        target_x = player_pixel_x - self.viewport_width / 2
        target_y = player_pixel_y - self.viewport_height / 2

        if lerp:
            # lerp 插值：平滑过渡到目标位置
            self.x += (target_x - self.x) * CAMERA_LERP_FACTOR
            self.y += (target_y - self.y) * CAMERA_LERP_FACTOR
        else:
            # 直接跳到目标位置
            self.x = target_x
            self.y = target_y

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
        return (int(world_x) // TILE_SIZE, int(world_y) // TILE_SIZE)

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
        start_x = int(self.x) // TILE_SIZE
        start_y = int(self.y) // TILE_SIZE
        end_x = int(self.x + self.viewport_width) // TILE_SIZE
        end_y = int(self.y + self.viewport_height) // TILE_SIZE

        # 确保范围有效
        start_x = max(0, start_x)
        start_y = max(0, start_y)
        end_x = max(0, end_x)
        end_y = max(0, end_y)

        return (start_x, start_y, end_x, end_y)
