#pragma once

#include "session.h"
#include <event2/util.h>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace farm {

class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager() = default;

    // 添加会话
    std::shared_ptr<Session> add_session(evutil_socket_t fd, struct bufferevent* bev);

    // 移除会话（按fd）
    void remove_session(evutil_socket_t fd);

    // 绑定玩家ID到会话
    void bind_player_id(evutil_socket_t fd, uint64_t player_id);

    // 查找会话
    std::shared_ptr<Session> find_by_fd(evutil_socket_t fd) const;
    std::shared_ptr<Session> find_by_player_id(uint64_t player_id) const;
    std::shared_ptr<Session> find_by_account_id(const std::string& account_id) const;

    // 获取所有会话（用于心跳检测遍历）
    std::vector<std::shared_ptr<Session>> get_all_sessions() const;

    // 当前会话数
    size_t session_count() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<evutil_socket_t, std::shared_ptr<Session>> sessions_;
    std::unordered_map<uint64_t, evutil_socket_t> player_to_fd_;
};

}  // namespace farm
