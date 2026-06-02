#pragma once

#include <cstdint>

namespace farm {

// 玩家最大能量值
inline constexpr int32_t MAX_ENERGY = 100;

// 玩家自动存盘间隔（秒）
inline constexpr int32_t PLAYER_SAVE_INTERVAL = 300;  // 5 minutes

// 移动校验
inline constexpr float MAX_PLAYER_SPEED = 128.0f;          // 像素/秒（客户端速度64的2倍容差）
inline constexpr int64_t POSITION_UPDATE_TIMEOUT_MS = 3000; // 超过3秒未更新视为过期

// 新玩家出生点（像素坐标，对应 farm_main 场景 tile 30,25）
inline constexpr float DEFAULT_SPAWN_POS_X = 480.0f;  // 30 * TILE_SIZE(16)
inline constexpr float DEFAULT_SPAWN_POS_Y = 400.0f;  // 25 * TILE_SIZE(16)

}  // namespace farm
