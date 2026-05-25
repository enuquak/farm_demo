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

[2026-05-25] [Pattern] [DBMgr 架构模式]
Description: DBMgr 作为独立进程，采用与 Gate/Game 相同的单线程 libevent 事件循环模式。DBMgr 被动监听端口等待 Game 连接，连接建立后主动发送 DBMgrIdentify 进行身份标识，随后定期发送心跳保活。消息路由区分身份识别前（只允许 IDENTIFY_RESP）和识别后（处理心跳响应和数据请求）两种状态。
Context: 实现 DBMgr 数据管理进程，为 Game 提供玩家数据持久化能力。
Action: 复用 Gate/Game 的架构模式（evconnlistener + bufferevent + 定时器），但连接语义相反：DBMgr 是被动接受方，Game 是主动连接方。DBMgr 主动发送 Identify 和 Heartbeat，Game 回复响应。

[2026-05-25] [Pattern] [DBMgr 数据操作模型]
Description: DBMgr 支持五种数据操作：GET（读单个 key）、SET（写单个 key）、DEL（删单个 key）、GET_ALL（读全部）、SET_ALL（写全部）。每个玩家独立一个 JSON 文件（data-dir/players/{player_id}.json），单 key 操作在 DBMgr 内部也是全文件读写。请求通过 request_id 异步匹配，DBMgr 不理解 value 内容，只做纯代理存储。
Context: Game 需要异步读写玩家数据，DBMgr 作为数据代理层。
Action: 在 DataManager 类中实现 JSON 文件读写，使用简单的 JSON 解析器处理 key 查找、设置、删除操作。响应中携带与请求相同的 request_id，便于 Game 匹配异步回调。

[2026-05-25] [Pitfall] [MSVC absl 链接错误 __std_rotate]
Description: 使用预编译的 absl/protobuf 库时，链接阶段可能出现 __std_rotate 未定义错误。这是因为预编译库使用的 MSVC 工具集版本与当前编译环境不同，导致 CRT 内部函数不兼容。
Context: C++ 项目依赖预编译的 protobuf-lite 和 absl 库，使用 MSVC 编译。
Action: 在项目中添加 msvc_compat.cpp 文件，提供 __std_rotate 函数的兼容实现。该文件使用 extern "C" 声明，内部调用 std::rotate 作为回退实现。

[2026-05-25] [Pattern] [CMake 多配置输出路径]
Description: CMake 使用 Visual Studio 生成器时，RUNTIME_OUTPUT_DIRECTORY 属性会自动添加配置子目录（如 Release/），导致实际输出路径为 Release/Release/dbmgr.exe。build_cpp14.bat 的 DLL 复制逻辑可能只复制到上层 Release/ 目录，导致 exe 找不到 DLL。
Context: 使用 CMake + Visual Studio 生成器构建 Windows 可执行文件，依赖 libevent DLL。
Action: 手动将 DLL 复制到 exe 所在的 Release/Release/ 目录，或修改 CMakeLists.txt 使用生成器表达式避免多层目录。建议在构建脚本中统一处理 exe 和 DLL 的输出路径。

[2026-05-25] [Pitfall] [跨项目头文件包含]
Description: 在 CMake 构建系统中，源文件的相对 include 路径是相对于源文件物理位置解析的，但 MSVC + CMake 构建时编译器工作目录在 build/ 下，导致相对路径 "../../../../other_project/src/header.h" 找不到文件。
Context: game_server 需要包含 dbmgr 项目的 internal_msg_ids.h，使用相对路径 include。
Action: 在 CMakeLists.txt 中通过 target_include_directories 添加目标项目的 src 目录，然后直接 #include "header.h"。注意：如果两个项目有同名头文件（如都有 internal_msg_ids.h），会产生冲突，需要在本地重新定义常量并添加注释标注来源。

[2026-05-25] [Pattern] [CMake 输出目录修复]
Description: CMake 的 RUNTIME_OUTPUT_DIRECTORY 在多配置生成器（Visual Studio）下会自动追加配置名子目录。使用 RUNTIME_OUTPUT_DIRECTORY_RELEASE 可以直接指定 Release 配置的输出路径，避免多层目录。
Context: game_server 项目构建后 exe 出现在 Release/Release/ 而非 Release/。
Action: 在 CMakeLists.txt 中同时设置 RUNTIME_OUTPUT_DIRECTORY 和 RUNTIME_OUTPUT_DIRECTORY_RELEASE 为同一路径。

[2026-05-25] [Pattern] [libevent 出站 TCP 连接]
Description: 使用 libevent 建立出站 TCP 连接的模式：bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE) 创建 bev，bufferevent_socket_connect(bev, addr, len) 发起异步连接，在 on_event 回调中检测 BEV_EVENT_CONNECTED 确认连接成功。连接失败时 on_event 收到 BEV_EVENT_ERROR。
Context: Game Server 主动连接 DBMgr 进程。
Action: 在 on_event 回调中先检查 BEV_EVENT_CONNECTED（连接成功），再检查 BEV_EVENT_EOF | BEV_EVENT_ERROR（断开/失败）。连接成功后等待对方发送身份标识消息。

[2026-05-25] [Pattern] [异步请求-回调队列]
Description: 实现异步请求-响应匹配的标准模式：使用 atomic 自增计数器生成唯一 request_id，发送请求时将 (request_id -> callback) 存入 unordered_map，收到响应时通过 request_id 查找并调用 callback，然后移除条目。连接断开时遍历 map 清理该连接的所有 pending 请求。
Context: Game 向 DBMgr 发送 PlayerDataReq 并异步等待 PlayerDataResp。
Action: 使用 std::atomic<uint64_t> 生成 request_id，std::unordered_map<uint64_t, PendingRequest> 存储待处理请求。PendingRequest 包含 request_id、目标 dbmgr_index、callback、发送时间。

[2026-05-25] [Pattern] [玩家数据层异步加载模式]
Description: Game Server 的玩家数据加载采用异步回调模式：PlayerManager::add_player_with_data_load() 创建 Player 对象（状态为 LOADING），通过 DBMgrConnectionManager 发送 GET_ALL 请求，响应回调中解析数据并填充 Player 对象（状态变为 LOADED），最后调用 PlayerJoinCallback 通知 GameServer 发送响应给 Gate。Gate 连接断开时，通过 remove_players_by_gate_with_save() 批量保存数据。
Context: Game Server 需要在玩家加入时异步加载数据，离开时保存数据。
Action: Player 类扩展为包含 PlayerData 结构体（level、gold、experience、inventory、farm_state、extra_data）和数据状态（NOT_LOADED/LOADING/LOADED/FAILED）。PlayerManager 维护 pending_join_callbacks_ 映射，在 handle_player_data_loaded() 回调中完成数据填充和回调通知。数据保存使用脏标记（dirty flag）避免不必要的写入。

[2026-05-25] [Pattern] [新玩家初始化与数据降级策略]
Description: 当 DBMgr 返回空数据（新玩家）时，Player::init_default_data() 构造初始数据（level=1、gold=100、空背包/农场），并标记为脏以便首次保存。当数据加载失败（DBMgr 不可用或网络错误）时，使用默认数据作为降级策略，确保玩家仍能正常加入游戏。
Context: Game Server 处理首次登录玩家和 DBMgr 故障场景。
Action: 在 handle_player_data_loaded() 中区分三种情况：(1) code=0 且有数据 -> 解析并填充；(2) code=0 且无数据 -> 初始化默认数据并保存；(3) code!=0 -> 使用默认数据降级。所有情况都设置 data_state 为 LOADED 并调用 join callback。

[2026-05-25] [Pattern] [Player 数据脏标记保存]
Description: Player 对象维护 dirty_ 标记，任何 set_xxx() 方法在数据实际变化时设置 dirty=true。save_player_data() 检查 dirty 标记，未修改则跳过保存。保存成功后重置 dirty=false。这避免了玩家离开时的不必要 DBMgr 写入。
Context: Game Server 需要在玩家离开时保存数据，但大部分玩家可能未修改任何数据。
Action: Player 的每个 setter 方法先检查新旧值是否相同，不同才设置 dirty=true。save_player_data() 在发送 SET_ALL 请求前检查 dirty 标记。回调中不重置 dirty（已在发送前重置），避免异步期间的修改丢失。
