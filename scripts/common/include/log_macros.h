#pragma once

/**
 * 统一日志头文件
 * 包含 log_modules.h 和 spdlog，提供统一的 include 入口
 *
 * 用法:
 *   #include "log_macros.h"
 *   SPDLOG_INFO("[{}] message", farm::LogModule::Gate);
 *   SPDLOG_ERROR("[{}] error: {}", farm::LogModule::DBMgr, err);
 */

#include "log_modules.h"
#include <spdlog/spdlog.h>
