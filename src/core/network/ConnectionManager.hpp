#pragma once

#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <functional>
#include "Connection.hpp"

namespace Murim {
namespace Core {
namespace Network {

/**
 * @brief 连接管理器
 *
 * 管理所有活动连接
 */
class ConnectionManager {
public:
    using Ptr = std::shared_ptr<ConnectionManager>;
    using ConnectionHandler = std::function<void(Connection::Ptr)>;

    ConnectionManager() = default;
    ~ConnectionManager() = default;

    /**
     * @brief 获取单例实例
     * @return ConnectionManager实例
     */
    static ConnectionManager& Instance() {
        static ConnectionManager instance;
        return instance;
    }

    /**
     * @brief 添加连接
     * @param connection 连接对象
     */
    void AddConnection(Connection::Ptr connection);

    /**
     * @brief 移除连接
     * @param connection_id 连接 ID
     */
    void RemoveConnection(uint64_t connection_id);

    /**
     * @brief 获取连接
     * @param connection_id 连接 ID
     * @return 连接对象，如果不存在返回 nullptr
     */
    Connection::Ptr GetConnection(uint64_t connection_id);

    /**
     * @brief 获取所有连接
     * @return 连接列表
     */
    std::vector<Connection::Ptr> GetAllConnections();

    /**
     * @brief 获取连接数量
     * @return 连接数量
     */
    size_t GetConnectionCount() const;

    /**
     * @brief 向所有连接广播消息
     * @param buffer 消息缓冲区
     * @param exclude_connection_id 排除的连接 ID (可选)
     */
    void BroadcastMessage(std::shared_ptr<ByteBuffer> buffer, uint64_t exclude_connection_id = 0);

    /**
     * @brief 清理已关闭的连接
     */
    void CleanupClosedConnections();

    /**
     * @brief 初始化连接管理器
     * @return true if successful
     */
    bool Initialize();

    /**
     * @brief 启动连接管理器
     * @return true if successful
     */
    bool Start();

    /**
     * @brief 停止连接管理器
     */
    void Stop();

    /**
     * @brief 更新连接管理器
     * @param delta_time 时间增量(毫秒)
     */
    void Update(uint32_t delta_time);

private:
    std::unordered_map<uint64_t, Connection::Ptr> connections_;
    mutable std::shared_mutex mutex_;
};

} // namespace Network
} // namespace Core
} // namespace Murim
