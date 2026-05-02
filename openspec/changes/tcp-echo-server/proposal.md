## Why

项目需要一个可验证的 TCP 通信基础设施，用于演示和测试客户端与服务器之间的双向通信。通过实现一个 echo 服务，可以验证连接建立、消息序列化/反序列化、多客户端并发处理等核心能力。

## What Changes

- 新增 C++14 TCP 服务器，使用 Winsock2 + select() 支持多客户端并发连接
- 新增 Python 3.12.10 桌面 OpenGL 客户端（ModernGL + PyGame），与服务器建立连接并发送/接收 Protobuf 消息，可视化展示通信状态
- 新增 `.proto` 消息定义文件，定义 EchoRequest / EchoResponse 消息格式
- 消息帧采用 4 字节大端长度前缀 + Protobuf 序列化内容

## Capabilities

### New Capabilities

- `echo-protocol-1`: Protobuf 消息协议定义，包含 EchoRequest 和 EchoResponse，以及 4 字节长度前缀帧格式
- `tcp-server-2`: C++14 TCP 服务器，监听端口，使用 select() 多路复用处理多个并发客户端连接，接收 Protobuf 消息并 echo 回客户端
- `tcp-client-3`: Python 3.12.10 桌面 OpenGL 客户端（ModernGL + PyGame），连接服务器，发送 EchoRequest，接收 EchoResponse，并以图形界面展示通信状态

### Modified Capabilities

## Impact

- 新增 `script/server_script/` 目录：C++ 服务器源码及构建文件
- 新增 `script/client_script/` 目录：Python 客户端源码
- 新增 `proto/` 目录：`.proto` 协议定义及生成代码
- 依赖：Winsock2（Windows 系统自带）、libprotobuf（已安装，protoc v30.2）、Python 3.12.10（调用方式：`py -3.12`）、Python protobuf / ModernGL / PyGame 包
