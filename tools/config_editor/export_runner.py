# -*- coding: utf-8 -*-
"""配置编辑器导表触发模块。

在 GUI 中触发导表操作，显示结果对话框。
"""
import logging
import threading
import tkinter as tk
from tkinter import messagebox
from typing import Any, Callable, Dict

import customtkinter as ctk

from tools.table_export import export_table, export_all

logger = logging.getLogger(__name__)


class ExportRunner:
    """导表触发器。"""

    def __init__(self, project_root: str):
        """初始化。

        Args:
            project_root: 项目根目录路径。
        """
        self._project_root = project_root

    def run_single(self, excel_path: str, parent: ctk.CTkFrame, callback: Callable[[Dict], None]) -> None:
        """在后台线程执行单表导出。

        Args:
            excel_path: Excel 文件路径。
            parent: 父窗口（用于显示结果对话框）。
            callback: 完成回调，参数为导出结果 dict。
        """
        def _do_export():
            result = export_table(excel_path, self._project_root)
            parent.after(0, lambda: self._show_single_result(result))
            parent.after(0, lambda: callback(result))

        thread = threading.Thread(target=_do_export, daemon=True)
        thread.start()

    def run_all(self, tables_dir: str, parent: ctk.CTkFrame, callback: Callable[[Dict], None]) -> None:
        """在后台线程执行全表导出。

        Args:
            tables_dir: tables 目录路径。
            parent: 父窗口（用于显示结果对话框）。
            callback: 完成回调，参数为导出结果 dict。
        """
        def _do_export():
            result = export_all(tables_dir, self._project_root)
            parent.after(0, lambda: self._show_all_result(result))
            parent.after(0, lambda: callback(result))

        thread = threading.Thread(target=_do_export, daemon=True)
        thread.start()

    def _show_single_result(self, result: Dict[str, Any]) -> None:
        """显示单表导出结果对话框。"""
        if result["success"]:
            messagebox.showinfo(
                "导出成功",
                f"表 '{result['table_name']}' 导出成功！\n\n"
                f"Python: {result['python_path']}\n"
                f"C++: {result['cpp_path']}",
            )
        else:
            errors_text = "\n".join(result["errors"])
            messagebox.showerror(
                "导出失败",
                f"表 '{result['table_name']}' 导出失败：\n\n{errors_text}",
            )

    def _show_all_result(self, result: Dict[str, Any]) -> None:
        """显示全表导出结果对话框。"""
        total = result["total"]
        success = result["success_count"]
        fail = result["fail_count"]

        details = []
        for r in result["results"]:
            status = "✓" if r["success"] else "✗"
            details.append(f"{status} {r['table_name']}")
            if r["errors"]:
                for err in r["errors"]:
                    details.append(f"  - {err}")

        details_text = "\n".join(details)
        message = f"导出完成！\n\n成功: {success}, 失败: {fail}, 总计: {total}\n\n{details_text}"

        if fail == 0:
            messagebox.showinfo("导出结果", message)
        else:
            messagebox.showwarning("导出结果", message)
