#pragma once

#include <memory>
#include <chrono>
#include "Connection.hpp"

namespace Murim {
namespace Core {
namespace Network {

/**
 * @brief 会话类
 *
 * 管理客户端会话状态
 */
class Session {
public:
    using Ptr = std::shared_ptr<Session>;

    /**
     * @brief 构造函数
     * @param connection 连接对象
     * @param session_id 会话 ID
     */
    Session(Connection::Ptr connection, uint64_t session_id);

    /**
     * @brief 析构函数
     */
    ~Session();

    /**
     * @brief 获取连接
     * @return 连接对象
     */
    Connection::Ptr GetConnection() { return connection_; }

    /**
     * @brief 获取会话 ID
     * @return 会话 ID
     */
    uint64_t GetSessionId() const { return session_id_; }

    /**
     * @brief 获取用户数据
     * @return 用户数据指针
     */
    void* GetUserData() { return user_data_; }

    /**
     * @brief 设置用户数据
     * @param data 用户数据指针
     */
    void SetUserData(void* data) { user_data_ = data; }

    /**
     * @brief 更新最后活跃时间
     */
    void UpdateLastActiveTime();

    /**
     * @brief 获取最后活跃时间
     * @return 最后活跃时间
     */
    std::chrono::system_clock::time_point GetLastActiveTime() const {
        return last_active_;
    }

    /**
     * @brief 关闭会话
     */
    void Close();

private:
    Connection::Ptr connection_;
    uint64_t session_id_;
    void* user_data_;
    std::chrono::system_clock::time_point last_active_;
};

} // namespace Network
} // namespace Core
} // namespace Murim
