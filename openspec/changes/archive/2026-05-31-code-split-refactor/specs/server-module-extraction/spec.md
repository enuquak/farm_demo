# 服务器端模块提取规格

## 概述

从 `GameServer`（game_server.cpp 1645 行）提取 4 个独立子系统，使 GameServer 缩减为纯基础设施编排器。

## 模块 1：GameSceneManager

### 新文件
- `scripts/server/game_server/src/game_scene_manager.h`
- `scripts/server/game_server/src/game_scene_manager.cpp`

### 职责
- 场景创建与查找（get_or_create_scene）
- 场景切换处理（handle_scene_change_req）
- 场景数据持久化（save_all_scenes, save_scene_data, load_scene_data）

### 移入内容
**成员变量**：
- `scenes_`：`std::unordered_map<std::string, std::unique_ptr<SceneState>>`

**方法**（从 game_server.cpp 移入）：
- `get_or_create_scene(const std::string& scene_id)` → 查找或创建场景
- `handle_scene_change_req(uint64_t player_id, payload, payload_len)` → 处理场景切换请求
- `save_all()` → 保存所有场景
- `save_data(const std::string& scene_id)` → 保存单个场景
- `load_data(const std::string& scene_id)` → 加载场景数据
- `make_scene_data_key(const std::string& scene_id)` → 生成持久化 key

### 依赖
```cpp
class GameSceneManager {
public:
    GameSceneManager(DBMgrConnectionManager* dbmgr_mgr);
    // ... 方法

    using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;
    void set_send_callback(SendGameMsgFunc func);

private:
    DBMgrConnectionManager* dbmgr_mgr_;
    SendGameMsgFunc send_func_;
    std::unordered_map<std::string, std::unique_ptr<SceneState>> scenes_;
};
```

---

## 模块 2：GameClock

### 新文件
- `scripts/server/game_server/src/game_clock.h`
- `scripts/server/game_server/src/game_clock.cpp`

### 职责
- 游戏时钟推进（update）
- 时钟同步广播（broadcast_sync）
- 日结束处理（on_day_end）
- 强制睡眠流程（handle_force_sleep_ready, complete_force_sleep）
- 时钟数据持久化（save_data, load_data）

### 移入内容
**成员变量**：
- `clock_day_`：int32_t（当前天数）
- `clock_time_slot_`：int32_t（当前时间槽）
- `clock_elapsed_`：double（已过秒数）
- `clock_paused_`：bool（是否暂停）
- `force_sleep_pending_`：bool（是否等待强制睡眠）
- `force_sleep_timeout_counter_`：int（超时计数）

**方法**（从 game_server.cpp 移入）：
- `update()` → 每秒调用，推进时钟
- `broadcast_sync()` → 广播 ClockSync 消息
- `on_day_end()` → 日结束处理（暂停时钟、发送 ForceSleepNotify）
- `handle_force_sleep_ready(player_id, payload, payload_len)` → 处理客户端强制睡眠就绪
- `save_data()` → 持久化时钟状态
- `load_data()` → 加载时钟状态

### 依赖
```cpp
class GameClock {
public:
    GameClock(PlayerManager* player_mgr, GameSceneManager* scene_mgr,
              DBMgrConnectionManager* dbmgr_mgr);
    // ... 方法

    using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;
    void set_send_callback(SendGameMsgFunc func);

private:
    void complete_force_sleep(Player* player);  // 跨模块协调
    PlayerManager* player_mgr_;
    GameSceneManager* scene_mgr_;
    DBMgrConnectionManager* dbmgr_mgr_;
    SendGameMsgFunc send_func_;
    // ... 6 个时钟成员变量
};
```

### 关键设计：complete_force_sleep 跨模块协调
`complete_force_sleep` 是最跨切面的方法，需要：
1. 通过 `scene_mgr_` 冻结当前场景、切换到 house 场景
2. 通过 `player_mgr_` 修改玩家位置和体力
3. 推进时钟状态
4. 通过 `send_func_` 发送 SceneChangeResp、ClockSync、EnergySync

这通过注入的 `GameSceneManager*` 和 `PlayerManager*` 指针实现，无需反向依赖 GameServer。

---

## 模块 3：AccountMessageHandler

### 新文件
- `scripts/server/game_server/src/account_message_handler.h`
- `scripts/server/game_server/src/account_message_handler.cpp`

### 职责
处理所有账号相关消息（原 handle_account_msg 方法，265 行）：
- 查询角色列表（MSG_ID_QUERY_ROLES_REQ）
- 创建角色（MSG_ID_CREATE_ROLE_REQ）
- 查询账号数据（MSG_ID_ACCOUNT_DATA_REQ）
- 设置账号数据（MSG_ID_ACCOUNT_SET_REQ）
- 通过账号通道进入游戏（MSG_ID_ENTER_GAME_REQ）

### 移入内容
将单个 265 行方法拆分为 5 个子方法：
```cpp
class AccountMessageHandler {
public:
    AccountMessageHandler(DBMgrConnectionManager* dbmgr_mgr,
                          PlayerManager* player_mgr,
                          PlayerIdGenerator* id_gen);

    void handle(std::shared_ptr<GateSession> session,
                const std::vector<uint8_t>& payload);

    using SendToGateFunc = std::function<void(std::shared_ptr<GateSession>, uint32_t, const std::string&)>;
    using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;
    void set_send_callbacks(SendToGateFunc gate_func, SendGameMsgFunc game_func);

private:
    void handle_query_roles(std::shared_ptr<GateSession> session,
                            const std::string& account_id, uint32_t inner_msg_id,
                            const std::string& inner_payload);
    void handle_create_role(std::shared_ptr<GateSession> session,
                            const std::string& account_id,
                            const std::string& inner_payload);
    void handle_account_data(std::shared_ptr<GateSession> session,
                             const std::string& account_id, uint32_t inner_msg_id,
                             const std::string& inner_payload);
    void handle_account_set(std::shared_ptr<GateSession> session,
                            const std::string& account_id,
                            const std::string& inner_payload);
    void handle_enter_game(std::shared_ptr<GateSession> session,
                           const std::string& account_id,
                           const std::string& inner_payload);

    DBMgrConnectionManager* dbmgr_mgr_;
    PlayerManager* player_mgr_;
    PlayerIdGenerator* id_gen_;
    SendToGateFunc send_to_gate_func_;
    SendGameMsgFunc send_game_msg_func_;
};
```

---

## 模块 4：AdminHandler

### 新文件
- `scripts/server/game_server/src/admin_handler.h`
- `scripts/server/game_server/src/admin_handler.cpp`

### 职责
- 管理消息分发（handle_admin_message）
- 停服请求处理（handle_shutdown）
- 停服响应处理（handle_shutdown_resp）

### 移入内容
```cpp
class AdminHandler {
public:
    AdminHandler(PlayerManager* player_mgr, GameSceneManager* scene_mgr,
                 GameClock* clock, DBMgrConnectionManager* dbmgr_mgr);

    void handle(std::shared_ptr<GateSession> session, uint32_t msg_id,
                const std::vector<uint8_t>& payload);
    void set_stop_callback(std::function<void()> stop_func);

private:
    void handle_shutdown(std::shared_ptr<GateSession> session, const AdminShutdownMsg& msg);
    void handle_shutdown_resp(std::shared_ptr<GateSession> session, const AdminShutdownResp& resp);

    PlayerManager* player_mgr_;
    GameSceneManager* scene_mgr_;
    GameClock* clock_;
    DBMgrConnectionManager* dbmgr_mgr_;
    std::function<void()> stop_func_;
};
```

---

## GameServer 重构

### 提取后的 GameServer 结构
```cpp
class GameServer {
    // 基础设施（保留）
    PlayerManager player_mgr_;
    MessageHandler msg_handler_;
    DBMgrConnectionManager dbmgr_mgr_;
    ItemInteractionHandler item_handler_;

    // 新提取的子系统
    std::unique_ptr<GameSceneManager> scene_mgr_;
    std::unique_ptr<GameClock> game_clock_;
    std::unique_ptr<AccountMessageHandler> account_handler_;
    std::unique_ptr<AdminHandler> admin_handler_;

    // libevent 基础设施（保留）
    // ...
};
```

### 变更点
1. **start()**：初始化子系统，注入依赖和回调
2. **update_game_logic()**：委托给 `game_clock_->update()`、`item_handler_.update()`
3. **route_internal_message()**：委托给 `account_handler_->handle()`、`admin_handler_->handle()`
4. **移除成员**：`scenes_`、6 个 clock 变量、`player_id_gen_`、`world_state_`、`drop_manager_`、`crop_system_`
5. **移除方法**：所有已迁移到子系统的方法

### 遗留成员清理
- `world_state_`、`drop_manager_`、`crop_system_`：已由 ItemInteractionHandler 内部管理，GameServer 中的副本是冗余的
- `player_id_gen_`：移入 AccountMessageHandler
- auto-pickup 中的 `drop_manager_` 引用改为 `item_handler_.drops()`
