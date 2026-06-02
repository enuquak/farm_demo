# Quest UI Design

**Date:** 2026-06-03
**Status:** Draft
**Author:** AI Assistant

## 1. Overview

This document describes the client-side Quest UI design for the farm_demo project. The Quest System server-side (QuestManager, EventBus, QuestConfig) and network handler (QuestHandler) are already implemented. This design covers the missing visual layer: how players see, track, and interact with quests in the game client.

**Key decisions:**
- Hybrid layout: HUD tracker bar (always visible) + full quest panel (Q key toggle)
- Notifications: Toast popups + tracker bar real-time sync
- Quest acceptance: NPC dialog (main/side quests) + auto-trigger (achievement quests)
- Panel layout: Left list (grouped by status) + right detail view

## 2. File Structure

```
scripts/client/
├── quest_handler.py          # Existing — network message send/recv
├── quest_data.py             # New — client-side quest data model
└── ui/
    ├── quest_tracker.py      # New — HUD tracker bar (always visible, right side)
    └── quest_panel.py        # New — full quest panel (Q key toggle, centered modal)
```

**Data flow:**
```
Server → quest_handler.py (network) → quest_data.py (data model) → quest_tracker.py / quest_panel.py (UI)
```

## 3. Data Model (`quest_data.py`)

### 3.1 QuestData Class

```python
class QuestData:
    """Client-side quest data manager."""

    def __init__(self):
        self._quests: Dict[str, QuestInfo] = {}

    # --- Data sync (from quest_handler) ---
    def sync_from_server(self, task_infos: dict):         # Full sync on login
    def update_progress(self, quest_id, progress: dict):   # Incremental progress update
    def add_quest(self, quest_info):                       # New quest accepted
    def remove_quest(self, quest_id):                      # Abandoned or completed

    # --- Query interface (for UI) ---
    def get_active_quests(self) -> List[QuestInfo]:        # In-progress quests
    def get_available_quests(self) -> List[QuestInfo]:     # Available (not accepted) quests
    def get_completed_quests(self) -> List[QuestInfo]:     # Completed quests
    def get_tracked_quests(self, limit=3) -> List[QuestInfo]:  # Quests shown in tracker
    def get_quest(self, quest_id) -> Optional[QuestInfo]:
    def get_npc_quest_status(self, npc_id: str) -> Optional[str]:  # '!', '?', or None
```

### 3.2 Data Structures

```python
@dataclass
class QuestInfo:
    quest_id: str
    name: str
    description: str
    state: str           # "in_progress", "completed", "available"
    objectives: List[ObjectiveInfo]
    rewards: RewardInfo
    trigger_npc: Optional[str]   # NPC that offers this quest
    submit_npc: Optional[str] = None  # NPC to submit to (if different, optional)
    accept_time: int = 0

@dataclass
class ObjectiveInfo:
    obj_id: str
    type: str            # "collect_item", "plant_crop", "harvest_crop", etc.
    target: str
    current: int
    required: int
    description: str

@dataclass
class RewardInfo:
    gold: int = 0
    exp: int = 0
    items: List[Tuple[str, int]] = field(default_factory=list)  # (item_id, count)
```

### 3.3 Design Notes

- `QuestData` is a pure data layer with no pygame dependency
- Data source is `quest_handler.py` network callbacks; no direct server access
- Tracker shows top 3 `in_progress` quests by default

## 4. HUD Tracker Bar (`quest_tracker.py`)

### 4.1 Layout

Position: Right side of screen, 40px from top (below TimeHUD), 200px wide.

```
┌─────────────────────┐
│ 📜 追踪中            │  ← Title bar
│                     │
│ 🌾 初识农耕          │  ← Quest name (gold)
│ ████████░░ 2/3      │  ← Progress bar + numbers
│                     │
│ 🍅 番茄之路          │
│ ██░░░░░░░░ 1/5      │
│                     │
│ 按 Q 展开            │  ← Hint text
└─────────────────────┘
```

### 4.2 Behavior

- Always visible (no show/hide toggle)
- Progress update: numbers flash green for 0.5 seconds
- Quest complete: entire row highlights gold, fades out after 1 second
- Click a quest row → opens full panel with that quest selected
- Maximum 3 in-progress quests displayed

### 4.3 Interface

```python
class QuestTracker:
    def __init__(self, screen_width: int, screen_height: int):
        self._screen_width = screen_width
        self._screen_height = screen_height
        self._animations: List[TrackerAnimation] = []
        ...

    def update(self, quest_data: QuestData) -> None:
        """Refresh displayed quests from data model."""
        ...

    def draw(self, screen: pygame.Surface) -> None:
        """Render tracker bar."""
        ...

    def handle_event(self, event) -> bool:
        """Handle mouse clicks on quest rows. Returns True if consumed."""
        ...
```

### 4.4 Render Order

In `GameRenderer.render()`: after ChatPanel, before DialogUI.

## 5. Full Quest Panel (`quest_panel.py`)

### 5.1 Trigger

- Q key opens/closes the panel
- When opened: `InputManager.set_ui_blocking(True)`
- When closed: `InputManager.set_ui_blocking(False)`
- Blocked while ChatPanel input is active

### 5.2 Layout

480x380px, centered on 800x600 screen, semi-transparent background `(20, 30, 20, 220)`.

```
┌──────────────────────────────────────────────────┐
│                    📜 任务面板                     │
├────────────────────┬─────────────────────────────┤
│  ── 进行中 (2) ──  │  🌾 初识农耕                 │
│ ▶ 🌾 初识农耕      │  学习基本的种植技巧           │
│   🍅 番茄之路      │                             │
│                    │  目标:                       │
│  ── 可接取 (1) ──  │  ☐ 种植3个小麦        2/3   │
│   ⛏ 矿工入门      │  ☐ 收获3个小麦        1/3   │
│                    │                             │
│  ── 已完成 (3) ──  │  奖励:                       │
│   ▸ 已完成 (3)     │  💰100  ⭐50  🌱番茄种子x5  │
│                    │                             │
│                    │  ┌─────────┐                │
│                    │  │ 放弃任务 │                │
│                    │  └─────────┘                │
├────────────────────┴─────────────────────────────┤
│              按 Q 或 ESC 关闭                      │
└──────────────────────────────────────────────────┘
```

### 5.3 Left Panel: Quest List

- Grouped by status: In Progress → Available → Completed (collapsed with count)
- Each row: icon + quest name, selected row highlighted
- Mouse click to select, Up/Down arrow keys to navigate
- Completed group collapsed by default, click to expand

### 5.4 Right Panel: Quest Detail

- Quest name (gold, large font)
- Description text
- Objective list: checkbox style + progress numbers
- Reward preview
- Action buttons:
  - Available quest → "接取任务" button → `quest_handler.request_accept_quest()`
  - In-progress quest → "放弃任务" button → `quest_handler.request_abandon_quest()`
  - Completed quest → No button, shows "已完成 ✓"

### 5.5 Interface

```python
class QuestPanel:
    def __init__(self, screen_width: int, screen_height: int, input_manager):
        self._visible = False
        self._selected_quest_id: Optional[str] = None
        self._quest_data: Optional[QuestData] = None
        self._input_manager = input_manager
        ...

    def show(self) -> None:
        """Open panel, block game input."""
        self._visible = True
        self._input_manager.set_ui_blocking(True)

    def hide(self) -> None:
        """Close panel, unblock game input."""
        self._visible = False
        self._input_manager.set_ui_blocking(False)

    def toggle(self) -> None:
        ...

    def select_quest(self, quest_id: str) -> None:
        """Select a specific quest (called by tracker click)."""
        ...

    def handle_event(self, event) -> bool:
        """Handle Q/ESC/mouse/keyboard. Returns True if consumed."""
        ...

    def draw(self, screen: pygame.Surface) -> None:
        """Render panel (only if visible)."""
        ...
```

## 6. System Integration

### 6.1 GameScene Integration

**Initialization (`GameScene.__init__`):**
```python
self._quest_data = QuestData()
self._quest_tracker = QuestTracker(self.WINDOW_WIDTH, self.WINDOW_HEIGHT)
self._quest_panel = QuestPanel(self.WINDOW_WIDTH, self.WINDOW_HEIGHT, self._input_manager)
```

**Event loop priority chain (`GameScene.run()`):**
```
ExhaustionModal → ChatPanel → QuestPanel → ESC → Game logic
```

Add Q key binding:
```python
if event.type == pygame.KEYDOWN and event.key == pygame.K_q:
    if not self._chat_panel.is_input_active:
        self._quest_panel.toggle()
```

### 6.2 GameRenderer Integration

**Render order (in `GameRenderer.render()`):**
```
1.  Clear screen
2.  Map + sprites
3.  Drop items
4.  Monsters
5.  NPC bubbles
6.  HUD
7.  Energy bar
8.  Time HUD
9.  Chat panel
10. Quest tracker      ← NEW
11. Dialog UI
12. Battle UI
13. Exhaustion modal
14. Notifications
15. Quest panel         ← NEW (modal level)
16. Scene transition
17. display.flip()
```

### 6.3 Quest Handler Integration

Sync data in GameScene quest callbacks:
```python
def _on_quest_sync_notify(self, notify):
    self._quest_handler.handle_quest_sync_notify(notify)
    self._quest_data.sync_from_server(self._quest_handler.active_quests)

def _on_quest_progress_notify(self, notify):
    self._quest_handler.handle_quest_progress_notify(notify)
    self._quest_data.update_progress(notify.quest_id, dict(notify.progress))
    self._show_quest_progress_toast(notify.quest_id, notify.progress)

def _on_quest_accept_resp(self, resp):
    self._quest_handler.handle_quest_accept_resp(resp)
    if resp.code == 0:
        self._quest_data.add_quest(resp.task_info)
        self._show_quest_accept_toast(resp.task_info.task_id)
```

### 6.4 Toast Notification Integration

Reuse existing `NotificationManager`:
```python
def _show_quest_progress_toast(self, quest_id, progress):
    quest = self._quest_data.get_quest(quest_id)
    if quest:
        for obj in quest.objectives:
            if obj.obj_id in progress:
                msg = f"🌾 {obj.description} {obj.current}/{obj.required}"
                self._notification_manager.show_toast(msg)
                break

def _show_quest_complete_toast(self, quest_id, rewards):
    quest = self._quest_data.get_quest(quest_id)
    msg = f"🎉 任务完成：{quest.name}"
    if rewards.gold > 0: msg += f"  💰+{rewards.gold}"
    if rewards.exp > 0: msg += f"  ⭐+{rewards.exp}"
    self._notification_manager.show_toast(msg)
```

## 7. NPC Quest Indicators

### 7.1 Bubble Indicators

When an NPC has an available quest, show an indicator above their head:

- `!` — Gold circle with exclamation mark, bouncing animation
- `?` — Blue circle with question mark (quest ready to submit)

Implementation: Add new bubble type in `BubbleUI`.

### 7.2 NPC Dialog Quest Flow

```
Player presses Space near NPC → DialogEngine.start_dialog()
  → Check if NPC has available quest
  → If yes: insert quest offer node into dialog
    → Player selects "Accept" → quest_handler.request_accept_quest(quest_id)
    → Server returns QuestAcceptResp → quest_data.add_quest()
    → Tracker bar auto-updates
  → If no: normal dialog
```

### 7.3 Dialog Node Extension

Extend `npc_dialog.json` with quest references:
```json
{
  "node_id": "farmer_quest_offer",
  "text": "你已经学会了种小麦，试试番茄吧！",
  "quest_id": "quest_002",
  "options": [
    {"text": "接受任务", "action": "accept_quest"},
    {"text": "稍后再来", "action": "close"}
  ]
}
```

`DialogEngine` handles `accept_quest` action via callback to GameScene.

## 8. Error Handling

| Scenario | Handling |
|---|---|
| Network disconnect loses quest data | Server pushes `QuestSyncNotify` on reconnect |
| Quest accept fails (prerequisites not met) | Toast shows error, panel refreshes |
| Quest abandon fails | Toast shows error, UI unchanged |
| Quest data corruption | Graceful degradation: show empty list, wait for next sync |
| Panel open during scene change | Auto-close panel, release UI blocking |

## 9. Performance Considerations

- Tracker: renders max 3 quest rows per frame, minimal overhead
- Panel: only renders when visible, zero overhead when hidden
- `QuestData`: pure in-memory data, no IO operations
- Toast: reuses existing notification system, no additional overhead

## 10. Testing Scenarios

1. **Login sync**: Server pushes quest data → tracker displays in-progress quests
2. **Progress update**: Player performs action (plant wheat) → Toast popup → tracker numbers update
3. **Quest complete**: All objectives met → completion Toast → tracker highlights → panel status changes
4. **Panel interaction**: Press Q → view list and detail → press ESC to close
5. **NPC quest**: NPC shows `!` indicator → dialog offers quest → accept → tracker adds new quest
6. **Abandon quest**: Panel → select in-progress quest → abandon → tracker removes quest
