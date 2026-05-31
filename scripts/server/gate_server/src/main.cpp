#include "gate_server.h"
#include "server_main_helper.h"
#include "log_init.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>

#include <csignal>
#include <fstream>
#include <string>

static farm::GateServer* g_server = nullptr;

static void signal_handler(int sig) {
    if (g_server) {
        SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    if (!farm::init_platform_network()) return 1;

    farm::init_logging_default("gate_server");

    // 解析配置文件
    std::string config_path = farm::parse_config_path(argc, argv, "config/gate_server.json");
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

    farm::init_logging_from_config("gate_server", config);

    // 读取服务器配置
    std::string ip = config.value("/server/ip"_json_pointer, "0.0.0.0");
    uint16_t port = static_cast<uint16_t>(config.value("/server/port"_json_pointer, 8080));
    std::string pid_file = config.value("pid_file", "./runtimeData/gate_server.pid");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    farm::write_pid_file(pid_file);

    SPDLOG_INFO("[Main]=== Gate Server ===");
    SPDLOG_INFO("[Main]IP: {}", ip);
    SPDLOG_INFO("[Main]Port: {}", port);

    farm::GateServer server(ip, port);

    // 加载 Game Server 配置
    if (config.contains("game_servers") && config["game_servers"].is_array()) {
        for (const auto& gs : config["game_servers"]) {
            std::string game_ip = gs.value("ip", "127.0.0.1");
            uint16_t game_port = static_cast<uint16_t>(gs.value("port", 9090));
            uint32_t server_id = gs.value("server_id", 1);
            SPDLOG_INFO("[Main]Game Server [{}]: {}:{}", server_id, game_ip, game_port);
            server.add_game_server(server_id, game_ip, game_port);
        }
    }

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
