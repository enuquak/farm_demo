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

[2026-05-25] [Pattern] [账号系统架构设计]
Description: 账号系统采用与玩家数据相同的异步请求-回调模式。账号数据存储在 DBMgr 的 accounts/ 目录下，按 hash(account_id) % dbmgr_count 路由到不同的 JSON 文件。每个账号文件包含 account_id 和 roles 数组，roles 数组中的每个元素包含 server_id、player_id、role_name。
Context: 实现玩家登录时的账号角色查询和创建功能。
Action: 在 account.proto 中定义 AccountDataReq/Resp（查询角色列表）和 AccountSetReq/Resp（添加角色），消息 ID 范围 4200-4299。DBMgr 的 DataManager 扩展 get_account/set_account 方法，Game Server 的 DBMgrConnectionManager 扩展 send_account_data_req/set_account_set_req 方法。

[2026-05-25] [Pattern] [DBMgr 账号数据存储]
Description: DBMgr 的 DataManager 类扩展账号数据存储功能。账号数据文件路径为 accounts/{hash(account_id) % dbmgr_count}.json，使用 djb2 哈希算法。get_account 方法读取账号文件并解析 roles 数组，set_account 方法检查角色是否已存在（相同 server_id），不存在则添加新角色。
Context: DBMgr 需要支持账号数据的存储和读取。
Action: 在 DataManager 中添加 accounts_dir_ 成员变量和 get_account_file_path/hash_account_id 辅助方法。使用 json_array_contains_server_id 和 json_array_append 辅助函数处理 JSON 数组操作。AccountResult 枚举定义 SUCCESS、ROLE_ALREADY_EXISTS、IO_ERROR、PARSE_ERROR 四种结果码。

[2026-05-25] [Pattern] [Game Server 账号消息处理]
Description: Game Server 通过 AccountMessage（Gate->Game）接收账号相关请求，通过 AccountMessageResp（Game->Gate）返回响应。handle_account_msg 方法根据 msg_id 分发到不同的处理逻辑：MSG_ID_ACCOUNT_DATA_REQ 查询角色列表，MSG_ID_ACCOUNT_SET_REQ 添加新角色。
Context: Game Server 需要处理 Gate 转发的账号消息。
Action: 在 game_server.cpp 中添加 handle_account_msg 方法，使用 dbmgr_mgr_.send_account_data_req/set_account_set_req 发送异步请求到 DBMgr。回调中构造 AccountDataResp/AccountSetResp，打包为 AccountMessageResp 发送回 Gate。

[2026-05-25] [Pattern] [跨组件消息 ID 同步]
Description: 当多个组件（如 DBMgr 和 Game Server）需要使用相同的消息 ID 常量时，需要在各自的头文件中定义相同的常量值。Game Server 的 dbmgr_msg_ids.h 文件需要与 DBMgr 的 internal_msg_ids.h 保持同步。
Context: Game Server 需要使用 DBMgr 定义的消息 ID（4201-4204）。
Action: 在 game_server/src/dbmgr_msg_ids.h 中添加账号数据操作的消息 ID 常量（MSG_ID_ACCOUNT_DATA_REQ=4201、MSG_ID_ACCOUNT_DATA_RESP=4202、MSG_ID_ACCOUNT_SET_REQ=4203、MSG_ID_ACCOUNT_SET_RESP=4204），确保与 dbmgr/src/internal_msg_ids.h 中的定义一致。

[2026-05-25] [Pattern] [Protobuf 消息生成流程]
Description: 修改 .proto 文件后，需要重新生成对应的 .pb.cc/.pb.h 文件。使用 protoc 命令生成 C++ 和 Python 文件：protoc --cpp_out=generated --python_out=generated xxx.proto。生成的文件需要添加到 CMakeLists.txt 的 PROTO_SRCS 和 PROTO_HDRS 中。
Context: 添加新的 protobuf 消息定义（如 account.proto）。
Action: 在 scripts/common/proto/ 目录下执行 protoc 命令生成文件，然后更新 DBMgr 和 Game Server 的 CMakeLists.txt，将 account.pb.cc/h 添加到编译依赖中。

[2026-05-26] [Pattern] [Gate Server 多 Game 连接管理]
Description: Gate Server 支持连接多个 Game Server，通过 server_id 索引。使用 GameServerConfig 结构体存储配置（server_id、ip、port），game_conns_ map 存储 server_id -> GameConnection 映射。提供 get_game_connection(server_id) 获取指定 Game 连接，get_any_game_connection() 轮询获取任意可用连接。
Context: 实现玩家登录流程，需要根据 server_id 路由消息到不同的 Game Server。
Action: 在 gate_server.h 中添加 GameServerConfig 结构体和 game_conns_ 成员，修改 start() 方法连接所有配置的 Game Server，更新消息回调绑定 server_id。

[2026-05-26] [Pattern] [消息路由按 ID 范围分发]
Description: Gate Server 的消息路由按 msg_id 范围分发：心跳（1-2）直接处理、登录（3-4）处理后通知 Game、账号消息（1000-1999）转发给任意 Game、玩家消息（2000-2999）根据 server_id 路由到指定 Game、内部消息（3000+）转发到 Game。账号消息使用 AccountMessage 包装，玩家消息使用 PlayerMsg 包装。
Context: Gate Server 需要处理多种类型的消息并正确路由。
Action: 在 route_message() 中按 msg_id 范围分发，账号消息调用 forward_account_msg_to_game()，玩家消息解析 server_id 后调用 forward_player_msg_to_game()。

[2026-05-26] [Pattern] [Player ID 生成策略]
Description: Player ID 使用 (server_id << 20) | sequence 格式生成，支持最多 4096 个服务器，每个服务器 1048576 个玩家。PlayerIdGenerator 类使用 mutex 保证线程安全，sequences_ map 存储每个 server_id 的当前序列号。
Context: Game Server 需要在创建角色时生成唯一的 player_id。
Action: 在 player_id_generator.h 中实现 PlayerIdGenerator 类，提供 generate(server_id) 方法。Game Server 持有 PlayerIdGenerator 实例，在处理 CreateRoleReq 时调用。

[2026-05-26] [Pattern] [Session 扩展字段]
Description: Session 类扩展 server_id_ 和 account_id_ 字段，用于记录玩家选择的服务器和登录的账号。server_id_ 用于 PlayerMsg 路由，account_id_ 用于 AccountMsg 响应查找。
Context: Gate Server 需要记录玩家的服务器选择和账号信息。
Action: 在 session.h 中添加 server_id_ 和 account_id_ 成员变量及 getter/setter，在 session.cpp 中初始化为 0 和空字符串。

[2026-05-26] [Pattern] [EnterGameReq 处理流程]
Description: Game Server 处理 EnterGameReq 时，从 DBMgr 加载玩家数据，构造 EnterGameResp 返回。响应通过 GameMessage 包装，包含完整的 PlayerData（player_id、server_id、role_name、level、exp、pos_x、pos_y、created_at）。
Context: 玩家选择角色后进入游戏，需要加载角色数据。
Action: 在 game_server.cpp 中添加 handle_enter_game_req 方法，使用 dbmgr_mgr_.send_player_data_req 加载数据，回调中构造 PlayerData 和 EnterGameResp，打包为 GameMessage 发送回 Gate。

[2026-05-26] [Pattern] [Player 实体数据扩展]
Description: PlayerBizData 结构体扩展为包含完整的游戏业务数据：role_name（角色名）、level（等级）、gold（金币）、experience（经验值）、pos_x/pos_y/pos_z（三维坐标）、scene_id（场景ID）、inventory（背包JSON）、farm_state（农场JSON）、extra_data（扩展JSON）。每个字段都有对应的 getter/setter 方法，setter 在值变化时自动设置 dirty 标记。
Context: Game Server 需要维护玩家的完整业务数据，支持数据加载、保存和运行时修改。
Action: 在 player.h 的 PlayerBizData 结构体中添加新字段，在 Player 类中添加对应的 getter/setter 方法。init_default_data() 设置合理的默认值（level=1、gold=100、scene_id="farm_main"）。

[2026-05-26] [Pattern] [Player 数据 Protobuf 序列化]
Description: PlayerBizData 与 PlayerData protobuf 之间的转换：加载时从 PlayerData proto 解析到 PlayerBizData 结构体，保存时从 PlayerBizData 序列化为 PlayerData proto。使用 PlayerData.ParseFromString() 和 SerializeToString() 进行序列化。
Context: Game Server 需要与 DBMgr 交换玩家数据，DBMgr 存储的是 protobuf 序列化的 PlayerData。
Action: 在 PlayerManager::handle_player_data_loaded() 中使用 farm::PlayerData::ParseFromString() 解析数据，在 save_player_data() 中使用 farm::PlayerData::SerializeToString() 序列化数据。注意处理 protobuf 解析失败的情况。

[2026-05-26] [Pattern] [EnterGameReq PlayerManager 集成]
Description: EnterGameReq 处理改为通过 PlayerManager::add_player_with_data_load() 创建 Player 对象并异步加载数据，而非直接调用 DBMgrConnectionManager。回调中从 Player 对象获取数据构造 EnterGameResp，包含完整的 PlayerData 字段。同时处理 DBMgr 不可用和玩家已存在的情况。
Context: Game Server 需要在玩家进入游戏时创建 Player 对象并加载数据。
Action: 在 handle_enter_game_req 中先检查 DBMgr 连接状态，不可用时回复失败。使用 player_mgr_.add_player_with_data_load() 创建玩家，回调中获取 Player 对象数据构造响应。account 消息通道的 ENTER_GAME_REQ 也使用相同的模式。

[2026-05-27] [Pattern] [JSON 配置文件管理]
Description: 使用 nlohmann/json 库实现服务进程的 JSON 配置文件管理。每个服务有独立的配置文件（config/目录下），包含 server、database、shutdown、pid_file 等配置块。配置文件不存在或格式错误时，服务应输出明确的错误信息并退出。
Context: 实现 Gate、Game、DBMgr 三个服务的配置文件加载功能。
Action: 在 main.cpp 中使用 nlohmann::json 解析配置文件，使用 json_pointer 访问嵌套字段，提供合理的默认值。配置文件路径使用相对路径（相对于可执行文件工作目录）。

[2026-05-27] [Pattern] [PID 文件写入]
Description: 服务进程启动时写入 PID 文件，便于进程管理和监控。PID 文件路径在配置文件中指定（pid_file 字段），默认放在 runtimeData/ 目录下。写入前自动创建父目录，写入后输出日志。
Context: 实现 Gate、Game、DBMgr 三个服务的 PID 文件写入功能。
Action: 在 main.cpp 中实现 write_pid_file() 函数，使用 _mkdir/mkdir 创建目录，ofstream 写入 PID。跨平台兼容 Windows 和 Linux。

[2026-05-27] [Pattern] [统一编译输出目录]
Description: 使用 CMake 的 RUNTIME_OUTPUT_DIRECTORY 属性将所有服务的可执行文件输出到项目根目录的 bin/ 目录。DLL 文件（如 libevent）也复制到 bin/ 目录，确保 exe 运行时能找到依赖。
Context: 统一管理多个服务的编译输出。
Action: 在 CMakeLists.txt 中设置 RUNTIME_OUTPUT_DIRECTORY 为 "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"，build 脚本中添加 DLL 复制逻辑。

[2026-05-27] [Pattern] [管理消息协议设计]
Description: 管理消息使用独立的 ID 范围（5000-5999），与业务消息（1-4999）分离。管理消息在任何连接状态下都可以处理，不依赖身份识别。使用 JSON 格式序列化消息体，便于跨语言解析。
Context: 实现服务的优雅停服功能，需要在 Gate、Game、DBMgr 之间传递停服指令。
Action: 在 common/include/admin_msg_ids.h 中定义管理消息 ID 和消息结构体，使用 nlohmann/json 进行序列化。在各服务的 route_message 中添加 5000-5999 范围的消息路由。

[2026-05-27] [Pattern] [级联停服流程]
Description: 优雅停服采用级联模式：外部工具发送 MSG_ID_SHUTDOWN 到 Gate，Gate 停止接受新连接并转发到 Game，Game 保存玩家数据并转发到 DBMgr，DBMgr 保存数据后发送 MSG_ID_SHUTDOWN_RESP，逐级确认后退出。
Context: 实现多服务架构的优雅停服功能。
Action: 在各服务的 handle_shutdown 方法中：(1) 停止接受新连接；(2) 保存数据（如有）；(3) 向下游服务转发停服消息；(4) 发送响应给上游；(5) 调用 stop() 退出。

[2026-05-27] [Pattern] [工具脚本目录结构]
Description: 将运维工具脚本统一放在 tools/ 目录下，包括 start_all.bat（启动）、stop_graceful.bat（优雅停服）、stop_force.bat（强制停服）、build_all.bat（编译）。脚本使用相对路径访问项目文件，便于在不同环境下使用。
Context: 提供服务的启动、停止、编译等运维功能。
Action: 创建 tools/ 目录，编写 .bat 脚本。start_all.bat 按依赖顺序启动服务（dbmgr -> game_server -> gate_server），stop_graceful.bat 通过 TCP 发送停服消息，stop_force.bat 使用 taskkill 强制终止进程。

[2026-05-27] [Pattern] [PlayerManager 批量保存]
Description: PlayerManager 添加 save_all_players() 方法，遍历所有在线玩家并调用 save_player_data() 保存数据。该方法在优雅停服时调用，确保玩家数据不丢失。
Context: Game Server 收到停服消息时需要保存所有玩家数据。
Action: 在 player_manager.h 中添加 save_all_players() 声明，在 player_manager.cpp 中实现为遍历 players_ 调用 save_player_data()。

[2026-05-27] [Pattern] [DBMgrConnectionManager 广播消息]
Description: DBMgrConnectionManager 添加 broadcast_message() 方法，向所有已识别的 DBMgr 连接发送相同的消息。用于优雅停服时向所有 DBMgr 广播停服指令。
Context: Game Server 需要向所有 DBMgr 发送停服消息。
Action: 在 dbmgr_connection_manager.h 中添加 broadcast_message(msg_id, payload) 声明，在 cpp 中实现为遍历 connections_ 调用 send_to_dbmgr()。

[2026-05-27] [Pitfall] [MSVC spdlog 宏兼容性]
Description: MSVC 的 C++17 标准模式预处理器（/Zc:preprocessor）不支持 ##__VA_ARGS__ 语法来消除空参数的尾部逗号。spdlog 的 SPDLOG_INFO 等宏内部已正确处理此问题，但自定义的包装宏（如 FARM_LOG_INFO）使用 ##__VA_ARGS__ 时会编译失败，报 C2059 语法错误。
Context: 在 MSVC C++17 项目中使用 spdlog，需要定义自定义日志宏来自动添加模块前缀。
Action: 不要定义包装宏，直接使用 spdlog 原生宏（SPDLOG_INFO/SPDLOG_ERROR）。将模块名作为格式化参数的第一个参数：SPDLOG_INFO("[{}] message", LogModule::Gate, args...)。格式模式中不使用 %n（logger name），避免进程名重复。

[2026-05-27] [Pattern] [spdlog 日志系统集成]
Description: 使用 spdlog header-only 库实现统一日志系统。日志格式为 [时间][级别][进程名:PID][模块] 内容。spdlog 模式字符串设置为 "[%Y-%m-%d %H:%M:%S.%e][%^%l%$][process:pid] %v"，模块名在各调用点以 "[ModuleName] message" 格式包含在消息中。使用 rotating_file_sink_mt 实现按大小轮转（20MB/文件，保留7个）。日志级别通过配置文件设置，支持 debug/info/error/critical。
Context: 为 C++ 服务器进程实现统一的日志输出格式和文件轮转。
Action: 创建 log_config.h（配置结构体）、log_init.h/cpp（初始化函数）、log_modules.h（模块常量和 spdlog include）、log_macros.h（spdlog include 入口头文件）。在 main.cpp 中先用默认配置初始化日志，加载配置文件后再重新初始化。
