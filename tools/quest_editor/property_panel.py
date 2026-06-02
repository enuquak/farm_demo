"""Property editing panel for the quest editor."""

from __future__ import annotations

import tkinter as tk
from tkinter import ttk
from typing import Callable, Dict, List, Optional

from quest_node import ItemReward, ObjectiveDef, QuestNode


# ---------------------------------------------------------------------------
# PropertyPanel
# ---------------------------------------------------------------------------

class PropertyPanel(ttk.LabelFrame):
    """Side panel that displays and edits properties of the selected quest."""

    QUEST_TYPES: List[str] = [
        "main",
        "side",
        "daily",
        "repeatable",
        "event",
        "hidden",
    ]

    TRIGGER_TYPES: List[str] = [
        "auto",
        "npc_talk",
        "item_pickup",
        "area_enter",
        "level_up",
        "quest_complete",
    ]

    OBJECTIVE_TYPES: List[str] = [
        "collect_item",
        "kill_monster",
        "talk_to_npc",
        "reach_area",
        "escort",
        "use_item",
    ]

    def __init__(
        self,
        master: tk.Widget,
        on_apply: Optional[Callable[[str, QuestNode], None]] = None,
        **kwargs,
    ) -> None:
        kwargs.setdefault("text", "任务属性")  # 任务属性
        super().__init__(master, **kwargs)

        self._on_apply = on_apply
        self._current_quest_id: Optional[str] = None
        self._current_node: Optional[QuestNode] = None

        # -- scrollable interior -------------------------------------------
        self._outer_frame = ttk.Frame(self)
        self._outer_frame.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        self._canvas = tk.Canvas(self._outer_frame, highlightthickness=0)
        self._scrollbar = ttk.Scrollbar(
            self._outer_frame, orient=tk.VERTICAL, command=self._canvas.yview
        )
        self._inner = ttk.Frame(self._canvas)

        self._inner.bind(
            "<Configure>",
            lambda e: self._canvas.configure(
                scrollregion=self._canvas.bbox("all")
            ),
        )
        self._canvas.create_window((0, 0), window=self._inner, anchor="nw")
        self._canvas.configure(yscrollcommand=self._scrollbar.set)

        self._canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self._scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        # -- build fields --------------------------------------------------
        self._build_fields()

        # -- apply button --------------------------------------------------
        self._apply_btn = ttk.Button(
            self._inner, text="应用更改", command=self._apply_changes  # 应用更改
        )
        self._apply_btn.pack(fill=tk.X, pady=(8, 4))

        # Start disabled until a quest is loaded
        self._set_enabled(False)

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def load_quest(self, quest_id: str, node: QuestNode) -> None:
        """Populate the panel with the given quest's data."""
        self._current_quest_id = quest_id
        self._current_node = node
        self._set_enabled(True)

        # Basic fields
        self._var_id.set(node.id)
        self._var_name.set(node.name)
        self._text_desc.delete("1.0", tk.END)
        self._text_desc.insert("1.0", node.description)
        self._var_type.set(node.type)
        self._var_trigger_type.set(node.trigger.type)
        self._var_trigger_cond.set(node.trigger.condition)
        self._var_trigger_npc.set(node.trigger.npc_id)
        self._var_prereqs.set(", ".join(node.prerequisites))
        self._var_branch.set(node.branch_group)

        # Objectives
        self._refresh_objectives(node.objectives)

        # Rewards
        self._refresh_rewards(node.rewards)

    # ------------------------------------------------------------------
    # Field construction
    # ------------------------------------------------------------------

    def _build_fields(self) -> None:
        parent = self._inner

        # Row 0 -- ID
        ttk.Label(parent, text="ID:").pack(anchor=tk.W, pady=(4, 0))
        self._var_id = tk.StringVar()
        self._entry_id = ttk.Entry(parent, textvariable=self._var_id, state="readonly")
        self._entry_id.pack(fill=tk.X, pady=(0, 4))

        # Row 1 -- Name
        ttk.Label(parent, text="名称:").pack(anchor=tk.W, pady=(4, 0))  # 名称
        self._var_name = tk.StringVar()
        self._entry_name = ttk.Entry(parent, textvariable=self._var_name)
        self._entry_name.pack(fill=tk.X, pady=(0, 4))

        # Row 2 -- Description
        ttk.Label(parent, text="描述:").pack(anchor=tk.W, pady=(4, 0))  # 描述
        self._text_desc = tk.Text(parent, height=4, wrap=tk.WORD)
        self._text_desc.pack(fill=tk.X, pady=(0, 4))

        # Row 3 -- Type
        ttk.Label(parent, text="类型:").pack(anchor=tk.W, pady=(4, 0))  # 类型
        self._var_type = tk.StringVar()
        self._combo_type = ttk.Combobox(
            parent, textvariable=self._var_type, values=self.QUEST_TYPES, state="readonly"
        )
        self._combo_type.pack(fill=tk.X, pady=(0, 4))

        # Row 4 -- Trigger type
        ttk.Label(parent, text="触发类型:").pack(anchor=tk.W, pady=(4, 0))  # 触发类型
        self._var_trigger_type = tk.StringVar()
        self._combo_trigger = ttk.Combobox(
            parent, textvariable=self._var_trigger_type, values=self.TRIGGER_TYPES, state="readonly"
        )
        self._combo_trigger.pack(fill=tk.X, pady=(0, 4))

        # Row 5 -- Trigger condition
        ttk.Label(parent, text="触发条件:").pack(anchor=tk.W, pady=(4, 0))  # 触发条件
        self._var_trigger_cond = tk.StringVar()
        self._entry_trigger_cond = ttk.Entry(parent, textvariable=self._var_trigger_cond)
        self._entry_trigger_cond.pack(fill=tk.X, pady=(0, 4))

        # Row 6 -- Trigger NPC
        ttk.Label(parent, text="触发NPC:").pack(anchor=tk.W, pady=(4, 0))  # 触发NPC
        self._var_trigger_npc = tk.StringVar()
        self._entry_trigger_npc = ttk.Entry(parent, textvariable=self._var_trigger_npc)
        self._entry_trigger_npc.pack(fill=tk.X, pady=(0, 4))

        # Row 7 -- Objectives
        ttk.Label(parent, text="任务目标:").pack(anchor=tk.W, pady=(4, 0))  # 任务目标
        self._obj_frame = ttk.LabelFrame(parent, text="目标列表")  # 目标列表
        self._obj_frame.pack(fill=tk.X, pady=(0, 4))
        self._obj_widgets: List[Dict[str, tk.Widget]] = []

        # Row 8 -- Rewards
        ttk.Label(parent, text="奖励:").pack(anchor=tk.W, pady=(4, 0))  # 奖励
        self._reward_frame = ttk.LabelFrame(parent, text="奖励列表")  # 奖励列表
        self._reward_frame.pack(fill=tk.X, pady=(0, 4))

        # Rewards -- gold / exp
        r_inner = ttk.Frame(self._reward_frame)
        r_inner.pack(fill=tk.X, padx=4, pady=2)
        ttk.Label(r_inner, text="金币:").pack(side=tk.LEFT)  # 金币
        self._var_gold = tk.IntVar(value=0)
        ttk.Entry(r_inner, textvariable=self._var_gold, width=8).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Label(r_inner, text="经验:").pack(side=tk.LEFT)  # 经验
        self._var_exp = tk.IntVar(value=0)
        ttk.Entry(r_inner, textvariable=self._var_exp, width=8).pack(side=tk.LEFT)

        # Rewards -- items text area
        ttk.Label(self._reward_frame, text="物品 (JSON数组):").pack(anchor=tk.W, padx=4)  # 物品 (JSON数组)
        self._text_reward_items = tk.Text(self._reward_frame, height=3, wrap=tk.WORD)
        self._text_reward_items.pack(fill=tk.X, padx=4, pady=(0, 4))

        # Row 9 -- Prerequisites
        ttk.Label(parent, text="前置任务 (逗号分隔):").pack(anchor=tk.W, pady=(4, 0))  # 前置任务 (逗号分隔)
        self._var_prereqs = tk.StringVar()
        self._entry_prereqs = ttk.Entry(parent, textvariable=self._var_prereqs)
        self._entry_prereqs.pack(fill=tk.X, pady=(0, 4))

        # Row 10 -- Branch group
        ttk.Label(parent, text="分支组:").pack(anchor=tk.W, pady=(4, 0))  # 分支组
        self._var_branch = tk.StringVar()
        self._entry_branch = ttk.Entry(parent, textvariable=self._var_branch)
        self._entry_branch.pack(fill=tk.X, pady=(0, 4))

    # ------------------------------------------------------------------
    # Objectives helpers
    # ------------------------------------------------------------------

    def _refresh_objectives(self, objectives: List[ObjectiveDef]) -> None:
        """Rebuild the objectives sub-panel."""
        for winfo in self._obj_frame.winfo_children():
            winfo.destroy()
        self._obj_widgets.clear()

        for idx, obj in enumerate(objectives):
            row = ttk.Frame(self._obj_frame)
            row.pack(fill=tk.X, padx=4, pady=2)

            ttk.Label(row, text=f"#{idx + 1}").pack(side=tk.LEFT)

            var_type = tk.StringVar(value=obj.type)
            ttk.Combobox(
                row,
                textvariable=var_type,
                values=self.OBJECTIVE_TYPES,
                width=14,
                state="readonly",
            ).pack(side=tk.LEFT, padx=(4, 0))

            var_target = tk.StringVar(value=obj.target)
            ttk.Entry(row, textvariable=var_target, width=12).pack(side=tk.LEFT, padx=(4, 0))

            var_count = tk.IntVar(value=obj.count)
            ttk.Entry(row, textvariable=var_count, width=5).pack(side=tk.LEFT, padx=(4, 0))

            var_desc = tk.StringVar(value=obj.description)
            ttk.Entry(row, textvariable=var_desc, width=16).pack(side=tk.LEFT, padx=(4, 0))

            self._obj_widgets.append(
                {
                    "type": var_type,
                    "target": var_target,
                    "count": var_count,
                    "desc": var_desc,
                }
            )

    def _collect_objectives(self) -> List[ObjectiveDef]:
        objectives: List[ObjectiveDef] = []
        for idx, widgets in enumerate(self._obj_widgets):
            objectives.append(
                ObjectiveDef(
                    id=f"obj_{idx}",
                    type=widgets["type"].get(),
                    target=widgets["target"].get(),
                    count=int(widgets["count"].get()),
                    description=widgets["desc"].get(),
                )
            )
        return objectives

    # ------------------------------------------------------------------
    # Rewards helpers
    # ------------------------------------------------------------------

    def _refresh_rewards(self, rewards) -> None:
        self._var_gold.set(rewards.gold)
        self._var_exp.set(rewards.exp)
        self._text_reward_items.delete("1.0", tk.END)
        import json

        items_data = [item.to_dict() for item in rewards.items]
        self._text_reward_items.insert("1.0", json.dumps(items_data, ensure_ascii=False))

    def _collect_rewards(self):
        from quest_node import QuestRewardDef

        import json

        items_raw = self._text_reward_items.get("1.0", tk.END).strip()
        items: List[ItemReward] = []
        if items_raw:
            try:
                parsed = json.loads(items_raw)
                if isinstance(parsed, list):
                    items = [ItemReward.from_dict(d) for d in parsed]
            except json.JSONDecodeError:
                pass

        return QuestRewardDef(
            items=items,
            gold=int(self._var_gold.get()),
            exp=int(self._var_exp.get()),
            unlock_quests=[],  # unlock_quests managed via connections
        )

    # ------------------------------------------------------------------
    # Apply changes
    # ------------------------------------------------------------------

    def _apply_changes(self) -> None:
        if self._current_node is None or self._current_quest_id is None:
            return

        node = self._current_node

        # Basic fields (id stays read-only)
        node.name = self._var_name.get()
        node.description = self._text_desc.get("1.0", tk.END).strip()
        node.type = self._var_type.get()

        # Trigger
        node.trigger.type = self._var_trigger_type.get()
        node.trigger.condition = self._var_trigger_cond.get()
        node.trigger.npc_id = self._var_trigger_npc.get()

        # Objectives
        node.objectives = self._collect_objectives()

        # Rewards
        node.rewards = self._collect_rewards()

        # Prerequisites
        prereq_text = self._var_prereqs.get().strip()
        if prereq_text:
            node.prerequisites = [p.strip() for p in prereq_text.split(",") if p.strip()]
        else:
            node.prerequisites = []

        # Branch group
        node.branch_group = self._var_branch.get().strip()

        # Notify external handler
        if self._on_apply is not None:
            self._on_apply(self._current_quest_id, node)

    # ------------------------------------------------------------------
    # Enable / disable
    # ------------------------------------------------------------------

    def _set_enabled(self, enabled: bool) -> None:
        """Enable or disable all editable fields."""
        state = "normal" if enabled else "disabled"
        combo_state = "readonly" if enabled else "disabled"

        self._entry_name.configure(state=state)
        self._text_desc.configure(state=state)
        self._combo_type.configure(state=combo_state)
        self._combo_trigger.configure(state=combo_state)
        self._entry_trigger_cond.configure(state=state)
        self._entry_trigger_npc.configure(state=state)
        self._entry_prereqs.configure(state=state)
        self._entry_branch.configure(state=state)
        self._text_reward_items.configure(state=state)
        self._apply_btn.configure(state=state)

        # Gold / exp entries
        for child in self._reward_frame.winfo_children():
            if isinstance(child, ttk.Frame):
                for widget in child.winfo_children():
                    if isinstance(widget, ttk.Entry):
                        widget.configure(state=state)
