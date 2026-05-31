#include "dbmgr_server.h"
#include "connection_manager.h"
#include "server_main_helper.h"
#include "log_init.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>

#include <csignal>
#include <fstream>
#include <string>

static farm::DbMgrServer* g_server = nullptr;

static void signal_handler(int sig) {
    if (g_server) {
        SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    if (!farm::init_platform_network()) return 1;

    farm::init_logging_default("dbmgr");

    // 解析配置文件
    std::string config_path = farm::parse_config_path(argc, argv, "config/dbmgr.json");
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        SPDLOG_ERROR("[Main]Config file not found: {}", config_path);
        return 1;
    }

    nlohmann::json config;
    try {
        config_file >> config;
    } catch (const nlohmann::json::parse_error& e) {
        SPDLOG_ERROR("[Main]Failed to parse config file: {}", e.what());
        return 1;
    }
    config_file.close();

    farm::init_logging_from_config("dbmgr", config);

    // 读取服务器配置
    uint32_t index = config.value("/server/index"_json_pointer, 0);
    uint16_t port = static_cast<uint16_t>(config.value("/server/port"_json_pointer, 5000));
    std::string ip = config.value("/server/ip"_json_pointer, "0.0.0.0");
    std::string pid_file = config.value("pid_file", "./runtimeData/dbmgr.pid");

    // 数据库连接配置
    farm::MongoConfig mongo_config;
    mongo_config.uri = config.value("/database/mongo/uri"_json_pointer, "mongodb://localhost:27017/farm");
    mongo_config.retry_interval_ms = config.value("/database/mongo/retry_interval_ms"_json_pointer, 3000);
    mongo_config.max_retry_count = config.value("/database/mongo/max_retry_count"_json_pointer, 0);

    farm::RedisConfig redis_config;
    redis_config.uri = config.value("/database/redis/uri"_json_pointer, "redis://localhost:6379");
    redis_config.retry_interval_ms = config.value("/database/redis/retry_interval_ms"_json_pointer, 3000);
    redis_config.max_retry_count = config.value("/database/redis/max_retry_count"_json_pointer, 0);

    std::string index_config_dir = config.value("/database/index_config_dir"_json_pointer, "config/mongo");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    farm::write_pid_file(pid_file);

    SPDLOG_INFO("[Main]=== DBMgr Server ===");
    SPDLOG_INFO("[Main]Index: {}", index);
    SPDLOG_INFO("[Main]IP: {}", ip);
    SPDLOG_INFO("[Main]Port: {}", port);
    SPDLOG_INFO("[Main]Mongo URI: {}", mongo_config.uri);
    SPDLOG_INFO("[Main]Redis URI: {}", redis_config.uri);
    SPDLOG_INFO("[Main]Index Config Dir: {}", index_config_dir);

    // 初始化连接管理器
    farm::ConnectionManager conn_mgr(mongo_config, redis_config);
    if (!conn_mgr.init()) {
        SPDLOG_WARN("[Main]Connection manager init returned false, will retry in background");
    }

    farm::DbMgrServer server(index, ip, port, conn_mgr, index_config_dir);
    g_server = &server;

    if (!server.start()) {
        SPDLOG_ERROR("[Main]Server failed to start");
        return 1;
    }

    SPDLOG_INFO("[Main]Server stopped");
    farm::shutdown_logging();
    farm::cleanup_platform_network();
    return 0;
}
