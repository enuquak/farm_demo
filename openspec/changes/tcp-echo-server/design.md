## Context

项目从零开始，在 Windows 平台上用 C++14 实现 TCP echo 服务器，用 Python 3.12.10 实现桌面 OpenGL 客户端。服务器需要同时处理多个客户端连接，消息格式使用 Protobuf。已确认环境：protoc v30.2 已安装，Python 3.12.10 通过 `py -3.12` 调用，Winsock2 为系统自带。客户端使用 ModernGL + PyGame 渲染图形界面。

目录结构：
- `script/server_script/`：C++ 服务器源码
- `script/client_script/`：Python 客户端源码
- `proto/`：`.proto` 定义及生成代码

## Goals / Non-Goals

**Goals:**
- 实现能同时处理多个客户端的 TCP 服务器（Windows Winsock2 + select()）
- 实现 Python 3.12.10 桌面 OpenGL 客户端（ModernGL + PyGame），图形化展示连接状态和 echo 消息
- 定义 Protobuf 消息协议（EchoRequest / EchoResponse）
- 消息帧使用 4 字节大端长度前缀解决 TCP 粘包问题

**Non-Goals:**
- 不支持 TLS/SSL 加密
- 不实现身份认证
- 不支持 Linux/macOS（仅 Windows）
- 不实现持久化或日志系统
- 不处理超过 FD_SETSIZE（64）个并发连接

## Decisions

**决策 1：并发模型选 select() 而非每连接一线程**
- 选择：单线程 select() I/O 多路复用，accept 在同一主循环内处理
- 原因：C++14 标准库无异步 I/O，select() 在 Windows 上原生支持，实现简单且对测试场景的并发量（< 63）完全够用
- 备选：每连接一线程——实现更直观但线程管理复杂，Windows 线程开销较大

**决策 2：消息协议选 Protobuf + 4 字节长度前缀**
- 选择：每条消息前置 4 字节大端 uint32 表示 payload 长度，payload 为 Protobuf 序列化字节
- 原因：Protobuf 提供强类型 schema 和跨语言支持；长度前缀解决 TCP 流式传输的粘包/拆包问题
- 备选：JSON + 换行符分隔——可读性好但无 schema 约束，解析性能较差

**决策 3：proto 文件统一放 proto/ 目录，生成代码各自放入对应子目录**
- 选择：`proto/echo.proto` 为唯一来源，C++ 生成代码放 `script/server_script/proto/`，Python 生成代码放 `script/client_script/proto/`
- 原因：单一来源避免协议不一致，各语言目录独立便于构建

**决策 4：客户端使用 ModernGL + PyGame**
- 选择：PyGame 负责窗口管理和事件循环，ModernGL 提供 OpenGL 上下文和渲染
- 原因：ModernGL 是现代 OpenGL API，比 PyOpenGL 更简洁；PyGame 在 Windows 上稳定且易于集成
- 备选：PyOpenGL + GLFW——更底层，配置复杂度更高

**决策 5：客户端 TCP 通信在独立线程中运行**
- 选择：网络收发逻辑放在独立线程，PyGame 主循环负责渲染，通过线程安全队列传递消息
- 原因：PyGame 主循环需要稳定帧率，阻塞式 recv 会导致界面卡顿

**决策 6：服务器主循环结构**
```
main()
  └── WSAStartup
  └── 创建 listen socket，bind，listen
  └── 主循环:
        fd_set 包含 listen_fd + 所有 client_fd
        select() 阻塞等待
        if listen_fd 可读 → accept 新连接，加入 client 列表
        for each client_fd 可读 → 读取帧 → 反序列化 → echo → 序列化 → 发送
        if 读取返回 0 → 客户端断开，移除
```

## Risks / Trade-offs

- [风险] TCP 粘包/拆包：单次 recv 可能收到不完整的帧 → 使用带缓冲区的帧读取函数，循环 recv 直到读满 4 字节头部和完整 payload
- [风险] FD_SETSIZE 上限 64：最多 63 个并发客户端 → 文档标注，测试场景够用，如需扩展可在编译时重定义
- [风险] Windows/Linux 差异：SOCKET 类型、WSAStartup、错误码不同 → 明确仅支持 Windows，不做跨平台抽象
- [风险] Python 3.12.10 的 protobuf 兼容性：protoc v30.2 生成的代码需与 pip 安装的 protobuf 版本匹配 → 使用 `py -3.12 -m pip install protobuf` 安装，生成时用 `--python_out`
- [风险] PyGame 主循环与网络线程的数据竞争 → 使用 `queue.Queue` 作为线程安全的消息传递机制
