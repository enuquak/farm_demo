# etcd 服务注册/发现与配置中心 设计文档

## 背景

当前服务器进程间发现机制是**静态配置**：每个进程从本地 JSON 配置文件中硬编码其他进程的 IP/端口。这导致：

- 增减节点需要修改配置文件并重启所有相关进程
- 无法动态感知其他进程的上下线（除了已有的心跳机制）
- 配置分散在各进程的 JSON 文件中，难以统一管理

目标是引入 etcd 作为服务注册中心和配置中心，实现进程的动态发现与配置统一管理。

## 需求决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 改造目标 | 服务注册/发现 + 配置中心 | 最完整的方案 |
| 客户端库 | etcd-cpp-apiv3 | 官方 C++ 客户端，与项目 protobuf 生态兼容 |
| 注册范围 | 全部进程（gate、game、dbmgr） | 统一管理 |
| 存活检测 | Lease 租约机制（TTL 15s） | 进程崩溃后自动清理注册信息 |
| 发现方式 | Watch 监听 | 实时感知节点变更 |
| 配置范围 | 服务器基础配置（地址、端口、server_id） | 先覆盖核心配置 |
| 兼容策略 | 硬依赖 etcd | 简化实现，不引入降级逻辑 |

## etcd Key 结构

所有 key 统一前缀 `/farm/`，分为两个命名空间：

```
/farm/
├── services/                    # 服务注册（绑定 lease，自动过期）
│   ├── gate/
│   │   └── {instance_id}       → {"ip":"0.0.0.0","port":8080,"status":"online"}
│   ├── game/
│   │   └── {server_id}         → {"ip":"0.0.0.0","port":9090,"server_id":1}
│   └── dbmgr/
│       └── {index}             → {"ip":"0.0.0.0","port":5000,"index":0}
│
└── config/                      # 配置中心（持久化，不绑定 lease）
    └── servers/
        ├── gate/{instance_id}   → {"ip":"0.0.0.0","port":8080}
        ├── game/{server_id}     → {"ip":"0.0.0.0","port":9090,"id":1}
        └── dbmgr/{index}        → {"ip":"0.0.0.0","port":5000,"index":0}
```

- `services/` 下的 key 绑定 lease，进程崩溃后 lease 过期自动删除
- `config/` 下的 key 是持久化的，由运维或管理工具写入
- 进程启动时：先从 `config/` 读取自己的配置，再向 `services/` 注册自己
- 进程发现其他服务时：watch `services/` 下对应目录

## EtcdManager 类设计

### 接口定义

```cpp
// scripts/server/common/include/etcd_manager.h

struct ServiceInstance {
    std::string instance_id;
    std::string ip;
    uint16_t port;
    uint32_t server_id;  // 仅 game_server 使用
    uint32_t index;      // 仅 dbmgr 使用
};

class EtcdManager {
public:
    EtcdManager(const std::string& etcd_endpoints, uint32_t lease_ttl = 15);
    ~EtcdManager();

    // === 生命周期 ===
    bool connect();                          // 连接 etcd，失败返回 false
    void shutdown();                         // 主动注销 + 关闭连接

    // === 错误回调 ===
    using ErrorCallback = std::function<void(const std::string& error_msg)>;
    void set_error_callback(ErrorCallback callback);

    // === 服务注册（带 lease）===
    bool register_service(const std::string& service_type,   // "gate"/"game"/"dbmgr"
                          const std::string& instance_id,    // 唯一标识（见下方说明）
                          const std::string& value_json);    // 注册信息 JSON

    void deregister_service(const std::string& service_type,
                            const std::string& instance_id);

    // === 服务发现 ===
    // 一次性查询当前在线的所有实例
    std::vector<ServiceInstance> discover_services(const std::string& service_type);

    // 持续监听变更，回调触发
    using ServiceChangeCallback = std::function<void(const std::string& instance_id,
                                                     const ServiceInstance& instance,
                                                     bool is_delete)>;
    void watch_services(const std::string& service_type,
                        ServiceChangeCallback callback);

    // === 配置中心（无 lease，持久化）===
    bool put_config(const std::string& key, const std::string& value_json);
    std::optional<std::string> get_config(const std::string& key);

    using ConfigChangeCallback = std::function<void(const std::string& key,
                                                    const std::string& value_json)>;
    void watch_config(const std::string& key_prefix, ConfigChangeCallback callback);

private:
    void lease_keepalive_loop();  // 后台线程：定期续租

    etcd::Client* client_;
    std::string endpoints_;
    uint32_t lease_ttl_;
    int64_t lease_id_;

    std::thread keepalive_thread_;
    std::atomic<bool> running_;

    std::unique_ptr<etcd::Watcher> service_watcher_;
    std::unique_ptr<etcd::Watcher> config_watcher_;

    ErrorCallback error_callback_;
};
```

### instance_id 命名规则

- gate_server: 使用本地配置中的 `etcd.instance_id` 字段（如 `"gate-1"`），需在配置文件中显式指定
- game_server: 使用 `server_id` 的字符串形式（如 `"1"`）
- dbmgr: 使用 `index` 的字符串形式（如 `"0"`）

### 关键行为

- `connect()` 时创建 lease，启动续租后台线程
- `register_service()` 用 `put(key, value, lease_id)` 写入，绑定 lease
- `watch_services()` 用 etcd Watch API 监听 `/farm/services/{type}/` 前缀
- `shutdown()` 主动删除注册 key，停止续租线程

## 各进程集成方式

### 启动流程变化

```
原流程：读取本地 JSON 配置 → 连接其他服务
新流程：读取本地 JSON（仅 etcd 地址）→ 连接 etcd → 从 etcd 读取配置 → 注册自己 → 发现其他服务 → 连接
```

### gate_server

```cpp
// main.cpp 变化
std::string etcd_endpoints = config.value("etcd/endpoints", "http://localhost:2379");

EtcdManager etcd(etcd_endpoints);
if (!etcd.connect()) return 1;

// 从 etcd 读取自己的配置
auto my_config = etcd.get_config("servers/gate/gate-1");

// 注册自己
etcd.register_service("gate", "gate-1", R"({"ip":"0.0.0.0","port":8080})");

// 发现 game servers（替代原来从 JSON 读取 game_servers[]）
auto games = etcd.discover_services("game");
for (auto& g : games) {
    server.add_game_server(g.server_id, g.ip, g.port);
}

// 监听 game server 变更（动态增减）
etcd.watch_services("game", [&](auto& id, auto& inst, bool is_delete) {
    if (is_delete) server.remove_game_server(inst.server_id);
    else server.add_game_server(inst.server_id, inst.ip, inst.port);
});
```

### game_server

```cpp
etcd.register_service("game", std::to_string(server_id), ...);

auto dbmgrs = etcd.discover_services("dbmgr");
// 转为 DBMgrConfig 传给 DBMgrConnectionManager

etcd.watch_services("dbmgr", [&](auto& id, auto& inst, bool is_delete) {
    // 动态更新 dbmgr 连接
});
```

### dbmgr

```cpp
// 最简单：只注册自己，不需要发现其他服务
etcd.register_service("dbmgr", "0", R"({"ip":"0.0.0.0","port":5000,"index":0})");
```

### 本地配置文件简化

```json
// config/gate_server.json（改造后）
{
    "etcd": {
        "endpoints": "http://localhost:2379",
        "lease_ttl": 15,
        "instance_id": "gate-1"
    },
    "logging": { ... }
}

// config/game_server.json（改造后）
{
    "etcd": {
        "endpoints": "http://localhost:2379",
        "lease_ttl": 15
    },
    "server": { "id": 1 },
    "logging": { ... }
}

// config/dbmgr.json（改造后）
{
    "etcd": {
        "endpoints": "http://localhost:2379",
        "lease_ttl": 15
    },
    "server": { "index": 0 },
    "logging": { ... }
}
```

原来硬编码的 `game_servers[]`、`dbmgrs[]` 全部删除，由 etcd 动态提供。

## 错误处理与边界情况

### etcd 连接失败

- `connect()` 返回 false → 进程打印错误日志后退出（硬依赖）
- 续租失败 → 尝试重连 3 次，仍失败则进程主动退出

### 注册信息残留

- 正常退出：`shutdown()` 主动删除 key
- 崩溃退出：lease 过期（15 秒后）自动清除，其他进程通过 watch 感知下线

### Watch 断开

- etcd-cpp-apiv3 的 Watcher 内部有重连机制
- 如果 Watch 永久断开，EtcdManager 回调 `on_error` 通知上层
- 上层可选择使用上次缓存的服务列表或退出

### 动态增减 Game Server

gate_server 的 `add_game_server` / `remove_game_server` 需要处理：
- 新增：创建 GameConnection 并发起连接
- 移除：断开连接，清理该 server_id 的玩家路由
- 需要用 `std::mutex` 保护 `game_conns_` map（watch 回调在 etcd 后台线程，主事件循环在 libevent 线程）

### 配置变更

- `config/` 下的配置变更通过 watch 通知
- 当前设计只读取启动时的配置，运行时配置变更暂不处理

## 依赖与构建变更

### 新增依赖

| 依赖 | 用途 | 安装方式 |
|------|------|----------|
| etcd-cpp-apiv3 | etcd 官方 C++ 客户端 | vcpkg / 手动编译安装 |
| grpc / cpprestsdk | etcd-cpp-apiv3 的依赖 | 随 etcd-cpp-apiv3 一起安装 |

### CMakeLists.txt 变更

三个进程的 CMakeLists 都需要：
- 新增 `ETCD_ROOT` 路径
- include 目录加入 etcd 头文件
- 链接 etcd-cpp-apiv3 库
- common 模块新增 `etcd_manager.cpp`

### 文件变更清单

```
新增：
  scripts/server/common/include/etcd_manager.h
  scripts/server/common/src/etcd_manager.cpp

修改：
  scripts/server/gate_server/src/main.cpp        ← etcd 集成
  scripts/server/gate_server/src/gate_server.h    ← 新增 remove_game_server
  scripts/server/gate_server/src/gate_server.cpp  ← 动态增减支持 + 线程安全
  scripts/server/gate_server/CMakeLists.txt       ← 链接 etcd
  scripts/server/game_server/src/main.cpp         ← etcd 集成
  scripts/server/game_server/CMakeLists.txt       ← 链接 etcd
  scripts/server/dbmgr/src/main.cpp               ← etcd 集成
  scripts/server/dbmgr/CMakeLists.txt             ← 链接 etcd
  config/gate_server.json                         ← 简化，只留 etcd 地址
  config/game_server.json                         ← 简化，只留 etcd 地址
  config/dbmgr.json                               ← 简化，只留 etcd 地址
```
