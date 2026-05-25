[2026-05-23] [Pitfall] [C++ 编译与运行时]
Description: build_cpp14.bat 编译成功后，未将 libevent DLL（event.dll、event_core.dll、event_extra.dll）复制到 exe 所在目录，导致 start /B 后台启动时弹窗报"找不到 event_core.dll"，进程静默退出。start /B 本身返回成功但进程并未存活，测试客户端连接超时。
Context: Windows 环境下使用 CMake + libevent 构建 C++ 服务器，通过 build_cpp14.bat 编译后直接运行 exe。
Action: 编译后必须确认 DLL 已复制到 exe 同目录。在测试脚本中，启动服务器后应先用 netstat 验证端口监听状态，再执行功能测试。不可仅依赖 start /B 的返回值判断进程是否启动成功。

[2026-05-23] [Pitfall] [Windows 后台进程启动]
Description: Windows 下使用 start /B 启动后台进程时，若 exe 依赖的 DLL 缺失，会弹出系统对话框阻塞，start 命令本身仍返回成功码 0。此时进程实际未启动，但脚本误判为成功。
Context: 在 bash shell（Git Bash / MobaXterm）中通过 start /B 启动依赖 DLL 的 Windows 可执行文件。
Action: 启动后台进程后，必须通过 netstat 或 tasklist 验证进程实际存活和端口监听状态，不能仅依赖启动命令的返回值。

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

[2026-05-25] [Pitfall] [消息 ID 定义不一致]
Description: 测试脚本中使用的消息 ID 与服务器 internal_msg_ids.h 中定义的不一致，导致服务器返回 "Unknown msg_id" 错误。proto 文件定义的消息 ID 范围（如 4000-4299）只是范围，具体值需要查看服务器代码。
Context: 测试使用自定义消息 ID 的 Protobuf 协议服务器。
Action: 测试前必须查看服务器的 internal_msg_ids.h 文件，获取准确的消息 ID 常量值，而不是假设或从 proto 文件推断。

[2026-05-25] [Pitfall] [相对路径启动失败]
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
