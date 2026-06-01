#include "game_server.h"
#include "dbmgr_connection_manager.h"
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
#include <vector>

static farm::GameServer* g_server = nullptr;
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

    farm::init_logging_default("game_server");

    // 解析配置文件
    std::string config_path = farm::parse_config_path(argc, argv, "config/game_server.json");
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

    farm::init_logging_from_config("game_server", config);

    // 读取服务器配置
    std::string ip = config.value("/server/ip"_json_pointer, "0.0.0.0");
    uint16_t port = static_cast<uint16_t>(config.value("/server/port"_json_pointer, 9090));
    std::string pid_file = config.value("pid_file", "./runtimeData/game_server.pid");

    // Parse server ID
    uint32_t server_id = config.value("/server/id"_json_pointer, 1u);

    // Parse GM config
    uint16_t gm_http_port = 7070;
    std::string gm_static_dir = "static";
    if (config.contains("gm_server")) {
        gm_http_port = static_cast<uint16_t>(config["gm_server"].value("http_port", 7070));
        gm_static_dir = config["gm_server"].value("static_dir", "static");
    }

    // DBMgr 配置
    std::vector<farm::DBMgrConfig> dbmgr_configs;

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

    // 注册服务到 etcd
    nlohmann::json service_info = {
        {"ip", ip},
        {"port", port},
        {"server_id", server_id}
    };
    if (!etcd.register_service("game", std::to_string(server_id), service_info.dump())) {
        SPDLOG_ERROR("[Main]Failed to register service to etcd");
        return 1;
    }

    SPDLOG_INFO("[Main]Registered to etcd as game/{}", server_id);

    // 从 etcd 发现 dbmgr 服务
    auto dbmgrs = etcd.discover_services("dbmgr");
    for (const auto& dbmgr : dbmgrs) {
        farm::DBMgrConfig cfg;
        cfg.host = dbmgr.ip;
        cfg.port = dbmgr.port;
        dbmgr_configs.push_back(cfg);
        SPDLOG_INFO("[Main]Discovered DBMgr from etcd: {}:{}", cfg.host, cfg.port);
    }

    if (dbmgr_configs.empty()) {
        SPDLOG_ERROR("[Main]No DBMgr instances found in etcd");
        return 1;
    }

    // 监听 dbmgr 服务变更
    etcd.watch_services("dbmgr", [&](const std::string& instance_id,
                                      const farm::ServiceInstance& inst,
                                      bool is_delete) {
        if (is_delete) {
            SPDLOG_INFO("[Main]DBMgr {} removed from etcd", instance_id);
        } else {
            SPDLOG_INFO("[Main]DBMgr {} added to etcd: {}:{}", instance_id, inst.ip, inst.port);
        }
    });
#else
    // 从配置文件读取 DBMgr 地址
    if (config.contains("dbmgrs") && config["dbmgrs"].is_array()) {
        for (const auto& db : config["dbmgrs"]) {
            farm::DBMgrConfig cfg;
            cfg.host = db.value("host", "127.0.0.1");
            cfg.port = static_cast<uint16_t>(db.value("port", 5000));
            dbmgr_configs.push_back(cfg);
        }
    }

    if (dbmgr_configs.empty()) {
        SPDLOG_ERROR("[Main]No DBMgr instances configured");
        return 1;
    }
#endif

    // Parse Redis config
    std::string redis_uri = config.value("/redis/uri"_json_pointer, "");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    farm::write_pid_file(pid_file);

    SPDLOG_INFO("[Main]=== Game Server ===");
    SPDLOG_INFO("[Main]Server ID: {}", server_id);
    SPDLOG_INFO("[Main]IP: {}", ip);
    SPDLOG_INFO("[Main]Port: {}", port);
    if (!redis_uri.empty()) {
        SPDLOG_INFO("[Main]Redis: {}", redis_uri);
    }
    if (!dbmgr_configs.empty()) {
        SPDLOG_INFO("[Main]DBMgrs: {}", dbmgr_configs.size());
        for (size_t i = 0; i < dbmgr_configs.size(); i++) {
            SPDLOG_INFO("[Main]  [{}] {}:{}", i, dbmgr_configs[i].host, dbmgr_configs[i].port);
        }
    }

    farm::GameServer server(ip, port, dbmgr_configs, redis_uri, server_id, gm_http_port, gm_static_dir);
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
