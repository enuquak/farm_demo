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

// Game <-> CrossServer (7000-7199)

// 连接管理 (7000-7099)
inline constexpr uint32_t MSG_ID_CROSS_IDENTIFY          = 7001;
inline constexpr uint32_t MSG_ID_CROSS_IDENTIFY_RESP     = 7002;
inline constexpr uint32_t MSG_ID_CROSS_HEARTBEAT         = 7003;
inline constexpr uint32_t MSG_ID_CROSS_HEARTBEAT_RESP    = 7004;

// 跨服查询 (7100-7199)
inline constexpr uint32_t MSG_ID_CROSS_QUERY_REQ         = 7101;
inline constexpr uint32_t MSG_ID_CROSS_QUERY_RESP        = 7102;
inline constexpr uint32_t MSG_ID_CROSS_FORWARD_REQ       = 7103;
inline constexpr uint32_t MSG_ID_CROSS_FORWARD_RESP      = 7104;

// Game <-> TeamServer (9000-9199)

// 连接管理 (9000-9099)
inline constexpr uint32_t MSG_ID_TEAM_SERVICE_IDENTIFY       = 9001;
inline constexpr uint32_t MSG_ID_TEAM_SERVICE_IDENTIFY_RESP  = 9002;
inline constexpr uint32_t MSG_ID_TEAM_SERVICE_HEARTBEAT       = 9003;
inline constexpr uint32_t MSG_ID_TEAM_SERVICE_HEARTBEAT_RESP  = 9004;

// 消息转发 (9100-9199)
inline constexpr uint32_t MSG_ID_TEAM_CLIENT_MSG              = 9101;
inline constexpr uint32_t MSG_ID_TEAM_SERVICE_MSG             = 9102;

// 队伍查询 (9110-9119)
inline constexpr uint32_t MSG_ID_TEAM_QUERY_MEMBERS_REQ       = 9111;
inline constexpr uint32_t MSG_ID_TEAM_QUERY_MEMBERS_RESP      = 9112;

// 队伍变更通知 (9120-9129)
inline constexpr uint32_t MSG_ID_TEAM_MEMBERS_CHANGED_NOTIFY  = 9121;

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

// Game <-> CrossServer
inline constexpr int CROSS_IDENTIFY_TIMEOUT = 10;   // 身份识别超时（秒）
inline constexpr int CROSS_HEARTBEAT_INTERVAL = 5;  // 心跳发送间隔（秒）
inline constexpr int CROSS_HEARTBEAT_TIMEOUT = 15;  // 心跳超时（秒）
inline constexpr int CROSS_QUERY_TIMEOUT = 5;       // 查询超时（秒）

// Game <-> TeamServer
inline constexpr int TEAM_IDENTIFY_TIMEOUT = 10;   // 身份识别超时（秒）
inline constexpr int TEAM_HEARTBEAT_INTERVAL = 5;  // 心跳发送间隔（秒）
inline constexpr int TEAM_HEARTBEAT_TIMEOUT = 15;  // 心跳超时（秒）

}  // namespace farm
