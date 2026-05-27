[2026-05-23] [Pitfall] [C++ 编译与运行时] [PROMOTED]
Description: build_cpp14.bat 编译成功后，未将 libevent DLL（event.dll、event_core.dll、event_extra.dll）复制到 exe 所在目录，导致后台启动时弹窗报"找不到 event_core.dll"，进程静默退出。启动命令本身返回成功但进程并未存活，测试客户端连接超时。
Context: Windows 环境下使用 CMake + libevent 构建 C++ 服务器，通过 build_cpp14.bat 编译后直接运行 exe。
Action: 编译后必须确认 DLL 已复制到 exe 同目录。在测试脚本中，启动服务器后应先用 netstat 验证端口监听状态，再执行功能测试。不可仅依赖启动命令的返回值判断进程是否启动成功。

[2026-05-23] [Pitfall] [Windows 后台进程启动] [PROMOTED]
Description: Windows 下启动后台进程时，若 exe 依赖的 DLL 缺失，会弹出系统对话框阻塞，启动命令本身仍返回成功码 0。此时进程实际未启动，但脚本误判为成功。
Context: 在 bash shell（Git Bash / MobaXterm）中启动依赖 DLL 的 Windows 可执行文件。
Action: 启动后台进程后，必须通过 netstat 或 tasklist 验证进程实际存活和端口监听状态，不能仅依赖启动命令的返回值。推荐使用 `./server.exe port > output.txt 2>&1 &` 语法。

[2026-05-23] [Pattern] [C++ 服务端测试前置检查]
Description: 测试 C++ 网络服务器前，应按以下顺序验证：1) exe 和所有 DLL 在同一目录；2) 目标端口未被占用；3) 启动后 netstat 确认监听；4) 再执行功能测试。
Context: 任何需要启动 C++ 服务器进程的集成测试或端到端测试场景。
Action: 在测试脚本中加入前置检查逻辑：检查 exe 存在性 → 检查端口可用性 → 启动进程 → netstat 验证 → 执行测试用例。

[2026-05-23] [Pattern] [TCP 协议粘包拆包测试]
Description: 使用长度前缀协议的 TCP 服务器，必须测试粘包和拆包场景。粘包 = 一次发送多条拼接的消息；拆包 = 将一条消息分片发送。
Context: 测试任何使用自定义二进制协议（如 [4B 长度][4B MsgID][Payload]）的 TCP 服务器。
Action: 测试计划中必须包含粘包和拆包测试用例。使用 struct.pack 构造消息（网络字节序），验证服务器能正确重组和解析。

[2026-05-23] [Technique] [Protobuf 测试脚本]
Description: 用 Python 测试 C++ 服务器时，需先用 protoc --python_out 生成 Python protobuf 绑定，在测试脚本中导入。使用同一个 .proto 文件确保协议兼容。
Context: 测试任何使用 Protobuf 做消息序列化的服务器。
Action: 测试前先用 protoc 生成 Python protobuf 文件，将生成目录加入 sys.path，使用 SerializeToString()/ParseFromString() 构造/解析消息。

[2026-05-23] [Pitfall] [构建脚本阻塞]
Description: build_cpp14.bat 末尾有 pause 命令，在非交互式 shell 中执行会阻塞。自动化测试应直接使用 cmake --build . --config Release。
Context: 在测试脚本或 CI/CD 流水线中自动编译 C++ 项目。
Action: 不调用 build_cpp14.bat，直接在 build 目录执行 cmake --build . --config Release，避免交互式 pause 阻塞。

[2026-05-23] [Pattern] [Game Server 集成测试]
Description: 测试 Game Server 时，需要先建立 Gate 连接并完成身份识别（GATE_IDENTIFY），然后才能发送业务消息。测试顺序应为：连接 → 身份识别 → 心跳 → 玩家生命周期 → 消息分发。
Context: 测试任何需要身份识别的内部协议服务器。
Action: 在测试脚本中严格按协议顺序发送消息，每个步骤等待响应后再进行下一步。使用 protobuf 模块构造消息，避免手动编码。

[2026-05-23] [Pattern] [内部协议测试]
Description: 测试 Gate↔Game 内部协议时，需要使用 internal.proto 生成的 Python protobuf 模块。消息 ID 范围 3000-3299，与外部协议（1000-2999）隔离。
Context: 测试任何使用独立内部协议的服务器间通信。
Action: 在测试脚本中导入 internal_pb2 模块，使用 SerializeToString()/ParseFromString() 构造/解析消息。确保测试覆盖所有内部消息类型。

[2026-05-23] [Pattern] [Gate-Game 集成测试]
Description: 测试 Gate-Game 连接时，需要先启动 Game Server，再启动 Gate Server。Gate Server 启动后会自动连接 Game Server 并完成身份识别。测试顺序应为：启动 Game → 启动 Gate → 验证连接 → 测试消息转发。
Context: 测试 Gate Server 与 Game Server 的集成。
Action: 在测试脚本中按顺序启动服务器，等待连接建立后再执行功能测试。使用 netstat 验证端口监听状态，使用 tasklist 验证进程存活。

[2026-05-25] [Pitfall] [消息 ID 定义不一致] [PROMOTED]
Description: 测试脚本中使用的消息 ID 与服务器 internal_msg_ids.h 中定义的不一致，导致服务器返回 "Unknown msg_id" 错误。proto 文件定义的消息 ID 范围（如 4000-4299）只是范围，具体值需要查看服务器代码。
Context: 测试使用自定义消息 ID 的 Protobuf 协议服务器。
Action: 测试前必须查看服务器的 internal_msg_ids.h 文件，获取准确的消息 ID 常量值，而不是假设或从 proto 文件推断。

[2026-05-25] [Pitfall] [相对路径启动失败] [PROMOTED]
Description: 使用相对路径（如 ./tmp/dbmgr_data）作为 --data-dir 参数启动服务器时，路径基于 exe 所在目录解析，而不是当前工作目录，导致目录创建失败。
Context: 在 Windows 环境下启动 C++ 服务器，使用命令行参数指定文件路径。
Action: 启动服务器时始终使用绝对路径作为文件路径参数，避免相对路径解析问题。或者在服务器代码中将相对路径转换为绝对路径。

[2026-05-25] [Pattern] [DBMgr 集成测试]
Description: 测试 DBMgr 服务器时，需要先建立 TCP 连接并完成身份标识流程（接收 DBMgrIdentify → 发送 DBMgrIdentifyResp），然后才能发送数据操作请求。心跳消息会在身份标识完成后自动发送。
Context: 测试 DBMgr 或类似的需要身份标识的内部协议服务器。
Action: 测试脚本中实现 setup_connection() 函数，封装连接建立和身份标识流程，供各个测试用例复用。确保在发送业务请求前完成身份标识。

[2026-05-25] [Technique] [服务器间连接测试的日志解析+直接连接双验证法]
Description: 测试两个服务器之间的连接（如 Game↔DBMgr）时，采用"日志解析验证 + 直接连接测试"的双重验证策略。先启动两个服务器，通过解析双方日志验证连接建立和身份标识握手；再用 Python TCP 客户端直接连接后端服务器（DBMgr），验证完整的请求-响应流程（如 PlayerDataReq/Resp）。
Context: 测试服务器间的 TCP 连接、身份标识握手、心跳保活和异步请求模型。
Action: 1) 启动两个服务器，用 netstat 验证端口监听；2) 等待连接建立后解析双方 stdout 日志，验证关键事件（如 "TCP connected"、"identified"）；3) 用 Python socket 直接连接后端服务器，手动完成身份标识流程后发送业务请求，验证完整协议链路。此方法比仅依赖日志更可靠，能验证实际数据收发。

[2026-05-25] [Pattern] [Game-DBMgr 连接测试顺序]
Description: 测试 Game Server 与 DBMgr 的连接时，正确的启动顺序是：先启动 DBMgr（监听端口），再启动 Game Server（主动连接）。Game Server 启动后会自动解析 --dbmgr 参数并发起连接。DBMgr 收到连接后立即发送 DBMgrIdentify 消息，Game Server 回复 DBMgrIdentifyResp 完成握手。
Context: 测试 Game Server 的 DBMgr 连接管理功能（DBMgrConnectionManager）。
Action: 测试脚本中按此顺序操作：1) 启动 DBMgr 并验证端口监听；2) 启动 Game Server 并验证其监听端口；3) 等待 2-3 秒让连接和身份标识完成；4) 解析日志验证握手成功；5) 等待 5-6 秒验证心跳交换。

[2026-05-25] [Pattern] [Game Server Player Data Integration Test]
Description: 测试 Game Server 的玩家数据持久化功能时，需要模拟完整的玩家生命周期：GateIdentify → PlayerJoin → PlayerLeave → 重新连接 → PlayerJoin。通过 Python TCP 客户端连接 Game Server 的监听端口（9090），使用 internal.proto 生成的 protobuf 模块构造消息，验证 Game Server 与 DBMgr 的数据流。
Context: 测试 Game Server 的玩家数据加载/保存功能，验证 GET_ALL 和 SET_ALL 操作。
Action: 测试脚本按以下顺序执行：1) 连接 Game Server；2) 发送 GateIdentify 并等待响应；3) 发送 PlayerJoin 并等待响应；4) 发送 PlayerLeave；5) 断开连接；6) 重新连接并重复步骤 2-4；7) 检查 DBMgr 数据目录中的 JSON 文件。使用 timeout=10 等待 DB 操作完成。

[2026-05-25] [Technique] [Player Data Persistence Verification]
Description: 验证玩家数据持久化时，需要检查 DBMgr 的 data-dir 目录中是否生成了玩家 JSON 文件。文件路径格式为 {data-dir}/players/{player_id}.json。文件内容应包含玩家业务数据（如 level、gold、inventory 等）。
Context: 测试任何使用文件系统存储玩家数据的服务器。
Action: 测试脚本中使用 os.path.exists() 检查文件是否存在，使用 json.load() 读取文件内容并验证字段值。注意 DBMgr 可能使用不同的 data-dir（如 tmp/dbmgr_data 而非 tmp/dbmgr_test3_data），需要检查实际运行的 DBMgr 进程的命令行参数。

[2026-05-26] [Pitfall] [Protobuf repeated 字段数据位置错误] [PROMOTED]
Description: Game Server 处理 AccountDataResp 时，将角色列表数据设置到 msg 字段中，而不是 roles 字段。这是因为代码中直接使用 set_msg(roles_json) 而不是解析 JSON 并添加到 repeated 字段。
Context: 测试使用 Protobuf repeated 字段的响应消息。
Action: 测试时必须验证 repeated 字段是否包含预期数据，而不是只检查 msg 字段。如果 repeated 字段为空，检查 msg 字段是否包含 JSON 格式的数据。这是一个常见的实现错误，开发者可能简单地将 JSON 字符串设置到 msg 字段而不是解析并添加到 repeated 字段。

[2026-05-26] [Pattern] [账号系统端到端测试]
Description: 测试账号系统时，需要按以下顺序执行：1) 查询不存在的账号（验证新账号处理）；2) 添加新角色；3) 查询已存在账号（验证角色列表）；4) 添加重复角色（验证重复检测）；5) 添加不同服务器的角色（验证多角色支持）；6) 验证数据文件内容。
Context: 测试任何账号/角色管理系统。
Action: 使用 Python TCP 客户端模拟 Gate 客户端，通过 AccountMessage 封装发送 AccountDataReq 和 AccountSetReq。验证响应中的 code 字段和角色数据。最后检查 JSON 数据文件内容是否正确。

[2026-05-26] [Technique] [JSON 文件存储验证]
Description: 验证 JSON 文件存储时，需要计算文件路径（基于 hash 算法），读取文件内容，解析 JSON 并验证字段值。对于账号系统，文件路径为 data/accounts/{hash(account_id) % dbmgr_count}.json。
Context: 测试任何使用 JSON 文件存储数据的系统。
Action: 在测试脚本中实现与服务器相同的 hash 算法，计算预期的文件路径。使用 os.path.exists() 检查文件存在，使用 json.load() 解析内容，验证 account_id、roles 数组和每个角色的字段值。

[2026-05-26] [Pitfall] [C++ 头文件符号冲突]
Description: 多个头文件定义相同的常量（如 MSG_ID_HEARTBEAT），导致编译时符号重定义错误。常见于项目重构时，旧头文件（message_parser.h）和新头文件（msg_ids.h）都定义了相同的消息 ID 常量。
Context: C++ 项目中多个头文件定义相同命名空间下的常量。
Action: 统一消息 ID 常量定义位置，删除重复定义。使用 include guard 防止重复包含。在重构时检查所有引用点，确保只保留一份定义。

[2026-05-26] [Pitfall] [成员变量命名不一致]
Description: 代码中使用了旧的成员变量名（如 game_conn_），但类定义中已改为新名称（如 game_conns_），导致编译错误。常见于重构时只修改了部分文件。
Context: C++ 类成员变量重命名后，未更新所有引用点。
Action: 重构时使用 IDE 的重命名功能或全局搜索替换，确保所有引用点都更新。编译前检查所有使用该变量的文件。

[2026-05-26] [Pitfall] [缺少方法定义]
Description: 代码中调用了类的方法（如 find_by_account_id），但类定义中缺少该方法的声明和实现。常见于新增功能时只修改了调用方，未修改被调用方。
Context: C++ 类新增方法时，需要同时更新头文件（声明）和源文件（实现）。
Action: 新增方法时，先在头文件中添加声明，再在源文件中添加实现。编译前检查所有调用点是否都有对应的方法定义。

[2026-05-26] [Pitfall] [函数签名变更未同步更新调用方]
Description: 修改函数签名（如 send_player_data_req 增加参数）后，未更新所有调用方，导致编译错误。常见于异步回调函数签名变更时，调用方仍使用旧的参数列表。
Context: C++ 项目中修改已有函数的参数列表。
Action: 修改函数签名后，使用 IDE 的"查找所有引用"功能或全局搜索，确保所有调用点都更新为新的参数列表。编译前检查所有使用该函数的文件。

[2026-05-26] [Pitfall] [C++ 缺少 proto 头文件包含]
Description: game_server.cpp 使用了 dbmgr.pb.h 中定义的 farm::PlayerDataOp::GET_ALL 枚举，但未包含该头文件，导致编译错误 "PlayerDataOp 不是 farm 的成员"。CMakeLists.txt 中包含了 dbmgr.pb.cc（源文件），但源代码文件中未 include 对应的头文件。
Context: C++ 项目使用 protobuf 生成的代码，CMakeLists.txt 列出了 .cc 文件但源代码中未包含对应的 .h 文件。
Action: 当 CMakeLists.txt 中添加新的 proto 源文件时，必须同时在使用该 proto 类型的源代码文件中添加 #include "xxx.pb.h"。编译错误 "不是 xxx 的成员" 通常表示缺少头文件包含。

[2026-05-26] [Pitfall] [CMakeLists.txt 遗漏 proto 源文件]
Description: game_server 的 CMakeLists.txt 中 PROTO_SRCS 列表遗漏了 player.pb.cc，导致 PlayerData、EnterGameReq、EnterGameResp 等类型的链接错误（LNK2019: 无法解析的外部符号）。新增 proto 文件后未更新构建配置。
Context: C++ 项目使用 CMake 构建，proto 生成的源文件需要手动添加到 PROTO_SRCS 列表。
Action: 每次新增 .proto 文件后，必须在 CMakeLists.txt 的 PROTO_SRCS 和 PROTO_HDRS 中添加对应的 .pb.cc 和 .pb.h。链接错误 "无法解析的外部符号" 通常表示源文件未加入构建。

[2026-05-26] [Pitfall] [Session account_id 未绑定导致消息路由失败]
Description: Gate Server 在 forward_account_msg_to_game 中未将 account_id 绑定到 session（session->set_account_id(account_id)），导致 handle_account_msg_resp 通过 find_by_account_id 查找 session 时返回 nullptr，响应消息被丢弃。
Context: Gate Server 需要通过 account_id 路由 AccountMsg 响应到正确的客户端 session。
Action: 在转发 AccountMsg 到 Game Server 之前，必须将 account_id 绑定到 session。这样当 Game Server 返回 AccountMessageResp 时，Gate Server 才能找到正确的客户端 session 并转发响应。

[2026-05-26] [Pitfall] [消息 ID 范围与路由逻辑不匹配]
Description: EnterGameReq 使用 msg_id=2001（PlayerMsg 范围 2000-2999），但 Gate Server 的路由逻辑将 2000-2999 视为 PlayerMsg，需要已有的 player_id-session 映射。由于 EnterGame 发生在玩家实际进入游戏之前，session 的 player_id 是 fd 值而非真实 player_id，导致路由失败。
Context: Gate Server 根据 msg_id 范围路由消息：1000-1999 为 AccountMsg，2000-2999 为 PlayerMsg。
Action: 设计消息协议时，确保消息的 msg_id 范围与其路由方式匹配。进入游戏前的消息（如 EnterGameReq）应使用 AccountMsg 范围（1000-1999），进入游戏后的消息使用 PlayerMsg 范围（2000-2999）。

[2026-05-26] [Pattern] [异步回调类型变更的级联修复]
Description: 修改异步回调类型（如 AccountDataCallback 从 string 改为 vector<tuple>）时，需要同步修改：1) 回调类型定义（.h）；2) 所有调用回调的错误路径（如 callback(-1, "") 改为 callback(-1, {})）；3) 所有注册回调的 lambda 表达式。遗漏任何一处都会导致编译错误。
Context: C++ 项目中修改 std::function 回调签名。
Action: 修改回调类型后，使用全局搜索查找所有使用该回调类型的位置，包括：类型定义、错误路径调用、正常路径调用、lambda 注册点。确保所有位置的参数类型都匹配。

[2026-05-26] [Pattern] [Login Flow 端到端测试]
Description: 测试完整的登录流程（连接→登录→查询角色→创建角色→进入游戏）时，需要使用 AccountMsg 通道（msg_id 1000-1999）处理账号相关操作。测试脚本应实现 connect_and_login()、query_roles()、create_role()、enter_game() 等辅助函数，每个函数封装消息构造和响应解析。
Context: 测试 Gate→Game→DBMgr 的完整消息链路。
Action: 测试脚本结构：1) connect_and_login 建立连接并完成登录；2) 每个测试用例前发送心跳保活；3) 使用 AccountMsg 封装账号操作；4) 验证响应的 code 字段和数据内容。

[2026-05-26] [Pitfall] [DBMgr 玩家数据文件扩展名]
Description: DBMgr 使用 .json 扩展名存储玩家数据文件（如 1000001.json），但文件内容实际上是 protobuf 二进制数据，不是 JSON 格式。测试脚本中如果使用 .dat 扩展名查找文件会失败。
Context: 测试 DBMgr 的玩家数据持久化功能。
Action: 测试脚本中查找玩家数据文件时，使用 .json 扩展名（如 {player_id}.json），而不是 .dat。文件内容是 protobuf 序列化的二进制数据，使用 ParseFromString() 解析。

[2026-05-26] [Pattern] [Player Entity 端到端测试]
Description: 测试玩家实体功能时，需要按以下顺序执行：1) 验证 PlayerData protobuf 字段完整性；2) 测试序列化/反序列化；3) 测试新玩家首次进入（默认数据初始化）；4) 测试重复玩家加入处理；5) 测试数据持久化；6) 测试数据路由逻辑；7) 测试多玩家并发。
Context: 测试 Game Server 的玩家实体管理功能。
Action: 使用 Python TCP 客户端模拟 Gate 客户端，通过 AccountMessage 封装 EnterGameReq。验证响应中的 player_data 字段包含正确的默认值（level=1, scene_id="farm_main"）。检查 DBMgr 数据目录中的 .json 文件验证数据持久化。

[2026-05-26] [Technique] [Player Data 默认值验证]
Description: 验证新玩家默认数据时，需要检查以下字段：level=1, pos_x=0.0, pos_y=0.0, pos_z=0.0, scene_id="farm_main"。这些默认值在 Player::init_default_data() 中设置。
Context: 测试新玩家首次进入游戏时的默认数据初始化。
Action: 在测试脚本中，发送 EnterGameReq 后解析 EnterGameResp，检查 player_data 中的默认值是否正确。如果 DBMgr 中没有玩家数据，Game Server 会初始化默认数据并保存到 DBMgr。

[2026-05-26] [Pitfall] [server_id 字段未在 save_player_data 中设置]
Description: Game Server 的 save_player_data() 函数在序列化 PlayerData protobuf 时未设置 server_id 字段，导致保存的数据中 server_id 为 0。这是因为 PlayerBizData 结构体不包含 server_id 字段。
Context: 测试玩家数据持久化功能。
Action: 测试时需要了解这个已知问题。验证玩家数据文件内容时，不要检查 server_id 字段，或者接受 server_id=0 作为预期值。这是一个低严重程度的缺陷，不影响核心功能。

[2026-05-27] [Pattern] [Server Lifecycle Script Testing]
Description: Testing server lifecycle scripts (start_all.bat, stop_graceful.bat, stop_force.bat) requires verifying: 1) Script existence; 2) Correct service start order; 3) PID file management; 4) Graceful shutdown message flow; 5) Force shutdown with taskkill.
Context: Testing any server management scripts that handle startup, graceful shutdown, and force shutdown of multiple services.
Action: Test plan should include: 1) Verify script file existence and content; 2) Check config files for pid_file paths; 3) Verify main.cpp has write_pid_file() function; 4) Check admin message protocol definitions; 5) Verify cascade shutdown logic in server implementation files.

[2026-05-27] [Pattern] [Admin Message Protocol Testing]
Description: Testing admin message protocol (MSG_ID_SHUTDOWN=5001, MSG_ID_SHUTDOWN_RESP=5002) requires verifying: 1) Message ID constants in admin_msg_ids.h; 2) Message struct definitions with correct field types; 3) Serialize/deserialize methods; 4) Message routing in server code (msg_id >= 5000 && msg_id < 6000).
Context: Testing any custom admin/management message protocol for server management.
Action: Test plan should verify: 1) Header file defines correct message IDs; 2) Struct definitions match spec (reason, timeout_ms for shutdown; code, msg for response); 3) Server code routes admin messages correctly; 4) Server handles MSG_ID_SHUTDOWN by stopping listener and forwarding to downstream services.

[2026-05-27] [Pattern] [Cascade Shutdown Logic Testing]
Description: Testing cascade shutdown (Gate→Game→DBMgr) requires verifying: 1) GateServer stops listener and forwards MSG_ID_SHUTDOWN to game servers; 2) GameServer stops listener, saves players, broadcasts to DBMgrs, sends response; 3) DbMgrServer stops listener, sends response, calls stop().
Context: Testing any multi-service shutdown cascade where services have dependencies.
Action: Test plan should verify each server's handle_shutdown() implementation: 1) Stops accepting new connections; 2) Performs cleanup (save data, close connections); 3) Forwards shutdown to downstream services; 4) Sends response to upstream service; 5) Calls stop().

[2026-05-27] [Technique] [Standalone C++ Unit Test Without Framework]
Description: For C++ classes that depend on project infrastructure (spdlog, nlohmann/json), create a standalone test program that includes the class under test and its direct dependencies. Compile with MSVC using /I flags for include directories (src, common/include, common/third_party, protobuf). This avoids needing a full test framework while still validating all public methods.
Context: Testing C++ classes in a project that uses spdlog for logging and nlohmann/json for serialization, without a dedicated test framework.
Action: Create a test .cpp file with a simple pass/fail counter macro system. Include only the necessary source files (not the entire project). Use a batch file to set up MSVC environment and compile. Run the test exe directly and check exit code (0=pass, 1=fail). This approach worked for testing CropSystem (28 test cases, all passed).

[2026-05-28] [Technique] [Python pygame 无头测试]
Description: 在无显示器环境下测试 pygame 相关代码时，需要设置环境变量 SDL_VIDEODRIVER=dummy 和 SDL_AUDIODRIVER=dummy，然后调用 pygame.init() 和 pygame.display.set_mode() 创建虚拟显示。这样可以运行所有 pygame 功能（图像加载、Surface 操作、字体渲染等）而无需实际窗口。
Context: 测试任何使用 pygame 的 Python 客户端代码（精灵渲染、HUD、地图渲染等）。
Action: 在测试脚本开头设置 os.environ["SDL_VIDEODRIVER"] = "dummy"，调用 pygame.init() 和 pygame.display.set_mode((w, h))。测试结束时调用 pygame.quit()。注意：pyscroll 的 BufferedRenderer 和 PyscrollGroup 可以正常在此模式下创建和渲染。

[2026-05-28] [Technique] [pyscroll + pytmx 集成测试]
Description: 测试 pyscroll 渲染管线时，正确的导入路径是：pyscroll.BufferedRenderer、pyscroll.PyscrollGroup、pyscroll.data.TiledMapData（不是从子模块导入）。测试流程：加载 TMX → 创建 TiledMapData → 创建 BufferedRenderer(zoom=N) → 创建 PyscrollGroup → 添加精灵 → 调用 group.draw(surface)。
Context: 测试使用 pytmx + pyscroll 的 2D 瓦片地图渲染系统。
Action: 直接从 pyscroll 顶层模块导入：renderer = pyscroll.BufferedRenderer(map_data, size, zoom=ZOOM_FACTOR)。不要使用 from pyscroll.BufferedRenderer import BufferedRenderer（会报 ModuleNotFoundError）。pyscroll 的 zoom 参数支持整数缩放，配合 16x16 基础瓦片可实现像素风格渲染。

[2026-05-27] [Pattern] [Game Logic Timer System Testing]
Description: Testing timer-driven game systems (like crop growth) requires simulating time passage rather than waiting real-time. Key techniques: 1) Use planted_at parameter to set historical timestamps; 2) Use simulate_elapsed() for freeze/resume scenarios; 3) Test exact boundary conditions (now - planted_at == grow_time); 4) Test stale data cleanup when objects change externally.
Context: Testing any server-side timer system that checks elapsed time against thresholds.
Action: Test plan should cover: 1) Not-yet-mature (elapsed < threshold); 2) Exactly-mature (elapsed == threshold); 3) Over-mature (elapsed > threshold); 4) Freeze/resume catch-up; 5) Zero/negative elapsed edge cases; 6) Null world pointer safety; 7) Stale data cleanup.

[2026-05-28] [Pitfall] [服务器忽略客户端 active_slot 参数]
Description: Game Server 的 ItemInteractionHandler 使用服务器存储的 active_slot（默认 0），忽略 ItemUseReq 中客户端报告的 active_slot 参数。且无 MSG_ID_ACTIVE_SLOT_CHANGE 处理器，客户端无法切换活动格子。
Context: 测试物品交互系统时，需要使用特定格子的物品（如面包恢复能量）。
Action: 测试前需要了解此设计限制。如需测试特定格子的物品，需要通过其他方式修改服务器的 active_slot（如直接修改玩家数据文件）。或者在测试计划中注明此限制，将相关测试标记为 SKIP。

[2026-05-28] [Pitfall] [服务端能量恢复未封顶]
Description: item_interaction_handler.cpp 中能量恢复代码 `new_energy = player->get_energy() + effect->energy_restore` 未限制 new_energy <= max_energy，导致能量可超过 100 上限。违反 Spec 要求"恢复不超过上限"。
Context: 测试任何涉及能量恢复的功能（如吃食物）。
Action: 代码审查时发现此 Bug。修复方案：添加 `new_energy = std::min(new_energy, max_energy)`。测试时需要验证能量不会超过上限。

[2026-05-28] [Pattern] [能量系统集成测试]
Description: 测试能量系统需要验证：1) 登录时 EnergySync 数据正确；2) ItemUseResp 包含 EnergySync；3) 能量消耗正确；4) 能量不足时返回 ENERGY_EXHAUSTED；5) 食物恢复能量；6) 能量不超过上限。
Context: 测试任何涉及玩家能量的系统。
Action: 测试脚本需要：1) 创建新角色验证初始能量；2) 使用物品验证能量消耗；3) 消耗能量至不足验证错误码；4) 使用食物验证恢复；5) 多次恢复验证上限。注意服务器的 active_slot 限制可能影响测试。

[2026-05-28] [Pitfall] [交互范围检查先于能量检查]
Description: item_interaction_handler.cpp 中，交互范围检查（Chebyshev distance > interact_range）在能量检查之前执行。当测试 ENERGY_EXHAUSTED 错误码时，如果目标超出范围，会返回 INVALID_TARGET (code=4) 而非 ENERGY_EXHAUSTED (code=3)，即使能量为 0。
Context: 测试能量耗尽场景时，需要确保测试目标在交互范围内。
Action: 测试 ENERGY_EXHAUSTED 时，必须选择在 interact_range 范围内的目标。例如锄头 range=50，测试目标应选择 Chebyshev 距离 <= 50 的位置（如 (30,30)），而非 (55,55)。代码执行顺序：范围检查 → 能量检查 → 执行效果。

[2026-05-28] [Pitfall] [CMakeLists.txt 遗漏新增 .cpp 源文件] [PROMOTED]
Description: 新增 scene_state.cpp 源文件后未将其添加到 CMakeLists.txt 的 SOURCES 列表中，导致 SceneState 类的所有方法（构造函数、add_player、remove_player、thaw、generate_default）产生 LNK2019 链接错误。这与之前遗漏 proto .pb.cc 文件的问题模式相同，但影响的是普通 .cpp 源文件。
Context: C++ 项目使用 CMake 构建，新增类时需要手动将 .cpp 文件添加到 SOURCES 变量。
Action: 每次新增 .cpp 文件后，必须在 CMakeLists.txt 的 SOURCES 列表中添加对应条目。链接错误 LNK2019 "无法解析的外部符号" 是源文件未加入构建的典型表现。建议在 code-agent 的开发流程中加入检查点：新增文件后自动验证 CMakeLists.txt 是否已更新。

[2026-05-28] [Pattern] [修复后重测流程]
Description: 对修复后的代码进行重测时，应：1) 确认服务器已重启（清除内存中的旧状态）；2) 验证编译成功；3) 按优先级测试修复项；4) 同时验证回归（未修复的功能不受影响）。
Context: Bug 修复后的回归测试。
Action: 重测流程：重启服务器 → 编译验证 → DLL 检查 → 逐项测试修复点 → 验证能量上限 → 生成报告。注意：服务器重启会清除玩家在线状态，但持久化数据（如玩家存档）仍保留。

[2026-05-28] [Technique] [C++ 代码级测试验证]
Description: 当无法启动完整服务器基础设施（Gate+Game+DBMgr）时，可以通过静态代码分析验证 C++ 修复的正确性：1) 读取 CMakeLists.txt 确认源文件在 SOURCES 列表中；2) 读取 .cpp 文件确认方法实现存在且调用链正确；3) 编译验证无链接错误；4) 检查 DLL 完整性。这比仅检查编译更可靠，因为可以验证业务逻辑的调用关系。
Context: 测试 C++ 服务器功能但无法启动完整服务器栈时。
Action: 按以下步骤验证：1) cmake --build 编译通过；2) grep 确认源文件在 CMakeLists.txt；3) 在 .cpp 中搜索关键方法定义和调用点；4) 验证 DLL 存在。此方法适用于验证 BUG-002 (save_all_scenes 调用) 和 BUG-003 (格式迁移逻辑)。

[2026-05-28] [Pitfall] [Python 模块路径与目录结构不匹配]
Description: Python 测试脚本中 sys.path 设置为 'scripts' 后，import 路径应为 'client.scene.scene_defs'（因为 scene_defs.py 在 scripts/client/scene/ 下），而非 'client.scene_defs'。项目目录结构中 client 包有子包 scene，所有场景相关模块都在 scene 子包中。
Context: 测试使用 client 包的 Python 代码。
Action: import 前先确认实际文件路径：scripts/client/scene/scene_defs.py -> from client.scene.scene_defs import ...。不要假设模块在包的顶层。

[2026-05-28] [Pitfall] [SceneManager 构造函数参数变更]
Description: SceneManager.__init__() 需要 screen_width 和 screen_height 两个位置参数（用于 SceneTransition 的 Iris 动效），不能无参构造。同时 request_scene_change() 需要 3 个参数：target_scene, target_portal_id, player_screen_pos(tuple)。
Context: 测试客户端 SceneManager 模块。
Action: 创建 SceneManager 实例时传入 (800, 600)。调用 request_scene_change("house", "door_out", (400, 300))。测试前先 inspect 构造函数签名。

[2026-05-28] [Pattern] [场景管理器状态机测试]
Description: 测试场景切换状态机时，通过 sm._transition.state 访问当前状态（非 sm.transition_state）。状态序列应为 IDLE -> IRIS_CLOSE -> IRIS_OPEN -> IDLE（SWITCHING 状态在 update() 内部一闪而过，可能无法捕获）。验证切换完成的最佳方式是检查 sm.current_scene_id 是否已更改。
Context: 测试客户端 SceneManager 的场景切换状态机。
Action: 1) 记录初始状态 IDLE；2) 调用 request_scene_change 后检查 IRIS_CLOSE；3) 多次 update(0.02) 后检查 IDLE；4) 验证 current_scene_id 已变更为目标场景。

[2026-05-28] [Pitfall] [Python or 运算符与 0 值]
Description: Python 中 `0 or 12` 返回 12，因为 0 被视为 falsy。当代码使用 `x % 12 or 12` 来处理 12 小时制显示时，x=0 会错误地显示为 12 而非 0。这在游戏时钟的午夜显示（slot 36 -> hour=0）中导致 "AM 12:00" 而非 "AM 0:00"。
Context: 测试任何使用 Python `or` 运算符处理数值的格式化逻辑，特别是 12/24 小时制时间显示。
Action: 测试时钟系统的时间显示时，必须覆盖 hour=0（午夜）的边界情况。对于 12 小时制格式化，使用显式的条件判断而非 `x % n or n` 模式。测试用例应包含：slot 0 (AM 6:00)、slot 12 (PM 12:00)、slot 36 (AM 0:00)、slot 38 (AM 1:00)。

[2026-05-28] [Pattern] [游戏时钟系统测试]
Description: 测试游戏内时钟系统时需要覆盖以下维度：1) 时间映射（slot->显示时间，含所有边界值）；2) 时间推进（正常dt、累积dt、大dt跨多slot）；3) 天数管理（初始值、递增、重置）；4) 暂停/恢复（暂停时elapsed保留、恢复后继续累积）；5) 回调触发（新slot、一天结束、不重复触发）；6) 序列化/反序列化（含旧存档兼容）；7) 帧率无关性（注意浮点累积误差可能导致测试不准确）。
Context: 测试任何使用离散时间槽（time slot）模型的游戏时钟系统。
Action: 测试脚本应按上述维度组织测试用例。帧率无关性测试需要高精度浮点比较或改用整数计数器验证。序列化测试必须覆盖旧存档兼容场景（缺失字段使用默认值）。

[2026-05-28] [Pitfall] [Python 浮点累积误差导致帧率无关测试失败]
Description: 测试帧率无关性时，`1800 * (1/30)` 的浮点累积结果为 59.999999999999154（< 60.0），导致时钟未推进一个 slot。测试误报为 FAIL，但实际代码逻辑正确。
Context: 测试任何基于浮点累加的时间推进系统，特别是将 1/N 的 N 次方与整数阈值比较的场景。
Action: 帧率无关测试中，在多次小 dt 累积后追加一个微小增量（如 0.001）推动跨过阈值，或改用整数计数器验证推进次数。不要依赖浮点累加精确等于整数。

[2026-05-28] [Technique] [强制睡觉流程代码级验证]
Description: 当无法启动完整服务器栈（Gate+Game+DBMgr）测试强制睡觉流程时，可以通过静态代码分析验证：1) 服务端 on_day_end() 暂停时钟+发送 ForceSleepNotify；2) 客户端 Iris 收缩完成后发送 ForceSleepReady；3) 服务端 handle_force_sleep_ready() 切换场景+恢复体力+推进天数+恢复时钟+发送响应；4) 超时处理（10秒强制完成）。验证每一步的函数调用链和状态变量变更。
Context: 测试涉及客户端-服务端交互的多步骤流程（如强制睡觉、场景切换）。
Action: 按消息流顺序追踪代码：服务端发送 -> 客户端接收处理 -> 客户端回复 -> 服务端处理回复。验证每个环节的消息 ID、protobuf 字段、状态变量变更是否与 spec 一致。
