#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace farm {

// 消息处理回调函数类型
// 参数: player_id, payload_data, payload_len
using MessageCallback = std::function<void(uint64_t, const uint8_t*, size_t)>;

class MessageHandler {
public:
    MessageHandler() = default;
    ~MessageHandler() = default;

    // 注册消息处理函数
    void register_handler(uint32_t msg_id, MessageCallback callback);

    // 分发消息到对应的 handler
    // 返回 true 表示找到了 handler 并调用，false 表示未注册
    bool dispatch(uint32_t msg_id, uint64_t player_id,
                  const uint8_t* payload, size_t payload_len);

    // 检查是否有注册的 handler
    bool has_handler(uint32_t msg_id) const;

private:
    std::unordered_map<uint32_t, MessageCallback> handlers_;
};

}  // namespace farm
