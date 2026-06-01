#pragma once

#include "dbmgr_connection_manager.h"
#include "log_macros.h"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstdint>

namespace farm {

class PlayerIdPool {
public:
    PlayerIdPool(DBMgrConnectionManager* dbmgr_mgr, uint32_t batch_size = 64)
        : dbmgr_mgr_(dbmgr_mgr)
        , batch_size_(batch_size)
        , fetching_(false)
    {
    }

    ~PlayerIdPool() = default;

    // 获取一个 player_id（池耗尽时同步等待）
    uint64_t acquire() {
        std::unique_lock<std::mutex> lock(mutex_);

        // 如果池为空，等待补充
        while (pool_.empty()) {
            // 触发一次预取
            if (!fetching_) {
                fetching_ = true;
                lock.unlock();
                dbmgr_mgr_->send_alloc_player_id_req(batch_size_,
                    [this](int32_t code, uint64_t start_id, uint32_t count) {
                        on_alloc_response(code, start_id, count);
                    });
                lock.lock();
            }
            cv_.wait(lock);
        }

        uint64_t id = pool_.front();
        pool_.pop();

        // 检查是否需要补充（剩余 < 30%）
        if (!fetching_ && pool_.size() < batch_size_ * 30 / 100) {
            fetching_ = true;
            lock.unlock();
            dbmgr_mgr_->send_alloc_player_id_req(batch_size_,
                [this](int32_t code, uint64_t start_id, uint32_t count) {
                    on_alloc_response(code, start_id, count);
                });
            lock.lock();
        }

        return id;
    }

    // 预取回调（DBMgr 响应后调用）
    void on_alloc_response(int32_t code, uint64_t start_id, uint32_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        fetching_ = false;

        if (code != 0 || count == 0) {
            SPDLOG_ERROR("[PlayerIdPool]Alloc failed, code={}", code);
            cv_.notify_all();  // 唤醒等待者（它们会重试）
            return;
        }

        // 将分配的 ID 逐个加入池
        for (uint32_t i = 0; i < count; ++i) {
            pool_.push(start_id + i);
        }

        SPDLOG_INFO("[PlayerIdPool]Replenished {} IDs, pool size now {}", count, pool_.size());

        // 唤醒等待的 acquire()
        cv_.notify_all();
    }

    // 当前池大小（用于监控/调试）
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

private:
    DBMgrConnectionManager* dbmgr_mgr_;
    uint32_t batch_size_;
    std::queue<uint64_t> pool_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool fetching_;
};

}  // namespace farm
