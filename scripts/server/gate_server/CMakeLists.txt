cmake_minimum_required(VERSION 3.14)
project(gate_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 静态链接运行时（与预编译的 absl/protobuf 匹配）
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 依赖路径
set(LIBEVENT_ROOT "C:/libevent_install")
set(PROTOBUF_ROOT "C:/protobuf_install")
set(COMMON_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/proto")

# Protobuf 生成的源文件
set(PROTO_GENERATED_DIR "${COMMON_PROTO_DIR}/generated")
set(PROTO_SRCS
    ${PROTO_GENERATED_DIR}/base.pb.cc
    ${PROTO_GENERATED_DIR}/internal.pb.cc
)
set(PROTO_HDRS
    ${PROTO_GENERATED_DIR}/base.pb.h
    ${PROTO_GENERATED_DIR}/internal.pb.h
)

# 源文件
set(SOURCES
    src/main.cpp
    src/gate_server.cpp
    src/session.cpp
    src/session_manager.cpp
    src/message_parser.cpp
    src/game_connection.cpp
    src/msvc_compat.cpp
    ${PROTO_SRCS}
)

add_executable(gate_server ${SOURCES})

# Include 目录
target_include_directories(gate_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROTOBUF_ROOT}/include
    ${LIBEVENT_ROOT}/include
    ${COMMON_PROTO_DIR}
    ${COMMON_PROTO_DIR}/generated
)

# 链接库目录
target_link_directories(gate_server PRIVATE
    ${PROTOBUF_ROOT}/lib
    ${LIBEVENT_ROOT}/lib
)

# MSVC 特定设置
if(MSVC)
    # 禁用安全警告
    target_compile_definitions(gate_server PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NOMINMAX
    )

    # UTF-8 源文件支持 + 静态运行时（匹配预编译的 absl/protobuf）
    target_compile_options(gate_server PRIVATE /utf-8 /MT$<$<CONFIG:Debug>:d>)

    # 链接库: protobuf-lite + absl + libevent + Winsock
    # 使用 file(GLOB) 获取所有 absl 库
    file(GLOB ABSL_LIBS "${PROTOBUF_ROOT}/lib/absl_*.lib")
    target_link_libraries(gate_server PRIVATE
        libprotobuf-lite.lib
        libutf8_range.lib
        libutf8_validity.lib
        ${ABSL_LIBS}
        event.lib
        event_core.lib
        event_extra.lib
        ws2_32.lib
        advapi32.lib
        shell32.lib
    )
else()
    # Linux/macOS
    target_link_libraries(gate_server PRIVATE
        protobuf
        event
        event_core
        event_extra
        pthread
    )
endif()

# 输出目录
set_target_properties(gate_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Release"
)
