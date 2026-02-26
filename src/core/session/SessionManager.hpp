#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <functional>

namespace Murim {
namespace Core {
namespace Session {

/**
 * @brief 会话信息
 */
struct SessionInfo {
    uint64_t session_id;
    uint64_t account_id;
    std::string session_token;
    std::string client_ip;
    uint16_t server_id;
    uint64_t character_id;

    std::chrono::system_clock::time_point login_time;
    std::chrono::system_clock::time_point last_active_time;
    std::chrono::system_clock::time_point logout_time;

    bool is_active = true;

    /**
     * @brief 检查会话是否超时
     */
    bool IsTimeout(int timeout_seconds) const {
        if (!is_active) return true;

        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_active_time
        ).count();

        return elapsed >= timeout_seconds;
    }

    /**
     * @brief 获取会话活跃时长（秒）
     */
    int64_t GetActiveDuration() const {
        auto end_time = is_active ? std::chrono::system_clock::now() : logout_time;
        return std::chrono::duration_cast<std::chrono::seconds>(
            end_time - login_time
        ).count();
    }
};

/**
 * @brief Session 清理回调
 *
 * 当 session 被清理时调用的回调函数
 */
using SessionCleanupCallback = std::function<void(uint64_t session_id, uint64_t account_id)>;

/**
 * @brief Session 管理器
 *
 * 负责：
 * 1. Session 的创建、更新、删除
 * 2. Session 超时检测
 * 3. 定期清理过期 session
 */
class SessionManager {
public:
    static SessionManager& Instance();

    /**
     * @brief 初始化 SessionManager
     * @param cleanup_interval_seconds 清理间隔（秒）
     * @param session_timeout_seconds Session 超时时间（秒）
     */
    void Initialize(int cleanup_interval_seconds = 300, int session_timeout_seconds = 1800);

    /**
     * @brief 启动清理线程
     */
    void Start();

    /**
     * @brief 停止清理线程
     */
    void Stop();

    /**
     * @brief 添加或更新 Session
     */
    void AddOrUpdateSession(const SessionInfo& session_info);

    /**
     * @brief 获取 Session
     * @return Session 信息，如果不存在返回 nullptr
     */
    const SessionInfo* GetSession(uint64_t session_id) const;

    /**
     * @brief 根据 account_id 获取 Session
     * @return Session 信息，如果不存在返回 nullptr
     */
    const SessionInfo* GetSessionByAccount(uint64_t account_id) const;

    /**
     * @brief 根据 session_token 获取 Session
     */
    const SessionInfo* GetSessionByToken(const std::string& token) const;

    /**
     * @brief 移除 Session
     * @param session_id Session ID
     * @param reason 移除原因 (0=正常, 1=超时, 2=被踢, 3=重复登录)
     */
    void RemoveSession(uint64_t session_id, int reason = 0);

    /**
     * @brief 移除账号的所有 Session
     * @param account_id 账号 ID
     * @param reason 移除原因
     * @return 移除的 Session 数量
     */
    size_t RemoveAccountSessions(uint64_t account_id, int reason = 0);

    /**
     * @brief 更新 Session 的最后活跃时间
     * @param session_id Session ID
     * @return true 如果成功更新
     */
    bool UpdateLastActiveTime(uint64_t session_id);

    /**
     * @brief 获取活跃 Session 数量
     */
    size_t GetActiveSessionCount() const;

    /**
     * @brief 获取总 Session 数量
     */
    size_t GetTotalSessionCount() const;

    /**
     * @brief 设置清理回调
     * @param callback 当 Session 被清理时调用的函数
     */
    void SetCleanupCallback(SessionCleanupCallback callback);

    /**
     * @brief 手动触发清理
     * @param timeout_seconds 超时时间（秒），0 表示使用默认值
     */
    void CleanupExpiredSessions(int timeout_seconds = 0);

private:
    SessionManager() = default;
    ~SessionManager();

    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    /**
     * @brief 清理线程函数
     */
    void CleanupThread();

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, SessionInfo> sessions_by_id_;
    std::unordered_map<uint64_t, uint64_t> sessions_by_account_;  // account_id -> session_id
    std::unordered_map<std::string, uint64_t> sessions_by_token_;  // token -> session_id

    int cleanup_interval_seconds_ = 300;   // 默认 5 分钟清理一次
    int session_timeout_seconds_ = 1800;   // 默认 30 分钟超时

    std::thread cleanup_thread_;
    std::atomic<bool> running_{false};
    SessionCleanupCallback cleanup_callback_;
};

} // namespace Session
} // namespace Core
} // namespace Murim
