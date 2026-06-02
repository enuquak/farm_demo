#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <hiredis/hiredis.h>

namespace farm {

class RedisConnection {
public:
    RedisConnection();
    ~RedisConnection();

    // Non-copyable, non-movable (owns a raw hiredis context)
    RedisConnection(const RedisConnection&) = delete;
    RedisConnection& operator=(const RedisConnection&) = delete;
    RedisConnection(RedisConnection&&) = delete;
    RedisConnection& operator=(RedisConnection&&) = delete;

    // Connection management
    bool connect(const std::string& uri);
    void disconnect();
    bool is_connected() const;

    // KV operations (synchronous, safe for single-threaded event loop)
    bool set(const std::string& key, const std::string& value);
    std::string get(const std::string& key);  // empty string = not found or error
    bool del(const std::string& key);

    // Set operations
    bool sadd(const std::string& key, const std::string& member);
    bool srem(const std::string& key, const std::string& member);
    bool sismember(const std::string& key, const std::string& member);
    size_t scard(const std::string& key);
    std::unordered_set<std::string> smembers(const std::string& key);

    // Hash operations
    bool hset(const std::string& key, const std::string& field, const std::string& value);
    std::string hget(const std::string& key, const std::string& field);
    bool hdel(const std::string& key, const std::string& field);
    std::unordered_map<std::string, std::string> hgetall(const std::string& key);

    // Key scanning
    std::vector<std::string> keys(const std::string& pattern);

private:
    // Parse redis://host:port from URI
    bool parse_uri(const std::string& uri, std::string& host, int& port);

    redisContext* context_ = nullptr;
    bool connected_ = false;
};

}  // namespace farm
