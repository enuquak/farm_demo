# -*- coding: utf-8 -*-
"""配置编辑器主窗口。

工具栏 + 左侧文件列表 + 右侧表格编辑区 + 状态栏。
"""
import logging
import os
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog
from pathlib import Path
from typing import Optional

import customtkinter as ctk

from .sidebar import Sidebar
from .table_editor import TableEditor
from .export_runner import ExportRunner
from tools.table_export import read_excel

logger = logging.getLogger(__name__)


class ConfigEditorApp:
    """配置编辑器主窗口。"""

    def __init__(self, project_root: str):
        """初始化编辑器。

        Args:
            project_root: 项目根目录路径。
        """
        self._project_root = project_root
        self._tables_dir: Optional[str] = None
        self._current_file: Optional[str] = None

        # 配置 CustomTkinter 外观
        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("blue")

        # 创建主窗口
        self.root = ctk.CTk()
        self.root.title("配置编辑器")
        self.root.geometry("1200x700")
        self.root.minsize(800, 500)

        # 导表触发器
        self._export_runner = ExportRunner(project_root)

        # 构建 UI
        self._build_toolbar()
        self._build_main_area()
        self._build_status_bar()

        # 默认尝试加载 tables/ 目录
        default_tables = os.path.join(project_root, "tables")
        if os.path.isdir(default_tables):
            self._tables_dir = default_tables
            self._sidebar.set_tables_dir(default_tables)

    def run(self) -> None:
        """启动 GUI 主循环。"""
        self.root.mainloop()

    # ---- UI 构建 ----

    def _build_toolbar(self) -> None:
        """构建工具栏。"""
        toolbar = ctk.CTkFrame(self.root, height=50)
        toolbar.pack(fill=tk.X, padx=10, pady=(10, 5))
        toolbar.pack_propagate(False)

        buttons = [
            ("打开目录", self._on_open_dir, "#0e639c", "#0a4a6e"),
            ("新建表", self._on_new_table, "#16825d", "#0e5c3f"),
            ("保存", self._on_save, "#0e639c", "#0a4a6e"),
            ("导出当前表", self._on_export_current, "#c2432c", "#8b2e1e"),
            ("导出全部", self._on_export_all, "#c2432c", "#8b2e1e"),
        ]

        for text, command, fg_color, hover_color in buttons:
            btn = ctk.CTkButton(
                toolbar, text=text, width=100, height=35,
                font=ctk.CTkFont(size=13),
                fg_color=fg_color, hover_color=hover_color,
                command=command,
            )
            btn.pack(side=tk.LEFT, padx=5, pady=7)

    def _build_main_area(self) -> None:
        """构建主内容区（左侧边栏 + 右侧编辑区）。"""
        main_frame = ctk.CTkFrame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # 左侧边栏
        self._sidebar = Sidebar(main_frame, on_file_select=self._on_file_select)
        self._sidebar.pack(side=tk.LEFT, fill=tk.Y)

        # 右侧编辑区
        self._table_editor = TableEditor(main_frame, on_modified=self._on_data_modified)
        self._table_editor.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(5, 0))

    def _build_status_bar(self) -> None:
        """构建底部状态栏。"""
        status_bar = ctk.CTkFrame(self.root, height=30)
        status_bar.pack(fill=tk.X, padx=10, pady=(5, 10))
        status_bar.pack_propagate(False)

        self._status_label = ctk.CTkLabel(
            status_bar, text="就绪", font=ctk.CTkFont(size=11), text_color="gray"
        )
        self._status_label.pack(side=tk.LEFT, padx=10)

        self._info_label = ctk.CTkLabel(
            status_bar, text="", font=ctk.CTkFont(size=11), text_color="gray"
        )
        self._info_label.pack(side=tk.RIGHT, padx=10)

    # ---- 事件处理 ----

    def _on_open_dir(self) -> None:
        """打开目录对话框。"""
        directory = filedialog.askdirectory(
            title="选择 tables 目录",
            initialdir=self._tables_dir or self._project_root,
        )
        if directory:
            self._tables_dir = directory
            self._sidebar.set_tables_dir(directory)
            self._update_status(f"已打开目录: {directory}")

    def _on_new_table(self) -> None:
        """创建新表。"""
        if not self._tables_dir:
            messagebox.showwarning("提示", "请先打开 tables 目录")
            return

        name = simpledialog.askstring("新建表", "请输入表名（英文，snake_case）：")
        if not name:
            return

        # 创建空白 Excel
        from openpyxl import Workbook

        wb = Workbook()
        ws = wb.active
        ws.append(["id", "name"])
        ws.append(["int", "str"])
        ws.append(["# ID", "# 名称"])

        file_path = os.path.join(self._tables_dir, f"{name}.xlsx")
        if os.path.exists(file_path):
            messagebox.showwarning("提示", f"文件 {name}.xlsx 已存在")
            return

        wb.save(file_path)
        self._sidebar.refresh()
        self._update_status(f"已创建: {name}.xlsx")

    def _on_save(self) -> None:
        """保存当前编辑的表。"""
        if not self._current_file or not self._table_editor.is_modified():
            return

        table_data = self._table_editor.get_table_data()
        if not table_data:
            return

        try:
            self._save_excel(self._current_file, table_data)
            self._update_status(f"已保存: {os.path.basename(self._current_file)}")
        except Exception as e:
            messagebox.showerror("保存失败", f"保存失败: {e}")

    def _on_export_current(self) -> None:
        """导出当前表。"""
        if not self._current_file:
            messagebox.showwarning("提示", "请先选择一张表")
            return

        # 先保存
        self._on_save()

        self._update_status("正在导出...")
        self._export_runner.run_single(
            self._current_file, self._table_editor,
            callback=lambda r: self._update_status(
                f"导出{'成功' if r['success'] else '失败'}: {r['table_name']}"
            ),
        )

    def _on_export_all(self) -> None:
        """导出全部表。"""
        if not self._tables_dir:
            messagebox.showwarning("提示", "请先打开 tables 目录")
            return

        # 保存当前表
        self._on_save()

        self._update_status("正在导出全部...")
        self._export_runner.run_all(
            self._tables_dir, self._table_editor,
            callback=lambda r: self._update_status(
                f"导出完成: 成功 {r['success_count']}, 失败 {r['fail_count']}"
            ),
        )

    def _on_file_select(self, file_path: str) -> None:
        """文件选中事件。"""
        # 检查是否有未保存的修改
        if self._table_editor.is_modified():
            if not messagebox.askyesno("未保存", "当前修改未保存，是否放弃？"):
                return

        # 加载新文件
        try:
            table_data = read_excel(file_path)
            self._table_editor.load_table(table_data)
            self._current_file = file_path
            self._update_status(f"已加载: {os.path.basename(file_path)}")
            self._update_info(f"共 {len(table_data['rows'])} 行数据")
        except Exception as e:
            messagebox.showerror("加载失败", f"无法加载文件: {e}")

    def _on_data_modified(self) -> None:
        """数据修改回调。"""
        self._update_status(f"已修改: {os.path.basename(self._current_file or '')}")

    # ---- 辅助方法 ----

    def _save_excel(self, file_path: str, table_data: dict) -> None:
        """将表数据保存为 Excel 文件。"""
        from openpyxl import Workbook

        wb = Workbook()
        ws = wb.active

        ws.append(table_data["fields"])
        ws.append(table_data["types"])
        ws.append(table_data["comments"])
        for row in table_data["rows"]:
            ws.append(row)

        wb.save(file_path)

    def _update_status(self, text: str) -> None:
        """更新状态栏文本。"""
        self._status_label.configure(text=text)

    def _update_info(self, text: str) -> None:
        """更新状态栏右侧信息。"""
        self._info_label.configure(text=text)
