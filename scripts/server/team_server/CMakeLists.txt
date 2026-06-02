cmake_minimum_required(VERSION 3.14)
project(team_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static runtime (matching precompiled absl/protobuf)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Dependency paths (inherited from parent project or defaults)
if(NOT DEFINED LIBEVENT_ROOT)
    set(LIBEVENT_ROOT "C:/libevent_install")
endif()
if(NOT DEFINED PROTOBUF_ROOT)
    set(PROTOBUF_ROOT "C:/protobuf_install")
endif()
if(NOT DEFINED HIREDIS_ROOT)
    set(HIREDIS_ROOT "C:/hiredis_install")
endif()
if(NOT DEFINED COMMON_PROTO_DIR)
    set(COMMON_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common/proto")
endif()

# Protobuf generated sources
set(PROTO_GENERATED_DIR "${COMMON_PROTO_DIR}/generated")
set(PROTO_SRCS
    ${PROTO_GENERATED_DIR}/base.pb.cc
    ${PROTO_GENERATED_DIR}/internal.pb.cc
    ${PROTO_GENERATED_DIR}/team.pb.cc
)
set(PROTO_HDRS
    ${PROTO_GENERATED_DIR}/base.pb.h
    ${PROTO_GENERATED_DIR}/internal.pb.h
    ${PROTO_GENERATED_DIR}/team.pb.h
)

# Source files
set(SOURCES
    src/main.cpp
    src/team_server.cpp
    src/gate_session.cpp
    src/team_manager.cpp
    src/redis_client.cpp
    ${PROTO_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/log_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/message_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/msvc_compat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/server_main_helper.cpp
)

add_executable(team_server ${SOURCES})

# Include directories
target_include_directories(team_server PRIVATE
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

# Link directories
target_link_directories(team_server PRIVATE
    ${PROTOBUF_ROOT}/lib
    ${LIBEVENT_ROOT}/lib
    ${HIREDIS_ROOT}/lib
)

# MSVC specific settings
if(MSVC)
    target_compile_definitions(team_server PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NOMINMAX
    )
    target_compile_options(team_server PRIVATE /utf-8 /MT$<$<CONFIG:Debug>:d>)

    file(GLOB ABSL_LIBS "${PROTOBUF_ROOT}/lib/absl_*.lib")
    target_link_libraries(team_server PRIVATE
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
else()
    target_link_libraries(team_server PRIVATE
        protobuf-lite
        event
        event_core
        event_extra
        hiredis
        pthread
    )
endif()

set_target_properties(team_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
)
