#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <cstdint>

namespace Murim {
namespace Core {
namespace Security {

/**
 * @brief 速率限制器
 *
 * 用于防止暴力破解攻击，限制特定操作的频率
 * 例如：登录尝试、密码重置、API调用等
 */
class RateLimiter {
public:
    static RateLimiter& Instance();

    /**
     * @brief 初始化速率限制器
     */
    void Initialize();

    /**
     * @brief 检查是否允许操作（基于 IP）
     * @param identifier 标识符（例如 IP 地址）
     * @param max_attempts 最大尝试次数
     * @param window_seconds 时间窗口（秒）
     * @return true 如果允许操作
     */
    bool IsAllowed(const std::string& identifier, int max_attempts, int window_seconds);

    /**
     * @brief 记录一次尝试
     * @param identifier 标识符
     */
    void RecordAttempt(const std::string& identifier);

    /**
     * @brief 获取剩余尝试次数
     * @param identifier 标识符
     * @param max_attempts 最大尝试次数
     * @param window_seconds 时间窗口（秒）
     * @return 剩余尝试次数
     */
    int GetRemainingAttempts(const std::string& identifier, int max_attempts, int window_seconds);

    /**
     * @brief 清除指定标识符的记录
     * @param identifier 标识符
     */
    void Clear(const std::string& identifier);

    /**
     * @brief 清理过期的记录
     * @param older_than_seconds 清理多少秒之前的记录
     */
    void Cleanup(int older_than_seconds = 3600);

private:
    RateLimiter() = default;
    ~RateLimiter() = default;
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;

    struct AttemptRecord {
        std::chrono::system_clock::time_point first_attempt;
        std::chrono::system_clock::time_point last_attempt;
        int count = 0;
    };

    std::unordered_map<std::string, AttemptRecord> records_;
    std::mutex mutex_;
};

} // namespace Security
} // namespace Core
} // namespace Murim
