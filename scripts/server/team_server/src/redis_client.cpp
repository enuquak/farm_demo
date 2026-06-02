#include "redis_client.h"
#include "log_macros.h"

#include <hiredis/hiredis.h>
#include <cstring>
#include <sstream>

namespace farm {

RedisClient::RedisClient() : ctx_(nullptr) {}

RedisClient::~RedisClient() { disconnect(); }

bool RedisClient::connect(const std::string& host, int port) {
    ctx_ = redisConnect(host.c_str(), port);
    if (!ctx_ || ctx_->err) {
        if (ctx_) {
            SPDLOG_ERROR("[Redis]Connection failed: {}", ctx_->errstr);
            redisFree(ctx_);
            ctx_ = nullptr;
        } else {
            SPDLOG_ERROR("[Redis]Connection failed: can't allocate context");
        }
        return false;
    }
    SPDLOG_INFO("[Redis]Connected to {}:{}", host, port);
    return true;
}

void RedisClient::disconnect() {
    if (ctx_) {
        redisFree(ctx_);
        ctx_ = nullptr;
    }
}

bool RedisClient::set_team_info(uint64_t team_id, uint64_t leader_id,
                                uint32_t status, int cave_level) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "HSET team:%llu:info leader_id %llu status %u cave_level %d",
        team_id, leader_id, status, cave_level);
    if (!reply) return false;
    bool ok = reply->type != REDIS_REPLY_ERROR;
    freeReplyObject(reply);
    return ok;
}

bool RedisClient::get_team_info(uint64_t team_id, uint64_t& leader_id,
                                uint32_t& status, int& cave_level) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "HGETALL team:%llu:info", team_id);
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        if (reply) freeReplyObject(reply);
        return false;
    }
    for (size_t i = 0; i < reply->elements; i += 2) {
        std::string key(reply->element[i]->str);
        std::string val(reply->element[i+1]->str);
        if (key == "leader_id") leader_id = std::stoull(val);
        else if (key == "status") status = std::stoul(val);
        else if (key == "cave_level") cave_level = std::stoi(val);
    }
    freeReplyObject(reply);
    return true;
}

bool RedisClient::del_team_info(uint64_t team_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "DEL team:%llu:info", team_id);
    if (!reply) return false;
    freeReplyObject(reply);
    return true;
}

bool RedisClient::add_team_member(uint64_t team_id, uint64_t player_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "RPUSH team:%llu:members %llu", team_id, player_id);
    if (!reply) return false;
    bool ok = reply->type != REDIS_REPLY_ERROR;
    freeReplyObject(reply);
    return ok;
}

bool RedisClient::remove_team_member(uint64_t team_id, uint64_t player_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "LREM team:%llu:members 0 %llu", team_id, player_id);
    if (!reply) return false;
    freeReplyObject(reply);
    return true;
}

bool RedisClient::get_team_members(uint64_t team_id, std::vector<uint64_t>& members) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "LRANGE team:%llu:members 0 -1", team_id);
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        if (reply) freeReplyObject(reply);
        return false;
    }
    members.clear();
    for (size_t i = 0; i < reply->elements; i++) {
        members.push_back(std::stoull(reply->element[i]->str));
    }
    freeReplyObject(reply);
    return true;
}

bool RedisClient::is_team_member(uint64_t team_id, uint64_t player_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "LPOS team:%llu:members %llu", team_id, player_id);
    if (!reply) return false;
    bool found = (reply->type == REDIS_REPLY_INTEGER);
    freeReplyObject(reply);
    return found;
}

size_t RedisClient::get_team_member_count(uint64_t team_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "LLEN team:%llu:members", team_id);
    if (!reply) return 0;
    size_t count = reply->integer;
    freeReplyObject(reply);
    return count;
}

bool RedisClient::set_player_team(uint64_t player_id, uint64_t team_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "SET team:player:%llu %llu", player_id, team_id);
    if (!reply) return false;
    bool ok = reply->type != REDIS_REPLY_ERROR;
    freeReplyObject(reply);
    return ok;
}

bool RedisClient::get_player_team(uint64_t player_id, uint64_t& team_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "GET team:player:%llu", player_id);
    if (!reply || reply->type == REDIS_REPLY_NIL) {
        if (reply) freeReplyObject(reply);
        team_id = 0;
        return false;
    }
    team_id = std::stoull(reply->str);
    freeReplyObject(reply);
    return true;
}

bool RedisClient::del_player_team(uint64_t player_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "DEL team:player:%llu", player_id);
    if (!reply) return false;
    freeReplyObject(reply);
    return true;
}

bool RedisClient::set_invite(uint64_t player_id, uint64_t team_id,
                             uint64_t inviter_id, uint64_t timestamp) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "HSET team:invite:%llu team_id %llu inviter_id %llu timestamp %llu",
        player_id, team_id, inviter_id, timestamp);
    if (!reply) return false;
    bool ok = reply->type != REDIS_REPLY_ERROR;
    freeReplyObject(reply);
    return ok;
}

bool RedisClient::get_invite(uint64_t player_id, uint64_t& team_id,
                             uint64_t& inviter_id, uint64_t& timestamp) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "HGETALL team:invite:%llu", player_id);
    if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements == 0) {
        if (reply) freeReplyObject(reply);
        return false;
    }
    for (size_t i = 0; i < reply->elements; i += 2) {
        std::string key(reply->element[i]->str);
        std::string val(reply->element[i+1]->str);
        if (key == "team_id") team_id = std::stoull(val);
        else if (key == "inviter_id") inviter_id = std::stoull(val);
        else if (key == "timestamp") timestamp = std::stoull(val);
    }
    freeReplyObject(reply);
    return true;
}

bool RedisClient::del_invite(uint64_t player_id) {
    redisReply* reply = (redisReply*)redisCommand(ctx_,
        "DEL team:invite:%llu", player_id);
    if (!reply) return false;
    freeReplyObject(reply);
    return true;
}

uint64_t RedisClient::generate_team_id() {
    redisReply* reply = (redisReply*)redisCommand(ctx_, "INCR team:id_counter");
    if (!reply) return 0;
    uint64_t id = reply->integer;
    freeReplyObject(reply);
    return id;
}

}  // namespace farm
