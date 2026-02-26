#pragma once

#include "libpq_wrapper.hpp"
#include "core/spdlog_wrapper.hpp"

#include <memory>
#include <functional>
#include <vector>
#include <string>

namespace Murim {
namespace Core {
namespace Database {

/**
 * @brief 查询执行器
 *
 * 提供便捷的数据库查询接口
 */
class QueryExecutor {
public:
    /**
     * @brief 构造函数
     * @param conn 数据库连接
     */
    explicit QueryExecutor(std::shared_ptr<Connection> conn);

    /**
     * @brief 析构函数
     */
    ~QueryExecutor();

    /**
     * @brief 执行查询（无结果）
     * @param sql SQL 语句
     * @return 影响的行数
     */
    size_t Execute(const std::string& sql);

    /**
     * @brief 执行查询（有结果），使用 lambda 处理每一行
     * @param sql SQL 语句
     * @param handler 结果处理函数，接收 Result 和 行号
     * @return 处理的行数
     */
    size_t Query(const std::string& sql,
                std::function<void(Result&, int)> handler);

    /**
     * @brief 执行查询（返回结果集）
     * @param sql SQL 语句
     * @return 结果集
     */
    std::shared_ptr<Result> Query(const std::string& sql);

    /**
     * @brief 执行参数化查询（无结果）
     * @param sql SQL 语句，使用 $1, $2, ... 作为参数占位符
     * @param params 参数列表
     * @return 影响的行数
     */
    size_t ExecuteParams(const std::string& sql, const std::vector<std::string>& params);

    /**
     * @brief 执行参数化查询（有结果）
     * @param sql SQL 语句，使用 $1, $2, ... 作为参数占位符
     * @param params 参数列表
     * @return 结果集
     */
    std::shared_ptr<Result> QueryParams(const std::string& sql, const std::vector<std::string>& params);

    /**
     * @brief 执行参数化查询（返回第一行）
     * @param sql SQL 语句，使用 $1, $2, ... 作为参数占位符
     * @param params 参数列表
     * @return 结果集和行号的包装器
     */
    std::shared_ptr<Result> QueryOneParams(const std::string& sql, const std::vector<std::string>& params);

    /**
     * @brief 开始事务
     */
    void BeginTransaction();

    /**
     * @brief 提交事务
     */
    void Commit();

    /**
     * @brief 回滚事务
     */
    void Rollback();

    /**
     * @brief 执行事务性操作
     * @param work 事务函数
     * @return 是否成功
     */
    bool Transaction(std::function<void()> work);

private:
    std::shared_ptr<Connection> conn_;
    bool in_transaction_;
};

} // namespace Database
} // namespace Core
} // namespace Murim
