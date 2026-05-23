#include "player.h"

namespace farm {

Player::Player(uint64_t player_id, GateSession* gate_session)
    : player_id_(player_id)
    , gate_session_(gate_session)
    , join_time_(std::time(nullptr))
{
}

Player::~Player() {
}

}  // namespace farm
