# DLL 热更新系统设计

## 概述

为 farm_demo 游戏服务器实现基于 DLL 动态链接库的热更新系统，支持线上不停服更新业务逻辑。

### 设计目标

- **全面热更**：所有业务子系统均可通过 DLL 独立更新
- **函数级粒度**：每个消息处理函数可独立热替换
- **双版本并行**：新旧版本 DLL 同时运行，新请求走新逻辑，旧请求走旧逻辑
- **分布式通知**：通过 etcd 通知所有服务器实例加载新 DLL
- **本地文件加载**：DLL 文件从本地 plugins/ 目录加载
- **自动回滚**：加载失败或运行时崩溃自动回退到上一版本

---

## 整体架构

```
┌─────────────────────────────────────────────────┐
│                 game_server.exe                  │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐   │
│  │ Network   │  │ Event     │  │ DB/Redis  │   │
│  │ (libevent)│  │ Loop      │  │ Connections│  │
│  └─────┬─────┘  └─────┬─────┘  └─────┬─────┘   │
│        │              │              │          │
│  ┌─────▼──────────────▼──────────────▼─────┐    │
│  │          Plugin Manager                  │    │
│  │  ┌─────────────┐  ┌─────────────────┐   │    │
│  │  │ DLL Loader  │  │ Function Registry│   │    │
│  │  │ (版本管理)   │  │ (msg_id→fn映射) │   │    │
│  │  └──────┬──────┘  └────────┬────────┘   │    │
│  └─────────┼──────────────────┼────────────┘    │
│            │                  │                 │
│  ┌─────────▼──────────────────▼────────────┐    │
│  │              Plugin DLLs                 │    │
│  │  quest_v2.dll  combat_v1.dll  ...       │    │
│  └─────────────────────────────────────────┘    │
└─────────────────────────────────────────────────┘
```

### 核心原则

- **exe 保持稳定**：网络层、事件循环、数据库连接、核心状态管理留在 exe，不热更
- **DLL 只含逻辑**：消息处理函数、业务规则、配置解析等纯逻辑放入 DLL
- **通过接口通信**：DLL 通过 PluginContext 访问 exe 提供的服务（玩家数据、DB、日志等）

---

## DLL 接口（ABI）

### DLL 导出接口

每个 DLL 必须导出以下标准接口：

```cpp
// === 元信息 ===
extern "C" PLUGIN_API const char* dll_get_version();      // "1.0.3"
extern "C" PLUGIN_API const char* dll_get_name();         // "quest_system"
extern "C" PLUGIN_API uint32_t dll_get_abi_version();     // 1

// === 生命周期 ===
extern "C" PLUGIN_API bool dll_init(PluginContext* ctx);  // 初始化
extern "C" PLUGIN_API void dll_shutdown();                 // 清理资源

// === 函数注册 ===
extern "C" PLUGIN_API FunctionEntry* dll_get_functions(size_t* count);
// 返回数组: [{msg_id, fn_ptr, fn_name}, ...]

// === 事件订阅（可选）===
extern "C" PLUGIN_API EventSubscription* dll_get_subscriptions(size_t* count);
// 返回数组: [{event_type, fn_ptr}, ...]

// === 健康检查 ===
extern "C" PLUGIN_API bool dll_health_check();  // 返回 true 表示 DLL 状态正常
```

### 数据结构

```cpp
struct FunctionEntry {
    uint32_t  msg_id;       // 消息 ID
    HandlerFn fn_ptr;       // 函数指针
    const char* fn_name;    // 函数名（调试用）
};

struct EventSubscription {
    uint32_t  event_type;   // EventType 枚举值
    EventHandler fn_ptr;    // 事件处理函数
};

using HandlerFn = void(*)(PluginContext* ctx, uint64_t player_id,
                          const uint8_t* payload, size_t len);
```

### PluginContext — exe 提供给 DLL 的服务接口

```cpp
class PluginContext {
public:
    // 玩家数据访问
    virtual PlayerData* get_player(uint64_t player_id) = 0;
    virtual void release_player(PlayerData* player) = 0;

    // 数据库操作
    virtual void db_query(const char* collection, const char* query,
                          DBCallback callback) = 0;
    virtual void db_save(const char* collection, const char* data) = 0;

    // Redis 操作
    virtual void redis_get(const char* key, RedisCallback callback) = 0;
    virtual void redis_set(const char* key, const char* value, int ttl) = 0;

    // 消息发送
    virtual void send_to_client(uint64_t player_id, uint32_t msg_id,
                                const uint8_t* data, size_t len) = 0;
    virtual void send_to_service(const char* service, uint32_t msg_id,
                                 const uint8_t* data, size_t len) = 0;

    // 日志
    virtual void log_info(const char* fmt, ...) = 0;
    virtual void log_error(const char* fmt, ...) = 0;

    // 内部消息（跨 DLL 通信）
    virtual void emit_internal(uint32_t internal_id, const char* data) = 0;
};
```

### DLL 间通信

DLL 之间不直接调用，统一通过 PluginContext 的 `emit_internal()` 机制：

```cpp
// quest_system.dll 需要通知 combat_system.dll
ctx->emit_internal(INTERNAL_QUEST_COMBAT_START, R"({"quest_id":123})");

// combat_system.dll 在 dll_init() 时订阅该内部消息
```

内部消息 ID 定义在 `scripts/server/common/include/internal_msg_ids.h`，与 DLL 共享。

### 共享类型

DLL 与 exe 共享以下类型（通过头文件，不通过 DLL 导出）：
- Protobuf 消息定义（`scripts/common/proto/generated/*.pb.h`）
- 通用数据结构（`PlayerData`、`GameEvent` 等）
- 错误码和消息 ID（`error_codes.h`、`message_ids.h`）

这些类型由 exe 和 DLL 使用相同的头文件编译，保证内存布局一致。

### ABI 兼容性规则

- `dll_get_abi_version()` 返回值必须与 exe 期望的版本匹配
- PluginContext 的虚函数表顺序不得改变
- 只能追加新的虚函数（放在末尾），不能删除或重排已有函数
- HandlerFn 签名不得改变

---

## 热替换机制

### 目录结构

```
plugins/
├── quest/
│   ├── quest_v1.dll          ← 当前稳定版本
│   ├── quest_v2.dll          ← 新版本（加载中）
│   └── quest_v1.dll.bak      ← 备份
├── combat/
│   └── combat_v1.dll
└── _manifest.json            ← 版本清单 + 状态
```

### 热替换流程

```
1. 收到 etcd 通知 → 检测 plugins/ 目录发现新 DLL
2. LoadLibrary(quest_v2.dll) → dll_init()
3. 校验 ABI 版本兼容性
4. dll_get_functions() → 获取新函数列表
5. FunctionRegistry 更新映射:
   - 新请求 → 路由到 quest_v2 的函数
   - 正在执行的请求 → 继续用 quest_v1 的函数
6. 等待 quest_v1 的所有活跃调用完成（引用计数归零）
7. FreeLibrary(quest_v1.dll)
8. 更新 manifest，quest_v2 成为稳定版本
```

### 双版本并行机制

```cpp
struct FunctionSlot {
    HandlerFn        fn_ptr;          // 当前函数指针
    uint32_t         dll_version;     // 所属 DLL 版本
    std::atomic<int> active_calls;    // 活跃调用引用计数
    HMODULE          dll_handle;      // DLL 句柄
};

// 消息分发时：
bool dispatch(uint32_t msg_id, uint64_t player_id, ...) {
    auto& slot = registry_[msg_id];
    slot.active_calls++;          // 原子递增
    slot.fn_ptr(ctx, player_id, payload, len);  // 调用
    slot.active_calls--;          // 原子递减
    return true;
}

// 热替换时：
void swap_function(uint32_t msg_id, HandlerFn new_fn, HMODULE new_dll) {
    auto& slot = registry_[msg_id];
    // 新请求立即走新函数
    slot.fn_ptr = new_fn;
    slot.dll_version = new_version;
    // 旧 DLL 在 active_calls 归零后才卸载
}
```

---

## 分布式通知与部署

### etcd 通知机制

```
部署工具 (tools/deploy_plugin.py)
    │
    ├── 1. 编译新 DLL → plugins/quest/quest_v2.dll
    ├── 2. 上传到各服务器实例的 plugins/ 目录
    └── 3. 写入 etcd: /farm/plugins/quest → {"version":"v2","checksum":"abc123"}
                                              │
                                              ▼
各 game_server 实例 (etcd watcher)
    ├── 检测到 /farm/plugins/quest 变化
    ├── 对比本地版本，发现新版本
    ├── 校验 checksum
    └── 触发热替换流程
```

### 部署流程

```bash
# 开发者操作：
1. 修改 quest 相关代码
2. tools/build_plugin.py quest      # 只编译 quest DLL
3. tools/deploy_plugin.py quest     # 分发 + 触发热更

# 自动执行：
- 编译 quest_system.dll → quest_v2.dll
- 通过 SSH/SCP 分发到所有 game_server 实例
- 写入 etcd 触发热更
- 等待所有实例确认加载成功
- 如有失败，自动回滚
```

### 版本清单 _manifest.json

```json
{
  "quest": {
    "current": "v2",
    "previous": "v1",
    "status": "active",
    "loaded_at": "2026-06-03T10:30:00Z",
    "checksum": "sha256:abc123..."
  },
  "combat": {
    "current": "v1",
    "previous": null,
    "status": "active",
    "loaded_at": "2026-06-02T08:00:00Z",
    "checksum": "sha256:def456..."
  }
}
```

---

## 错误处理与回滚

### 崩溃隔离

```cpp
bool safe_dispatch(HandlerFn fn, PluginContext* ctx, uint64_t player_id,
                   const uint8_t* payload, size_t len) {
    __try {
        fn(ctx, player_id, payload, len);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        log_error("DLL crash in handler: code=%d", GetExceptionCode());
        mark_dll_broken(current_dll);
        rollback_to_previous(msg_id);
        send_error_to_client(player_id, ERROR_SERVICE_UNAVAILABLE);
        return false;
    }
}
```

### 自动回滚触发条件

1. **加载失败**：dll_init() 返回 false → 保留旧版本
2. **ABI 不兼容**：dll_get_abi_version() 不匹配 → 拒绝加载
3. **运行时崩溃**：SEH 捕获异常 → 恢复旧 DLL，标记新版本为 broken
4. **健康检查失败**：dll_health_check() 返回 false → 触发回滚

### 回滚策略

- 保留最近 2 个版本的 DLL 文件
- 回滚时将 previous 版本恢复为 current
- 通过 etcd 通知所有实例回滚
- 记录回滚原因到日志和 manifest

---

## 模块拆分

### DLL 模块清单

| DLL 模块 | 包含的源文件 | 热更频率 |
|----------|-------------|---------|
| quest_system.dll | quest_types, quest_config, quest_manager | 高 |
| combat_system.dll | combat_handler, monster_manager | 中 |
| item_system.dll | item_effects, item_interaction_handler, drop_item_manager | 中 |
| crop_system.dll | crop_system | 低 |
| player_system.dll | player, player_manager, login_stub, online_stub | 中 |
| scene_system.dll | scene_state, game_scene_manager, world_state | 低 |
| gm_system.dll | gm_stub, gm_http_handler, admin_handler | 高 |
| friend_bridge.dll | friend_service_connection | 低 |
| event_bridge.dll | event_bus（DLL 版本，桥接 exe 的 EventBus） | 低 |

### 核心留在 exe 的部分（不热更）

- game_server.cpp — 主事件循环
- gate_session.cpp — 网络会话管理
- message_handler.cpp — 消息路由框架
- dbmgr_connection* — 数据库连接管理
- redis_connection.cpp — Redis 连接
- game_clock.cpp — 游戏时钟

### 边界原则

- exe 负责**基础设施**（网络、连接、调度）
- DLL 负责**业务逻辑**（处理消息、执行游戏规则）
- DLL 通过 PluginContext 访问 exe 的服务，不直接操作底层资源
- DLL 可以管理自己的内部状态（如 quest_manager 的任务配置），但玩家共享数据必须通过 PluginContext 访问

---

## CMake 构建变更

### 新增插件构建系统

每个 DLL 模块需要独立的 `CMakeLists.txt`，使用 `add_library(SHARED)` 构建：

```cmake
# scripts/server/plugins/quest_system/CMakeLists.txt
add_library(quest_system SHARED
    quest_types.cpp
    quest_config.cpp
    quest_manager.cpp
    dll_main.cpp  # DLL 入口点 + 导出函数实现
)
target_link_libraries(quest_system PRIVATE farm_plugin_common)
```

### 公共插件库

新增 `farm_plugin_common` 静态库，包含：
- PluginContext 接口定义
- 共享类型头文件
- ABI 版本常量
- DLL 辅助宏（PLUGIN_API 定义等）

### 构建命令

```bash
# 构建所有插件
tools/build_plugins.bat

# 构建单个插件
tools/build_plugin.py quest_system
```

---

## 测试策略

- **ABI 兼容性测试**：编译时静态断言 + 运行时版本校验
- **DLL 加载测试**：模拟加载/卸载/重载循环
- **崩溃隔离测试**：故意在 DLL 中触发崩溃，验证 exe 不受影响
- **双版本并行测试**：同时加载两个版本，验证路由正确性
- **性能测试**：对比 DLL 调用 vs 直接调用的开销

---

## 实施阶段

| 阶段 | 内容 | 预计工作量 |
|------|------|-----------|
| Phase 1 | PluginManager + FunctionRegistry 框架 | 2-3 天 |
| Phase 2 | PluginContext 实现（桥接 exe 服务） | 2-3 天 |
| Phase 3 | 第一个 DLL 模块迁移（gm_system，最简单） | 1-2 天 |
| Phase 4 | 热替换机制 + 双版本并行 | 2-3 天 |
| Phase 5 | etcd 通知 + 部署工具 | 1-2 天 |
| Phase 6 | 迁移剩余模块（quest, combat, item 等） | 3-5 天 |
| Phase 7 | 自动回滚 + 监控 + 压力测试 | 2-3 天 |

**总计约 13-21 天**
