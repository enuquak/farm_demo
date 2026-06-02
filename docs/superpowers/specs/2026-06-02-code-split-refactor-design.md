# Code Split Refactoring Design (Updated)

**Date**: 2026-06-02 (Updated from 2026-05-31)
**Status**: In Progress

## Overview

Refactor the two largest monolithic files in the project:
- **Server**: `game_server.cpp` (1645 lines → 947 lines, -42%)
- **Client**: `game_scene.py` (877 lines → 759 lines, -13%)

The refactoring extracted core logic into dedicated modules with clear responsibilities.

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

### Architecture (Actual Implementation)

```
GameServer (infrastructure orchestrator, 947 lines)
  ├── PlayerManager          (260 lines)
  ├── MessageHandler          (message routing framework)
  ├── DBMgrConnectionManager  (831 lines)
  ├── ItemInteractionHandler  (361 lines)
  ├── GameSceneManager        (269 lines) ← extracted
  ├── GameClock               (288 lines) ← extracted
  ├── AdminHandler            ← extracted
  ├── LoginStub               (471 lines) ← replaces original AccountMessageHandler
  ├── OnlineStub              ← new: player online status via Redis
  ├── PlayerIdPool            ← new: ID management with object pool
  ├── QuestManager/Config     ← new: quest system
  ├── GMStub/GmHttpHandler    ← new: GM management system
  └── ServerMonsterManager/CombatHandler ← new: combat system
```

### Deviations from Original Design

| Original Module | Actual Implementation | Reason for Change |
|-----------------|----------------------|-------------------|
| `AccountMessageHandler` | `LoginStub` | Extended responsibilities: includes Redis online status management |
| `PlayerIdGenerator` | `PlayerIdPool` | Object pool pattern for better efficiency |
| None | `OnlineStub` | New requirement: cross-server player online query |
| None | `GMStub`/`GmHttpHandler` | New requirement: GM management system |
| None | `QuestManager` | New requirement: quest system |
| None | `CombatHandler` | New requirement: combat system |

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

### LoginStub (Replaces AccountMessageHandler)

Handles account-related messages with extended Redis online status management.

```cpp
class LoginStub {
public:
    LoginStub(PlayerManager* player_mgr,
              DBMgrConnectionManager* dbmgr_mgr,
              RedisConnection* redis_conn,
              uint32_t server_id,
              PlayerIdPool* id_pool,
              SendToGateFunc send_to_gate);

    void set_clock(GameClock* clock) { clock_ = clock; }

    // Handle account messages (top-level entry point)
    void handle_account_msg(std::shared_ptr<GateSession> session,
                            const std::vector<uint8_t>& payload);

    // Handle EnterGameReq from ClientMessage path
    void handle_enter_game_req(std::shared_ptr<GateSession> session,
                               uint64_t player_id, const std::string& payload);

    // Called when a player goes offline (cleans up Redis)
    void on_player_offline(uint64_t player_id);

private:
    // Account message sub-handlers
    void handle_query_roles(...);
    void handle_create_role(...);
    void handle_account_data(...);
    void handle_account_set(...);
    void handle_enter_game(...);

    // Redis helper: mark player online
    void mark_player_online(uint64_t player_id);

    PlayerManager* player_mgr_;
    DBMgrConnectionManager* dbmgr_mgr_;
    RedisConnection* redis_conn_;
    uint32_t server_id_;
    PlayerIdPool* id_pool_;
    SendToGateFunc send_to_gate_;
    GameClock* clock_ = nullptr;
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

### Unit Tests (Google Test)

| Test File | Coverage |
|-----------|----------|
| `game_clock_test.cpp` | tick, day/week/month events, save/load |
| `game_scene_manager_test.cpp` | create, find, switch, persistence |
| `login_stub_test.cpp` | 5 message types |
| `admin_handler_test.cpp` | shutdown request/response |

---

## Client-Side: Module Extraction

### Architecture (Actual Implementation)

```
GameScene (game loop orchestrator, 759 lines)
  ├── PlayerController         (303 lines) ← extracted
  ├── NetworkMessageDispatcher  (478 lines) ← extracted
  ├── GameRenderer             ← extracted
  ├── DropItemRenderer         ← new: drop item rendering
  ├── SceneManager             (existing)
  ├── InputManager             (220 lines)
  ├── Inventory                (322 lines)
  ├── Connection               (358 lines)
  ├── NPCManager               ← new: NPC management
  ├── DialogEngine             (270 lines) ← new: dialog system
  ├── AffectionSystem          ← new: affection system
  ├── QuestHandler             ← new: quest handling
  ├── NotificationManager      ← new: notification system
  └── CombatSystem/BattleUI    ← new: combat system (optional module)
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

## Implementation Status

### Completed Work

| Category | Task | Status |
|----------|------|--------|
| Server | GameClock extraction | ✅ Done |
| Server | GameSceneManager extraction | ✅ Done |
| Server | AdminHandler extraction | ✅ Done |
| Server | LoginStub (replaces AccountMessageHandler) | ✅ Done |
| Server | GameServer refactoring (1645→947 lines) | ✅ Done |
| Client | PlayerController extraction | ✅ Done |
| Client | NetworkMessageDispatcher extraction | ✅ Done |
| Client | GameRenderer extraction | ✅ Done |
| Client | sprite_data.py creation | ✅ Done |
| Client | Dead code deletion (camera.py, tile_renderer.py, message_handler.py) | ✅ Done |

### Remaining Work

| Category | Task | Priority |
|----------|------|----------|
| Client | Fix `constants.py` missing constants (`HOTBAR_*`, `PANEL_*`, `ITEM_ICON_PALETTE`) | High |
| Server | Unit tests: GameClock | Medium |
| Server | Unit tests: GameSceneManager | Medium |
| Server | Unit tests: LoginStub | Medium |
| Client | Unit tests: PlayerController | Medium |
| Client | Unit tests: NetworkMessageDispatcher | Medium |
| Client | Unit tests: GameRenderer | Medium |
| Verification | All server builds verification (gate_server, game_server, dbmgr) | High |
| Verification | Client import verification | High |

---

## Files Changed

### Server

| Action | File | Lines |
|--------|------|-------|
| New | `game_clock.h`, `game_clock.cpp` | 288 |
| New | `game_scene_manager.h`, `game_scene_manager.cpp` | 269 |
| New | `login_stub.h`, `login_stub.cpp` | 471 |
| New | `admin_handler.h`, `admin_handler.cpp` | - |
| New | `online_stub.h`, `online_stub.cpp` | - |
| New | `player_id_pool.h`, `player_id_pool.cpp` | - |
| New | `gm_stub.h`, `gm_stub.cpp` | 423 |
| New | `gm_http_handler.h`, `gm_http_handler.cpp` | 260 |
| New | `quest_manager.h`, `quest_manager.cpp` | 509 |
| New | `quest_config.h`, `quest_config.cpp` | 178 |
| New | `monster_manager.h`, `monster_manager.cpp` | - |
| New | `combat_handler.h`, `combat_handler.cpp` | - |
| Modify | `game_server.h`, `game_server.cpp` | 947 |
| Modify | `CMakeLists.txt` | - |

### Client

| Action | File | Lines |
|--------|------|-------|
| New | `player_controller.py` | 303 |
| New | `network_dispatcher.py` | 478 |
| New | `game_renderer.py` | - |
| New | `sprite_data.py` | 256 |
| New | `drop_item_renderer.py` | - |
| New | `npc_manager.py` | - |
| New | `dialog_engine.py` | 270 |
| New | `affection_system.py` | - |
| New | `quest_handler.py` | - |
| New | `notification.py`, `ui/notification_manager.py` | - |
| New | `monster_manager.py`, `weapon_manager.py`, `combat_system.py`, `battle_ui.py` | - |
| Modify | `game_scene.py` | 759 |
| Modify | `constants.py` | 337 |
| Modify | `connection.py` | 358 |
| Delete | `camera.py`, `tile_renderer.py`, `message_handler.py` | - |

### Tests to Create

| Category | File | Coverage |
|----------|------|----------|
| Server | `game_clock_test.cpp` | tick, day/week/month events, save/load |
| Server | `game_scene_manager_test.cpp` | create, find, switch, persistence |
| Server | `login_stub_test.cpp` | 5 message types |
| Server | `admin_handler_test.cpp` | shutdown request/response |
| Client | `test_player_controller.py` | Movement, collision, position correction |
| Client | `test_network_dispatcher.py` | Dispatch table, each handler |
| Client | `test_game_renderer.py` | Render call chain |
