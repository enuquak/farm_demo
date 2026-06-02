# -*- coding: utf-8 -*-
"""服务器控制台 GUI 模块。

基于 CustomTkinter 构建的服务器控制台界面，提供进程管理、状态监控和日志查看功能。
"""

import logging
import threading
import tkinter as tk
from typing import Optional

import customtkinter as ctk

from .constants import SERVICE_NAMES, STATUS_CHECK_INTERVAL_MS
from .log_reader import LogReader
from .process_manager import force_stop_all, get_service_status, restart_all

logger = logging.getLogger(__name__)

# 状态指示符
STATUS_RUNNING = "●"  # 黑色圆点
STATUS_STOPPED = "○"  # 白色圆点


class ServerConsoleGUI:
    """服务器控制台 GUI 主类。"""

    def __init__(self) -> None:
        """初始化 GUI。"""
        # 配置 CustomTkinter 外观
        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("blue")

        # 创建主窗口
        self.root = ctk.CTk()
        self.root.title("Server Console")
        self.root.geometry("900x600")
        self.root.minsize(700, 400)

        # 状态
        self._is_restarting = False
        self._auto_scroll: dict[str, bool] = {name: True for name in SERVICE_NAMES}
        self._text_widgets: dict[str, ctk.CTkTextbox] = {}
        self._tab_view: Optional[ctk.CTkTabview] = None
        self._segmented_button = None
        self._status_label: Optional[ctk.CTkLabel] = None
        self._restart_button: Optional[ctk.CTkButton] = None
        self._force_stop_button: Optional[ctk.CTkButton] = None
        self._log_readers: dict[str, LogReader] = {}
        self._current_tab: str = SERVICE_NAMES[0]

        # 构建 UI
        self._build_toolbar()
        self._build_tab_view()
        self._build_status_bar()

        # 启动日志读取器
        self._start_log_readers()

        # 启动状态检测定时器
        self._schedule_status_check()

    def run(self) -> None:
        """启动 GUI 主循环。"""
        logger.info("ServerConsoleGUI.run, starting main loop")
        self.root.mainloop()

    # ---- UI 构建 ----

    def _build_toolbar(self) -> None:
        """构建工具栏：Restart All + Force Stop 按钮。"""
        toolbar = ctk.CTkFrame(self.root, height=50)
        toolbar.pack(fill=tk.X, padx=10, pady=(10, 5))
        toolbar.pack_propagate(False)

        self._restart_button = ctk.CTkButton(
            toolbar,
            text="Restart All",
            width=120,
            height=35,
            command=self._on_restart_all,
        )
        self._restart_button.pack(side=tk.LEFT, padx=(10, 5), pady=7)

        self._force_stop_button = ctk.CTkButton(
            toolbar,
            text="Force Stop",
            width=120,
            height=35,
            fg_color="#d9534f",
            hover_color="#c9302c",
            command=self._on_force_stop,
        )
        self._force_stop_button.pack(side=tk.LEFT, padx=5, pady=7)

        self._status_label = ctk.CTkLabel(
            toolbar,
            text="Status: 0/3",
            font=ctk.CTkFont(size=14),
        )
        self._status_label.pack(side=tk.RIGHT, padx=10, pady=7)

    def _build_tab_view(self) -> None:
        """构建日志页签视图。"""
        self._tab_view = ctk.CTkTabview(self.root)
        self._tab_view.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        for name in SERVICE_NAMES:
            tab = self._tab_view.add(name)

            # 创建日志文本框
            text_box = ctk.CTkTextbox(
                tab,
                font=ctk.CTkFont(family="Consolas", size=12),
                wrap=tk.NONE,
                state=tk.DISABLED,
            )
            text_box.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
            self._text_widgets[name] = text_box

            # 绑定滚动事件用于自动滚动检测
            text_box._textbox.bind("<MouseWheel>", lambda e, n=name: self._on_scroll(n))
            text_box._textbox.bind("<Button-4>", lambda e, n=name: self._on_scroll(n))
            text_box._textbox.bind("<Button-5>", lambda e, n=name: self._on_scroll(n))

        # 保存分段按钮引用，用于更新页签标题
        self._segmented_button = self._tab_view._segmented_button
        self._segmented_button.configure(command=self._on_tab_change)

    def _build_status_bar(self) -> None:
        """构建底部状态栏。"""
        status_bar = ctk.CTkFrame(self.root, height=30)
        status_bar.pack(fill=tk.X, padx=10, pady=(5, 10))
        status_bar.pack_propagate(False)

        self._info_label = ctk.CTkLabel(
            status_bar,
            text="Server Console Ready",
            font=ctk.CTkFont(size=11),
            text_color="gray",
        )
        self._info_label.pack(side=tk.LEFT, padx=10)

    # ---- 日志读取 ----

    def _start_log_readers(self) -> None:
        """为每个服务启动日志读取器。"""
        for name in SERVICE_NAMES:
            reader = LogReader(name, on_new_line=lambda line, n=name: self._on_log_line(n, line))
            reader.start()
            self._log_readers[name] = reader

    def _on_log_line(self, name: str, line: str) -> None:
        """日志行到达回调（从后台线程调用）。"""
        self.root.after(0, self._append_log_line, name, line)

    def _append_log_line(self, name: str, line: str) -> None:
        """在 GUI 线程中追加日志行（线程安全）。"""
        if name != self._current_tab:
            return

        text_box = self._text_widgets.get(name)
        if text_box is None:
            return

        text_box.configure(state=tk.NORMAL)
        text_box.insert(tk.END, line + "\n")
        text_box.configure(state=tk.DISABLED)

        # 自动滚动
        if self._auto_scroll.get(name, True):
            text_box.see(tk.END)

    def _on_tab_change(self, display_name: str) -> None:
        """页签切换事件。

        Args:
            display_name: 分段按钮的显示文本（可能包含状态图标，如 '● dbmgr_server'）
        """
        # 从显示文本中提取原始服务名称（去掉状态图标前缀）
        tab_name = display_name
        for name in SERVICE_NAMES:
            if name in display_name:
                tab_name = name
                break

        self._current_tab = tab_name
        self._replay_log_cache(tab_name)

    def _replay_log_cache(self, name: str) -> None:
        """从缓存回放日志到文本框。"""
        text_box = self._text_widgets.get(name)
        if text_box is None:
            return

        reader = self._log_readers.get(name)
        if reader is None:
            return

        text_box.configure(state=tk.NORMAL)
        text_box.delete("1.0", tk.END)
        for line in reader.get_all_lines():
            text_box.insert(tk.END, line + "\n")
        text_box.configure(state=tk.DISABLED)

        # 回放后自动滚动到底部
        self._auto_scroll[name] = True
        text_box.see(tk.END)

    def _on_scroll(self, name: str) -> None:
        """用户手动滚动事件，用于检测是否需要暂停自动滚动。"""
        text_box = self._text_widgets.get(name)
        if text_box is None:
            return

        # 延迟检测，让滚动操作完成
        self.root.after(100, self._check_scroll_position, name)

    def _check_scroll_position(self, name: str) -> None:
        """检查滚动位置，判断是否在底部。"""
        text_box = self._text_widgets.get(name)
        if text_box is None:
            return

        try:
            # 检查是否在底部
            current_pos = text_box._textbox.yview()
            at_bottom = current_pos[1] >= 0.99
            self._auto_scroll[name] = at_bottom
        except Exception:
            pass

    # ---- 按钮事件 ----

    def _on_restart_all(self) -> None:
        """点击 Restart All 按钮。"""
        if self._is_restarting:
            return

        self._is_restarting = True
        self._restart_button.configure(state=tk.DISABLED, text="Restarting...")
        self._update_info_label("Restarting all services...")

        logger.info("GUI._on_restart_all, starting restart in background thread")

        def _do_restart():
            restart_all()
            self.root.after(0, self._on_restart_complete)

        thread = threading.Thread(target=_do_restart, daemon=True)
        thread.start()

    def _on_restart_complete(self) -> None:
        """重启完成回调（GUI 线程）。"""
        self._is_restarting = False
        self._restart_button.configure(state=tk.NORMAL, text="Restart All")
        self._update_info_label("Restart complete")
        self._check_all_status()

    def _on_force_stop(self) -> None:
        """点击 Force Stop 按钮。"""
        logger.info("GUI._on_force_stop, stopping all services")

        def _do_stop():
            force_stop_all()
            self.root.after(0, self._on_force_stop_complete)

        thread = threading.Thread(target=_do_stop, daemon=True)
        thread.start()

    def _on_force_stop_complete(self) -> None:
        """强制停服完成回调（GUI 线程）。"""
        self._update_info_label("All services force stopped")
        self._check_all_status()

    # ---- 状态检测 ----

    def _schedule_status_check(self) -> None:
        """定时检测服务状态。"""
        self._check_all_status()
        self.root.after(STATUS_CHECK_INTERVAL_MS, self._schedule_status_check)

    def _check_all_status(self) -> None:
        """检测所有服务状态并更新 UI。"""
        running_count = 0
        tab_values = []

        for name in SERVICE_NAMES:
            is_running = get_service_status(name)
            if is_running:
                running_count += 1

            # 构建页签显示文本（状态图标 + 服务名）
            status_icon = STATUS_RUNNING if is_running else STATUS_STOPPED
            tab_values.append(f"{status_icon} {name}")

        # 更新分段按钮的显示文本
        if self._segmented_button is not None:
            try:
                self._segmented_button.configure(values=tab_values)
            except Exception:
                pass

        # 更新状态栏
        total = len(SERVICE_NAMES)
        if self._status_label is not None:
            self._status_label.configure(text=f"Status: {running_count}/{total}")

    def _update_info_label(self, text: str) -> None:
        """更新底部信息标签。"""
        if self._info_label is not None:
            self._info_label.configure(text=text)
