#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace farm {

// 写入 PID 文件
bool write_pid_file(const std::string& pid_file);

// 解析 --config 命令行参数
std::string parse_config_path(int argc, char* argv[], const std::string& default_path);

// 从 JSON config 中读取日志配置并初始化日志系统
// process_name: 进程名（如 "gate_server"）
// config: 已解析的 JSON 配置对象
void init_logging_from_config(const std::string& process_name, const nlohmann::json& config);

// 初始化平台网络（Windows WSAStartup）
bool init_platform_network();

// 清理平台网络（Windows WSACleanup）
void cleanup_platform_network();

}  // namespace farm
