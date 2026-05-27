"""
输入管理器模块
提供 Action Map 抽象层将物理按键归并为语义动作，支持鼠标状态追踪和屏幕→世界坐标转换，以及基于切比雪夫距离的交互范围校验。
"""
import pygame
from typing import Dict, List, Tuple, Optional, Any


class InputManager:
    """
    输入管理器
    维护硬编码的 Action Map，将多个物理键映射到语义动作。
    追踪鼠标位置和按键状态。
    """

    # 默认交互范围
    DEFAULT_INTERACT_RANGE = 1
    # 无限制交互范围
    UNLIMITED_INTERACT_RANGE = -1

    def __init__(self):
        """初始化输入管理器"""
        # Action Map: 动作名 -> 物理键列表
        self._action_map: Dict[str, List[int]] = {
            "move_up": [pygame.K_w, pygame.K_UP],
            "move_down": [pygame.K_s, pygame.K_DOWN],
            "move_left": [pygame.K_a, pygame.K_LEFT],
            "move_right": [pygame.K_d, pygame.K_RIGHT],
            "interact": [pygame.K_SPACE],
            "open_inventory": [pygame.K_i, pygame.K_TAB],
            "hotbar_1": [pygame.K_1],
            "hotbar_2": [pygame.K_2],
            "hotbar_3": [pygame.K_3],
            "hotbar_4": [pygame.K_4],
            "hotbar_5": [pygame.K_5],
            "hotbar_6": [pygame.K_6],
            "hotbar_7": [pygame.K_7],
            "hotbar_8": [pygame.K_8],
            "hotbar_9": [pygame.K_9],
            "hotbar_0": [pygame.K_0],
        }

        # 鼠标状态
        self.mouse_pos: Tuple[int, int] = (0, 0)
        self.mouse_pressed: Dict[int, bool] = {1: False, 2: False, 3: False}
        self.mouse_just_pressed: Dict[int, bool] = {1: False, 2: False, 3: False}

        # 上一帧鼠标按键状态（用于计算 just_pressed）
        self._prev_mouse_pressed: Dict[int, bool] = {1: False, 2: False, 3: False}

        # UI 层屏蔽标志
        self._ui_blocking: bool = False

    def update(self):
        """
        每帧更新输入状态
        应在事件循环之后、游戏逻辑之前调用
        """
        # 更新鼠标位置
        self.mouse_pos = pygame.mouse.get_pos()

        # 获取当前鼠标按键状态
        current_pressed = pygame.mouse.get_pressed()

        # 更新鼠标按键状态 (pygame.mouse.get_pressed 返回 (left, middle, right))
        self.mouse_pressed[1] = current_pressed[0]
        self.mouse_pressed[2] = current_pressed[1]
        self.mouse_pressed[3] = current_pressed[2]

        # 计算 just_pressed（本帧按下，上一帧未按下）
        for button in [1, 2, 3]:
            self.mouse_just_pressed[button] = (
                self.mouse_pressed[button] and not self._prev_mouse_pressed[button]
            )

        # 保存当前帧状态到上一帧
        self._prev_mouse_pressed = self.mouse_pressed.copy()

    def is_action_pressed(self, action_name: str) -> bool:
        """
        查询指定动作是否被按下

        Args:
            action_name: 动作名称（如 "move_up", "interact" 等）

        Returns:
            True 表示该动作对应的任意物理键被按下，False 表示未按下
        """
        if action_name not in self._action_map:
            return False

        keys = pygame.key.get_pressed()
        for key in self._action_map[action_name]:
            if keys[key]:
                return True

        return False

    def get_action_keys(self, action_name: str) -> List[int]:
        """
        获取动作映射的物理键列表

        Args:
            action_name: 动作名称

        Returns:
            物理键列表
        """
        return self._action_map.get(action_name, [])

    def set_ui_blocking(self, blocking: bool):
        """
        设置 UI 层屏蔽标志

        Args:
            blocking: True 表示 UI 层屏蔽游戏交互
        """
        self._ui_blocking = blocking

    def is_ui_blocking(self) -> bool:
        """
        检查 UI 层是否屏蔽游戏交互

        Returns:
            True 表示 UI 层屏蔽，不应触发游戏交互
        """
        return self._ui_blocking

    def is_left_mouse_just_pressed(self) -> bool:
        """
        检查鼠标左键是否在本帧按下（上一帧未按下）

        Returns:
            True 表示鼠标左键在本帧按下
        """
        return self.mouse_just_pressed[1]

    def get_mouse_pos(self) -> Tuple[int, int]:
        """
        获取鼠标位置

        Returns:
            (screen_x, screen_y) 屏幕坐标
        """
        return self.mouse_pos


def check_distance(player_tile_x: int, player_tile_y: int,
                   target_tile_x: int, target_tile_y: int,
                   interact_range: int) -> bool:
    """
    使用切比雪夫距离校验交互范围

    Args:
        player_tile_x: 玩家 tile X 坐标
        player_tile_y: 玩家 tile Y 坐标
        target_tile_x: 目标 tile X 坐标
        target_tile_y: 目标 tile Y 坐标
        interact_range: 交互范围（-1 表示无限制）

    Returns:
        True 表示在交互范围内，False 表示超出范围
    """
    # 无限制范围
    if interact_range == InputManager.UNLIMITED_INTERACT_RANGE:
        return True

    # 切比雪夫距离 = max(|x1-x2|, |y1-y2|)
    distance = max(abs(player_tile_x - target_tile_x),
                   abs(player_tile_y - target_tile_y))

    return distance <= interact_range


def get_interact_range(item_effects: Dict[str, Any], effect_key: str) -> int:
    """
    获取物品效果的交互范围

    Args:
        item_effects: ITEM_EFFECTS 字典
        effect_key: 效果键（如 "obj:STONE", "gnd:GRASS", "ANY"）

    Returns:
        交互范围（默认为 1，即九宫格）
    """
    effect_data = item_effects.get(effect_key)
    if effect_data is None:
        return InputManager.DEFAULT_INTERACT_RANGE

    return effect_data.get("interactRange", InputManager.DEFAULT_INTERACT_RANGE)
