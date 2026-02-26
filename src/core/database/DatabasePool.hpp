#pragma once

// 确保异常支持启用(MSVC)
#if defined(_MSC_VER)
    #include <eh.h>
#endif

#include "libpq_wrapper.hpp"

#include <memory>
#include <queue>
#include <mutex>
#include <string>

namespace Murim {
namespace Core {
namespace Database {

/**
 * @brief 数据库连接池
 *
 * 管理 PostgreSQL 连接，提供连接复用
 */
class ConnectionPool {
public:
    static ConnectionPool& Instance();

    /**
     * @brief 初始化连接池
     * @param conn_str 数据库连接字符串
     *              格式: "host=localhost port=5432 dbname=mh_game user=postgres password=xxx"
     * @param pool_size 连接池大小
     */
    void Initialize(const std::string& conn_str, size_t pool_size);

    /**
     * @brief 获取连接
     * @return 连接对象，如果无可用连接则阻塞等待
     */
    std::shared_ptr<Connection> GetConnection();

    /**
     * @brief 尝试获取连接（非阻塞）
     * @param conn 输出参数，获取到的连接
     * @return true 如果成功获取连接
     */
    bool TryGetConnection(std::shared_ptr<Connection>& conn);

    /**
     * @brief 归还连接
     * @param conn 连接对象
     */
    void ReturnConnection(std::shared_ptr<Connection> conn);

    /**
     * @brief 获取可用连接数
     */
    size_t GetAvailableCount() const;

    /**
     * @brief 获取总连接数
     */
    size_t GetTotalCount() const { return max_size_; }

    /**
     * @brief 关闭所有连接
     */
    void Close();

    /**
     * @brief 关闭连接池（别名）
     */
    void Shutdown() { Close(); }

    /**
     * @brief 检查连接池是否已初始化
     */
    bool IsInitialized() const { return initialized_; }

private:
    ConnectionPool() = default;
    ~ConnectionPool();

    // 禁止拷贝
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    /**
     * @brief 创建新连接
     * @return 新连接对象
     */
    std::shared_ptr<Connection> CreateConnection();

    std::queue<std::shared_ptr<Connection>> pool_;
    std::mutex mutex_;
    std::string conn_str_;
    size_t max_size_;
    bool initialized_;
};

} // namespace Database
} // namespace Core
} // namespace Murim
