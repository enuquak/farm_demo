#include "session_manager.h"

namespace farm {

std::shared_ptr<Session> SessionManager::add_session(evutil_socket_t fd, struct bufferevent* bev) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto session = std::make_shared<Session>(fd, bev);
    sessions_[fd] = session;
    return session;
}

void SessionManager::remove_session(evutil_socket_t fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(fd);
    if (it != sessions_.end()) {
        if (it->second->player_id() != 0) {
            player_to_fd_.erase(it->second->player_id());
        }
        sessions_.erase(it);
    }
}

std::shared_ptr<Session> SessionManager::find_by_fd(evutil_socket_t fd) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(fd);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Session> SessionManager::find_by_player_id(uint64_t player_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = player_to_fd_.find(player_id);
    if (it != player_to_fd_.end()) {
        auto sit = sessions_.find(it->second);
        if (sit != sessions_.end()) {
            return sit->second;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<Session>> SessionManager::get_all_sessions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<Session>> result;
    result.reserve(sessions_.size());
    for (auto& kv : sessions_) {
        result.push_back(kv.second);
    }
    return result;
}

size_t SessionManager::session_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

void SessionManager::bind_player_id(evutil_socket_t fd, uint64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(fd);
    if (it != sessions_.end()) {
        it->second->set_player_id(player_id);
        player_to_fd_[player_id] = fd;
    }
}

std::shared_ptr<Session> SessionManager::find_by_account_id(const std::string& account_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : sessions_) {
        if (kv.second->account_id() == account_id) {
            return kv.second;
        }
    }
    return nullptr;
}

}  // namespace farm
