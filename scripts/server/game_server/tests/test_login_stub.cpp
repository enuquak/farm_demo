// LoginStub unit tests
// Tests account message handling, role queries, and enter game flow

#include "test_login_stub.h"
#include "login_stub.h"
#include "player_manager.h"
#include "player.h"
#include "dbmgr_connection_manager.h"
#include "redis_connection.h"
#include "player_id_pool.h"
#include "game_clock.h"
#include "game_scene_manager.h"
#include "game_constants.h"
#include "internal_msg_ids.h"
#include "message_ids.h"
#include "log_macros.h"

#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <memory>

static int ls_passed = 0;
static int ls_failed = 0;

#define LS_TEST(name) std::cout << "  " << name << "... ";
#define LS_PASS() do { std::cout << "PASS" << std::endl; ls_passed++; } while(0)
#define LS_FAIL(msg) do { std::cout << "FAIL: " << msg << std::endl; ls_failed++; } while(0)
#define LS_ASSERT_EQ(a, b) if ((a) != (b)) { LS_FAIL(#a " != " #b); return; }
#define LS_ASSERT_TRUE(x) if (!(x)) { LS_FAIL(#x " is false"); return; }
#define LS_ASSERT_FALSE(x) if ((x)) { LS_FAIL(#x " is true"); return; }

using namespace farm;

// Stub send functions
static void ls_stub_send_game(uint64_t pid, uint32_t msg_id, const uint8_t* data, size_t len) {
    // no-op
}

static void ls_stub_send_gate(std::shared_ptr<GateSession> session, uint32_t msg_id, const std::string& payload) {
    // no-op
}

static void test_constructor() {
    LS_TEST("Constructor initializes correctly");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    RedisConnection redis;
    PlayerIdPool id_pool(&dbmgr);

    LoginStub stub(&pm, &dbmgr, &redis, 1, &id_pool, SendToGateFunc(ls_stub_send_gate));
    // Should not crash
    LS_PASS();
}

static void test_set_clock() {
    LS_TEST("Set clock reference");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    RedisConnection redis;
    PlayerIdPool id_pool(&dbmgr);
    GameSceneManager sm(&pm, &dbmgr, SendGameMsgFunc(ls_stub_send_game));
    GameClock clock(&pm, &sm, &dbmgr, SendGameMsgFunc(ls_stub_send_game));

    LoginStub stub(&pm, &dbmgr, &redis, 1, &id_pool, SendToGateFunc(ls_stub_send_gate));
    stub.set_clock(&clock);
    // Should not crash
    LS_PASS();
}

static void test_handle_empty_account_msg() {
    LS_TEST("Handle empty account message does not crash");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    RedisConnection redis;
    PlayerIdPool id_pool(&dbmgr);

    LoginStub stub(&pm, &dbmgr, &redis, 1, &id_pool, SendToGateFunc(ls_stub_send_gate));

    // Empty payload should be handled gracefully
    // (LoginStub parses a default-constructed AccountMessage with msg_id=0 => "Unknown")
    std::vector<uint8_t> empty_payload;
    stub.handle_account_msg(nullptr, empty_payload);
    LS_PASS();
}

static void test_on_player_offline() {
    LS_TEST("On player offline does not crash");
    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    RedisConnection redis;
    PlayerIdPool id_pool(&dbmgr);

    LoginStub stub(&pm, &dbmgr, &redis, 1, &id_pool, SendToGateFunc(ls_stub_send_gate));

    // Offline a non-existent player should not crash
    // (Redis is not connected, so on_player_offline is a no-op)
    stub.on_player_offline(999);
    LS_PASS();
}

int run_login_stub_tests() {
    std::cout << std::endl << "=== LoginStub Tests ===" << std::endl;
    test_constructor();
    test_set_clock();
    test_handle_empty_account_msg();
    test_on_player_offline();

    std::cout << "LoginStub: " << ls_passed << " passed, " << ls_failed << " failed" << std::endl;
    return ls_failed;
}
