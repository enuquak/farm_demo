"""Canvas-based node graph view for the quest editor."""

from __future__ import annotations

import tkinter as tk
from typing import Callable, Dict, Optional, Tuple


# ---------------------------------------------------------------------------
# QuestNodeWidget -- visual representation of a single quest node
# ---------------------------------------------------------------------------

class QuestNodeWidget:
    """Draws and manages a single quest node on a tkinter Canvas."""

    WIDTH: int = 150
    HEIGHT: int = 60

    FILL_NORMAL: str = "#4a90d9"
    FILL_SELECTED: str = "#6ab0ff"
    OUTLINE_NORMAL: str = "#2d6cb4"
    OUTLINE_SELECTED: str = "#a0d0ff"
    TEXT_COLOR: str = "#ffffff"

    def __init__(
        self,
        canvas: tk.Canvas,
        quest_id: str,
        label: str,
        x: float,
        y: float,
        on_select: Optional[Callable[[str], None]] = None,
    ) -> None:
        self.canvas = canvas
        self.quest_id = quest_id
        self.on_select = on_select

        self._selected: bool = False
        self._x: float = x
        self._y: float = y

        # Draw rectangle (anchor = center)
        self._rect_id: int = canvas.create_rectangle(
            x - self.WIDTH / 2,
            y - self.HEIGHT / 2,
            x + self.WIDTH / 2,
            y + self.HEIGHT / 2,
            fill=self.FILL_NORMAL,
            outline=self.OUTLINE_NORMAL,
            width=2,
            tags=("node",),
        )

        # Draw text label centered on the rectangle
        self._text_id: int = canvas.create_text(
            x,
            y,
            text=label,
            fill=self.TEXT_COLOR,
            font=("Microsoft YaHei", 10, "bold"),
            width=self.WIDTH - 10,
            tags=("node_text",),
        )

    # -- public helpers ----------------------------------------------------

    @property
    def center(self) -> Tuple[float, float]:
        """Return the current center ``(x, y)`` of the widget."""
        return self._x, self._y

    @property
    def selected(self) -> bool:
        return self._selected

    def update_position(self, x: float, y: float) -> None:
        """Move the widget to a new center position."""
        dx, dy = x - self._x, y - self._y
        self.canvas.move(self._rect_id, dx, dy)
        self.canvas.move(self._text_id, dx, dy)
        self._x, self._y = x, y

    def update_text(self, label: str) -> None:
        """Change the displayed label."""
        self.canvas.itemconfigure(self._text_id, text=label)

    def set_selected(self, selected: bool) -> None:
        """Update the visual style to reflect selection state."""
        self._selected = selected
        fill = self.FILL_SELECTED if selected else self.FILL_NORMAL
        outline = self.OUTLINE_SELECTED if selected else self.OUTLINE_NORMAL
        self.canvas.itemconfigure(self._rect_id, fill=fill, outline=outline)

    def delete(self) -> None:
        """Remove the widget from the canvas."""
        self.canvas.delete(self._rect_id)
        self.canvas.delete(self._text_id)

    def contains(self, canvas_x: float, canvas_y: float) -> bool:
        """Return *True* if the given canvas coordinate is inside the node."""
        half_w = self.WIDTH / 2
        half_h = self.HEIGHT / 2
        return (
            self._x - half_w <= canvas_x <= self._x + half_w
            and self._y - half_h <= canvas_y <= self._y + half_h
        )


# ---------------------------------------------------------------------------
# CanvasView -- scrollable canvas hosting node widgets and connections
# ---------------------------------------------------------------------------

class CanvasView(tk.Frame):
    """A scrollable canvas that displays quest nodes and their connections."""

    CONNECTION_COLOR: str = "#ffcc00"
    BG_COLOR: str = "#2b2b2b"

    def __init__(
        self,
        master: tk.Widget,
        on_node_selected: Optional[Callable[[Optional[str]], None]] = None,
        **kwargs,
    ) -> None:
        super().__init__(master, **kwargs)

        self.on_node_selected = on_node_selected

        # -- data ----------------------------------------------------------
        self._nodes: Dict[str, QuestNodeWidget] = {}
        # connection_lines[(from_id, to_id)] = canvas_line_id
        self._connection_lines: Dict[Tuple[str, str], int] = {}

        # -- drag state ----------------------------------------------------
        self._drag_data: Optional[Dict] = None

        # -- scrollable canvas ---------------------------------------------
        self._h_scroll = tk.Scrollbar(self, orient=tk.HORIZONTAL)
        self._v_scroll = tk.Scrollbar(self, orient=tk.VERTICAL)

        self._canvas = tk.Canvas(
            self,
            bg=self.BG_COLOR,
            highlightthickness=0,
            xscrollcommand=self._h_scroll.set,
            yscrollcommand=self._v_scroll.set,
        )
        self._h_scroll.config(command=self._canvas.xview)
        self._v_scroll.config(command=self._canvas.yview)

        self._canvas.grid(row=0, column=0, sticky="nsew")
        self._v_scroll.grid(row=0, column=1, sticky="ns")
        self._h_scroll.grid(row=1, column=0, sticky="ew")

        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(0, weight=1)

        # A large scrollable region
        self._canvas.config(scrollregion=(0, 0, 3000, 3000))

        # -- bindings ------------------------------------------------------
        self._canvas.bind("<Button-1>", self._on_button_press)
        self._canvas.bind("<B1-Motion>", self._on_drag)
        self._canvas.bind("<ButtonRelease-1>", self._on_button_release)

    # -- canvas access (for subclasses or external callers) ----------------

    @property
    def canvas(self) -> tk.Canvas:
        return self._canvas

    # ------------------------------------------------------------------
    # Node management
    # ------------------------------------------------------------------

    def add_node(
        self,
        quest_id: str,
        label: str,
        x: float = 100.0,
        y: float = 100.0,
    ) -> QuestNodeWidget:
        """Create a node widget and register it."""
        if quest_id in self._nodes:
            # Already exists -- just update text / position.
            node = self._nodes[quest_id]
            node.update_text(label)
            node.update_position(x, y)
            return node

        node = QuestNodeWidget(
            self._canvas,
            quest_id=quest_id,
            label=label,
            x=x,
            y=y,
        )
        self._nodes[quest_id] = node
        return node

    def remove_node(self, quest_id: str) -> None:
        """Delete a node widget and all its connections."""
        node = self._nodes.pop(quest_id, None)
        if node is not None:
            node.delete()

        # Remove associated connection lines
        to_remove = [
            key for key in self._connection_lines if quest_id in key
        ]
        for key in to_remove:
            line_id = self._connection_lines.pop(key)
            self._canvas.delete(line_id)

    def get_node(self, quest_id: str) -> Optional[QuestNodeWidget]:
        return self._nodes.get(quest_id)

    # ------------------------------------------------------------------
    # Connection management
    # ------------------------------------------------------------------

    def add_connection(self, from_id: str, to_id: str) -> None:
        """Draw a directed arrow from *from_id* to *to_id*."""
        key = (from_id, to_id)
        if key in self._connection_lines:
            return  # already drawn

        from_node = self._nodes.get(from_id)
        to_node = self._nodes.get(to_id)
        if from_node is None or to_node is None:
            return

        line_id = self._draw_connection(from_node, to_node)
        self._connection_lines[key] = line_id

    def remove_connection(self, from_id: str, to_id: str) -> None:
        """Remove the arrow from *from_id* to *to_id*."""
        key = (from_id, to_id)
        line_id = self._connection_lines.pop(key, None)
        if line_id is not None:
            self._canvas.delete(line_id)

    def update_connections(self, quest_id: str) -> None:
        """Redraw all connection lines involving *quest_id*."""
        for key in list(self._connection_lines.keys()):
            if quest_id in key:
                from_id, to_id = key
                from_node = self._nodes.get(from_id)
                to_node = self._nodes.get(to_id)
                if from_node is None or to_node is None:
                    continue
                self._canvas.delete(self._connection_lines[key])
                self._connection_lines[key] = self._draw_connection(
                    from_node, to_node
                )

    # ------------------------------------------------------------------
    # Selection
    # ------------------------------------------------------------------

    def select_node(self, quest_id: Optional[str]) -> None:
        """Select the given node (or deselect all if *None*)."""
        for nid, node in self._nodes.items():
            node.set_selected(nid == quest_id)

        if self.on_node_selected is not None:
            self.on_node_selected(quest_id)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _draw_connection(
        self, from_node: QuestNodeWidget, to_node: QuestNodeWidget
    ) -> int:
        """Draw an arrow line between two nodes and return its canvas id."""
        fx, fy = from_node.center
        tx, ty = to_node.center
        return self._canvas.create_line(
            fx,
            fy,
            tx,
            ty,
            fill=self.CONNECTION_COLOR,
            width=2,
            arrow=tk.LAST,
            arrowshape=(12, 15, 6),
            tags=("connection",),
        )

    # -- drag handling -----------------------------------------------------

    def _on_button_press(self, event: tk.Event) -> None:
        canvas_x = self._canvas.canvasx(event.x)
        canvas_y = self._canvas.canvasy(event.y)

        # Find which node (if any) was clicked
        for quest_id, node in self._nodes.items():
            if node.contains(canvas_x, canvas_y):
                self._drag_data = {
                    "quest_id": quest_id,
                    "start_x": canvas_x,
                    "start_y": canvas_y,
                    "node_x": node.center[0],
                    "node_y": node.center[1],
                }
                self.select_node(quest_id)
                return

        # Clicked on empty space -- deselect
        self.select_node(None)

    def _on_drag(self, event: tk.Event) -> None:
        if self._drag_data is None:
            return

        canvas_x = self._canvas.canvasx(event.x)
        canvas_y = self._canvas.canvasy(event.y)

        dx = canvas_x - self._drag_data["start_x"]
        dy = canvas_y - self._drag_data["start_y"]

        new_x = self._drag_data["node_x"] + dx
        new_y = self._drag_data["node_y"] + dy

        quest_id = self._drag_data["quest_id"]
        node = self._nodes.get(quest_id)
        if node is not None:
            node.update_position(new_x, new_y)
            self.update_connections(quest_id)

    def _on_button_release(self, event: tk.Event) -> None:
        self._drag_data = None
