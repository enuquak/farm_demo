# Test Knowledge Base

[PROMOTED] [2026-05-23] [Category: Pitfall] [Task Type: Windows C++ Server Testing]
Description: Windows C++ servers using libevent require WSAStartup to be called before any Winsock functions. If main.cpp does not call WSAStartup(), libevent's internal socketpair() will fail silently and the server will not start.
Context: Testing any C++ server on Windows that uses libevent for networking.
Action: Always verify that main.cpp calls WSAStartup(MAKEWORD(2, 2), &wsa_data) before server initialization. If the server fails with "WSAStartup not called" error, add the initialization code.

[2026-05-23] [Category: Pattern] [Task Type: TCP Protocol Testing]
Description: For TCP servers using length-prefixed protocols, sticky packet and split packet tests are essential. Sticky packet = concatenate multiple messages and send in one call. Split packet = break one message into parts and send with delays.
Context: Testing any TCP server with custom binary protocol (e.g., [4B length][4B MsgID][Payload]).
Action: Always include TC-005 (sticky packet) and TC-006 (split packet) in the test plan. Use struct.pack/network byte order for message construction. Verify server correctly reassembles and parses messages.

[PROMOTED] [2026-05-23] [Category: Technique] [Task Type: Protobuf Test Script]
Description: When testing C++ servers with Python clients, generate Python protobuf bindings using protoc --python_out and import them in the test script. Use the same .proto file to ensure protocol compatibility.
Context: Testing any server that uses Protobuf for message serialization.
Action: Generate Python protobuf file with protoc before running tests. Add the generated directory to sys.path. Use SerializeToString()/ParseFromString() for message construction/parsing.

[2026-05-23] [Category: Pitfall] [Task Type: Build Script]
Description: The build_cpp14.bat script has a `pause` at the end that blocks execution when run from non-interactive shells. Use cmake --build . --config Release directly instead of the full script.
Context: Running automated builds in test scripts or CI/CD pipelines.
Action: Instead of calling build_cpp14.bat, run cmake --build . --config Release directly from the build directory. This avoids the interactive pause.

[2026-05-23] [Category: Pattern] [Task Type: Server Pre-check]
Description: Before running functional tests on a TCP server, always verify: (1) exe exists, (2) DLLs are in same directory, (3) port is free, (4) server starts successfully, (5) port is listening. This 5-step pre-check prevents 90% of connection timeout failures.
Context: Testing any TCP/network server.
Action: Follow the pre-check sequence: exe exists -> DLLs present -> port free -> start server -> verify port listening with netstat. Only proceed with tests after all checks pass.
