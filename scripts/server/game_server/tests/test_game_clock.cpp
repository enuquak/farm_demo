// GameClock integration test
// Tests clock advancement logic using real dependencies

#include "test_login_stub.h"
#include "game_clock.h"
#include "game_scene_manager.h"
#include "player_manager.h"
#include "player.h"
#include "game_constants.h"
#include "dbmgr_connection_manager.h"
#include "log_macros.h"

#include <iostream>
#include <cassert>
#include <string>
#include <vector>

static int passed = 0;
static int failed = 0;

#define TEST(name) std::cout << "  " << name << "... ";
#define PASS() do { std::cout << "PASS" << std::endl; passed++; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << std::endl; failed++; } while(0)
#define ASSERT_EQ(a, b) if ((a) != (b)) { FAIL(#a " != " #b); return; }
#define ASSERT_TRUE(x) if (!(x)) { FAIL(#x " is false"); return; }

using namespace farm;

// Stub send function
static void stub_send(uint64_t pid, uint32_t msg_id, const uint8_t* data, size_t len) {
    // no-op
}

void test_initial_state() {
    TEST("Initial state");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    GameSceneManager sm(&pm, &dbmgr, SendGameMsgFunc(stub_send));
    GameClock clock(&pm, &sm, &dbmgr, SendGameMsgFunc(stub_send));

    ASSERT_EQ(clock.day(), 1);
    ASSERT_EQ(clock.time_slot(), 0);
    ASSERT_TRUE(!clock.paused());
    PASS();
}

void test_tick_advances_elapsed() {
    TEST("Tick advances elapsed");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    GameSceneManager sm(&pm, &dbmgr, SendGameMsgFunc(stub_send));
    GameClock clock(&pm, &sm, &dbmgr, SendGameMsgFunc(stub_send));

    // One tick = 1 second
    clock.update();
    ASSERT_EQ(clock.time_slot(), 0);  // still in first slot
    ASSERT_EQ(clock.day(), 1);
    PASS();
}

void test_time_slot_advances() {
    TEST("Time slot advances after 60 ticks");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    GameSceneManager sm(&pm, &dbmgr, SendGameMsgFunc(stub_send));
    GameClock clock(&pm, &sm, &dbmgr, SendGameMsgFunc(stub_send));

    // 60 ticks = 1 time_slot
    for (int i = 0; i < 60; i++) {
        clock.update();
    }
    ASSERT_EQ(clock.time_slot(), 1);
    ASSERT_EQ(clock.day(), 1);
    PASS();
}

void test_day_end() {
    TEST("Day ends after 40 slots (2400 ticks)");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    GameSceneManager sm(&pm, &dbmgr, SendGameMsgFunc(stub_send));
    GameClock clock(&pm, &sm, &dbmgr, SendGameMsgFunc(stub_send));

    // 40 slots * 60 ticks = 2400 ticks
    for (int i = 0; i < 2400; i++) {
        clock.update();
    }

    // Day should have ended, clock paused
    ASSERT_EQ(clock.day(), 1);  // day hasn't advanced yet (on_day_end pauses)
    ASSERT_TRUE(clock.paused());
    // No players in manager, so no messages sent, but clock state is correct
    PASS();
}

void test_max_energy() {
    TEST("MAX_ENERGY constant");
    ASSERT_EQ(MAX_ENERGY, 100);
    PASS();
}

void test_scene_manager_create() {
    TEST("Scene manager creates scenes");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    GameSceneManager sm(&pm, &dbmgr, SendGameMsgFunc(stub_send));

    auto* s1 = sm.get_or_create_scene("farm");
    ASSERT_TRUE(s1 != nullptr);

    auto* s2 = sm.get_or_create_scene("farm");
    ASSERT_EQ(s1, s2);  // same scene returned

    auto* s3 = sm.get_or_create_scene("house");
    ASSERT_TRUE(s3 != nullptr);
    ASSERT_TRUE(s3 != s1);  // different scene
    PASS();
}

void test_scene_manager_get_nonexistent() {
    TEST("Get nonexistent scene returns nullptr");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    GameSceneManager sm(&pm, &dbmgr, SendGameMsgFunc(stub_send));

    auto s = sm.get_scene("nonexistent");
    ASSERT_TRUE(!s.has_value());
    PASS();
}

int main() {
    std::cout << "=== GameClock Tests ===" << std::endl;
    test_initial_state();
    test_tick_advances_elapsed();
    test_time_slot_advances();
    test_day_end();
    test_max_energy();

    std::cout << std::endl << "=== GameSceneManager Tests ===" << std::endl;
    test_scene_manager_create();
    test_scene_manager_get_nonexistent();

    // Run LoginStub tests
    failed += run_login_stub_tests();

    std::cout << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed > 0 ? 1 : 0;
}
