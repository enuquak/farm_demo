#include "dbmgr_server.h"
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <string>
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#endif

static farm::DbMgrServer* g_server = nullptr;

static void signal_handler(int sig) {
    if (g_server) {
        std::cout << "\n[Main] Received signal " << sig << ", shutting down..." << std::endl;
        g_server->stop();
    }
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --index <N>      DBMgr instance index (default: 0)" << std::endl;
    std::cout << "  --port <port>    Listen port (default: 5000)" << std::endl;
    std::cout << "  --data-dir <dir> Data directory (default: ./data)" << std::endl;
    std::cout << "  --help           Show this help message" << std::endl;
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

    // 默认参数
    uint32_t index = 0;
    uint16_t port = 5000;
    std::string data_dir = "./data";
    std::string ip = "0.0.0.0";

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--index") == 0 && i + 1 < argc) {
            index = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--data-dir") == 0 && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "[Main] Unknown argument: " << argv[i] << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "=== DBMgr Server ===" << std::endl;
    std::cout << "Index: " << index << std::endl;
    std::cout << "IP: " << ip << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Data Dir: " << data_dir << std::endl;

    farm::DbMgrServer server(index, ip, port, data_dir);
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
