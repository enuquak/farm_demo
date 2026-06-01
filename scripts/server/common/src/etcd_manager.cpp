#include "etcd_manager.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <sstream>

namespace farm {

// ============================================================================
// 构造/析构
// ============================================================================

EtcdManager::EtcdManager(const std::string& etcd_endpoints, uint32_t lease_ttl)
    : endpoints_(etcd_endpoints)
    , lease_ttl_(lease_ttl)
{
}

EtcdManager::~EtcdManager() {
    shutdown();
}

// ============================================================================
// 生命周期
// ============================================================================

bool EtcdManager::connect() {
    try {
        // 创建 etcd 同步客户端
        client_ = std::make_unique<etcd::SyncClient>(endpoints_);
        if (!client_) {
            SPDLOG_ERROR("[Etcd]Failed to create client for endpoints: {}", endpoints_);
            return false;
        }

        // 创建 KeepAlive（自动续租）
        keep_alive_ = std::make_unique<etcd::KeepAlive>(
            endpoints_,
            [this](std::exception_ptr eptr) {
                try {
                    if (eptr) std::rethrow_exception(eptr);
                } catch (const std::exception& e) {
                    SPDLOG_ERROR("[Etcd]KeepAlive error: {}", e.what());
                    ErrorCallback cb;
                    {
                        std::lock_guard<std::mutex> lock(error_mutex_);
                        cb = error_callback_;
                    }
                    if (cb) cb(std::string("KeepAlive error: ") + e.what());
                }
            },
            static_cast<int>(lease_ttl_)
        );

        // 获取 lease ID
        lease_id_ = keep_alive_->Lease();

        running_ = true;

        SPDLOG_INFO("[Etcd]Connected to {}, lease_id={}, ttl={}", endpoints_, lease_id_, lease_ttl_);
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Etcd]Connect exception: {}", e.what());
        return false;
    }
}

void EtcdManager::shutdown() {
    // 设置 running_ 为 false
    running_ = false;

    // 停止 KeepAlive
    if (keep_alive_) {
        keep_alive_->Cancel();
        keep_alive_.reset();
    }

    // 注销所有已注册的服务
    {
        std::lock_guard<std::mutex> lock(registered_keys_mutex_);
        for (const auto& key : registered_keys_) {
            try {
                if (client_) {
                    client_->rm(key);
                    SPDLOG_INFO("[Etcd]Deregistered key: {}", key);
                }
            } catch (const std::exception& e) {
                SPDLOG_ERROR("[Etcd]Failed to deregister key {}: {}", key, e.what());
            }
        }
        registered_keys_.clear();
    }

    // 停止 watch
    service_watcher_.reset();
    config_watcher_.reset();

    // 关闭客户端
    client_.reset();

    SPDLOG_INFO("[Etcd]Shutdown complete");
}

void EtcdManager::set_error_callback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    error_callback_ = std::move(callback);
}

// ============================================================================
// 服务注册
// ============================================================================

bool EtcdManager::register_service(const std::string& service_type,
                                    const std::string& instance_id,
                                    const std::string& value_json) {
    if (!client_ || !running_) {
        SPDLOG_ERROR("[Etcd]Not connected");
        return false;
    }

    std::string key = std::string(SERVICES_PREFIX) + service_type + "/" + instance_id;

    try {
        auto resp = client_->set(key, value_json, lease_id_);
        if (resp.error_code() != 0) {
            SPDLOG_ERROR("[Etcd]Register service failed: {} - {}",
                         resp.error_code(), resp.error_message());
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(registered_keys_mutex_);
            if (std::find(registered_keys_.begin(), registered_keys_.end(), key) == registered_keys_.end()) {
                registered_keys_.push_back(key);
            }
        }

        SPDLOG_INFO("[Etcd]Registered service: {} = {}", key, value_json);
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Etcd]Register service exception: {}", e.what());
        return false;
    }
}

void EtcdManager::deregister_service(const std::string& service_type,
                                      const std::string& instance_id) {
    if (!client_) return;

    std::string key = std::string(SERVICES_PREFIX) + service_type + "/" + instance_id;

    try {
        client_->rm(key);
        SPDLOG_INFO("[Etcd]Deregistered service: {}", key);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Etcd]Deregister service exception: {}", e.what());
    }

    // 从 registered_keys_ 中移除
    {
        std::lock_guard<std::mutex> lock(registered_keys_mutex_);
        auto it = std::find(registered_keys_.begin(), registered_keys_.end(), key);
        if (it != registered_keys_.end()) {
            registered_keys_.erase(it);
        }
    }
}

// ============================================================================
// 服务发现
// ============================================================================

std::vector<ServiceInstance> EtcdManager::discover_services(const std::string& service_type) {
    std::vector<ServiceInstance> result;
    if (!client_) return result;

    std::string prefix = std::string(SERVICES_PREFIX) + service_type + "/";

    try {
        auto resp = client_->ls(prefix);
        if (resp.error_code() != 0) {
            // error_code 100 = key not found, 这是正常情况（还没有注册的实例）
            if (resp.error_code() != ETCD_KEY_NOT_FOUND) {
                SPDLOG_ERROR("[Etcd]Discover services failed: {} - {}",
                             resp.error_code(), resp.error_message());
            }
            return result;
        }

        // 遍历所有 key
        size_t keys_count = resp.keys().size();
        for (size_t i = 0; i < keys_count; ++i) {
            std::string key = resp.key(i);
            std::string instance_id = key.substr(prefix.length());
            std::string value = resp.value(i).as_string();

            ServiceInstance inst = parse_service_json(instance_id, value);
            if (!inst.ip.empty()) {
                result.push_back(inst);
            }
        }

        SPDLOG_INFO("[Etcd]Discovered {} {} instances", result.size(), service_type);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Etcd]Discover services exception: {}", e.what());
    }

    return result;
}

ServiceInstance EtcdManager::parse_service_json(const std::string& instance_id,
                                                 const std::string& json_str) {
    ServiceInstance inst;
    inst.instance_id = instance_id;

    try {
        auto json = nlohmann::json::parse(json_str);
        inst.ip = json.value("ip", "");
        inst.port = static_cast<uint16_t>(json.value("port", 0));
        inst.server_id = json.value("server_id", 0u);
        inst.index = json.value("index", 0u);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Etcd]Parse service JSON failed: {} - {}", json_str, e.what());
    }

    return inst;
}

void EtcdManager::watch_services(const std::string& service_type,
                                  ServiceChangeCallback callback) {
    if (!client_) return;

    std::string prefix = std::string(SERVICES_PREFIX) + service_type + "/";

    // 创建 watch（异步）
    service_watcher_ = std::make_unique<etcd::Watcher>(
        *client_, prefix,
        [this, prefix, service_type, cb = std::move(callback)](etcd::Response resp) {
            if (resp.error_code() != 0) {
                SPDLOG_ERROR("[Etcd]Watch services error: {} - {}",
                             resp.error_code(), resp.error_message());
                {
                    ErrorCallback err_cb;
                    {
                        std::lock_guard<std::mutex> lock(error_mutex_);
                        err_cb = error_callback_;
                    }
                    if (err_cb) err_cb("Watch services error: " + resp.error_message());
                }
                return;
            }

            for (const auto& ev : resp.events()) {
                std::string key = ev.kv().key();
                std::string instance_id = key.substr(prefix.length());
                bool is_delete = (etcd::Event::EventType::DELETE_ == ev.event_type());

                ServiceInstance inst;
                if (!is_delete) {
                    inst = parse_service_json(instance_id, ev.kv().as_string());
                }

                SPDLOG_INFO("[Etcd]Service change: {} instance_id={} delete={}",
                            service_type, instance_id, is_delete);

                cb(instance_id, inst, is_delete);
            }
        },
        true  // recursive
    );

    SPDLOG_INFO("[Etcd]Watching services: {}", service_type);
}

// ============================================================================
// 配置中心
// ============================================================================

bool EtcdManager::put_config(const std::string& key, const std::string& value_json) {
    if (!client_) {
        SPDLOG_ERROR("[Etcd]Not connected");
        return false;
    }

    std::string full_key = std::string(CONFIG_PREFIX) + key;

    try {
        auto resp = client_->set(full_key, value_json);
        if (resp.error_code() != 0) {
            SPDLOG_ERROR("[Etcd]Put config failed: {} - {}",
                         resp.error_code(), resp.error_message());
            return false;
        }

        SPDLOG_INFO("[Etcd]Put config: {} = {}", full_key, value_json);
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Etcd]Put config exception: {}", e.what());
        return false;
    }
}

std::optional<std::string> EtcdManager::get_config(const std::string& key) {
    if (!client_) return std::nullopt;

    std::string full_key = std::string(CONFIG_PREFIX) + key;

    try {
        auto resp = client_->get(full_key);
        if (resp.error_code() != 0) {
            if (resp.error_code() == ETCD_KEY_NOT_FOUND) {
                SPDLOG_INFO("[Etcd]Config not found: {}", full_key);
                return std::nullopt;
            }
            SPDLOG_ERROR("[Etcd]Get config failed: {} - {}",
                         resp.error_code(), resp.error_message());
            return std::nullopt;
        }

        return resp.value().as_string();
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Etcd]Get config exception: {}", e.what());
        return std::nullopt;
    }
}

void EtcdManager::watch_config(const std::string& key_prefix,
                                ConfigChangeCallback callback) {
    if (!client_) return;

    std::string prefix = std::string(CONFIG_PREFIX) + key_prefix;

    config_watcher_ = std::make_unique<etcd::Watcher>(
        *client_, prefix,
        [this, prefix, cb = std::move(callback)](etcd::Response resp) {
            if (resp.error_code() != 0) {
                SPDLOG_ERROR("[Etcd]Config watch error: {} - {}",
                             resp.error_code(), resp.error_message());
                {
                    ErrorCallback err_cb;
                    {
                        std::lock_guard<std::mutex> lock(error_mutex_);
                        err_cb = error_callback_;
                    }
                    if (err_cb) err_cb("Config watch error: " + resp.error_message());
                }
                return;
            }

            for (const auto& ev : resp.events()) {
                if (etcd::Event::EventType::DELETE_ == ev.event_type()) {
                    SPDLOG_WARN("[Etcd]Config key deleted: {}", ev.kv().key());
                    continue;
                }

                std::string key = ev.kv().key();
                std::string relative_key = key.substr(prefix.length());
                std::string value = ev.kv().as_string();

                SPDLOG_INFO("[Etcd]Config change: {} = {}", key, value);
                cb(relative_key, value);
            }
        },
        true  // recursive
    );

    SPDLOG_INFO("[Etcd]Watching config: {}", key_prefix);
}

}  // namespace farm
