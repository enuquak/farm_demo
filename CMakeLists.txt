cmake_minimum_required(VERSION 3.14)
project(farm_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 静态链接运行时（与预编译的 absl/protobuf 匹配）
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 可选依赖: etcd（默认关闭）
option(ENABLE_ETCD "Enable etcd service discovery" OFF)

# 依赖路径
set(LIBEVENT_ROOT "C:/libevent_install" CACHE PATH "libevent install path")
set(HIREDIS_ROOT "C:/hiredis_install" CACHE PATH "hiredis install path")
set(PROTOBUF_ROOT "C:/protobuf_install" CACHE PATH "protobuf install path")
set(MONGOC_ROOT "C:/mongoc_install" CACHE PATH "libmongoc install path")

if(ENABLE_ETCD)
    set(VCPKG_ROOT "C:/vcpkg/installed/x64-windows" CACHE PATH "vcpkg install path")
    set(ETCD_ROOT "C:/etcd-cpp-apiv3_install" CACHE PATH "etcd-cpp-apiv3 install path")
endif()

# 共享 Proto 目录
set(COMMON_PROTO_DIR "${CMAKE_SOURCE_DIR}/scripts/common/proto")

# 添加子目录
add_subdirectory(scripts/server/dbmgr_server)
add_subdirectory(scripts/server/game_server)
add_subdirectory(scripts/server/gate_server)
add_subdirectory(scripts/server/chat_server)
add_subdirectory(scripts/server/friend_service)
add_subdirectory(scripts/server/cross_server)
add_subdirectory(scripts/server/team_server)
