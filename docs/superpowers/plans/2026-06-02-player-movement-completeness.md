# Player Movement Completeness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the player movement network sync layer — define missing proto messages, implement server-side position validation with correction, fix client send/receive, and fix new player spawn position.

**Architecture:** Server-authoritative position validation. Client sends PositionUpdate every 100ms. Server validates boundary + speed, updates Player data if valid, sends PositionCorrect if invalid. Client lerps to corrected position over 200ms.

**Tech Stack:** Python/PyGame (client), C++/libevent (server), Protobuf 3, TMX maps

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `scripts/common/proto/player.proto` | Modify | Add PositionUpdate, PositionCorrect messages |
| `scripts/common/proto/generated/player_pb2.py` | Regenerate | Compiled proto |
| `scripts/server/common/include/game_constants.h` | Modify | Add MAX_PLAYER_SPEED, POSITION_UPDATE_TIMEOUT_MS, DEFAULT_SPAWN_POS |
| `scripts/server/game_server/src/player.cpp` | Modify | Fix init_default_data() spawn position |
| `scripts/server/game_server/src/game_server.h` | Modify | Add handle_position_update declaration |
| `scripts/server/game_server/src/game_server.cpp` | Modify | Register handler, implement position validation |
| `scripts/client/player_controller.py` | Modify | Fix PositionUpdate field names |
| `scripts/client/network_dispatcher.py` | Modify | Fix PositionCorrect field names |

---

### Task 1: Add Proto Messages

**Files:**
- Modify: `scripts/common/proto/player.proto:136` (after last message)
- Regenerate: `scripts/common/proto/generated/player_pb2.py`

- [x] **Step 1: Add PositionUpdate and PositionCorrect to player.proto**

Open `scripts/common/proto/player.proto` and append after the last message (`ActiveSlotChange`):

```protobuf
// 位置更新 (Client -> Game)
message PositionUpdate {
    float pos_x = 1;            // 位置 X (像素)
    float pos_y = 2;            // 位置 Y (像素)
    float pos_z = 3;            // 位置 Z (像素)
    uint64 timestamp = 4;       // 客户端时间戳 (ms)
}

// 位置纠正 (Game -> Client)
message PositionCorrect {
    float pos_x = 1;            // 纠正后位置 X (像素)
    float pos_y = 2;            // 纠正后位置 Y (像素)
    float pos_z = 3;            // 纠正后位置 Z (像素)
    uint64 timestamp = 4;       // 原始更新的时间戳 (ms)
}
```

- [x] **Step 2: Regenerate Python proto**

Run from project root:

```bash
cd D:/mb_workspace/farm_demo
python -m grpc_tools.protoc -I=scripts/common/proto --python_out=scripts/common/proto/generated scripts/common/proto/player.proto
```

Expected: `scripts/common/proto/generated/player_pb2.py` regenerated with `PositionUpdate` and `PositionCorrect` classes.

- [x] **Step 3: Verify generated classes**

```bash
cd D:/mb_workspace/farm_demo
python -c "import sys; sys.path.insert(0, 'scripts/common/proto/generated'); import player_pb2; print(player_pb2.PositionUpdate.DESCRIPTOR.fields_by_name.keys()); print(player_pb2.PositionCorrect.DESCRIPTOR.fields_by_name.keys())"
```

Expected output:
```
['pos_x', 'pos_y', 'pos_z', 'timestamp']
['pos_x', 'pos_y', 'pos_z', 'timestamp']
```

- [x] **Step 4: Commit**

```bash
git add scripts/common/proto/player.proto scripts/common/proto/generated/player_pb2.py
git commit -m "feat(proto): add PositionUpdate and PositionCorrect messages"
```

---

### Task 2: Add Server Constants

**Files:**
- Modify: `scripts/server/common/include/game_constants.h:12` (after PLAYER_SAVE_INTERVAL)

- [x] **Step 1: Add movement validation constants**

Open `scripts/server/common/include/game_constants.h` and add before the closing `}`:

```cpp
// 移动校验
inline constexpr float MAX_PLAYER_SPEED = 128.0f;          // 像素/秒（客户端速度64的2倍容差）
inline constexpr int64_t POSITION_UPDATE_TIMEOUT_MS = 3000; // 超过3秒未更新视为过期

// 新玩家出生点（像素坐标，对应 farm_main 场景 tile 30,25）
inline constexpr float DEFAULT_SPAWN_POS_X = 480.0f;  // 30 * TILE_SIZE(16)
inline constexpr float DEFAULT_SPAWN_POS_Y = 400.0f;  // 25 * TILE_SIZE(16)
```

- [x] **Step 2: Commit**

```bash
git add scripts/server/common/include/game_constants.h
git commit -m "feat(server): add movement validation and spawn position constants"
```

---

### Task 3: Fix New Player Spawn Position

**Files:**
- Modify: `scripts/server/game_server/src/player.cpp:260-261`

- [x] **Step 1: Update init_default_data() spawn position**

Open `scripts/server/game_server/src/player.cpp`. Find lines 260-261:

```cpp
    player_data_.pos_x = 0.0f;
    player_data_.pos_y = 0.0f;
```

Replace with:

```cpp
    player_data_.pos_x = DEFAULT_SPAWN_POS_X;
    player_data_.pos_y = DEFAULT_SPAWN_POS_Y;
```

- [x] **Step 2: Verify the constant is accessible**

The file already includes `game_constants.h` via `player.h` (line 10: `#include "game_constants.h"`). No additional include needed.

- [x] **Step 3: Commit**

```bash
git add scripts/server/game_server/src/player.cpp
git commit -m "fix(server): use correct spawn position for new players"
```

---

### Task 4: Implement Server Position Update Handler

**Files:**
- Modify: `scripts/server/game_server/src/game_server.h:99` (add declaration)
- Modify: `scripts/server/game_server/src/game_server.cpp:195` (register handler + implement)

- [x] **Step 1: Add handler declaration to game_server.h**

Open `scripts/server/game_server/src/game_server.h`. Find the private handler declarations (around line 98-101):

```cpp
    void handle_item_use_req(uint64_t player_id,
                              const uint8_t* payload, size_t payload_len);
    void handle_enter_game_req(std::shared_ptr<GateSession> session,
                               uint64_t player_id, const std::string& payload);
```

Add after `handle_enter_game_req`:

```cpp
    void handle_position_update(uint64_t player_id,
                                const uint8_t* payload, size_t payload_len);
```

- [x] **Step 2: Register the handler in game_server.cpp**

Open `scripts/server/game_server/src/game_server.cpp`. Find the handler registration block (around line 191-195):

```cpp
    // 注册强制睡觉就绪消息处理
    msg_handler_.register_handler(MSG_ID_FORCE_SLEEP_READY,
        [this](uint64_t player_id, const uint8_t* payload, size_t payload_len) {
            game_clock_->handle_force_sleep_ready(player_id, payload, payload_len);
        });
    SPDLOG_INFO("[Game]Force sleep handler registered");
```

Add after the force sleep handler registration:

```cpp
    // 注册位置更新消息处理
    msg_handler_.register_handler(MSG_ID_POSITION_UPDATE,
        [this](uint64_t player_id, const uint8_t* payload, size_t payload_len) {
            handle_position_update(player_id, payload, payload_len);
        });
    SPDLOG_INFO("[Game]Position update handler registered");
```

- [x] **Step 3: Implement handle_position_update**

Open `scripts/server/game_server/src/game_server.cpp`. Find `handle_item_use_req` (around line 658). Add the new handler before it:

```cpp
void GameServer::handle_position_update(uint64_t player_id,
                                        const uint8_t* payload, size_t payload_len) {
    // Parse PositionUpdate
    farm::PositionUpdate req;
    if (payload_len > 0 && !req.ParseFromArray(payload, static_cast<int>(payload_len))) {
        SPDLOG_ERROR("[Game]Failed to parse PositionUpdate for player_id={}", player_id);
        return;
    }

    // Find player
    Player* player = player_mgr_.get_player(player_id).value_or(nullptr);
    if (!player || player->data_state() != PlayerBizDataState::LOADED) {
        return;
    }

    // Check scene exists
    const std::string& scene_id = player->get_scene_id();
    auto scene_opt = scene_mgr_->get_scene(scene_id);
    if (!scene_opt.has_value() || !scene_opt.value()) {
        return;
    }
    SceneState* scene = scene_opt.value();

    // Timeout check: discard stale updates
    int64_t now_ms = static_cast<int64_t>(std::time(nullptr)) * 1000;
    if (req.timestamp() > 0 && (now_ms - static_cast<int64_t>(req.timestamp())) > POSITION_UPDATE_TIMEOUT_MS) {
        SPDLOG_DEBUG("[Game]Stale position update from player_id={} (age={}ms)", player_id, now_ms - req.timestamp());
        return;
    }

    float new_x = req.pos_x();
    float new_y = req.pos_y();
    float new_z = req.pos_z();

    // Boundary check (pixel coordinates, scene dimensions in tiles, TILE_SIZE=16)
    constexpr float TILE_SIZE_PX = 16.0f;
    float max_x = scene->width() * TILE_SIZE_PX - TILE_SIZE_PX;
    float max_y = scene->height() * TILE_SIZE_PX - TILE_SIZE_PX;

    if (new_x < 0.0f || new_x > max_x || new_y < 0.0f || new_y > max_y) {
        SPDLOG_WARN("[Game]Boundary violation: player_id={} pos=({:.1f},{:.1f}) bounds=[0,0]-[{:.1f},{:.1f}]",
                    player_id, new_x, new_y, max_x, max_y);
        // Send correction with clamped position
        float corr_x = std::max(0.0f, std::min(new_x, max_x));
        float corr_y = std::max(0.0f, std::min(new_y, max_y));
        farm::PositionCorrect corr;
        corr.set_pos_x(corr_x);
        corr.set_pos_y(corr_y);
        corr.set_pos_z(new_z);
        corr.set_timestamp(req.timestamp());
        std::string corr_data;
        corr.SerializeToString(&corr_data);
        send_game_msg(player_id, MSG_ID_POSITION_CORRECT,
                      reinterpret_cast<const uint8_t*>(corr_data.data()), corr_data.size());
        return;
    }

    // Speed check
    float old_x = player->get_pos_x();
    float old_y = player->get_pos_y();
    float dx = new_x - old_x;
    float dy = new_y - old_y;
    float distance = std::sqrt(dx * dx + dy * dy);

    if (distance > 0.01f) {
        // Estimate time delta: use 100ms as default if no prior timestamp
        float time_delta_s = 0.1f;
        float speed = distance / time_delta_s;

        if (speed > MAX_PLAYER_SPEED) {
            SPDLOG_WARN("[Game]Speed violation: player_id={} speed={:.1f} max={:.1f} dist={:.1f}",
                        player_id, speed, MAX_PLAYER_SPEED, distance);
            // Send correction with last known good position
            farm::PositionCorrect corr;
            corr.set_pos_x(old_x);
            corr.set_pos_y(old_y);
            corr.set_pos_z(player->get_pos_z());
            corr.set_timestamp(req.timestamp());
            std::string corr_data;
            corr.SerializeToString(&corr_data);
            send_game_msg(player_id, MSG_ID_POSITION_CORRECT,
                          reinterpret_cast<const uint8_t*>(corr_data.data()), corr_data.size());
            return;
        }
    }

    // Valid update — apply
    player->set_pos_x(new_x);
    player->set_pos_y(new_y);
    player->set_pos_z(new_z);

    SPDLOG_DEBUG("[Game]Position updated: player_id=({:.1f},{:.1f})", player_id, new_x, new_y);
}
```

- [x] **Step 4: Commit**

```bash
git add scripts/server/game_server/src/game_server.h scripts/server/game_server/src/game_server.cpp
git commit -m "feat(server): implement position update handler with boundary and speed validation"
```

---

### Task 5: Fix Client Position Update Sending

**Files:**
- Modify: `scripts/client/player_controller.py:279-283`

- [x] **Step 1: Fix PositionUpdate field names**

Open `scripts/client/player_controller.py`. Find `_send_position_update` method (line 276). The current code at lines 279-283:

```python
            pos_update = player_pb2.PositionUpdate()
            pos_update.x = self._player_sprite.world_x
            pos_update.y = self._player_sprite.world_y
            pos_update.direction = self._facing_direction
            pos_update.timestamp = int(time.time() * 1000)
```

Replace with:

```python
            pos_update = player_pb2.PositionUpdate()
            pos_update.pos_x = self._player_sprite.world_x
            pos_update.pos_y = self._player_sprite.world_y
            pos_update.pos_z = 0.0
            pos_update.timestamp = int(time.time() * 1000)
```

- [x] **Step 2: Commit**

```bash
git add scripts/client/player_controller.py
git commit -m "fix(client): use correct PositionUpdate proto field names"
```

---

### Task 6: Fix Client Position Correction Receiving

**Files:**
- Modify: `scripts/client/network_dispatcher.py:111-114`

- [x] **Step 1: Fix PositionCorrect field names**

Open `scripts/client/network_dispatcher.py`. Find `_handle_position_correct` method (line 105). The current code at lines 111-114:

```python
            pos_correct = player_pb2.PositionCorrect()
            pos_correct.ParseFromString(player_msg.payload)

            self._callbacks["on_position_correct"](pos_correct.x, pos_correct.y)
```

Replace with:

```python
            pos_correct = player_pb2.PositionCorrect()
            pos_correct.ParseFromString(player_msg.payload)

            self._callbacks["on_position_correct"](pos_correct.pos_x, pos_correct.pos_y)
```

- [x] **Step 2: Commit**

```bash
git add scripts/client/network_dispatcher.py
git commit -m "fix(client): use correct PositionCorrect proto field names"
```

---

### Task 7: Integration Verification

**Files:** None (manual testing)

- [x] **Step 1: Build the C++ server**

```bash
cd D:/mb_workspace/farm_demo/scripts/server
# Build based on project's build system (CMake)
mkdir -p build && cd build
cmake .. && cmake --build .
```

Expected: Build succeeds with no errors related to PositionUpdate/PositionCorrect.

- [x] **Step 2: Start server and check handler registration**

Start the game server. Look for these log lines in order:

```
[Game]Position update handler registered
```

- [x] **Step 3: Start client and verify movement**

1. Start the client, log in
2. Move around with WASD
3. Verify no `AttributeError` on `player_pb2.PositionUpdate`
4. Check client logs for `PositionUpdate sent` messages

- [x] **Step 4: Verify boundary correction**

1. Walk player to map edge
2. Server logs should show boundary warnings if player clips edge
3. Player should be corrected back smoothly (200ms lerp)

- [x] **Step 5: Verify new player spawn**

1. Create a new player account
2. Player should spawn at tile (30, 25) — roughly center of farm map
3. Not at (0, 0) corner

- [x] **Step 6: Final commit with all changes**

```bash
git add -A
git commit -m "feat: complete player movement sync — proto, server validation, client fixes, spawn position"
```
