#include "DatabasePool.hpp"
#include "core/spdlog_wrapper.hpp"
#include <exception>
#include <thread>
#include <chrono>

namespace Murim {
namespace Core {
namespace Database {

ConnectionPool& ConnectionPool::Instance() {
    static ConnectionPool instance;
    return instance;
}

ConnectionPool::~ConnectionPool() {
    Close();
}

void ConnectionPool::Initialize(const std::string& conn_str, size_t pool_size) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        spdlog::warn("ConnectionPool already initialized");
        return;
    }

    conn_str_ = conn_str;
    max_size_ = pool_size;
    initialized_ = true;

    spdlog::info("Initializing database connection pool: size={}", pool_size);

    // 创建初始连接
    for (size_t i = 0; i < pool_size; ++i) {
        try {
            auto conn = CreateConnection();
            if (conn) {
                pool_.push(conn);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to create database connection {}/{}: {}",
                        i + 1, pool_size, e.what());
        }
    }

    spdlog::info("Database connection pool initialized: {}/{} connections available",
                pool_.size(), pool_size);
}

std::shared_ptr<Connection> ConnectionPool::GetConnection() {
    std::unique_lock<std::mutex> lock(mutex_);

    // 等待可用连接
    while (pool_.empty()) {
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        lock.lock();
    }

    auto conn = pool_.front();
    pool_.pop();

    // 检查连接是否仍然有效
    try {
        auto res = conn->Execute("SELECT 1");
        if (!res || !res->HasData()) {
            spdlog::warn("Database connection invalid, recreating");
            try {
                conn = CreateConnection();
            } catch (const std::exception& e2) {
                spdlog::error("Failed to recreate database connection: {}", e2.what());
                return nullptr;
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Database connection invalid, recreating: {}", e.what());
        try {
            conn = CreateConnection();
        } catch (const std::exception& e2) {
            spdlog::error("Failed to recreate database connection: {}", e2.what());
            return nullptr;
        }
    }

    return conn;
}

bool ConnectionPool::TryGetConnection(std::shared_ptr<Connection>& conn) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (pool_.empty()) {
        return false;
    }

    conn = pool_.front();
    pool_.pop();

    // 检查连接是否有效
    try {
        auto res = conn->Execute("SELECT 1");
        if (!res || !res->HasData()) {
            spdlog::warn("Database connection invalid");
            try {
                conn = CreateConnection();
            } catch (const std::exception& e2) {
                spdlog::error("Failed to recreate database connection: {}", e2.what());
                return false;
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Database connection invalid: {}", e.what());
        try {
            conn = CreateConnection();
        } catch (const std::exception& e2) {
            spdlog::error("Failed to recreate database connection: {}", e2.what());
            return false;
        }
    }

    return true;
}

void ConnectionPool::ReturnConnection(std::shared_ptr<Connection> conn) {
    if (!conn) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    pool_.push(conn);
}

size_t ConnectionPool::GetAvailableCount() const {
    return pool_.size();
}

void ConnectionPool::Close() {
    std::lock_guard<std::mutex> lock(mutex_);

    while (!pool_.empty()) {
        auto conn = pool_.front();
        pool_.pop();
        // 连接自动关闭（shared_ptr 释放时）
    }

    initialized_ = false;
    spdlog::info("Database connection pool closed");
}

std::shared_ptr<Connection> ConnectionPool::CreateConnection() {
    try {
        auto conn = std::make_shared<Connection>(conn_str_);
        spdlog::debug("Created new database connection");
        return conn;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create database connection: {}", e.what());
        throw;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
