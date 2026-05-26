#include "message_handler.h"
#include "log_macros.h"

namespace farm {

void MessageHandler::register_handler(uint32_t msg_id, MessageCallback callback) {
    handlers_[msg_id] = std::move(callback);
}

bool MessageHandler::dispatch(uint32_t msg_id, uint64_t player_id,
                              const uint8_t* payload, size_t payload_len) {
    auto it = handlers_.find(msg_id);
    if (it != handlers_.end()) {
        it->second(player_id, payload, payload_len);
        return true;
    }
    SPDLOG_INFO("[MessageHandler]Unhandled msg_id={} from player_id={}", msg_id, player_id);
    return false;
}

bool MessageHandler::has_handler(uint32_t msg_id) const {
    return handlers_.find(msg_id) != handlers_.end();
}

}  // namespace farm
