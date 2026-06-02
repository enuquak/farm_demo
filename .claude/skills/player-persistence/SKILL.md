---
name: player-persistence
description: 玩家持久化：脏字段追踪、三种保存路径、JSON 序列化、定时保存、实体自治。
metadata:
  type: reference
---

# 玩家持久化（Player Persistence）

## 概述

玩家数据持久化机制，目标是防止服务器崩溃导致脏数据丢失。原有设计仅在玩家断线/离开时保存数据，若服务器意外崩溃，自上次保存以来的所有玩家数据将丢失。新设计引入了定时自动保存、增量保存和脏字段追踪，确保数据安全。

核心改进：
- 每个 Player 实体拥有独立的 5 分钟定时保存器
- 业务逻辑可主动触发增量保存（仅脏字段）或指定字段保存
- 脏字段追踪机制精确记录哪些字段被修改，避免全量写入

## 架构设计

### 脏字段追踪（Dirty Field Tracking）

采用 `dirty_` + `dirty_fields_` 双重标记机制：

```cpp
// player.h
class Player {
    bool dirty_ = false;                          // 快速检查是否有脏数据
    std::unordered_set<std::string> dirty_fields_; // 精确记录哪些字段被修改
};
```

每个 setter 方法同时设置两个标记：

```cpp
void Player::set_gold(int64_t gold) {
    if (player_data_.gold != gold) {
        player_data_.gold = gold;
        dirty_ = true;
        dirty_fields_.insert("gold");
    }
}
```

字段名映射表（setter → 字段 key）：

| Setter | 字段 key |
|--------|----------|
| `set_role_name` | `"role_name"` |
| `set_level` | `"level"` |
| `set_gold` | `"gold"` |
| `set_experience` | `"experience"` |
| `set_pos_x/y/z` | `"pos_x"`, `"pos_y"`, `"pos_z"` |
| `set_energy` | `"energy"` |
| `set_scene_id` | `"scene_id"` |
| `set_inventory` | `"inventory"` |
| `set_farm_state` | `"farm_state"` |
| `set_extra_data` | `"extra_data"` |

`set_player_data()` 批量设置时，将所有字段标记为脏。

### 三种保存路径

Player 提供三个公开保存方法：

**1. `save_full()` — 全量保存**
- 使用 `SET_ALL` 操作，发送完整的 PlayerBizData JSON
- 用于定时保存和退出保存
- 清除 `dirty_` 和 `dirty_fields_`

**2. `save()` — 脏字段批量保存**
- 遍历 `dirty_fields_`，为每个字段发送独立的 `SET` 请求
- 用于业务逻辑主动刷新所有脏数据
- 发送完毕后清除 `dirty_` 和 `dirty_fields_`
- 如果没有脏字段，则为空操作（no-op）

**3. `save_field(field)` — 指定字段保存**
- 发送单个 `SET` 请求，仅保存指定字段
- 用于业务逻辑立即持久化关键字段（如金币变动）
- 从 `dirty_fields_` 中移除该字段；若为空则清除 `dirty_`

### JSON 序列化（nlohmann::json）

使用 `nlohmann::json` 将 PlayerBizData 序列化为 JSON 格式，与 DBMgr 的 MongoDB 存储格式一致：

```cpp
nlohmann::json data;
data["role_name"] = player_data_.role_name;
data["level"] = player_data_.level;
data["gold"] = player_data_.gold;
data["experience"] = player_data_.experience;
data["pos_x"] = player_data_.pos_x;
data["pos_y"] = player_data_.pos_y;
data["pos_z"] = player_data_.pos_z;
data["energy"] = player_data_.energy;
data["scene_id"] = player_data_.scene_id;
data["inventory"] = player_data_.inventory;   // 已是 JSON 字符串
data["farm_state"] = player_data_.farm_state;  // 已是 JSON 字符串
data["extra_data"] = player_data_.extra_data;  // 已是 JSON 字符串
std::string value = data.dump();
```

使用 JSON 替代 protobuf 序列化的原因：原 `PlayerData` proto 不包含 `gold`、`inventory`、`farm_state`、`extra_data` 等字段，而 JSON 格式与 DBMgr 存储的 `data` 子文档直接对应，字段级 `SET` 操作更自然。

### 定时保存（libevent timer）

每个 Player 实体拥有独立的 libevent 定时器，每 5 分钟触发一次：

```cpp
class Player {
    struct event* save_timer_ = nullptr;  // libevent 定时器
    struct event_base* base_ = nullptr;   // 非拥有指针

public:
    void start_save_timer(struct event_base* base);
    void stop_save_timer();

private:
    static void on_save_timer(evutil_socket_t fd, short events, void* ctx);
    void handle_save_timer();
};
```

- `start_save_timer(base)` 在 Player 创建后由 PlayerManager 调用
- `stop_save_timer()` 在 Player 销毁前调用
- 定时器回调调用 `save_full()`，仅在 `dirty_` 为 true 时执行写入
- libevent 定时器基于 O(log n) 堆，即使玩家数量较多开销也可接受

### 实体自治（Player 持有 DBMgrConnectionManager*）

Player 直接持有 `DBMgrConnectionManager*` 指针，可自主发送保存请求，无需经过 PlayerManager 中转：

```cpp
class Player {
public:
    Player(uint64_t player_id, GateSession* gate_session,
           DBMgrConnectionManager* dbmgr_mgr);
private:
    DBMgrConnectionManager* dbmgr_mgr_;  // 非拥有指针
};
```

PlayerManager 在创建 Player 时传入 `dbmgr_mgr_`，Player 可直接调用 `send_player_data_req()` 发起数据请求。

## 关键流程

### Setter 脏标记流程

```
业务逻辑调用 player->set_gold(500)
  → 检查新值是否与旧值不同
  → 更新 player_data_.gold = 500
  → dirty_ = true
  → dirty_fields_.insert("gold")
```

### 定时保存流程

```
Player 创建
  → PlayerManager 调用 start_save_timer(base)
  → 注册 5 分钟 libevent 定时器

每 5 分钟定时器触发
  → on_save_timer() 回调
  → handle_save_timer() 检查 dirty_ 标志
  → 如果 dirty_ == true:
      → save_full() → 构建完整 JSON → SET_ALL 请求
      → 收到响应后清除 dirty_ 和 dirty_fields_
  → 如果 dirty_ == false: 跳过，等待下次触发

Player 销毁
  → stop_save_timer() 移除定时器事件
```

### 断线保存流程

```
Player 断线事件
  → PlayerManager::remove_player_with_save()
  → save_full() → SET_ALL 请求
  → 清除 dirty 标记
  → 销毁 Player 对象
```

保存响应处理：
- 成功（code=0）：记录日志，清除脏标记
- 失败（code!=0）：记录错误日志，**不清除脏标记**，等待下次定时器重试

## 关键代码路径

| 文件 | 职责 |
|------|------|
| `player.h` / `player.cpp` | Player 实体，包含脏标记成员（`dirty_`、`dirty_fields_`）、定时器成员（`save_timer_`、`base_`）、`dbmgr_mgr_` 指针；实现 `save()`、`save_field()`、`save_full()`、`start_save_timer()`、`stop_save_timer()` 方法；所有 setter 方法更新脏标记 |
| `player_manager.h` / `player_manager.cpp` | PlayerManager，暴露 `dbmgr_mgr()` 访问器；创建 Player 时传入 `dbmgr_mgr_` 并调用 `start_save_timer()`；销毁前调用 `stop_save_timer()` |
| DBMgr 协议（PlayerDataReq/Resp） | DBMgr 提供 GET_ALL/GET/SET_ALL/SET/DEL 操作，数据以 JSON 文件存储于 `data-dir/players/<player_id>.json` |

## 常见陷阱

### 脏标记未清除
- 在 `save_full()` 或 `save()` 失败时**不应**清除脏标记，否则数据将永久丢失
- 仅在收到成功响应后才清除脏标记
- `save_field(key)` 成功后只移除该字段的标记，不影响其他脏字段

### JSON 格式不一致
- 字段名必须使用 snake_case（如 `"role_name"`、`"pos_x"`），与 DBMgr 存储格式一致
- `inventory`、`farm_state`、`extra_data` 本身是 JSON 字符串，序列化时需注意嵌套
- DBMgr 使用 `data` 子文档存储，SET 操作的 key 映射为 `data.<field_name>`

### 定时器泄漏
- `stop_save_timer()` 必须在 Player 销毁前调用，否则定时器回调将访问已释放内存
- Player 析构函数中应包含定时器清理逻辑
- 如果 Player 在定时器回调执行期间被销毁，需要通过 request_id 追踪机制避免悬空回调

### 并发保存冲突
- 多个保存请求可能同时在飞（in-flight），如定时保存和业务触发的增量保存
- DBMgr 的请求-响应通过 `request_id` 匹配，不会混淆
- 但需注意同一字段的多次写入顺序由网络决定，业务层应考虑幂等性

## 扩展指南：添加新的持久化字段

1. 在 `PlayerBizData` 结构体中添加新字段（如 `int32_t vip_level`）

2. 在 `player.h` / `player.cpp` 中添加对应的 setter 方法：

```cpp
void Player::set_vip_level(int32_t vip_level) {
    if (player_data_.vip_level != vip_level) {
        player_data_.vip_level = vip_level;
        dirty_ = true;
        dirty_fields_.insert("vip_level");
    }
}
```

3. 在 JSON 序列化代码（`save_full()` 和 `save()` 的构建逻辑）中添加该字段：

```cpp
data["vip_level"] = player_data_.vip_level;
```

4. 更新字段名映射表（本 Skill 文档中的表格）

5. 如果该字段需要从 DBMgr 读取时解析，确保 `GET` 和 `GET_ALL` 的反序列化逻辑也包含该字段

## 相关 Skill

- [[server-architecture]] — 游戏服务器整体架构，包含 GameServer、PlayerManager 的职责划分
- [[dbmgr-data-layer]] — DBMgr 数据层协议（PlayerDataReq/Resp），操作类型（GET_ALL/GET/SET_ALL/SET/DEL），数据存储结构
