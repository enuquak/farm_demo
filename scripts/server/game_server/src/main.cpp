#include "game_server.h"
#include "dbmgr_connection_manager.h"
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
        std::cout << "\n[Main] Received signal " << sig << ", shutting down..." << std::endl;
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
        std::cerr << "[Main] Failed to write PID file: " << pid_file << std::endl;
        return false;
    }
#ifdef _WIN32
    ofs << _getpid();
#else
    ofs << getpid();
#endif
    ofs.close();
    std::cout << "[Main] PID file written: " << pid_file << std::endl;
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

    // Load config from JSON file
    std::string config_path = "config/game_server.json";
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        std::cerr << "[Main] Error: Config file not found: " << config_path << std::endl;
        return 1;
    }

    nlohmann::json config;
    try {
        config_file >> config;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[Main] Error: Failed to parse config file: " << e.what() << std::endl;
        return 1;
    }
    config_file.close();

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

    std::cout << "=== Game Server ===" << std::endl;
    std::cout << "IP: " << ip << std::endl;
    std::cout << "Port: " << port << std::endl;
    if (!dbmgr_configs.empty()) {
        std::cout << "DBMgrs: " << dbmgr_configs.size() << std::endl;
        for (size_t i = 0; i < dbmgr_configs.size(); i++) {
            std::cout << "  [" << i << "] " << dbmgr_configs[i].host
                      << ":" << dbmgr_configs[i].port << std::endl;
        }
    }

    farm::GameServer server(ip, port, dbmgr_configs);
    g_server = &server;

    if (!server.start()) {
        std::cerr << "[Main] Server failed to start" << std::endl;
        return 1;
    }

    std::cout << "[Main] Server stopped" << std::endl;
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
