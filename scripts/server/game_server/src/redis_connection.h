#pragma once

#include <string>
#include <hiredis/hiredis.h>

namespace farm {

class RedisConnection {
public:
    RedisConnection();
    ~RedisConnection();

    // Non-copyable
    RedisConnection(const RedisConnection&) = delete;
    RedisConnection& operator=(const RedisConnection&) = delete;

    // Connection management
    bool connect(const std::string& uri);
    void disconnect();
    bool is_connected() const;

    // KV operations (synchronous, safe for single-threaded event loop)
    bool set(const std::string& key, const std::string& value);
    std::string get(const std::string& key);  // empty string = not found or error
    bool del(const std::string& key);

private:
    // Parse redis://host:port from URI
    bool parse_uri(const std::string& uri, std::string& host, int& port);

    redisContext* context_ = nullptr;
    bool connected_ = false;
};

}  // namespace farm
