#include "team_server.h"
#include "server_main_helper.h"
#include "log_init.h"
#include "log_macros.h"

#include <csignal>
#include <string>

static farm::TeamServer* g_server = nullptr;

static void signal_handler(int sig) {
    if (g_server) {
        SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    if (!farm::init_platform_network()) return 1;

    farm::init_logging_default("team_server");

    std::string ip = "0.0.0.0";
    uint16_t port = 8891;  // TeamServer 端口（ChatServer 用 8889）

    if (argc >= 2) ip = argv[1];
    if (argc >= 3) port = static_cast<uint16_t>(std::stoi(argv[2]));

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    SPDLOG_INFO("[Main]=== Team Server ===");
    SPDLOG_INFO("[Main]IP: {}", ip);
    SPDLOG_INFO("[Main]Port: {}", port);

    farm::TeamServer server(ip, port);
    g_server = &server;

    if (!server.start()) {
        SPDLOG_ERROR("[Main]Failed to start TeamServer");
        return 1;
    }

    SPDLOG_INFO("[Main]Server stopped");
    farm::shutdown_logging();
    farm::cleanup_platform_network();
    return 0;
}
