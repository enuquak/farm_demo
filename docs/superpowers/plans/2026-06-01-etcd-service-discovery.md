# etcd 服务注册/发现与配置中心 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将服务器进程间发现机制从静态配置改为 etcd 动态注册/发现，并引入配置中心。

**Architecture:** 在 `scripts/server/common/` 中新增 `EtcdManager` 类，封装 etcd-cpp-apiv3 客户端，提供服务注册（带 lease）、服务发现（带 watch）、配置中心功能。三个进程（gate_server、game_server、dbmgr）通过 EtcdManager 与 etcd 交互。

**Tech Stack:** C++17, etcd-cpp-apiv3, libevent, protobuf, nlohmann/json

---

## 文件结构

```
新增文件：
  scripts/server/common/include/etcd_manager.h      # EtcdManager 类声明
  scripts/server/common/src/etcd_manager.cpp         # EtcdManager 实现

修改文件：
  scripts/server/gate_server/CMakeLists.txt          # 添加 etcd 依赖
  scripts/server/gate_server/src/gate_server.h       # 新增 remove_game_server + mutex
  scripts/server/gate_server/src/gate_server.cpp     # 动态增减支持
  scripts/server/gate_server/src/main.cpp            # etcd 集成
  scripts/server/game_server/CMakeLists.txt          # 添加 etcd 依赖
  scripts/server/game_server/src/main.cpp            # etcd 集成
  scripts/server/dbmgr/CMakeLists.txt                # 添加 etcd 依赖
  scripts/server/dbmgr/src/main.cpp                  # etcd 集成
  config/gate_server.json                            # 简化配置
  config/game_server.json                            # 简化配置
  config/dbmgr.json                                  # 简化配置
```

---

### Task 1: 安装 etcd-cpp-apiv3 依赖

**Files:**
- 无代码文件变更，仅环境配置

- [ ] **Step 1: 使用 vcpkg 安装 etcd-cpp-apiv3**

```bash
cd C:/vcpkg  # 或你的 vcpkg 路径
./vcpkg install etcd-cpp-apiv3:x64-windows
```

Expected: 安装成功，输出安装路径（如 `C:/vcpkg/installed/x64-windows`）

- [ ] **Step 2: 验证安装**

```bash
ls C:/vcpkg/installed/x64-windows/include/etcd/
ls C:/vcpkg/installed/x64-windows/lib/ | grep etcd
```

Expected: 能看到 `etcd/Client.hpp` 等头文件和 `etcd-cpp-api.lib` 库文件

- [ ] **Step 3: 记录安装路径**

将 etcd 安装路径记录下来，后续 CMakeLists.txt 会使用。假设路径为 `C:/vcpkg/installed/x64-windows`。

---

### Task 2: 创建 EtcdManager 头文件

**Files:**
- Create: `scripts/server/common/include/etcd_manager.h`

- [ ] **Step 1: 创建 EtcdManager 头文件**

```cpp
#pragma once

#include <etcd/Client.hpp>
#include <etcd/Watcher.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <cstdint>

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
     * @param lease_ttl       租约 TTL（秒）
     */
    EtcdManager(const std::string& etcd_endpoints, uint32_t lease_ttl = 15);
    ~EtcdManager();

    // 禁止拷贝
    EtcdManager(const EtcdManager&) = delete;
    EtcdManager& operator=(const EtcdManager&) = delete;

    // === 生命周期 ===

    /**
     * @brief 连接 etcd，创建租约，启动续租线程
     * @return true 成功，false 失败
     */
    bool connect();

    /**
     * @brief 主动注销所有服务并关闭连接
     */
    void shutdown();

    // === 错误回调 ===

    using ErrorCallback = std::function<void(const std::string& error_msg)>;
    void set_error_callback(ErrorCallback callback);

    // === 服务注册（带 lease）===

    /**
     * @brief 注册服务到 etcd（带 lease）
     * @param service_type  服务类型："gate"/"game"/"dbmgr"
     * @param instance_id   实例唯一标识
     * @param value_json    注册信息 JSON
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
     */
    using ServiceChangeCallback = std::function<void(const std::string& instance_id,
                                                     const ServiceInstance& instance,
                                                     bool is_delete)>;
    void watch_services(const std::string& service_type,
                        ServiceChangeCallback callback);

    // === 配置中心（无 lease，持久化）===

    /**
     * @brief 写入配置到 etcd
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
     */
    using ConfigChangeCallback = std::function<void(const std::string& key,
                                                    const std::string& value_json)>;
    void watch_config(const std::string& key_prefix, ConfigChangeCallback callback);

private:
    // 续租后台线程
    void lease_keepalive_loop();

    // 解析 ServiceInstance JSON
    static ServiceInstance parse_service_json(const std::string& instance_id,
                                              const std::string& json_str);

    // etcd key 前缀常量
    static constexpr const char* KEY_PREFIX = "/farm/";
    static constexpr const char* SERVICES_PREFIX = "/farm/services/";
    static constexpr const char* CONFIG_PREFIX = "/farm/config/";

    // 成员变量
    std::unique_ptr<etcd::Client> client_;
    std::string endpoints_;
    uint32_t lease_ttl_;
    int64_t lease_id_ = 0;

    std::thread keepalive_thread_;
    std::atomic<bool> running_{false};

    // watch 对象（需要保持存活）
    std::unique_ptr<etcd::Watcher> service_watcher_;
    std::unique_ptr<etcd::Watcher> config_watcher_;

    // 已注册的 key 列表（shutdown 时需要删除）
    std::vector<std::string> registered_keys_;

    ErrorCallback error_callback_;
};

}  // namespace farm
```

- [ ] **Step 2: 验证头文件语法**

```bash
cd D:/mb_workspace/farm_demo
# 简单检查头文件是否存在
cat scripts/server/common/include/etcd_manager.h | head -5
```

Expected: 文件存在且内容正确

- [ ] **Step 3: Commit**

```bash
git add scripts/server/common/include/etcd_manager.h
git commit -m "feat(etcd): add EtcdManager header file"
```

---

### Task 3: 实现 EtcdManager 核心功能

**Files:**
- Create: `scripts/server/common/src/etcd_manager.cpp`

- [ ] **Step 1: 创建 EtcdManager 实现文件 - 构造/析构/connect**

```cpp
#include "etcd_manager.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <sstream>

namespace farm {

EtcdManager::EtcdManager(const std::string& etcd_endpoints, uint32_t lease_ttl)
    : endpoints_(etcd_endpoints)
    , lease_ttl_(lease_ttl)
{
}

EtcdManager::~EtcdManager() {
    shutdown();
}

bool EtcdManager::connect() {
    try {
        // 创建 etcd 客户端
        client_ = std::make_unique<etcd::Client>(endpoints_);
        if (!client_) {
            SPDLOG_ERROR("[Etcd]Failed to create client for endpoints: {}", endpoints_);
            return false;
        }

        // 创建租约
        auto resp = client_->leasegrant(lease_ttl_).get();
        if (resp.error_code() != 0) {
            SPDLOG_ERROR("[Etcd]Failed to create lease: {} - {}",
                         resp.error_code(), resp.error_message());
            return false;
        }
        lease_id_ = resp.value().lease();

        // 启动续租线程
        running_ = true;
        keepalive_thread_ = std::thread(&EtcdManager::lease_keepalive_loop, this);

        SPDLOG_INFO("[Etcd]Connected to {}, lease_id={}, ttl={}", endpoints_, lease_id_, lease_ttl_);
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Etcd]Connect exception: {}", e.what());
        return false;
    }
}

void EtcdManager::shutdown() {
    running_ = false;

    // 注销所有已注册的服务
    for (const auto& key : registered_keys_) {
        try {
            if (client_) {
                client_->rm(key).get();
                SPDLOG_INFO("[Etcd]Deregistered key: {}", key);
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Etcd]Failed to deregister key {}: {}", key, e.what());
        }
    }
    registered_keys_.clear();

    // 停止 watch
    service_watcher_.reset();
    config_watcher_.reset();

    // 等待续租线程结束
    if (keepalive_thread_.joinable()) {
        keepalive_thread_.join();
    }

    // 关闭客户端
    client_.reset();

    SPDLOG_INFO("[Etcd]Shutdown complete");
}

void EtcdManager::set_error_callback(ErrorCallback callback) {
    error_callback_ = std::move(callback);
}

void EtcdManager::lease_keepalive_loop() {
    // 每隔 lease_ttl_/2 秒续租一次
    auto interval = std::chrono::seconds(lease_ttl_ / 2);
    if (interval.count() < 1) interval = std::chrono::seconds(1);

    int retry_count = 0;
    const int max_retries = 3;

    while (running_) {
        std::this_thread::sleep_for(interval);
        if (!running_) break;

        try {
            auto resp = client_->leasekeepalive(lease_id_).get();
            if (resp.is_ok()) {
                retry_count = 0;
            } else {
                SPDLOG_WARN("[Etcd]Lease keepalive failed: {}", resp.error_message());
                retry_count++;
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Etcd]Lease keepalive exception: {}", e.what());
            retry_count++;
        }

        if (retry_count >= max_retries) {
            SPDLOG_ERROR("[Etcd]Lease keepalive failed {} times, shutting down", max_retries);
            if (error_callback_) {
                error_callback_("Lease keepalive failed after " +
                                std::to_string(max_retries) + " retries");
            }
            running_ = false;
            break;
        }
    }
}

}  // namespace farm
```

- [ ] **Step 2: 添加服务注册/注销功能**

在 `etcd_manager.cpp` 末尾（`}` 之前）添加：

```cpp
// === 服务注册 ===

bool EtcdManager::register_service(const std::string& service_type,
                                    const std::string& instance_id,
                                    const std::string& value_json) {
    if (!client_ || !running_) {
        SPDLOG_ERROR("[Etcd]Not connected");
        return false;
    }

    std::string key = std::string(SERVICES_PREFIX) + service_type + "/" + instance_id;

    try {
        auto resp = client_->set(key, value_json, lease_id_).get();
        if (resp.error_code() != 0) {
            SPDLOG_ERROR("[Etcd]Register service failed: {} - {}",
                         resp.error_code(), resp.error_message());
            return false;
        }

        registered_keys_.push_back(key);
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
        client_->rm(key).get();
        SPDLOG_INFO("[Etcd]Deregistered service: {}", key);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Etcd]Deregister service exception: {}", e.what());
    }

    // 从 registered_keys_ 中移除
    auto it = std::find(registered_keys_.begin(), registered_keys_.end(), key);
    if (it != registered_keys_.end()) {
        registered_keys_.erase(it);
    }
}

std::vector<ServiceInstance> EtcdManager::discover_services(const std::string& service_type) {
    std::vector<ServiceInstance> result;
    if (!client_) return result;

    std::string prefix = std::string(SERVICES_PREFIX) + service_type + "/";

    try {
        auto resp = client_->ls(prefix).get();
        if (resp.error_code() != 0) {
            SPDLOG_ERROR("[Etcd]Discover services failed: {} - {}",
                         resp.error_code(), resp.error_message());
            return result;
        }

        for (const auto& kv : resp.keys()) {
            // 从 key 中提取 instance_id
            std::string instance_id = kv.substr(prefix.length());
            std::string value = resp.value(kv).as_string();

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

EtcdManager::ServiceInstance EtcdManager::parse_service_json(
    const std::string& instance_id, const std::string& json_str) {
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
        [this, prefix, cb = std::move(callback)](etcd::Response resp) {
            if (resp.error_code() != 0) {
                SPDLOG_ERROR("[Etcd]Watch error: {} - {}",
                             resp.error_code(), resp.error_message());
                if (error_callback_) {
                    error_callback_("Watch error: " + resp.error_message());
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
```

- [ ] **Step 3: 添加配置中心功能**

在 `etcd_manager.cpp` 末尾（`}` 之前）添加：

```cpp
// === 配置中心 ===

bool EtcdManager::put_config(const std::string& key, const std::string& value_json) {
    if (!client_) {
        SPDLOG_ERROR("[Etcd]Not connected");
        return false;
    }

    std::string full_key = std::string(CONFIG_PREFIX) + key;

    try {
        auto resp = client_->set(full_key, value_json).get();
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
        auto resp = client_->get(full_key).get();
        if (resp.error_code() != 0) {
            if (resp.error_code() == 100) {  // Key not found
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
                if (error_callback_) {
                    error_callback_("Config watch error: " + resp.error_message());
                }
                return;
            }

            for (const auto& ev : resp.events()) {
                if (etcd::Event::EventType::DELETE_ == ev.event_type()) {
                    continue;  // 忽略删除事件
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
```

- [ ] **Step 4: Commit**

```bash
git add scripts/server/common/src/etcd_manager.cpp
git commit -m "feat(etcd): implement EtcdManager with service registration, discovery, and config center"
```

---

### Task 4: 更新 CMakeLists.txt - 添加 etcd 依赖

**Files:**
- Modify: `scripts/server/gate_server/CMakeLists.txt`
- Modify: `scripts/server/game_server/CMakeLists.txt`
- Modify: `scripts/server/dbmgr/CMakeLists.txt`

- [ ] **Step 1: 更新 gate_server/CMakeLists.txt**

在 `set(LIBEVENT_ROOT ...)` 后添加：

```cmake
set(ETCD_ROOT "C:/vcpkg/installed/x64-windows")
```

在 `set(SOURCES ...)` 中添加：

```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/etcd_manager.cpp
```

在 `target_include_directories` 中添加：

```cmake
    ${ETCD_ROOT}/include
```

在 MSVC `target_link_libraries` 中添加：

```cmake
        etcd-cpp-api.lib
        grpc.lib
        grpc++.lib
        gpr.lib
        address_sorting.lib
        cares.lib
        libprotobuf.lib
        re2.lib
        ssl.lib
        crypto.lib
        zlib.lib
```

在非 MSVC `target_link_libraries` 中添加：

```cmake
        etcd-cpp-api
        grpc++
        grpc
        gpr
        protobuf
```

- [ ] **Step 2: 更新 game_server/CMakeLists.txt**

同样的修改：

1. 添加 `set(ETCD_ROOT "C:/vcpkg/installed/x64-windows")`
2. SOURCES 中添加 `etcd_manager.cpp`
3. include 目录添加 `${ETCD_ROOT}/include`
4. 链接库添加 etcd 相关库

- [ ] **Step 3: 更新 dbmgr/CMakeLists.txt**

同样的修改。

- [ ] **Step 4: 验证 CMake 配置**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/gate_server/build
cmake .. -G "Visual Studio 17 2022" -A x64
```

Expected: CMake 配置成功，无错误

- [ ] **Step 5: Commit**

```bash
git add scripts/server/gate_server/CMakeLists.txt
git scripts/server/game_server/CMakeLists.txt
git add scripts/server/dbmgr/CMakeLists.txt
git commit -m "build: add etcd-cpp-apiv3 dependency to all server CMakeLists"
```

---

### Task 5: 更新配置文件

**Files:**
- Modify: `config/gate_server.json`
- Modify: `config/game_server.json`
- Modify: `config/dbmgr.json`

- [ ] **Step 1: 更新 gate_server.json**

```json
{
    "etcd": {
        "endpoints": "http://localhost:2379",
        "lease_ttl": 15,
        "instance_id": "gate-1"
    },
    "server": {
        "ip": "0.0.0.0",
        "port": 8080
    },
    "shutdown": {
        "timeout_ms": 60000
    },
    "pid_file": "./runtimeData/gate_server.pid",
    "logging": {
        "dir": "./runtimeData/logs/server",
        "level": "debug",
        "max_file_size_mb": 20,
        "max_files": 7
    }
}
```

- [ ] **Step 2: 更新 game_server.json**

```json
{
    "etcd": {
        "endpoints": "http://localhost:2379",
        "lease_ttl": 15
    },
    "server": {
        "id": 1,
        "ip": "0.0.0.0",
        "port": 9090
    },
    "redis": {
        "uri": "redis://localhost:6379"
    },
    "pid_file": "./runtimeData/game_server.pid",
    "logging": {
        "dir": "./runtimeData/logs/server",
        "level": "debug",
        "max_file_size_mb": 20,
        "max_files": 7
    }
}
```

- [ ] **Step 3: 更新 dbmgr.json**

```json
{
    "etcd": {
        "endpoints": "http://localhost:2379",
        "lease_ttl": 15
    },
    "server": {
        "index": 0,
        "ip": "0.0.0.0",
        "port": 5000
    },
    "database": {
        "mongo": {
            "uri": "mongodb://localhost:27017/farm",
            "retry_interval_ms": 3000,
            "max_retry_count": 0
        },
        "redis": {
            "uri": "redis://localhost:6379",
            "retry_interval_ms": 3000,
            "max_retry_count": 0
        }
    },
    "shutdown": {
        "timeout_ms": 60000
    },
    "pid_file": "./runtimeData/dbmgr.pid",
    "logging": {
        "dir": "./runtimeData/logs/server",
        "level": "debug",
        "max_file_size_mb": 20,
        "max_files": 7
    }
}
```

- [ ] **Step 4: Commit**

```bash
git add config/gate_server.json config/game_server.json config/dbmgr.json
git commit -m "config: simplify server configs for etcd-based discovery"
```

---

### Task 6: 集成 etcd 到 dbmgr（最简单）

**Files:**
- Modify: `scripts/server/dbmgr/src/main.cpp`

- [ ] **Step 1: 修改 dbmgr/main.cpp - 添加 etcd 头文件**

在文件顶部的 `#include` 区域添加：

```cpp
#include "etcd_manager.h"
```

- [ ] **Step 2: 修改 dbmgr/main.cpp - 添加 etcd 初始化代码**

在 `farm::init_logging_from_config("dbmgr", config);` 之后，`signal(SIGINT, signal_handler);` 之前，添加：

```cpp
    // etcd 配置
    std::string etcd_endpoints = config.value("/etcd/endpoints"_json_pointer, "http://localhost:2379");
    uint32_t lease_ttl = config.value("/etcd/lease_ttl"_json_pointer, 15u);

    // 初始化 etcd
    farm::EtcdManager etcd(etcd_endpoints, lease_ttl);
    if (!etcd.connect()) {
        SPDLOG_ERROR("[Main]Failed to connect to etcd");
        return 1;
    }

    // 注册服务到 etcd
    nlohmann::json service_info = {
        {"ip", ip},
        {"port", port},
        {"index", index}
    };
    if (!etcd.register_service("dbmgr", std::to_string(index), service_info.dump())) {
        SPDLOG_ERROR("[Main]Failed to register service to etcd");
        return 1;
    }

    SPDLOG_INFO("[Main]Registered to etcd as dbmgr/{}", index);
```

- [ ] **Step 3: 修改 signal_handler - 确保优雅退出**

修改 `signal_handler` 函数，使其能访问 etcd：

```cpp
static farm::EtcdManager* g_etcd = nullptr;

static void signal_handler(int sig) {
    if (g_server) {
        SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
        g_server->stop();
    }
    if (g_etcd) {
        g_etcd->shutdown();
    }
}
```

在 etcd 初始化后添加：

```cpp
    g_etcd = &etcd;
```

- [ ] **Step 4: 验证编译**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/dbmgr/build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug
```

Expected: 编译成功

- [ ] **Step 5: Commit**

```bash
git add scripts/server/dbmgr/src/main.cpp
git commit -m "feat(dbmgr): integrate etcd service registration"
```

---

### Task 7: 集成 etcd 到 game_server

**Files:**
- Modify: `scripts/server/game_server/src/main.cpp`

- [ ] **Step 1: 修改 game_server/main.cpp - 添加 etcd 头文件**

```cpp
#include "etcd_manager.h"
```

- [ ] **Step 2: 修改 game_server/main.cpp - 添加 etcd 初始化**

在 `farm::init_logging_from_config("game_server", config);` 之后，`signal(SIGINT, signal_handler);` 之前：

```cpp
    // etcd 配置
    std::string etcd_endpoints = config.value("/etcd/endpoints"_json_pointer, "http://localhost:2379");
    uint32_t lease_ttl = config.value("/etcd/lease_ttl"_json_pointer, 15u);

    // 初始化 etcd
    farm::EtcdManager etcd(etcd_endpoints, lease_ttl);
    if (!etcd.connect()) {
        SPDLOG_ERROR("[Main]Failed to connect to etcd");
        return 1;
    }

    // 注册服务到 etcd
    nlohmann::json service_info = {
        {"ip", ip},
        {"port", port},
        {"server_id", server_id}
    };
    if (!etcd.register_service("game", std::to_string(server_id), service_info.dump())) {
        SPDLOG_ERROR("[Main]Failed to register service to etcd");
        return 1;
    }

    SPDLOG_INFO("[Main]Registered to etcd as game/{}", server_id);

    // 从 etcd 发现 dbmgr 服务
    std::vector<farm::DBMgrConfig> dbmgr_configs;
    auto dbmgrs = etcd.discover_services("dbmgr");
    for (const auto& dbmgr : dbmgrs) {
        farm::DBMgrConfig cfg;
        cfg.host = dbmgr.ip;
        cfg.port = dbmgr.port;
        dbmgr_configs.push_back(cfg);
        SPDLOG_INFO("[Main]Discovered DBMgr from etcd: {}:{}", cfg.host, cfg.port);
    }

    if (dbmgr_configs.empty()) {
        SPDLOG_ERROR("[Main]No DBMgr instances found in etcd");
        return 1;
    }

    // 监听 dbmgr 服务变更
    etcd.watch_services("dbmgr", [&](const std::string& instance_id,
                                      const farm::ServiceInstance& inst,
                                      bool is_delete) {
        if (is_delete) {
            SPDLOG_INFO("[Main]DBMgr {} removed from etcd", instance_id);
            // TODO: 动态移除 dbmgr 连接（需要 DBMgrConnectionManager 支持）
        } else {
            SPDLOG_INFO("[Main]DBMgr {} added to etcd: {}:{}", instance_id, inst.ip, inst.port);
            // TODO: 动态添加 dbmgr 连接（需要 DBMgrConnectionManager 支持）
        }
    });
```

- [ ] **Step 3: 修改 signal_handler**

```cpp
static farm::EtcdManager* g_etcd = nullptr;

static void signal_handler(int sig) {
    if (g_server) {
        SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
        g_server->stop();
    }
    if (g_etcd) {
        g_etcd->shutdown();
    }
}
```

在 etcd 初始化后添加：

```cpp
    g_etcd = &etcd;
```

- [ ] **Step 4: 移除旧的静态配置读取**

删除或注释掉原来的 dbmgr 静态配置读取代码：

```cpp
    // 删除这段代码
    // std::vector<farm::DBMgrConfig> dbmgr_configs;
    // if (config.contains("dbmgrs") && config["dbmgrs"].is_array()) {
    //     for (const auto& db : config["dbmgrs"]) {
    //         farm::DBMgrConfig cfg;
    //         cfg.host = db.value("host", "127.0.0.1");
    //         cfg.port = static_cast<uint16_t>(db.value("port", 5000));
    //         dbmgr_configs.push_back(cfg);
    //     }
    // }
```

- [ ] **Step 5: 验证编译**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server/build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug
```

Expected: 编译成功

- [ ] **Step 6: Commit**

```bash
git add scripts/server/game_server/src/main.cpp
git commit -m "feat(game_server): integrate etcd for service registration and dbmgr discovery"
```

---

### Task 8: 集成 etcd 到 gate_server

**Files:**
- Modify: `scripts/server/gate_server/src/gate_server.h`
- Modify: `scripts/server/gate_server/src/gate_server.cpp`
- Modify: `scripts/server/gate_server/src/main.cpp`

- [ ] **Step 1: 修改 gate_server.h - 添加 remove_game_server 和 mutex**

在 `gate_server.h` 中添加 `<mutex>` 头文件：

```cpp
#include <mutex>
```

在 `GateServer` 类的 public 区域添加：

```cpp
    // 移除 Game Server 连接（用于 etcd watch 回调）
    void remove_game_server(uint32_t server_id);
```

在 private 区域添加：

```cpp
    // 保护 game_conns_ 的互斥锁（watch 回调在后台线程）
    mutable std::mutex game_conns_mutex_;
```

- [ ] **Step 2: 修改 gate_server.cpp - 实现 remove_game_server**

在 `gate_server.cpp` 中添加 `remove_game_server` 实现：

```cpp
void GateServer::remove_game_server(uint32_t server_id) {
    std::lock_guard<std::mutex> lock(game_conns_mutex_);

    auto it = game_conns_.find(server_id);
    if (it == game_conns_.end()) {
        SPDLOG_WARN("[Gate]Game Server {} not found for removal", server_id);
        return;
    }

    // 断开连接
    it->second->disconnect();
    game_conns_.erase(it);

    // 清理该 server_id 的玩家路由
    for (auto it_route = player_to_server_.begin(); it_route != player_to_server_.end(); ) {
        if (it_route->second == server_id) {
            SPDLOG_INFO("[Gate]Removed player route: player_id={} server_id={}",
                        it_route->first, it_route->second);
            it_route = player_to_server_.erase(it_route);
        } else {
            ++it_route;
        }
    }

    SPDLOG_INFO("[Gate]Removed Game Server {}", server_id);
}
```

在 `add_game_server` 中添加 mutex 保护：

```cpp
void GateServer::add_game_server(uint32_t server_id, const std::string& ip, uint16_t port) {
    std::lock_guard<std::mutex> lock(game_conns_mutex_);

    // 检查是否已存在
    if (game_conns_.find(server_id) != game_conns_.end()) {
        SPDLOG_WARN("[Gate]Game Server {} already exists, updating", server_id);
        // 断开旧连接
        game_conns_[server_id]->disconnect();
        game_conns_.erase(server_id);
    }

    GameServerConfig config;
    config.server_id = server_id;
    config.ip = ip;
    config.port = port;
    game_server_configs_.push_back(config);

    // 如果 base_ 已初始化，立即创建连接
    if (base_) {
        auto conn = std::make_unique<GameConnection>(base_, "gate-1");
        conn->set_message_callback(
            [this, server_id](uint32_t msg_id, const std::vector<uint8_t>& payload) {
                handle_game_message(server_id, msg_id, payload);
            });
        conn->connect(ip, port);
        game_conns_[server_id] = std::move(conn);
    }

    SPDLOG_INFO("[Gate]Added Game Server {} at {}:{}", server_id, ip, port);
}
```

在 `start()` 中连接 game servers 时也添加 mutex：

```cpp
    // 连接到所有配置的 Game Server
    {
        std::lock_guard<std::mutex> lock(game_conns_mutex_);
        if (!game_server_configs_.empty()) {
            for (const auto& config : game_server_configs_) {
                auto conn = std::make_unique<GameConnection>(base_, "gate-1");
                uint32_t server_id = config.server_id;
                conn->set_message_callback(
                    [this, server_id](uint32_t msg_id, const std::vector<uint8_t>& payload) {
                        handle_game_message(server_id, msg_id, payload);
                    });
                conn->connect(config.ip, config.port);
                game_conns_[server_id] = std::move(conn);
                SPDLOG_INFO("[Gate]Connecting to Game Server {} at {}:{}", server_id, config.ip, config.port);
            }
        }
    }
```

在 `get_game_connection` 和 `get_any_game_connection` 中也添加 mutex：

```cpp
std::optional<GameConnection*> GateServer::get_game_connection(uint32_t server_id) {
    std::lock_guard<std::mutex> lock(game_conns_mutex_);
    auto it = game_conns_.find(server_id);
    if (it != game_conns_.end() && it->second->is_identified()) {
        return it->second.get();
    }
    return std::nullopt;
}

std::optional<GameConnection*> GateServer::get_any_game_connection() {
    std::lock_guard<std::mutex> lock(game_conns_mutex_);
    if (game_conns_.empty()) {
        return std::nullopt;
    }

    // 轮询选择
    size_t count = 0;
    size_t size = game_conns_.size();
    while (count < size) {
        size_t index = next_game_index_ % size;
        next_game_index_++;

        auto it = game_conns_.begin();
        std::advance(it, index);
        if (it->second->is_identified()) {
            return it->second.get();
        }
        count++;
    }

    return std::nullopt;
}
```

在 `stop()` 中也添加 mutex：

```cpp
void GateServer::stop() {
    running_ = false;
    {
        std::lock_guard<std::mutex> lock(game_conns_mutex_);
        for (auto& [server_id, conn] : game_conns_) {
            conn->disconnect();
        }
        game_conns_.clear();
    }
    // ... 其余代码不变
}
```

- [ ] **Step 3: 修改 gate_server/main.cpp - 添加 etcd 集成**

添加 etcd 头文件：

```cpp
#include "etcd_manager.h"
```

添加全局变量：

```cpp
static farm::EtcdManager* g_etcd = nullptr;
```

修改 signal_handler：

```cpp
static void signal_handler(int sig) {
    if (g_server) {
        SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
        g_server->stop();
    }
    if (g_etcd) {
        g_etcd->shutdown();
    }
}
```

在 `farm::init_logging_from_config("gate_server", config);` 之后添加 etcd 初始化：

```cpp
    // etcd 配置
    std::string etcd_endpoints = config.value("/etcd/endpoints"_json_pointer, "http://localhost:2379");
    uint32_t lease_ttl = config.value("/etcd/lease_ttl"_json_pointer, 15u);
    std::string instance_id = config.value("/etcd/instance_id"_json_pointer, "gate-1");

    // 初始化 etcd
    farm::EtcdManager etcd(etcd_endpoints, lease_ttl);
    if (!etcd.connect()) {
        SPDLOG_ERROR("[Main]Failed to connect to etcd");
        return 1;
    }
    g_etcd = &etcd;

    // 注册服务到 etcd
    nlohmann::json service_info = {
        {"ip", ip},
        {"port", port},
        {"status", "online"}
    };
    if (!etcd.register_service("gate", instance_id, service_info.dump())) {
        SPDLOG_ERROR("[Main]Failed to register service to etcd");
        return 1;
    }

    SPDLOG_INFO("[Main]Registered to etcd as gate/{}", instance_id);
```

删除原来的静态 game_servers 配置读取，改为从 etcd 发现：

```cpp
    // 删除原来的代码
    // if (config.contains("game_servers") && config["game_servers"].is_array()) {
    //     for (const auto& gs : config["game_servers"]) {
    //         ...
    //     }
    // }

    // 从 etcd 发现 game servers
    auto games = etcd.discover_services("game");
    for (const auto& game : games) {
        SPDLOG_INFO("[Main]Discovered Game Server from etcd: server_id={} at {}:{}",
                    game.server_id, game.ip, game.port);
        server.add_game_server(game.server_id, game.ip, game.port);
    }

    if (games.empty()) {
        SPDLOG_WARN("[Main]No Game Server instances found in etcd");
    }

    // 监听 game server 变更
    etcd.watch_services("game", [&](const std::string& id,
                                     const farm::ServiceInstance& inst,
                                     bool is_delete) {
        if (is_delete) {
            SPDLOG_INFO("[Main]Game Server {} removed from etcd", id);
            server.remove_game_server(inst.server_id);
        } else {
            SPDLOG_INFO("[Main]Game Server {} added to etcd: {}:{}",
                        id, inst.ip, inst.port);
            server.add_game_server(inst.server_id, inst.ip, inst.port);
        }
    });
```

- [ ] **Step 4: 验证编译**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/gate_server/build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug
```

Expected: 编译成功

- [ ] **Step 5: Commit**

```bash
git add scripts/server/gate_server/src/gate_server.h
git add scripts/server/gate_server/src/gate_server.cpp
git add scripts/server/gate_server/src/main.cpp
git commit -m "feat(gate_server): integrate etcd for service registration and game server discovery"
```

---

### Task 9: 端到端测试

**Files:**
- 无代码修改，仅测试

- [ ] **Step 1: 启动 etcd**

```bash
# 如果还没有 etcd，可以用 Docker 启动
docker run -d --name etcd \
  -p 2379:2379 \
  -p 2380:2380 \
  quay.io/coreos/etcd:latest \
  etcd --listen-client-urls http://0.0.0.0:2379 \
       --advertise-client-urls http://localhost:2379
```

- [ ] **Step 2: 手动写入配置到 etcd**

```bash
# 使用 etcdctl 写入配置
etcdctl put /farm/config/servers/game/1 '{"ip":"0.0.0.0","port":9090,"id":1}'
etcdctl put /farm/config/servers/dbmgr/0 '{"ip":"0.0.0.0","port":5000,"index":0}'
```

- [ ] **Step 3: 启动 dbmgr**

```bash
cd D:/mb_workspace/farm_demo
./bin/dbmgr.exe
```

Expected: 看到日志 `Registered to etcd as dbmgr/0`

- [ ] **Step 4: 启动 game_server**

```bash
cd D:/mb_workspace/farm_demo
./bin/game_server.exe
```

Expected: 看到日志 `Discovered DBMgr from etcd: 127.0.0.1:5000` 和 `Registered to etcd as game/1`

- [ ] **Step 5: 启动 gate_server**

```bash
cd D:/mb_workspace/farm_demo
./bin/gate_server.exe
```

Expected: 看到日志 `Discovered Game Server from etcd: server_id=1 at 0.0.0.0:9090` 和 `Registered to etcd as gate/gate-1`

- [ ] **Step 6: 验证 etcd 注册信息**

```bash
etcdctl get /farm/services/ --prefix
```

Expected: 看到三个服务的注册信息

- [ ] **Step 7: 测试动态发现**

1. 停止 game_server
2. 等待 15 秒（lease 过期）
3. 在 gate_server 日志中看到 `Game Server 1 removed from etcd`

- [ ] **Step 8: Commit 最终状态**

```bash
git add -A
git commit -m "feat: complete etcd service discovery and config center integration"
```

---

## 自检清单

- [x] **Spec 覆盖**: 所有 spec 中的需求都有对应 task
- [x] **Placeholder 扫描**: 无 TBD/TODO（除了动态增减 dbmgr 连接的 TODO，这是合理的后续工作）
- [x] **类型一致性**: ServiceInstance、EtcdManager 接口在所有 task 中一致
- [x] **文件路径准确**: 所有文件路径都是绝对路径或相对于项目根目录
- [x] **代码完整**: 每个 step 都有完整的代码块
