#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace farm {

// 消息头: 4字节长度 + 4字节MsgID
// 长度字段包含 MsgID + Payload 的总长度
static constexpr size_t MSG_HEADER_SIZE = 8;  // 4 (length) + 4 (msg_id)
static constexpr size_t MSG_LENGTH_SIZE = 4;
static constexpr size_t MSG_ID_SIZE = 4;

// 每条解析出来的消息
struct ParsedMessage {
    uint32_t msg_id;
    std::vector<uint8_t> payload;
};

class MessageParser {
public:
    // 从缓冲区中尝试解析消息
    // 成功解析一条消息返回 true，msg 填充数据，consumed 为消耗的字节数
    // 缓冲区数据不足返回 false
    static bool try_parse(const uint8_t* data, size_t len, ParsedMessage& msg, size_t& consumed);

    // 打包消息为网络格式: [4字节长度][4字节MsgID][Payload]
    static std::vector<uint8_t> pack(uint32_t msg_id, const uint8_t* payload, size_t payload_len);
    static std::vector<uint8_t> pack(uint32_t msg_id, const std::string& payload);
    static std::vector<uint8_t> pack(uint32_t msg_id, const std::vector<uint8_t>& payload);
};

}  // namespace farm
