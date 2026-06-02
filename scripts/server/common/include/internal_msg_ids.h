#pragma once

#include <cstdint>

namespace farm {

// ===========================================
// 内部消息 ID 常量
// ===========================================

// Gate <-> Game (3000-3399)

// 连接管理 (3000-3099)
inline constexpr uint32_t MSG_ID_INTERN_HEARTBEAT      = 3001;
inline constexpr uint32_t MSG_ID_INTERN_HEARTBEAT_RESP = 3002;
inline constexpr uint32_t MSG_ID_GATE_IDENTIFY         = 3003;
inline constexpr uint32_t MSG_ID_GATE_IDENTIFY_RESP    = 3004;

// 玩家生命周期 (3100-3199)
inline constexpr uint32_t MSG_ID_PLAYER_JOIN           = 3101;
inline constexpr uint32_t MSG_ID_PLAYER_JOIN_RESP      = 3102;
inline constexpr uint32_t MSG_ID_PLAYER_LEAVE          = 3103;

// 消息转发 (3200-3299)
inline constexpr uint32_t MSG_ID_CLIENT_MSG            = 3201;
inline constexpr uint32_t MSG_ID_GAME_MSG              = 3202;

// 账号消息转发 (3300-3399)
inline constexpr uint32_t MSG_ID_ACCOUNT_MSG           = 3301;
inline constexpr uint32_t MSG_ID_ACCOUNT_MSG_RESP      = 3302;

// Game <-> DBMgr (4000-4299)

// 连接管理 (4000-4099)
inline constexpr uint32_t MSG_ID_DBMGR_IDENTIFY          = 4001;
inline constexpr uint32_t MSG_ID_DBMGR_IDENTIFY_RESP     = 4002;
inline constexpr uint32_t MSG_ID_DBMGR_HEARTBEAT         = 4003;
inline constexpr uint32_t MSG_ID_DBMGR_HEARTBEAT_RESP    = 4004;

// 数据操作 (4100-4199)
inline constexpr uint32_t MSG_ID_PLAYER_DATA_REQ         = 4101;
inline constexpr uint32_t MSG_ID_PLAYER_DATA_RESP        = 4102;

// 账号数据操作 (4200-4299)
inline constexpr uint32_t MSG_ID_ACCOUNT_DATA_REQ        = 4201;
inline constexpr uint32_t MSG_ID_ACCOUNT_DATA_RESP       = 4202;
inline constexpr uint32_t MSG_ID_ACCOUNT_SET_REQ         = 4203;
inline constexpr uint32_t MSG_ID_ACCOUNT_SET_RESP        = 4204;

// 玩家 ID 分配 (4205-4206)
inline constexpr uint32_t MSG_ID_ALLOC_PLAYER_ID_REQ     = 4205;
inline constexpr uint32_t MSG_ID_ALLOC_PLAYER_ID_RESP    = 4206;

// Game <-> FriendService (6000-6199)

// Friend Service 消息 ID 范围: 6000-6199
inline constexpr uint32_t MSG_ID_FRIEND_SERVICE_IDENTIFY      = 6001;
inline constexpr uint32_t MSG_ID_FRIEND_SERVICE_IDENTIFY_RESP = 6002;
inline constexpr uint32_t MSG_ID_FRIEND_SERVICE_HEARTBEAT      = 6003;
inline constexpr uint32_t MSG_ID_FRIEND_SERVICE_HEARTBEAT_RESP = 6004;
inline constexpr uint32_t MSG_ID_FRIEND_CLIENT_MSG             = 6011;
inline constexpr uint32_t MSG_ID_FRIEND_SERVICE_MSG            = 6012;

// ===========================================
// 超时常量（带前缀避免冲突）
// ===========================================

// Gate <-> Game
inline constexpr int GATE_IDENTIFY_TIMEOUT = 10;   // 身份识别超时（秒）
inline constexpr int GATE_HEARTBEAT_TIMEOUT = 15;  // 心跳超时（秒）

// Game <-> DBMgr
inline constexpr int DBMGR_IDENTIFY_TIMEOUT = 10;  // 身份识别超时（秒）
inline constexpr int DBMGR_HEARTBEAT_INTERVAL = 5; // 心跳发送间隔（秒）
inline constexpr int DBMGR_HEARTBEAT_TIMEOUT = 15; // 心跳超时（秒）

}  // namespace farm
