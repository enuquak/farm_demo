#pragma once

/**
 * @file mongo_replica_set.h
 * @brief MongoDB Replica Set 连接管理，支持 read preference 切换
 *
 * 基于 libmongoc 实现，支持：
 * - Replica Set URI 连接（自动发现 primary/secondary）
 * - Read Preference 管理（primary / secondary / primaryPreferred）
 * - 写后读一致性保证（read_from_primary_after_write / restore_read_preference）
 * - 连接状态机（DISCONNECTED / CONNECTING / CONNECTED / FAILED）
 */

#include "mongo_connection.h"
#include "db_types.h"

#include <mongoc/mongoc.h>
#include <string>

namespace farm {

/**
 * @brief MongoDB Replica Set 配置
 */
struct MongoReplicaSetConfig {
    std::string uri;                          // mongodb://host1,host2,host3/db?replicaSet=rs0
    int connect_timeout_ms = 10000;           // 连接超时（毫秒）
    int socket_timeout_ms = 30000;            // Socket 超时（毫秒）
    int retry_interval_ms = 3000;             // 重试间隔（毫秒）
    int max_retry_count = 0;                  // 最大重试次数（0 = 无限重试）
    std::string read_preference = "primary";  // primary / secondary / primaryPreferred
};

/**
 * @brief MongoDB Replica Set 连接
 *
 * 继承自 MongoConnection，保持与现有 MongoServer 的兼容性。
 * 支持 read preference 管理，提供写后读一致性保证。
 *
 * 使用示例:
 * @code
 *   MongoReplicaSetConfig config;
 *   config.uri = "mongodb://127.0.0.1:27017,127.0.0.1:27018/farm?replicaSet=rs0";
 *
 *   MongoReplicaSetConnection conn(config);
 *   conn.connect();
 *
 *   // 默认从 primary 读取
 *   mongoc_client_t* client = conn.client();
 *
 *   // 写后切到 primary 读，保证一致性
 *   conn.read_from_primary_after_write();
 *   // ... 执行读操作 ...
 *   conn.restore_read_preference();
 *
 *   conn.disconnect();
 * @endcode
 */
class MongoReplicaSetConnection : public MongoConnection {
public:
    /**
     * @brief 构造函数
     * @param config Replica Set 配置
     */
    explicit MongoReplicaSetConnection(const MongoReplicaSetConfig& config);

    /**
     * @brief 析构函数
     */
    ~MongoReplicaSetConnection() override;

    // 禁止拷贝
    MongoReplicaSetConnection(const MongoReplicaSetConnection&) = delete;
    MongoReplicaSetConnection& operator=(const MongoReplicaSetConnection&) = delete;

    /**
     * @brief 连接到 MongoDB Replica Set（使用配置中的 URI）
     * 解析 URI，配置 read preference，测试连接
     * @return true 成功，false 失败
     */
    bool connect();

    /**
     * @brief 断开连接
     */
    void disconnect() override;

    /**
     * @brief 是否已连接
     */
    bool is_connected() const override;

    /**
     * @brief 写后切到 primary 读，保证 read-your-own-write 一致性
     *
     * 调用后，所有读操作将路由到 primary 节点。
     * 典型用法：写操作完成后立即调用，确保后续读能读到刚写入的数据。
     */
    void read_from_primary_after_write();

    /**
     * @brief 恢复到配置文件中定义的 read preference
     *
     * 配合 read_from_primary_after_write() 使用。
     * 典型用法：读操作完成后调用，恢复到默认的读偏好。
     */
    void restore_read_preference();

    /**
     * @brief 获取当前连接状态
     */
    ConnectionState state() const;

private:
    /**
     * @brief 应用 read preference 到 client
     * @param preference 读偏好名称（primary / secondary / primaryPreferred）
     */
    void apply_read_preference(const std::string& preference);

    MongoReplicaSetConfig config_;
    ConnectionState state_ = ConnectionState::DISCONNECTED;
    std::string saved_read_preference_;
};

}  // namespace farm
