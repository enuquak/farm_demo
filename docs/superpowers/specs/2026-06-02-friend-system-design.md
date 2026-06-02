# 好友系统设计文档

## 概述

为农场游戏新增完整的社交好友系统，包括好友关系管理、私聊消息、礼物赠送、农场访问（互助+偷菜）、好友推荐等功能。

## 架构设计

### 整体架构

采用独立微服务方案，新增 Friend Service 处理所有好友相关逻辑：

```
┌─────────────┐     ┌─────────────┐     ┌─────────────────┐
│   Client    │────▶│ Gate Server │────▶│   Game Server   │
│  (Python)   │◀────│   (C++)     │◀────│     (C++)       │
└─────────────┘     └─────────────┘     └────────┬────────┘
                                                  │
                                                  │ Internal RPC
                                                  ▼
                                        ┌─────────────────┐
                                        │ Friend Service  │
                                        │     (C++)       │
                                        └────────┬────────┘
                                                  │
                                    ┌─────────────┼─────────────┐
                                    ▼             ▼             ▼
                               ┌────────┐   ┌────────┐   ┌────────┐
                               │ Redis  │   │ DBMgr  │   │  ...   │
                               └────────┘   └────────┘   └────────┘
```

### Friend Service 职责

1. **好友关系管理**：添加/删除/查询好友、好友列表
2. **在线状态维护**：通过 Redis 记录玩家在线状态
3. **私聊消息**：文字消息、表情、离线消息存储
4. **礼物系统**：物品赠送逻辑
5. **农场访问控制**：授权/撤销好友访问权限
6. **好友推荐**：基于共同好友或活跃度推荐

### 通信协议

- **Game Server ↔ Friend Service**：使用 Protobuf over TCP，类似 Game Server ↔ DBMgr 的模式
- **Client ↔ Game Server**：复用现有的 Gate 转发机制，新增好友相关 MsgID

## 数据模型与存储

### 好友关系数据

```
Redis 结构：
  friend:{player_id}:set          → Set<player_id>    // 好友列表
  friend:{player_id}:requests     → Hash<player_id, timestamp>  // 待处理的好友请求
  friend:{player_id}:online       → String "1"/"0"    // 在线状态
  friend:{player_id}:last_login   → String timestamp  // 最后登录时间
  friend:{player_id}:scene        → String scene_id   // 当前所在场景

DBMgr 持久化：
  friend_data:{player_id} → JSON {
    "friends": [100001, 100002, ...],
    "blocked": [100003, ...],
    "settings": {
      "allow_visit": true,
      "allow_steal": true
    }
  }
```

### 私聊消息数据

```
Redis 结构：
  chat:offline:{receiver_id}      → List<{sender_id, msg_type, content, timestamp}>  // 离线消息队列

DBMgr 持久化：
  chat_history:{player1_id}:{player2_id} → JSON [
    {sender_id, msg_type, content, timestamp},
    ...
  ]
```

### 好友农场访问数据

```
Redis 结构：
  farm:visit:authorized:{owner_id} → Set<visitor_id>  // 已授权访问的玩家
  farm:visit:active:{owner_id}    → Set<visitor_id>   // 当前正在访问的玩家

DBMgr 持久化：
  访问授权关系持久化到 owner 的 friend_data 中
```

### 数据同步策略

1. **写入**：先写 Redis，异步持久化到 DBMgr
2. **读取**：优先读 Redis，缓存未命中时从 DBMgr 加载
3. **一致性**：使用定时任务（如每 5 分钟）将 Redis 脏数据刷到 DBMgr

## 消息协议设计

### MsgID 分配

好友系统使用 MsgID 范围 **5000-5999**：

| MsgID | 名称 | 方向 | 说明 |
|-------|------|------|------|
| 5001 | FRIEND_SEARCH_REQ | Client → Game | 搜索玩家 |
| 5002 | FRIEND_SEARCH_RESP | Game → Client | 搜索结果 |
| 5003 | FRIEND_ADD_REQ | Client → Game | 发送好友请求 |
| 5004 | FRIEND_ADD_RESP | Game → Client | 请求结果 |
| 5005 | FRIEND_ADD_NOTIFY | Game → Client | 收到好友请求通知 |
| 5006 | FRIEND_ACCEPT_REQ | Client → Game | 接受好友请求 |
| 5007 | FRIEND_ACCEPT_RESP | Game → Client | 接受结果 |
| 5008 | FRIEND_REJECT_REQ | Client → Game | 拒绝好友请求 |
| 5009 | FRIEND_REJECT_RESP | Game → Client | 拒绝结果 |
| 5010 | FRIEND_DELETE_REQ | Client → Game | 删除好友 |
| 5011 | FRIEND_DELETE_RESP | Game → Client | 删除结果 |
| 5012 | FRIEND_LIST_REQ | Client → Game | 获取好友列表 |
| 5013 | FRIEND_LIST_RESP | Game → Client | 好友列表数据 |
| 5014 | FRIEND_ONLINE_NOTIFY | Game → Client | 好友上线通知 |
| 5015 | FRIEND_OFFLINE_NOTIFY | Game → Client | 好友下线通知 |
| 5020 | FRIEND_CHAT_REQ | Client → Game | 发送私聊消息 |
| 5021 | FRIEND_CHAT_RESP | Game → Client | 发送结果 |
| 5022 | FRIEND_CHAT_NOTIFY | Game → Client | 收到私聊消息 |
| 5023 | FRIEND_CHAT_HISTORY_REQ | Client → Game | 获取聊天记录 |
| 5024 | FRIEND_CHAT_HISTORY_RESP | Game → Client | 聊天记录数据 |
| 5030 | FRIEND_GIFT_REQ | Client → Game | 赠送物品 |
| 5031 | FRIEND_GIFT_RESP | Game → Client | 赠送结果 |
| 5032 | FRIEND_GIFT_NOTIFY | Game → Client | 收到礼物通知 |
| 5040 | FRIEND_VISIT_REQ | Client → Game | 访问好友农场 |
| 5041 | FRIEND_VISIT_RESP | Game → Client | 访问结果 |
| 5042 | FRIEND_VISIT_ACTION_REQ | Client → Game | 访问中操作（浇水/偷菜） |
| 5043 | FRIEND_VISIT_ACTION_RESP | Game → Client | 操作结果 |
| 5050 | FRIEND_RECOMMEND_REQ | Client → Game | 获取推荐好友 |
| 5051 | FRIEND_RECOMMEND_RESP | Game → Client | 推荐列表 |

### Protobuf 消息定义

```protobuf
// 好友信息
message FriendInfo {
    uint64 player_id = 1;
    string role_name = 2;
    uint32 level = 3;
    bool online = 4;
    string scene_id = 5;
    uint64 last_login = 6;
}

// 好友请求
message FriendRequest {
    uint64 sender_id = 1;
    string sender_name = 2;
    uint32 sender_level = 3;
    uint64 timestamp = 4;
}

// 搜索玩家请求
message FriendSearchReq {
    string keyword = 1;     // 角色名或 player_id
}

// 搜索玩家响应
message FriendSearchResp {
    int32 code = 1;
    repeated FriendInfo results = 2;
}

// 好友列表响应
message FriendListResp {
    int32 code = 1;
    repeated FriendInfo friends = 2;
    repeated FriendRequest pending_requests = 3;
}

// 私聊消息
message FriendChatMsg {
    uint64 sender_id = 1;
    uint64 receiver_id = 2;
    int32 msg_type = 3;     // 0=text, 1=emote, 2=gift
    string content = 4;
    uint64 timestamp = 5;
}

// 赠送物品
message FriendGiftReq {
    uint64 receiver_id = 1;
    int32 item_id = 2;
    int32 count = 3;
}

// 访问好友农场
message FriendVisitReq {
    uint64 owner_id = 1;
}

// 访问中操作
message FriendVisitActionReq {
    uint64 owner_id = 1;
    int32 action_type = 2;  // 0=water, 1=weed, 2=harvest, 3=steal
    int32 target_x = 3;
    int32 target_y = 4;
}
```

## Friend Service 核心逻辑

### 服务架构

```
FriendService
├── main.cpp                    // 入口，事件循环
├── friend_service.h/cpp        // 核心服务类
├── gate_session.h/cpp          // 与 Game Server 的连接会话
├── friend_manager.h/cpp        // 好友关系管理
├── chat_manager.h/cpp          // 私聊消息管理
├── gift_manager.h/cpp          // 礼物系统管理
├── visit_manager.h/cpp         // 农场访问管理
├── redis_client.h/cpp          // Redis 客户端
└── dbmgr_client.h/cpp          // DBMgr 客户端（持久化）
```

### 核心流程

#### 1. 添加好友流程

```
Client A                Game Server             Friend Service           Redis
   │                        │                        │                    │
   │──FRIEND_ADD_REQ──────▶│                        │                    │
   │                        │──FRIEND_ADD_REQ──────▶│                    │
   │                        │                        │──SISMEMBER────────▶│ (检查是否已是好友)
   │                        │                        │◀──────────────────│
   │                        │                        │──HSET────────────▶│ (记录请求)
   │                        │◀─FRIEND_ADD_RESP──────│                    │
   │◀─FRIEND_ADD_RESP──────│                        │                    │
   │                        │                        │                    │
   │                        │                        │──PUBLISH─────────▶│ (通知目标玩家)
   │                        │◀─FRIEND_ADD_NOTIFY────│                    │
   │◀─FRIEND_ADD_NOTIFY────│                        │                    │
```

#### 2. 接受好友流程

```
Client B                Game Server             Friend Service           Redis
   │                        │                        │                    │
   │──FRIEND_ACCEPT_REQ───▶│                        │                    │
   │                        │──FRIEND_ACCEPT_REQ───▶│                    │
   │                        │                        │──SADD────────────▶│ (双向添加好友)
   │                        │                        │──HDEL────────────▶│ (删除请求)
   │                        │                        │──SET────────────▶│ (持久化标记)
   │                        │◀─FRIEND_ACCEPT_RESP──│                    │
   │◀─FRIEND_ACCEPT_RESP──│                        │                    │
   │                        │                        │                    │
   │                        │                        │──PUBLISH─────────▶│ (通知 A)
   │                        │◀─FRIEND_ONLINE_NOTIFY─│                    │
   │◀─FRIEND_ONLINE_NOTIFY│                        │                    │
```

#### 3. 私聊消息流程

```
Client A                Game Server             Friend Service           Redis
   │                        │                        │                    │
   │──FRIEND_CHAT_REQ────▶│                        │                    │
   │                        │──FRIEND_CHAT_REQ────▶│                    │
   │                        │                        │──SISMEMBER────────▶│ (检查是否好友)
   │                        │                        │◀──────────────────│
   │                        │                        │──PUBLISH─────────▶│ (实时推送)
   │                        │                        │──LPUSH───────────▶│ (离线消息队列)
   │                        │◀─FRIEND_CHAT_RESP────│                    │
   │◀─FRIEND_CHAT_RESP────│                        │                    │
```

#### 4. 农场访问流程

```
Client A                Game Server             Friend Service           Redis
   │                        │                        │                    │
   │──FRIEND_VISIT_REQ───▶│                        │                    │
   │                        │──FRIEND_VISIT_REQ───▶│                    │
   │                        │                        │──SISMEMBER────────▶│ (检查好友关系)
   │                        │                        │──SADD────────────▶│ (记录访问者)
   │                        │◀─FRIEND_VISIT_RESP───│                    │
   │◀─FRIEND_VISIT_RESP───│                        │                    │
   │                        │                        │                    │
   │ (进入好友农场场景)      │                        │                    │
   │                        │                        │                    │
   │──FRIEND_VISIT_ACTION─▶│                        │                    │
   │                        │──FRIEND_VISIT_ACTION─▶│                    │
   │                        │                        │──验证权限──────────▶│
   │                        │                        │──执行操作──────────▶│
   │                        │◀─FRIEND_VISIT_ACTION──│                    │
   │◀─FRIEND_VISIT_ACTION──│                        │                    │
```

## 客户端 UI 设计

### 好友系统 UI 组件

```
FriendUI
├── friend_list_panel.py        // 好友列表面板
├── friend_request_panel.py     // 好友请求面板
├── friend_chat_panel.py        // 私聊面板
├── friend_search_panel.py      // 搜索面板
├── friend_gift_panel.py        // 赠送物品面板
├── friend_visit_panel.py       // 农场访问面板
└── friend_recommend_panel.py   // 推荐好友面板
```

### UI 布局设计

#### 好友列表面板

```
┌─────────────────────────────────────────┐
│  好友列表 (12/50)           [搜索] [添加] │
├─────────────────────────────────────────┤
│  ┌─────────────────────────────────────┐│
│  │ 🟢 玩家A  Lv.15  农场    [私聊][访问] ││
│  │ 🟢 玩家B  Lv.12  农场    [私聊][访问] ││
│  │ 🔴 玩家C  Lv.8   离线    [私聊][访问] ││
│  │ 🟢 玩家D  Lv.20  森林    [私聊][访问] ││
│  │ ...                                 ││
│  └─────────────────────────────────────┘│
├─────────────────────────────────────────┤
│  [好友请求 (3)]  [黑名单]  [推荐好友]     │
└─────────────────────────────────────────┘
```

#### 私聊面板

```
┌─────────────────────────────────────────┐
│  与 玩家A 私聊              [赠送物品] [×] │
├─────────────────────────────────────────┤
│  玩家A: 你好啊！                         │
│  我: 嗨，最近怎么样？                     │
│  玩家A: 在种菜呢 🌱                      │
│  [系统] 玩家A 赠送了 5 个 种子            │
│  ...                                    │
├─────────────────────────────────────────┤
│  [😊] [👋] [❤️] [🎉]                   │
│  ┌─────────────────────────┐ [发送]     │
│  │ 输入消息...              │            │
│  └─────────────────────────┘            │
└─────────────────────────────────────────┘
```

#### 农场访问面板

```
┌─────────────────────────────────────────┐
│  访问 玩家A 的农场           [返回我的农场] │
├─────────────────────────────────────────┤
│  当前位置: 玩家A 的农场                   │
│  授权状态: 已授权                         │
│                                         │
│  可执行操作:                             │
│  [浇水] [除草] [收获] [偷菜]             │
│                                         │
│  剩余操作次数: 3/5                       │
└─────────────────────────────────────────┘
```

### 客户端消息处理

在 `NetworkDispatcher` 中新增好友消息处理：

```python
# 新增 MsgID 常量
MSG_ID_FRIEND_SEARCH_REQ = 5001
MSG_ID_FRIEND_SEARCH_RESP = 5002
# ... 其他 MsgID

# 新增回调
on_friend_search_resp: Callable[[Any], None]
on_friend_list_resp: Callable[[Any], None]
on_friend_chat_notify: Callable[[Any], None]
# ... 其他回调
```

## 错误处理与边界情况

### 错误码定义

```cpp
enum class FriendErrorCode : int32_t {
    SUCCESS = 0,
    ALREADY_FRIENDS = 1,        // 已经是好友
    FRIEND_LIST_FULL = 2,       // 好友列表已满（50人）
    PLAYER_NOT_FOUND = 3,       // 玩家不存在
    REQUEST_NOT_FOUND = 4,      // 好友请求不存在
    NOT_FRIENDS = 5,            // 不是好友关系
    CHAT_MSG_TOO_LONG = 6,      // 聊天消息过长
    GIFT_ITEM_NOT_FOUND = 7,    // 赠送物品不存在
    GIFT_COUNT_INVALID = 8,     // 赠送数量无效
    VISIT_NOT_AUTHORIZED = 9,   // 未授权访问
    VISIT_ACTION_FAILED = 10,   // 访问操作失败
    SELF_OPERATION = 11,        // 不能对自己操作
    BLOCKED = 12,               // 被对方拉黑
    COOLDOWN = 13,              // 操作冷却中
};
```

### 边界情况处理

#### 1. 好友列表已满

```
场景：玩家 A（已有 50 好友）尝试添加玩家 B
处理：
1. Friend Service 检查 A 的好友数量
2. 返回 FRIEND_LIST_FULL 错误
3. 客户端提示 "好友列表已满，请先删除一些好友"
```

#### 2. 重复添加好友

```
场景：玩家 A 向已经是好友的玩家 B 发送好友请求
处理：
1. Friend Service 检查 Redis 中 friend:{A}:set 是否包含 B
2. 返回 ALREADY_FRIENDS 错误
3. 客户端提示 "你们已经是好友了"
```

#### 3. 好友请求过期

```
场景：玩家 A 发送好友请求后，玩家 B 7 天内未处理
处理：
1. Friend Service 定时任务扫描过期请求
2. 自动删除过期请求
3. 通知玩家 A "好友请求已过期"
```

#### 4. 离线消息堆积

```
场景：玩家 B 长期不上线，离线消息队列过长
处理：
1. 限制每个玩家的离线消息上限（如 100 条）
2. 超出上限时，删除最早的消息
3. 上线时提示 "有 X 条离线消息已过期"
```

#### 5. 农场访问冲突

```
场景：玩家 A 正在访问玩家 B 的农场，玩家 B 此时修改了访问权限
处理：
1. 玩家 B 修改权限时，检查是否有活跃访问者
2. 通知所有活跃访问者 "农场访问权限已变更"
3. 访问者被强制返回自己的农场
```

#### 6. 网络断开重连

```
场景：玩家 A 在访问好友农场时网络断开
处理：
1. 客户端检测到断开，显示重连界面
2. 重连后，Game Server 重新建立与 Friend Service 的会话
3. 检查之前的访问状态，如果已失效则返回自己的农场
```

#### 7. 数据一致性

```
场景：Redis 和 DBMgr 数据不一致
处理：
1. 定时任务每 5 分钟将 Redis 脏数据刷到 DBMgr
2. 服务启动时，从 DBMgr 加载全量数据到 Redis
3. 提供手动同步命令用于运维
```

## 测试策略

### 测试层次

#### 1. 单元测试

```
Friend Service 单元测试：
├── friend_manager_test.cpp     // 好友关系管理测试
├── chat_manager_test.cpp       // 私聊消息测试
├── gift_manager_test.cpp       // 礼物系统测试
├── visit_manager_test.cpp      // 农场访问测试
└── redis_client_test.cpp       // Redis 操作测试
```

#### 2. 集成测试

```
集成测试场景：
├── 好友添加完整流程（A 发送请求 → B 接受 → 双方好友列表更新）
├── 私聊消息完整流程（发送 → 实时推送 → 离线存储 → 上线接收）
├── 农场访问完整流程（请求访问 → 授权 → 操作 → 返回）
├── 礼物赠送完整流程（赠送 → 扣除物品 → 通知接收方）
└── 数据持久化测试（Redis → DBMgr 同步）
```

#### 3. 客户端测试

```
客户端测试：
├── friend_list_panel_test.py       // 好友列表面板测试
├── friend_chat_panel_test.py       // 私聊面板测试
├── friend_search_panel_test.py     // 搜索面板测试
└── network_dispatcher_test.py      // 消息分发测试（新增好友消息）
```

### 测试用例示例

#### 好友添加测试

```python
def test_add_friend_success():
    """测试添加好友成功"""
    # 玩家 A 发送好友请求给玩家 B
    # 验证：玩家 B 收到好友请求通知
    # 玩家 B 接受请求
    # 验证：双方好友列表都包含对方

def test_add_friend_already_friends():
    """测试重复添加好友"""
    # 玩家 A 和玩家 B 已是好友
    # 玩家 A 再次发送好友请求
    # 验证：返回 ALREADY_FRIENDS 错误

def test_add_friend_list_full():
    """测试好友列表已满"""
    # 玩家 A 已有 50 个好友
    # 玩家 A 尝试添加第 51 个好友
    # 验证：返回 FRIEND_LIST_FULL 错误
```

#### 私聊消息测试

```python
def test_chat_realtime():
    """测试实时私聊"""
    # 玩家 A 向在线的好友 B 发送消息
    # 验证：玩家 B 实时收到消息

def test_chat_offline():
    """测试离线私聊"""
    # 玩家 A 向离线的好友 B 发送消息
    # 验证：消息存储到离线队列
    # 玩家 B 上线
    # 验证：玩家 B 收到离线消息
```

#### 农场访问测试

```python
def test_visit_authorized():
    """测试授权访问"""
    # 玩家 A 访问已授权的好友 B 的农场
    # 验证：访问成功，可以执行操作

def test_visit_unauthorized():
    """测试未授权访问"""
    # 玩家 A 尝试访问未授权的玩家 C 的农场
    # 验证：返回 VISIT_NOT_AUTHORIZED 错误

def test_visit_steal():
    """测试偷菜功能"""
    # 玩家 A 访问好友 B 的农场
    # 玩家 A 执行偷菜操作
    # 验证：物品从 B 转移到 A，B 收到通知
```

## 实现计划

### 阶段一：基础设施（Week 1）

1. 创建 Friend Service 项目结构
2. 实现 Redis 客户端封装
3. 实现 DBMgr 客户端封装
4. 实现与 Game Server 的通信协议

### 阶段二：核心功能（Week 2）

1. 实现好友关系管理（添加/删除/查询）
2. 实现好友列表功能
3. 实现在线状态维护
4. 集成到 Game Server 消息分发

### 阶段三：社交功能（Week 3）

1. 实现私聊消息系统
2. 实现礼物赠送功能
3. 实现离线消息存储
4. 实现消息持久化

### 阶段四：农场访问（Week 4）

1. 实现农场访问授权
2. 实现互助操作（浇水/除草）
3. 实现偷菜功能
4. 实现访问冲突处理

### 阶段五：客户端 UI（Week 5）

1. 实现好友列表面板
2. 实现私聊面板
3. 实现搜索面板
4. 实现农场访问面板

### 阶段六：测试与优化（Week 6）

1. 编写单元测试
2. 编写集成测试
3. 性能优化
4. 文档完善

## 依赖关系

- Redis：在线状态、好友关系、离线消息队列
- DBMgr：持久化存储
- Game Server：消息路由、玩家数据
- Gate Server：客户端连接转发
