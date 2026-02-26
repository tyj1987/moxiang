#include "QueryExecutor.hpp"
#include "../spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Database {

QueryExecutor::QueryExecutor(std::shared_ptr<Connection> conn)
    : conn_(conn), in_transaction_(false) {
}

QueryExecutor::~QueryExecutor() {
    // 清理未提交的事务
    if (in_transaction_) {
        try {
            Rollback();
        } catch (const std::exception& e) {
            spdlog::error("Error in QueryExecutor destructor: {}", e.what());
        }
    }
}

size_t QueryExecutor::Execute(const std::string& sql) {
    try {
        auto result = conn_->Execute(sql);
        if (!result) {
            throw std::runtime_error("Execute returned null result");
        }

        // 对于 INSERT/UPDATE/DELETE，返回影响的行数
        // PostgreSQL 返回的字符串 "COMMAND_OK"
        const char* cmdTuples = PQcmdTuples(result->GetPGResult());
        if (cmdTuples && strlen(cmdTuples) > 0) {
            try {
                return std::stoull(cmdTuples);
            } catch (...) {
                return 0;
            }
        }

        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Execute SQL failed: {}\nSQL: {}", e.what(), sql);
        throw;
    }
}

size_t QueryExecutor::Query(const std::string& sql,
                           std::function<void(Result&, int)> handler) {
    try {
        auto result = conn_->Execute(sql);
        if (!result || !result->HasData()) {
            return 0;
        }

        size_t count = 0;
        int row_count = result->RowCount();

        for (int i = 0; i < row_count; ++i) {
            handler(*result, i);
            ++count;
        }

        return count;
    } catch (const std::exception& e) {
        spdlog::error("Query SQL failed: {}\nSQL: {}", e.what(), sql);
        throw;
    }
}

std::shared_ptr<Result> QueryExecutor::Query(const std::string& sql) {
    try {
        auto result = conn_->Execute(sql);
        if (!result) {
            throw std::runtime_error("Query returned null result");
        }
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Query SQL failed: {}\nSQL: {}", e.what(), sql);
        throw;
    }
}

size_t QueryExecutor::ExecuteParams(const std::string& sql, const std::vector<std::string>& params) {
    try {
        auto result = conn_->ExecuteParams(sql, params);
        if (!result) {
            throw std::runtime_error("ExecuteParams returned null result");
        }

        // 返回影响的行数
        const char* cmdTuples = PQcmdTuples(result->GetPGResult());
        if (cmdTuples && strlen(cmdTuples) > 0) {
            try {
                return std::stoull(cmdTuples);
            } catch (...) {
                return 0;
            }
        }

        return 0;
    } catch (const std::exception& e) {
        spdlog::error("ExecuteParams SQL failed: {}\nSQL: {}", e.what(), sql);
        throw;
    }
}

std::shared_ptr<Result> QueryExecutor::QueryParams(const std::string& sql, const std::vector<std::string>& params) {
    try {
        auto result = conn_->ExecuteParams(sql, params);
        if (!result || !result->HasData()) {
            throw std::runtime_error("QueryParams failed or returned no data");
        }
        return result;
    } catch (const std::exception& e) {
        spdlog::error("QueryParams SQL failed: {}\nSQL: {}", e.what(), sql);
        throw;
    }
}

std::shared_ptr<Result> QueryExecutor::QueryOneParams(const std::string& sql, const std::vector<std::string>& params) {
    try {
        auto result = conn_->ExecuteParams(sql, params);
        if (!result || !result->HasData()) {
            throw std::runtime_error("QueryOneParams failed or returned no data");
        }

        if (result->RowCount() == 0) {
            throw std::runtime_error("Expected 1 row, got 0");
        }

        if (result->RowCount() > 1) {
            throw std::runtime_error("Expected 1 row, got " + std::to_string(result->RowCount()));
        }

        return result;
    } catch (const std::exception& e) {
        spdlog::error("QueryOneParams SQL failed: {}\nSQL: {}", e.what(), sql);
        throw;
    }
}

void QueryExecutor::BeginTransaction() {
    if (in_transaction_) {
        spdlog::warn("Transaction already in progress");
        return;
    }

    try {
        if (conn_->BeginTransaction()) {
            in_transaction_ = true;
            spdlog::debug("Transaction started");
        } else {
            throw std::runtime_error("Failed to begin transaction");
        }
    } catch (const std::exception& e) {
        spdlog::error("BeginTransaction failed: {}", e.what());
        throw;
    }
}

void QueryExecutor::Commit() {
    if (!in_transaction_) {
        spdlog::warn("No transaction to commit");
        return;
    }

    try {
        if (conn_->Commit()) {
            in_transaction_ = false;
            spdlog::debug("Transaction committed");
        } else {
            throw std::runtime_error("Failed to commit transaction");
        }
    } catch (const std::exception& e) {
        spdlog::error("Commit failed: {}", e.what());
        in_transaction_ = false;
        throw;
    }
}

void QueryExecutor::Rollback() {
    if (!in_transaction_) {
        spdlog::warn("No transaction to rollback");
        return;
    }

    try {
        if (conn_->Rollback()) {
            in_transaction_ = false;
            spdlog::debug("Transaction rolled back");
        } else {
            throw std::runtime_error("Failed to rollback transaction");
        }
    } catch (const std::exception& e) {
        spdlog::error("Rollback failed: {}", e.what());
        in_transaction_ = false;
        throw;
    }
}

bool QueryExecutor::Transaction(std::function<void()> work) {
    BeginTransaction();

    try {
        work();
        Commit();
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Transaction failed: {}", e.what());
        try {
            Rollback();
        } catch (...) {
            // Ignore rollback errors
        }
        return false;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
