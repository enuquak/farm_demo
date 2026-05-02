## ADDED Requirements

### Requirement: 消息帧格式
所有 TCP 消息 SHALL 使用 4 字节大端 uint32 长度前缀帧格式，前缀表示后续 Protobuf payload 的字节长度。

#### Scenario: 发送消息帧
- **WHEN** 发送方序列化一条 Protobuf 消息得到 N 字节
- **THEN** 发送方 SHALL 先发送 4 字节大端编码的 N，再发送 N 字节的 Protobuf 内容

#### Scenario: 接收消息帧
- **WHEN** 接收方读取消息
- **THEN** 接收方 SHALL 先读取 4 字节解析出 payload 长度 N，再读取恰好 N 字节进行反序列化

### Requirement: EchoRequest 消息定义
EchoRequest SHALL 包含 message（string）和 timestamp（int64）两个字段。

#### Scenario: 构造 EchoRequest
- **WHEN** 客户端发起 echo 请求
- **THEN** EchoRequest 的 message 字段 SHALL 为非空字符串，timestamp 字段 SHALL 为发送时的 Unix 毫秒时间戳

### Requirement: EchoResponse 消息定义
EchoResponse SHALL 包含 message（string）、timestamp（int64）和 server_id（string）三个字段。

#### Scenario: 服务器构造 EchoResponse
- **WHEN** 服务器收到 EchoRequest
- **THEN** EchoResponse 的 message 和 timestamp SHALL 与 EchoRequest 中的值完全一致，server_id SHALL 为服务器的标识字符串（如 "server-1"）
