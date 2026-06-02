#pragma once

/**
 * @file db_types.h
 * @brief 数据库管理器共享类型定义
 *
 * 定义 DBMgr 与各服务间共享的枚举、结构体类型，
 * 包括连接状态、操作结果码、账户角色、连接池统计等。
 */

#include <cstdint>
#include <string>

namespace farm {

// 连接状态机
enum class ConnectionState {
    DISCONNECTED,       // 未连接
    CONNECTING,         // 正在连接
    CONNECTED,          // 已连接
    FAILED,             // 连接失败（可重试）
    FAILED_PERMANENT    // 连接失败（不可恢复）
};

// 通用数据操作结果码
enum class DataResult : int32_t {
    SUCCESS         = 0,
    KEY_NOT_FOUND   = 1,
    IO_ERROR        = 2,
    PARSE_ERROR     = 3
};

// 账户操作结果码
enum class AccountResult : int32_t {
    SUCCESS             = 0,
    IO_ERROR            = 1,
    ROLE_ALREADY_EXISTS = 2
};

// 缓存操作结果码
enum class CacheResult : int32_t {
    SUCCESS             = 0,
    NOT_FOUND           = 1,
    CONNECTION_ERROR    = 2,
    TIMEOUT             = 3
};

// 账户角色信息
struct AccountRole {
    int32_t server_id = 0;
    int64_t player_id = 0;
    std::string role_name;
};

// 连接池统计信息
struct PoolStats {
    size_t total_connections = 0;
    size_t idle_connections = 0;
    size_t active_connections = 0;
    size_t waiting_requests = 0;
    size_t connection_errors = 0;
};

// 集群统计信息
struct ClusterStats {
    size_t total_nodes = 0;
    size_t healthy_nodes = 0;
    size_t slot_coverage = 0;
    std::string current_master;
};

}  // namespace farm
