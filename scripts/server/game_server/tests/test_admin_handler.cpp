// AdminHandler unit tests
// Tests admin message handling: construction, unknown messages, shutdown flow

#include "test_admin_handler.h"
#include "admin_handler.h"
#include "admin_msg_ids.h"
#include "player_manager.h"
#include "game_scene_manager.h"
#include "game_clock.h"
#include "dbmgr_connection_manager.h"
#include "gate_session.h"
#include "game_types.h"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>

static int ah_passed = 0;
static int ah_failed = 0;

#define AH_TEST(name) std::cout << "  " << name << "... ";
#define AH_PASS() do { std::cout << "PASS" << std::endl; ah_passed++; } while(0)
#define AH_FAIL(msg) do { std::cout << "FAIL: " << msg << std::endl; ah_failed++; } while(0)
#define AH_ASSERT_TRUE(x) if (!(x)) { AH_FAIL(#x " is false"); return; }

using namespace farm;

// Stub send functions
static void ah_stub_send_game(uint64_t pid, uint32_t msg_id, const uint8_t* data, size_t len) {
    // no-op
}

static void ah_stub_send_gate(std::shared_ptr<GateSession> session, uint32_t msg_id, const std::string& payload) {
    // no-op
}

static void test_constructor() {
    AH_TEST("Constructor initializes correctly");

    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    GameSceneManager sm(&pm, &dbmgr, SendGameMsgFunc(ah_stub_send_game));
    GameClock clock(&pm, &sm, &dbmgr, SendGameMsgFunc(ah_stub_send_game));

    bool disable_called = false;
    bool stop_called = false;

    AdminHandler handler(
        &pm, &sm, &clock, &dbmgr,
        SendToGateFunc(ah_stub_send_gate),
        [&]() { disable_called = true; },
        [&]() { stop_called = true; }
    );

    // Callbacks should not have been invoked yet
    AH_ASSERT_TRUE(!disable_called);
    AH_ASSERT_TRUE(!stop_called);
    AH_PASS();
}

static void test_handle_unknown_message() {
    AH_TEST("Handle unknown message does not crash");

    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    GameSceneManager sm(&pm, &dbmgr, SendGameMsgFunc(ah_stub_send_game));
    GameClock clock(&pm, &sm, &dbmgr, SendGameMsgFunc(ah_stub_send_game));

    AdminHandler handler(
        &pm, &sm, &clock, &dbmgr,
        SendToGateFunc(ah_stub_send_gate),
        []() {},
        []() {}
    );

    // Pass an arbitrary unknown msg_id with empty payload
    std::vector<uint8_t> empty_payload;
    handler.handle(nullptr, 12345, empty_payload);

    // Should not crash
    AH_PASS();
}

static void test_handle_shutdown() {
    AH_TEST("Handle shutdown triggers disable_listener and stop_server");

    PlayerManager pm;
    DBMgrConnectionManager dbmgr;
    GameSceneManager sm(&pm, &dbmgr, SendGameMsgFunc(ah_stub_send_game));
    GameClock clock(&pm, &sm, &dbmgr, SendGameMsgFunc(ah_stub_send_game));

    bool disable_called = false;
    bool stop_called = false;

    AdminHandler handler(
        &pm, &sm, &clock, &dbmgr,
        SendToGateFunc(ah_stub_send_gate),
        [&]() { disable_called = true; },
        [&]() { stop_called = true; }
    );

    // Build a serialized shutdown message
    AdminShutdownMsg shutdown_msg;
    shutdown_msg.reason = "test shutdown";
    shutdown_msg.timeout_ms = 5000;
    std::string payload_str = shutdown_msg.serialize();
    std::vector<uint8_t> payload(payload_str.begin(), payload_str.end());

    handler.handle(nullptr, MSG_ID_SHUTDOWN, payload);

    AH_ASSERT_TRUE(disable_called);
    AH_ASSERT_TRUE(stop_called);
    AH_PASS();
}

int run_admin_handler_tests() {
    std::cout << std::endl << "=== AdminHandler Tests ===" << std::endl;
    test_constructor();
    test_handle_unknown_message();
    test_handle_shutdown();

    std::cout << "AdminHandler: " << ah_passed << " passed, " << ah_failed << " failed" << std::endl;
    return ah_failed;
}
