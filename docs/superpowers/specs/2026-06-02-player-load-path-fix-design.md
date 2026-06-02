# Player Load Path Fix — Serialization Mismatch & Field Completeness

**Date:** 2026-06-02
**Status:** Draft
**Scope:** game_server (Player, PlayerManager), common/proto (player.proto)
**Follows:** 2026-06-01-player-persistence-redesign (Phase 1 — dirty tracking & auto-save)

---

## 1. Problem

The Phase 1 persistence redesign changed the save path to use JSON serialization (`get_all_data_json()`), but the load path in `PlayerManager::handle_player_data_loaded()` still attempts to parse the response as protobuf (`PlayerData::ParseFromString()`). Since MongoDB stores the data as a BSON subdocument (originating from JSON), the protobuf parse always fails, causing all player data to fall back to defaults on server restart.

**Root cause:** Save writes JSON → MongoDB stores BSON → Load reads BSON → tries protobuf parse → fails → defaults used.

**Secondary issue:** Even if the parse succeeded, the protobuf-to-struct conversion only maps 7 of 20+ fields, hardcoding `gold=0` and omitting all combat fields and `task_infos`.

**Impact:** Any server restart causes all player data loss (gold, inventory, farm state, combat stats, quest progress).

---

## 2. Goals

1. **Fix the load path** — parse DBMgr response as JSON instead of protobuf
2. **Complete field coverage** — map all 20 PlayerBizData fields including combat and task_infos
3. **Encapsulate serialization** — add `Player::load_from_json()` symmetric with `Player::get_all_data_json()`
4. **Clean up dead code** — remove unused protobuf include from player_manager.cpp
5. **Enhance observability** — structured logging for load failures, data corruption, and field-level issues
6. **Update proto** — add missing fields to PlayerData for client-server communication

---

## 3. Design

### 3.1 Player::load_from_json()

Add a new public method to Player that deserializes a JSON string into PlayerBizData, symmetric with the existing `get_all_data_json()`.

```cpp
// player.h — public section
bool load_from_json(const std::string& json_str);
```

**Implementation:**

```cpp
bool Player::load_from_json(const std::string& json_str) {
    try {
        auto json = nlohmann::json::parse(json_str);

        PlayerBizData data;
        data.role_name = json.value("role_name", "");
        data.level = json.value("level", 1);
        data.gold = json.value("gold", int64_t(0));
        data.experience = json.value("experience", int64_t(0));
        data.pos_x = json.value("pos_x", 0.0f);
        data.pos_y = json.value("pos_y", 0.0f);
        data.pos_z = json.value("pos_z", 0.0f);
        data.energy = json.value("energy", 100);
        data.scene_id = json.value("scene_id", "");

        // JSON string fields: object → dump(), string → use directly
        auto load_json_str = [&json](const std::string& key) -> std::string {
            if (json.contains(key)) {
                auto& v = json[key];
                if (v.is_string()) return v.get<std::string>();
                return v.dump();
            }
            return "{}";
        };
        data.inventory = load_json_str("inventory");
        data.farm_state = load_json_str("farm_state");
        data.extra_data = load_json_str("extra_data");
        data.task_infos = load_json_str("task_infos");
        data.equipped_weapon = load_json_str("equipped_weapon");

        // Combat fields
        data.max_hp = json.value("max_hp", 100);
        data.current_hp = json.value("current_hp", 100);
        data.attack_power = json.value("attack_power", 0);
        data.defense_power = json.value("defense_power", 0);
        data.combat_exp = json.value("combat_exp", 0);
        data.combat_level = json.value("combat_level", 1);

        // Log missing critical fields
        static const std::vector<std::string> critical_fields = {
            "role_name", "level", "gold", "scene_id"
        };
        for (const auto& field : critical_fields) {
            if (!json.contains(field)) {
                SPDLOG_WARN("[Player]Missing field '{}' in loaded data for player_id={}",
                            field, player_id_);
            }
        }

        set_player_data(std::move(data));
        clear_dirty();  // loaded data is not dirty
        return true;

    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Player]load_from_json failed for player_id={} error={}",
                     player_id_, e.what());
        return false;
    }
}
```

**Key design decisions:**
- Uses `json.value("key", default)` which returns the default on missing OR type-mismatch fields — safe and self-healing
- JSON string fields (inventory, farm_state, etc.) handle both object and string representations — compatible with old and new data formats
- Calls `clear_dirty()` after loading — data from DB represents current state, not pending changes
- Logs missing critical fields as warnings — helps detect data migration issues

### 3.2 PlayerManager Load Path Rewrite

Replace the protobuf parsing in `handle_player_data_loaded()` with a call to `Player::load_from_json()`.

**Before (broken):**
```cpp
std::string data_str(reinterpret_cast<const char*>(value_data), value_len);
farm::PlayerData player_data;
if (player_data.ParseFromString(data_str)) {
    PlayerBizData data;
    data.role_name = player_data.role_name();
    data.level = player_data.level();
    data.gold = 0;  // hardcoded!
    data.experience = player_data.exp();
    // ... only 7 fields mapped
    player->set_player_data(std::move(data));
}
```

**After (fixed):**
```cpp
std::string data_str(reinterpret_cast<const char*>(value_data), value_len);

if (player->load_from_json(data_str)) {
    player->set_data_state(PlayerBizDataState::LOADED);
    player->start_save_timer(base_);
    SPDLOG_INFO("[Player]Data loaded from DB for player_id={}", player_id);
} else {
    SPDLOG_ERROR("[Player]Corrupted data for player_id={}, falling back to defaults", player_id);
    player->init_default_data();
    player->set_data_state(PlayerBizDataState::LOADED);
    player->start_save_timer(base_);
    // Save defaults immediately to overwrite corrupted data
    player->save_full();
}
```

**Behavior changes:**

| Scenario | Before | After |
|----------|--------|-------|
| Valid JSON data | ❌ Parse fails → defaults | ✅ All 20 fields loaded |
| Corrupted data | ❌ Silent default fallback | ✅ Defaults + immediate save + ERROR log |
| Partial data (old format) | ❌ Missing fields hardcoded to 0 | ✅ Missing fields get sensible defaults |
| New player (empty) | ✅ Defaults | ✅ Unchanged |

**Include cleanup:** Remove `#include "player.pb.h"` from `player_manager.cpp` — no longer needed since protobuf parsing is removed. Keep `#include "dbmgr.pb.h"` for `PlayerDataOp`.

### 3.3 Proto Cleanup

`PlayerData` in `player.proto` is still needed for client-server communication (`EnterGameResp`), but is missing fields that were added to `PlayerBizData` after the proto was defined.

**Add missing fields to PlayerData:**

```protobuf
message PlayerData {
    uint64 player_id = 1;
    uint32 server_id = 2;
    string role_name = 3;
    uint32 level = 4;
    uint64 exp = 5;
    float pos_x = 6;
    float pos_y = 7;
    float pos_z = 8;
    uint64 created_at = 9;
    string scene_id = 10;
    int32 energy = 11;
    // Phase 2 additions
    int64 gold = 12;
    int32 max_hp = 13;
    int32 current_hp = 14;
    int32 attack_power = 15;
    int32 defense_power = 16;
    int32 combat_level = 17;
}
```

**Add comment at top of PlayerData:**
```protobuf
// NOTE: PlayerData is used for client-server communication (EnterGameResp).
// It is NOT used for persistence — player data is stored as JSON in MongoDB.
```

**Regenerate protobuf files** after modification.

### 3.4 Logging & Observability

| Location | Level | Content | Trigger |
|----------|-------|---------|---------|
| `load_from_json` parse success | INFO | player_id | Normal load |
| `load_from_json` parse exception | ERROR | player_id, exception, data_len | Data corruption |
| `load_from_json` missing field | WARN | player_id, field_name | Incomplete data |
| `handle_player_data_loaded` data loaded | INFO | player_id | Normal flow |
| `handle_player_data_loaded` data empty | INFO | player_id | New player |
| `handle_player_data_loaded` fallback to defaults | ERROR | player_id, reason | Needs attention |
| `handle_player_data_loaded` auto-save after fallback | WARN | player_id | Overwriting corrupted data |

### 3.5 Error Handling & Edge Cases

| Scenario | Handling |
|----------|----------|
| Invalid JSON syntax | `nlohmann::json::parse()` throws → caught → return false → fallback to defaults |
| Type mismatch (e.g., level="five") | `json.value("level", 1)` returns default 1 → WARN logged |
| Very large JSON | nlohmann/json has built-in size limits → safe |
| Concurrent load of same player | Already guarded by `players_.find()` check in `add_player_with_data_load` |
| `load_from_json` called before data arrives | Only called from `handle_player_data_loaded` callback → safe |

**Degradation strategy:**

```
Load data → JSON parse OK → use loaded data → start timer → clear dirty
          → JSON parse FAIL → init_default_data() → start timer → save_full() → ERROR log
          → data empty (new) → init_default_data() → start timer → save_full() → INFO log
          → DBMgr error → init_default_data() → NO timer → ERROR log
```

---

## 4. Files to Modify

| File | Changes |
|------|---------|
| `scripts/server/game_server/src/player.h` | Add `load_from_json()` declaration |
| `scripts/server/game_server/src/player.cpp` | Implement `load_from_json()` |
| `scripts/server/game_server/src/player_manager.cpp` | Rewrite `handle_player_data_loaded()` data parsing, remove `#include "player.pb.h"` |
| `scripts/common/proto/player.proto` | Add gold/combat fields to PlayerData, add usage comment |
| `scripts/common/proto/generated/player.pb.h` | Regenerate |
| `scripts/common/proto/generated/player.pb.cc` | Regenerate |

---

## 5. Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| Existing data in MongoDB has unexpected format | `load_from_json` handles both object and string representations of nested fields |
| Old protobuf-format data still in DB (from before Phase 1) | `load_from_json` will fail → fallback to defaults → immediate save overwrites. Acceptable data loss since Phase 1 was never fully working. |
| Proto field number collision when adding fields | New fields use numbers 12-17, well above existing max (11). No collision. |
| Client code expects old PlayerData without new fields | Protobuf is forward-compatible — unknown fields are silently ignored by old clients |

---

## 6. Testing Strategy

1. **Manual test:** Start server, create player, set gold=500, restart server, verify gold=500 after reload
2. **Corruption test:** Manually corrupt a player document in MongoDB (invalid JSON), restart, verify server falls back to defaults and logs ERROR
3. **Missing field test:** Remove a field from a player document in MongoDB, restart, verify default is used and WARN is logged
4. **New player test:** Create a brand new player, verify default data is saved and loaded correctly
5. **Build verification:** Full clean build with no warnings
