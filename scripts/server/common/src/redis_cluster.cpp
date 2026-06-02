#include "redis_cluster.h"
#include "log_macros.h"

// Windows: timeval 在 winsock2.h 中定义，hiredis 需要它
#ifdef _WIN32
#include <winsock2.h>
#endif

#include <hiredis/hiredis.h>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <sstream>

namespace farm {

// ============================================================================
// CRC16 查找表（Redis Cluster 标准 CRC16-CCITT）
// ============================================================================
static const uint16_t CRC16_TABLE[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x4864, 0x5845, 0x6826, 0x7807, 0x08e0, 0x18c1, 0x28a2, 0x38a3,
    0xc94c, 0xd96d, 0xe90e, 0xf92f, 0x89c8, 0x99e9, 0xa98a, 0xb9ab,
    0x5a75, 0x4a54, 0x7a37, 0x6a16, 0x1af1, 0x0ad0, 0x3ab3, 0x2a92,
    0xdb7d, 0xcb5c, 0xfb3f, 0xeb1e, 0x9bf9, 0x8bd8, 0xbbbb, 0xab9a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x85a9, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4425,
    0xa7db, 0xb7fa, 0x8799, 0x9798, 0xe77f, 0xf75e, 0xc73d, 0xd71c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa98b,
    0x5864, 0x4845, 0x7826, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x2883,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xab9b, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

// ============================================================================
// CRC16 实现
// ============================================================================

uint16_t RedisClusterClient::crc16(const char* data, size_t len)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc = (crc << 8) ^ CRC16_TABLE[((crc >> 8) ^ static_cast<uint8_t>(data[i])) & 0xff];
    }
    return crc;
}

// ============================================================================
// key_slot 实现（支持 hash tag）
// ============================================================================

uint16_t RedisClusterClient::key_slot(const std::string& key)
{
    // 处理 hash tag: "foo{bar}baz" 应该只 hash "bar"
    size_t start = key.find('{');
    if (start != std::string::npos) {
        size_t end = key.find('}', start + 1);
        if (end != std::string::npos && end > start + 1) {
            // 找到有效的 hash tag
            std::string tag = key.substr(start + 1, end - start - 1);
            return crc16(tag.c_str(), tag.length()) % 16384;
        }
    }
    // 没有 hash tag，使用完整 key
    return crc16(key.c_str(), key.length()) % 16384;
}

// ============================================================================
// 辅助函数
// ============================================================================

std::string RedisClusterClient::make_node_key(const std::string& host, int port)
{
    return host + ":" + std::to_string(port);
}

// ============================================================================
// 构造/析构
// ============================================================================

RedisClusterClient::RedisClusterClient(event_base* base, const RedisClusterConfig& config)
    : base_(base)
    , config_(config)
{
    // 初始化 slot_map_ 为空
    for (int i = 0; i < 16384; ++i) {
        slot_map_[i].clear();
    }
}

RedisClusterClient::~RedisClusterClient()
{
    shutdown();
}

// ============================================================================
// 解析 seed URI
// ============================================================================

std::vector<ClusterNode> RedisClusterClient::parse_seed_uri(const std::string& uri) const
{
    std::vector<ClusterNode> nodes;
    std::string remaining = uri;

    // 支持格式: redis://host1:port1,redis://host2:port2,...
    // 或: host1:port1,host2:port2,...

    while (!remaining.empty()) {
        std::string token;
        size_t comma_pos = remaining.find(',');

        if (comma_pos != std::string::npos) {
            token = remaining.substr(0, comma_pos);
            remaining = remaining.substr(comma_pos + 1);
        } else {
            token = remaining;
            remaining.clear();
        }

        // 移除协议前缀
        if (token.find("redis://") == 0) {
            token = token.substr(8);
        } else if (token.find("tcp://") == 0) {
            token = token.substr(6);
        }

        // 解析 host:port
        auto colon_pos = token.find(':');
        if (colon_pos != std::string::npos) {
            ClusterNode node;
            node.host = token.substr(0, colon_pos);
            try {
                node.port = std::stoi(token.substr(colon_pos + 1));
            } catch (const std::exception&) {
                SPDLOG_ERROR("[RedisCluster]Failed to parse port from seed: {}", token);
                continue;
            }
            if (!node.host.empty()) {
                nodes.push_back(node);
            }
        } else if (!token.empty()) {
            ClusterNode node;
            node.host = token;
            node.port = 6379;
            nodes.push_back(node);
        }
    }

    return nodes;
}

// ============================================================================
// 连接节点
// ============================================================================

redisContext* RedisClusterClient::connect_node(const std::string& host, int port)
{
    struct timeval timeout;
    timeout.tv_sec = config_.connect_timeout_ms / 1000;
    timeout.tv_usec = (config_.connect_timeout_ms % 1000) * 1000;

    redisContext* ctx = redisConnectWithTimeout(host.c_str(), port, timeout);
    if (!ctx) {
        SPDLOG_ERROR("[RedisCluster]Failed to allocate redis context for {}:{}",
                     host, port);
        return nullptr;
    }

    if (ctx->err) {
        SPDLOG_ERROR("[RedisCluster]Connection failed to {}:{}: {}",
                     host, port, ctx->errstr);
        redisFree(ctx);
        return nullptr;
    }

    // 设置命令超时
    struct timeval cmd_timeout;
    cmd_timeout.tv_sec = config_.command_timeout_ms / 1000;
    cmd_timeout.tv_usec = (config_.command_timeout_ms % 1000) * 1000;
    redisSetTimeout(ctx, cmd_timeout);

    // PING 验证
    redisReply* reply = static_cast<redisReply*>(redisCommand(ctx, "PING"));
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        SPDLOG_ERROR("[RedisCluster]PING failed on {}:{}: {}",
                     host, port, reply ? reply->str : "null reply");
        if (reply) {
            freeReplyObject(reply);
        }
        redisFree(ctx);
        return nullptr;
    }
    freeReplyObject(reply);

    SPDLOG_INFO("[RedisCluster]Connected to {}:{}", host, port);
    return ctx;
}

// ============================================================================
// 关闭连接
// ============================================================================

void RedisClusterClient::close_connection(const std::string& node_key)
{
    auto it = connections_.find(node_key);
    if (it != connections_.end()) {
        if (it->second) {
            redisFree(it->second);
        }
        connections_.erase(it);
    }
}

void RedisClusterClient::close_all_connections()
{
    for (auto& pair : connections_) {
        if (pair.second) {
            redisFree(pair.second);
        }
    }
    connections_.clear();
}

// ============================================================================
// 获取任意可用连接
// ============================================================================

redisContext* RedisClusterClient::get_any_connection()
{
    for (auto& pair : connections_) {
        if (pair.second && !pair.second->err) {
            return pair.second;
        }
    }
    return nullptr;
}

// ============================================================================
// 获取指定 slot 的连接
// ============================================================================

redisContext* RedisClusterClient::get_connection(uint16_t slot)
{
    if (slot >= 16384) {
        return nullptr;
    }

    const std::string& node_key = slot_map_[slot];
    if (node_key.empty()) {
        // slot 未映射，尝试刷新
        SPDLOG_WARN("[RedisCluster]Slot {} not mapped, attempting refresh", slot);
        refresh_slots();
        if (slot_map_[slot].empty()) {
            return nullptr;
        }
        return get_any_connection();
    }

    auto it = connections_.find(node_key);
    if (it != connections_.end() && it->second && !it->second->err) {
        return it->second;
    }

    // 连接不存在或已断开，尝试重连
    SPDLOG_WARN("[RedisCluster]Connection to {} lost, reconnecting", node_key);
    close_connection(node_key);

    // 解析 host:port
    auto colon_pos = node_key.find(':');
    if (colon_pos == std::string::npos) {
        return nullptr;
    }

    std::string host = node_key.substr(0, colon_pos);
    int port = 6379;
    try {
        port = std::stoi(node_key.substr(colon_pos + 1));
    } catch (const std::exception&) {
        return nullptr;
    }

    redisContext* ctx = connect_node(host, port);
    if (ctx) {
        connections_[node_key] = ctx;
        return ctx;
    }

    return nullptr;
}

// ============================================================================
// 解析 CLUSTER SLOTS 响应
// ============================================================================

bool RedisClusterClient::parse_cluster_slots(redisReply* reply)
{
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        SPDLOG_ERROR("[RedisCluster]Invalid CLUSTER SLOTS response type: {}",
                     reply ? reply->type : -1);
        return false;
    }

    // 临时存储新的映射
    std::string new_slot_map[16384];
    std::vector<ClusterNode> new_nodes;

    for (size_t i = 0; i < reply->elements; ++i) {
        redisReply* slot_range = reply->element[i];
        if (!slot_range || slot_range->type != REDIS_REPLY_ARRAY || slot_range->elements < 3) {
            continue;
        }

        // 解析 slot 范围: [start_slot, end_slot, master_node, ...replicas]
        int start_slot = 0, end_slot = 0;
        if (slot_range->element[0]->type == REDIS_REPLY_INTEGER) {
            start_slot = static_cast<int>(slot_range->element[0]->integer);
        } else if (slot_range->element[0]->type == REDIS_REPLY_STRING) {
            start_slot = std::stoi(slot_range->element[0]->str);
        }

        if (slot_range->element[1]->type == REDIS_REPLY_INTEGER) {
            end_slot = static_cast<int>(slot_range->element[1]->integer);
        } else if (slot_range->element[1]->type == REDIS_REPLY_STRING) {
            end_slot = std::stoi(slot_range->element[1]->str);
        }

        // 解析 master 节点: [host, port, node_id]
        if (slot_range->element[2]->type != REDIS_REPLY_ARRAY || slot_range->element[2]->elements < 2) {
            continue;
        }

        redisReply* master_info = slot_range->element[2];
        std::string host;
        int port = 6379;
        std::string node_id;

        if (master_info->element[0]->type == REDIS_REPLY_STRING) {
            host = std::string(master_info->element[0]->str, master_info->element[0]->len);
        }
        if (master_info->element[1]->type == REDIS_REPLY_INTEGER) {
            port = static_cast<int>(master_info->element[1]->integer);
        } else if (master_info->element[1]->type == REDIS_REPLY_STRING) {
            port = std::stoi(master_info->element[1]->str);
        }
        if (master_info->elements >= 3 && master_info->element[2]->type == REDIS_REPLY_STRING) {
            node_id = std::string(master_info->element[2]->str, master_info->element[2]->len);
        }

        if (host.empty()) {
            continue;
        }

        std::string node_key = make_node_key(host, port);

        // 填充 slot 映射
        for (int s = start_slot; s <= end_slot && s < 16384; ++s) {
            new_slot_map[s] = node_key;
        }

        // 记录节点信息
        ClusterNode node;
        node.host = host;
        node.port = port;
        node.node_id = node_id;
        node.is_master = true;
        node.slots.push_back({start_slot, end_slot});
        new_nodes.push_back(node);
    }

    // 更新 slot_map_ 和 nodes_
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (int i = 0; i < 16384; ++i) {
            slot_map_[i] = new_slot_map[i];
        }
        nodes_ = new_nodes;

        // 确保所有新节点都有连接
        for (const auto& node : new_nodes) {
            std::string key = make_node_key(node.host, node.port);
            if (connections_.find(key) == connections_.end()) {
                redisContext* ctx = connect_node(node.host, node.port);
                if (ctx) {
                    connections_[key] = ctx;
                }
            }
        }

        // 关闭不再需要的连接
        std::vector<std::string> to_remove;
        for (const auto& pair : connections_) {
            bool found = false;
            for (const auto& node : new_nodes) {
                if (make_node_key(node.host, node.port) == pair.first) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                to_remove.push_back(pair.first);
            }
        }
        for (const auto& key : to_remove) {
            close_connection(key);
        }
    }

    SPDLOG_INFO("[RedisCluster]Slot map refreshed, {} nodes, {} slots mapped",
                new_nodes.size(),
                std::count_if(new_slot_map, new_slot_map + 16384,
                              [](const std::string& s) { return !s.empty(); }));
    return true;
}

// ============================================================================
// 刷新 slot 映射
// ============================================================================

bool RedisClusterClient::refresh_slots()
{
    std::lock_guard<std::mutex> lock(mutex_);

    redisContext* ctx = get_any_connection();
    if (!ctx) {
        SPDLOG_ERROR("[RedisCluster]No available connection for slot refresh");
        healthy_.store(false, std::memory_order_release);
        return false;
    }

    redisReply* reply = static_cast<redisReply*>(redisCommand(ctx, "CLUSTER SLOTS"));
    if (!reply) {
        SPDLOG_ERROR("[RedisCluster]CLUSTER SLOTS command failed: {}", ctx->errstr);
        healthy_.store(false, std::memory_order_release);
        return false;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        SPDLOG_ERROR("[RedisCluster]CLUSTER SLOTS error: {}", reply->str);
        freeReplyObject(reply);
        healthy_.store(false, std::memory_order_release);
        return false;
    }

    // 注意: parse_cluster_slots 内部也会加锁，这里需要先释放锁再调用
    // 但为了简化，我们在调用前释放锁
    mutex_.unlock();
    bool result = parse_cluster_slots(reply);
    mutex_.lock();

    freeReplyObject(reply);

    if (result) {
        healthy_.store(true, std::memory_order_release);
    }

    return result;
}

// ============================================================================
// 执行命令（带 MOVED/ASK 重定向处理）
// ============================================================================

redisReply* RedisClusterClient::execute_command(uint16_t slot, const char* format, ...)
{
    const int MAX_REDIRECTS = 5;

    for (int attempt = 0; attempt < MAX_REDIRECTS; ++attempt) {
        redisContext* ctx = get_connection(slot);
        if (!ctx) {
            SPDLOG_ERROR("[RedisCluster]No connection for slot {}", slot);
            return nullptr;
        }

        // 构建命令参数
        va_list args;
        va_start(args, format);
        // 使用 redisvformatCommand 构建命令
        char* cmd = nullptr;
        int cmd_len = redisvformatCommand(&cmd, format, args);
        va_end(args);

        if (cmd_len < 0 || !cmd) {
            SPDLOG_ERROR("[RedisCluster]Failed to format command");
            return nullptr;
        }

        // 执行命令
        redisReply* reply = static_cast<redisReply*>(redisCommand(ctx, "%s", cmd));
        free(cmd);

        if (!reply) {
            SPDLOG_ERROR("[RedisCluster]Command failed on slot {}: {}", slot, ctx->errstr);
            // 连接可能已断开，下次 get_connection 会重连
            return nullptr;
        }

        // 检查 MOVED 重定向
        if (reply->type == REDIS_REPLY_ERROR && reply->str) {
            std::string err_str(reply->str, reply->len);

            if (err_str.substr(0, 6) == "MOVED ") {
                // MOVED slot host:port
                SPDLOG_WARN("[RedisCluster]MOVED redirect on slot {}: {}", slot, err_str);

                // 解析重定向目标
                // 格式: MOVED 1234 127.0.0.1:7001
                std::istringstream iss(err_str.substr(6));
                int new_slot;
                std::string new_addr;
                iss >> new_slot >> new_addr;

                // 解析 host:port
                auto colon_pos = new_addr.find(':');
                if (colon_pos != std::string::npos) {
                    std::string new_host = new_addr.substr(0, colon_pos);
                    int new_port = 6379;
                    try {
                        new_port = std::stoi(new_addr.substr(colon_pos + 1));
                    } catch (const std::exception&) {
                        freeReplyObject(reply);
                        return nullptr;
                    }

                    // 连接到新节点
                    std::string new_key = make_node_key(new_host, new_port);
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (connections_.find(new_key) == connections_.end()) {
                            redisContext* new_ctx = connect_node(new_host, new_port);
                            if (new_ctx) {
                                connections_[new_key] = new_ctx;
                            }
                        }
                    }

                    // 刷新 slot 映射
                    refresh_slots();
                }

                freeReplyObject(reply);
                continue;  // 重试
            }

            if (err_str.substr(0, 4) == "ASK ") {
                // ASK slot host:port
                SPDLOG_WARN("[RedisCluster]ASK redirect on slot {}: {}", slot, err_str);

                // 解析重定向目标
                std::istringstream iss(err_str.substr(4));
                int new_slot;
                std::string new_addr;
                iss >> new_slot >> new_addr;

                auto colon_pos = new_addr.find(':');
                if (colon_pos != std::string::npos) {
                    std::string new_host = new_addr.substr(0, colon_pos);
                    int new_port = 6379;
                    try {
                        new_port = std::stoi(new_addr.substr(colon_pos + 1));
                    } catch (const std::exception&) {
                        freeReplyObject(reply);
                        return nullptr;
                    }

                    // 连接到目标节点并发送 ASKING
                    std::string new_key = make_node_key(new_host, new_port);
                    redisContext* new_ctx = nullptr;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        auto it = connections_.find(new_key);
                        if (it == connections_.end() || !it->second || it->second->err) {
                            close_connection(new_key);
                            new_ctx = connect_node(new_host, new_port);
                            if (new_ctx) {
                                connections_[new_key] = new_ctx;
                            }
                        } else {
                            new_ctx = it->second;
                        }
                    }

                    if (new_ctx) {
                        // 发送 ASKING 命令
                        redisReply* ask_reply = static_cast<redisReply*>(
                            redisCommand(new_ctx, "ASKING"));
                        if (ask_reply) {
                            freeReplyObject(ask_reply);
                        }
                    }
                }

                freeReplyObject(reply);
                continue;  // 重试
            }
        }

        // 正常响应
        return reply;
    }

    SPDLOG_ERROR("[RedisCluster]Max redirects exceeded for slot {}", slot);
    return nullptr;
}

// ============================================================================
// KV 操作
// ============================================================================

bool RedisClusterClient::set(const std::string& key, const std::string& value)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "SET %s %s", key.c_str(), value.c_str());
    if (!reply) {
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_STATUS &&
                    std::string(reply->str, reply->len) == "OK");
    freeReplyObject(reply);
    return success;
}

bool RedisClusterClient::set(const std::string& key, const std::string& value, int ttl_seconds)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "SET %s %s EX %d",
                                        key.c_str(), value.c_str(), ttl_seconds);
    if (!reply) {
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_STATUS &&
                    std::string(reply->str, reply->len) == "OK");
    freeReplyObject(reply);
    return success;
}

bool RedisClusterClient::get(const std::string& key, std::string& value)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "GET %s", key.c_str());
    if (!reply) {
        return false;
    }

    if (reply->type == REDIS_REPLY_NIL) {
        freeReplyObject(reply);
        return false;
    }

    if (reply->type == REDIS_REPLY_STRING) {
        value.assign(reply->str, reply->len);
        freeReplyObject(reply);
        return true;
    }

    freeReplyObject(reply);
    return false;
}

bool RedisClusterClient::del(const std::string& key)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "DEL %s", key.c_str());
    if (!reply) {
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    freeReplyObject(reply);
    return success;
}

// ============================================================================
// Set 操作
// ============================================================================

bool RedisClusterClient::sadd(const std::string& key, const std::string& member)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "SADD %s %s", key.c_str(), member.c_str());
    if (!reply) {
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_INTEGER);
    freeReplyObject(reply);
    return success;
}

bool RedisClusterClient::srem(const std::string& key, const std::string& member)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "SREM %s %s", key.c_str(), member.c_str());
    if (!reply) {
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_INTEGER);
    freeReplyObject(reply);
    return success;
}

bool RedisClusterClient::sismember(const std::string& key, const std::string& member, bool& result)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "SISMEMBER %s %s", key.c_str(), member.c_str());
    if (!reply) {
        return false;
    }

    if (reply->type == REDIS_REPLY_INTEGER) {
        result = (reply->integer == 1);
        freeReplyObject(reply);
        return true;
    }

    freeReplyObject(reply);
    return false;
}

bool RedisClusterClient::smembers(const std::string& key, std::vector<std::string>& members)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "SMEMBERS %s", key.c_str());
    if (!reply) {
        return false;
    }

    if (reply->type == REDIS_REPLY_ARRAY) {
        members.clear();
        members.reserve(reply->elements);
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i] && reply->element[i]->type == REDIS_REPLY_STRING) {
                members.push_back(std::string(reply->element[i]->str, reply->element[i]->len));
            }
        }
        freeReplyObject(reply);
        return true;
    }

    freeReplyObject(reply);
    return false;
}

bool RedisClusterClient::scard(const std::string& key, int64_t& count)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "SCARD %s", key.c_str());
    if (!reply) {
        return false;
    }

    if (reply->type == REDIS_REPLY_INTEGER) {
        count = reply->integer;
        freeReplyObject(reply);
        return true;
    }

    freeReplyObject(reply);
    return false;
}

// ============================================================================
// Hash 操作
// ============================================================================

bool RedisClusterClient::hset(const std::string& key, const std::string& field, const std::string& value)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "HSET %s %s %s",
                                        key.c_str(), field.c_str(), value.c_str());
    if (!reply) {
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_INTEGER);
    freeReplyObject(reply);
    return success;
}

bool RedisClusterClient::hget(const std::string& key, const std::string& field, std::string& value)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "HGET %s %s", key.c_str(), field.c_str());
    if (!reply) {
        return false;
    }

    if (reply->type == REDIS_REPLY_NIL) {
        freeReplyObject(reply);
        return false;
    }

    if (reply->type == REDIS_REPLY_STRING) {
        value.assign(reply->str, reply->len);
        freeReplyObject(reply);
        return true;
    }

    freeReplyObject(reply);
    return false;
}

bool RedisClusterClient::hdel(const std::string& key, const std::string& field)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "HDEL %s %s", key.c_str(), field.c_str());
    if (!reply) {
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    freeReplyObject(reply);
    return success;
}

bool RedisClusterClient::hgetall(const std::string& key,
                                  std::unordered_map<std::string, std::string>& kvs)
{
    uint16_t slot = key_slot(key);
    redisReply* reply = execute_command(slot, "HGETALL %s", key.c_str());
    if (!reply) {
        return false;
    }

    if (reply->type == REDIS_REPLY_ARRAY) {
        kvs.clear();
        // HGETALL 返回 [field1, value1, field2, value2, ...]
        for (size_t i = 0; i + 1 < reply->elements; i += 2) {
            if (reply->element[i] && reply->element[i]->type == REDIS_REPLY_STRING &&
                reply->element[i + 1] && reply->element[i + 1]->type == REDIS_REPLY_STRING) {
                std::string field(reply->element[i]->str, reply->element[i]->len);
                std::string value(reply->element[i + 1]->str, reply->element[i + 1]->len);
                kvs[field] = value;
            }
        }
        freeReplyObject(reply);
        return true;
    }

    freeReplyObject(reply);
    return false;
}

// ============================================================================
// 集群状态
// ============================================================================

std::vector<ClusterNode> RedisClusterClient::get_nodes() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_;
}

bool RedisClusterClient::is_healthy() const
{
    return healthy_.load(std::memory_order_acquire);
}

// ============================================================================
// 后台刷新线程
// ============================================================================

void RedisClusterClient::refresh_loop()
{
    SPDLOG_INFO("[RedisCluster]Refresh thread started, interval={}ms",
                config_.retry_interval_ms);

    while (running_.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.retry_interval_ms));

        if (!running_.load(std::memory_order_acquire)) {
            break;
        }

        SPDLOG_DEBUG("[RedisCluster]Periodic slot refresh");
        refresh_slots();
    }

    SPDLOG_INFO("[RedisCluster]Refresh thread stopped");
}

// ============================================================================
// 初始化
// ============================================================================

bool RedisClusterClient::init()
{
    if (running_.load(std::memory_order_acquire)) {
        SPDLOG_WARN("[RedisCluster]Already initialized");
        return true;
    }

    // 解析 seed URI
    std::vector<ClusterNode> seeds = parse_seed_uri(config_.seed_uri);
    if (seeds.empty()) {
        SPDLOG_ERROR("[RedisCluster]No valid seed nodes in URI: {}", config_.seed_uri);
        return false;
    }

    SPDLOG_INFO("[RedisCluster]Initializing with {} seed nodes", seeds.size());

    // 连接到 seed 节点
    bool connected = false;
    for (const auto& seed : seeds) {
        redisContext* ctx = connect_node(seed.host, seed.port);
        if (ctx) {
            std::string key = make_node_key(seed.host, seed.port);
            std::lock_guard<std::mutex> lock(mutex_);
            connections_[key] = ctx;
            connected = true;
            SPDLOG_INFO("[RedisCluster]Connected to seed node {}:{}", seed.host, seed.port);
            break;  // 只需要连接一个 seed 节点即可获取 CLUSTER SLOTS
        }
    }

    if (!connected) {
        SPDLOG_ERROR("[RedisCluster]Failed to connect to any seed node");
        return false;
    }

    // 获取 CLUSTER SLOTS
    if (!refresh_slots()) {
        SPDLOG_ERROR("[RedisCluster]Failed to get initial CLUSTER SLOTS");
        // 不是致命错误，继续尝试
    }

    // 启动后台刷新线程
    running_.store(true, std::memory_order_release);
    refresh_thread_ = std::thread(&RedisClusterClient::refresh_loop, this);

    SPDLOG_INFO("[RedisCluster]Initialization complete");
    return true;
}

// ============================================================================
// 关闭
// ============================================================================

void RedisClusterClient::shutdown()
{
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }

    SPDLOG_INFO("[RedisCluster]Shutting down...");

    running_.store(false, std::memory_order_release);

    // 等待刷新线程结束
    if (refresh_thread_.joinable()) {
        refresh_thread_.join();
    }

    // 关闭所有连接
    {
        std::lock_guard<std::mutex> lock(mutex_);
        close_all_connections();
    }

    healthy_.store(false, std::memory_order_release);
    SPDLOG_INFO("[RedisCluster]Shutdown complete");
}

}  // namespace farm
