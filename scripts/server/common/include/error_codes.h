// 自动生成，请勿手动修改
// 生成时间：2026-06-03 00:09:20
// 源文件：shared/error_codes.json
#pragma once

#include <cstdint>

namespace farm {

inline constexpr uint32_t SUCCESS                                 = 0;  // 操作成功
inline constexpr uint32_t INVALID_PASSWORD                        = 1001;  // 密码错误
inline constexpr uint32_t USER_NOT_FOUND                          = 1002;  // 用户不存在
inline constexpr uint32_t ALREADY_LOGGED_IN                       = 1003;  // 用户已登录
inline constexpr uint32_t INVALID_ITEM                            = 2001;  // 无效物品
inline constexpr uint32_t INVENTORY_FULL                          = 2002;  // 背包已满
inline constexpr uint32_t TEAM_ALREADY_IN_TEAM                    = 7001;  // 已在队伍中
inline constexpr uint32_t TEAM_FULL                               = 7002;  // 队伍已满（4人）
inline constexpr uint32_t TEAM_NOT_IN_TEAM                        = 7003;  // 不在任何队伍中
inline constexpr uint32_t TEAM_NOT_LEADER                         = 7004;  // 不是队长，无权限
inline constexpr uint32_t TEAM_TARGET_NOT_FOUND                   = 7005;  // 目标玩家不存在
inline constexpr uint32_t TEAM_TARGET_ALREADY_IN_TEAM             = 7006;  // 目标已有队伍
inline constexpr uint32_t TEAM_TARGET_OFFLINE                     = 7007;  // 目标不在线
inline constexpr uint32_t TEAM_INVITE_NOT_FOUND                   = 7008;  // 邀请不存在或已过期
inline constexpr uint32_t TEAM_INVITE_EXPIRED                     = 7009;  // 邀请已过期
inline constexpr uint32_t TEAM_CANNOT_KICK_SELF                   = 7010;  // 不能踢自己
inline constexpr uint32_t TEAM_IN_CAVE                            = 7011;  // 队伍在矿洞中，不能解散
inline constexpr uint32_t TEAM_SELF_OPERATION                     = 7012;  // 不能邀请自己
inline constexpr uint32_t TEAM_PLAYER_BLOCKED                     = 7013;  // 被对方拉黑

}  // namespace farm
