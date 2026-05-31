#pragma once

#include "log_config.h"
#include <string>

namespace farm {

/**
 * 初始化日志系统
 * 使用 spdlog 创建日志器，配置文件输出、格式和级别
 *
 * @param process_name 进程名称 (如 "gate_server", "game_server", "dbmgr")
 * @param config 日志配置
 */
void init_logging(const std::string& process_name, const LoggingConfig& config);

/**
 * 使用默认配置初始化日志系统
 * 在配置文件加载之前使用，确保日志可用
 *
 * @param process_name 进程名称
 */
void init_logging_default(const std::string& process_name);

/**
 * 关闭日志系统
 * 在进程退出前调用，确保所有日志都写入文件
 */
void shutdown_logging();

}  // namespace farm
