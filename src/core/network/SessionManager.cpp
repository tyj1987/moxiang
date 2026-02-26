#include "SessionManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>

namespace Murim {
namespace Core {
namespace Network {

void SessionManager::AddSession(Session::Ptr session) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    sessions_[session->GetSessionId()] = session;
    spdlog::debug("Session added: id={}, total={}", session->GetSessionId(), sessions_.size());
}

void SessionManager::RemoveSession(uint64_t session_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        sessions_.erase(it);
        spdlog::debug("Session removed: id={}, total={}", session_id, sessions_.size());
    }
}

Session::Ptr SessionManager::GetSession(uint64_t session_id) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<Session::Ptr> SessionManager::GetAllSessions() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<Session::Ptr> sessions;
    sessions.reserve(sessions_.size());

    for (const auto& pair : sessions_) {
        sessions.push_back(pair.second);
    }

    return sessions;
}

size_t SessionManager::GetSessionCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return sessions_.size();
}

void SessionManager::CleanupInactiveSessions(int timeout_seconds) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    size_t cleaned_count = 0;

    for (auto it = sessions_.begin(); it != sessions_.end();) {
        auto last_active = it->second->GetLastActiveTime();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_active).count();

        if (elapsed > timeout_seconds) {
            spdlog::info("Cleaning up inactive session: id={}, inactive={}s",
                        it->first, elapsed);
            it = sessions_.erase(it);
            ++cleaned_count;
        } else {
            ++it;
        }
    }

    if (cleaned_count > 0) {
        spdlog::info("Cleaned {} inactive sessions, remaining={}",
                    cleaned_count, sessions_.size());
    }
}

bool SessionManager::Initialize() {
    spdlog::info("[SessionManager] Initializing...");
    // SessionManager is ready to use after construction
    spdlog::info("[SessionManager] Initialized successfully");
    return true;
}

bool SessionManager::Start() {
    spdlog::info("[SessionManager] Starting...");
    // Start any background tasks or timers if needed
    spdlog::info("[SessionManager] Started successfully");
    return true;
}

void SessionManager::Stop() {
    spdlog::info("[SessionManager] Stopping...");
    // Clean up all sessions
    std::unique_lock<std::shared_mutex> lock(mutex_);
    size_t count = sessions_.size();
    sessions_.clear();
    spdlog::info("[SessionManager] Stopped, cleaned {} sessions", count);
}

void SessionManager::Update(uint32_t delta_time) {
    // Update all active sessions
    std::shared_lock<std::shared_mutex> lock(mutex_);

    for (auto& pair : sessions_) {
        if (pair.second) {
            // Update session (process send queues, etc.)
            // Session-specific update logic can be added here
        }
    }

    // Periodic cleanup of inactive sessions (every 60 seconds)
    static uint32_t cleanup_timer = 0;
    cleanup_timer += delta_time;
    if (cleanup_timer >= 60000) {
        cleanup_timer = 0;
        lock.unlock();
        CleanupInactiveSessions(300); // 5 minutes timeout
    }
}

} // namespace Network
} // namespace Core
} // namespace Murim
