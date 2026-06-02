# C++17 升级设计文档

**日期**: 2026-05-31
**状态**: ✅ 已完成 (2026-06-02)
**范围**: 全部代码（服务器端 C++ + 通用库）

---

## 1. 背景

项目早期使用 C++14，现已决定升级到 C++17。CMakeLists.txt 已设置 `CMAKE_CXX_STANDARD 17`，部分代码已使用 C++17 特性（如 `std::string_view`、`inline constexpr`）。本次升级旨在系统性地应用 C++17 特性，提升代码可读性和安全性。

## 2. 目标

- **主要目标**：提升代码可读性
- **次要目标**：提升代码安全性、小幅性能优化
- **兼容性保证**：编译通过 + 测试通过（除非严重性能回退）

## 3. 优化内容

### 3.1 结构化绑定（Structured Bindings）

**目标**：将所有使用 `kv.first`/`kv.second` 的循环改为结构化绑定

**影响范围**：

| 文件 | 当前代码 | 改进后 |
|------|----------|--------|
| `crop_system.cpp:127` | `for (const auto& kv : growing_tiles_)` + `kv.first/kv.second` | `for (const auto& [key, crop] : growing_tiles_)` |
| `dbmgr_connection_manager.cpp:77` | `for (auto& kv : pending_requests_)` + `kv.first/kv.second` | `for (auto& [id, request] : pending_requests_)` |
| `drop_item_manager.cpp:64` | `for (const auto& kv : drops_)` + `kv.first/kv.second` | `for (const auto& [id, drop] : drops_)` |
| `game_server.cpp:305` | `for (auto& kv : gate_sessions_)` + `kv.first/kv.second` | `for (auto& [fd, session] : gate_sessions_)` |
| `player_manager.cpp:117` | `for (auto& kv : players_)` + `kv.first/kv.second` | `for (auto& [id, player] : players_)` |
| `session_manager.cpp:48` | `for (auto& kv : sessions_)` + `kv.first/kv.second` | `for (auto& [fd, session] : sessions_)` |
| `gate_server.cpp:109` | `for (auto& kv : game_conns_)` + `kv.first/kv.second` | `for (auto& [id, conn] : game_conns_)` |

**改动原则**：
- 变量名应具有语义（如 `id`、`session`、`drop`），而不是 `key`/`value`
- 保持 `const` 修饰符与原代码一致
- 不改变任何业务逻辑

### 3.2 std::optional（安全返回值）

**目标**：将返回 `nullptr` 的函数改为返回 `std::optional`，明确表达"可能无值"的语义

**影响范围**：

| 文件 | 函数 | 当前返回类型 | 改进后 |
|------|------|--------------|--------|
| `player_manager.h:39` | `get_player()` | `Player*`（返回 nullptr） | `std::optional<Player*>` |
| `session_manager.h` | `get_session()` | `Session*`（返回 nullptr） | `std::optional<Session*>` |
| `gate_server.cpp:141` | `get_game_connection()` | `GameConnection*`（返回 nullptr） | `std::optional<GameConnection*>` |
| `drop_item_manager.h:84` | `get()` | `const DropItem*`（返回 nullptr） | `std::optional<const DropItem*>` |
| `game_scene_manager.cpp:32` | `get_scene()` | `SceneState*`（返回 nullptr） | `std::optional<SceneState*>` |
| `item_interaction_handler.h:56` | `get_active_slot()` | `InventorySlot*`（返回 nullptr） | `std::optional<InventorySlot*>` |

**改动原则**：
- 调用处从 `if (ptr)` 改为 `if (opt.has_value())` 或 `if (opt)`
- 使用 `opt.value()` 或 `*opt` 获取值
- 保持函数签名的向后兼容性（逐步迁移）

### 3.3 std::string_view（性能优化）

**目标**：将只读的 `const std::string&` 参数改为 `std::string_view`，减少不必要的字符串拷贝

**影响范围**：

| 文件 | 函数 | 当前参数 | 改进后 |
|------|------|----------|--------|
| `data_manager.h:36` | `DataManager()` | `const std::string& data_dir` | `std::string_view data_dir` |
| `data_manager.h:46` | `get()` | `const std::string& key` | `std::string_view key` |
| `data_manager.h:52` | `set()` | `const std::string& key` | `std::string_view key` |
| `data_manager.h:55` | `del()` | `const std::string& key` | `std::string_view key` |
| `data_manager.h:59` | `get_account()` | `const std::string& account_id` | `std::string_view account_id` |
| `data_manager.h:62` | `set_account()` | `const std::string& account_id` | `std::string_view account_id` |
| `message_parser.h:33` | `pack()` | `const std::string& payload` | `std::string_view payload` |
| `player.h:62` | `set_role_name()` | `const std::string& role_name` | `std::string_view role_name` |
| `player.h:83` | `set_scene_id()` | `const std::string& scene_id` | `std::string_view scene_id` |
| `player.h:86` | `set_inventory()` | `const std::string& inventory` | `std::string_view inventory` |
| `player.h:91` | `set_farm_state()` | `const std::string& farm_state` | `std::string_view farm_state` |
| `player.h:94` | `set_extra_data()` | `const std::string& extra_data` | `std::string_view extra_data` |

**改动原则**：
- 只修改**只读**参数（不修改输出参数或需要存储的参数）
- 如果函数内部需要存储字符串，保持 `const std::string&` 或使用 `std::string`
- 在需要 `std::string` 的地方使用 `std::string(sv)` 转换

**注意事项**：
- `std::string_view` 不保证 null 结尾，与 C 字符串交互时需谨慎
- 如果函数内部调用需要 `const char*` 的 C API，需要额外处理

## 4. 实施顺序

| 阶段 | 模块 | 改动内容 | 风险 |
|------|------|----------|------|
| **阶段 1** | common | 结构化绑定 + std::string_view | 低 |
| **阶段 2** | dbmgr | 结构化绑定 + std::optional + std::string_view | 低 |
| **阶段 3** | gate_server | 结构化绑定 + std::optional + std::string_view | 中 |
| **阶段 4** | game_server | 结构化绑定 + std::optional + std::string_view | 中 |

**每个阶段的验证步骤**：
1. 修改代码
2. 编译验证（`cmake --build`）
3. 运行测试（如果有）
4. 功能验证（启动服务，检查日志）

**依赖关系**：
- common 模块被其他三个模块依赖，必须先改
- dbmgr、gate_server、game_server 相对独立，可并行或按顺序改

## 5. 不改动的内容

| 内容 | 原因 |
|------|------|
| `#ifdef _WIN32` 块 | 平台特定代码，用 `if constexpr` 替代需要重构逻辑，收益不大 |
| `std::filesystem` | 当前文件操作逻辑简单，改动收益不大 |
| Python 客户端代码 | Python 不涉及 C++17 升级 |
| protobuf 生成的代码 | 第三方生成，不应手动修改 |

## 6. 预期收益

- **可读性显著提升**：结构化绑定让代码更易读，变量名直接表达含义
- **安全性提升**：std::optional 强制处理空值情况，减少空指针解引用风险
- **性能小幅提升**：std::string_view 减少临时字符串的创建

## 7. 风险控制

- 增量式改动，每阶段验证
- 不改变业务逻辑
- 保持向后兼容
