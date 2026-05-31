#pragma once

#include <functional>
#include <memory>
#include <string>
#include <cstdint>

namespace farm {

class GateSession;

// Callback for sending a message to a gate session
using SendToGateFunc = std::function<void(std::shared_ptr<GateSession>, uint32_t, const std::string&)>;

// Callback for sending a game message to a player
using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;

}  // namespace farm
