// 自动生成，请勿手动修改
// 生成时间：2026-06-02 23:07:12
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

}  // namespace farm
