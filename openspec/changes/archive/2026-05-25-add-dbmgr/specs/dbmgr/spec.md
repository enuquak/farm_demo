## ADDED Requirements

### Requirement: TCP 监听
DBMgr SHALL 监听指定端口，接受 Game Server 的 TCP 长连接。

#### Scenario: 服务器启动监听
- **WHEN** DBMgr 启动，传入 --index、--port、--data-dir 参数
- **THEN** 绑定指定端口，创建 data-dir/players/ 目录（如不存在），开始监听 TCP 连接

#### Scenario: 接受 Game 连接
- **WHEN** Game Server 发起 TCP 连接请求
- **THEN** 接受连接，创建 GameSession 对象，注册到事件循环，发送 DBMgrIdentify 消息

#### Scenario: Game 连接断开清理
- **WHEN** Game 主动断开或连接异常断开
- **THEN** 检测到断开事件，销毁 GameSession，记录断开日志

### Requirement: 身份标识
DBMgr SHALL 在连接建立后主动向 Game 发送身份标识。

#### Scenario: 发送身份标识
- **WHEN** 与 Game 的 TCP 连接建立成功
- **THEN** 发送 DBMgrIdentify 消息（MsgID 4001），携带 index 和 address

#### Scenario: 收到身份标识确认
- **WHEN** 收到 DBMgrIdentify_RESP 消息（MsgID 4002）
- **THEN** 标记连接状态为 IDENTIFIED，可以开始处理数据请求

### Requirement: 心跳保活
DBMgr SHALL 定期向 Game 发送心跳，维持连接活性。

#### Scenario: 发送心跳
- **WHEN** 连接状态为 IDENTIFIED，且距离上次心跳已过 5 秒
- **THEN** 发送 DBMgrHeartbeat 消息（MsgID 4003），携带当前时间戳

#### Scenario: 收到心跳响应
- **WHEN** 收到 DBMgrHeartbeat_RESP 消息（MsgID 4004）
- **THEN** 更新最后心跳时间

### Requirement: 玩家数据读取
DBMgr SHALL 支持读取玩家数据，从 JSON 文件加载并返回。

#### Scenario: 读取玩家全部数据
- **WHEN** 收到 PlayerDataReq，op=GET_ALL，player_id=N
- **THEN** 读取 data-dir/players/N.json 文件，将内容作为 value 返回 PlayerDataResp，code=0

#### Scenario: 读取玩家指定 key
- **WHEN** 收到 PlayerDataReq，op=GET，player_id=N，key="inventory"
- **THEN** 读取 data-dir/players/N.json，提取 inventory 字段的值，序列化后返回 PlayerDataResp，code=0

#### Scenario: 玩家数据文件不存在
- **WHEN** 收到 PlayerDataReq，但 data-dir/players/N.json 文件不存在
- **THEN** 返回 PlayerDataResp，code=0，value 为空（表示数据不存在，非错误）

#### Scenario: 读取指定 key 但 key 不存在
- **WHEN** 收到 PlayerDataReq，op=GET，但 JSON 中不存在该 key
- **THEN** 返回 PlayerDataResp，code=0，value 为空

### Requirement: 玩家数据写入
DBMgr SHALL 支持写入玩家数据，将数据持久化到 JSON 文件。

#### Scenario: 写入玩家全部数据
- **WHEN** 收到 PlayerDataReq，op=SET_ALL，player_id=N，value 有内容
- **THEN** 将 value 反序列化为 JSON，写入 data-dir/players/N.json，返回 PlayerDataResp code=0

#### Scenario: 写入玩家指定 key
- **WHEN** 收到 PlayerDataReq，op=SET，player_id=N，key="gold"，value 有内容
- **THEN** 读取 JSON 文件，更新 gold 字段，写回文件，返回 PlayerDataResp code=0

#### Scenario: 写入时文件不存在
- **WHEN** 收到 PlayerDataReq，op=SET 或 SET_ALL，但文件不存在
- **THEN** 创建新文件，写入数据，返回 PlayerDataResp code=0

### Requirement: 玩家数据删除
DBMgr SHALL 支持删除玩家数据的指定 key 或整个文件。

#### Scenario: 删除指定 key
- **WHEN** 收到 PlayerDataReq，op=DEL，player_id=N，key="inventory"
- **THEN** 读取 JSON 文件，删除 inventory 字段，写回文件，返回 PlayerDataResp code=0

#### Scenario: 删除时 key 不存在
- **WHEN** 收到 PlayerDataReq，op=DEL，但 JSON 中不存在该 key
- **THEN** 返回 PlayerDataResp code=0（幂等操作）

### Requirement: 请求路由
DBMgr SHALL 在 PlayerDataResp 中携带与请求相同的 request_id，以便 Game 匹配异步回调。

#### Scenario: 正常请求响应
- **WHEN** 收到 PlayerDataReq，request_id=1001
- **THEN** 返回 PlayerDataResp，request_id=1001，code=0 或错误码

#### Scenario: 未知消息处理
- **WHEN** 收到未知的 MsgID
- **THEN** 记录警告日志，丢弃消息
