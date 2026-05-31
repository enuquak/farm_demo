# Code Split Refactoring Design

**Date**: 2026-05-31
**Status**: Approved

## Overview

Refactor the two largest monolithic files in the project:
- **Server**: `game_server.cpp` (1645 lines) → 4 extracted modules + orchestrator
- **Client**: `game_scene.py` (877 lines) → 3 extracted modules + orchestrator

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Communication pattern | Callback injection (std::function / Python functions) | Simple, debuggable, fits project scale |
| Dead code (camera.py, tile_renderer.py, message_handler.py) | Delete directly | Unused in game loop |
| Testing | Compile + comprehensive unit tests for all new modules | Ensure correctness |
| Order | Server first, then client | Server changes are larger and more complex |
| Clock design | Generic time scheduler with callback registration | Extensible (day/week/month) |
| Force sleep | Simplified: server notifies, client handles animation | Reduced server complexity |

---

## Server-Side: Module Extraction

### Architecture

```
GameServer (infrastructure orchestrator, ~500 lines)
  ├── PlayerManager          (existing)
  ├── MessageHandler          (existing)
  ├── DBMgrConnectionManager  (existing)
  ├── ItemInteractionHandler  (existing)
  ├── GameSceneManager        (new: ~180 lines)
  ├── GameClock               (new: ~150 lines)
  ├── AccountMessageHandler   (new: ~280 lines)
  └── AdminHandler            (new: ~90 lines)
```

### GameClock: Generic Time Scheduler

GameClock is a pure time scheduler. It manages time progression and fires registered callbacks at day/week/month boundaries. It does NOT handle any business logic.

```cpp
enum class TimeEvent { DAY_UPDATE, WEEK_UPDATE, MONTH_UPDATE };
using TimeCallback = std::function<void(int32_t day, int32_t week, int32_t month)>;

class GameClock {
public:
    void on(TimeEvent event, TimeCallback callback);
    void tick();  // called every second
    void save_data();
    void load_data();
    
    int32_t day() const;
    int32_t week() const;   // (day - 1) / 7 + 1
    int32_t month() const;  // (day - 1) / 30 + 1
    int32_t time_slot() const;  // 0-39, 60 seconds each, 40 = 1 day

private:
    int32_t day_ = 1;
    int32_t time_slot_ = 0;
    double elapsed_ = 0.0;
    std::unordered_map<TimeEvent, std::vector<TimeCallback>> callbacks_;
};
```

GameServer registers callbacks:
```cpp
clock_->on(TimeEvent::DAY_UPDATE, [this](int32_t day, int32_t week, int32_t month) {
    // Notify clients of day change, client handles animation
    broadcast_clock_sync(day, time_slot);
});
```

Time calculation:
- 1 tick = 1 second
- 60 ticks = 1 time_slot
- 40 time_slots = 1 day (40 real minutes)
- 7 days = 1 week
- 30 days = 1 month

### GameSceneManager

Extracts scene lifecycle management from GameServer.

```cpp
class GameSceneManager {
public:
    GameSceneManager(DBMgrConnectionManager* dbmgr_mgr);
    
    SceneState* get_or_create_scene(const std::string& scene_id);
    void handle_scene_change_req(uint64_t player_id, const uint8_t* payload, size_t payload_len,
                                  PlayerManager* player_mgr);
    void save_all();
    void save_data(const std::string& scene_id);
    void load_data(const std::string& scene_id);

    using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;
    void set_send_callback(SendGameMsgFunc func);

private:
    std::unordered_map<std::string, std::unique_ptr<SceneState>> scenes_;
    DBMgrConnectionManager* dbmgr_mgr_;
    SendGameMsgFunc send_func_;
};
```

### AccountMessageHandler

Extracts the 265-line `handle_account_msg()` into a class with 5 sub-methods.

```cpp
class AccountMessageHandler {
public:
    AccountMessageHandler(DBMgrConnectionManager* dbmgr_mgr,
                          PlayerManager* player_mgr,
                          PlayerIdGenerator* id_gen);
    void handle(std::shared_ptr<GateSession> session, const std::vector<uint8_t>& payload);

    using SendToGateFunc = std::function<void(std::shared_ptr<GateSession>, uint32_t, const std::string&)>;
    using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;
    void set_send_callbacks(SendToGateFunc gate_func, SendGameMsgFunc game_func);

private:
    void handle_query_roles(...);
    void handle_create_role(...);
    void handle_account_data(...);
    void handle_account_set(...);
    void handle_enter_game(...);
};
```

### AdminHandler

Extracts admin message handling (shutdown request/response).

```cpp
class AdminHandler {
public:
    AdminHandler(PlayerManager* player_mgr, GameSceneManager* scene_mgr,
                 GameClock* clock, DBMgrConnectionManager* dbmgr_mgr);
    void handle(std::shared_ptr<GateSession> session, uint32_t msg_id,
                const std::vector<uint8_t>& payload);
    void set_stop_callback(std::function<void()> stop_func);

private:
    void handle_shutdown(...);
    void handle_shutdown_resp(...);
};
```

### Legacy Cleanup

- Remove `world_state_`, `drop_manager_`, `crop_system_` from GameServer (managed by ItemInteractionHandler)
- Move `player_id_gen_` to AccountMessageHandler
- Change auto-pickup `drop_manager_` to `item_handler_.drops()`

### Unit Tests (Google Test)

| Test File | Coverage |
|-----------|----------|
| `game_clock_test.cpp` | tick, day/week/month events, save/load |
| `game_scene_manager_test.cpp` | create, find, switch, persistence |
| `account_message_handler_test.cpp` | 5 message types |
| `admin_handler_test.cpp` | shutdown request/response |

---

## Client-Side: Module Extraction

### Architecture

```
GameScene (game loop orchestrator, ~200 lines)
  ├── PlayerController         (new: ~200 lines)
  ├── NetworkMessageDispatcher  (new: ~150 lines)
  ├── GameRenderer             (new: ~120 lines)
  ├── SceneManager             (existing)
  ├── InputManager             (existing)
  ├── Inventory                (existing)
  └── Connection               (existing)
```

### PlayerController

Extracts movement, collision, position correction from GameScene.

```python
class PlayerController:
    def __init__(self, connection, tmx_map, player_sprite, scene_manager):
        self.facing = Direction.DOWN
        self.correction_target = None
        self.send_timer = 0.0

    def handle_input(self, dt, input_mgr):
        """WASD movement, collision detection, facing update"""

    def update_position_sending(self, dt):
        """Send position to server periodically"""

    def start_correction(self, server_x, server_y):
        """Server-authoritative position correction"""
```

### NetworkMessageDispatcher

Dict-based message dispatch table.

```python
class NetworkMessageDispatcher:
    def __init__(self):
        self.handlers = {
            MSG_ID_MAP_DATA: self._handle_map_data,
            MSG_ID_POSITION_UPDATE: self._handle_position,
            MSG_ID_ITEM_USE_RESP: self._handle_item_resp,
            MSG_ID_SCENE_CHANGE_RESP: self._handle_scene_resp,
            MSG_ID_CLOCK_SYNC: self._handle_clock_sync,
        }
        self.on_force_sleep = None  # callback for GameScene

    def dispatch_pending(self, connection):
        for msg_id, payload in connection.recv_all_messages():
            handler = self.handlers.get(msg_id)
            if handler:
                handler(payload)
```

### GameRenderer

Coordinates rendering of all UI components.

```python
class GameRenderer:
    def __init__(self, screen):
        self.hud = HUD()
        self.energy_bar = EnergyBar(screen)
        self.time_hud = TimeHUD(screen)
        self.exhaustion_modal = ExhaustionModal(screen)

    def render(self, map_group, player_sprite, scene_transition=None):
        """Render one complete frame"""
```

### constants.py Cleanup

1. **Split**: Move `OBJECT_SPRITES` (~300 lines of pixel data) → `sprite_data.py`
2. **Add missing constants**: `HOTBAR_*`, `PANEL_*`, `ITEM_ICON_PALETTE`
3. **Delete dead code**: `camera.py`, `tile_renderer.py`, `message_handler.py`
4. **Clean connection.py**: Move `send_heartbeat`/`send_login_request` to callers

### Unit Tests (pytest)

| Test File | Coverage |
|-----------|----------|
| `test_player_controller.py` | Movement, collision, position correction |
| `test_network_dispatcher.py` | Dispatch table, each handler |
| `test_game_renderer.py` | Render call chain |

---

## Implementation Order

### Phase 1: Server (sequential)
1. GameSceneManager (no dependencies)
2. GameClock (no dependencies on other new modules)
3. AccountMessageHandler (independent)
4. AdminHandler (depends on 1-3)
5. GameServer refactor + legacy cleanup
6. Unit tests for all new modules
7. Build verification (CMake Release)

### Phase 2: Client (sequential)
1. Fix constants.py + create sprite_data.py
2. PlayerController extraction
3. NetworkMessageDispatcher extraction
4. GameRenderer extraction
5. GameScene refactor
6. Dead code deletion
7. Unit tests for all new modules
8. Import verification

---

## Files Changed

### Server
| Action | File |
|--------|------|
| New | `game_clock.h`, `game_clock.cpp` |
| New | `game_scene_manager.h`, `game_scene_manager.cpp` |
| New | `account_message_handler.h`, `account_message_handler.cpp` |
| New | `admin_handler.h`, `admin_handler.cpp` |
| New | `game_clock_test.cpp`, `game_scene_manager_test.cpp`, `account_message_handler_test.cpp`, `admin_handler_test.cpp` |
| Modify | `game_server.h`, `game_server.cpp` |
| Modify | `CMakeLists.txt` |

### Client
| Action | File |
|--------|------|
| New | `player_controller.py` |
| New | `network_dispatcher.py` |
| New | `game_renderer.py` |
| New | `sprite_data.py` |
| New | `test_player_controller.py`, `test_network_dispatcher.py`, `test_game_renderer.py` |
| Modify | `game_scene.py`, `constants.py`, `connection.py`, `__init__.py` |
| Modify | `heartbeat.py`, `login_flow.py` |
| Delete | `camera.py`, `tile_renderer.py`, `message_handler.py` |
