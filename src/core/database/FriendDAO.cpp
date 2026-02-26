#include "FriendDAO.hpp"
#include "DatabaseUtils.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"
#include "shared/social/Friend.hpp"  // Full type definitions
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Database {

// Shorten type names
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

// Import Game types into Database namespace for this file
using Murim::Game::Friend;
using Murim::Game::FriendRequest;
using Murim::Game::FriendStatus;
using Murim::Game::FriendRequestStatus;

// ========== 好友关系操作 ==========

bool FriendDAO::AddFriend(uint64_t character_id, uint64_t friend_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        executor.BeginTransaction();

        // 检查是否已经是好友 - 使用参数化查询防止SQL注入
        std::string sql = "SELECT 1 FROM friends "
            "WHERE (character_id = $1 AND friend_id = $2) "
            "   OR (character_id = $2 AND friend_id = $1)";

        auto result = executor.QueryParams(sql, {std::to_string(character_id), std::to_string(friend_id)});

        if (result->RowCount() > 0) {
            executor.Rollback();
            spdlog::warn("Friend relationship already exists: {} <-> {}",
                         character_id, friend_id);
            return false;
        }

        // 添加双向好友关系 - 使用参数化查询防止SQL注入
        std::string sql2 = "INSERT INTO friends (character_id, friend_id) "
            "VALUES ($1, $2), ($2, $1)";

        executor.ExecuteParams(sql2, {std::to_string(character_id), std::to_string(friend_id)});

        executor.Commit();

        spdlog::info("Friend relationship added: {} <-> {}", character_id, friend_id);

        return true;

    } catch (const std::exception& e) {
        executor.Rollback();
        spdlog::error("Failed to add friend relationship: {}", e.what());
        return false;
    }
}

bool FriendDAO::RemoveFriend(uint64_t character_id, uint64_t friend_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 删除双向好友关系 - 使用参数化查询防止SQL注入
        std::string sql = "DELETE FROM friends "
            "WHERE (character_id = $1 AND friend_id = $2) "
            "   OR (character_id = $2 AND friend_id = $1)";

        executor.ExecuteParams(sql, {std::to_string(character_id), std::to_string(friend_id)});

        spdlog::info("Friend relationship removed: {} <-> {}", character_id, friend_id);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to remove friend relationship: {}", e.what());
        return false;
    }
}

bool FriendDAO::IsFriend(uint64_t character_id, uint64_t friend_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT 1 FROM friends "
            "WHERE character_id = $1 AND friend_id = $2";

        auto result = executor.QueryParams(sql, {std::to_string(character_id), std::to_string(friend_id)});

        return result->RowCount() > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check friend relationship: {}", e.what());
        return false;
    }
}

std::vector<uint64_t> FriendDAO::GetFriendList(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<uint64_t> friend_id_list;

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT friend_id FROM friends "
            "WHERE character_id = $1 "
            "ORDER BY friend_id";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        friend_id_list.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            friend_id_list.push_back(result->Get<uint64_t>(i, "friend_id").value_or(0));
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get friend list: {}", e.what());
    }

    return friend_id_list;
}

std::vector<Friend> FriendDAO::GetFriendInfo(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<Friend> friends;

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT f.friend_id, c.character_name, c.level, c.job, c.status, c.last_logout "
            "FROM friends f "
            "JOIN characters c ON f.friend_id = c.character_id "
            "WHERE f.character_id = $1 "
            "ORDER BY c.status DESC, c.level DESC";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        friends.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            Friend friend_info;
            friend_info.character_id = result->Get<uint64_t>(i, "friend_id").value_or(0);
            friend_info.character_name = result->GetValue(i, "character_name");
            friend_info.level = result->Get<uint16_t>(i, "level").value_or(0);
            friend_info.job = result->Get<uint8_t>(i, "job").value_or(0);
            friend_info.status = static_cast<FriendStatus>(result->Get<uint8_t>(i, "status").value_or(0));

            // 解析 last_logout 时间戳 - 对应 legacy 时间戳解析
            if (!result->IsNull(i, result->ColumnIndex("last_logout"))) {
                auto time_str = result->GetValue(i, "last_logout");
                auto time_point = DatabaseUtils::ParseTimestamp(time_str);
                if (time_point.has_value()) {
                    friend_info.last_logout = time_point.value();
                }
            }

            friends.push_back(friend_info);
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get friend info: {}", e.what());
    }

    return friends;
}

std::optional<Friend> FriendDAO::GetSingleFriendInfo(uint64_t character_id) {
    auto friends = GetFriendInfo(character_id);
    if (!friends.empty()) {
        return friends[0];
    }
    return std::nullopt;
}

// ========== 好友请求操作 ==========

uint64_t FriendDAO::CreateFriendRequest(
    uint64_t requester_id,
    uint64_t target_id,
    const std::string& message
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        executor.BeginTransaction();

        // 检查是否已经发送过请求 - 使用参数化查询防止SQL注入
        std::string sql = "SELECT 1 FROM friend_requests "
            "WHERE requester_id = $1 AND target_id = $2 AND status = 0";

        auto result = executor.QueryParams(sql, {std::to_string(requester_id), std::to_string(target_id)});

        if (result->RowCount() > 0) {
            executor.Rollback();
            spdlog::warn("Friend request already exists: {} -> {}", requester_id, target_id);
            return 0;
        }

        // 创建好友请求 - 使用参数化查询防止SQL注入
        std::string sql2 = "INSERT INTO friend_requests "
            "(requester_id, target_id, status, message, request_time, expire_time) "
            "VALUES ($1, $2, 0, $3, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP + INTERVAL '7 days') "
            "RETURNING request_id";

        auto result2 = executor.QueryParams(sql2, {std::to_string(requester_id), std::to_string(target_id), message});

        uint64_t request_id = result2->Get<uint64_t>(0, 0).value_or(0);

        executor.Commit();

        spdlog::info("Friend request created: request_id={}, {} -> {}",
                 request_id, requester_id, target_id);

        return request_id;

    } catch (const std::exception& e) {
        executor.Rollback();
        spdlog::error("Failed to create friend request: {}", e.what());
        return 0;
    }
}

std::vector<FriendRequest> FriendDAO::GetPendingRequests(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<FriendRequest> requests;

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT request_id, requester_id, target_id, status, message, "
            "       request_time, expire_time "
            "FROM friend_requests "
            "WHERE target_id = $1 AND status = 0 "
            "ORDER BY request_time DESC";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        requests.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            FriendRequest request;
            request.request_id = result->Get<uint64_t>(i, "request_id").value_or(0);
            request.requester_id = result->Get<uint64_t>(i, "requester_id").value_or(0);
            request.target_id = result->Get<uint64_t>(i, "target_id").value_or(0);
            request.status = static_cast<FriendRequestStatus>(result->Get<uint8_t>(i, "status").value_or(0));
            request.message = result->GetValue(i, "message");

            // 解析 request_time 时间戳 - 对应 legacy 时间戳解析
            if (!result->IsNull(i, result->ColumnIndex("request_time"))) {
                auto time_str = result->GetValue(i, "request_time");
                auto time_point = DatabaseUtils::ParseTimestamp(time_str);
                if (time_point.has_value()) {
                    request.request_time = time_point.value();
                }
            }


            requests.push_back(request);
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get pending friend requests: {}", e.what());
    }

    return requests;
}

bool FriendDAO::UpdateRequestStatus(
    uint64_t request_id,
    FriendRequestStatus status
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE friend_requests "
            "SET status = $1 "
            "WHERE request_id = $2";

        executor.ExecuteParams(sql, {std::to_string(static_cast<uint8_t>(status)), std::to_string(request_id)});

        spdlog::debug("Friend request {} status updated to {}",
                     request_id, static_cast<int>(status));

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update friend request status: {}", e.what());
        return false;
    }
}

bool FriendDAO::DeleteFriendRequest(uint64_t request_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "DELETE FROM friend_requests "
            "WHERE request_id = $1";

        executor.ExecuteParams(sql, {std::to_string(request_id)});

        spdlog::info("Friend request {} deleted", request_id);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to delete friend request: {}", e.what());
        return false;
    }
}

size_t FriendDAO::CleanupExpiredRequests(uint32_t expire_days) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "DELETE FROM friend_requests "
            "WHERE status = 0 AND CURRENT_TIMESTAMP > expire_time "
            "RETURNING COUNT";

        auto result = executor.Query(sql);

        size_t deleted_count = result->Get<size_t>(0, 0).value_or(0);

        spdlog::info("Cleaned up {} expired friend requests ({} days)",
                     deleted_count, expire_days);

        return deleted_count;

    } catch (const std::exception& e) {
        spdlog::error("Failed to cleanup expired friend requests: {}", e.what());
        return 0;
    }
}

// ========== 在线状态管理 ==========

bool FriendDAO::UpdateOnlineStatus(
    uint64_t character_id,
    FriendStatus status
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE characters "
            "SET online_status = $1, "
            "last_logout = CASE WHEN $1 != 0 THEN CURRENT_TIMESTAMP ELSE NULL END "
            "WHERE character_id = $2";

        executor.ExecuteParams(sql, {std::to_string(static_cast<uint8_t>(status)), std::to_string(character_id)});

        spdlog::debug("Character {} online status updated to {}",
                     character_id, static_cast<int>(status));

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update online status: {}", e.what());
        return false;
    }
}

std::optional<FriendStatus> FriendDAO::GetOnlineStatus(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT online_status FROM characters "
            "WHERE character_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        if (result->RowCount() > 0) {
            return static_cast<FriendStatus>(result->Get<uint8_t>(0, "online_status").value_or(0));
        }

        return std::optional<FriendStatus>{};

    } catch (const std::exception& e) {
        spdlog::error("Failed to get online status: {}", e.what());
        return std::optional<FriendStatus>{};
    }
}

// ========== 好友统计 ==========

size_t FriendDAO::GetFriendCount(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT COUNT(*) as count FROM friends "
            "WHERE character_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        if (result->RowCount() > 0) {
            return result->Get<size_t>(0, "count").value_or(0);
        }

        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get friend count: {}", e.what());
        return 0;
    }
}

bool FriendDAO::IsFriendListFull(
    uint64_t character_id,
    size_t max_friends
) {
    size_t count = GetFriendCount(character_id);
    return count >= max_friends;
}

} // namespace Database
} // namespace Core
} // namespace Murim
