# Player Data Persistence Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add dirty field tracking, entity-owned 5-minute auto-save timer, and incremental save support to the Player class.

**Architecture:** Player becomes self-sufficient for persistence — it holds a `DBMgrConnectionManager*` pointer and a libevent timer. Each setter marks dirty fields. Three save paths: `save_full()` (SET_ALL, timer/exit), `save()` (SET per dirty field, business flush), `save_field(key)` (single SET, targeted).

**Tech Stack:** C++17, libevent (timers), nlohmann/json (serialization), spdlog (logging), protobuf (existing DBMgr protocol)

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `scripts/server/game_server/src/player.h` | Modify | Add dirty_fields_, timer, dbmgr_mgr, save methods |
| `scripts/server/game_server/src/player.cpp` | Modify | Implement dirty tracking, save methods, timer callback |
| `scripts/server/game_server/src/player_manager.h` | Modify | Update Player constructor calls, add dbmgr_mgr accessor |
| `scripts/server/game_server/src/player_manager.cpp` | Modify | Pass dbmgr_mgr to Player, manage timer lifecycle |
| `scripts/server/common/include/game_constants.h` | Modify | Add PLAYER_SAVE_INTERVAL constant |

---

### Task 1: Add dirty field tracking to Player

**Files:**
- Modify: `scripts/server/game_server/src/player.h:1-111`
- Modify: `scripts/server/game_server/src/player.cpp:1-128`

- [ ] **Step 1: Add dirty_fields_ member and include to player.h**

Add `#include <unordered_set>` after `#include <unordered_map>` (line 6), and add `dirty_fields_` member next to `dirty_`:

```cpp
// player.h — after line 6
#include <unordered_set>
```

```cpp
// player.h — replace line 108
    bool dirty_ = false;
    std::unordered_set<std::string> dirty_fields_;
```

- [ ] **Step 2: Add helper method to mark fields dirty**

Add private helper in player.h after `dirty_fields_`:

```cpp
    void mark_dirty(const std::string& field);
```

- [ ] **Step 3: Implement mark_dirty in player.cpp**

Add after the destructor (line 15):

```cpp
void Player::mark_dirty(const std::string& field) {
    dirty_ = true;
    dirty_fields_.insert(field);
}
```

- [ ] **Step 4: Update all setters to call mark_dirty**

Replace every `dirty_ = true;` in the setters with `mark_dirty("<field_name>");`. The mapping:

| Setter (line) | Replace `dirty_ = true;` with |
|---|---|
| `set_player_data(const&)` (line 19) | Mark all fields — see step 5 |
| `set_player_data(&&)` (line 23) | Mark all fields — see step 5 |
| `set_role_name` (line 30) | `mark_dirty("role_name");` |
| `set_level` (line 36) | `mark_dirty("level");` |
| `set_gold` (line 42) | `mark_dirty("gold");` |
| `set_experience` (line 48) | `mark_dirty("experience");` |
| `set_pos_x` (line 56) | `mark_dirty("pos_x");` |
| `set_pos_y` (line 62) | `mark_dirty("pos_y");` |
| `set_pos_z` (line 68) | `mark_dirty("pos_z");` |
| `set_scene_id` (line 76) | `mark_dirty("scene_id");` |
| `set_inventory` (line 82) | `mark_dirty("inventory");` |
| `set_energy` (line 90) | `mark_dirty("energy");` |
| `set_farm_state` (line 98) | `mark_dirty("farm_state");` |
| `set_extra_data` (line 104) | `mark_dirty("extra_data");` |
| `init_default_data` (line 123) | Mark all fields — see step 5 |

- [ ] **Step 5: Update bulk set_player_data and init_default_data to mark all fields**

Replace the `set_player_data` implementations:

```cpp
void Player::set_player_data(const PlayerBizData& data) {
    player_data_ = data;
    dirty_ = true;
    dirty_fields_ = {"role_name", "level", "gold", "experience",
                     "pos_x", "pos_y", "pos_z", "energy",
                     "scene_id", "inventory", "farm_state", "extra_data"};
}

void Player::set_player_data(PlayerBizData&& data) {
    player_data_ = std::move(data);
    dirty_ = true;
    dirty_fields_ = {"role_name", "level", "gold", "experience",
                     "pos_x", "pos_y", "pos_z", "energy",
                     "scene_id", "inventory", "farm_state", "extra_data"};
}
```

In `init_default_data()`, replace `dirty_ = true;` (line 123) with:

```cpp
    dirty_ = true;
    dirty_fields_ = {"role_name", "level", "gold", "experience",
                     "pos_x", "pos_y", "pos_z", "energy",
                     "scene_id", "inventory", "farm_state", "extra_data"};
```

- [ ] **Step 6: Compile to verify**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build --target game_server 2>&1 | head -50`
Expected: Build succeeds (no new errors)

- [ ] **Step 7: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/player.h scripts/server/game_server/src/player.cpp
git commit -m "feat(player): add dirty_fields_ tracking to all setters"
```

---

### Task 2: Add DBMgrConnectionManager dependency and save constants

**Files:**
- Modify: `scripts/server/game_server/src/player.h:1-111`
- Modify: `scripts/server/game_server/src/player.cpp:1-128`
- Modify: `scripts/server/common/include/game_constants.h:1-10`

- [ ] **Step 1: Add forward declaration and include**

In player.h, add after the existing forward declaration (line 10):

```cpp
class DBMgrConnectionManager;
```

- [ ] **Step 2: Add DBMgrConnectionManager pointer to Player**

In player.h, add new member after `dirty_fields_`:

```cpp
    DBMgrConnectionManager* dbmgr_mgr_ = nullptr;
```

- [ ] **Step 3: Update Player constructor signature**

Replace the constructor declaration (line 38):

```cpp
    Player(uint64_t player_id, GateSession* gate_session,
           DBMgrConnectionManager* dbmgr_mgr = nullptr);
```

- [ ] **Step 4: Update Player constructor implementation**

In player.cpp, replace the constructor (lines 7-11):

```cpp
Player::Player(uint64_t player_id, GateSession* gate_session,
               DBMgrConnectionManager* dbmgr_mgr)
    : player_id_(player_id)
    , gate_session_(gate_session)
    , join_time_(std::time(nullptr))
    , dbmgr_mgr_(dbmgr_mgr)
{
}
```

- [ ] **Step 5: Add setter for dbmgr_mgr**

In player.h, add after `set_gate_session` (line 45):

```cpp
    void set_dbmgr_mgr(DBMgrConnectionManager* mgr) { dbmgr_mgr_ = mgr; }
```

- [ ] **Step 6: Add save interval constant**

In `scripts/server/common/include/game_constants.h`, add before the closing brace:

```cpp
// 玩家自动存盘间隔（秒）
inline constexpr int32_t PLAYER_SAVE_INTERVAL = 300;  // 5 minutes

```

- [ ] **Step 7: Compile to verify**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build --target game_server 2>&1 | head -50`
Expected: Build succeeds

- [ ] **Step 8: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/player.h scripts/server/game_server/src/player.cpp scripts/server/common/include/game_constants.h
git commit -m "feat(player): add DBMgrConnectionManager dependency and save interval constant"
```

---

### Task 3: Implement save interface methods

**Files:**
- Modify: `scripts/server/game_server/src/player.h:1-111`
- Modify: `scripts/server/game_server/src/player.cpp:1-128`

- [ ] **Step 1: Add save method declarations to player.h**

Add public methods after `init_default_data()` (line 98):

```cpp
    // 存盘接口
    void save();                              // 差量存盘：遍历 dirty_fields_ 发 SET
    void save_field(const std::string& field); // 单字段存盘：发 SET
    void save_full();                         // 全量存盘：发 SET_ALL

    // 是否有脏数据
    bool has_dirty_fields() const { return !dirty_fields_.empty(); }
    const std::unordered_set<std::string>& dirty_fields() const { return dirty_fields_; }
```

- [ ] **Step 2: Add private helper for field serialization**

Add private method after `mark_dirty`:

```cpp
    std::string get_field_json(const std::string& field) const;
    std::string get_all_data_json() const;
    void clear_dirty();
```

- [ ] **Step 3: Add nlohmann/json include to player.cpp**

Add at the top of player.cpp after existing includes:

```cpp
#include "dbmgr_connection_manager.h"
#include "dbmgr.pb.h"
#include <nlohmann/json.hpp>
```

- [ ] **Step 4: Implement get_all_data_json**

In player.cpp, add after `mark_dirty`:

```cpp
std::string Player::get_all_data_json() const {
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
    data["inventory"] = player_data_.inventory;
    data["farm_state"] = player_data_.farm_state;
    data["extra_data"] = player_data_.extra_data;
    return data.dump();
}
```

- [ ] **Step 5: Implement get_field_json**

```cpp
std::string Player::get_field_json(const std::string& field) const {
    nlohmann::json val;
    if (field == "role_name") val = player_data_.role_name;
    else if (field == "level") val = player_data_.level;
    else if (field == "gold") val = player_data_.gold;
    else if (field == "experience") val = player_data_.experience;
    else if (field == "pos_x") val = player_data_.pos_x;
    else if (field == "pos_y") val = player_data_.pos_y;
    else if (field == "pos_z") val = player_data_.pos_z;
    else if (field == "energy") val = player_data_.energy;
    else if (field == "scene_id") val = player_data_.scene_id;
    else if (field == "inventory") {
        // inventory is already a JSON string, parse it first to avoid double-encoding
        try { val = nlohmann::json::parse(player_data_.inventory); }
        catch (...) { val = player_data_.inventory; }
    }
    else if (field == "farm_state") {
        try { val = nlohmann::json::parse(player_data_.farm_state); }
        catch (...) { val = player_data_.farm_state; }
    }
    else if (field == "extra_data") {
        try { val = nlohmann::json::parse(player_data_.extra_data); }
        catch (...) { val = player_data_.extra_data; }
    }
    else val = nullptr;
    return val.dump();
}
```

- [ ] **Step 6: Implement clear_dirty**

```cpp
void Player::clear_dirty() {
    dirty_ = false;
    dirty_fields_.clear();
}
```

- [ ] **Step 7: Implement save_full**

```cpp
void Player::save_full() {
    if (!dbmgr_mgr_ || !dirty_) return;

    std::string value = get_all_data_json();

    auto callback = [this](int32_t code, const uint8_t*, size_t) {
        if (code == 0) {
            SPDLOG_INFO("[Player]Full save success for player_id={}", player_id_);
        } else {
            SPDLOG_ERROR("[Player]Full save failed for player_id={} code={}", player_id_, code);
        }
    };

    uint64_t request_id = dbmgr_mgr_->send_player_data_req(
        player_id_,
        static_cast<int32_t>(farm::PlayerDataOp::SET_ALL),
        "", value, std::move(callback));

    if (request_id == 0) {
        SPDLOG_ERROR("[Player]Failed to send full save for player_id={}", player_id_);
    } else {
        clear_dirty();
    }
}
```

- [ ] **Step 8: Implement save (flush all dirty fields)**

```cpp
void Player::save() {
    if (!dbmgr_mgr_ || dirty_fields_.empty()) return;

    auto callback = [pid = player_id_](int32_t code, const uint8_t*, size_t) {
        if (code == 0) {
            SPDLOG_INFO("[Player]Field save success for player_id={}", pid);
        } else {
            SPDLOG_ERROR("[Player]Field save failed for player_id={} code={}", pid, code);
        }
    };

    for (const auto& field : dirty_fields_) {
        std::string value = get_field_json(field);
        uint64_t request_id = dbmgr_mgr_->send_player_data_req(
            player_id_,
            static_cast<int32_t>(farm::PlayerDataOp::SET),
            field, value, callback);
        if (request_id == 0) {
            SPDLOG_ERROR("[Player]Failed to send field save '{}' for player_id={}", field, player_id_);
        }
    }

    clear_dirty();
}
```

- [ ] **Step 9: Implement save_field**

```cpp
void Player::save_field(const std::string& field) {
    if (!dbmgr_mgr_) return;

    std::string value = get_field_json(field);

    auto callback = [pid = player_id_, field](int32_t code, const uint8_t*, size_t) {
        if (code == 0) {
            SPDLOG_INFO("[Player]Field '{}' saved for player_id={}", field, pid);
        } else {
            SPDLOG_ERROR("[Player]Field '{}' save failed for player_id={} code={}", field, pid, code);
        }
    };

    uint64_t request_id = dbmgr_mgr_->send_player_data_req(
        player_id_,
        static_cast<int32_t>(farm::PlayerDataOp::SET),
        field, value, std::move(callback));

    if (request_id == 0) {
        SPDLOG_ERROR("[Player]Failed to send field '{}' save for player_id={}", field, player_id_);
    } else {
        dirty_fields_.erase(field);
        if (dirty_fields_.empty()) dirty_ = false;
    }
}
```

- [ ] **Step 10: Compile to verify**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build --target game_server 2>&1 | head -50`
Expected: Build succeeds

- [ ] **Step 11: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/player.h scripts/server/game_server/src/player.cpp
git commit -m "feat(player): implement save(), save_field(), save_full() methods"
```

---

### Task 4: Add libevent auto-save timer to Player

**Files:**
- Modify: `scripts/server/game_server/src/player.h:1-111`
- Modify: `scripts/server/game_server/src/player.cpp:1-128`

- [ ] **Step 1: Add libevent include to player.h**

Add at the top of player.h after existing includes:

```cpp
#include <event2/event.h>
```

- [ ] **Step 2: Add timer members to Player class**

In player.h, add private members after `dbmgr_mgr_`:

```cpp
    // 自动存盘定时器
    struct event* save_timer_ = nullptr;
    struct event_base* base_ = nullptr;
```

- [ ] **Step 3: Add timer public methods**

In player.h, add public methods after `save_full()`:

```cpp
    // 自动存盘定时器
    void start_save_timer(struct event_base* base);
    void stop_save_timer();
```

- [ ] **Step 4: Add timer private callback**

In player.h, add private methods after `clear_dirty`:

```cpp
    static void on_save_timer(evutil_socket_t fd, short events, void* ctx);
    void handle_save_timer();
```

- [ ] **Step 5: Implement timer methods in player.cpp**

Add at the end of player.cpp (before closing namespace):

```cpp
void Player::start_save_timer(struct event_base* base) {
    if (save_timer_ || !base) return;

    base_ = base;
    save_timer_ = event_new(base_, -1, EV_PERSIST, on_save_timer, this);
    if (!save_timer_) {
        SPDLOG_ERROR("[Player]Failed to create save timer for player_id={}", player_id_);
        return;
    }

    struct timeval tv;
    tv.tv_sec = PLAYER_SAVE_INTERVAL;
    tv.tv_usec = 0;
    evtimer_add(save_timer_, &tv);

    SPDLOG_INFO("[Player]Save timer started for player_id={} interval={}s", player_id_, PLAYER_SAVE_INTERVAL);
}

void Player::stop_save_timer() {
    if (save_timer_) {
        event_del(save_timer_);
        event_free(save_timer_);
        save_timer_ = nullptr;
    }
}

void Player::on_save_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* player = static_cast<Player*>(ctx);
    player->handle_save_timer();
}

void Player::handle_save_timer() {
    if (!dirty_) return;
    SPDLOG_INFO("[Player]Auto-save timer fired for player_id={}", player_id_);
    save_full();
}
```

- [ ] **Step 6: Update destructor to stop timer**

Replace the destructor in player.cpp:

```cpp
Player::~Player() {
    stop_save_timer();
}
```

- [ ] **Step 7: Compile to verify**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build --target game_server 2>&1 | head -50`
Expected: Build succeeds

- [ ] **Step 8: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/player.h scripts/server/game_server/src/player.cpp
git commit -m "feat(player): add 5-minute libevent auto-save timer"
```

---

### Task 5: Update PlayerManager to wire up new Player dependencies

**Files:**
- Modify: `scripts/server/game_server/src/player_manager.h:1-83`
- Modify: `scripts/server/game_server/src/player_manager.cpp:1-307`

- [ ] **Step 1: Add dbmgr_mgr accessor to PlayerManager**

In player_manager.h, add public method after `set_dbmgr_manager`:

```cpp
    DBMgrConnectionManager* dbmgr_mgr() const { return dbmgr_mgr_; }
```

- [ ] **Step 2: Update add_player to pass dbmgr_mgr**

In player_manager.cpp, replace `add_player` (lines 14-22):

```cpp
bool PlayerManager::add_player(uint64_t player_id, GateSession* gate_session) {
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        return false;
    }
    // Note: timer not started here — data state is NOT_LOADED.
    // Timer starts after data loads (in handle_player_data_loaded).
    players_[player_id] = std::make_unique<Player>(player_id, gate_session, dbmgr_mgr_);
    return true;
}
```

- [ ] **Step 3: Add event_base to PlayerManager**

In player_manager.h, add member after `dbmgr_mgr_`:

```cpp
    struct event_base* base_ = nullptr;
```

Add public setter:

```cpp
    void set_event_base(struct event_base* base) { base_ = base; }
```

- [ ] **Step 4: Update add_player_with_data_load to pass dbmgr_mgr**

In player_manager.cpp, replace the Player creation in `add_player_with_data_load` (line 33):

```cpp
    auto player = std::make_unique<Player>(player_id, gate_session, dbmgr_mgr_);
```

Timer start is deferred to `handle_player_data_loaded` (step 4b).

- [ ] **Step 4b: Start timer after data loads in handle_player_data_loaded**

In `handle_player_data_loaded`, there are 4 code paths that set `LOADED` state. Add `player->start_save_timer(base_);` after each one:

Path 1 — existing data loaded (after line 195):
```cpp
            player->set_data_state(PlayerBizDataState::LOADED);
            player->start_save_timer(base_);
```

Path 2 — protobuf parse failed, defaults used (after line 202):
```cpp
            player->set_data_state(PlayerBizDataState::LOADED);
            player->start_save_timer(base_);
```

Path 3 — new player, defaults initialized (after line 209):
```cpp
        player->set_data_state(PlayerBizDataState::LOADED);
        player->start_save_timer(base_);
```

Path 4 — load failed, fallback to defaults (after line 221):
```cpp
        player->set_data_state(PlayerBizDataState::LOADED);
        player->start_save_timer(base_);
```

- [ ] **Step 5: Update remove_player to stop timer**

In player_manager.cpp, replace `remove_player` (lines 88-93):

```cpp
void PlayerManager::remove_player(uint64_t player_id) {
    auto it = players_.find(player_id);
    if (it == players_.end()) return;

    it->second->stop_save_timer();
    if (offline_callback_) {
        offline_callback_(player_id);
    }
    players_.erase(it);
}
```

- [ ] **Step 6: Update remove_player_with_save to stop timer and use save_full**

In player_manager.cpp, replace `remove_player_with_save` (lines 95-108):

```cpp
void PlayerManager::remove_player_with_save(uint64_t player_id) {
    auto it = players_.find(player_id);
    if (it == players_.end()) return;

    it->second->stop_save_timer();
    it->second->save_full();

    if (offline_callback_) {
        offline_callback_(player_id);
    }
    players_.erase(it);
}
```

- [ ] **Step 7: Update remove_players_by_gate to stop timer**

In player_manager.cpp, replace `remove_players_by_gate` (lines 127-141):

```cpp
void PlayerManager::remove_players_by_gate(GateSession* gate_session) {
    std::vector<uint64_t> to_remove;
    for (auto& kv : players_) {
        if (kv.second->gate_session() == gate_session) {
            to_remove.push_back(kv.first);
        }
    }
    for (uint64_t pid : to_remove) {
        SPDLOG_INFO("[Player]Removing player {} due to Gate disconnect", pid);
        auto it = players_.find(pid);
        if (it != players_.end()) {
            it->second->stop_save_timer();
            if (offline_callback_) {
                offline_callback_(pid);
            }
            players_.erase(it);
        }
    }
}
```

- [ ] **Step 8: Update remove_players_by_gate_with_save to stop timer and use save_full**

In player_manager.cpp, replace `remove_players_by_gate_with_save` (lines 143-158):

```cpp
void PlayerManager::remove_players_by_gate_with_save(GateSession* gate_session) {
    std::vector<uint64_t> to_remove;
    for (auto& kv : players_) {
        if (kv.second->gate_session() == gate_session) {
            to_remove.push_back(kv.first);
        }
    }
    for (uint64_t pid : to_remove) {
        SPDLOG_INFO("[Player]Removing player {} due to Gate disconnect (with save)", pid);
        auto it = players_.find(pid);
        if (it != players_.end()) {
            it->second->stop_save_timer();
            it->second->save_full();
            if (offline_callback_) {
                offline_callback_(pid);
            }
            players_.erase(it);
        }
    }
}
```

- [ ] **Step 9: Update save_all_players to use save_full directly**

In player_manager.cpp, replace `save_all_players` (lines 297-305):

```cpp
void PlayerManager::save_all_players() {
    SPDLOG_INFO("[Player]Saving all players data...");
    for (auto& [player_id, player] : players_) {
        player->save_full();
    }
    SPDLOG_INFO("[Player]All players data save requests sent");
}
```

- [ ] **Step 10: Update save_player_data to use save_full**

In player_manager.cpp, replace `save_player_data` (lines 242-295):

```cpp
void PlayerManager::save_player_data(uint64_t player_id) {
    auto it = players_.find(player_id);
    if (it == players_.end()) return;
    it->second->save_full();
}
```

- [ ] **Step 11: Update GameServer to set event_base on PlayerManager**

In `game_server.cpp`, after `player_mgr_.set_dbmgr_manager(&dbmgr_mgr_);` (line 109), add:

```cpp
            player_mgr_.set_event_base(base_);
```

- [ ] **Step 12: Compile to verify**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build --target game_server 2>&1 | head -50`
Expected: Build succeeds

- [ ] **Step 13: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/player_manager.h scripts/server/game_server/src/player_manager.cpp scripts/server/game_server/src/game_server.cpp
git commit -m "feat(player_manager): wire up Player auto-save timer and save_full() calls"
```

---

### Task 6: Remove old protobuf-based save logic

**Files:**
- Modify: `scripts/server/game_server/src/player_manager.cpp:1-307`

- [ ] **Step 1: Remove unused protobuf includes from player_manager.cpp**

The `save_player_data` method no longer needs protobuf serialization. The `dbmgr.pb.h` include is still needed for `PlayerDataOp`. The `player.pb.h` include can be removed if no other code in player_manager.cpp uses it.

Check if `player.pb.h` is used elsewhere in player_manager.cpp. Looking at the code, `handle_player_data_loaded` still uses `farm::PlayerData` for deserialization. Keep `player.pb.h` for now.

No changes needed — the old `save_player_data` is already replaced in Task 5 Step 10.

- [ ] **Step 2: Verify handle_player_data_loaded still works**

The load path uses `PlayerData` protobuf for deserialization from DBMgr. This is separate from the save path and should remain unchanged. Verify by reading `handle_player_data_loaded` — it reads from DBMgr and populates PlayerBizData. This is correct.

No changes needed.

- [ ] **Step 3: Commit (if any changes made)**

No commit needed — no changes in this task.

---

### Task 7: Full build verification and cleanup

**Files:**
- All modified files

- [ ] **Step 1: Full clean build**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build 2>&1 | tail -20`
Expected: All targets build successfully

- [ ] **Step 2: Verify no compiler warnings related to our changes**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build 2>&1 | grep -i "player\." | grep -i "warn"`
Expected: No warnings

- [ ] **Step 3: Verify the complete data flow**

Review the following paths in the code:

1. **Player created** → `Player(id, gate, dbmgr_mgr)` → data loaded → `start_save_timer(base)` ✓
2. **Setter called** → `mark_dirty("field")` → `dirty_ = true`, `dirty_fields_.insert(field)` ✓
3. **Timer fires (5 min)** → `handle_save_timer()` → `save_full()` → `SET_ALL` → `clear_dirty()` ✓
4. **Business calls save()** → iterate `dirty_fields_` → `SET` per field → `clear_dirty()` ✓
5. **Business calls save_field("gold")** → `SET data.gold` → erase from `dirty_fields_` ✓
6. **Player exits** → `stop_save_timer()` → `save_full()` → `SET_ALL` → destroy ✓

- [ ] **Step 4: Final commit (if any fixes needed)**

```bash
cd D:/mb_workspace/farm_demo
git add -A
git commit -m "fix: address build warnings in player persistence redesign"
```
