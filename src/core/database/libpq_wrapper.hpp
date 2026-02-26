#pragma once

// libpq C API 的 C++ 包装器
// 替代 libpqxx，提供简单的 C++ 接口

#include <libpq-fe.h>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <stdexcept>  // for std::runtime_error

namespace Murim {
namespace Core {
namespace Database {

/**
 * @brief libpq 结果集包装器
 */
class Result {
public:
    Result(PGresult* result) : result_(result) {}
    ~Result() {
        if (result_) {
            PQclear(result_);
        }
    }

    // 禁止拷贝
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    // 允许移动
    Result(Result&& other) noexcept : result_(other.result_) {
        other.result_ = nullptr;
    }
    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            if (result_) PQclear(result_);
            result_ = other.result_;
            other.result_ = nullptr;
        }
        return *this;
    }

    // 检查命令是否成功
    bool OK() const {
        return result_ && PQresultStatus(result_) == PGRES_COMMAND_OK;
    }

    // 检查是否有数据返回
    bool HasData() const {
        return result_ && PQresultStatus(result_) == PGRES_TUPLES_OK;
    }

    // 获取行数
    int RowCount() const {
        return result_ ? PQntuples(result_) : 0;
    }

    // 获取列数
    int ColumnCount() const {
        return result_ ? PQnfields(result_) : 0;
    }

    // 获取列名
    std::string ColumnName(int col) const {
        if (!result_ || col < 0 || col >= ColumnCount()) return "";
        return PQfname(result_, col);
    }

    // 检查字段是否为 NULL
    bool IsNull(int row, int col) const {
        if (!result_ || row < 0 || row >= RowCount() || col < 0 || col >= ColumnCount()) {
            return true;
        }
        return PQgetisnull(result_, row, col);
    }

    // 获取字符串值
    std::string GetValue(int row, int col) const {
        if (!result_ || row < 0 || row >= RowCount() || col < 0 || col >= ColumnCount()) {
            return "";
        }
        if (IsNull(row, col)) return "";
        const char* val = PQgetvalue(result_, row, col);
        return val ? std::string(val) : "";
    }

    // 通过列名获取值
    std::string GetValue(int row, const std::string& col_name) const {
        if (!result_) return "";
        int col = PQfnumber(result_, col_name.c_str());
        if (col < 0) return "";
        return GetValue(row, col);
    }

    // 获取列索引
    int ColumnIndex(const std::string& col_name) const {
        if (!result_) return -1;
        return PQfnumber(result_, col_name.c_str());
    }

    // 检查列是否存在
    bool HasColumn(const std::string& col_name) const {
        return ColumnIndex(col_name) >= 0;
    }

    // 获取原始 PGresult 对象（用于高级操作）
    PGresult* GetPGResult() const {
        return result_;
    }

    // 模板函数：获取特定类型的值
    template<typename T>
    std::optional<T> Get(int row, int col) const {
        std::string val = GetValue(row, col);
        if (val.empty()) return std::nullopt;

        if constexpr (std::is_same_v<T, std::string>) {
            return val;
        } else if constexpr (std::is_integral_v<T>) {
            try {
                if constexpr (std::is_signed_v<T>) {
                    return static_cast<T>(std::stoll(val));
                } else {
                    return static_cast<T>(std::stoull(val));
                }
            } catch (...) {
                return std::nullopt;
            }
        } else if constexpr (std::is_floating_point_v<T>) {
            try {
                return static_cast<T>(std::stod(val));
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    template<typename T>
    std::optional<T> Get(int row, const std::string& col_name) const {
        int col = ColumnIndex(col_name);
        if (col < 0) return std::nullopt;
        return Get<T>(row, col);
    }

private:
    PGresult* result_;
};

/**
 * @brief libpq 连接的 C++ 包装器
 */
class Connection {
public:
    Connection(const std::string& conn_str) {
        conn_ = PQconnectdb(conn_str.c_str());
        if (PQstatus(conn_) != CONNECTION_OK) {
            std::string error = PQerrorMessage(conn_);
            PQfinish(conn_);
            conn_ = nullptr;
            throw std::runtime_error("Connection failed: " + error);
        }
    }

    ~Connection() {
        if (conn_) {
            PQfinish(conn_);
        }
    }

    // 禁止拷贝
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // 执行 SQL 查询
    std::shared_ptr<Result> Execute(const std::string& sql) {
        if (!conn_) {
            throw std::runtime_error("Connection is null");
        }

        PGresult* result = PQexec(conn_, sql.c_str());
        return std::make_shared<Result>(result);
    }

    // 执行参数化查询
    std::shared_ptr<Result> ExecuteParams(const std::string& sql,
                                          const std::vector<std::string>& params) {
        if (!conn_) {
            throw std::runtime_error("Connection is null");
        }

        // 转换参数为 C 字符串指针数组
        std::vector<const char*> param_values;
        std::vector<int> param_lengths;
        std::vector<int> param_formats;

        for (const auto& param : params) {
            param_values.push_back(param.c_str());
            param_lengths.push_back(static_cast<int>(param.size()));
            param_formats.push_back(0);  // text format
        }

        PGresult* result = PQexecParams(
            conn_,
            sql.c_str(),
            static_cast<int>(params.size()),
            nullptr,  // param types (let server infer)
            param_values.data(),
            param_lengths.data(),
            param_formats.data(),
            0  // result format (text)
        );

        return std::make_shared<Result>(result);
    }

    // 开始事务
    bool BeginTransaction() {
        auto res = Execute("BEGIN");
        return res && res->OK();
    }

    // 提交事务
    bool Commit() {
        auto res = Execute("COMMIT");
        return res && res->OK();
    }

    // 回滚事务
    bool Rollback() {
        auto res = Execute("ROLLBACK");
        return res && res->OK();
    }

    // 检查连接是否正常
    bool IsOK() const {
        return conn_ && PQstatus(conn_) == CONNECTION_OK;
    }

    // 获取错误消息
    std::string ErrorMessage() const {
        if (!conn_) return "Connection is null";
        return PQerrorMessage(conn_);
    }

    // 获取原始 PGconn 对象（用于直接使用 libpq C API）
    PGconn* GetPGConn() const {
        return conn_;
    }

private:
    PGconn* conn_;
};

} // namespace Database
} // namespace Core
} // namespace Murim
