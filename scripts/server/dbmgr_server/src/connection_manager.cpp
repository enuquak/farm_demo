#include "connection_manager.h"
#include "log_macros.h"

#include <chrono>

namespace farm {

ConnectionManager::ConnectionManager(const MongoReplicaSetConfig& mongo_config, const RedisPoolConfig& redis_config)
    : mongo_config_(mongo_config)
    , redis_config_(redis_config)
    , mongo_conn_(mongo_config)
    , redis_pool_(redis_config) {
}

ConnectionManager::~ConnectionManager() {
    shutdown();
}

bool ConnectionManager::init() {
    SPDLOG_INFO("[ConnectionManager]Initializing connections...");

    state_ = ConnectionState::CONNECTING;

    bool mongo_ok = try_connect_mongo();
    bool redis_ok = try_connect_redis();

    if (mongo_ok && redis_ok) {
        state_ = ConnectionState::CONNECTED;
        SPDLOG_INFO("[ConnectionManager]All connections established");
        if (on_ready_callback_) {
            on_ready_callback_();
        }
        return true;
    }

    // 部分或全部连接失败，启动后台重试
    state_ = ConnectionState::FAILED;
    running_ = true;
    retry_thread_ = std::thread(&ConnectionManager::retry_thread_func, this);

    SPDLOG_WARN("[ConnectionManager]Some connections failed, retry thread started");
    return false;
}

void ConnectionManager::shutdown() {
    running_ = false;

    if (retry_thread_.joinable()) {
        retry_thread_.join();
    }

    mongo_conn_.disconnect();
    redis_pool_.shutdown();

    state_ = ConnectionState::DISCONNECTED;
    SPDLOG_INFO("[ConnectionManager]Shutdown complete");
}

bool ConnectionManager::is_ready() const {
    return state_ == ConnectionState::CONNECTED;
}

ConnectionState ConnectionManager::state() const {
    return state_;
}

MongoReplicaSetConnection& ConnectionManager::mongo_connection() {
    return mongo_conn_;
}

RedisPool& ConnectionManager::redis_pool() {
    return redis_pool_;
}

void ConnectionManager::read_from_primary_after_write() {
    mongo_conn_.read_from_primary_after_write();
}

void ConnectionManager::restore_read_preference() {
    mongo_conn_.restore_read_preference();
}

void ConnectionManager::set_on_ready_callback(std::function<void()> callback) {
    on_ready_callback_ = callback;
}

bool ConnectionManager::try_connect_mongo() {
    if (mongo_conn_.is_connected()) {
        return true;
    }

    SPDLOG_INFO("[ConnectionManager]Connecting to MongoDB Replica Set...");
    if (mongo_conn_.connect()) {
        mongo_retry_count_ = 0;
        return true;
    }

    mongo_retry_count_++;
    SPDLOG_ERROR("[ConnectionManager]MongoDB connection failed (attempt {})", mongo_retry_count_);
    return false;
}

bool ConnectionManager::try_connect_redis() {
    if (redis_pool_.is_ready()) {
        return true;
    }

    SPDLOG_INFO("[ConnectionManager]Connecting to Redis Pool...");
    if (redis_pool_.init()) {
        redis_retry_count_ = 0;
        return true;
    }

    redis_retry_count_++;
    SPDLOG_ERROR("[ConnectionManager]Redis pool init failed (attempt {})", redis_retry_count_);
    return false;
}

void ConnectionManager::retry_thread_func() {
    SPDLOG_INFO("[ConnectionManager]Retry thread started");

    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(mongo_config_.retry_interval_ms));

        if (!running_) break;

        // 检查是否超过最大重试次数
        if (mongo_config_.max_retry_count > 0 && mongo_retry_count_ >= mongo_config_.max_retry_count) {
            SPDLOG_ERROR("[ConnectionManager]MongoDB max retry count reached");
            state_ = ConnectionState::FAILED_PERMANENT;
            break;
        }
        if (redis_config_.max_retry_count > 0 && redis_retry_count_ >= redis_config_.max_retry_count) {
            SPDLOG_ERROR("[ConnectionManager]Redis max retry count reached");
            state_ = ConnectionState::FAILED_PERMANENT;
            break;
        }

        // 尝试重连
        bool mongo_ok = try_connect_mongo();
        bool redis_ok = try_connect_redis();

        if (mongo_ok && redis_ok) {
            state_ = ConnectionState::CONNECTED;
            SPDLOG_INFO("[ConnectionManager]All connections recovered");
            if (on_ready_callback_) {
                on_ready_callback_();
            }
            break;
        }
    }

    SPDLOG_INFO("[ConnectionManager]Retry thread exiting");
}

void ConnectionManager::update_state() {
    if (mongo_conn_.is_connected() && redis_pool_.is_ready()) {
        state_ = ConnectionState::CONNECTED;
    } else if (state_ == ConnectionState::CONNECTED) {
        state_ = ConnectionState::FAILED;
    }
}

}  // namespace farm
