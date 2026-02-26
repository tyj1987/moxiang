#include "SessionManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>

namespace Murim {
namespace Core {
namespace Session {

SessionManager::~SessionManager() {
    Stop();
}

SessionManager& SessionManager::Instance() {
    static SessionManager instance;
    return instance;
}

void SessionManager::Initialize(int cleanup_interval_seconds, int session_timeout_seconds) {
    cleanup_interval_seconds_ = cleanup_interval_seconds;
    session_timeout_seconds_ = session_timeout_seconds;

    spdlog::info("SessionManager initialized: cleanup_interval={}s, timeout={}s",
                 cleanup_interval_seconds_, session_timeout_seconds_);
}

void SessionManager::Start() {
    if (running_.load()) {
        spdlog::warn("SessionManager cleanup thread is already running");
        return;
    }

    running_.store(true);
    cleanup_thread_ = std::thread(&SessionManager::CleanupThread, this);

    spdlog::info("SessionManager cleanup thread started");
}

void SessionManager::Stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }

    spdlog::info("SessionManager cleanup thread stopped");
}

void SessionManager::AddOrUpdateSession(const SessionInfo& session_info) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否已存在
    auto it = sessions_by_id_.find(session_info.session_id);
    bool is_new = (it == sessions_by_id_.end());

    sessions_by_id_[session_info.session_id] = session_info;
    sessions_by_account_[session_info.account_id] = session_info.session_id;
    sessions_by_token_[session_info.session_token] = session_info.session_id;

    if (is_new) {
        spdlog::info("Session added: session_id={}, account_id={}, ip={}",
                     session_info.session_id, session_info.account_id, session_info.client_ip);
    } else {
        spdlog::debug("Session updated: session_id={}, account_id={}",
                      session_info.session_id, session_info.account_id);
    }
}

const SessionInfo* SessionManager::GetSession(uint64_t session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_by_id_.find(session_id);
    if (it != sessions_by_id_.end()) {
        return &it->second;
    }

    return nullptr;
}

const SessionInfo* SessionManager::GetSessionByAccount(uint64_t account_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_by_account_.find(account_id);
    if (it != sessions_by_account_.end()) {
        auto session_it = sessions_by_id_.find(it->second);
        if (session_it != sessions_by_id_.end()) {
            return &session_it->second;
        }
    }

    return nullptr;
}

const SessionInfo* SessionManager::GetSessionByToken(const std::string& token) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_by_token_.find(token);
    if (it != sessions_by_token_.end()) {
        auto session_it = sessions_by_id_.find(it->second);
        if (session_it != sessions_by_id_.end()) {
            return &session_it->second;
        }
    }

    return nullptr;
}

void SessionManager::RemoveSession(uint64_t session_id, int reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_by_id_.find(session_id);
    if (it == sessions_by_id_.end()) {
        return;
    }

    const auto& session_info = it->second;
    uint64_t account_id = session_info.account_id;

    // 从所有映射中移除
    sessions_by_account_.erase(account_id);
    sessions_by_token_.erase(session_info.session_token);
    sessions_by_id_.erase(it);

    spdlog::info("Session removed: session_id={}, account_id={}, reason={}",
                 session_id, account_id, reason);

    // 调用清理回调
    if (cleanup_callback_) {
        cleanup_callback_(session_id, account_id);
    }
}

size_t SessionManager::RemoveAccountSessions(uint64_t account_id, int reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_by_account_.find(account_id);
    if (it == sessions_by_account_.end()) {
        return 0;
    }

    uint64_t session_id = it->second;
    auto session_it = sessions_by_id_.find(session_id);

    if (session_it != sessions_by_id_.end()) {
        const auto& session_info = session_it->second;

        // 从所有映射中移除
        sessions_by_token_.erase(session_info.session_token);
        sessions_by_id_.erase(session_it);
    }

    sessions_by_account_.erase(it);

    spdlog::info("Account sessions removed: account_id={}, session_id={}, reason={}",
                 account_id, session_id, reason);

    // 调用清理回调
    if (cleanup_callback_) {
        cleanup_callback_(session_id, account_id);
    }

    return 1;
}

bool SessionManager::UpdateLastActiveTime(uint64_t session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_by_id_.find(session_id);
    if (it == sessions_by_id_.end()) {
        return false;
    }

    it->second.last_active_time = std::chrono::system_clock::now();
    return true;
}

size_t SessionManager::GetActiveSessionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = 0;
    for (const auto& pair : sessions_by_id_) {
        if (pair.second.is_active) {
            count++;
        }
    }

    return count;
}

size_t SessionManager::GetTotalSessionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_by_id_.size();
}

void SessionManager::SetCleanupCallback(SessionCleanupCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup_callback_ = callback;
}

void SessionManager::CleanupExpiredSessions(int timeout_seconds) {
    if (timeout_seconds == 0) {
        timeout_seconds = session_timeout_seconds_;
    }

    std::vector<uint64_t> expired_sessions;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& pair : sessions_by_id_) {
            if (pair.second.IsTimeout(timeout_seconds)) {
                expired_sessions.push_back(pair.first);
            }
        }
    }

    // 在锁外移除
    for (uint64_t session_id : expired_sessions) {
        RemoveSession(session_id, 1);  // reason=1: 超时
    }

    if (!expired_sessions.empty()) {
        spdlog::info("Session cleanup: removed {} expired sessions", expired_sessions.size());
    }
}

void SessionManager::CleanupThread() {
    spdlog::info("Session cleanup thread running");

    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(cleanup_interval_seconds_));

        if (!running_.load()) {
            break;
        }

        CleanupExpiredSessions();
    }

    spdlog::info("Session cleanup thread exiting");
}

} // namespace Session
} // namespace Core
} // namespace Murim
