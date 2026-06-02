"""Main application window for the quest editor."""

from __future__ import annotations

import os
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from typing import Optional

from quest_node import QuestNode
from quest_data import QuestData
from canvas_view import CanvasView
from property_panel import PropertyPanel
from validation import validate_quest_config


class EditorApp:
    """Quest editor main application window."""

    WINDOW_TITLE: str = "任务编辑器"
    WINDOW_SIZE: str = "1200x800"
    INITIAL_DIR: str = "config"

    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title(self.WINDOW_TITLE)
        self.root.geometry(self.WINDOW_SIZE)

        self._quest_data = QuestData()
        self._selected_quest_id: Optional[str] = None
        self._connect_mode: bool = False
        self._connect_source: Optional[str] = None

        self._build_menu()
        self._build_toolbar()
        self._build_main_layout()
        self._build_status_bar()

        self._set_status("就绪")

    # ------------------------------------------------------------------
    # Menu bar
    # ------------------------------------------------------------------

    def _build_menu(self) -> None:
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)

        # File menu
        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="文件", menu=file_menu)
        file_menu.add_command(label="新建", accelerator="Ctrl+N", command=self._new_file)
        file_menu.add_command(label="打开", accelerator="Ctrl+O", command=self._open_file)
        file_menu.add_command(label="保存", accelerator="Ctrl+S", command=self._save_file)
        file_menu.add_command(label="另存为", command=self._save_as_file)
        file_menu.add_separator()
        file_menu.add_command(label="退出", command=self.root.quit)

        # Edit menu
        edit_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="编辑", menu=edit_menu)
        edit_menu.add_command(label="添加任务", command=self._add_quest)
        edit_menu.add_command(label="删除选中", command=self._delete_selected)
        edit_menu.add_separator()
        edit_menu.add_command(label="验证配置", command=self._validate_config)

        # Keyboard shortcuts
        self.root.bind("<Control-n>", lambda e: self._new_file())
        self.root.bind("<Control-o>", lambda e: self._open_file())
        self.root.bind("<Control-s>", lambda e: self._save_file())

    # ------------------------------------------------------------------
    # Toolbar
    # ------------------------------------------------------------------

    def _build_toolbar(self) -> None:
        toolbar = ttk.Frame(self.root)
        toolbar.pack(side=tk.TOP, fill=tk.X, padx=2, pady=2)

        buttons = [
            ("新建", self._new_file),
            ("打开", self._open_file),
            ("保存", self._save_file),
            ("添加任务", self._add_quest),
            ("连线", self._start_connect),
            ("删除", self._delete_selected),
        ]

        for text, command in buttons:
            ttk.Button(toolbar, text=text, command=command).pack(
                side=tk.LEFT, padx=2
            )

    # ------------------------------------------------------------------
    # Main layout
    # ------------------------------------------------------------------

    def _build_main_layout(self) -> None:
        paned = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        # Left: canvas view (weight 3)
        self._canvas_view = CanvasView(
            paned, on_node_selected=self._on_node_select
        )
        paned.add(self._canvas_view, weight=3)

        # Right: property panel (weight 1)
        self._property_panel = PropertyPanel(
            paned, on_apply=self._on_property_update
        )
        paned.add(self._property_panel, weight=1)

    # ------------------------------------------------------------------
    # Status bar
    # ------------------------------------------------------------------

    def _build_status_bar(self) -> None:
        self._status_var = tk.StringVar(value="")
        status_bar = ttk.Label(
            self.root, textvariable=self._status_var, relief=tk.SUNKEN, anchor=tk.W
        )
        status_bar.pack(side=tk.BOTTOM, fill=tk.X)

    def _set_status(self, message: str) -> None:
        self._status_var.set(message)

    # ------------------------------------------------------------------
    # File operations
    # ------------------------------------------------------------------

    def _new_file(self) -> None:
        """Create a new empty quest configuration."""
        self._quest_data.new()
        self._selected_quest_id = None
        self._refresh_canvas()
        self._property_panel.load_quest("", QuestNode())
        self._set_status("已创建新文件")

    def _open_file(self) -> None:
        """Open a JSON quest configuration via file dialog."""
        file_path = filedialog.askopenfilename(
            title="打开任务配置",
            initialdir=self.INITIAL_DIR,
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
        )
        if not file_path:
            return

        success, message = self._quest_data.load(file_path)
        if success:
            self._selected_quest_id = None
            self._refresh_canvas()
            self._set_status(message)
        else:
            messagebox.showerror("打开失败", message)
            self._set_status("打开失败")

    def _save_file(self) -> None:
        """Save the current quest configuration."""
        if self._quest_data.file_path is None:
            self._save_as_file()
            return

        success, message = self._quest_data.save()
        if success:
            self._set_status(message)
        else:
            messagebox.showerror("保存失败", message)

    def _save_as_file(self) -> None:
        """Save the quest configuration to a new file path."""
        file_path = filedialog.asksaveasfilename(
            title="另存为",
            initialdir=self.INITIAL_DIR,
            defaultextension=".json",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
        )
        if not file_path:
            return

        success, message = self._quest_data.save(file_path)
        if success:
            self._set_status(message)
        else:
            messagebox.showerror("保存失败", message)

    # ------------------------------------------------------------------
    # Quest operations
    # ------------------------------------------------------------------

    def _add_quest(self) -> None:
        """Add a new quest with an auto-incrementing ID."""
        # Find the next available quest_NNN id
        existing = set(self._quest_data.quests.keys())
        idx = 1
        while True:
            quest_id = f"quest_{idx:03d}"
            if quest_id not in existing:
                break
            idx += 1

        # Place the new node at a reasonable position on the canvas
        x = 200.0 + (len(self._quest_data.quests) % 5) * 180
        y = 150.0 + (len(self._quest_data.quests) // 5) * 100

        self._quest_data.add_quest(quest_id, x=x, y=y)
        self._canvas_view.add_node(quest_id, quest_id, x, y)
        self._canvas_view.select_node(quest_id)
        self._set_status(f"已添加任务: {quest_id}")

    def _delete_selected(self) -> None:
        """Delete the currently selected quest node."""
        if self._selected_quest_id is None:
            self._set_status("未选中任何任务")
            return

        quest_id = self._selected_quest_id
        self._quest_data.remove_quest(quest_id)
        self._canvas_view.remove_node(quest_id)
        self._selected_quest_id = None
        self._property_panel.load_quest("", QuestNode())
        self._set_status(f"已删除任务: {quest_id}")

    def _start_connect(self) -> None:
        """Enter connection mode (placeholder for now)."""
        self._connect_mode = not self._connect_mode
        if self._connect_mode:
            self._connect_source = self._selected_quest_id
            self._set_status("连线模式已开启 - 请选择目标任务")
        else:
            self._connect_source = None
            self._set_status("连线模式已关闭")

    def _validate_config(self) -> None:
        """Run validation and show results in a message box."""
        errors = validate_quest_config(self._quest_data.quests)
        if errors:
            messagebox.showwarning(
                "验证结果",
                f"发现 {len(errors)} 个问题:\n\n" + "\n".join(errors),
            )
            self._set_status(f"验证完成: 发现 {len(errors)} 个问题")
        else:
            messagebox.showinfo("验证结果", "配置验证通过，没有发现问题。")
            self._set_status("验证通过")

    # ------------------------------------------------------------------
    # Event handlers
    # ------------------------------------------------------------------

    def _on_node_select(self, quest_id: Optional[str]) -> None:
        """Handle node selection on the canvas."""
        self._selected_quest_id = quest_id

        if quest_id is not None:
            node = self._quest_data.get_quest(quest_id)
            if node is not None:
                # If in connect mode, complete the connection
                if self._connect_mode and self._connect_source is not None:
                    if self._connect_source != quest_id:
                        self._quest_data.connect_quests(
                            self._connect_source, quest_id
                        )
                        self._canvas_view.add_connection(
                            self._connect_source, quest_id
                        )
                        self._set_status(
                            f"已连接: {self._connect_source} -> {quest_id}"
                        )
                    self._connect_mode = False
                    self._connect_source = None
                    return

                self._property_panel.load_quest(quest_id, node)
                self._set_status(f"已选中: {quest_id}")
        else:
            self._property_panel.load_quest("", QuestNode())
            self._set_status("就绪")

    def _on_property_update(self, quest_id: str, node: QuestNode) -> None:
        """Handle property changes from the property panel."""
        # Update the node label on the canvas
        canvas_node = self._canvas_view.get_node(quest_id)
        if canvas_node is not None:
            canvas_node.update_text(node.name if node.name else quest_id)
        self._quest_data.modified = True
        self._set_status(f"已更新任务: {quest_id}")

    # ------------------------------------------------------------------
    # Canvas refresh
    # ------------------------------------------------------------------

    def _refresh_canvas(self) -> None:
        """Clear and redraw all nodes and connections on the canvas."""
        # Remove all existing nodes
        for quest_id in list(self._canvas_view._nodes.keys()):
            self._canvas_view.remove_node(quest_id)

        # Re-add all nodes from quest data
        for quest_id, node in self._quest_data.quests.items():
            label = node.name if node.name else quest_id
            self._canvas_view.add_node(quest_id, label, node.x, node.y)

        # Redraw connections based on prerequisites
        for quest_id, node in self._quest_data.quests.items():
            for prereq_id in node.prerequisites:
                if prereq_id in self._quest_data.quests:
                    self._canvas_view.add_connection(prereq_id, quest_id)


def main() -> None:
    """Entry point for the quest editor."""
    root = tk.Tk()
    _app = EditorApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
