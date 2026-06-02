cmake_minimum_required(VERSION 3.14)
project(friend_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime (matches precompiled absl/protobuf)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Dependency paths
set(LIBEVENT_ROOT "C:/libevent_install")
set(HIREDIS_ROOT "C:/hiredis_install")
set(PROTOBUF_ROOT "C:/protobuf_install")
set(SPDLOG_ROOT "C:/spdlog_install")
set(COMMON_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/proto")

# Protobuf generated source files
set(PROTO_GENERATED_DIR "${COMMON_PROTO_DIR}/generated")
set(PROTO_SRCS
    ${PROTO_GENERATED_DIR}/friend.pb.cc
    ${PROTO_GENERATED_DIR}/base.pb.cc
)
set(PROTO_HDRS
    ${PROTO_GENERATED_DIR}/friend.pb.h
    ${PROTO_GENERATED_DIR}/base.pb.h
)

# Source files
set(SOURCES
    src/main.cpp
    src/friend_service.cpp
    src/game_session.cpp
    ${PROTO_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/log_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/message_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/msvc_compat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/server_main_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../game_server/src/redis_connection.cpp
)

add_executable(friend_service ${SOURCES})

# Include directories
set(FRIEND_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROTOBUF_ROOT}/include
    ${LIBEVENT_ROOT}/include
    ${HIREDIS_ROOT}/include
    ${SPDLOG_ROOT}/include
    ${COMMON_PROTO_DIR}
    ${COMMON_PROTO_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../game_server/src
)
target_include_directories(friend_service PRIVATE ${FRIEND_INCLUDE_DIRS})

# Link directories
set(FRIEND_LINK_DIRS
    ${PROTOBUF_ROOT}/lib
    ${LIBEVENT_ROOT}/lib
    ${HIREDIS_ROOT}/lib
    ${SPDLOG_ROOT}/lib
)
target_link_directories(friend_service PRIVATE ${FRIEND_LINK_DIRS})

# MSVC specific settings
if(MSVC)
    target_compile_definitions(friend_service PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NOMINMAX
    )

    target_compile_options(friend_service PRIVATE /utf-8 /MT$<$<CONFIG:Debug>:d>)

    # Link libraries: protobuf-lite + absl + libevent + hiredis + Winsock
    file(GLOB ABSL_LIBS "${PROTOBUF_ROOT}/lib/absl_*.lib")
    set(FRIEND_LINK_LIBS
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
    target_link_libraries(friend_service PRIVATE ${FRIEND_LINK_LIBS})
else()
    # Linux/macOS
    set(FRIEND_LINK_LIBS
        protobuf
        event
        event_core
        event_extra
        hiredis
        pthread
    )
    target_link_libraries(friend_service PRIVATE ${FRIEND_LINK_LIBS})
endif()

# Output directory
set_target_properties(friend_service PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
)
