#pragma once

#include <cstdint>
#include <string>
#include <algorithm>

namespace farm {

enum class FriendErrorCode : int32_t {
    SUCCESS = 0,
    ALREADY_FRIENDS = 1,
    FRIEND_LIST_FULL = 2,
    PLAYER_NOT_FOUND = 3,
    REQUEST_NOT_FOUND = 4,
    NOT_FRIENDS = 5,
    CHAT_MSG_TOO_LONG = 6,
    GIFT_ITEM_NOT_FOUND = 7,
    GIFT_COUNT_INVALID = 8,
    VISIT_NOT_AUTHORIZED = 9,
    VISIT_ACTION_FAILED = 10,
    SELF_OPERATION = 11,
    BLOCKED = 12,
    COOLDOWN = 13,
};

inline constexpr size_t MAX_FRIENDS = 50;
inline constexpr size_t MAX_OFFLINE_MESSAGES = 100;
inline constexpr size_t MAX_CHAT_HISTORY_DAYS = 7;
inline constexpr size_t MAX_CHAT_MSG_LENGTH = 200;
inline constexpr size_t MAX_RECOMMEND_COUNT = 10;
inline constexpr size_t MAX_SEARCH_RESULTS = 20;
inline constexpr size_t FRIEND_REQUEST_EXPIRY_SECONDS = 7 * 24 * 3600;

enum class VisitAction : int32_t {
    WATER = 0,
    WEED = 1,
    HARVEST = 2,
    STEAL = 3,
};

enum class ChatMsgType : int32_t {
    TEXT = 0,
    EMOTE = 1,
    GIFT = 2,
};

namespace redis_key {
    inline std::string friend_set(uint64_t pid) { return "friend:" + std::to_string(pid) + ":set"; }
    inline std::string friend_requests(uint64_t pid) { return "friend:" + std::to_string(pid) + ":requests"; }
    inline std::string friend_blocked(uint64_t pid) { return "friend:" + std::to_string(pid) + ":blocked"; }
    inline std::string player_online(uint64_t pid) { return "player:" + std::to_string(pid) + ":online"; }
    inline std::string player_info(uint64_t pid) { return "player:" + std::to_string(pid) + ":info"; }
    inline std::string chat_offline(uint64_t pid) { return "chat:offline:" + std::to_string(pid); }
    inline std::string farm_visit_auth(uint64_t pid) { return "farm:visit:auth:" + std::to_string(pid); }
    inline std::string farm_visit_active(uint64_t pid) { return "farm:visit:active:" + std::to_string(pid); }
    inline std::string dirty(uint64_t pid) { return "dirty:friend:" + std::to_string(pid); }
}  // namespace redis_key

namespace dbmgr_key {
    inline std::string friend_data(uint64_t pid) { return "friend_data:" + std::to_string(pid); }
    inline std::string chat_history(uint64_t pid1, uint64_t pid2) {
        if (pid1 > pid2) std::swap(pid1, pid2);
        return "chat_history:" + std::to_string(pid1) + ":" + std::to_string(pid2);
    }
}  // namespace dbmgr_key

}  // namespace farm
