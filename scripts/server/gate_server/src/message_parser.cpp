#include "message_parser.h"
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace farm {

bool MessageParser::try_parse(const uint8_t* data, size_t len, ParsedMessage& msg, size_t& consumed) {
    // 至少需要 4 字节长度头
    if (len < MSG_LENGTH_SIZE) {
        return false;
    }

    // 读取长度（网络字节序）
    uint32_t body_len = 0;
    std::memcpy(&body_len, data, MSG_LENGTH_SIZE);
    body_len = ntohl(body_len);

    // 检查长度合理性
    if (body_len > 1024 * 1024) {  // 最大 1MB
        consumed = len;  // 消费掉所有数据，避免卡死
        return false;
    }

    // 检查是否有完整消息
    if (len < MSG_LENGTH_SIZE + body_len) {
        return false;
    }

    // 解析 MsgID
    uint32_t msg_id = 0;
    std::memcpy(&msg_id, data + MSG_LENGTH_SIZE, MSG_ID_SIZE);
    msg.msg_id = ntohl(msg_id);

    // 提取 Payload
    size_t payload_len = body_len - MSG_ID_SIZE;
    if (payload_len > 0) {
        msg.payload.assign(data + MSG_HEADER_SIZE, data + MSG_HEADER_SIZE + payload_len);
    } else {
        msg.payload.clear();
    }

    consumed = MSG_LENGTH_SIZE + body_len;
    return true;
}

std::vector<uint8_t> MessageParser::pack(uint32_t msg_id, const uint8_t* payload, size_t payload_len) {
    // 总长度 = 4(length) + 4(msg_id) + payload_len
    size_t total = MSG_LENGTH_SIZE + MSG_ID_SIZE + payload_len;
    std::vector<uint8_t> buf(total);

    // 写入 body 长度（msg_id + payload）
    uint32_t body_len = htonl(static_cast<uint32_t>(MSG_ID_SIZE + payload_len));
    std::memcpy(buf.data(), &body_len, MSG_LENGTH_SIZE);

    // 写入 MsgID
    uint32_t net_msg_id = htonl(msg_id);
    std::memcpy(buf.data() + MSG_LENGTH_SIZE, &net_msg_id, MSG_ID_SIZE);

    // 写入 Payload
    if (payload_len > 0) {
        std::memcpy(buf.data() + MSG_HEADER_SIZE, payload, payload_len);
    }

    return buf;
}

std::vector<uint8_t> MessageParser::pack(uint32_t msg_id, const std::string& payload) {
    return pack(msg_id, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
}

std::vector<uint8_t> MessageParser::pack(uint32_t msg_id, const std::vector<uint8_t>& payload) {
    return pack(msg_id, payload.data(), payload.size());
}

}  // namespace farm
