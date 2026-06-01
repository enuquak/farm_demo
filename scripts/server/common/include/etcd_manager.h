#pragma once

/**
 * EtcdManager - etcd 服务注册/发现与配置中心
 *
 * 封装 etcd-cpp-apiv3 客户端，提供：
 * - 服务注册（带 lease 自动过期）
 * - 服务发现（一次性查询 + watch 监听）
 * - 配置中心（持久化存储 + watch 监听）
 *
 * etcd key 结构：
 *   /farm/services/{service_type}/{instance_id}  -- 服务注册（带 lease）
 *   /farm/config/{key}                           -- 配置中心（无 lease）
 */

#include <etcd/Client.hpp>
#include <etcd/SyncClient.hpp>
#include <etcd/Watcher.hpp>
#include <etcd/KeepAlive.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace farm {

// 服务实例信息
struct ServiceInstance {
    std::string instance_id;
    std::string ip;
    uint16_t port = 0;
    uint32_t server_id = 0;  // 仅 game_server 使用
    uint32_t index = 0;      // 仅 dbmgr 使用
};

class EtcdManager {
public:
    /**
     * @brief 构造函数
     * @param etcd_endpoints  etcd 端点，多个用逗号分隔（如 "http://localhost:2379"）
     * @param lease_ttl       租约 TTL（秒），默认 15
     */
    EtcdManager(const std::string& etcd_endpoints, uint32_t lease_ttl = 15);
    ~EtcdManager();

    // 禁止拷贝和移动
    EtcdManager(const EtcdManager&) = delete;
    EtcdManager& operator=(const EtcdManager&) = delete;
    EtcdManager(EtcdManager&&) = delete;
    EtcdManager& operator=(EtcdManager&&) = delete;

    // === 生命周期 ===

    /**
     * @brief 连接 etcd，创建租约，启动续租
     * @return true 成功，false 失败
     */
    bool connect();

    /**
     * @brief 主动注销所有服务并关闭连接
     */
    void shutdown();

    // === 错误回调 ===

    using ErrorCallback = std::function<void(const std::string& error_msg)>;

    /**
     * @brief 设置错误回调（续租失败、watch 错误等）
     */
    void set_error_callback(ErrorCallback callback);

    // === 服务注册（带 lease）===

    /**
     * @brief 注册服务到 etcd（带 lease，进程崩溃后自动过期）
     * @param service_type  服务类型："gate"/"game"/"dbmgr"
     * @param instance_id   实例唯一标识
     * @param value_json    注册信息 JSON（如 {"ip":"0.0.0.0","port":8080}）
     * @return true 成功，false 失败
     */
    bool register_service(const std::string& service_type,
                          const std::string& instance_id,
                          const std::string& value_json);

    /**
     * @brief 从 etcd 注销服务
     */
    void deregister_service(const std::string& service_type,
                            const std::string& instance_id);

    // === 服务发现 ===

    /**
     * @brief 一次性查询当前在线的所有服务实例
     * @param service_type  服务类型："gate"/"game"/"dbmgr"
     * @return 实例列表
     */
    std::vector<ServiceInstance> discover_services(const std::string& service_type);

    /**
     * @brief 持续监听服务变更
     * @param service_type  服务类型
     * @param callback      变更回调（instance_id, instance, is_delete）
     *
     * 注意：回调在 etcd 后台线程中调用，上层需要注意线程安全
     */
    using ServiceChangeCallback = std::function<void(const std::string& instance_id,
                                                     const ServiceInstance& instance,
                                                     bool is_delete)>;
    void watch_services(const std::string& service_type,
                        ServiceChangeCallback callback);

    // === 配置中心（无 lease，持久化）===

    /**
     * @brief 写入配置到 etcd（持久化，不绑定 lease）
     * @param key         配置 key（不含 /farm/config/ 前缀）
     * @param value_json  配置值 JSON
     * @return true 成功，false 失败
     */
    bool put_config(const std::string& key, const std::string& value_json);

    /**
     * @brief 从 etcd 读取配置
     * @param key  配置 key（不含 /farm/config/ 前缀）
     * @return 配置值 JSON，不存在返回 nullopt
     */
    std::optional<std::string> get_config(const std::string& key);

    /**
     * @brief 监听配置变更
     * @param key_prefix  key 前缀
     * @param callback    变更回调（key, value_json）
     *
     * 注意：回调在 etcd 后台线程中调用，上层需要注意线程安全
     */
    using ConfigChangeCallback = std::function<void(const std::string& key,
                                                    const std::string& value_json)>;
    void watch_config(const std::string& key_prefix, ConfigChangeCallback callback);

private:
    // 解析 ServiceInstance JSON
    static ServiceInstance parse_service_json(const std::string& instance_id,
                                              const std::string& json_str);

    // etcd key 前缀常量
    static constexpr const char* KEY_PREFIX = "/farm/";
    static constexpr const char* SERVICES_PREFIX = "/farm/services/";
    static constexpr const char* CONFIG_PREFIX = "/farm/config/";

    // etcd 错误码常量
    static constexpr int ETCD_KEY_NOT_FOUND = 100;

    // 成员变量
    std::string endpoints_;
    uint32_t lease_ttl_;
    int64_t lease_id_ = 0;

    // etcd 客户端
    std::unique_ptr<etcd::SyncClient> client_;

    // KeepAlive 对象（自动续租）
    std::unique_ptr<etcd::KeepAlive> keep_alive_;

    // watch 对象（需要保持存活）
    // 注意：当前只支持一个 service watcher 和一个 config watcher。
    //       如果后续需要监听多个 service_type 或 key_prefix，
    //       应改为 std::map<std::string, std::unique_ptr<etcd::Watcher>>。
    std::unique_ptr<etcd::Watcher> service_watcher_;
    std::unique_ptr<etcd::Watcher> config_watcher_;

    // 已注册的 key 列表（shutdown 时需要删除）
    std::vector<std::string> registered_keys_;
    std::mutex registered_keys_mutex_;

    std::mutex error_mutex_;
    ErrorCallback error_callback_;

    std::atomic<bool> running_{false};
};

}  // namespace farm
