#include "friend_service.h"
#include "server_main_helper.h"
#include "log_init.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>

#include <csignal>
#include <fstream>
#include <string>

static farm::FriendService* g_service = nullptr;

static void signal_handler(int sig) {
    if (g_service) {
        SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
        g_service->stop();
    }
}

int main(int argc, char* argv[]) {
    if (!farm::init_platform_network()) return 1;

    farm::init_logging_default("friend_service");

    // Parse config file
    std::string config_path = farm::parse_config_path(argc, argv, "config/friend_service.json");
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

    farm::init_logging_from_config("friend_service", config);

    // Read config values
    std::string redis_uri = config.value("/redis/uri"_json_pointer, "");
    std::string game_host = config.value("/game_server/host"_json_pointer, "127.0.0.1");
    int game_port = config.value("/game_server/port"_json_pointer, 9090);
    int listen_port = config.value("/server/port"_json_pointer, 0);
    std::string pid_file = config.value("pid_file", "./runtimeData/friend_service.pid");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    farm::write_pid_file(pid_file);

    SPDLOG_INFO("[Main]=== Friend Service ===");
    if (!redis_uri.empty()) {
        SPDLOG_INFO("[Main]Redis: {}", redis_uri);
    }
    SPDLOG_INFO("[Main]Game Server: {}:{}", game_host, game_port);

    // Create event base
    struct event_base* base = event_base_new();
    if (!base) {
        SPDLOG_ERROR("[Main]Failed to create event_base");
        return 1;
    }

    // Create and start service
    farm::FriendService service(base);
    g_service = &service;

    if (!service.start(redis_uri, game_host, game_port, listen_port)) {
        SPDLOG_ERROR("[Main]Failed to start FriendService");
        event_base_free(base);
        return 1;
    }

    SPDLOG_INFO("[Main]FriendService running");

    // Run event loop
    event_base_dispatch(base);

    // Cleanup
    service.stop();
    g_service = nullptr;

    event_base_free(base);

    SPDLOG_INFO("[Main]FriendService stopped");
    farm::shutdown_logging();
    farm::cleanup_platform_network();
    return 0;
}
