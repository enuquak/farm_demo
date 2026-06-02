#include "recommend_manager.h"
#include "friend_constants.h"
#include "friend_manager.h"
#include "redis_connection.h"
#include "game_session.h"
#include "message_ids.h"
#include "log_macros.h"

#include "friend.pb.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

namespace farm {

RecommendManager::RecommendManager(RedisConnection* redis, GameSession* session,
                                   FriendManager* friend_mgr)
    : redis_(redis)
    , session_(session)
    , friend_mgr_(friend_mgr)
{
}

// ===========================================
// handle_recommend
// ===========================================

void RecommendManager::handle_recommend(uint64_t player_id, const uint8_t* data, size_t len) {
    // FriendRecommendReq has no fields, parse is optional
    FriendRecommendReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[RecommendManager]Failed to parse FriendRecommendReq from player={}", player_id);
        return;
    }

    FriendRecommendResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Get player's own friend set
    auto my_friends = redis_->smembers(redis_key::friend_set(player_id));

    // Get player's blocked set
    auto my_blocked = redis_->smembers(redis_key::friend_blocked(player_id));

    // Convert my_friends to a set for fast lookup
    std::unordered_set<std::string> my_friend_set(my_friends.begin(), my_friends.end());
    std::unordered_set<std::string> blocked_set(my_blocked.begin(), my_blocked.end());

    // Count how many times each candidate appears as a friend-of-friend
    std::unordered_map<std::string, int> candidate_counts;

    for (const auto& fid_str : my_friends) {
        // Get each friend's friend set
        auto fof = redis_->smembers(redis_key::friend_set(fid_str));

        for (const auto& candidate_str : fof) {
            // Skip self
            if (candidate_str == std::to_string(player_id)) continue;

            // Skip existing friends
            if (my_friend_set.count(candidate_str)) continue;

            // Skip blocked players
            if (blocked_set.count(candidate_str)) continue;

            candidate_counts[candidate_str]++;
        }
    }

    // Sort candidates by common friend count (descending)
    struct Candidate {
        std::string id_str;
        int count;
    };
    std::vector<Candidate> sorted_candidates;
    sorted_candidates.reserve(candidate_counts.size());
    for (const auto& [id_str, count] : candidate_counts) {
        sorted_candidates.push_back({id_str, count});
    }

    std::sort(sorted_candidates.begin(), sorted_candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  return a.count > b.count;
              });

    // Take top N
    size_t limit = std::min(sorted_candidates.size(), MAX_RECOMMEND_COUNT);
    for (size_t i = 0; i < limit; ++i) {
        uint64_t candidate_id = 0;
        try {
            candidate_id = std::stoull(sorted_candidates[i].id_str);
        } catch (...) {
            continue;
        }

        // Verify candidate exists (has player_info)
        std::string info_raw = redis_->get(redis_key::player_info(candidate_id));
        if (info_raw.empty()) continue;

        auto* rec = resp.add_recommendations();
        rec->set_player_id(candidate_id);

        // Parse player_info "role_name|level|scene_id"
        size_t pipe1 = info_raw.find('|');
        size_t pipe2 = (pipe1 != std::string::npos) ? info_raw.find('|', pipe1 + 1) : std::string::npos;

        if (pipe1 != std::string::npos) {
            rec->set_role_name(info_raw.substr(0, pipe1));
        }
        if (pipe1 != std::string::npos && pipe2 != std::string::npos) {
            try {
                rec->set_level(std::stoi(info_raw.substr(pipe1 + 1, pipe2 - pipe1 - 1)));
            } catch (...) {
                rec->set_level(1);
            }
            rec->set_scene_id(info_raw.substr(pipe2 + 1));
        }

        // Check online status
        std::string online = redis_->get(redis_key::player_online(candidate_id));
        rec->set_online(online == "1");
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_RECOMMEND_RESP, resp_data);

    SPDLOG_INFO("[RecommendManager]Player={} got {} recommendations",
                player_id, resp.recommendations_size());
}

}  // namespace farm
