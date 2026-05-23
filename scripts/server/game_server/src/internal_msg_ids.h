#pragma once

#include <cstdint>

namespace farm {

// ===========================================
// 内部消息 ID 常量 (Gate <-> Game)
// MsgID 范围: 3000-3299
// ===========================================

// 连接管理 (3000-3099)
static constexpr uint32_t MSG_ID_INTERN_HEARTBEAT      = 3001;
static constexpr uint32_t MSG_ID_INTERN_HEARTBEAT_RESP = 3002;
static constexpr uint32_t MSG_ID_GATE_IDENTIFY         = 3003;
static constexpr uint32_t MSG_ID_GATE_IDENTIFY_RESP    = 3004;

// 玩家生命周期 (3100-3199)
static constexpr uint32_t MSG_ID_PLAYER_JOIN           = 3101;
static constexpr uint32_t MSG_ID_PLAYER_JOIN_RESP      = 3102;
static constexpr uint32_t MSG_ID_PLAYER_LEAVE          = 3103;

// 消息转发 (3200-3299)
static constexpr uint32_t MSG_ID_CLIENT_MSG            = 3201;
static constexpr uint32_t MSG_ID_GAME_MSG              = 3202;

// 身份识别超时时间（秒）
static constexpr int GATE_IDENTIFY_TIMEOUT = 10;

// 心跳超时时间（秒）
static constexpr int HEARTBEAT_TIMEOUT = 15;

}  // namespace farm
