[2026-05-23] [Pitfall] [C++ 构建脚本]
Description: build_cpp14.bat 仅执行 CMake 编译，不处理运行时依赖复制。CMakeLists.txt 中的 add_custom_command POST_BUILD 步骤可能因直接调用 msbuild 而跳过。
Context: 使用 build_cpp14.bat 编译依赖 libevent 等动态库的 C++ 项目。
Action: 在 build_cpp14.bat 的编译成功分支中，显式添加 DLL 复制逻辑（从依赖安装路径复制到 Release 目录）。

[2026-05-23] [Pitfall] [C++ 项目结构]
Description: CMake 构建输出在 build/Release/ 目录，但 build_cpp14.bat 的 install 步骤可能将 exe 复制到上层 Release/ 目录。DLL 仅存在于 build/Release/，导致上层 Release/ 目录的 exe 运行时找不到 DLL。
Context: 使用 CMake + install 的 C++ 项目，构建后在非 build 目录运行 exe。
Action: 在构建脚本中统一将 exe 和 DLL 复制到同一个输出目录，避免路径分散。

[2026-05-23] [Pattern] [第三方库 DLL 管理]
Description: Windows 上使用第三方 C++ 库时，应在项目文档或构建脚本中明确列出所有运行时 DLL 依赖及其来源路径，便于新环境搭建和 CI 配置。
Context: 项目依赖 libevent、protobuf 等需要 DLL 的第三方库。
Action: 在 CMakeLists.txt 或构建脚本中维护 DLL 依赖清单，编译后自动复制到输出目录。

[2026-05-23] [Pattern] [Game Server 架构]
Description: Game Server 采用与 Gate Server 相同的单线程 libevent 事件循环模式，监听 TCP 端口接受 Gate 连接。使用 GateSession 管理 Gate 连接状态，PlayerManager 管理在线玩家，MessageHandler 提供业务消息注册和分发框架。
Context: 实现 Game Server 处理 Gate 转发的客户端游戏逻辑消息。
Action: 复用 Gate Server 的架构模式（evconnlistener + bufferevent + 定时器），但 Session 语义不同（GateSession 表示 Gate 连接，而非客户端连接）。

[2026-05-23] [Pattern] [内部协议消息处理]
Description: Game Server 处理两类消息：内部协议消息（3000-3299 范围，如心跳、身份识别、玩家生命周期）和业务消息（4000+ 范围，通过 CLIENT_MSG 转发）。内部消息在 GameServer 类中直接处理，业务消息通过 MessageHandler 框架分发。
Context: Gate↔Game 内部通信协议实现。
Action: 在 route_internal_message 中区分已识别和未识别的 Gate 连接，未识别时只允许 GATE_IDENTIFY 消息。

[2026-05-23] [Pitfall] [CMake 构建输出路径]
Description: CMake 的 RUNTIME_OUTPUT_DIRECTORY 属性在多配置生成器（如 Visual Studio）下会自动添加配置子目录（Release/Release/），导致 exe 路径比预期多一层。
Context: 使用 CMake + Visual Studio 生成器构建 Windows 可执行文件。
Action: 手动将 exe 从 build/Release/ 复制到目标 Release 目录，或在 CMakeLists.txt 中使用生成器表达式 ${CMAKE_CFG_INTDIR} 处理。

[2026-05-23] [Pattern] [Gate Server Game 连接]
Description: Gate Server 通过 GameConnection 类管理与 Game Server 的连接。GameConnection 封装了 libevent 的 bufferevent，支持自动重连（5 秒间隔）和心跳（5 秒间隔）。连接状态包括 DISCONNECTED、CONNECTING、IDENTIFIED。
Context: Gate Server 需要主动连接 Game Server 并维持长连接。
Action: 在 GateServer 类中持有 GameConnection 实例，启动时自动连接 Game，断开时自动重连。消息回调通过 std::function 绑定到 GateServer 的处理方法。

[2026-05-23] [Pattern] [消息路由扩展]
Description: Gate Server 的消息路由需要区分三类消息：心跳（1001）直接处理、登录（2001）处理后通知 Game、游戏逻辑（4000+）转发到 Game。转发时使用 ClientMessage 包装 player_id + msg_id + payload。
Context: Gate Server 需要同时处理客户端消息和转发游戏逻辑消息到 Game。
Action: 在 route_message() 中按 msg_id 范围分发，游戏逻辑消息打包为 ClientMessage 发送到 Game，Game 响应的 GameMessage 解包后直接发送给客户端。
