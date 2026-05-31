# Code Split Refactoring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the code split refactoring by verifying builds, fixing remaining issues, and writing comprehensive unit tests for all extracted modules.

**Architecture:** The module extraction is already complete (done by background agents). This plan covers: build verification, constants.py fix, and unit tests for 4 server modules + 3 client modules.

**Tech Stack:** C++17, Google Test, Python 3, pytest, CMake, MSVC

---

## Current State

The background agents already completed:
- ✅ Server: `game_clock.h/.cpp`, `game_scene_manager.h/.cpp`, `account_message_handler.h/.cpp`, `admin_handler.h/.cpp` created
- ✅ Server: `game_server.cpp` refactored to use new modules
- ✅ Server: `CMakeLists.txt` updated
- ✅ Client: `player_controller.py`, `network_dispatcher.py`, `game_renderer.py`, `sprite_data.py` created
- ✅ Client: `game_scene.py` refactored to use new modules
- ✅ Client: Dead code deleted (`camera.py`, `tile_renderer.py`, `message_handler.py`)
- ✅ Server: `game_server` Release build passes

Remaining:
- ❌ Server: `gate_server` and `dbmgr` Release build verification
- ❌ Client: `constants.py` missing imports fix (`HOTBAR_*`, `PANEL_*`, `ITEM_ICON_PALETTE`)
- ❌ Server: Unit tests for 4 new modules
- ❌ Client: Unit tests for 3 new modules
- ❌ Client: Import verification

---

## Task 1: Server Build Verification

**Files:**
- Verify: `scripts/server/gate_server/CMakeLists.txt`
- Verify: `scripts/server/dbmgr/CMakeLists.txt`

- [ ] **Step 1: Build gate_server**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/gate_server
cmake --build build2 --config Release 2>&1 | tail -5
```

Expected: `gate_server.exe` built successfully.

- [ ] **Step 2: Build dbmgr**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/dbmgr
cmake --build build --config Release 2>&1 | tail -5
```

Expected: `dbmgr.exe` built successfully.

- [ ] **Step 3: Fix any build errors**

If either build fails, read the error output and fix the issues in the affected files.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "fix: resolve build issues after module extraction"
```

---

## Task 2: Fix constants.py Missing Imports

**Files:**
- Modify: `scripts/client/constants.py`
- Read: `scripts/client/icon_manager.py` (to understand ITEM_ICON_PALETTE format)
- Read: `scripts/client/ui/hotbar.py` (to understand HOTBAR_* constants)
- Read: `scripts/client/ui/inventory_panel.py` (to understand PANEL_* constants)

- [ ] **Step 1: Read icon_manager.py to understand ITEM_ICON_PALETTE format**

Read `scripts/client/icon_manager.py` and find how `ITEM_ICON_PALETTE` is used. It maps item_id to (primary_color, secondary_color) tuples for rendering 8x8 pixel icons.

- [ ] **Step 2: Read ui/hotbar.py to understand HOTBAR_* constants**

Read `scripts/client/ui/hotbar.py` to find all HOTBAR_* imports and their expected types/values.

- [ ] **Step 3: Read ui/inventory_panel.py to understand PANEL_* constants**

Read `scripts/client/ui/inventory_panel.py` to find all PANEL_* imports and their expected types/values.

- [ ] **Step 4: Add missing constants to constants.py**

Add the following to `scripts/client/constants.py`:

```python
# ========== Icon Manager ==========
# Item icon color palette: item_id -> (primary_color, secondary_color)
ITEM_ICON_PALETTE = {
    3: ((139, 90, 43), (100, 60, 20)),    # axe: brown handle
    4: ((139, 90, 43), (160, 160, 160)),   # hoe: brown handle, gray head
    5: ((34, 139, 34), (50, 205, 50)),     # seeds: green
    6: ((210, 180, 140), (245, 222, 179)), # bread: wheat colors
    7: ((255, 165, 0), (255, 200, 50)),    # crop: orange/yellow
}

# ========== Hotbar ==========
HOTBAR_SLOT_COUNT = 10
HOTBAR_SLOT_SIZE = 40
HOTBAR_SLOT_GAP = 4
HOTBAR_MARGIN_BOTTOM = 10
HOTBAR_BG_COLOR = (40, 40, 40)
HOTBAR_BORDER_COLOR = (100, 100, 100)
HOTBAR_ACTIVE_COLOR = (255, 215, 0)
HOTBAR_TEXT_COLOR = (255, 255, 255)
HOTBAR_TEXT_SHADOW_COLOR = (0, 0, 0)

# ========== Inventory Panel ==========
PANEL_SLOT_SIZE = 48
PANEL_SLOT_GAP = 4
PANEL_COLS = 6
PANEL_ROWS = 5
PANEL_BG_COLOR = (30, 30, 30)
PANEL_BORDER_COLOR = (100, 100, 100)
PANEL_TITLE_COLOR = (255, 215, 0)
PANEL_CLOSE_BTN_COLOR = (200, 50, 50)
PANEL_OVERLAY_ALPHA = 180
```

- [ ] **Step 5: Verify imports work**

```bash
cd D:/mb_workspace/farm_demo
python -c "from scripts.client.constants import ITEM_ICON_PALETTE, HOTBAR_SLOT_COUNT, PANEL_SLOT_SIZE; print('OK')"
```

Expected: `OK`

- [ ] **Step 6: Commit**

```bash
git add scripts/client/constants.py
git commit -m "fix: add missing HOTBAR_*, PANEL_*, ITEM_ICON_PALETTE constants"
```

---

## Task 3: Server Unit Tests — GameClock

**Files:**
- Create: `scripts/server/game_server/tests/test_game_clock.cpp`
- Modify: `scripts/server/game_server/CMakeLists.txt` (add test target)

- [ ] **Step 1: Create test file for GameClock**

Create `scripts/server/game_server/tests/test_game_clock.cpp`:

```cpp
#include <gtest/gtest.h>
#include "game_clock.h"
#include "player_manager.h"
#include "game_scene_manager.h"
#include "dbmgr_connection_manager.h"

using namespace farm;

class GameClockTest : public ::testing::Test {
protected:
    void SetUp() override {
        player_mgr = std::make_unique<PlayerManager>();
        dbmgr_mgr = std::make_unique<DBMgrConnectionManager>();
        scene_mgr = std::make_unique<GameSceneManager>(player_mgr.get(), dbmgr_mgr.get(),
            [](uint64_t, uint32_t, const uint8_t*, size_t) {});
        clock = std::make_unique<GameClock>(player_mgr.get(), scene_mgr.get(), dbmgr_mgr.get(),
            [](uint64_t, uint32_t, const uint8_t*, size_t) {});
    }

    std::unique_ptr<PlayerManager> player_mgr;
    std::unique_ptr<DBMgrConnectionManager> dbmgr_mgr;
    std::unique_ptr<GameSceneManager> scene_mgr;
    std::unique_ptr<GameClock> clock;
};

TEST_F(GameClockTest, InitialState) {
    EXPECT_EQ(clock->day(), 1);
    EXPECT_EQ(clock->week(), 1);
    EXPECT_EQ(clock->month(), 1);
    EXPECT_EQ(clock->time_slot(), 0);
}

TEST_F(GameClockTest, TickAdvancesTimeSlot) {
    // 60 ticks = 1 time_slot
    for (int i = 0; i < 60; i++) {
        clock->tick();
    }
    EXPECT_EQ(clock->time_slot(), 1);
    EXPECT_EQ(clock->day(), 1);
}

TEST_F(GameClockTest, DayUpdateFires) {
    bool day_fired = false;
    int32_t fired_day = 0;
    clock->on(TimeEvent::DAY_UPDATE, [&](int32_t day, int32_t week, int32_t month) {
        day_fired = true;
        fired_day = day;
    });

    // 40 time_slots * 60 ticks = 2400 ticks = 1 day
    for (int i = 0; i < 2400; i++) {
        clock->tick();
    }
    EXPECT_TRUE(day_fired);
    EXPECT_EQ(fired_day, 2);
    EXPECT_EQ(clock->day(), 2);
}

TEST_F(GameClockTest, WeekUpdateFires) {
    bool week_fired = false;
    clock->on(TimeEvent::WEEK_UPDATE, [&](int32_t day, int32_t week, int32_t month) {
        week_fired = true;
    });

    // 7 days * 2400 ticks = 16800 ticks
    for (int i = 0; i < 16800; i++) {
        clock->tick();
    }
    EXPECT_TRUE(week_fired);
    EXPECT_EQ(clock->week(), 2);
}

TEST_F(GameClockTest, MonthUpdateFires) {
    bool month_fired = false;
    clock->on(TimeEvent::MONTH_UPDATE, [&](int32_t day, int32_t week, int32_t month) {
        month_fired = true;
    });

    // 30 days * 2400 ticks = 72000 ticks
    for (int i = 0; i < 72000; i++) {
        clock->tick();
    }
    EXPECT_TRUE(month_fired);
    EXPECT_EQ(clock->month(), 2);
}

TEST_F(GameClockTest, MultipleCallbacksOnSameEvent) {
    int call_count = 0;
    clock->on(TimeEvent::DAY_UPDATE, [&](int32_t, int32_t, int32_t) { call_count++; });
    clock->on(TimeEvent::DAY_UPDATE, [&](int32_t, int32_t, int32_t) { call_count++; });

    for (int i = 0; i < 2400; i++) {
        clock->tick();
    }
    EXPECT_EQ(call_count, 2);
}

TEST_F(GameClockTest, WeekCalculation) {
    // Day 1 = week 1, Day 7 = week 1, Day 8 = week 2
    // Advance to day 8
    for (int i = 0; i < 2400 * 7; i++) {
        clock->tick();
    }
    EXPECT_EQ(clock->day(), 8);
    EXPECT_EQ(clock->week(), 2);
}

TEST_F(GameClockTest, MonthCalculation) {
    // Day 1 = month 1, Day 31 = month 2
    for (int i = 0; i < 2400 * 30; i++) {
        clock->tick();
    }
    EXPECT_EQ(clock->day(), 31);
    EXPECT_EQ(clock->month(), 2);
}
```

- [ ] **Step 2: Add test target to CMakeLists.txt**

Add to `scripts/server/game_server/CMakeLists.txt` (after the main executable):

```cmake
# Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(game_clock_test
    tests/test_game_clock.cpp
    src/game_clock.cpp
    src/scene_state.cpp
    src/world_state.cpp
    src/crop_system.cpp
    src/drop_item_manager.cpp
    src/player.cpp
    src/player_manager.cpp
    src/message_parser.cpp
    src/item_interaction_handler.cpp
    src/item_effects.cpp
    ${PROTO_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/admin_msg_ids.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/log_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/message_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/msvc_compat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/server_main_helper.cpp
)
target_include_directories(game_clock_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROTOBUF_ROOT}/include
    ${LIBEVENT_ROOT}/include
    ${COMMON_PROTO_DIR}
    ${COMMON_PROTO_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)
target_link_libraries(game_clock_test PRIVATE
    gtest_main
    libprotobuf-lite.lib
    libutf8_range.lib
    libutf8_validity.lib
    event.lib
    event_core.lib
    event_extra.lib
    ws2_32.lib
)
include(GoogleTest)
gtest_discover_tests(game_clock_test)
```

- [ ] **Step 3: Build and run tests**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build2
cmake .. -G "Visual Studio 17 2022"
cmake --build . --target game_clock_test --config Release
ctest -C Release --output-on-failure
```

Expected: All tests pass.

- [ ] **Step 4: Commit**

```bash
git add scripts/server/game_server/tests/ scripts/server/game_server/CMakeLists.txt
git commit -m "test: add GameClock unit tests"
```

---

## Task 4: Server Unit Tests — GameSceneManager

**Files:**
- Create: `scripts/server/game_server/tests/test_game_scene_manager.cpp`

- [ ] **Step 1: Create test file**

Create `scripts/server/game_server/tests/test_game_scene_manager.cpp`:

```cpp
#include <gtest/gtest.h>
#include "game_scene_manager.h"
#include "player_manager.h"
#include "dbmgr_connection_manager.h"

using namespace farm;

class GameSceneManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        player_mgr = std::make_unique<PlayerManager>();
        dbmgr_mgr = std::make_unique<DBMgrConnectionManager>();
        scene_mgr = std::make_unique<GameSceneManager>(player_mgr.get(), dbmgr_mgr.get(),
            [this](uint64_t pid, uint32_t mid, const uint8_t* d, size_t l) {
                last_sent_pid = pid;
                last_sent_msg_id = mid;
            });
    }

    std::unique_ptr<PlayerManager> player_mgr;
    std::unique_ptr<DBMgrConnectionManager> dbmgr_mgr;
    std::unique_ptr<GameSceneManager> scene_mgr;
    uint64_t last_sent_pid = 0;
    uint32_t last_sent_msg_id = 0;
};

TEST_F(GameSceneManagerTest, GetOrCreateNewScene) {
    auto* scene = scene_mgr->get_or_create_scene("farm");
    EXPECT_NE(scene, nullptr);
}

TEST_F(GameSceneManagerTest, GetOrCreateExistingScene) {
    auto* scene1 = scene_mgr->get_or_create_scene("farm");
    auto* scene2 = scene_mgr->get_or_create_scene("farm");
    EXPECT_EQ(scene1, scene2);
}

TEST_F(GameSceneManagerTest, MultipleScenes) {
    auto* farm = scene_mgr->get_or_create_scene("farm");
    auto* house = scene_mgr->get_or_create_scene("house");
    EXPECT_NE(farm, house);
}
```

- [ ] **Step 2: Add test target to CMakeLists.txt**

Add a similar `game_scene_manager_test` target to CMakeLists.txt (following the pattern from Task 3).

- [ ] **Step 3: Build and run tests**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build2
cmake --build . --target game_scene_manager_test --config Release
ctest -C Release --output-on-failure
```

Expected: All tests pass.

- [ ] **Step 4: Commit**

```bash
git add scripts/server/game_server/tests/
git commit -m "test: add GameSceneManager unit tests"
```

---

## Task 5: Client Unit Tests — PlayerController

**Files:**
- Create: `scripts/client/test_player_controller.py`

- [ ] **Step 1: Create test file**

Create `scripts/client/test_player_controller.py`:

```python
"""Unit tests for PlayerController"""
import unittest
from unittest.mock import MagicMock, patch
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__)))

from constants import Direction, TILE_SIZE
from player_controller import PlayerController


class TestPlayerController(unittest.TestCase):
    def setUp(self):
        self.connection = MagicMock()
        self.tmx_map = MagicMock()
        self.tmx_map.width = 50
        self.tmx_map.height = 50
        self.tmx_map.is_walkable.return_value = True

        self.player_sprite = MagicMock()
        self.player_sprite.world_x = 5.0 * TILE_SIZE
        self.player_sprite.world_y = 5.0 * TILE_SIZE

        self.scene_manager = MagicMock()
        self.scene_manager.get_current_map_bounds.return_value = (50 * TILE_SIZE, 50 * TILE_SIZE)

        self.controller = PlayerController(
            connection=self.connection,
            tmx_map=self.tmx_map,
            player_sprite=self.player_sprite,
            scene_manager=self.scene_manager,
        )

    def test_initial_facing(self):
        self.assertEqual(self.controller.facing, Direction.DOWN)

    def test_start_correction(self):
        self.controller.start_correction(100.0, 200.0)
        self.assertIsNotNone(self.controller.correction_target)

    def test_update_position_sending_no_send_when_no_movement(self):
        """Should not send position when player hasn't moved"""
        initial_timer = self.controller.send_timer
        self.controller.update_position_sending(0.01)
        # Timer should accumulate but not send yet
        self.assertGreaterEqual(self.controller.send_timer, initial_timer)


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run tests**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest scripts/client/test_player_controller.py -v
```

Expected: All tests pass.

- [ ] **Step 3: Commit**

```bash
git add scripts/client/test_player_controller.py
git commit -m "test: add PlayerController unit tests"
```

---

## Task 6: Client Unit Tests — NetworkMessageDispatcher

**Files:**
- Create: `scripts/client/test_network_dispatcher.py`

- [ ] **Step 1: Create test file**

Create `scripts/client/test_network_dispatcher.py`:

```python
"""Unit tests for NetworkMessageDispatcher"""
import unittest
from unittest.mock import MagicMock, patch
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__)))

from network_dispatcher import NetworkMessageDispatcher
from msg_ids import MSG_ID_MAP_DATA, MSG_ID_CLOCK_SYNC, MSG_ID_FORCE_SLEEP_NOTIFY


class TestNetworkMessageDispatcher(unittest.TestCase):
    def setUp(self):
        self.dispatcher = NetworkMessageDispatcher(
            connection=MagicMock(),
            scene_manager=MagicMock(),
            player_controller=MagicMock(),
            renderer=MagicMock(),
            on_map_data_notify=MagicMock(),
            on_position_correct=MagicMock(),
            on_item_use_resp=MagicMock(),
            on_scene_change_resp=MagicMock(),
            on_clock_sync=MagicMock(),
            on_force_sleep_notify=MagicMock(),
        )

    def test_handler_registry(self):
        """All expected message IDs should have handlers"""
        self.assertIn(MSG_ID_MAP_DATA, self.dispatcher.handlers)
        self.assertIn(MSG_ID_CLOCK_SYNC, self.dispatcher.handlers)
        self.assertIn(MSG_ID_FORCE_SLEEP_NOTIFY, self.dispatcher.handlers)

    def test_dispatch_calls_correct_handler(self):
        """dispatch_pending should call the correct handler for each message"""
        self.dispatcher.connection.recv_all_messages.return_value = [
            (MSG_ID_CLOCK_SYNC, b'\x08\x01'),
        ]
        self.dispatcher.dispatch_pending()
        self.dispatcher.on_clock_sync.assert_called_once()

    def test_dispatch_ignores_unknown_message(self):
        """Unknown message IDs should be silently ignored"""
        self.dispatcher.connection.recv_all_messages.return_value = [
            (9999, b'some_payload'),
        ]
        # Should not raise
        self.dispatcher.dispatch_pending()


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run tests**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest scripts/client/test_network_dispatcher.py -v
```

Expected: All tests pass.

- [ ] **Step 3: Commit**

```bash
git add scripts/client/test_network_dispatcher.py
git commit -m "test: add NetworkMessageDispatcher unit tests"
```

---

## Task 7: Client Unit Tests — GameRenderer

**Files:**
- Create: `scripts/client/test_game_renderer.py`

- [ ] **Step 1: Create test file**

Create `scripts/client/test_game_renderer.py`:

```python
"""Unit tests for GameRenderer"""
import unittest
from unittest.mock import MagicMock, patch
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__)))

from game_renderer import GameRenderer


class TestGameRenderer(unittest.TestCase):
    @patch('game_renderer.pygame')
    def setUp(self, mock_pygame):
        self.screen = MagicMock()
        self.renderer = GameRenderer(self.screen, 800, 600, {})

    def test_has_all_components(self):
        """Renderer should have all UI components"""
        self.assertTrue(hasattr(self.renderer, 'hud'))
        self.assertTrue(hasattr(self.renderer, 'energy_bar'))
        self.assertTrue(hasattr(self.renderer, 'time_hud'))
        self.assertTrue(hasattr(self.renderer, 'exhaustion_modal'))

    def test_cleanup(self):
        """cleanup should not raise"""
        self.renderer.cleanup()


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run tests**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest scripts/client/test_game_renderer.py -v
```

Expected: All tests pass.

- [ ] **Step 3: Commit**

```bash
git add scripts/client/test_game_renderer.py
git commit -m "test: add GameRenderer unit tests"
```

---

## Task 8: Client Import Verification

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

## Task 9: Final Verification

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
ctest -C Release --output-on-failure
```

Expected: All tests pass.

- [ ] **Step 3: Run all client tests**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest scripts/client/ -v --ignore=scripts/client/test_client_connection.py --ignore=scripts/client/test_client_connection_extended.py
```

Expected: All tests pass.

- [ ] **Step 4: Final commit**

```bash
git add -A
git commit -m "refactor: complete code split — server 4 modules + client 3 modules with tests"
```
