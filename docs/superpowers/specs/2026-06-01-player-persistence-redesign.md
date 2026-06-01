# Player Data Persistence Redesign

**Date:** 2026-06-01
**Status:** Draft
**Scope:** game_server (Player, PlayerManager, GameServer)

---

## 1. Problem

Current player data is only persisted on player disconnect/leave events. If the game server crashes, all dirty player data since the last save event is lost. There is no periodic auto-save, and no way for business logic to trigger an incremental save of specific fields.

## 2. Goals

1. **Periodic full save** — each Player entity owns a 5-minute libevent timer; when it fires and the entity is dirty, write all data to MongoDB via `SET_ALL`.
2. **Exit save** — already exists (`remove_player_with_save`), unchanged.
3. **Incremental save** — business code can call `player->save()` or `player->save_field("gold")` to immediately persist dirty fields via `SET` operations.

## 3. Design

### 3.1 Dirty Field Tracking

Player currently has a single `bool dirty_` flag. Extend with a field-name set:

```cpp
// player.h
#include <unordered_set>

class Player {
    // ... existing members ...
    bool dirty_ = false;
    std::unordered_set<std::string> dirty_fields_;  // NEW
};
```

Each setter marks both `dirty_ = true` and inserts the field name into `dirty_fields_`:

```cpp
void Player::set_gold(int64_t gold) {
    if (player_data_.gold != gold) {
        player_data_.gold = gold;
        dirty_ = true;
        dirty_fields_.insert("gold");
    }
}
```

Field name mapping (setter → field string):

| Setter | Field key |
|--------|-----------|
| `set_role_name` | `"role_name"` |
| `set_level` | `"level"` |
| `set_gold` | `"gold"` |
| `set_experience` | `"experience"` |
| `set_pos_x/y/z` | `"pos_x"`, `"pos_y"`, `"pos_z"` |
| `set_energy` | `"energy"` |
| `set_scene_id` | `"scene_id"` |
| `set_inventory` | `"inventory"` |
| `set_farm_state` | `"farm_state"` |
| `set_extra_data` | `"extra_data"` |

`set_player_data()` (bulk set) marks all fields as dirty.

### 3.2 Player Save Interface

Add three public methods to Player:

```cpp
// Flush all dirty fields as individual SET requests.
// If no dirty fields, no-op.
void save();

// Save a specific field immediately via SET.
void save_field(const std::string& field);

// Full save via SET_ALL (used by timer and exit).
void save_full();
```

Behavior:
- `save()` — if `dirty_fields_` is non-empty, sends one `SET` request per dirty field (e.g. `SET data.gold=500`, `SET data.level=5`), then clears `dirty_` and `dirty_fields_`. This is the "flush all dirty" path for business code.
- `save_field("gold")` — sends a single `SET` for `data.gold` with the current value. Removes `"gold"` from `dirty_fields_`. If `dirty_fields_` becomes empty, clears `dirty_`.
- `save_full()` — sends a single `SET_ALL` with the full PlayerBizData. Used by the timer and exit path. Clears `dirty_` and `dirty_fields_`.

### 3.3 Entity-Owned Timer

Each Player owns a libevent timer that fires every 5 minutes:

```cpp
// player.h
class Player {
    // ... existing members ...
    struct event* save_timer_ = nullptr;  // libevent timer
    struct event_base* base_ = nullptr;   // non-owning pointer

public:
    void start_save_timer(struct event_base* base);
    void stop_save_timer();

private:
    static void on_save_timer(evutil_socket_t fd, short events, void* ctx);
    void handle_save_timer();
};
```

`start_save_timer(base)` is called by PlayerManager after the Player is created. `stop_save_timer()` is called before the Player is destroyed.

Timer callback calls `save()` which does a full `SET_ALL` if dirty.

### 3.4 PlayerManager Changes

```cpp
// player_manager.h
class PlayerManager {
    // ... existing members ...
    DBMgrConnectionManager* dbmgr_mgr() const { return dbmgr_mgr_; }
};
```

PlayerManager exposes `dbmgr_mgr_` so Player can send save requests directly. Alternatively, Player can hold its own pointer to `DBMgrConnectionManager` — the cleaner option.

**Preferred approach:** Player stores a `DBMgrConnectionManager*` pointer (set during construction or via a setter), so it can send save requests autonomously without going through PlayerManager.

```cpp
// player.h
class Player {
public:
    Player(uint64_t player_id, GateSession* gate_session,
           DBMgrConnectionManager* dbmgr_mgr);
    // ...
private:
    DBMgrConnectionManager* dbmgr_mgr_;  // non-owning
};
```

### 3.5 GameServer Integration

No changes needed in GameServer. The timer is managed entirely within Player. PlayerManager passes `dbmgr_mgr_` when creating Player objects.

### 3.6 Save Callback

When the save response arrives from DBMgr, the existing `handle_player_data_saved` callback in PlayerManager handles logging. For entity-driven saves, Player can handle its own callback internally:

```cpp
void Player::on_save_response(int32_t code) {
    if (code == 0) {
        SPDLOG_INFO("[Player]Data saved for player_id={}", player_id_);
    } else {
        SPDLOG_ERROR("[Player]Data save failed for player_id={} code={}", player_id_, code);
        // Do NOT clear dirty flags on failure — retry on next timer tick
    }
}
```

### 3.7 Data Serialization

Current `save_player_data()` in PlayerManager serializes PlayerBizData into a `PlayerData` protobuf. This has a problem: `PlayerData` proto doesn't include `gold`, `inventory`, `farm_state`, or `extra_data`.

**Fix:** Use JSON serialization for the data field instead of protobuf, consistent with how DBMgr stores it (`data` subdocument in MongoDB). This also makes field-level `SET` operations cleaner — the key maps directly to the JSON field name.

```cpp
// In Player::save()
nlohmann::json data;
data["role_name"] = player_data_.role_name;
data["level"] = player_data_.level;
data["gold"] = player_data_.gold;
data["experience"] = player_data_.experience;
data["pos_x"] = player_data_.pos_x;
data["pos_y"] = player_data_.pos_y;
data["pos_z"] = player_data_.pos_z;
data["energy"] = player_data_.energy;
data["scene_id"] = player_data_.scene_id;
data["inventory"] = player_data_.inventory;   // already JSON string
data["farm_state"] = player_data_.farm_state;  // already JSON string
data["extra_data"] = player_data_.extra_data;  // already JSON string

std::string value = data.dump();
dbmgr_mgr_->send_player_data_req(player_id_,
    static_cast<int32_t>(farm::PlayerDataOp::SET_ALL),
    "", value, callback);
```

For `save()` (flush all dirty fields):
```cpp
for (const auto& field : dirty_fields_) {
    std::string value = get_field_json(field);  // serialize field to JSON
    dbmgr_mgr_->send_player_data_req(player_id_,
        static_cast<int32_t>(farm::PlayerDataOp::SET),
        field, value, callback);
}
dirty_fields_.clear();
dirty_ = false;
```

For `save_field("gold")`:
```cpp
nlohmann::json val = player_data_.gold;
std::string value = val.dump();
dbmgr_mgr_->send_player_data_req(player_id_,
    static_cast<int32_t>(farm::PlayerDataOp::SET),
    "gold", value, callback);
dirty_fields_.erase("gold");
if (dirty_fields_.empty()) dirty_ = false;
```

### 3.8 Dirty Flag Reset

- After `save_full()` (timer or exit): clear `dirty_` and `dirty_fields_`.
- After `save()` (flush all dirty): clear `dirty_` and `dirty_fields_` after all SET requests sent.
- After `save_field(key)`: remove `key` from `dirty_fields_`. If `dirty_fields_` empty, clear `dirty_`.
- On save failure (DBMgr returns error): do NOT clear flags. The next timer tick will retry.

## 4. Data Flow Summary

```
Business logic calls player->set_gold(500)
  → dirty_ = true, dirty_fields_ = {"gold"}

Option A: Wait for timer (5 min)
  → Player::on_save_timer fires
  → save_full() → SET_ALL (full JSON) → clear dirty

Option B: Business calls player->save()
  → for each field in dirty_fields_: SET data.<field> = <value>
  → clear dirty

Option C: Business calls player->save_field("gold")
  → SET (data.gold = 500) → remove "gold" from dirty_fields_

Player exits:
  → PlayerManager::remove_player_with_save()
  → save_full() → SET_ALL → clear dirty → destroy Player
```

## 5. Files to Modify

| File | Changes |
|------|---------|
| `player.h` | Add `dirty_fields_`, `save_timer_`, `base_`, `dbmgr_mgr_` members; add `save()`, `save_field()`, `save_full()`, `start_save_timer()`, `stop_save_timer()` methods |
| `player.cpp` | Implement new methods; update all setters to mark `dirty_fields_`; implement timer callback |
| `player_manager.h` | Expose `dbmgr_mgr()` accessor |
| `player_manager.cpp` | Pass `dbmgr_mgr_` to Player constructor; call `start_save_timer()` after creation; call `stop_save_timer()` before destruction |

## 6. Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| Timer fires after Player destroyed | `stop_save_timer()` in destructor ensures cleanup |
| Save request in-flight when Player exits | Callback checks if Player still exists (request_id tracking) |
| Many players → many timers | libevent timers are lightweight (O(log n) heap); acceptable for typical online counts |
| JSON field name mismatch with DB | Use consistent snake_case keys; field names are compile-time constants |
