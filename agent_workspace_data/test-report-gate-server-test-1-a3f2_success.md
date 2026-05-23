# Gate Server Functional Test Report

**Date**: 2026-05-23
**Test Agent ID**: test-1-a3f2
**Overall Status**: PASS

---

## Overview

| Metric | Value |
|--------|-------|
| Total Tests | 6 |
| Passed | 6 |
| Failed | 0 |
| Skipped | 0 |
| Pass Rate | 100% |

---

## Compilation Result

| Item | Status |
|------|--------|
| Build Tool | VS2022 + MSVC + C++14 (CMake) |
| Build Status | SUCCESS |
| Warnings | None |
| Errors | None |

**Note**: A pre-existing bug was found and fixed during testing. The `main.cpp` was missing `WSAStartup` initialization on Windows, which caused `libevent` to fail with `socketpair: WSAStartup not called`. The fix adds `WSAStartup(MAKEWORD(2, 2), ...)` before server initialization and `WSACleanup()` at exit.

---

## Test Cases

| ID | Name | Status | Detail |
|----|------|--------|--------|
| TC-001 | Server startup and port listening | PASS | Successfully connected to localhost:8080 |
| TC-002 | TCP connection establishment | PASS | TCP connection established to localhost:8080 |
| TC-003 | Heartbeat message send/receive | PASS | Heartbeat response received. MsgID=1002, timestamp=1779506746 |
| TC-004 | Login message send/receive | PASS | Login response: code=0, msg="login success" |
| TC-005 | Sticky packet handling | PASS | Sent 2 messages in one send, received 2 responses correctly. MsgIDs=[1002, 1002] |
| TC-006 | Split packet handling | PASS | Split packet handled correctly. Response: code=0, msg="login success" |

### Test Case Details

#### TC-001: Server startup and port listening
- **Objective**: Verify the server can start and listen on port 8080
- **Method**: TCP socket connect to localhost:8080
- **Result**: Connection successful, server is listening

#### TC-002: TCP connection establishment
- **Objective**: Verify TCP connections can be established and closed cleanly
- **Method**: Create TCP socket, connect, then close
- **Result**: Connection established and closed without errors

#### TC-003: Heartbeat message send/receive
- **Objective**: Verify heartbeat request/response cycle
- **Method**: Send Heartbeat message (MsgID=1001) with protobuf timestamp, expect Heartbeat response (MsgID=1002)
- **Protocol**: `[4B length][4B MsgID=1001][Heartbeat protobuf]` -> `[4B length][4B MsgID=1002][Heartbeat protobuf]`
- **Result**: Response received with correct MsgID and server timestamp

#### TC-004: Login message send/receive
- **Objective**: Verify login request/response cycle
- **Method**: Send LoginReq (MsgID=2001) with token, expect LoginResp (MsgID=2002) with code=0
- **Protocol**: `[4B length][4B MsgID=2001][LoginReq protobuf]` -> `[4B length][4B MsgID=2002][LoginResp protobuf]`
- **Result**: Login successful with code=0, msg="login success"

#### TC-005: Sticky packet handling
- **Objective**: Verify server correctly handles multiple messages received in a single TCP segment
- **Method**: Concatenate two heartbeat messages and send in one `sendall()` call
- **Result**: Server correctly parsed both messages and sent two separate responses

#### TC-006: Split packet handling
- **Objective**: Verify server correctly handles a single message split across multiple TCP segments
- **Method**: Split a login message into 3 parts (4 bytes, 4 bytes, remaining) and send with small delays
- **Result**: Server correctly reassembled the message and sent the login response

---

## Defects Found

### BUG-001: Missing WSAStartup in main.cpp (Fixed)

- **Severity**: Critical
- **Description**: The `main.cpp` did not call `WSAStartup()` before initializing libevent on Windows. This caused the server to fail immediately with: `[warn] evsig_init_: socketpair: WSAStartup not called`
- **Root Cause**: Windows requires `WSAStartup` to be called before any Winsock functions. libevent's internal `socketpair` implementation relies on Winsock being initialized.
- **Fix Applied**: Added `WSAStartup(MAKEWORD(2, 2), &wsa_data)` at the beginning of `main()` and `WSACleanup()` at exit, wrapped in `#ifdef _WIN32`.
- **File Modified**: `D:\mb_workspace\farm_demo\scripts\server\gate_server\src\main.cpp`

---

## Recommendations

1. **WSAStartup Fix**: The WSAStartup fix in main.cpp should be committed to the repository. This is a critical fix for Windows compatibility.
2. **Heartbeat Timeout Test**: Consider adding a test that verifies the server disconnects clients after 15 seconds of no heartbeat (currently not tested due to time constraints).
3. **Unknown MsgID Test**: Consider adding a test that sends an unknown MsgID and verifies the server handles it gracefully (logs warning, does not crash).
4. **Multiple Concurrent Clients**: Consider testing multiple simultaneous TCP connections to verify session management.

---

## Test Coverage Assessment

| Area | Covered | Notes |
|------|---------|-------|
| Server startup | Yes | Port listening verified |
| TCP connection | Yes | Connect/close verified |
| Heartbeat protocol | Yes | Request/response cycle verified |
| Login protocol | Yes | Request/response cycle verified |
| Sticky packet (TCP粘包) | Yes | Multiple messages in one send |
| Split packet (TCP拆包) | Yes | One message split across sends |
| Heartbeat timeout | No | Server-side timeout not tested |
| Unknown message handling | No | Unknown MsgID not tested |
| Multiple concurrent clients | No | Only single client tested |
| Large message handling | No | Messages > 1MB not tested |

---

## Files Generated

- Test script: `D:\mb_workspace\farm_demo\agent_workspace_data\gate_server_test.py`
- Server output: `D:\mb_workspace\farm_demo\agent_workspace_data\server_output.txt`
- This report: `D:\mb_workspace\farm_demo\agent_workspace_data\test-report-gate-server-test-1-a3f2_success.md`
