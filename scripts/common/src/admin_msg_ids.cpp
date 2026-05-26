#include "admin_msg_ids.h"
#include <nlohmann/json.hpp>

namespace farm {

// AdminShutdownMsg 序列化
std::string AdminShutdownMsg::serialize() const {
    nlohmann::json j;
    j["reason"] = reason;
    j["timeout_ms"] = timeout_ms;
    return j.dump();
}

// AdminShutdownMsg 反序列化
bool AdminShutdownMsg::deserialize(const std::string& json_str, AdminShutdownMsg& msg) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_str);
        if (!j.contains("reason") || !j["reason"].is_string()) {
            return false;
        }
        if (!j.contains("timeout_ms") || !j["timeout_ms"].is_number_unsigned()) {
            return false;
        }
        msg.reason = j["reason"].get<std::string>();
        msg.timeout_ms = j["timeout_ms"].get<uint32_t>();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// AdminShutdownResp 序列化
std::string AdminShutdownResp::serialize() const {
    nlohmann::json j;
    j["code"] = code;
    j["msg"] = msg;
    return j.dump();
}

// AdminShutdownResp 反序列化
bool AdminShutdownResp::deserialize(const std::string& json_str, AdminShutdownResp& resp) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_str);
        if (!j.contains("code") || !j["code"].is_number_integer()) {
            return false;
        }
        if (!j.contains("msg") || !j["msg"].is_string()) {
            return false;
        }
        resp.code = j["code"].get<int32_t>();
        resp.msg = j["msg"].get<std::string>();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace farm
