#include "log_init.h"
#include "log_modules.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>

#include <iostream>
#include <filesystem>
#include <memory>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace farm {

/**
 * 获取当前进程 PID
 */
static uint32_t get_pid() {
#ifdef _WIN32
    return static_cast<uint32_t>(GetCurrentProcessId());
#else
    return static_cast<uint32_t>(getpid());
#endif
}

/**
 * 解析日志级别字符串
 */
static spdlog::level::level_enum parse_log_level(const std::string& level) {
    if (level == "debug" || level == "DEBUG") {
        return spdlog::level::debug;
    } else if (level == "info" || level == "INFO") {
        return spdlog::level::info;
    } else if (level == "error" || level == "ERROR") {
        return spdlog::level::err;
    } else if (level == "critical" || level == "CRITICAL") {
        return spdlog::level::critical;
    }
    return spdlog::level::info;  // 默认 INFO
}

/**
 * 创建日志目录
 */
static void ensure_log_directory(const std::string& dir) {
    try {
        std::filesystem::create_directories(dir);
    } catch (const std::exception& e) {
        std::cerr << "[LogInit] Failed to create log directory: " << dir
                  << ", error: " << e.what() << std::endl;
    }
}

/**
 * 初始化日志系统
 */
void init_logging(const std::string& process_name, const LoggingConfig& config) {
    // 确保日志目录存在
    ensure_log_directory(config.dir);

    // 获取 PID
    uint32_t pid = get_pid();

    // 构建日志文件路径
    std::string log_file = config.dir + "/" + process_name + ".log";

    // 创建 sinks
    std::vector<spdlog::sink_ptr> sinks;

    // 文件 sink (按大小轮转)
    try {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file,
            config.max_file_size_mb * 1024 * 1024,  // 转换为字节
            config.max_files
        );
        sinks.push_back(file_sink);
    } catch (const std::exception& e) {
        std::cerr << "[LogInit] Failed to create file sink: " << e.what() << std::endl;
    }

    // 创建 logger
    auto logger = std::make_shared<spdlog::logger>(process_name, sinks.begin(), sinks.end());

    // 设置格式: [时间][级别][进程名:PID][内容]
    // 模块名在各日志调用中以 [Module] 前缀形式包含在消息中
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e][%^%l%$][" + process_name + ":" + std::to_string(pid) + "] %v");

    // 设置日志级别
    logger->set_level(parse_log_level(config.level));

    // 注册为默认 logger
    spdlog::set_default_logger(logger);

    // 刷新策略: 每次写入都刷新
    logger->flush_on(spdlog::level::debug);
}

/**
 * 使用默认配置初始化日志系统
 */
void init_logging_default(const std::string& process_name) {
    LoggingConfig default_config;
    default_config.dir = "./runtimeData/logs/server";
    default_config.level = "info";
    default_config.max_file_size_mb = 20;
    default_config.max_files = 7;

    init_logging(process_name, default_config);
}

/**
 * 关闭日志系统
 */
void shutdown_logging() {
    spdlog::shutdown();
}

}  // namespace farm
