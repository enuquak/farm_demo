# Player Load Path Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the serialization mismatch where save writes JSON but load expects protobuf, causing all player data to be lost on server restart.

**Architecture:** Add `Player::load_from_json()` symmetric with the existing `get_all_data_json()`. Rewrite `PlayerManager::handle_player_data_loaded()` to use JSON parsing instead of protobuf. Update `PlayerData` proto with missing fields for client communication.

**Tech Stack:** C++17, nlohmann/json, protobuf-lite, spdlog

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `scripts/server/game_server/src/player.h` | Modify | Add `load_from_json()` declaration |
| `scripts/server/game_server/src/player.cpp` | Modify | Implement `load_from_json()` |
| `scripts/server/game_server/src/player_manager.cpp` | Modify | Rewrite load path, remove `player.pb.h` include |
| `scripts/common/proto/player.proto` | Modify | Add gold/combat fields to PlayerData, add usage comment |
| `scripts/common/proto/generated/player.pb.h` | Regenerate | Protobuf C++ header |
| `scripts/common/proto/generated/player.pb.cc` | Regenerate | Protobuf C++ source |
| `scripts/common/proto/generated/player_pb2.py` | Regenerate | Protobuf Python source |
| `scripts/server/game_server/src/login_stub.cpp` | Modify | Populate new PlayerData fields in EnterGameResp |

---

### Task 1: Add Player::load_from_json() method

**Files:**
- Modify: `scripts/server/game_server/src/player.h:49-175`
- Modify: `scripts/server/game_server/src/player.cpp:1-394`

- [ ] **Step 1: Add load_from_json declaration to player.h**

In `player.h`, add after `init_default_data()` declaration (line 137):

```cpp
    // 从 JSON 字符串加载数据（DBMgr 返回的 BSON/JSON 数据）
    // 返回 true 表示解析成功，false 表示解析失败
    bool load_from_json(const std::string& json_str);
```

- [ ] **Step 2: Implement load_from_json in player.cpp**

In `player.cpp`, add after `init_default_data()` implementation (after line 355):

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

        // JSON 字符串字段：对象→dump()，字符串→直接用
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

        // 战斗字段
        data.max_hp = json.value("max_hp", 100);
        data.current_hp = json.value("current_hp", 100);
        data.attack_power = json.value("attack_power", 0);
        data.defense_power = json.value("defense_power", 0);
        data.combat_exp = json.value("combat_exp", 0);
        data.combat_level = json.value("combat_level", 1);

        // 检查关键字段是否存在于 JSON 中
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
        clear_dirty();  // 加载的数据不是脏数据
        return true;

    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Player]load_from_json failed for player_id={} error={}",
                     player_id_, e.what());
        return false;
    }
}
```

- [ ] **Step 3: Compile to verify**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build --target game_server 2>&1 | head -50`
Expected: Build succeeds

- [ ] **Step 4: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/player.h scripts/server/game_server/src/player.cpp
git commit -m "feat(player): add load_from_json() for JSON deserialization"
```

---

### Task 2: Rewrite PlayerManager load path

**Files:**
- Modify: `scripts/server/game_server/src/player_manager.cpp:1-260`

- [ ] **Step 1: Remove unused player.pb.h include**

In `player_manager.cpp`, delete line 9:

```cpp
#include "player.pb.h"
```

The file should now have these includes at the top:

```cpp
#include "player_manager.h"
#include "gate_session.h"
#include "dbmgr_connection_manager.h"
#include "internal_msg_ids.h"

#include <cstring>

#include "dbmgr.pb.h"
#include "log_macros.h"
```

- [ ] **Step 2: Rewrite handle_player_data_loaded data parsing**

In `player_manager.cpp`, replace the entire `if (code == 0 && value_data && value_len > 0)` block (lines 185-216) with:

```cpp
    if (code == 0 && value_data && value_len > 0) {
        // 数据加载成功，从 JSON 解析
        std::string data_str(reinterpret_cast<const char*>(value_data), value_len);

        if (player->load_from_json(data_str)) {
            player->set_data_state(PlayerBizDataState::LOADED);
            player->start_save_timer(base_);
            SPDLOG_INFO("[Player]Data loaded from DB for player_id={}", player_id);
        } else {
            // JSON 解析失败，数据可能损坏
            SPDLOG_ERROR("[Player]Corrupted data for player_id={}, falling back to defaults", player_id);
            player->init_default_data();
            player->set_data_state(PlayerBizDataState::LOADED);
            player->start_save_timer(base_);
            // 立即保存默认数据，防止下次加载仍然读到损坏数据
            player->save_full();
        }
```

- [ ] **Step 3: Compile to verify**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build --target game_server 2>&1 | head -50`
Expected: Build succeeds

- [ ] **Step 4: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/player_manager.cpp
git commit -m "fix(player_manager): rewrite load path to use JSON instead of protobuf

Fixes critical data loss bug where save writes JSON but load expected
protobuf, causing all player data to fall back to defaults on restart.
Now uses Player::load_from_json() for symmetric serialization."
```

---

### Task 3: Update PlayerData proto with missing fields

**Files:**
- Modify: `scripts/common/proto/player.proto:13-25`
- Regenerate: `scripts/common/proto/generated/player.pb.h`
- Regenerate: `scripts/common/proto/generated/player.pb.cc`
- Regenerate: `scripts/common/proto/generated/player_pb2.py`

- [ ] **Step 1: Add usage comment and missing fields to PlayerData**

In `player.proto`, replace the `PlayerData` message (lines 12-25) with:

```protobuf
// 玩家数据（用于客户端-服务器通信，如 EnterGameResp）
// NOTE: PlayerData 不用于持久化 — 玩家数据以 JSON 格式存储在 MongoDB 中
message PlayerData {
    uint64 player_id = 1;       // 玩家 ID
    uint32 server_id = 2;       // 服务器 ID
    string role_name = 3;       // 角色名
    uint32 level = 4;           // 等级
    uint64 exp = 5;             // 经验值
    float pos_x = 6;            // 位置 X
    float pos_y = 7;            // 位置 Y
    uint64 created_at = 8;      // 创建时间
    float pos_z = 9;            // 位置 Z
    string scene_id = 10;       // 场景 ID
    int32 energy = 11;          // 能量值
    // Phase 2: 新增字段（与 PlayerBizData 对齐）
    int64 gold = 12;            // 金币
    int32 max_hp = 13;          // 最大生命值
    int32 current_hp = 14;      // 当前生命值
    int32 attack_power = 15;    // 额外攻击力
    int32 defense_power = 16;   // 额外防御力
    int32 combat_level = 17;    // 战斗等级
}
```

- [ ] **Step 2: Regenerate protobuf C++ files**

Run: `cd D:/mb_workspace/farm_demo && protoc --cpp_out=scripts/common/proto/generated --proto_path=scripts/common/proto scripts/common/proto/player.proto 2>&1`

Expected: No errors. Files `player.pb.h` and `player.pb.cc` regenerated.

- [ ] **Step 3: Regenerate protobuf Python files**

Run: `cd D:/mb_workspace/farm_demo && protoc --python_out=scripts/common/proto/generated --proto_path=scripts/common/proto scripts/common/proto/player.proto 2>&1`

Expected: No errors. File `player_pb2.py` regenerated.

- [ ] **Step 4: Compile to verify**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build --target game_server 2>&1 | head -50`
Expected: Build succeeds

- [ ] **Step 5: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/common/proto/player.proto scripts/common/proto/generated/player.pb.h scripts/common/proto/generated/player.pb.cc scripts/common/proto/generated/player_pb2.py
git commit -m "feat(proto): add gold/combat fields to PlayerData for client communication"
```

---

### Task 4: Update LoginStub to populate new PlayerData fields

**Files:**
- Modify: `scripts/server/game_server/src/login_stub.cpp:208-447`

- [ ] **Step 1: Update handle_enter_game to populate new fields**

In `login_stub.cpp`, find the `handle_enter_game` method. In the callback lambda (around line 262-271), after the existing `player_data.set_scene_id(data.scene_id);` line, add:

```cpp
                player_data.set_energy(data.energy);
                player_data.set_gold(data.gold);
                player_data.set_max_hp(data.max_hp);
                player_data.set_current_hp(data.current_hp);
                player_data.set_attack_power(data.attack_power);
                player_data.set_defense_power(data.defense_power);
                player_data.set_combat_level(data.combat_level);
```

- [ ] **Step 2: Update handle_enter_game_req to populate new fields**

In `login_stub.cpp`, find the `handle_enter_game_req` method. In the callback lambda (around line 382-391), after the existing `player_data.set_scene_id(data.scene_id);` line, add:

```cpp
                player_data.set_energy(data.energy);
                player_data.set_gold(data.gold);
                player_data.set_max_hp(data.max_hp);
                player_data.set_current_hp(data.current_hp);
                player_data.set_attack_power(data.attack_power);
                player_data.set_defense_power(data.defense_power);
                player_data.set_combat_level(data.combat_level);
```

- [ ] **Step 3: Compile to verify**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build --target game_server 2>&1 | head -50`
Expected: Build succeeds

- [ ] **Step 4: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/login_stub.cpp
git commit -m "feat(login_stub): populate gold/combat fields in EnterGameResp"
```

---

### Task 5: Full build verification and cleanup

**Files:**
- All modified files

- [ ] **Step 1: Full clean build**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build 2>&1 | tail -20`
Expected: All targets build successfully

- [ ] **Step 2: Verify no compiler warnings related to our changes**

Run: `cd D:/mb_workspace/farm_demo && cmake --build build 2>&1 | grep -i "player\.\|player_manager\.\|login_stub\." | grep -i "warn"`
Expected: No warnings

- [ ] **Step 3: Verify the complete data flow**

Review the following paths in the code:

1. **Save path (unchanged):** `Player::save_full()` → `get_all_data_json()` → JSON string → DBMgr → MongoDB BSON
2. **Load path (fixed):** MongoDB BSON → DBMgr → `Player::load_from_json()` → JSON parse → `PlayerBizData` → `set_player_data()` → `clear_dirty()`
3. **Corruption handling:** JSON parse fails → `init_default_data()` → `start_save_timer()` → `save_full()` → ERROR log
4. **New player:** Empty data → `init_default_data()` → `save_full()` → INFO log
5. **EnterGameResp:** `PlayerBizData` → `PlayerData` proto with gold/combat fields → client

- [ ] **Step 4: Final commit (if any fixes needed)**

```bash
cd D:/mb_workspace/farm_demo
git add -A
git commit -m "fix: address build warnings in player load path fix"
```
