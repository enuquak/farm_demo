---
name: etcd-service-discovery
description: etcd 服务发现集成、Lease 注册、Watch 发现、与 JSON 配置共存策略。
metadata:
  type: reference
---

# etcd 服务发现

## 概述

引入 etcd 作为服务注册中心和配置中心，实现进程动态发现与配置统一管理。与现有 JSON 配置系统共存，etcd 优先、JSON 降级。

## 架构设计

### etcd Key 结构

```
/farm/
├── services/                    # 服务注册（绑定 lease，自动过期）
│   ├── gate/{instance_id}       → {"ip":"0.0.0.0","port":8080,"status":"online"}
│   ├── game/{server_id}         → {"ip":"0.0.0.0","port":9090,"server_id":1}
│   └── dbmgr/{index}            → {"ip":"0.0.0.0","port":5000,"index":0}
│
└── config/                      # 配置中心（持久化，不绑定 lease）
    └── servers/
        ├── gate/{instance_id}
        ├── game/{server_id}
        └── dbmgr/{index}
```

### EtcdManager 接口

```cpp
class EtcdManager {
    bool connect();
    void shutdown();
    bool register_service(service_type, instance_id, value_json);
    void deregister_service(service_type, instance_id);
    std::vector<ServiceInstance> discover_services(service_type);
    void watch_services(service_type, callback);
    bool put_config(key, value_json);
    std::optional<std::string> get_config(key);
    void watch_config(key_prefix, callback);
};
```

## 关键流程

### 服务注册流程

1. 进程启动，连接 etcd
2. 创建 Lease（TTL 15 秒）
3. 将自身信息写入 `/farm/services/{type}/{id}`，绑定 Lease
4. 启动定期续约（每 10 秒续约一次）
5. 进程关闭时主动删除 key

### 服务发现流程

1. 启动时 Watch `/farm/services/{type}/` 前缀
2. 收到 PUT 事件：添加/更新服务实例
3. 收到 DELETE 事件：移除服务实例
4. 本地维护服务实例列表缓存

### 降级策略

1. 启动时尝试连接 etcd
2. 连接失败：使用 JSON 配置文件中的静态配置
3. 运行时 etcd 断开：继续使用本地缓存，定期尝试重连

## 关键代码路径

- etcd 客户端封装：`scripts/server/common/include/etcd_manager.h`
- 服务注册：各服务的 `main.cpp` 启动流程
- 配置：`config/*.json` 中的 etcd 配置段

## 常见陷阱

### Lease 过期

服务注册信息被自动删除：
- 确认定约线程正常运行
- 检查网络连通性
- TTL 不要设置过短

### Watch 断开重连

Watch 连接断开后丢失事件：
- 实现 Watch 的自动重连机制
- 重连后重新获取全量数据作为基线

### etcd 集群不可用

etcd 完全不可用时的处理：
- 启动时降级到 JSON 配置
- 运行时使用本地缓存
- 记录错误日志，定期重试

## 扩展指南

### 添加新的服务类型

1. 定义新的 service_type 常量
2. 在对应服务的 main.cpp 中添加注册逻辑
3. 在需要发现该服务的进程中添加 Watch 逻辑

## 相关 Skill

- [[server-architecture]] — 服务器架构与配置系统
