cmake_minimum_required(VERSION 3.14)
project(cross_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 静态链接运行时（与预编译的 absl/protobuf 匹配）
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 可选依赖: etcd（默认关闭）
option(ENABLE_ETCD "Enable etcd service discovery" OFF)

# 依赖路径
set(LIBEVENT_ROOT "C:/libevent_install")
set(PROTOBUF_ROOT "C:/protobuf_install")
set(COMMON_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/proto")

if(ENABLE_ETCD)
    set(VCPKG_ROOT "C:/vcpkg/installed/x64-windows" CACHE PATH "vcpkg install path")
    set(ETCD_ROOT "C:/etcd-cpp-apiv3_install" CACHE PATH "etcd-cpp-apiv3 install path")
endif()

# Protobuf 生成的源文件
set(PROTO_GENERATED_DIR "${COMMON_PROTO_DIR}/generated")
set(PROTO_SRCS
    ${PROTO_GENERATED_DIR}/cross.pb.cc
    ${PROTO_GENERATED_DIR}/base.pb.cc
    ${PROTO_GENERATED_DIR}/internal.pb.cc
)
set(PROTO_HDRS
    ${PROTO_GENERATED_DIR}/cross.pb.h
    ${PROTO_GENERATED_DIR}/base.pb.h
    ${PROTO_GENERATED_DIR}/internal.pb.h
)

# 源文件
set(SOURCES
    src/main.cpp
    src/cross_server.cpp
    src/game_session.cpp
    src/game_connection.cpp
    src/route_cache.cpp
    ${PROTO_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/log_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/message_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/msvc_compat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/server_main_helper.cpp
)

if(ENABLE_ETCD)
    list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/etcd_manager.cpp)
endif()

add_executable(cross_server ${SOURCES})

if(ENABLE_ETCD)
    target_compile_definitions(cross_server PRIVATE ENABLE_ETCD)
endif()

# Include 目录
set(CROSS_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROTOBUF_ROOT}/include
    ${LIBEVENT_ROOT}/include
    ${COMMON_PROTO_DIR}
    ${COMMON_PROTO_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)
if(ENABLE_ETCD)
    list(APPEND CROSS_INCLUDE_DIRS ${ETCD_ROOT}/include ${VCPKG_ROOT}/include)
endif()
target_include_directories(cross_server PRIVATE ${CROSS_INCLUDE_DIRS})

# 链接库目录
set(CROSS_LINK_DIRS
    ${PROTOBUF_ROOT}/lib
    ${LIBEVENT_ROOT}/lib
)
if(ENABLE_ETCD)
    list(APPEND CROSS_LINK_DIRS ${ETCD_ROOT}/lib ${VCPKG_ROOT}/lib)
endif()
target_link_directories(cross_server PRIVATE ${CROSS_LINK_DIRS})

# MSVC 特定设置
if(MSVC)
    # 禁用安全警告
    target_compile_definitions(cross_server PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NOMINMAX
    )

    # UTF-8 源文件支持 + 静态运行时（匹配预编译的 absl/protobuf）
    target_compile_options(cross_server PRIVATE /utf-8 /MT$<$<CONFIG:Debug>:d>)

    # 链接库: protobuf-lite + absl + libevent + Winsock
    # 使用 file(GLOB) 获取所有 absl 库
    file(GLOB ABSL_LIBS "${PROTOBUF_ROOT}/lib/absl_*.lib")
    set(CROSS_LINK_LIBS
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
    if(ENABLE_ETCD)
        list(APPEND CROSS_LINK_LIBS
            etcd-cpp-api.lib
            grpc.lib
            grpc++.lib
            gpr.lib
            address_sorting.lib
            cares.lib
            libprotobuf.lib
            re2.lib
            libssl.lib
            libcrypto.lib
            cpprest_2_10.lib
            bcrypt.lib
            winhttp.lib
            crypt32.lib
        )
    endif()
    target_link_libraries(cross_server PRIVATE ${CROSS_LINK_LIBS})
else()
    # Linux/macOS
    set(CROSS_LINK_LIBS
        protobuf
        event
        event_core
        event_extra
        pthread
    )
    if(ENABLE_ETCD)
        list(APPEND CROSS_LINK_LIBS
            etcd-cpp-api
            grpc++
            grpc
            gpr
            protobuf
        )
    endif()
    target_link_libraries(cross_server PRIVATE ${CROSS_LINK_LIBS})
endif()

# 输出目录
set_target_properties(cross_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
)
