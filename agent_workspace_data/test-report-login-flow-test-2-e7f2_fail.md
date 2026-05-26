# Login Flow Test Report

## Overview
- **Test ID**: test-2-e7f2
- **Date**: 2026-05-26
- **Branch**: proposal/add-player-login
- **Spec**: openspec/changes/add-player-login/specs/login-flow/spec.md
- **Test Type**: 第2次测试（缺陷修复后重试）
- **Overall Status**: **FAIL**

## Summary
| Metric | Count |
|--------|-------|
| Total Tests | 7 |
| PASS | 1 |
| FAIL | 0 |
| ERROR | 5 |
| SKIP | 1 |
| **Pass Rate** | **14.3%** |

## Compilation Results

### Gate Server
- **Status**: SUCCESS (已编译，exe 存在)
- **Path**: `D:/mb_workspace/farm_demo/scripts/server/gate_server/Release/Release/gate_server.exe`
- **DLL**: event.dll, event_core.dll, event_extra.dll 齐全

### Game Server
- **Status**: **FAIL** (编译错误)
- **Path**: `D:/mb_workspace/farm_demo/scripts/server/game_server/Release/game_server.exe`
- **Error**: `game_server.cpp:464 - send_player_data_req` 函数签名不匹配
  - 期望: `send_player_data_req(uint64_t, int32_t, string, string, callback)`
  - 实际调用: `send_player_data_req(uint64_t, "all", callback)`
- **影响**: Game Server 无法重新编译，现有 exe (May 25 23:47) 不包含账号消息处理代码

### DBMgr
- **Status**: SUCCESS (已编译，exe 存在)
- **Path**: `D:/mb_workspace/farm_demo/scripts/server/dbmgr/Release/Release/dbmgr.exe`

## Test Cases

| ID | Name | Status | Details |
|----|------|--------|---------|
| TC-01 | Connect and Login | **PASS** | code=0, login success |
| TC-02 | Query Roles (new account) | **ERROR** | timed out - Game Server 未处理 MSG_ID_QUERY_ROLES_REQ |
| TC-03 | Create Role | **ERROR** | timed out - Game Server 未处理 MSG_ID_CREATE_ROLE_REQ |
| TC-04 | Query Roles (after creation) | **ERROR** | timed out |
| TC-05 | Create Duplicate Role | **ERROR** | timed out |
| TC-06 | Enter Game | **SKIP** | 依赖 TC-03 的 player_id |
| TC-07 | Enter Game (invalid) | **ERROR** | timed out |

## Server Startup Verification

| Server | Port | Status | Notes |
|--------|------|--------|-------|
| DBMgr | 10010 | LISTENING | 正常运行 |
| Game Server | 9090 | LISTENING | 连接到 DBMgr 成功 |
| Gate Server | 8080 | LISTENING | 连接到 Game Server 成功，身份识别完成 |

## Root Cause Analysis

### Defect: Game Server 编译错误
- **Location**: `scripts/server/game_server/src/game_server.cpp:464`
- **Function**: `handle_enter_game_req`
- **Error**: `send_player_data_req` 函数调用参数不匹配
- **Impact**: Game Server 无法重新编译，导致:
  1. 现有 exe 不包含 `handle_account_msg` 函数的账号消息处理逻辑
  2. MSG_ID_QUERY_ROLES_REQ (1001) 和 MSG_ID_CREATE_ROLE_REQ (1003) 被识别为 "Unknown account msg_id"
  3. 所有依赖 Game Server 处理的账号消息测试用例超时

### Log Evidence
```
[GameServer] Account message: account_id=test_user_login_001 msg_id=1001
[GameServer] Unknown account msg_id=1001
[GameServer] Account message: account_id=test_user_login_001 msg_id=1003
[GameServer] Unknown account msg_id=1003
```

## Defects Found

### BUG-001: Game Server send_player_data_req 签名不匹配
- **Severity**: HIGH
- **Location**: `scripts/server/game_server/src/game_server.cpp:464`
- **Description**: `handle_enter_game_req` 函数调用 `send_player_data_req` 时参数数量不正确
- **Expected**: `dbmgr_mgr_.send_player_data_req(req_player_id, 3, "", "", callback)` (GET_ALL=3)
- **Actual**: `dbmgr_mgr_.send_player_data_req(req_player_id, "all", callback)`
- **Impact**: Game Server 无法编译，阻止所有依赖 Game Server 的功能测试
- **Fix**: 修改 `handle_enter_game_req` 函数，使用正确的 `send_player_data_req` 签名

## Recommendations

1. **修复编译错误**: 修改 `game_server.cpp:464` 的 `send_player_data_req` 调用，使用正确的参数
2. **重新编译 Game Server**: 修复后重新编译，确保 exe 包含最新的账号消息处理代码
3. **重新执行测试**: 编译成功后重新运行完整的 login flow 测试

## Test Coverage Assessment

| Requirement | Coverage | Status |
|-------------|----------|--------|
| 查询角色列表 | 未测试 | BLOCKED (编译错误) |
| 创建新角色 | 未测试 | BLOCKED (编译错误) |
| 登录进入游戏 | 未测试 | BLOCKED (编译错误) |
| Gate 消息路由 | 部分测试 | TC-01 验证了登录消息路由 |
| Gate 配置管理 | 未测试 | N/A |

## Test Environment
- **OS**: Windows 11 Pro
- **Python**: 3.12.10
- **Protobuf**: 7.34.1
- **Build Tool**: VS2022 + MSVC + C++14
