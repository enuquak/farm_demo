"""
客户端常量定义
包含 TileMap 相关的常量和 GroundType 枚举
"""
import os
from enum import IntEnum
from typing import Tuple, Dict, Any, Optional

# 资源目录
ASSETS_DIR = os.path.join(os.path.dirname(__file__), "assets")

# 默认地图文件路径
DEFAULT_MAP_PATH = os.path.join(ASSETS_DIR, "maps", "farm.tmx")

# 玩家精灵表路径
PLAYER_SPRITE_PATH = os.path.join(ASSETS_DIR, "sprites", "player.png")

# 网格尺寸（像素）- 16x16 基础瓦片
TILE_SIZE = 16

# 地物精灵尺寸（像素）
OBJECT_SPRITE_SIZE = 16

# 渲染缩放倍数（16x16 基础瓦片 x 4 = 64px 渲染尺寸）
ZOOM_FACTOR = 4


class GroundType(IntEnum):
    """地面类型枚举"""
    GRASS = 0       # 草地
    DIRT = 1        # 泥土
    WATER = 2       # 水面
    SAND = 3        # 沙地
    TILLED = 4      # 翻耕
    WALL = 5        # 墙壁，不可通行
    WOOD_FLOOR = 6  # 木地板，可通行


class ObjectType(IntEnum):
    """地物类型枚举"""
    NONE = 0           # 无地物
    STONE = 1          # 石头
    CROP_GROWING = 2   # 作物生长中
    CROP_READY = 3     # 作物成熟
    TREE = 4           # 树木
    DOOR_IN = 10       # 门（进入）
    DOOR_OUT = 11      # 门（退出）
    BED = 12           # 床
    TV = 13            # 电视
    STOVE = 14         # 炉灶


class Direction:
    """玩家朝向常量"""
    UP = "up"
    DOWN = "down"
    LEFT = "left"
    RIGHT = "right"


# 地面类型属性表
# 每个条目包含 color (RGB 元组) 和 walkable (是否可通行)
GROUND_PROPERTIES = {
    GroundType.GRASS: {
        "color": (74, 124, 46),       # #4a7c2e 绿色
        "walkable": True,
    },
    GroundType.DIRT: {
        "color": (139, 90, 43),       # 棕色
        "walkable": True,
    },
    GroundType.WATER: {
        "color": (30, 90, 160),       # 蓝色
        "walkable": False,
    },
    GroundType.SAND: {
        "color": (210, 180, 120),     # 沙色
        "walkable": True,
    },
    GroundType.TILLED: {
        "color": (100, 60, 30),       # 深棕色
        "walkable": True,
    },
    GroundType.WALL: {
        "color": (80, 80, 80),        # 深灰色
        "walkable": False,
    },
    GroundType.WOOD_FLOOR: {
        "color": (160, 110, 60),      # 浅棕色（木地板）
        "walkable": True,
    },
}


# 地物类型属性表
# 每个条目包含 walkable, interactable, interact_type, sprite_data
OBJECT_PROPERTIES: Dict[ObjectType, Dict[str, Any]] = {
    ObjectType.NONE: {
        "walkable": True,
        "interactable": False,
        "interact_type": None,
    },
    ObjectType.STONE: {
        "walkable": False,
        "interactable": False,
        "interact_type": None,
    },
    ObjectType.CROP_GROWING: {
        "walkable": True,
        "interactable": False,
        "interact_type": None,
    },
    ObjectType.CROP_READY: {
        "walkable": True,
        "interactable": True,
        "interact_type": "harvest",
    },
    ObjectType.TREE: {
        "walkable": False,
        "interactable": False,
        "interact_type": None,
    },
    ObjectType.DOOR_IN: {
        "walkable": True,
        "interactable": True,
        "interact_type": "portal",
    },
    ObjectType.DOOR_OUT: {
        "walkable": True,
        "interactable": True,
        "interact_type": "portal",
    },
    ObjectType.BED: {
        "walkable": False,
        "interactable": True,
        "interact_type": "sleep",
    },
    ObjectType.TV: {
        "walkable": False,
        "interactable": True,
        "interact_type": "watch",
    },
    ObjectType.STOVE: {
        "walkable": False,
        "interactable": True,
        "interact_type": "cook",
    },
}

# 2.5D 高光/阴影系数
HIGHLIGHT_ALPHA = 30    # 高光增量
SHADOW_ALPHA = 40       # 阴影减量

# 玩家动画帧间隔（秒）
PLAYER_ANIM_FRAME_DURATION = 0.15

# 默认地图尺寸（16px tiles，120x100 = 约与之前 60x50@32px 相同世界大小）
DEFAULT_MAP_WIDTH = 120
DEFAULT_MAP_HEIGHT = 100

# 帧率
TARGET_FPS = 60

# 玩家移动速度（像素/秒）- 16px tiles, 约 4 tiles/s
PLAYER_SPEED = 64.0

# 位置更新间隔（秒）
POSITION_UPDATE_INTERVAL = 0.1  # 100ms

# 位置纠正插值时长（秒）
POSITION_CORRECT_DURATION = 0.2  # 200ms

# ========== 快捷栏 UI 常量 ==========
HOTBAR_SLOT_COUNT = 10          # 快捷栏格数
HOTBAR_SLOT_SIZE = 48           # 单格尺寸（像素）
HOTBAR_SLOT_GAP = 4             # 格间距（像素）
HOTBAR_MARGIN_BOTTOM = 10       # 距屏幕底部边距（像素）
HOTBAR_BG_COLOR = (40, 40, 40)              # 深灰色背景
HOTBAR_BORDER_COLOR = (80, 80, 80)          # 灰色边框
HOTBAR_ACTIVE_COLOR = (255, 215, 0)         # 金色选中高亮
HOTBAR_TEXT_COLOR = (255, 255, 255)         # 白色数量文字
HOTBAR_TEXT_SHADOW_COLOR = (0, 0, 0)        # 黑色文字阴影

# ========== 背包面板 UI 常量 ==========
PANEL_SLOT_SIZE = 48            # 面板单格尺寸（像素）
PANEL_SLOT_GAP = 4              # 面板格间距（像素）
PANEL_COLS = 10                 # 面板列数
PANEL_ROWS = 3                  # 面板行数（30 格 = 10x3）
PANEL_BG_COLOR = (50, 50, 50)              # 面板背景色
PANEL_BORDER_COLOR = (100, 100, 100)       # 面板边框色
PANEL_TITLE_COLOR = (255, 255, 255)        # 面板标题色
PANEL_CLOSE_BTN_COLOR = (200, 60, 60)      # 关闭按钮颜色（红色）
PANEL_OVERLAY_ALPHA = 128                  # 覆盖层透明度（0-255）

# ========== 物品图标调色板 ==========
# 每个条目包含 colors (像素值->RGB 映射) 和 pixels (8x8 像素数据)
# 0 = 透明，非零 = 颜色索引
ITEM_ICON_PALETTE: Dict[int, Dict[str, Any]] = {
    # 斧头 (item_id=3)
    3: {
        "colors": {
            1: (101, 67, 33),   # 棕色（木柄）
            2: (60, 60, 60),    # 深灰色（金属刃）
            3: (140, 140, 140), # 浅灰色（金属高光）
        },
        "pixels": [
            [0,0,0,0,0,2,2,0],
            [0,0,0,0,2,3,2,0],
            [0,0,0,2,3,3,2,0],
            [0,0,0,2,3,2,0,0],
            [0,0,1,2,2,0,0,0],
            [0,1,0,1,0,0,0,0],
            [1,0,0,0,1,0,0,0],
            [0,0,0,0,0,1,0,0],
        ],
    },
    # 锄头 (item_id=4)
    4: {
        "colors": {
            1: (101, 67, 33),   # 棕色（木柄）
            2: (60, 60, 60),    # 深灰色（金属头）
            3: (140, 140, 140), # 浅灰色（金属高光）
        },
        "pixels": [
            [0,0,0,0,0,0,2,2],
            [0,0,0,0,0,2,3,2],
            [0,0,0,0,0,2,3,2],
            [0,0,0,0,0,0,2,0],
            [0,0,0,0,1,0,0,0],
            [0,0,0,1,0,0,0,0],
            [0,0,1,0,0,0,0,0],
            [0,1,0,0,0,0,0,0],
        ],
    },
    # 种子 (item_id=5)
    5: {
        "colors": {
            1: (101, 67, 33),   # 棕色（袋口）
            2: (160, 110, 60),  # 浅棕色（袋身）
            3: (50, 180, 50),   # 绿色（种子露出）
        },
        "pixels": [
            [0,0,0,0,0,0,0,0],
            [0,0,0,1,1,0,0,0],
            [0,0,1,2,2,1,0,0],
            [0,0,1,2,2,1,0,0],
            [0,0,1,2,3,1,0,0],
            [0,0,1,3,3,1,0,0],
            [0,0,1,2,2,1,0,0],
            [0,0,0,1,1,0,0,0],
        ],
    },
    # 面包 (item_id=6)
    6: {
        "colors": {
            1: (180, 120, 50),  # 深棕色（面包皮）
            2: (220, 170, 90),  # 浅棕色（面包体）
            3: (255, 220, 150), # 亮色（面包高光）
        },
        "pixels": [
            [0,0,0,0,0,0,0,0],
            [0,0,0,1,1,0,0,0],
            [0,0,1,3,3,1,0,0],
            [0,1,2,2,2,2,1,0],
            [0,1,2,2,2,2,1,0],
            [0,1,2,2,2,2,1,0],
            [0,0,1,1,1,1,0,0],
            [0,0,0,0,0,0,0,0],
        ],
    },
}
