#include "gate_server.h"
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <string>
#ifdef _WIN32
#include <winsock2.h>
#endif

static farm::GateServer* g_server = nullptr;

static void signal_handler(int sig) {
    if (g_server) {
        std::cout << "\n[Main] Received signal " << sig << ", shutting down..." << std::endl;
        g_server->stop();
    }
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

    std::string ip = "0.0.0.0";
    uint16_t port = 8080;

    // 解析命令行参数
    if (argc >= 2) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    if (argc >= 3) {
        ip = argv[2];
    }

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "=== Gate Server ===" << std::endl;
    std::cout << "IP: " << ip << std::endl;
    std::cout << "Port: " << port << std::endl;

    farm::GateServer server(ip, port);
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
