#include "game_server.h"
#include "dbmgr_connection_manager.h"
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <string>
#include <vector>
#include <sstream>
#ifdef _WIN32
#include <winsock2.h>
#endif

static farm::GameServer* g_server = nullptr;

static void signal_handler(int sig) {
    if (g_server) {
        std::cout << "\n[Main] Received signal " << sig << ", shutting down..." << std::endl;
        g_server->stop();
    }
}

/**
 * @brief Parse dbmgr config string.
 * Format: "host1:port1,host2:port2,..."
 * Example: "127.0.0.1:5000,127.0.0.1:5001"
 */
static std::vector<farm::DBMgrConfig> parse_dbmgr_config(const std::string& config_str) {
    std::vector<farm::DBMgrConfig> configs;
    if (config_str.empty()) return configs;

    std::istringstream ss(config_str);
    std::string token;
    while (std::getline(ss, token, ',')) {
        // Trim whitespace
        size_t start = token.find_first_not_of(" \t");
        size_t end = token.find_last_not_of(" \t");
        if (start == std::string::npos) continue;
        token = token.substr(start, end - start + 1);

        // Parse host:port
        size_t colon_pos = token.find(':');
        if (colon_pos == std::string::npos) {
            std::cerr << "[Main] Invalid dbmgr config entry: " << token << std::endl;
            continue;
        }

        farm::DBMgrConfig cfg;
        cfg.host = token.substr(0, colon_pos);
        cfg.port = static_cast<uint16_t>(std::atoi(token.substr(colon_pos + 1).c_str()));
        configs.push_back(cfg);
    }
    return configs;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Initialize Winsock on Windows (required before any Winsock functions)
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "[Main] WSAStartup failed" << std::endl;
        return 1;
    }
#endif

    std::string ip = "127.0.0.1";
    uint16_t port = 9090;
    std::string dbmgr_str;

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--dbmgr" && i + 1 < argc) {
            dbmgr_str = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--ip" && i + 1 < argc) {
            ip = argv[++i];
        } else if (port == 9090 && !arg.empty() && arg[0] != '-') {
            // Legacy: first positional arg is port
            port = static_cast<uint16_t>(std::atoi(argv[i]));
        }
    }

    // Parse DBMgr config
    std::vector<farm::DBMgrConfig> dbmgr_configs = parse_dbmgr_config(dbmgr_str);

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

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
