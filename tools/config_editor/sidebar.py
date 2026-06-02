# -*- coding: utf-8 -*-
"""配置编辑器侧边栏组件。

显示 tables/ 目录下的 Excel 文件列表，支持点击切换和右键菜单。
"""
import os
import logging
import tkinter as tk
from typing import Callable, Optional

import customtkinter as ctk

logger = logging.getLogger(__name__)


class Sidebar(ctk.CTkFrame):
    """左侧文件列表侧边栏。"""

    def __init__(self, master, on_file_select: Callable[[str], None], **kwargs):
        """初始化侧边栏。

        Args:
            master: 父组件。
            on_file_select: 文件选中回调，参数为文件完整路径。
        """
        super().__init__(master, width=200, **kwargs)
        self.pack_propagate(False)

        self._on_file_select = on_file_select
        self._tables_dir: Optional[str] = None
        self._selected_btn: Optional[ctk.CTkButton] = None

        # 标题
        self._title_label = ctk.CTkLabel(
            self, text="配置表", font=ctk.CTkFont(size=14, weight="bold")
        )
        self._title_label.pack(padx=10, pady=(10, 5), anchor="w")

        # 文件列表容器（可滚动）
        self._list_frame = ctk.CTkScrollableFrame(self)
        self._list_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # 文件按钮列表
        self._file_buttons: list = []

    def set_tables_dir(self, tables_dir: str) -> None:
        """设置 tables 目录路径并刷新文件列表。"""
        self._tables_dir = tables_dir
        self._refresh_file_list()

    def refresh(self) -> None:
        """刷新文件列表。"""
        self._refresh_file_list()

    def _refresh_file_list(self) -> None:
        """重新扫描目录并更新文件列表。"""
        # 清除旧按钮
        for btn in self._file_buttons:
            btn.destroy()
        self._file_buttons.clear()

        if not self._tables_dir or not os.path.isdir(self._tables_dir):
            return

        # 扫描 .xlsx 文件
        xlsx_files = sorted(
            f for f in os.listdir(self._tables_dir)
            if f.endswith(".xlsx") and not f.startswith("~$")  # 排除临时文件
        )

        for filename in xlsx_files:
            btn = ctk.CTkButton(
                self._list_frame,
                text=f"📦 {filename}",
                anchor="w",
                height=32,
                font=ctk.CTkFont(size=13),
                fg_color="transparent",
                text_color=("gray10", "gray90"),
                hover_color=("gray75", "gray25"),
                command=lambda p=os.path.join(self._tables_dir, filename): self._select_file(p),
            )
            btn.pack(fill=tk.X, padx=2, pady=1)
            self._file_buttons.append(btn)

    def _select_file(self, file_path: str) -> None:
        """选中文件并高亮按钮。"""
        # 取消旧选中
        if self._selected_btn:
            self._selected_btn.configure(fg_color="transparent")

        # 高亮新选中
        for btn in self._file_buttons:
            full_path = os.path.join(self._tables_dir, btn.cget("text").replace("📦 ", ""))
            if full_path == file_path:
                btn.configure(fg_color=("gray75", "gray25"))
                self._selected_btn = btn
                break

        self._on_file_select(file_path)