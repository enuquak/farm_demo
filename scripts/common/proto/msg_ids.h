#pragma once

#include <cstdint>

namespace farm {

// ===========================================
// 公共消息 ID 常量 (Client <-> Gate <-> Game)
// ===========================================

// 基础消息 (0-999)
static constexpr uint32_t MSG_ID_HEARTBEAT              = 1;
static constexpr uint32_t MSG_ID_HEARTBEAT_RESP         = 2;
static constexpr uint32_t MSG_ID_LOGIN_REQ              = 3;
static constexpr uint32_t MSG_ID_LOGIN_RESP             = 4;

// 账号相关 (1000-1999)
static constexpr uint32_t MSG_ID_QUERY_ROLES_REQ        = 1001;
static constexpr uint32_t MSG_ID_QUERY_ROLES_RESP       = 1002;
static constexpr uint32_t MSG_ID_CREATE_ROLE_REQ        = 1003;
static constexpr uint32_t MSG_ID_CREATE_ROLE_RESP       = 1004;

// 进入游戏 (1005-1006, AccountMsg 通道)
static constexpr uint32_t MSG_ID_ENTER_GAME_REQ         = 1005;
static constexpr uint32_t MSG_ID_ENTER_GAME_RESP        = 1006;

// 物品使用 (3001-3002)
static constexpr uint32_t MSG_ID_ITEM_USE_REQ           = 3001;
static constexpr uint32_t MSG_ID_ITEM_USE_RESP          = 3002;

// 农场状态同步 (3003)
static constexpr uint32_t MSG_ID_FARM_STATE_SYNC        = 3003;

// 掉落物同步 (3004-3005)
static constexpr uint32_t MSG_ID_DROP_ITEM_SYNC         = 3004;

}  // namespace farm
