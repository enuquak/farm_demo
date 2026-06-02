#include "cross_server.h"
#ifdef ENABLE_ETCD
#include "etcd_manager.h"
#endif
#include "server_main_helper.h"
#include "log_init.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>

#include <csignal>
#include <fstream>
#include <string>

static farm::CrossServer* g_server = nullptr;
#ifdef ENABLE_ETCD
static farm::EtcdManager* g_etcd = nullptr;
#endif

static void signal_handler(int sig) {
    if (g_server) {
        SPDLOG_INFO("[Main]Received signal {}, shutting down...", sig);
        g_server->stop();
    }
#ifdef ENABLE_ETCD
    if (g_etcd) {
        g_etcd->shutdown();
    }
#endif
}

int main(int argc, char* argv[]) {
    if (!farm::init_platform_network()) return 1;

    farm::init_logging_default("cross_server");

    // 解析配置文件
    std::string config_path = farm::parse_config_path(argc, argv, "config/cross_server.json");
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

    farm::init_logging_from_config("cross_server", config);

    // 读取本地配置（作为 fallback）
    std::string ip = config.value("/server/ip"_json_pointer, "0.0.0.0");
    uint16_t port = static_cast<uint16_t>(config.value("/server/port"_json_pointer, 7070));
    uint32_t instance_id = config.value("/server/instance_id"_json_pointer, 1u);
    std::string pid_file = config.value("pid_file", "./runtimeData/cross_server.pid");

#ifdef ENABLE_ETCD
    // etcd 配置
    std::string etcd_endpoints = config.value("/etcd/endpoints"_json_pointer, "http://localhost:2379");
    uint32_t lease_ttl = config.value("/etcd/lease_ttl"_json_pointer, 15u);

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
    });

    // 注册服务到 etcd
    nlohmann::json service_info = {
        {"ip", ip},
        {"port", port},
        {"instance_id", instance_id}
    };
    if (!etcd.register_service("cross", std::to_string(instance_id), service_info.dump())) {
        SPDLOG_ERROR("[Main]Failed to register service to etcd");
        return 1;
    }

    SPDLOG_INFO("[Main]Registered to etcd as cross/{}", instance_id);
#endif

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    farm::write_pid_file(pid_file);

    // 构建 CrossServerConfig
    farm::CrossServerConfig server_config;
    server_config.ip = ip;
    server_config.port = port;
    server_config.instance_id = instance_id;

    SPDLOG_INFO("[Main]=== Cross Server ===");
    SPDLOG_INFO("[Main]Instance ID: {}", instance_id);
    SPDLOG_INFO("[Main]IP: {}", ip);
    SPDLOG_INFO("[Main]Port: {}", port);

    farm::CrossServer server(server_config);
    g_server = &server;

#ifdef ENABLE_ETCD
    // 从 etcd 发现 game 服务
    auto games = etcd.discover_services("game");
    for (const auto& game : games) {
        try {
            uint32_t server_id = static_cast<uint32_t>(std::stoul(game.instance_id));
            server.add_game_server(server_id, game.ip, game.port);
            SPDLOG_INFO("[Main]Discovered Game Server from etcd: {} ({}:{})", server_id, game.ip, game.port);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Main]Failed to parse game server instance_id={}: {}", game.instance_id, e.what());
        }
    }

    // 监听 game 服务变更
    etcd.watch_services("game", [&server](const std::string& instance_id,
                                           const farm::ServiceInstance& inst,
                                           bool is_delete) {
        if (is_delete) {
            SPDLOG_INFO("[Main]Game Server {} removed from etcd, removing connection", instance_id);
            try {
                uint32_t server_id = static_cast<uint32_t>(std::stoul(instance_id));
                server.remove_game_server(server_id);
            } catch (const std::exception& e) {
                SPDLOG_ERROR("[Main]Failed to parse game server instance_id={}: {}", instance_id, e.what());
            }
        } else {
            SPDLOG_INFO("[Main]Game Server {} added to etcd: {}:{}, adding connection", instance_id, inst.ip, inst.port);
            try {
                uint32_t server_id = static_cast<uint32_t>(std::stoul(instance_id));
                server.add_game_server(server_id, inst.ip, inst.port);
            } catch (const std::exception& e) {
                SPDLOG_ERROR("[Main]Failed to parse game server instance_id={}: {}", instance_id, e.what());
            }
        }
    });

    // 监听玩家路由配置变更
    // etcd key: /farm/config/players/{player_id}, value: {"server_id": N}
    // watch_config 回调中的 key 已去掉前缀 "players/"，直接是 player_id
    etcd.watch_config("players/", [&server](const std::string& key,
                                             const std::string& value_json) {
        // key 已经是 player_id（watch_config 会去掉 "players/" 前缀）
        try {
            uint64_t player_id = std::stoull(key);
            // 更新路由（DELETE 事件当前被 etcd_manager 跳过，此处只处理 PUT）
            auto val = nlohmann::json::parse(value_json);
            uint32_t server_id = val.value("server_id", 0u);
            if (server_id > 0) {
                SPDLOG_INFO("[Main]Player route updated: player_id={} -> server_id={}", player_id, server_id);
                server.update_route(player_id, server_id);
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Main]Failed to parse player route key={}: {}", key, e.what());
        }
    });
#endif

    if (!server.start()) {
        SPDLOG_ERROR("[Main]Server failed to start");
        return 1;
    }

    SPDLOG_INFO("[Main]Server stopped");
    farm::shutdown_logging();
    farm::cleanup_platform_network();
    return 0;
}
