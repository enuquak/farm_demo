#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace farm {

// ===========================================
// 数据操作结果枚举
// ===========================================

enum class DataResult : int32_t {
    SUCCESS = 0,
    KEY_NOT_FOUND = 1,
    IO_ERROR = 2,
    PARSE_ERROR = 3,
};

// ===========================================
// 账号操作结果枚举
// ===========================================

enum class AccountResult : int32_t {
    SUCCESS = 0,
    IO_ERROR = 1,
    ROLE_ALREADY_EXISTS = 2,
};

// ===========================================
// 账号角色信息
// ===========================================

struct AccountRole {
    int32_t server_id = 0;
    int64_t player_id = 0;
    std::string role_name;
};

}  // namespace farm
