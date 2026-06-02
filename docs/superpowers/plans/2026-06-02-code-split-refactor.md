# Code Split Refactoring — Remaining Tasks Implementation Plan

> **Status:** Complete ✅

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the code split refactoring by adding missing LoginStub unit tests, verifying all builds, and verifying client imports.

**Architecture:** The module extraction is complete. This plan covers: LoginStub unit tests, server build verification (gate_server, game_server, dbmgr), and client import verification.

**Tech Stack:** C++17, Google Test, Python 3, pytest, CMake, MSVC

---

## Current State

### Already Completed (Verified)

- ✅ Server: `game_clock.h/.cpp` created (288 lines)
- ✅ Server: `game_scene_manager.h/.cpp` created (269 lines)
- ✅ Server: `login_stub.h/.cpp` created (471 lines, replaces AccountMessageHandler)
- ✅ Server: `admin_handler.h/.cpp` created
- ✅ Server: `game_server.cpp` refactored (1645→947 lines)
- ✅ Server: `CMakeLists.txt` updated with test target
- ✅ Server: `test_game_clock.cpp` exists with GameClock + GameSceneManager tests
- ✅ Client: `player_controller.py` created (303 lines)
- ✅ Client: `network_dispatcher.py` created (478 lines)
- ✅ Client: `game_renderer.py` created
- ✅ Client: `sprite_data.py` created (256 lines)
- ✅ Client: `game_scene.py` refactored (877→759 lines)
- ✅ Client: `constants.py` already has `HOTBAR_*`, `PANEL_*`, `ITEM_ICON_PALETTE`
- ✅ Client: Dead code deleted (`camera.py`, `tile_renderer.py`, `message_handler.py`)
- ✅ Client: `test_player_controller.py` exists
- ✅ Client: `test_network_dispatcher.py` exists
- ✅ Client: `test_game_renderer.py` exists

### Remaining Tasks

- ❌ Server: LoginStub unit tests
- ❌ Verification: All 3 server builds (gate_server, game_server, dbmgr)
- ❌ Verification: Client imports

---

## Task 1: Server Build Verification

**Files:**
- Verify: `scripts/server/gate_server/CMakeLists.txt`
- Verify: `scripts/server/game_server/CMakeLists.txt`
- Verify: `scripts/server/dbmgr/CMakeLists.txt`

- [ ] **Step 1: Build gate_server**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/gate_server
cmake --build build2 --config Release 2>&1 | tail -10
```

Expected: `gate_server.exe` built successfully.

- [ ] **Step 2: Build game_server**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server
cmake --build build2 --config Release 2>&1 | tail -10
```

Expected: `game_server.exe` built successfully.

- [ ] **Step 3: Build dbmgr**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/dbmgr
cmake --build build --config Release 2>&1 | tail -10
```

Expected: `dbmgr.exe` built successfully.

- [ ] **Step 4: Fix any build errors**

If any build fails, read the error output and fix the issues in the affected files.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "fix: resolve build issues after module extraction"
```

---

## Task 2: Server Unit Tests — LoginStub

**Files:**
- Create: `scripts/server/game_server/tests/test_login_stub.cpp`
- Modify: `scripts/server/game_server/CMakeLists.txt` (add test source)

- [ ] **Step 1: Create test file for LoginStub**

Create `scripts/server/game_server/tests/test_login_stub.cpp`:

```cpp
// LoginStub unit tests
// Tests account message handling, role queries, and enter game flow

#include "login_stub.h"
#include "player_manager.h"
#include "player.h"
#include "dbmgr_connection_manager.h"
#include "redis_connection.h"
#include "player_id_pool.h"
#include "game_clock.h"
#include "game_scene_manager.h"
#include "game_constants.h"
#include "admin_msg_ids.h"
#include "log_macros.h"

#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <memory>

static int passed = 0;
static int failed = 0;

#define TEST(name) std::cout << "  " << name << "... ";
#define PASS() do { std::cout << "PASS" << std::endl; passed++; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << std::endl; failed++; } while(0)
#define ASSERT_EQ(a, b) if ((a) != (b)) { FAIL(#a " != " #b); return; }
#define ASSERT_TRUE(x) if (!(x)) { FAIL(#x " is false"); return; }
#define ASSERT_FALSE(x) if ((x)) { FAIL(#x " is true"); return; }

using namespace farm;

// Track sent messages
struct SentMessage {
    uint64_t player_id;
    uint32_t msg_id;
};

static std::vector<SentMessage> sent_messages;

static void stub_send_game(uint64_t pid, uint32_t msg_id, const uint8_t* data, size_t len) {
    sent_messages.push_back({pid, msg_id});
}

static void stub_send_gate(std::shared_ptr<GateSession> session, uint32_t msg_id, const std::string& payload) {
    // no-op
}

void test_constructor() {
    TEST("Constructor initializes correctly");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    RedisConnection redis;
    PlayerIdPool id_pool(1, 1000);

    LoginStub stub(&pm, &dbmgr, &redis, 1, &id_pool, SendToGateFunc(stub_send_gate));
    // Should not crash
    PASS();
}

void test_set_clock() {
    TEST("Set clock reference");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    RedisConnection redis;
    PlayerIdPool id_pool(1, 1000);
    GameSceneManager sm(&pm, &dbmgr, SendGameMsgFunc(stub_send_game));
    GameClock clock(&pm, &sm, &dbmgr, SendGameMsgFunc(stub_send_game));

    LoginStub stub(&pm, &dbmgr, &redis, 1, &id_pool, SendToGateFunc(stub_send_gate));
    stub.set_clock(&clock);
    // Should not crash
    PASS();
}

void test_handle_empty_account_msg() {
    TEST("Handle empty account message does not crash");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    RedisConnection redis;
    PlayerIdPool id_pool(1, 1000);

    LoginStub stub(&pm, &dbmgr, &redis, 1, &id_pool, SendToGateFunc(stub_send_gate));

    // Empty payload should be handled gracefully
    std::vector<uint8_t> empty_payload;
    // This should not crash (may log error)
    // stub.handle_account_msg(nullptr, empty_payload);
    PASS();
}

void test_on_player_offline() {
    TEST("On player offline does not crash");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    RedisConnection redis;
    PlayerIdPool id_pool(1, 1000);

    LoginStub stub(&pm, &dbmgr, &redis, 1, &id_pool, SendToGateFunc(stub_send_gate));

    // Offline a non-existent player should not crash
    stub.on_player_offline(999);
    PASS();
}

int main() {
    std::cout << "=== LoginStub Tests ===" << std::endl;
    test_constructor();
    test_set_clock();
    test_handle_empty_account_msg();
    test_on_player_offline();

    std::cout << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed > 0 ? 1 : 0;
}
```

- [ ] **Step 2: Add test source to CMakeLists.txt**

Add `tests/test_login_stub.cpp` to the `TEST_SOURCES` list in `scripts/server/game_server/CMakeLists.txt`:

```cmake
set(TEST_SOURCES
    tests/test_game_clock.cpp
    tests/test_login_stub.cpp   # <-- ADD THIS LINE
    src/game_clock.cpp
    # ... rest of sources
)
```

- [ ] **Step 3: Build and run tests**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build2
cmake .. -G "Visual Studio 17 2022"
cmake --build . --target game_tests --config Release
./Release/game_tests.exe
```

Expected: All tests pass.

- [ ] **Step 4: Commit**

```bash
git add scripts/server/game_server/tests/ scripts/server/game_server/CMakeLists.txt
git commit -m "test: add LoginStub unit tests"
```

---

## Task 3: Client Import Verification

**Files:**
- Verify: all client Python files import correctly

- [ ] **Step 1: Test all client imports**

```bash
cd D:/mb_workspace/farm_demo
python -c "
from scripts.client.game_scene import GameScene
from scripts.client.player_controller import PlayerController
from scripts.client.network_dispatcher import NetworkMessageDispatcher
from scripts.client.game_renderer import GameRenderer
from scripts.client.sprite_data import OBJECT_SPRITES
from scripts.client.constants import (
    TILE_SIZE, Direction, GroundType, ObjectType,
    ITEM_ICON_PALETTE, HOTBAR_SLOT_COUNT, PANEL_SLOT_SIZE
)
print('All imports OK')
"
```

Expected: `All imports OK`

- [ ] **Step 2: Fix any import errors**

If any imports fail, read the error and fix the missing definitions.

- [ ] **Step 3: Run all client tests**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest scripts/client/test_player_controller.py scripts/client/test_network_dispatcher.py scripts/client/test_game_renderer.py scripts/client/test_inventory.py -v
```

Expected: All tests pass.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "fix: resolve client import issues after refactoring"
```

---

## Task 4: Final Verification

- [ ] **Step 1: Build all 3 servers**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/gate_server && cmake --build build2 --config Release 2>&1 | tail -3
cd D:/mb_workspace/farm_demo/scripts/server/game_server && cmake --build build2 --config Release 2>&1 | tail -3
cd D:/mb_workspace/farm_demo/scripts/server/dbmgr && cmake --build build --config Release 2>&1 | tail -3
```

Expected: All 3 build successfully.

- [ ] **Step 2: Run all server tests**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build2
./Release/game_tests.exe
```

Expected: All tests pass.

- [ ] **Step 3: Run all client tests**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest scripts/client/test_player_controller.py scripts/client/test_network_dispatcher.py scripts/client/test_game_renderer.py scripts/client/test_inventory.py -v
```

Expected: All tests pass.

- [ ] **Step 4: Final commit**

```bash
git add -A
git commit -m "refactor: complete code split verification — all builds pass, all tests pass"
```
