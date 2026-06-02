"""任务面板 UI 模块

全屏模态面板，左侧任务列表 + 右侧任务详情。
支持分组显示（进行中 / 可接取 / 已完成折叠）、键盘导航和鼠标点击交互。
"""
import logging
from typing import Any, Callable, Dict, List, Optional, Tuple

import pygame

logger = logging.getLogger("client.ui.quest_panel")


class QuestPanel:
    """任务面板模态组件

    480x380 居中面板，左侧按状态分组的任务列表，右侧显示选中任务的详情。
    支持 Q/ESC 关闭、点击面板外关闭、上下箭头导航、折叠/展开已完成分组。
    """

    # === 布局常量 ===
    PANEL_WIDTH = 480
    PANEL_HEIGHT = 380
    PADDING = 10
    TITLE_HEIGHT = 28

    # 左侧列表区域
    LIST_WIDTH = 170
    LIST_ITEM_HEIGHT = 28
    GROUP_HEADER_HEIGHT = 24
    SCROLLBAR_WIDTH = 6

    # 右侧详情区域（自动计算）
    # DETAIL_WIDTH = PANEL_WIDTH - LIST_WIDTH - PADDING * 3

    # 按钮
    BUTTON_WIDTH = 80
    BUTTON_HEIGHT = 28
    BUTTON_GAP = 10

    # === 颜色常量 ===
    BG_COLOR = (20, 30, 20, 220)            # 面板背景（半透明深绿）
    BORDER_COLOR = (80, 120, 80)            # 面板边框
    TITLE_COLOR = (106, 176, 255)           # 标题蓝色
    GROUP_HEADER_COLOR = (180, 180, 100)    # 分组标题（暗金色）
    GROUP_HEADER_BG = (40, 50, 40, 180)     # 分组标题背景
    QUEST_NAME_COLOR = (220, 220, 220)      # 任务名白色
    QUEST_NAME_SELECTED = (255, 255, 180)   # 选中任务名亮色
    SELECTED_BG = (60, 90, 60, 160)         # 选中行背景
    HOVER_BG = (50, 70, 50, 120)            # 悬停行背景
    DESCRIPTION_COLOR = (200, 200, 200)     # 描述文字
    OBJECTIVE_DONE_COLOR = (100, 180, 100)  # 目标完成绿色
    OBJECTIVE_TODO_COLOR = (200, 200, 200)  # 目标未完成灰色
    REWARD_COLOR = (255, 215, 0)            # 奖励金色
    SEPARATOR_COLOR = (60, 80, 60)          # 分隔线

    BUTTON_COLOR = (60, 100, 60)            # 按钮背景
    BUTTON_HOVER_COLOR = (80, 130, 80)      # 按钮悬停
    BUTTON_DISABLED_COLOR = (50, 50, 50)    # 按钮禁用
    BUTTON_TEXT_COLOR = (255, 255, 255)      # 按钮文字
    BUTTON_ABANDON_COLOR = (140, 60, 60)    # 放弃按钮背景
    BUTTON_ABANDON_HOVER = (170, 80, 80)    # 放弃按钮悬停

    HINT_COLOR = (100, 100, 100)            # 底部提示文字

    # === 分组顺序 ===
    GROUP_ORDER = ["in_progress", "available", "completed"]
    GROUP_LABELS = {
        "in_progress": "进行中",
        "available": "可接取",
        "completed": "已完成",
    }

    def __init__(self, screen_width: int, screen_height: int, input_manager: Any):
        """初始化任务面板

        Args:
            screen_width: 屏幕宽度
            screen_height: 屏幕高度
            input_manager: 输入管理器，需提供 set_ui_blocking(bool) 方法
        """
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._input_manager = input_manager
        self._visible: bool = False

        # 面板位置（居中）
        self._panel_x = (screen_width - self.PANEL_WIDTH) // 2
        self._panel_y = (screen_height - self.PANEL_HEIGHT) // 2
        self._panel_rect = pygame.Rect(
            self._panel_x, self._panel_y,
            self.PANEL_WIDTH, self.PANEL_HEIGHT
        )

        # 左侧列表区域
        self._list_x = self._panel_x + self.PADDING
        self._list_y = self._panel_y + self.PADDING + self.TITLE_HEIGHT
        self._list_height = self.PANEL_HEIGHT - self.PADDING * 2 - self.TITLE_HEIGHT

        # 右侧详情区域
        self._detail_x = self._list_x + self.LIST_WIDTH + self.PADDING
        self._detail_y = self._list_y
        self._detail_width = self.PANEL_WIDTH - self.LIST_WIDTH - self.PADDING * 3
        self._detail_height = self._list_height

        # 关闭按钮
        self._close_rect = pygame.Rect(
            self._panel_x + self.PANEL_WIDTH - 28,
            self._panel_y + 4, 24, 24
        )

        # 字体初始化（SysFont 带 fallback）
        pygame.font.init()
        self._title_font = self._make_font(18, bold=True)
        self._font = self._make_font(14)
        self._small_font = self._make_font(12)
        self._hint_font = self._make_font(11)

        # 数据
        self._quest_data: Any = None  # QuestData 实例
        self._grouped_quests: Dict[str, list] = {
            "in_progress": [],
            "available": [],
            "completed": [],
        }

        # 列表视图状态
        self._flat_list: List[dict] = []  # 展平后的列表项（含分组头和任务项）
        self._selected_index: int = -1    # 选中的 flat_list 索引
        self._hover_index: int = -1       # 悬停的 flat_list 索引
        self._scroll_offset: int = 0      # 列表滚动偏移
        self._completed_expanded: bool = False  # 已完成分组是否展开

        # 选中的任务 ID（用于 select_quest 外部调用）
        self._selected_quest_id: Optional[str] = None

        # 按钮回调
        self._on_accept: Optional[Callable[[str], None]] = None
        self._on_abandon: Optional[Callable[[str], None]] = None

        # 按钮悬停状态
        self._hover_accept: bool = False
        self._hover_abandon: bool = False

        # 按钮矩形（在 draw 时计算，用于 handle_event 检测）
        self._accept_btn_rect: Optional[pygame.Rect] = None
        self._abandon_btn_rect: Optional[pygame.Rect] = None

        logger.info(f"[QuestPanel]Initialized at ({self._panel_x}, {self._panel_y})")

    # ------------------------------------------------------------------ #
    # 公开接口
    # ------------------------------------------------------------------ #

    def show(self) -> None:
        """打开任务面板，阻断游戏输入。"""
        self._visible = True
        if self._input_manager:
            self._input_manager.set_ui_blocking(True)
        self._rebuild_flat_list()
        # 自动选中第一个任务
        if self._selected_quest_id is None:
            self._select_first_quest()
        else:
            self._sync_selection_to_quest_id(self._selected_quest_id)
        logger.info("[QuestPanel]Shown")

    def hide(self) -> None:
        """关闭任务面板，恢复游戏输入。"""
        self._visible = False
        if self._input_manager:
            self._input_manager.set_ui_blocking(False)
        logger.info("[QuestPanel]Hidden")

    def toggle(self) -> None:
        """切换面板可见状态。"""
        if self._visible:
            self.hide()
        else:
            self.show()

    @property
    def is_visible(self) -> bool:
        """面板是否可见。"""
        return self._visible

    def select_quest(self, quest_id: str) -> None:
        """外部选中指定任务（供追踪条点击调用）。

        Args:
            quest_id: 要选中的任务 ID
        """
        self._selected_quest_id = quest_id
        if self._visible:
            self._sync_selection_to_quest_id(quest_id)

    def set_quest_data(self, quest_data: Any) -> None:
        """设置任务数据源。

        Args:
            quest_data: QuestData 实例，需提供 get_active_quests / get_available_quests /
                        get_completed_quests / get_quest 方法
        """
        self._quest_data = quest_data
        self._refresh_grouped_quests()
        if self._visible:
            self._rebuild_flat_list()

    def set_callbacks(self, on_accept: Optional[Callable[[str], None]],
                      on_abandon: Optional[Callable[[str], None]]) -> None:
        """设置接受/放弃任务的回调函数。

        Args:
            on_accept: 接受任务回调 callback(quest_id)
            on_abandon: 放弃任务回调 callback(quest_id)
        """
        self._on_accept = on_accept
        self._on_abandon = on_abandon

    def handle_event(self, event: pygame.event.Event) -> bool:
        """处理输入事件。

        Args:
            event: PyGame 事件

        Returns:
            True 表示事件被消费
        """
        if not self._visible:
            return False

        # --- 键盘事件 ---
        if event.type == pygame.KEYDOWN:
            if event.key in (pygame.K_q, pygame.K_ESCAPE):
                self.hide()
                return True

            if event.key == pygame.K_UP:
                self._navigate(-1)
                return True

            if event.key == pygame.K_DOWN:
                self._navigate(1)
                return True

            if event.key in (pygame.K_RETURN, pygame.K_SPACE):
                # 展开/折叠分组头 或 选中任务
                if 0 <= self._selected_index < len(self._flat_list):
                    item = self._flat_list[self._selected_index]
                    if item.get("type") == "group_header":
                        self._toggle_group(item["group"])
                        return True
                return True

        # --- 鼠标滚轮 ---
        if event.type == pygame.MOUSEWHEEL:
            mouse_pos = pygame.mouse.get_pos()
            if self._is_in_list_area(mouse_pos):
                self._scroll_offset = max(
                    0,
                    min(self._scroll_offset - event.y,
                        max(0, len(self._flat_list) - self._visible_rows()))
                )
                return True

        # --- 鼠标移动（悬停） ---
        if event.type == pygame.MOUSEMOTION:
            self._update_hover(event.pos)
            return False  # 不消费移动事件

        # --- 鼠标点击 ---
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            pos = event.pos

            # 点击面板外 → 关闭
            if not self._panel_rect.collidepoint(pos):
                self.hide()
                return True

            # 点击关闭按钮
            if self._close_rect.collidepoint(pos):
                self.hide()
                return True

            # 点击列表区域
            if self._is_in_list_area(pos):
                clicked_idx = self._get_list_index_at(pos)
                if clicked_idx >= 0:
                    item = self._flat_list[clicked_idx]
                    if item.get("type") == "group_header":
                        self._toggle_group(item["group"])
                    elif item.get("type") == "quest":
                        self._selected_index = clicked_idx
                        self._selected_quest_id = item["quest_id"]
                return True

            # 点击接受按钮
            if self._accept_btn_rect and self._accept_btn_rect.collidepoint(pos):
                quest = self._get_selected_quest()
                if quest and self._on_accept and quest.state == "available":
                    self._on_accept(quest.quest_id)
                return True

            # 点击放弃按钮
            if self._abandon_btn_rect and self._abandon_btn_rect.collidepoint(pos):
                quest = self._get_selected_quest()
                if quest and self._on_abandon and quest.state == "in_progress":
                    self._on_abandon(quest.quest_id)
                return True

            return True  # 面板内点击但未命中其他区域

        return False

    def draw(self, screen: pygame.Surface) -> None:
        """绘制任务面板到屏幕。

        Args:
            screen: 目标屏幕 Surface
        """
        if not self._visible:
            return

        # 1. 半透明遮罩
        overlay = pygame.Surface(
            (self._screen_width, self._screen_height), pygame.SRCALPHA
        )
        overlay.fill((0, 0, 0, 100))
        screen.blit(overlay, (0, 0))

        # 2. 面板背景
        panel_surf = pygame.Surface(
            (self.PANEL_WIDTH, self.PANEL_HEIGHT), pygame.SRCALPHA
        )
        panel_surf.fill(self.BG_COLOR)
        pygame.draw.rect(
            panel_surf, self.BORDER_COLOR,
            (0, 0, self.PANEL_WIDTH, self.PANEL_HEIGHT),
            width=2, border_radius=6
        )
        screen.blit(panel_surf, (self._panel_x, self._panel_y))

        # 3. 标题
        self._draw_title(screen)

        # 4. 关闭按钮
        self._draw_close_button(screen)

        # 5. 左侧列表
        self._draw_list(screen)

        # 6. 分隔线
        sep_x = self._list_x + self.LIST_WIDTH + self.PADDING // 2
        pygame.draw.line(
            screen, self.SEPARATOR_COLOR,
            (sep_x, self._list_y),
            (sep_x, self._list_y + self._list_height),
            width=1
        )

        # 7. 右侧详情
        self._draw_detail(screen)

        # 8. 底部提示
        self._draw_hint(screen)

    # ------------------------------------------------------------------ #
    # 内部方法 — 数据
    # ------------------------------------------------------------------ #

    def _refresh_grouped_quests(self) -> None:
        """从 QuestData 刷新分组数据。"""
        if self._quest_data is None:
            self._grouped_quests = {
                "in_progress": [], "available": [], "completed": [],
            }
            return
        self._grouped_quests = {
            "in_progress": list(self._quest_data.get_active_quests()),
            "available": list(self._quest_data.get_available_quests()),
            "completed": list(self._quest_data.get_completed_quests()),
        }

    def _rebuild_flat_list(self) -> None:
        """重建用于渲染的展平列表。每项是 dict: {type, ...}。"""
        self._refresh_grouped_quests()
        self._flat_list.clear()

        for group in self.GROUP_ORDER:
            quests = self._grouped_quests.get(group, [])
            # 添加分组头
            self._flat_list.append({
                "type": "group_header",
                "group": group,
                "count": len(quests),
            })
            # 已完成分组默认折叠
            if group == "completed" and not self._completed_expanded:
                continue
            # 添加任务项
            for quest in quests:
                self._flat_list.append({
                    "type": "quest",
                    "quest_id": quest.quest_id,
                    "name": quest.name,
                })

        # 滚动修正
        max_scroll = max(0, len(self._flat_list) - self._visible_rows())
        self._scroll_offset = min(self._scroll_offset, max_scroll)

    def _select_first_quest(self) -> None:
        """选中列表中第一个非分组头项。"""
        for i, item in enumerate(self._flat_list):
            if item.get("type") == "quest":
                self._selected_index = i
                self._selected_quest_id = item["quest_id"]
                return
        self._selected_index = -1
        self._selected_quest_id = None

    def _sync_selection_to_quest_id(self, quest_id: str) -> None:
        """根据 quest_id 同步选中索引。"""
        for i, item in enumerate(self._flat_list):
            if item.get("type") == "quest" and item.get("quest_id") == quest_id:
                self._selected_index = i
                self._selected_quest_id = quest_id
                # 确保可见
                visible_start = self._scroll_offset
                visible_end = self._scroll_offset + self._visible_rows()
                if i < visible_start:
                    self._scroll_offset = i
                elif i >= visible_end:
                    self._scroll_offset = i - self._visible_rows() + 1
                return
        # 未找到则选中第一个
        self._select_first_quest()

    def _get_selected_quest(self) -> Optional[Any]:
        """获取当前选中的 QuestInfo 对象。"""
        if not self._selected_quest_id or self._quest_data is None:
            return None
        return self._quest_data.get_quest(self._selected_quest_id)

    def _navigate(self, direction: int) -> None:
        """上下导航。direction: -1=上, 1=下。"""
        if not self._flat_list:
            return

        new_idx = self._selected_index + direction
        # 跳过分组头（除非方向反转无更多项）
        while 0 <= new_idx < len(self._flat_list):
            if self._flat_list[new_idx].get("type") == "quest":
                break
            new_idx += direction

        if 0 <= new_idx < len(self._flat_list):
            self._selected_index = new_idx
            item = self._flat_list[new_idx]
            if item.get("type") == "quest":
                self._selected_quest_id = item["quest_id"]
            # 滚动到可见区域
            visible_start = self._scroll_offset
            visible_end = self._scroll_offset + self._visible_rows()
            if new_idx < visible_start:
                self._scroll_offset = new_idx
            elif new_idx >= visible_end:
                self._scroll_offset = new_idx - self._visible_rows() + 1

    def _toggle_group(self, group: str) -> None:
        """展开/折叠指定分组。"""
        if group == "completed":
            self._completed_expanded = not self._completed_expanded
            self._rebuild_flat_list()

    def _visible_rows(self) -> int:
        """列表区域可见行数。"""
        return self._list_height // self.LIST_ITEM_HEIGHT

    # ------------------------------------------------------------------ #
    # 内部方法 — 事件辅助
    # ------------------------------------------------------------------ #

    def _is_in_list_area(self, pos: Tuple[int, int]) -> bool:
        """鼠标是否在列表区域内。"""
        x, y = pos
        return (self._list_x <= x <= self._list_x + self.LIST_WIDTH and
                self._list_y <= y <= self._list_y + self._list_height)

    def _get_list_index_at(self, pos: Tuple[int, int]) -> int:
        """根据鼠标位置获取 flat_list 索引，返回 -1 表示未命中。"""
        x, y = pos
        rel_y = y - self._list_y
        row = rel_y // self.LIST_ITEM_HEIGHT + self._scroll_offset
        if 0 <= row < len(self._flat_list):
            return row
        return -1

    def _update_hover(self, pos: Tuple[int, int]) -> None:
        """更新悬停状态。"""
        # 列表悬停
        if self._is_in_list_area(pos):
            self._hover_index = self._get_list_index_at(pos)
        else:
            self._hover_index = -1

        # 按钮悬停
        self._hover_accept = (
            self._accept_btn_rect is not None and
            self._accept_btn_rect.collidepoint(pos)
        )
        self._hover_abandon = (
            self._abandon_btn_rect is not None and
            self._abandon_btn_rect.collidepoint(pos)
        )

    # ------------------------------------------------------------------ #
    # 内部方法 — 绘制辅助
    # ------------------------------------------------------------------ #

    @staticmethod
    def _make_font(size: int, bold: bool = False) -> pygame.font.Font:
        """创建字体，优先使用 SysFont 微软雅黑，失败则用默认字体。"""
        try:
            font = pygame.font.SysFont("microsoftyahei", size, bold=bold)
            if font is not None:
                return font
        except Exception:
            pass
        return pygame.font.Font(None, size)

    def _draw_title(self, screen: pygame.Surface) -> None:
        """绘制面板标题。"""
        title_surf = self._title_font.render("任务手册", True, self.TITLE_COLOR)
        screen.blit(title_surf, (self._panel_x + self.PADDING, self._panel_y + 4))

    def _draw_close_button(self, screen: pygame.Surface) -> None:
        """绘制关闭按钮 [x]。"""
        mouse_pos = pygame.mouse.get_pos()
        is_hover = self._close_rect.collidepoint(mouse_pos)
        color = (255, 100, 100) if is_hover else (200, 80, 80)
        pygame.draw.rect(screen, color, self._close_rect, border_radius=4)
        x_surf = self._small_font.render("x", True, (255, 255, 255))
        tx = self._close_rect.x + (self._close_rect.width - x_surf.get_width()) // 2
        ty = self._close_rect.y + (self._close_rect.height - x_surf.get_height()) // 2
        screen.blit(x_surf, (tx, ty))

    def _draw_list(self, screen: pygame.Surface) -> None:
        """绘制左侧任务列表。"""
        # 列表背景
        list_bg = pygame.Surface(
            (self.LIST_WIDTH, self._list_height), pygame.SRCALPHA
        )
        list_bg.fill((10, 15, 10, 120))
        screen.blit(list_bg, (self._list_x, self._list_y))

        visible_rows = self._visible_rows()
        end_idx = min(self._scroll_offset + visible_rows, len(self._flat_list))

        for i in range(self._scroll_offset, end_idx):
            item = self._flat_list[i]
            row_y = self._list_y + (i - self._scroll_offset) * self.LIST_ITEM_HEIGHT
            row_rect = pygame.Rect(
                self._list_x, row_y, self.LIST_WIDTH, self.LIST_ITEM_HEIGHT
            )

            if item.get("type") == "group_header":
                self._draw_group_header(screen, item, row_rect)
            elif item.get("type") == "quest":
                self._draw_quest_row(screen, item, row_rect, i)

        # 简单滚动条指示器
        if len(self._flat_list) > visible_rows:
            bar_height = max(
                20,
                int(self._list_height * visible_rows / len(self._flat_list))
            )
            bar_y = self._list_y + int(
                self._list_height * self._scroll_offset / len(self._flat_list)
            )
            pygame.draw.rect(
                screen, (100, 100, 100, 100),
                (self._list_x + self.LIST_WIDTH - self.SCROLLBAR_WIDTH,
                 bar_y, self.SCROLLBAR_WIDTH, bar_height),
                border_radius=3
            )

    def _draw_group_header(self, screen: pygame.Surface, item: dict,
                           rect: pygame.Rect) -> None:
        """绘制分组标题行。"""
        # 背景
        header_bg = pygame.Surface(
            (rect.width, rect.height), pygame.SRCALPHA
        )
        header_bg.fill(self.GROUP_HEADER_BG)
        screen.blit(header_bg, rect.topleft)

        group = item["group"]
        count = item["count"]
        label = self.GROUP_LABELS.get(group, group)

        # 展开/折叠指示
        if group == "completed":
            arrow = "▼" if self._completed_expanded else "▶"
        else:
            arrow = "▼"

        text = f"{arrow} {label} ({count})"
        text_surf = self._small_font.render(text, True, self.GROUP_HEADER_COLOR)
        screen.blit(text_surf, (rect.x + 6, rect.y + (rect.height - text_surf.get_height()) // 2))

    def _draw_quest_row(self, screen: pygame.Surface, item: dict,
                        rect: pygame.Rect, flat_idx: int) -> None:
        """绘制任务行。"""
        is_selected = (flat_idx == self._selected_index)
        is_hover = (flat_idx == self._hover_index and not is_selected)

        # 背景高亮
        if is_selected:
            sel_surf = pygame.Surface(
                (rect.width, rect.height), pygame.SRCALPHA
            )
            sel_surf.fill(self.SELECTED_BG)
            screen.blit(sel_surf, rect.topleft)
            pygame.draw.rect(
                screen, self.BORDER_COLOR, rect, width=1, border_radius=2
            )
        elif is_hover:
            hover_surf = pygame.Surface(
                (rect.width, rect.height), pygame.SRCALPHA
            )
            hover_surf.fill(self.HOVER_BG)
            screen.blit(hover_surf, rect.topleft)

        # 任务名
        name_color = self.QUEST_NAME_SELECTED if is_selected else self.QUEST_NAME_COLOR
        name_text = item.get("name", "")
        # 截断过长名称
        max_chars = (self.LIST_WIDTH - 16) // max(1, self._font.size("测")[0])
        if len(name_text) > max_chars:
            name_text = name_text[:max_chars - 1] + "…"
        name_surf = self._font.render(name_text, True, name_color)
        name_y = rect.y + (rect.height - name_surf.get_height()) // 2
        screen.blit(name_surf, (rect.x + 8, name_y))

    def _draw_detail(self, screen: pygame.Surface) -> None:
        """绘制右侧任务详情。"""
        quest = self._get_selected_quest()

        if quest is None:
            hint = self._font.render("选择一个任务查看详情", True, self.HINT_COLOR)
            screen.blit(
                hint,
                (self._detail_x + self._detail_width // 2 - hint.get_width() // 2,
                 self._detail_y + 40)
            )
            return

        x = self._detail_x
        y = self._detail_y
        w = self._detail_width

        # --- 任务名称 ---
        name_surf = self._title_font.render(quest.name, True, self.TITLE_COLOR)
        screen.blit(name_surf, (x, y))
        y += self.TITLE_HEIGHT + 4

        # 状态标签
        state_label, state_color = self._state_badge(quest.state)
        badge_surf = self._small_font.render(state_label, True, state_color)
        screen.blit(badge_surf, (x, y))
        y += 18

        # 分隔线
        pygame.draw.line(screen, self.SEPARATOR_COLOR, (x, y), (x + w, y), width=1)
        y += 6

        # --- 任务描述 ---
        desc_lines = self._wrap_text(quest.description, w, self._small_font)
        for line in desc_lines:
            line_surf = self._small_font.render(line, True, self.DESCRIPTION_COLOR)
            screen.blit(line_surf, (x, y))
            y += 16
        y += 4

        # --- 任务目标 ---
        obj_title = self._font.render("目标：", True, self.DESCRIPTION_COLOR)
        screen.blit(obj_title, (x, y))
        y += 20

        for obj in quest.objectives:
            done = obj.current >= obj.required
            check = "✔" if done else "□"
            color = self.OBJECTIVE_DONE_COLOR if done else self.OBJECTIVE_TODO_COLOR
            progress_text = f"{check} {obj.description}  ({obj.current}/{obj.required})"
            obj_surf = self._small_font.render(progress_text, True, color)
            screen.blit(obj_surf, (x + 8, y))
            y += 16

        y += 4

        # --- 奖励 ---
        rewards = quest.rewards
        reward_parts = []
        if rewards.gold > 0:
            reward_parts.append(f"金币 +{rewards.gold}")
        if rewards.exp > 0:
            reward_parts.append(f"经验 +{rewards.exp}")
        for item_id, count in rewards.items:
            reward_parts.append(f"物品 {item_id} x{count}")

        if reward_parts:
            reward_title = self._font.render("奖励：", True, self.REWARD_COLOR)
            screen.blit(reward_title, (x, y))
            y += 20
            for part in reward_parts:
                part_surf = self._small_font.render(part, True, self.REWARD_COLOR)
                screen.blit(part_surf, (x + 8, y))
                y += 16

        # --- 操作按钮 ---
        self._accept_btn_rect = None
        self._abandon_btn_rect = None

        btn_y = self._detail_y + self._detail_height - self.BUTTON_HEIGHT - 4

        if quest.state == "available" and self._on_accept:
            self._accept_btn_rect = pygame.Rect(
                x, btn_y, self.BUTTON_WIDTH, self.BUTTON_HEIGHT
            )
            btn_color = (
                self.BUTTON_HOVER_COLOR if self._hover_accept
                else self.BUTTON_COLOR
            )
            pygame.draw.rect(screen, btn_color, self._accept_btn_rect, border_radius=4)
            pygame.draw.rect(
                screen, self.BORDER_COLOR, self._accept_btn_rect,
                width=1, border_radius=4
            )
            btn_text = self._font.render("接取", True, self.BUTTON_TEXT_COLOR)
            bx = self._accept_btn_rect.x + (self.BUTTON_WIDTH - btn_text.get_width()) // 2
            by = self._accept_btn_rect.y + (self.BUTTON_HEIGHT - btn_text.get_height()) // 2
            screen.blit(btn_text, (bx, by))

        if quest.state == "in_progress" and self._on_abandon:
            self._abandon_btn_rect = pygame.Rect(
                x, btn_y, self.BUTTON_WIDTH, self.BUTTON_HEIGHT
            )
            btn_color = (
                self.BUTTON_ABANDON_HOVER if self._hover_abandon
                else self.BUTTON_ABANDON_COLOR
            )
            pygame.draw.rect(screen, btn_color, self._abandon_btn_rect, border_radius=4)
            pygame.draw.rect(
                screen, self.BORDER_COLOR, self._abandon_btn_rect,
                width=1, border_radius=4
            )
            btn_text = self._font.render("放弃", True, self.BUTTON_TEXT_COLOR)
            bx = self._abandon_btn_rect.x + (self.BUTTON_WIDTH - btn_text.get_width()) // 2
            by = self._abandon_btn_rect.y + (self.BUTTON_HEIGHT - btn_text.get_height()) // 2
            screen.blit(btn_text, (bx, by))

    def _draw_hint(self, screen: pygame.Surface) -> None:
        """绘制底部操作提示。"""
        hint_text = "Q/ESC 关闭  |  ↑↓ 导航  |  点击选择"
        hint_surf = self._hint_font.render(hint_text, True, self.HINT_COLOR)
        hx = self._panel_x + (self.PANEL_WIDTH - hint_surf.get_width()) // 2
        hy = self._panel_y + self.PANEL_HEIGHT - 18
        screen.blit(hint_surf, (hx, hy))

    # ------------------------------------------------------------------ #
    # 工具方法
    # ------------------------------------------------------------------ #

    @staticmethod
    def _state_badge(state: str) -> Tuple[str, Tuple[int, int, int]]:
        """返回 (标签文字, 颜色) 用于状态标签。"""
        if state == "in_progress":
            return "[ 进行中 ]", (100, 200, 100)
        if state == "available":
            return "[ 可接取 ]", (100, 180, 255)
        if state == "completed":
            return "[ 已完成 ]", (180, 180, 180)
        return f"[ {state} ]", (200, 200, 200)

    @staticmethod
    def _wrap_text(text: str, max_width: int, font: pygame.font.Font) -> List[str]:
        """自动换行。按字符宽度切分（CJK 按字切分）。"""
        if not text:
            return [""]

        lines: List[str] = []
        current = ""
        for char in text:
            if char == "\n":
                lines.append(current)
                current = ""
                continue
            test = current + char
            if font.size(test)[0] > max_width:
                if current:
                    lines.append(current)
                current = char
            else:
                current = test
        if current:
            lines.append(current)
        return lines if lines else [""]

    def cleanup(self) -> None:
        """清理资源。"""
        logger.info("[QuestPanel]Cleaned up")
