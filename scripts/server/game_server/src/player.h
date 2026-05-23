#pragma once

#include <cstdint>
#include <ctime>

namespace farm {

class GateSession;

class Player {
public:
    Player(uint64_t player_id, GateSession* gate_session);
    ~Player();

    uint64_t player_id() const { return player_id_; }
    GateSession* gate_session() const { return gate_session_; }
    time_t join_time() const { return join_time_; }

    void set_gate_session(GateSession* session) { gate_session_ = session; }

private:
    uint64_t player_id_;
    GateSession* gate_session_;
    time_t join_time_;
};

}  // namespace farm
