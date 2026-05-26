#pragma once

#include <cstdint>
#include <string>

namespace farm {

// ===========================================
// 管理消息 ID 常量
// MsgID 范围: 5000-5999
// ===========================================

// 服务管理 (5000-5099)
static constexpr uint32_t MSG_ID_SHUTDOWN           = 5001;
static constexpr uint32_t MSG_ID_SHUTDOWN_RESP      = 5002;

// 停服请求消息结构
struct AdminShutdownMsg {
    std::string reason;       // 停服原因
    uint32_t timeout_ms;      // 建议超时时间（毫秒）

    // 序列化为 JSON 字符串
    std::string serialize() const;

    // 从 JSON 字符串反序列化
    static bool deserialize(const std::string& json_str, AdminShutdownMsg& msg);
};

// 停服响应消息结构
struct AdminShutdownResp {
    int32_t code;             // 响应码，0 表示准备就绪
    std::string msg;          // 响应消息

    // 序列化为 JSON 字符串
    std::string serialize() const;

    // 从 JSON 字符串反序列化
    static bool deserialize(const std::string& json_str, AdminShutdownResp& resp);
};

}  // namespace farm
