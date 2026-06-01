#pragma once

#include <string_view>

namespace farm {
namespace LogModule {

// 服务器模块
constexpr std::string_view Gate = "Gate";
constexpr std::string_view Game = "Game";
constexpr std::string_view DBMgr = "DBMgr";

// 网络模块
constexpr std::string_view Network = "Network";
constexpr std::string_view Session = "Session";
constexpr std::string_view Player = "Player";

// 配置模块
constexpr std::string_view Config = "Config";

// 管理模块
constexpr std::string_view Admin = "Admin";

// 连接模块
constexpr std::string_view GameConnection = "GameConnection";
constexpr std::string_view DBMgrConnection = "DBMgrConnection";

// etcd 模块
constexpr std::string_view Etcd = "Etcd";

// 消息处理模块
constexpr std::string_view MessageHandler = "MessageHandler";
constexpr std::string_view MessageParser = "MessageParser";

// 数据管理模块
constexpr std::string_view DataManager = "DataManager";

// 主程序模块
constexpr std::string_view Main = "Main";

}  // namespace LogModule
}  // namespace farm
