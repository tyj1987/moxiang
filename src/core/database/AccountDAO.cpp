#include "AccountDAO.hpp"
#include "DatabaseUtils.hpp"
#include "../spdlog_wrapper.hpp"
#include <sstream>
#include <random>

namespace Murim {
namespace Core {
namespace Database {

// ========== 账号操作 ==========

uint64_t AccountDAO::Create(const std::string& username,
                           const std::string& password_hash,
                           const std::string& salt,
                           const std::string& email) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return 0;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO accounts (username, password_hash, salt, email, create_time, status) "
                         "VALUES ($1, $2, $3, $4, NOW(), 0) "
                         "RETURNING account_id";

        auto row = executor.QueryOneParams(sql, {username, password_hash, salt, email});
        uint64_t account_id = row->Get<uint64_t>(0, "account_id").value_or(0);

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Created account: {}", account_id);

        return account_id;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create account {}: {}", username, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return 0;
    }
}

std::optional<Game::Account> AccountDAO::LoadByUsername(const std::string& username) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT account_id, username, password_hash, salt, email, phone, "
                         "create_time, last_login_time, last_login_ip, "
                         "status, ban_reason, ban_expire_time "
                         "FROM accounts WHERE username = $1";

        auto row = executor.QueryOneParams(sql, {username});
        ConnectionPool::Instance().ReturnConnection(conn);

        return RowToAccount(*row, 0);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load account by username {}: {}", username, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

std::optional<Game::Account> AccountDAO::Load(uint64_t account_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询
        std::string sql = "SELECT account_id, username, password_hash, salt, email, phone, "
                         "create_time, last_login_time, last_login_ip, "
                         "status, ban_reason, ban_expire_time "
                         "FROM accounts WHERE account_id = $1";

        auto row = executor.QueryOneParams(sql, {std::to_string(account_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return RowToAccount(*row, 0);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load account {}: {}", account_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

bool AccountDAO::Exists(const std::string& username) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT COUNT(*) as count FROM accounts WHERE username = $1";

        auto row = executor.QueryOneParams(sql, {username});
        ConnectionPool::Instance().ReturnConnection(conn);

        int count = row->Get<int>(0, "count").value_or(0);
        return count > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check account existence {}: {}", username, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool AccountDAO::UpdateLastLoginTime(uint64_t account_id, const std::string& ip) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE accounts SET last_login_time = NOW(), last_login_ip = $1 "
                         "WHERE account_id = $2";

        size_t affected = executor.ExecuteParams(sql, {ip, std::to_string(account_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update last login time for account {}: {}", account_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool AccountDAO::Ban(uint64_t account_id, const std::string& reason,
                     const std::chrono::system_clock::time_point& expire_time) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE accounts SET status = 1, ban_reason = $1, ban_expire_time = $2 "
                         "WHERE account_id = $3";

        // 转换时间点到字符串格式
        auto expire_time_t = std::chrono::system_clock::to_time_t(expire_time);
        std::string expire_time_str = std::ctime(&expire_time_t);
        // 移除末尾的换行符
        if (!expire_time_str.empty() && expire_time_str.back() == '\n') {
            expire_time_str.pop_back();
        }

        size_t affected = executor.ExecuteParams(sql, {reason, expire_time_str, std::to_string(account_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to ban account {}: {}", account_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool AccountDAO::Unban(uint64_t account_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询
        std::string sql = "UPDATE accounts SET status = 0, ban_reason = NULL, ban_expire_time = NULL "
                         "WHERE account_id = $1";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(account_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to unban account {}: {}", account_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

// ========== 会话操作 ==========

uint64_t AccountDAO::CreateSession(uint64_t account_id,
                                  const std::string& session_token,
                                  uint16_t server_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return 0;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO sessions (account_id, session_token, server_id, login_time, last_active_time) "
                         "VALUES ($1, $2, $3, NOW(), NOW()) "
                         "RETURNING session_id";

        auto row = executor.QueryOneParams(sql, {std::to_string(account_id), session_token, std::to_string(server_id)});
        uint64_t session_id = row->Get<uint64_t>(0, "session_id").value_or(0);

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Created session {} for account {}", session_id, account_id);

        return session_id;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create session for account {}: {}", account_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return 0;
    }
}

std::optional<Game::Session> AccountDAO::LoadSession(uint64_t session_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询
        std::string sql = "SELECT session_id, account_id, session_token, server_id, character_id, "
                         "login_time, last_active_time, logout_time, logout_reason "
                         "FROM sessions WHERE session_id = $1";

        auto row = executor.QueryOneParams(sql, {std::to_string(session_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return RowToSession(*row, 0);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load session {}: {}", session_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

std::optional<Game::Session> AccountDAO::LoadSessionByToken(const std::string& session_token) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT session_id, account_id, session_token, server_id, character_id, "
                         "login_time, last_active_time, logout_time, logout_reason "
                         "FROM sessions WHERE session_token = $1 AND logout_time IS NULL";

        auto row = executor.QueryOneParams(sql, {session_token});
        ConnectionPool::Instance().ReturnConnection(conn);

        return RowToSession(*row, 0);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load session by token: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

bool AccountDAO::UpdateSessionActiveTime(uint64_t session_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询
        std::string sql = "UPDATE sessions SET last_active_time = NOW() "
                         "WHERE session_id = $1";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(session_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update session active time {}: {}", session_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool AccountDAO::SelectCharacter(uint64_t session_id, uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询
        std::string sql = "UPDATE sessions SET character_id = $1 WHERE session_id = $2";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(character_id), std::to_string(session_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to select character {} for session {}: {}", character_id, session_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool AccountDAO::LogoutSession(uint64_t session_id, uint8_t reason) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询
        std::string sql = "UPDATE sessions SET logout_time = NOW(), logout_reason = $1 "
                         "WHERE session_id = $2";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(reason), std::to_string(session_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to logout session {}: {}", session_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

std::vector<Game::Session> AccountDAO::GetActiveSessions(uint64_t account_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return {};
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询
        std::string sql = "SELECT session_id, account_id, session_token, server_id, character_id, "
                         "login_time, last_active_time, logout_time, logout_reason "
                         "FROM sessions WHERE account_id = $1 AND logout_time IS NULL "
                         "ORDER BY login_time DESC";

        auto result = executor.QueryParams(sql, {std::to_string(account_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        std::vector<Game::Session> sessions;
        for (size_t i = 0; i < result->RowCount(); ++i) {
            auto session = RowToSession(*result, i);
            if (session.has_value()) {
                sessions.push_back(session.value());
            }
        }

        return sessions;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get active sessions for account {}: {}", account_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return {};
    }
}

size_t AccountDAO::KickAllSessions(uint64_t account_id, uint8_t reason) {
    auto active_sessions = GetActiveSessions(account_id);
    size_t kicked_count = 0;

    for (const auto& session : active_sessions) {
        if (LogoutSession(session.session_id, reason)) {
            kicked_count++;
        }
    }

    if (kicked_count > 0) {
        spdlog::info("Kicked {} sessions for account {}, reason={}",
                     kicked_count, account_id, reason);
    }

    return kicked_count;
}

// ========== 辅助方法 ==========

std::optional<Game::Account> AccountDAO::RowToAccount(Result& result, int row) {
    try {
        Game::Account account;

        account.account_id = result.Get<uint64_t>(row, "account_id").value_or(0);
        account.username = result.GetValue(row, "username");
        account.password_hash = result.GetValue(row, "password_hash");
        account.salt = result.GetValue(row, "salt");

        std::string email = result.GetValue(row, "email");
        if (!email.empty()) {
            account.email = email;
        }

        std::string phone = result.GetValue(row, "phone");
        if (!phone.empty()) {
            account.phone = phone;
        }

        // 处理时间戳 - 对应 legacy 时间戳解析
        std::string create_time_str = result.GetValue(row, "create_time");
        if (!create_time_str.empty()) {
            auto time_point = DatabaseUtils::ParseTimestamp(create_time_str);
            if (time_point.has_value()) {
                account.create_time = time_point.value();
            }
        }

        std::string last_login_time_str = result.GetValue(row, "last_login_time");
        if (!last_login_time_str.empty()) {
            auto time_point = DatabaseUtils::ParseTimestamp(last_login_time_str);
            if (time_point.has_value()) {
                account.last_login_time = time_point.value();
            }
        }

        std::string last_login_ip = result.GetValue(row, "last_login_ip");
        if (!last_login_ip.empty()) {
            account.last_login_ip = last_login_ip;
        }

        account.status = result.Get<uint8_t>(row, "status").value_or(0);

        std::string ban_reason = result.GetValue(row, "ban_reason");
        if (!ban_reason.empty()) {
            account.ban_reason = ban_reason;
        }

        std::string ban_expire_time_str = result.GetValue(row, "ban_expire_time");
        if (!ban_expire_time_str.empty()) {
            // 解析封禁过期时间 - 对应 legacy 时间戳解析
            auto time_point = DatabaseUtils::ParseTimestamp(ban_expire_time_str);
            if (time_point.has_value()) {
                account.ban_expire_time = time_point.value();
            }
        }

        return account;

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse account row: {}", e.what());
        return std::nullopt;
    }
}

std::optional<Game::Session> AccountDAO::RowToSession(Result& result, int row) {
    try {
        Game::Session session;

        session.session_id = result.Get<uint64_t>(row, "session_id").value_or(0);
        session.account_id = result.Get<uint64_t>(row, "account_id").value_or(0);
        session.session_token = result.GetValue(row, "session_token");
        session.server_id = result.Get<uint16_t>(row, "server_id").value_or(0);

        auto character_id = result.Get<uint64_t>(row, "character_id");
        if (character_id.has_value()) {
            session.character_id = character_id.value();
        }

        // 处理时间戳 - 对应 legacy 时间戳解析
        std::string login_time_str = result.GetValue(row, "login_time");
        if (!login_time_str.empty()) {
            auto time_point = DatabaseUtils::ParseTimestamp(login_time_str);
            if (time_point.has_value()) {
                session.login_time = time_point.value();
            }
        }

        std::string last_active_time_str = result.GetValue(row, "last_active_time");
        if (!last_active_time_str.empty()) {
            auto time_point = DatabaseUtils::ParseTimestamp(last_active_time_str);
            if (time_point.has_value()) {
                session.last_active_time = time_point.value();
            }
        }

        std::string logout_time_str = result.GetValue(row, "logout_time");
        if (!logout_time_str.empty()) {
            auto time_point = DatabaseUtils::ParseTimestamp(logout_time_str);
            if (time_point.has_value()) {
                session.logout_time = time_point.value();
            }
        }

        auto logout_reason = result.Get<uint8_t>(row, "logout_reason");
        if (logout_reason.has_value()) {
            session.logout_reason = logout_reason.value();
        }

        return session;

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse session row: {}", e.what());
        return std::nullopt;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim

