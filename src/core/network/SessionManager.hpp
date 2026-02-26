#pragma once

#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include "Session.hpp"

namespace Murim {
namespace Core {
namespace Network {

/**
 * @brief 会话管理器
 *
 * 管理所有活动会话
 */
class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager() = default;

    /**
     * @brief 获取单例实例
     * @return SessionManager实例
     */
    static SessionManager& Instance() {
        static SessionManager instance;
        return instance;
    }

    /**
     * @brief 添加会话
     * @param session 会话对象
     */
    void AddSession(Session::Ptr session);

    /**
     * @brief 移除会话
     * @param session_id 会话 ID
     */
    void RemoveSession(uint64_t session_id);

    /**
     * @brief 获取会话
     * @param session_id 会话 ID
     * @return 会话对象，如果不存在返回 nullptr
     */
    Session::Ptr GetSession(uint64_t session_id);

    /**
     * @brief 获取所有会话
     * @return 会话列表
     */
    std::vector<Session::Ptr> GetAllSessions();

    /**
     * @brief 获取会话数量
     * @return 会话数量
     */
    size_t GetSessionCount() const;

    /**
     * @brief 清理超时会话
     * @param timeout_seconds 超时时间（秒）
     */
    void CleanupInactiveSessions(int timeout_seconds);

    /**
     * @brief 初始化会话管理器
     * @return true if successful
     */
    bool Initialize();

    /**
     * @brief 启动会话管理器
     * @return true if successful
     */
    bool Start();

    /**
     * @brief 停止会话管理器
     */
    void Stop();

    /**
     * @brief 更新会话管理器
     * @param delta_time 时间增量(毫秒)
     */
    void Update(uint32_t delta_time);

private:
    std::unordered_map<uint64_t, Session::Ptr> sessions_;
    mutable std::shared_mutex mutex_;
};

} // namespace Network
} // namespace Core
} // namespace Murim
