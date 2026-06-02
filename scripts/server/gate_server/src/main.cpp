#include "gate_server.h"
#include "etcd_manager.h"
#include "server_main_helper.h"
#include "log_init.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>

#include <csignal>
#include <fstream>
#include <string>

static farm::GateServer* g_server = nullptr;
static farm::EtcdManager* g_etcd = nullptr;

static void signal_handler(int sig) {
    if (g_server) {
        SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
        g_server->stop();
    }
    if (g_etcd) {
        g_etcd->shutdown();
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

    // 读取本地配置（作为 fallback）
    std::string ip = config.value("/server/ip"_json_pointer, "0.0.0.0");
    uint16_t port = static_cast<uint16_t>(config.value("/server/port"_json_pointer, 8080));
    std::string pid_file = config.value("pid_file", "./runtimeData/gate_server.pid");

    // etcd 配置
    std::string etcd_endpoints = config.value("/etcd/endpoints"_json_pointer, "http://localhost:2379");
    uint32_t lease_ttl = config.value("/etcd/lease_ttl"_json_pointer, 15u);
    std::string instance_id = config.value("/etcd/instance_id"_json_pointer, "gate-1");

    // 初始化 etcd
    farm::EtcdManager etcd(etcd_endpoints, lease_ttl);
    if (!etcd.connect()) {
        SPDLOG_ERROR("[Main]Failed to connect to etcd");
        return 1;
    }
    g_etcd = &etcd;

    // 设置 etcd 错误回调
    etcd.set_error_callback([](const std::string& error_msg) {
        SPDLOG_ERROR("[Etcd]Error: {}", error_msg);
        // 可以在这里添加告警或其他处理逻辑
    });

    // 从 etcd 配置中心读取服务器配置（如果存在）
    std::string config_key = "servers/gate/" + instance_id;
    auto etcd_config = etcd.get_config(config_key);
    if (etcd_config.has_value()) {
        try {
            auto cfg = nlohmann::json::parse(etcd_config.value());
            ip = cfg.value("ip", ip);
            port = static_cast<uint16_t>(cfg.value("port", port));
            SPDLOG_INFO("[Main]Loaded config from etcd: {}:{}", ip, port);
        } catch (const std::exception& e) {
            SPDLOG_WARN("[Main]Failed to parse etcd config, using local: {}", e.what());
        }
    } else {
        SPDLOG_INFO("[Main]No config in etcd, using local config");
        // 将本地配置写入 etcd 配置中心
        nlohmann::json local_cfg = {{"ip", ip}, {"port", port}};
        etcd.put_config(config_key, local_cfg.dump());
    }

    // 注册服务到 etcd
    nlohmann::json service_info = {
        {"ip", ip},
        {"port", port},
        {"status", "online"}
    };
    if (!etcd.register_service("gate", instance_id, service_info.dump())) {
        SPDLOG_ERROR("[Main]Failed to register service to etcd");
        return 1;
    }

    SPDLOG_INFO("[Main]Registered to etcd as gate/{}", instance_id);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    farm::write_pid_file(pid_file);

    SPDLOG_INFO("[Main]=== Gate Server ===");
    SPDLOG_INFO("[Main]IP: {}", ip);
    SPDLOG_INFO("[Main]Port: {}", port);

    farm::GateServer server(ip, port);

    // 从 etcd 发现 game servers
    auto games = etcd.discover_services("game");
    for (const auto& game : games) {
        SPDLOG_INFO("[Main]Discovered Game Server from etcd: server_id={} at {}:{}",
                    game.server_id, game.ip, game.port);
        server.add_game_server(game.server_id, game.ip, game.port);
    }

    if (games.empty()) {
        SPDLOG_WARN("[Main]No Game Server instances found in etcd");
    }

    // 监听 game server 变更
    etcd.watch_services("game", [&](const std::string& id,
                                     const farm::ServiceInstance& inst,
                                     bool is_delete) {
        if (is_delete) {
            SPDLOG_INFO("[Main]Game Server {} removed from etcd", id);
            server.dispatch_to_event_loop([&server, server_id = inst.server_id]() {
                server.remove_game_server(server_id);
            });
        } else {
            SPDLOG_INFO("[Main]Game Server {} added to etcd: {}:{}",
                        id, inst.ip, inst.port);
            server.dispatch_to_event_loop([&server, server_id = inst.server_id,
                                            ip = inst.ip, port = inst.port]() {
                server.add_game_server(server_id, ip, port);
            });
        }
    });

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
