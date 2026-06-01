cmake_minimum_required(VERSION 3.14)
project(game_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 静态链接运行时（与预编译的 absl/protobuf 匹配）
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 可选依赖: etcd（默认关闭）
# 启用方法: cmake ... -DENABLE_ETCD=ON
# 需要: 1) vcpkg install grpc cpprestsdk  2) 编译安装 etcd-cpp-apiv3
option(ENABLE_ETCD "Enable etcd service discovery" OFF)

# 依赖路径
set(LIBEVENT_ROOT "C:/libevent_install")
set(HIREDIS_ROOT "C:/hiredis_install")
set(PROTOBUF_ROOT "C:/protobuf_install")
set(COMMON_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/proto")

if(ENABLE_ETCD)
    set(ETCD_ROOT "C:/vcpkg/installed/x64-windows" CACHE PATH "etcd-cpp-apiv3 install path")
    set(ETCD_LIB_DIR "C:/etcd-cpp-apiv3_install/lib" CACHE PATH "etcd-cpp-api lib path")
    set(ETCD_INCLUDE_DIR "C:/etcd-cpp-apiv3_install/include" CACHE PATH "etcd-cpp-api include path")
endif()

# Protobuf 生成的源文件
set(PROTO_GENERATED_DIR "${COMMON_PROTO_DIR}/generated")
set(PROTO_SRCS
    ${PROTO_GENERATED_DIR}/base.pb.cc
    ${PROTO_GENERATED_DIR}/internal.pb.cc
    ${PROTO_GENERATED_DIR}/dbmgr.pb.cc
    ${PROTO_GENERATED_DIR}/account.pb.cc
    ${PROTO_GENERATED_DIR}/player.pb.cc
)
set(PROTO_HDRS
    ${PROTO_GENERATED_DIR}/base.pb.h
    ${PROTO_GENERATED_DIR}/internal.pb.h
    ${PROTO_GENERATED_DIR}/dbmgr.pb.h
    ${PROTO_GENERATED_DIR}/account.pb.h
    ${PROTO_GENERATED_DIR}/player.pb.h
)

# 源文件
set(SOURCES
    src/main.cpp
    src/game_server.cpp
    src/gate_session.cpp
    src/player.cpp
    src/player_manager.cpp
    src/message_handler.cpp
    src/dbmgr_connection.cpp
    src/dbmgr_connection_manager.cpp
    src/item_effects.cpp
    src/world_state.cpp
    src/drop_item_manager.cpp
    src/item_interaction_handler.cpp
    src/crop_system.cpp
    src/scene_state.cpp
    src/game_scene_manager.cpp
    src/game_clock.cpp
    src/redis_connection.cpp
    src/login_stub.cpp
    src/online_stub.cpp
    src/admin_handler.cpp
    src/gm_stub.cpp
    src/gm_http_handler.cpp
    ${PROTO_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/admin_msg_ids.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/log_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/message_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/msvc_compat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/server_main_helper.cpp
)

if(ENABLE_ETCD)
    list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/etcd_manager.cpp)
endif()

add_executable(game_server ${SOURCES})

if(ENABLE_ETCD)
    target_compile_definitions(game_server PRIVATE ENABLE_ETCD)
endif()

# Include 目录
set(GAME_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROTOBUF_ROOT}/include
    ${LIBEVENT_ROOT}/include
    ${HIREDIS_ROOT}/include
    ${COMMON_PROTO_DIR}
    ${COMMON_PROTO_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)
if(ENABLE_ETCD)
    list(APPEND GAME_INCLUDE_DIRS ${ETCD_ROOT}/include)
endif()
target_include_directories(game_server PRIVATE ${GAME_INCLUDE_DIRS})

# 链接库目录
set(GAME_LINK_DIRS
    ${PROTOBUF_ROOT}/lib
    ${LIBEVENT_ROOT}/lib
    ${HIREDIS_ROOT}/lib
)
if(ENABLE_ETCD)
    list(APPEND GAME_LINK_DIRS ${ETCD_ROOT}/lib)
endif()
target_link_directories(game_server PRIVATE ${GAME_LINK_DIRS})

# MSVC 特定设置
if(MSVC)
    # 禁用安全警告
    target_compile_definitions(game_server PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NOMINMAX
    )

    # UTF-8 源文件支持 + 静态运行时（匹配预编译的 absl/protobuf）
    target_compile_options(game_server PRIVATE /utf-8 /MT$<$<CONFIG:Debug>:d>)

    # 链接库: protobuf-lite + absl + libevent + Winsock
    # 使用 file(GLOB) 获取所有 absl 库
    file(GLOB ABSL_LIBS "${PROTOBUF_ROOT}/lib/absl_*.lib")
    set(GAME_LINK_LIBS
        libprotobuf-lite.lib
        libutf8_range.lib
        libutf8_validity.lib
        ${ABSL_LIBS}
        event.lib
        event_core.lib
        event_extra.lib
        hiredis.lib
        ws2_32.lib
        advapi32.lib
        shell32.lib
    )
    if(ENABLE_ETCD)
        list(APPEND GAME_LINK_LIBS
            etcd-cpp-api.lib
            grpc.lib
            grpc++.lib
            gpr.lib
            address_sorting.lib
            cares.lib
            libprotobuf.lib
            re2.lib
            ssl.lib
            crypto.lib
            zlib.lib
        )
    endif()
    target_link_libraries(game_server PRIVATE ${GAME_LINK_LIBS})
else()
    # Linux/macOS
    set(GAME_LINK_LIBS
        protobuf
        event
        event_core
        event_extra
        hiredis
        pthread
    )
    if(ENABLE_ETCD)
        list(APPEND GAME_LINK_LIBS
            etcd-cpp-api
            grpc++
            grpc
            gpr
            protobuf
        )
    endif()
    target_link_libraries(game_server PRIVATE ${GAME_LINK_LIBS})
endif()

# 输出目录
set_target_properties(game_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
)

# ========== 测试目标 ==========
# 测试源文件（排除 main.cpp）
set(TEST_SOURCES
    tests/test_game_clock.cpp
    src/game_clock.cpp
    src/game_scene_manager.cpp
    src/gate_session.cpp
    src/player.cpp
    src/player_manager.cpp
    src/message_handler.cpp
    src/dbmgr_connection.cpp
    src/dbmgr_connection_manager.cpp
    src/item_effects.cpp
    src/world_state.cpp
    src/drop_item_manager.cpp
    src/item_interaction_handler.cpp
    src/crop_system.cpp
    src/scene_state.cpp
    src/redis_connection.cpp
    src/login_stub.cpp
    src/online_stub.cpp
    src/admin_handler.cpp
    src/gm_stub.cpp
    src/gm_http_handler.cpp
    ${PROTO_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/admin_msg_ids.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/log_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/message_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/msvc_compat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/server_main_helper.cpp
)

if(ENABLE_ETCD)
    list(APPEND TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/etcd_manager.cpp)
endif()

add_executable(game_tests ${TEST_SOURCES})

if(ENABLE_ETCD)
    target_compile_definitions(game_tests PRIVATE ENABLE_ETCD)
endif()

# Include 目录
set(TEST_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROTOBUF_ROOT}/include
    ${LIBEVENT_ROOT}/include
    ${HIREDIS_ROOT}/include
    ${COMMON_PROTO_DIR}
    ${COMMON_PROTO_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)
if(ENABLE_ETCD)
    list(APPEND TEST_INCLUDE_DIRS ${ETCD_ROOT}/include)
endif()
target_include_directories(game_tests PRIVATE ${TEST_INCLUDE_DIRS})

# 链接库目录
set(TEST_LINK_DIRS
    ${PROTOBUF_ROOT}/lib
    ${LIBEVENT_ROOT}/lib
    ${HIREDIS_ROOT}/lib
)
if(ENABLE_ETCD)
    list(APPEND TEST_LINK_DIRS ${ETCD_ROOT}/lib)
endif()
target_link_directories(game_tests PRIVATE ${TEST_LINK_DIRS})

if(MSVC)
    target_compile_definitions(game_tests PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NOMINMAX
    )
    target_compile_options(game_tests PRIVATE /utf-8 /MT$<$<CONFIG:Debug>:d>)
    file(GLOB ABSL_LIBS_TEST "${PROTOBUF_ROOT}/lib/absl_*.lib")
    set(TEST_LINK_LIBS
        libprotobuf-lite.lib
        libutf8_range.lib
        libutf8_validity.lib
        ${ABSL_LIBS_TEST}
        event.lib
        event_core.lib
        event_extra.lib
        hiredis.lib
        ws2_32.lib
        advapi32.lib
        shell32.lib
    )
    if(ENABLE_ETCD)
        list(APPEND TEST_LINK_LIBS
            etcd-cpp-api.lib
            grpc.lib
            grpc++.lib
            gpr.lib
            address_sorting.lib
            cares.lib
            libprotobuf.lib
            re2.lib
            ssl.lib
            crypto.lib
            zlib.lib
        )
    endif()
    target_link_libraries(game_tests PRIVATE ${TEST_LINK_LIBS})
else()
    set(TEST_LINK_LIBS
        protobuf
        event
        event_core
        event_extra
        hiredis
        pthread
    )
    if(ENABLE_ETCD)
        list(APPEND TEST_LINK_LIBS
            etcd-cpp-api
            grpc++
            grpc
            gpr
            protobuf
        )
    endif()
    target_link_libraries(game_tests PRIVATE ${TEST_LINK_LIBS})
endif()

set_target_properties(game_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
)
