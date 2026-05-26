#pragma once

#include <cstdint>

namespace farm {

// ===========================================
// 内部消息 ID 常量 (Game <-> DBMgr)
// MsgID 范围: 4000-4299
// ===========================================

// 连接管理 (4000-4099)
static constexpr uint32_t MSG_ID_DBMGR_IDENTIFY          = 4001;
static constexpr uint32_t MSG_ID_DBMGR_IDENTIFY_RESP     = 4002;
static constexpr uint32_t MSG_ID_DBMGR_HEARTBEAT         = 4003;
static constexpr uint32_t MSG_ID_DBMGR_HEARTBEAT_RESP    = 4004;

// 数据操作 (4100-4199)
static constexpr uint32_t MSG_ID_PLAYER_DATA_REQ         = 4101;
static constexpr uint32_t MSG_ID_PLAYER_DATA_RESP        = 4102;

// 账号数据操作 (4200-4299)
static constexpr uint32_t MSG_ID_ACCOUNT_DATA_REQ        = 4201;
static constexpr uint32_t MSG_ID_ACCOUNT_DATA_RESP       = 4202;
static constexpr uint32_t MSG_ID_ACCOUNT_SET_REQ         = 4203;
static constexpr uint32_t MSG_ID_ACCOUNT_SET_RESP        = 4204;

// 身份识别超时时间（秒）
static constexpr int DBMGR_IDENTIFY_TIMEOUT = 10;

// 心跳相关常量
static constexpr int HEARTBEAT_INTERVAL = 5;    // 心跳发送间隔（秒）
static constexpr int HEARTBEAT_TIMEOUT = 15;    // 心跳超时时间（秒）

}  // namespace farm
