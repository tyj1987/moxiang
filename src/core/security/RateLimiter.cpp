#include "RateLimiter.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>

namespace Murim {
namespace Core {
namespace Security {

RateLimiter& RateLimiter::Instance() {
    static RateLimiter instance;
    return instance;
}

void RateLimiter::Initialize() {
    spdlog::info("RateLimiter initialized");
}

bool RateLimiter::IsAllowed(const std::string& identifier, int max_attempts, int window_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    auto it = records_.find(identifier);

    if (it == records_.end()) {
        // 没有记录，允许操作
        return true;
    }

    auto& record = it->second;
    auto time_since_first = std::chrono::duration_cast<std::chrono::seconds>(
        now - record.first_attempt
    ).count();

    // 如果超过时间窗口，重置计数
    if (time_since_first >= window_seconds) {
        return true;
    }

    // 检查是否超过最大尝试次数
    return record.count < max_attempts;
}

void RateLimiter::RecordAttempt(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    auto it = records_.find(identifier);

    if (it == records_.end()) {
        // 新记录
        AttemptRecord record;
        record.first_attempt = now;
        record.last_attempt = now;
        record.count = 1;
        records_[identifier] = record;

        spdlog::debug("RateLimiter: First attempt recorded for {}", identifier);
    } else {
        // 更新现有记录
        auto& record = it->second;
        record.last_attempt = now;
        record.count++;

        spdlog::debug("RateLimiter: Attempt {} recorded for {}", record.count, identifier);
    }
}

int RateLimiter::GetRemainingAttempts(const std::string& identifier, int max_attempts, int window_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    auto it = records_.find(identifier);

    if (it == records_.end()) {
        return max_attempts;
    }

    auto& record = it->second;
    auto time_since_first = std::chrono::duration_cast<std::chrono::seconds>(
        now - record.first_attempt
    ).count();

    // 如果超过时间窗口，重置计数
    if (time_since_first >= window_seconds) {
        return max_attempts;
    }

    return std::max(0, max_attempts - record.count);
}

void RateLimiter::Clear(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(mutex_);
    records_.erase(identifier);

    spdlog::debug("RateLimiter: Cleared records for {}", identifier);
}

void RateLimiter::Cleanup(int older_than_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    size_t removed = 0;

    for (auto it = records_.begin(); it != records_.end();) {
        auto time_since_last = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.last_attempt
        ).count();

        if (time_since_last > older_than_seconds) {
            it = records_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        spdlog::info("RateLimiter: Cleaned up {} expired records", removed);
    }
}

} // namespace Security
} // namespace Core
} // namespace Murim
