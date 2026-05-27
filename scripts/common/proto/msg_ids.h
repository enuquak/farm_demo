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

// 玩家移动 (2100-2199)
static constexpr uint32_t MSG_ID_POSITION_UPDATE         = 2101;
static constexpr uint32_t MSG_ID_POSITION_CORRECT        = 2102;

// 场景切换 (2200-2299)
static constexpr uint32_t MSG_ID_SCENE_CHANGE_REQ        = 2201;
static constexpr uint32_t MSG_ID_SCENE_CHANGE_RESP       = 2202;

// 场景数据 (2000-2099)
static constexpr uint32_t MSG_ID_MAP_DATA_NOTIFY         = 2001;

// 背包同步 (3100-3199)
static constexpr uint32_t MSG_ID_INVENTORY_SYNC          = 3100;
static constexpr uint32_t MSG_ID_ACTIVE_SLOT_CHANGE      = 3101;

// 游戏时钟 (2300-2399)
static constexpr uint32_t MSG_ID_CLOCK_SYNC              = 2301;
static constexpr uint32_t MSG_ID_FORCE_SLEEP_NOTIFY      = 2302;
static constexpr uint32_t MSG_ID_FORCE_SLEEP_READY       = 2303;

}  // namespace farm
