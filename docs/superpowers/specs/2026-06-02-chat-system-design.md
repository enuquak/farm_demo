# Chat System Design Spec

## Overview

Add a channel-based chat system to the farm game, starting with a world channel and reserving party/private channel interfaces for future implementation.

## Goals

- World channel chat for all players on the same server
- Extensible architecture for party and private channels
- Clean separation between client UI, protocol, and server logic
- Channel-specific configuration (persistence, rate limits, message length)

## Architecture

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Client    │────>│  GateServer │────>│  ChatServer │
│  (Python)   │<────│   (C++)     │<────│   (C++)     │
└─────────────┘     └─────────────┘     └─────────────┘
                          │
                          ▼
                    ┌─────────────┐
                    │  GameServer │
                    │   (C++)     │
                    └─────────────┘
```

**Message Routing**:
- `msg_id 4000-4999` → ChatServer (chat messages)
- `msg_id 2000-3999` → GameServer (game messages)

## Channel Configuration

### Channel Types

```python
class ChannelType(Enum):
    WORLD = "world"      # 全服广播
    PARTY = "party"      # 队伍广播
    WHISPER = "whisper"  # 点对点私聊
```

### Channel Config (Strategy Pattern)

```python
@dataclass
class ChannelConfig:
    channel_type: ChannelType
    persistence: str        # "none" | "memory" | "database"
    max_msg_length: int     # 消息最大长度
    cooldown_sec: float     # 发送冷却时间
    scope: str              # "server" | "party" | "private"
    max_history: int        # 客户端保留的历史条数
```

### Default Configuration

| Channel | Persistence | Max Length | Cooldown | Scope |
|---------|-------------|------------|----------|-------|
| World | none | 100 | 5s | server |
| Party | memory | 200 | 3s | party |
| Whisper | database | 500 | 1s | private |

## Protocol Design

### Message IDs (4000-4999)

```
4001  ChatSendReq        # 发送聊天请求 (Client → ChatServer)
4002  ChatSendResp       # 发送响应 (ChatServer → Client)
4003  ChatMessage        # 聊天消息推送 (ChatServer → Client)
4004  ChatHistoryReq     # 拉取历史 (Client → ChatServer) [私聊用]
4005  ChatHistoryResp    # 历史响应 (ChatServer → Client) [私聊用]
```

### Protobuf Definitions

```protobuf
syntax = "proto3";
package farm;
option optimize_for = LITE_RUNTIME;

// 聊天消息
message ChatMessage {
    uint32 channel_type = 1;    // 频道类型 (0=world, 1=party, 2=whisper)
    uint64 sender_id = 2;       // 发送者 player_id
    string sender_name = 3;     // 发送者角色名
    string content = 4;         // 消息内容
    uint64 timestamp = 5;       // 时间戳 (ms)
    uint64 target_id = 6;       // 目标 player_id (私聊用)
}

// 发送请求
message ChatSendReq {
    uint32 channel_type = 1;
    string content = 2;
    uint64 target_id = 3;       // 私聊时指定目标
}

// 发送响应
message ChatSendResp {
    int32 code = 0;             // 0=成功, 其他=错误码
    string msg = 1;
}

// 历史消息请求 (私聊用)
message ChatHistoryReq {
    uint64 target_id = 1;       // 私聊对象
    uint64 before_timestamp = 2; // 分页：此时间戳之前的消息
    uint32 limit = 3;           // 每页条数
}

// 历史消息响应
message ChatHistoryResp {
    repeated ChatMessage messages = 1;
}
```

## Client Design

### File Structure

```
scripts/client/
├── chat/
│   ├── __init__.py
│   ├── chat_channel.py      # 频道抽象 + 配置
│   ├── chat_manager.py      # 客户端聊天管理
│   └── chat_panel.py        # UI 渲染 + 输入
```

### ChatChannel (Abstract)

```python
class ChatChannel:
    """频道抽象基类"""
    def __init__(self, config: ChannelConfig):
        self.config = config
        self.messages: List[ChatMessage] = []

    def add_message(self, msg: ChatMessage) -> None:
        """添加消息到频道"""
        self.messages.append(msg)
        # 限制历史条数
        if len(self.messages) > self.config.max_history:
            self.messages.pop(0)

    def get_messages(self) -> List[ChatMessage]:
        """获取频道消息"""
        return self.messages
```

### ChatManager

```python
class ChatManager:
    """客户端聊天管理器"""
    def __init__(self, connection, player_data):
        self._connection = connection
        self._player_data = player_data
        self._channels: Dict[ChannelType, ChatChannel] = {}
        self._current_channel = ChannelType.WORLD

        # 初始化频道
        self._init_channels()

    def send_message(self, content: str, target_id: int = 0) -> None:
        """发送聊天消息"""
        # 验证限流
        # 构造 ChatSendReq
        # 通过 connection 发送

    def on_chat_message(self, msg: ChatMessage) -> None:
        """收到聊天消息回调"""
        # 路由到对应频道
        channel = self._channels.get(ChannelType(msg.channel_type))
        if channel:
            channel.add_message(msg)

    def switch_channel(self) -> None:
        """切换频道（Tab 键）"""
        # 循环切换: world → party → whisper → world
```

### ChatPanel (UI)

```python
class ChatPanel:
    """聊天面板 UI 组件"""
    # 位置：左下角
    # 尺寸：宽 350px, 高 200px
    # 背景：半透明黑色

    def __init__(self, screen_width: int, screen_height: int):
        self._x = 10
        self._y = screen_height - 210
        self._width = 350
        self._height = 200
        self._input_active = False
        self._input_text = ""
        self._selected_filter = "all"  # "all" | "world" | "party" | "whisper"

    def handle_event(self, event: pygame.event.Event) -> bool:
        """处理输入事件"""
        # Enter: 切换输入框状态
        # Tab: 切换频道
        # Esc: 关闭输入框
        # 其他: 输入文字

    def render(self, screen: pygame.Surface, chat_manager: ChatManager) -> None:
        """渲染聊天面板"""
        # 绘制半透明背景
        # 绘制频道标签 [全部][世界][队伍][私聊]
        # 绘制消息列表（根据筛选过滤）
        # 绘制输入框
```

## Server Design

### ChatServer Architecture

```
ChatServer
├── GateSessionManager    # 管理与 GateServer 的连接
├── ChannelManager        # 管理频道实例
│   ├── WorldChannel      # 世界频道（全服广播）
│   ├── PartyChannel[]    # 队伍频道（按队伍ID）
│   └── PrivateChannel    # 私聊（点对点）
├── RateLimiter           # 限流器（按玩家+频道）
└── MessageStore          # 消息存储（私聊持久化）
```

### Online Player Discovery

ChatServer needs to know which players are online for world channel broadcast. Two approaches:

**Option A: GateServer notifies ChatServer**
- GateServer sends player online/offline notifications to ChatServer
- ChatServer maintains its own player list
- More independent, but duplicates player tracking

**Option B: ChatServer queries GateServer**
- ChatServer requests player list from GateServer when needed
- GateServer is the source of truth
- Simpler, but adds request overhead

**Recommended: Option A** - GateServer notifies ChatServer of player join/leave events, similar to how it notifies GameServer.

### Message Flow

1. Client sends `ChatSendReq` to GateServer
2. GateServer routes to ChatServer (msg_id 4001)
3. ChatServer validates and processes:
   - Check rate limit
   - Check message length
   - Route to appropriate channel
4. Channel broadcasts message:
   - World: all online players (from player list maintained via GateServer notifications)
   - Party: party members
   - Whisper: target player + persist
5. ChatServer sends `ChatSendResp` to sender
6. ChatServer sends `ChatMessage` to recipients

### Rate Limiter

```cpp
class RateLimiter {
    // Key: player_id + channel_type
    // Value: last_send_timestamp
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_send_;

public:
    bool check_limit(uint64_t player_id, uint32_t channel_type, float cooldown_sec);
    void update(uint64_t player_id, uint32_t channel_type);
};
```

### Message Store (Private Messages)

```cpp
class MessageStore {
    // 存储私聊消息到数据库
    void store_private_message(uint64_t sender_id, uint64_t target_id,
                               const std::string& content, uint64_t timestamp);

    // 查询私聊历史
    std::vector<ChatMessage> get_history(uint64_t user1_id, uint64_t user2_id,
                                          uint64_t before_timestamp, uint32_t limit);
};
```

## GateServer Extension

### New Connection Type

```cpp
// In GateServer
class ChatServerConnection {
    // Similar to GameServer connection
    // Handles ChatServer-specific protocol
};
```

### Routing Logic

```cpp
// In GateServer message routing
if (msg_id >= 4000 && msg_id < 5000) {
    // Route to ChatServer
    chat_server_conn->forward(player_id, msg_id, payload);
} else if (msg_id >= 2000 && msg_id < 4000) {
    // Route to GameServer
    game_server_conn->forward(player_id, msg_id, payload);
}
```

## Implementation Scope (Phase 1)

### In Scope
- World channel complete functionality
- ChatPanel UI with channel switching framework
- ChatServer basic framework
- GateServer routing extension
- Protobuf message definitions

### Out of Scope (Future)
- Party channel (interface reserved, implementation deferred)
- Private channel (interface reserved, implementation deferred)
- Message persistence for private messages (interface reserved, database integration deferred)
- Emoji/sticker support
- Chat commands (/mute, /block)

## Error Handling

### Error Codes

```python
class ChatErrorCode(Enum):
    SUCCESS = 0
    RATE_LIMITED = 1      # 发送太频繁
    MESSAGE_TOO_LONG = 2  # 消息超长
    TARGET_OFFLINE = 3    # 私聊目标不在线
    CHANNEL_NOT_FOUND = 4 # 频道不存在
    PERMISSION_DENIED = 5 # 无权限
```

### Client Error Handling

- Rate limited: show toast notification "发送太频繁"
- Message too long: client-side validation, prevent send
- Target offline: show toast "玩家不在线"

## Testing Strategy

### Unit Tests
- Channel config validation
- Rate limiter logic
- Message routing

### Integration Tests
- Client → GateServer → ChatServer message flow
- World channel broadcast
- Channel switching UI

### Manual Tests
- Multiple clients chatting
- Rate limit enforcement
- UI responsiveness

## Future Considerations

1. **Party Channel**: Requires party system integration
2. **Private Channel**: Requires friend list or direct message UI
3. **Message Persistence**: Database schema for private messages
4. **Moderation**: Mute, block, report functionality
5. **Emoji Support**: Unicode emoji or custom stickers
