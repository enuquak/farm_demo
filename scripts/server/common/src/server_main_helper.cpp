#include "server_main_helper.h"
#include "log_init.h"
#include "log_config.h"
#include "log_macros.h"

#include <fstream>
#include <iostream>
#include <string>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock2.h>
#include <process.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace farm {

bool write_pid_file(const std::string& pid_file) {
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
        SPDLOG_ERROR("[Main]Failed to write PID file: {}", pid_file);
        return false;
    }
#ifdef _WIN32
    ofs << _getpid();
#else
    ofs << getpid();
#endif
    ofs.close();
    SPDLOG_INFO("[Main]PID file written: {}", pid_file);
    return true;
}

std::string parse_config_path(int argc, char* argv[], const std::string& default_path) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--config") {
            return argv[i + 1];
        }
    }
    return default_path;
}

void init_logging_from_config(const std::string& process_name, const nlohmann::json& config) {
    LoggingConfig log_config;
    if (config.contains("logging")) {
        auto& logging = config["logging"];
        log_config.dir = logging.value("dir", log_config.dir);
        log_config.level = logging.value("level", log_config.level);
        log_config.max_file_size_mb = logging.value("max_file_size_mb", log_config.max_file_size_mb);
        log_config.max_files = logging.value("max_files", log_config.max_files);
    }
    init_logging(process_name, log_config);
}

bool init_platform_network() {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "[Main] WSAStartup failed" << std::endl;
        return false;
    }
#endif
    return true;
}

void cleanup_platform_network() {
#ifdef _WIN32
    WSACleanup();
#endif
}

}  // namespace farm
