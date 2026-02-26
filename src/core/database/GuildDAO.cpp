#include "GuildDAO.hpp"
#include "../spdlog_wrapper.hpp"
#include <sstream>
#include <chrono>

using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Core {
namespace Database {

// ============================================================================
// 公会 CRUD 操作
// ============================================================================

uint64_t GuildDAO::CreateGuild(const std::string& guild_name, uint64_t leader_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("[GuildDAO] Failed to get database connection");
        return 0;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        // 对应 legacy: CGuildManager::CreateGuildSyn() (GuildManager.cpp:1477)
        std::string sql = "INSERT INTO guilds (name, leader_id, level, exp, "
                         "member_count, max_members) "
                         "VALUES ($1, $2, 1, 0, 1, 50) "
                         "RETURNING id";

        auto result = executor.QueryOneParams(sql, {guild_name, std::to_string(leader_id)});
        uint64_t guild_id = result->Get<uint64_t>(0, "id").value_or(0);

        // 添加会长为成员
        sql = "INSERT INTO guild_members (guild_id, character_id, rank) "
              "VALUES ($1, $2, 'Leader')";
        executor.ExecuteParams(sql, {std::to_string(guild_id), std::to_string(leader_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("[GuildDAO] Created guild: {} (ID: {}, Leader: {})",
                    guild_name, guild_id, leader_id);

        return guild_id;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to create guild {}: {}", guild_name, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return 0;
    }
}

std::optional<Game::GuildInfo> GuildDAO::LoadGuild(uint64_t guild_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT id, name, leader_id, level, exp, "
                         "member_count, max_members, created_at "
                         "FROM guilds WHERE id = $1";

        auto result = executor.QueryOneParams(sql, {std::to_string(guild_id)});
        auto guild_info = RowToGuildInfo(*result, 0);

        ConnectionPool::Instance().ReturnConnection(conn);
        return guild_info;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to load guild {}: {}", guild_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

std::optional<Game::GuildInfo> GuildDAO::LoadGuildByName(const std::string& guild_name) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT id, name, leader_id, level, exp, "
                         "member_count, max_members, created_at "
                         "FROM guilds WHERE name = $1";

        auto result = executor.QueryOneParams(sql, {guild_name});
        auto guild_info = RowToGuildInfo(*result, 0);

        ConnectionPool::Instance().ReturnConnection(conn);
        return guild_info;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to load guild by name {}: {}", guild_name, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

bool GuildDAO::SaveGuild(const Game::GuildInfo& guild_info) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "UPDATE guilds SET "
                         "name = $2, leader_id = $3, level = $4, exp = $5, "
                         "member_count = $6, max_members = $7 "
                         "WHERE id = $1";

        executor.ExecuteParams(sql,
            {std::to_string(guild_info.guild_id),
             guild_info.guild_name,
             std::to_string(guild_info.leader_id),
             std::to_string(guild_info.level),
             std::to_string(guild_info.exp),
             std::to_string(guild_info.current_members),
             std::to_string(guild_info.max_members)});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::debug("[GuildDAO] Saved guild: {}", guild_info.guild_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to save guild {}: {}",
                     guild_info.guild_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool GuildDAO::DeleteGuild(uint64_t guild_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 对应 legacy: CGuildManager::BreakUpGuildSyn() (GuildManager.cpp:1560)
        // 使用事务确保成员和公会同时删除
        executor.BeginTransaction();

        // 先删除所有成员 (级联删除会自动处理，但显式删除更安全)
        std::string sql = "DELETE FROM guild_members WHERE guild_id = $1";
        executor.ExecuteParams(sql, {std::to_string(guild_id)});

        // 删除公会
        sql = "DELETE FROM guilds WHERE id = $1";
        executor.ExecuteParams(sql, {std::to_string(guild_id)});

        executor.Commit();

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("[GuildDAO] Deleted guild: {}", guild_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to delete guild {}: {}", guild_id, e.what());
        executor.Rollback();
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

// ============================================================================
// 成员管理操作
// ============================================================================

bool GuildDAO::AddMember(uint64_t guild_id, uint64_t character_id,
                         Game::GuildPosition position) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 对应 legacy: CGuildManager::AddMemberSyn() (GuildManager.cpp:1815)
        std::string position_str;
        switch (position) {
            case Game::GuildPosition::kGuildMaster:
                position_str = "Leader";
                break;
            case Game::GuildPosition::kViceMaster:
                position_str = "Officer";
                break;
            case Game::GuildPosition::kMember:
            default:
                position_str = "Member";
                break;
        }

        std::string sql = "INSERT INTO guild_members (guild_id, character_id, rank) "
                         "VALUES ($1, $2, $3)";

        executor.ExecuteParams(sql, {std::to_string(guild_id), std::to_string(character_id), position_str});

        // 更新成员计数
        sql = "UPDATE guilds SET member_count = member_count + 1 WHERE id = $1";
        executor.ExecuteParams(sql, {std::to_string(guild_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("[GuildDAO] Added member {} to guild {}", character_id, guild_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to add member {} to guild {}: {}",
                     character_id, guild_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool GuildDAO::RemoveMember(uint64_t guild_id, uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 对应 legacy: CGuildManager::DeleteMemberSyn() (GuildManager.cpp:1690)
        std::string sql = "DELETE FROM guild_members WHERE "
                         "guild_id = $1 AND character_id = $2";

        size_t rows = executor.ExecuteParams(sql, {std::to_string(guild_id), std::to_string(character_id)});

        if (rows > 0) {
            // 更新成员计数
            sql = "UPDATE guilds SET member_count = member_count - 1 WHERE id = $1";
            executor.ExecuteParams(sql, {std::to_string(guild_id)});

            ConnectionPool::Instance().ReturnConnection(conn);
            spdlog::info("[GuildDAO] Removed member {} from guild {}", character_id, guild_id);
            return true;
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::warn("[GuildDAO] Member {} not found in guild {}", character_id, guild_id);
        return false;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to remove member {} from guild {}: {}",
                     character_id, guild_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool GuildDAO::UpdateMemberPosition(uint64_t guild_id, uint64_t character_id,
                                   Game::GuildPosition position) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string position_str;
        switch (position) {
            case Game::GuildPosition::kGuildMaster:
                position_str = "Leader";
                break;
            case Game::GuildPosition::kViceMaster:
                position_str = "Officer";
                break;
            case Game::GuildPosition::kMember:
            default:
                position_str = "Member";
                break;
        }

        std::string sql = "UPDATE guild_members SET rank = $3 "
                         "WHERE guild_id = $1 AND character_id = $2";

        executor.ExecuteParams(sql, {std::to_string(guild_id), std::to_string(character_id), position_str});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("[GuildDAO] Updated member {} position in guild {} to {}",
                    character_id, guild_id, position_str);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to update member {} position: {}",
                     character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

std::vector<Game::GuildMemberInfo> GuildDAO::LoadMembers(uint64_t guild_id) {
    std::vector<Game::GuildMemberInfo> members;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return members;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT guild_id, character_id, rank, joined_at "
                         "FROM guild_members WHERE guild_id = $1 "
                         "ORDER BY CASE rank "
                         "  WHEN 'Leader' THEN 1 "
                         "  WHEN 'Officer' THEN 2 "
                         "  WHEN 'Member' THEN 3 "
                         "  ELSE 4 "
                         "END";

        auto result = executor.QueryParams(sql, {std::to_string(guild_id)});

        for (int i = 0; i < result->RowCount(); ++i) {
            auto member_info = RowToMemberInfo(*result, i);
            if (member_info) {
                members.push_back(*member_info);
            }
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::debug("[GuildDAO] Loaded {} members for guild {}",
                     members.size(), guild_id);
        return members;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to load members for guild {}: {}",
                     guild_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return members;
    }
}

std::optional<Game::GuildMemberInfo> GuildDAO::LoadMemberInfo(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT guild_id, character_id, rank, joined_at "
                         "FROM guild_members WHERE character_id = $1";

        auto result = executor.QueryOneParams(sql, {std::to_string(character_id)});
        auto member_info = RowToMemberInfo(*result, 0);

        ConnectionPool::Instance().ReturnConnection(conn);
        return member_info;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to load member info for {}: {}",
                     character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

uint32_t GuildDAO::GetMemberCount(uint64_t guild_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return 0;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT COUNT(*) as count FROM guild_members WHERE guild_id = $1";
        auto result = executor.QueryOneParams(sql, {std::to_string(guild_id)});
        uint32_t count = result->Get<uint32_t>(0, "count").value_or(0);

        ConnectionPool::Instance().ReturnConnection(conn);
        return count;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to get member count for guild {}: {}",
                     guild_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return 0;
    }
}

// ============================================================================
// 查询和检查操作
// ============================================================================

bool GuildDAO::GuildExists(uint64_t guild_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT EXISTS(SELECT 1 FROM guilds WHERE id = $1)";
        auto result = executor.QueryOneParams(sql, {std::to_string(guild_id)});
        bool exists = result->Get<bool>(0, "exists").value_or(false);

        ConnectionPool::Instance().ReturnConnection(conn);
        return exists;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to check guild existence: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool GuildDAO::GuildNameExists(const std::string& guild_name) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT EXISTS(SELECT 1 FROM guilds WHERE name = $1)";
        auto result = executor.QueryOneParams(sql, {guild_name});
        bool exists = result->Get<bool>(0, "exists").value_or(false);

        ConnectionPool::Instance().ReturnConnection(conn);
        return exists;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to check guild name existence: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool GuildDAO::IsCharacterInGuild(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT EXISTS(SELECT 1 FROM guild_members WHERE character_id = $1)";
        auto result = executor.QueryOneParams(sql, {std::to_string(character_id)});
        bool exists = result->Get<bool>(0, "exists").value_or(false);

        ConnectionPool::Instance().ReturnConnection(conn);
        return exists;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to check character guild membership: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

// ============================================================================
// 更新操作
// ============================================================================

bool GuildDAO::UpdateGuildLevel(uint64_t guild_id, uint8_t level) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "UPDATE guilds SET level = $2 WHERE id = $1";
        executor.ExecuteParams(sql, {std::to_string(guild_id), std::to_string(level)});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("[GuildDAO] Updated guild {} level to {}", guild_id, level);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to update guild level: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool GuildDAO::UpdateGuildExp(uint64_t guild_id, uint32_t exp) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "UPDATE guilds SET exp = $2 WHERE id = $1";
        executor.ExecuteParams(sql, {std::to_string(guild_id), std::to_string(exp)});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::debug("[GuildDAO] Updated guild {} exp to {}", guild_id, exp);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to update guild exp: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool GuildDAO::UpdateGuildNotice(uint64_t guild_id, const std::string& notice) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "UPDATE guilds SET notice = $2 WHERE id = $1";
        executor.ExecuteParams(sql, {std::to_string(guild_id), notice});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::debug("[GuildDAO] Updated guild {} notice", guild_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to update guild notice: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

// ============================================================================
// 批量加载 (服务器启动)
// ============================================================================

std::vector<Game::GuildInfo> GuildDAO::LoadAllGuilds() {
    std::vector<Game::GuildInfo> guilds;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return guilds;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT id, name, leader_id, level, exp, "
                         "member_count, max_members, created_at "
                         "FROM guilds ORDER BY id";

        auto result = executor.Query(sql);

        for (int i = 0; i < result->RowCount(); ++i) {
            auto guild_info = RowToGuildInfo(*result, i);
            if (guild_info) {
                guilds.push_back(*guild_info);
            }
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("[GuildDAO] Loaded {} guilds from database", guilds.size());
        return guilds;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to load all guilds: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return guilds;
    }
}

std::vector<Game::GuildMemberInfo> GuildDAO::LoadAllMembers() {
    std::vector<Game::GuildMemberInfo> members;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return members;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT guild_id, character_id, rank, joined_at "
                         "FROM guild_members ORDER BY guild_id";

        auto result = executor.Query(sql);

        for (int i = 0; i < result->RowCount(); ++i) {
            auto member_info = RowToMemberInfo(*result, i);
            if (member_info) {
                members.push_back(*member_info);
            }
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("[GuildDAO] Loaded {} guild members from database", members.size());
        return members;

    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to load all members: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return members;
    }
}

// ============================================================================
// 辅助方法
// ============================================================================

std::optional<Game::GuildInfo> GuildDAO::RowToGuildInfo(Result& result, int row) {
    try {
        Game::GuildInfo info;
        info.guild_id = result.Get<uint64_t>(row, "id").value_or(0);
        info.guild_name = result.GetValue(row, "name");
        info.leader_id = result.Get<uint64_t>(row, "leader_id").value_or(0);
        info.level = result.Get<uint8_t>(row, "level").value_or(0);
        info.exp = result.Get<uint32_t>(row, "exp").value_or(0);
        info.current_members = result.Get<uint32_t>(row, "member_count").value_or(0);
        info.max_members = result.Get<uint32_t>(row, "max_members").value_or(0);
        info.notice = "";  // 需要时可以添加 notice 字段
        info.created_time = std::chrono::system_clock::now();

        return info;
    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to parse guild info: {}", e.what());
        return std::nullopt;
    }
}

std::optional<Game::GuildMemberInfo> GuildDAO::RowToMemberInfo(Result& result, int row) {
    try {
        Game::GuildMemberInfo info;
        info.guild_id = result.Get<uint64_t>(row, "guild_id").value_or(0);
        info.character_id = result.Get<uint64_t>(row, "character_id").value_or(0);

        // 解析职位
        std::string rank_str = result.GetValue(row, "rank");
        if (rank_str == "Leader") {
            info.position = Game::GuildPosition::kGuildMaster;
        } else if (rank_str == "Officer") {
            info.position = Game::GuildPosition::kViceMaster;
        } else {
            info.position = Game::GuildPosition::kMember;
        }

        info.join_time = std::chrono::system_clock::now();
        info.contribution = 0;
        info.level = 1;
        info.character_name = "";

        return info;
    } catch (const std::exception& e) {
        spdlog::error("[GuildDAO] Failed to parse member info: {}", e.what());
        return std::nullopt;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
