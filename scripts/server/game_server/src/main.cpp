#include "game_server.h"
#include "dbmgr_connection_manager.h"
#include "log_init.h"
#include "log_config.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <csignal>
#include <string>
#include <vector>
#include <sys/types.h>
#ifdef _WIN32
#include <winsock2.h>
#include <process.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

static farm::GameServer* g_server = nullptr;

static void signal_handler(int sig) {
    if (g_server) {
        SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
        g_server->stop();
    }
}

static bool write_pid_file(const std::string& pid_file) {
    size_t last_sep = pid_file.find_last_of("/\\");
    if (last_sep != std::string::npos) {
        std::string dir = pid_file.substr(0, last_sep);
#ifdef _WIN32
        _mkdir(dir.c_str());
#else
        mkdir(dir.c_str(), 0755);
#endif
    }

    std::ofstream ofs(pid_file);
    if (!ofs.is_open()) {
        SPDLOG_ERROR("[Main]Failed to write PID file: {}", pid_file);
        return false;
    }
#ifdef _WIN32
    ofs << _getpid();
#else
    ofs << getpid();
#endif
    ofs.close();
    SPDLOG_INFO("[Main]PID file written: {}", pid_file);
    return true;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "[Main] WSAStartup failed" << std::endl;
        return 1;
    }
#endif

    // 使用默认日志配置初始化（配置文件加载前）
    farm::init_logging_default("game_server");

    // Parse --config from command line
    std::string config_path = "config/game_server.json";
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--config") {
            config_path = argv[i + 1];
            break;
        }
    }
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

    // 读取日志配置并重新初始化
    farm::LoggingConfig log_config;
    if (config.contains("logging")) {
        auto& logging = config["logging"];
        log_config.dir = logging.value("dir", log_config.dir);
        log_config.level = logging.value("level", log_config.level);
        log_config.max_file_size_mb = logging.value("max_file_size_mb", log_config.max_file_size_mb);
        log_config.max_files = logging.value("max_files", log_config.max_files);
    }
    farm::init_logging("game_server", log_config);

    // Read config values
    std::string ip = config.value("/server/ip"_json_pointer, "0.0.0.0");
    uint16_t port = static_cast<uint16_t>(config.value("/server/port"_json_pointer, 9090));
    std::string pid_file = config.value("pid_file", "./runtimeData/game_server.pid");

    // Parse DBMgr configs from JSON
    std::vector<farm::DBMgrConfig> dbmgr_configs;
    if (config.contains("dbmgrs") && config["dbmgrs"].is_array()) {
        for (const auto& db : config["dbmgrs"]) {
            farm::DBMgrConfig cfg;
            cfg.host = db.value("host", "127.0.0.1");
            cfg.port = static_cast<uint16_t>(db.value("port", 5000));
            dbmgr_configs.push_back(cfg);
        }
    }

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Write PID file
    write_pid_file(pid_file);

    SPDLOG_INFO("[Main]=== Game Server ===");
    SPDLOG_INFO("[Main]IP: {}", ip);
    SPDLOG_INFO("[Main]Port: {}", port);
    if (!dbmgr_configs.empty()) {
        SPDLOG_INFO("[Main]DBMgrs: {}", dbmgr_configs.size());
        for (size_t i = 0; i < dbmgr_configs.size(); i++) {
            SPDLOG_INFO("[Main]  [{}] {}:{}",
                              i, dbmgr_configs[i].host, dbmgr_configs[i].port);
        }
    }

    farm::GameServer server(ip, port, dbmgr_configs);
    g_server = &server;

    if (!server.start()) {
        SPDLOG_ERROR("[Main]Server failed to start");
        return 1;
    }

    SPDLOG_INFO("[Main]Server stopped");
    farm::shutdown_logging();

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
