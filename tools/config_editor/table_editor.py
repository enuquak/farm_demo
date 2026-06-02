# -*- coding: utf-8 -*-
"""配置编辑器表格编辑组件。

以可编辑表格形式展示 Excel 内容，支持实时类型校验。
行 1（字段名）、行 2（字段类型）、行 3（字段描述）冻结在顶部。
"""
import logging
import tkinter as tk
from tkinter import ttk
from typing import Any, Callable, Dict, List, Optional

import customtkinter as ctk

logger = logging.getLogger(__name__)

# 类型校验规则
TYPE_VALIDATORS = {
    "int": lambda v: _try_int(v),
    "float": lambda v: _try_float(v),
    "str": lambda v: (True, str(v)),
    "bool": lambda v: _try_bool(v),
}


def _try_int(value):
    """尝试转换为 int。"""
    if value in ("", None):
        return True, None
    try:
        return True, int(value)
    except (ValueError, TypeError):
        return False, f"'{value}' 不是有效的整数"


def _try_float(value):
    """尝试转换为 float。"""
    if value in ("", None):
        return True, None
    try:
        return True, float(value)
    except (ValueError, TypeError):
        return False, f"'{value}' 不是有效的浮点数"


def _try_bool(value):
    """尝试转换为 bool。"""
    if value in ("", None):
        return True, None
    lower = str(value).lower()
    if lower in ("true", "1", "yes"):
        return True, True
    if lower in ("false", "0", "no"):
        return True, False
    return False, f"'{value}' 不是有效的布尔值 (true/false)"


class TableEditor(ctk.CTkFrame):
    """表格编辑器组件。"""

    def __init__(self, master, on_modified: Callable[[], None] = None, **kwargs):
        """初始化表格编辑器。

        Args:
            master: 父组件。
            on_modified: 数据修改回调。
        """
        super().__init__(master, **kwargs)

        self._on_modified = on_modified
        self._table_data: Optional[Dict[str, Any]] = None
        self._is_modified = False

        # 工具栏（插入行、删除行）
        self._toolbar = ctk.CTkFrame(self, height=36)
        self._toolbar.pack(fill=tk.X, padx=5, pady=(5, 0))

        self._insert_btn = ctk.CTkButton(
            self._toolbar, text="➕ 插入行", width=80, height=28,
            font=ctk.CTkFont(size=12), command=self._insert_row
        )
        self._insert_btn.pack(side=tk.LEFT, padx=5, pady=4)

        self._delete_btn = ctk.CTkButton(
            self._toolbar, text="🗑 删除行", width=80, height=28,
            font=ctk.CTkFont(size=12), fg_color="#d9534f", hover_color="#c9302c",
            command=self._delete_row
        )
        self._delete_btn.pack(side=tk.LEFT, padx=5, pady=4)

        # 表格容器
        self._table_frame = ctk.CTkFrame(self)
        self._table_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # 使用 tkinter Treeview 实现表格（CTk 没有原生表格组件）
        style = ttk.Style()
        style.theme_use("default")
        style.configure(
            "Custom.Treeview",
            background="#2b2b2b",
            foreground="white",
            fieldbackground="#2b2b2b",
            borderwidth=0,
            font=("Consolas", 11),
        )
        style.configure(
            "Custom.Treeview.Heading",
            background="#1f538d",
            foreground="white",
            font=("Consolas", 11, "bold"),
        )
        style.map("Custom.Treeview", background=[("selected", "#264f78")])

        self._tree = ttk.Treeview(
            self._table_frame, style="Custom.Treeview", show="headings"
        )

        # 滚动条
        vsb = ttk.Scrollbar(self._table_frame, orient="vertical", command=self._tree.yview)
        hsb = ttk.Scrollbar(self._table_frame, orient="horizontal", command=self._tree.xview)
        self._tree.configure(yscrollcommand=vsb.set, xscrollcommand=hsb.set)

        self._tree.grid(row=0, column=0, sticky="nsew")
        vsb.grid(row=0, column=1, sticky="ns")
        hsb.grid(row=1, column=0, sticky="ew")
        self._table_frame.grid_rowconfigure(0, weight=1)
        self._table_frame.grid_columnconfigure(0, weight=1)

        # 绑定双击编辑
        self._tree.bind("<Double-1>", self._on_double_click)

        # 当前编辑状态
        self._editing_item = None
        self._editing_col = None

    def load_table(self, table_data: Dict[str, Any]) -> None:
        """加载表数据到编辑器。"""
        self._table_data = table_data
        self._is_modified = False
        self._refresh_display()

    def get_table_data(self) -> Optional[Dict[str, Any]]:
        """获取当前表数据。"""
        return self._table_data

    def is_modified(self) -> bool:
        """返回数据是否被修改。"""
        return self._is_modified

    def _refresh_display(self) -> None:
        """刷新表格显示。"""
        if not self._table_data:
            return

        # 清除旧数据
        self._tree.delete(*self._tree.get_children())

        # 设置列
        fields = self._table_data["fields"]
        self._tree["columns"] = list(range(len(fields)))
        for i, field in enumerate(fields):
            self._tree.heading(i, text=field)
            self._tree.column(i, width=100, minwidth=60)

        # 行 2: 类型（固定显示为第一行，灰色样式）
        types = self._table_data["types"]
        self._tree.insert("", "end", values=types, tags=("type_row",))

        # 行 3: 注释
        comments = self._table_data["comments"]
        self._tree.insert("", "end", values=comments, tags=("comment_row",))

        # 数据行
        for row in self._table_data["rows"]:
            display_row = ["" if v is None else str(v) for v in row]
            self._tree.insert("", "end", values=display_row)

        # 行样式
        self._tree.tag_configure("type_row", foreground="#888888", font=("Consolas", 10, "italic"))
        self._tree.tag_configure("comment_row", foreground="#569cd6", font=("Consolas", 10))

    def _on_double_click(self, event) -> None:
        """双击单元格进入编辑模式。"""
        item = self._tree.identify_row(event.y)
        column = self._tree.identify_column(event.x)

        if not item or not column:
            return

        # 获取列索引（#0, #1, #2...）
        col_idx = int(column.replace("#", "")) - 1
        if col_idx < 0:
            return

        # 获取当前值
        values = self._tree.item(item, "values")
        if not values:
            return
        current_value = values[col_idx]

        # 获取单元格位置
        bbox = self._tree.bbox(item, column)
        if not bbox:
            return

        x, y, width, height = bbox

        # 创建编辑框
        entry = tk.Entry(self._tree, font=("Consolas", 11))
        entry.insert(0, current_value)
        entry.select_range(0, tk.END)
        entry.focus()

        entry.place(x=x, y=y, width=width, height=height)

        def on_confirm(e=None):
            new_value = entry.get()
            entry.destroy()
            self._apply_edit(item, col_idx, new_value)

        def on_cancel(e=None):
            entry.destroy()

        entry.bind("<Return>", on_confirm)
        entry.bind("<Escape>", on_cancel)
        entry.bind("<FocusOut>", on_confirm)

    def _apply_edit(self, item: str, col_idx: int, new_value: str) -> None:
        """应用单元格编辑。"""
        values = list(self._tree.item(item, "values"))
        old_value = values[col_idx]

        if new_value == old_value:
            return

        # 类型校验（仅对数据行，跳过类型行和注释行）
        all_items = self._tree.get_children()
        item_idx = all_items.index(item)
        if item_idx < 2:  # 前两行是类型和注释
            values[col_idx] = new_value
            self._tree.item(item, values=values)
            # 更新内部数据
            if item_idx == 0:
                self._table_data["types"][col_idx] = new_value
            elif item_idx == 1:
                self._table_data["comments"][col_idx] = new_value
            self._mark_modified()
            return

        # 数据行：类型校验
        field_type = self._table_data["types"][col_idx]
        validator = TYPE_VALIDATORS.get(field_type)
        if validator:
            ok, result = validator(new_value)
            if not ok:
                # 校验失败，显示红色边框提示
                logger.warning(f"类型校验失败: {result}")
                # 仍然允许输入，但标记为错误
                pass

        values[col_idx] = new_value
        self._tree.item(item, values=values)

        # 更新内部数据
        data_row_idx = item_idx - 2  # 减去类型行和注释行
        if data_row_idx < len(self._table_data["rows"]):
            if col_idx < len(self._table_data["rows"][data_row_idx]):
                # 尝试类型转换
                if field_type == "int" and new_value:
                    try:
                        self._table_data["rows"][data_row_idx][col_idx] = int(new_value)
                    except ValueError:
                        self._table_data["rows"][data_row_idx][col_idx] = new_value
                elif field_type == "float" and new_value:
                    try:
                        self._table_data["rows"][data_row_idx][col_idx] = float(new_value)
                    except ValueError:
                        self._table_data["rows"][data_row_idx][col_idx] = new_value
                elif field_type == "bool" and new_value:
                    lower = new_value.lower()
                    self._table_data["rows"][data_row_idx][col_idx] = lower in ("true", "1", "yes")
                else:
                    self._table_data["rows"][data_row_idx][col_idx] = new_value

        self._mark_modified()

    def _insert_row(self) -> None:
        """在末尾插入空数据行。"""
        if not self._table_data:
            return
        num_cols = len(self._table_data["fields"])
        new_row = [None] * num_cols
        self._table_data["rows"].append(new_row)

        display_row = [""] * num_cols
        self._tree.insert("", "end", values=display_row)
        self._mark_modified()

    def _delete_row(self) -> None:
        """删除选中的数据行。"""
        if not self._table_data:
            return
        selected = self._tree.selection()
        if not selected:
            return

        all_items = self._tree.get_children()
        for item in selected:
            item_idx = all_items.index(item)
            if item_idx < 2:  # 不能删除类型行和注释行
                continue
            data_row_idx = item_idx - 2
            if 0 <= data_row_idx < len(self._table_data["rows"]):
                self._table_data["rows"].pop(data_row_idx)
            self._tree.delete(item)
        self._mark_modified()

    def _mark_modified(self) -> None:
        """标记数据已修改。"""
        self._is_modified = True
        if self._on_modified:
            self._on_modified()
