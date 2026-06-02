"""任务追踪条 HUD

屏幕右侧常驻显示 1-3 个进行中任务的简要进度。
点击任务行可打开完整任务面板。
"""
import logging
import time
from typing import List, Optional, Tuple

import pygame

logger = logging.getLogger("client.ui.quest_tracker")


class _FlashAnimation:
    """进度更新时的闪烁动画"""

    def __init__(self, quest_id: str, duration: float = 0.5):
        self.quest_id = quest_id
        self.start_time = time.time()
        self.duration = duration

    @property
    def active(self) -> bool:
        return time.time() - self.start_time < self.duration

    @property
    def alpha(self) -> float:
        """返回 0.0 ~ 1.0 的闪烁强度"""
        elapsed = time.time() - self.start_time
        t = elapsed / self.duration
        # 快速闪烁 3 次然后衰减
        import math
        return max(0.0, (1.0 - t) * abs(math.sin(t * math.pi * 6)))


class QuestTracker:
    """任务追踪条 HUD 组件"""

    # 布局配置
    WIDTH = 200
    MARGIN_TOP = 40       # 距屏幕顶部（避开 TimeHUD）
    MARGIN_RIGHT = 10     # 距屏幕右侧
    PADDING = 8
    ROW_HEIGHT = 36       # 每个任务行高度
    TITLE_HEIGHT = 24
    HINT_HEIGHT = 18
    MAX_TRACKED = 3       # 最多追踪 3 个任务

    # 颜色配置
    BG_COLOR = (0, 0, 0, 160)
    BORDER_COLOR = (60, 80, 60, 200)
    TITLE_COLOR = (106, 176, 255)    # 蓝色标题
    QUEST_NAME_COLOR = (255, 215, 0) # 金色任务名
    PROGRESS_COLOR = (200, 200, 200) # 灰色进度文字
    PROGRESS_BAR_BG = (40, 40, 40)
    PROGRESS_BAR_FG = (76, 175, 80)  # 绿色进度条
    FLASH_COLOR = (100, 255, 100)    # 闪烁绿色
    HINT_COLOR = (100, 100, 100)
    HOVER_BG = (255, 255, 255, 20)

    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height

        # 位置（右上角）
        self._x = screen_width - self.WIDTH - self.MARGIN_RIGHT
        self._y = self.MARGIN_TOP

        # 字体
        pygame.font.init()
        self._font = pygame.font.SysFont("microsoftyahei", 13)
        if self._font is None:
            self._font = pygame.font.Font(None, 13)
        self._title_font = pygame.font.SysFont("microsoftyahei", 14)
        if self._title_font is None:
            self._title_font = pygame.font.Font(None, 14)
        self._hint_font = pygame.font.SysFont("microsoftyahei", 11)
        if self._hint_font is None:
            self._hint_font = pygame.font.Font(None, 11)

        # 追踪的任务数据（从 QuestData 更新）
        self._tracked_quests: list = []  # List of QuestInfo-like objects

        # 闪烁动画
        self._flash_animations: List[_FlashAnimation] = []

        # 鼠标悬停行索引
        self._hover_row: int = -1

        # 回调：点击任务行时调用
        self._on_quest_click = None

        logger.info(f"[QuestTracker]Initialized at ({self._x}, {self._y})")

    def set_on_quest_click(self, callback) -> None:
        """设置点击任务行的回调函数。callback(quest_id: str)"""
        self._on_quest_click = callback

    def update(self, quest_data) -> None:
        """从 QuestData 刷新追踪的任务列表。"""
        tracked = quest_data.get_tracked_quests(self.MAX_TRACKED)
        # 检测进度变化并触发闪烁
        old_quests = {q.quest_id: q for q in self._tracked_quests}
        for quest in tracked:
            old = old_quests.get(quest.quest_id)
            if old:
                for new_obj in quest.objectives:
                    for old_obj in old.objectives:
                        if new_obj.obj_id == old_obj.obj_id and new_obj.current != old_obj.current:
                            self._flash_animations.append(
                                _FlashAnimation(quest.quest_id)
                            )
        self._tracked_quests = tracked

    def draw(self, screen: pygame.Surface) -> None:
        """绘制追踪条。"""
        if not self._tracked_quests:
            return

        # 计算总高度
        content_height = (self.TITLE_HEIGHT +
                          len(self._tracked_quests) * self.ROW_HEIGHT +
                          self.HINT_HEIGHT + self.PADDING * 2)
        surface = pygame.Surface((self.WIDTH, content_height), pygame.SRCALPHA)

        # 背景
        surface.fill(self.BG_COLOR)
        pygame.draw.rect(surface, self.BORDER_COLOR,
                         (0, 0, self.WIDTH, content_height), 1, border_radius=6)

        # 标题
        title_surf = self._title_font.render("📜 追踪中", True, self.TITLE_COLOR)
        surface.blit(title_surf, (self.PADDING, self.PADDING))

        # 任务行
        y_offset = self.PADDING + self.TITLE_HEIGHT
        for i, quest in enumerate(self._tracked_quests):
            row_y = y_offset + i * self.ROW_HEIGHT

            # 悬停高亮
            if i == self._hover_row:
                hover_surf = pygame.Surface((self.WIDTH, self.ROW_HEIGHT), pygame.SRCALPHA)
                hover_surf.fill(self.HOVER_BG)
                surface.blit(hover_surf, (0, row_y))

            # 任务名
            name_color = self.QUEST_NAME_COLOR
            name_surf = self._font.render(quest.name, True, name_color)
            surface.blit(name_surf, (self.PADDING, row_y + 2))

            # 进度条
            if quest.objectives:
                obj = quest.objectives[0]  # 显示第一个目标的进度
                progress_text = f"{obj.current}/{obj.required}"
                bar_y = row_y + 20
                bar_width = self.WIDTH - self.PADDING * 2
                bar_height = 6

                # 背景条
                pygame.draw.rect(surface, self.PROGRESS_BAR_BG,
                                 (self.PADDING, bar_y, bar_width, bar_height),
                                 border_radius=3)

                # 填充条
                fill_ratio = min(1.0, obj.current / max(1, obj.required))
                fill_width = int(bar_width * fill_ratio)
                bar_color = self.PROGRESS_BAR_FG

                # 闪烁效果
                for flash in self._flash_animations:
                    if flash.quest_id == quest.quest_id and flash.active:
                        bar_color = self.FLASH_COLOR
                        break

                pygame.draw.rect(surface, bar_color,
                                 (self.PADDING, bar_y, fill_width, bar_height),
                                 border_radius=3)

                # 进度数字
                prog_surf = self._font.render(progress_text, True, self.PROGRESS_COLOR)
                surface.blit(prog_surf, (self.WIDTH - self.PADDING - 40, row_y + 2))

        # 提示文字
        hint_y = y_offset + len(self._tracked_quests) * self.ROW_HEIGHT + 2
        hint_surf = self._hint_font.render("按 Q 展开", True, self.HINT_COLOR)
        surface.blit(hint_surf, (self.WIDTH // 2 - hint_surf.get_width() // 2, hint_y))

        # 清理过期动画
        self._flash_animations = [f for f in self._flash_animations if f.active]

        # 绘制到屏幕
        screen.blit(surface, (self._x, self._y))

    def handle_event(self, event: pygame.event.Event) -> bool:
        """处理鼠标事件。点击任务行返回 True 并触发回调。"""
        if not self._tracked_quests:
            return False

        if event.type == pygame.MOUSEMOTION:
            rel_x = event.pos[0] - self._x
            rel_y = event.pos[1] - self._y
            if 0 <= rel_x <= self.WIDTH:
                row_start = self.PADDING + self.TITLE_HEIGHT
                row_idx = (rel_y - row_start) // self.ROW_HEIGHT
                if 0 <= row_idx < len(self._tracked_quests):
                    self._hover_row = row_idx
                else:
                    self._hover_row = -1
            else:
                self._hover_row = -1

        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            if self._hover_row >= 0 and self._hover_row < len(self._tracked_quests):
                quest = self._tracked_quests[self._hover_row]
                if self._on_quest_click:
                    self._on_quest_click(quest.quest_id)
                return True

        return False
