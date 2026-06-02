cmake_minimum_required(VERSION 3.14)
project(dbmgr_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 静态链接运行时
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 依赖路径（从父项目继承或使用默认值）
if(NOT DEFINED LIBEVENT_ROOT)
    set(LIBEVENT_ROOT "C:/libevent_install")
endif()
if(NOT DEFINED PROTOBUF_ROOT)
    set(PROTOBUF_ROOT "C:/protobuf_install")
endif()
if(NOT DEFINED MONGOC_ROOT)
    set(MONGOC_ROOT "C:/mongoc_install")
endif()
if(NOT DEFINED HIREDIS_ROOT)
    set(HIREDIS_ROOT "C:/hiredis_install")
endif()
if(NOT DEFINED COMMON_PROTO_DIR)
    set(COMMON_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/proto")
endif()

# Protobuf 生成的源文件
set(PROTO_GENERATED_DIR "${COMMON_PROTO_DIR}/generated")
set(PROTO_SRCS
    ${PROTO_GENERATED_DIR}/base.pb.cc
    ${PROTO_GENERATED_DIR}/dbmgr.pb.cc
    ${PROTO_GENERATED_DIR}/account.pb.cc
)
set(PROTO_HDRS
    ${PROTO_GENERATED_DIR}/base.pb.h
    ${PROTO_GENERATED_DIR}/dbmgr.pb.h
    ${PROTO_GENERATED_DIR}/account.pb.h
)

# 源文件
set(SOURCES
    src/main.cpp
    src/dbmgr_server.cpp
    src/game_session.cpp
    src/connection_manager.cpp
    src/mongo_connection.cpp
    src/mongo_server.cpp
    src/redis_server.cpp
    ${PROTO_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/admin_msg_ids.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/log_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/message_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/msvc_compat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/server_main_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/etcd_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/redis_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/redis_async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/redis_cluster.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/mongo_replica_set.cpp
)

add_executable(dbmgr_server ${SOURCES})

# Include 目录
target_include_directories(dbmgr_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROTOBUF_ROOT}/include
    ${LIBEVENT_ROOT}/include
    ${MONGOC_ROOT}/include/libmongoc-1.0
    ${MONGOC_ROOT}/include/libbson-1.0
    ${HIREDIS_ROOT}/include
    ${COMMON_PROTO_DIR}
    ${COMMON_PROTO_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    ${ETCD_ROOT}/include
    ${VCPKG_ROOT}/include
)

# 链接库目录
target_link_directories(dbmgr_server PRIVATE
    ${PROTOBUF_ROOT}/lib
    ${LIBEVENT_ROOT}/lib
    ${MONGOC_ROOT}/lib
    ${HIREDIS_ROOT}/lib
    ${ETCD_ROOT}/lib
    ${VCPKG_ROOT}/lib
)

# MSVC 特定设置
if(MSVC)
    target_compile_definitions(dbmgr_server PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NOMINMAX
    )

    target_compile_options(dbmgr_server PRIVATE /utf-8 /MT$<$<CONFIG:Debug>:d>)

    # 链接库
    file(GLOB ABSL_LIBS "${PROTOBUF_ROOT}/lib/absl_*.lib")
    target_link_libraries(dbmgr_server PRIVATE
        libprotobuf-lite.lib
        libutf8_range.lib
        libutf8_validity.lib
        ${ABSL_LIBS}
        event.lib
        event_core.lib
        event_extra.lib
        mongoc-1.0.lib
        bson-1.0.lib
        hiredis.lib
        ws2_32.lib
        advapi32.lib
        shell32.lib
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
else()
    # Linux/macOS
    target_link_libraries(dbmgr_server PRIVATE
        protobuf
        event
        event_core
        event_extra
        mongoc-1.0
        bson-1.0
        hiredis
        etcd-cpp-api
        grpc++
        grpc
        gpr
        protobuf
        pthread
    )
endif()

# 输出目录
set_target_properties(dbmgr_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
)
