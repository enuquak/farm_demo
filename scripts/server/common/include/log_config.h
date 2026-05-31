#pragma once

#include <string>
#include <cstdint>

namespace farm {

/**
 * 日志配置结构体
 * 从 JSON 配置文件中读取日志相关配置
 */
struct LoggingConfig {
    std::string dir = "./runtimeData/logs/server";  // 日志目录
    std::string level = "info";                      // 最低日志级别 (debug, info, error, critical)
    uint32_t max_file_size_mb = 20;                  // 单文件最大大小 (MB)
    uint32_t max_files = 7;                          // 历史文件保留数量
};

}  // namespace farm
