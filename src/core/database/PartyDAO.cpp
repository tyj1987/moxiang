#include "PartyDAO.hpp"
#include "DatabaseUtils.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Database {

// Shorten type names
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

// ========== 队伍操作 ==========

uint64_t PartyDAO::CreateParty(uint64_t leader_id, uint32_t max_members) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        executor.BeginTransaction();

        // 创建队伍
        std::string sql = "INSERT INTO parties "
            "(leader_id, max_members, exp_distribution, loot_mode, min_loot_level, created_time) "
            "VALUES ($1, $2, 0, 0, 1, CURRENT_TIMESTAMP) "
            "RETURNING party_id";

        auto result = executor.QueryOneParams(sql, {std::to_string(leader_id), std::to_string(max_members)});
        uint64_t party_id = result->Get<uint64_t>(0, "party_id").value_or(0);

        // 添加队长为成员
        // TODO: 获取队长角色信息
        std::string sql2 = "INSERT INTO party_members "
            "(party_id, character_id, character_name, level, job, status, join_time) "
            "VALUES ($1, $2, 'Leader', 1, 0, 0, CURRENT_TIMESTAMP)";

        executor.ExecuteParams(sql2, {std::to_string(party_id), std::to_string(leader_id)});

        executor.Commit();

        spdlog::info("Party created: party_id={}, leader_id={}, max_members={}",
                     party_id, leader_id, max_members);

        return party_id;

    } catch (const std::exception& e) {
        executor.Rollback();
        spdlog::error("Failed to create party: {}", e.what());
        return 0;
    }
}

bool PartyDAO::DisbandParty(uint64_t party_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        executor.BeginTransaction();

        // 删除所有成员
        executor.ExecuteParams(
            "DELETE FROM party_members WHERE party_id = $1", {std::to_string(party_id)}
        );

        // 删除队伍
        executor.ExecuteParams(
            "DELETE FROM parties WHERE party_id = $1", {std::to_string(party_id)}
        );

        executor.Commit();

        spdlog::info("Party disbanded: party_id={}", party_id);

        return true;

    } catch (const std::exception& e) {
        executor.Rollback();
        spdlog::error("Failed to disband party: {}", e.what());
        return false;
    }
}

bool PartyDAO::AddPartyMember(
    uint64_t party_id,
    uint64_t character_id,
    const std::string& character_name,
    uint16_t level,
    uint8_t job
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "INSERT INTO party_members "
            "(party_id, character_id, character_name, level, job, status, join_time) "
            "VALUES ($1, $2, $3, $4, $5, 0, CURRENT_TIMESTAMP)";

        executor.ExecuteParams(sql, {std::to_string(party_id), std::to_string(character_id), character_name, std::to_string(level), std::to_string(job)});

        spdlog::info("Party member added: party_id={}, character_id={}, name={}",
                     party_id, character_id, character_name);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to add party member: {}", e.what());
        return false;
    }
}

bool PartyDAO::RemovePartyMember(uint64_t party_id, uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "DELETE FROM party_members "
            "WHERE party_id = $1 AND character_id = $2";

        executor.ExecuteParams(sql, {std::to_string(party_id), std::to_string(character_id)});

        spdlog::info("Party member removed: party_id={}, character_id={}",
                     party_id, character_id);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to remove party member: {}", e.what());
        return false;
    }
}

bool PartyDAO::TransferLeadership(uint64_t party_id, uint64_t new_leader_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "UPDATE parties "
            "SET leader_id = $1 WHERE party_id = $2";
        executor.ExecuteParams(sql, {std::to_string(new_leader_id), std::to_string(party_id)});

        spdlog::info("Party leadership transferred: party_id={}, new_leader={}",
                     party_id, new_leader_id);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to transfer leadership: {}", e.what());
        return false;
    }
}

bool PartyDAO::UpdateMemberStatus(
    uint64_t party_id,
    uint64_t character_id,
    Game::PartyMemberStatus status
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "UPDATE party_members "
            "SET status = $1 WHERE party_id = $2 AND character_id = $3";
        executor.ExecuteParams(sql, {std::to_string(static_cast<uint8_t>(status)), std::to_string(party_id), std::to_string(character_id)});

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update member status: {}", e.what());
        return false;
    }
}

bool PartyDAO::UpdateMemberPosition(
    uint64_t party_id,
    uint64_t character_id,
    float x, float y, float z
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "UPDATE party_members "
            "SET x = $1, y = $2, z = $3 "
            "WHERE party_id = $4 AND character_id = $5";
        executor.ExecuteParams(sql, {std::to_string(x), std::to_string(y), std::to_string(z), std::to_string(party_id), std::to_string(character_id)});

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update member position: {}", e.what());
        return false;
    }
}

// ========== 队伍查询 ==========

std::optional<Game::Party> PartyDAO::GetParty(uint64_t party_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 获取队伍信息
        std::string sql = "SELECT party_id, leader_id, max_members, exp_distribution, "
            "       loot_mode, min_loot_level, created_time "
            "FROM parties "
            "WHERE party_id = $1";

        auto result = executor.QueryOneParams(sql, {std::to_string(party_id)});

        Game::Party party;
        party.party_id = result->Get<uint64_t>(0, "party_id").value_or(0);
        party.leader_id = result->Get<uint64_t>(0, "leader_id").value_or(0);
        party.settings.exp_type = static_cast<Game::PartyExpDistributionType>(
            result->Get<uint8_t>(0, "exp_distribution").value_or(0)
        );
        party.settings.loot_mode = static_cast<Game::PartyLootMode>(
            result->Get<uint8_t>(0, "loot_mode").value_or(0)
        );
        party.settings.max_members = result->Get<uint8_t>(0, "max_members").value_or(0);
        party.settings.min_loot_level = result->Get<uint16_t>(0, "min_loot_level").value_or(0);

        // 获取成员列表
        party.members = GetPartyMembers(party_id);

        return party;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get party: {}", e.what());
        return std::nullopt;
    }
}

uint64_t PartyDAO::GetPartyID(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT party_id FROM party_members "
            "WHERE character_id = $1";

        auto result = executor.QueryOneParams(sql, {std::to_string(character_id)});
        return result->Get<uint64_t>(0, "party_id").value_or(0);

    } catch (const std::exception& e) {
        spdlog::error("Failed to get party ID: {}", e.what());
        return 0;
    }
}

std::vector<Game::PartyMember> PartyDAO::GetPartyMembers(uint64_t party_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<Game::PartyMember> members;

    try {
        std::string sql = "SELECT character_id, character_name, level, job, status, x, y, z "
            "FROM party_members "
            "WHERE party_id = $1 "
            "ORDER BY join_time";

        auto result = executor.QueryParams(sql, {std::to_string(party_id)});

        members.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            Game::PartyMember member;
            member.character_id = result->Get<uint64_t>(i, "character_id").value_or(0);
            member.character_name = result->GetValue(i, "character_name");
            member.level = result->Get<uint16_t>(i, "level").value_or(0);
            member.job = result->Get<uint8_t>(i, "job").value_or(0);
            member.status = static_cast<Game::PartyMemberStatus>(
                result->Get<uint8_t>(i, "status").value_or(0)
            );

            // TODO: 加载HP/MP信息
            member.hp = 0;
            member.max_hp = 0;
            member.mp = 0;
            member.max_mp = 0;

            member.x = result->Get<float>(i, "x").value_or(0.0f);
            member.y = result->Get<float>(i, "y").value_or(0.0f);
            member.z = result->Get<float>(i, "z").value_or(0.0f);

            members.push_back(member);
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get party members: {}", e.what());
    }

    return members;
}

size_t PartyDAO::GetPartyMemberCount(uint64_t party_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT COUNT(*) as count FROM party_members "
            "WHERE party_id = $1";

        auto result = executor.QueryOneParams(sql, {std::to_string(party_id)});
        return result->Get<size_t>(0, "count").value_or(0);

    } catch (const std::exception& e) {
        spdlog::error("Failed to get party member count: {}", e.what());
        return 0;
    }
}

// ========== 队伍设置 ==========

bool PartyDAO::UpdateExpDistribution(
    uint64_t party_id,
    Game::PartyExpDistributionType exp_type
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "UPDATE parties "
            "SET exp_distribution = $1 WHERE party_id = $2";
        executor.ExecuteParams(sql, {std::to_string(static_cast<uint8_t>(exp_type)), std::to_string(party_id)});

        spdlog::info("Party exp distribution updated: party_id={}, type={}",
                     party_id, static_cast<int>(exp_type));

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update exp distribution: {}", e.what());
        return false;
    }
}

bool PartyDAO::UpdateLootMode(
    uint64_t party_id,
    Game::PartyLootMode loot_mode
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "UPDATE parties "
            "SET loot_mode = $1 WHERE party_id = $2";
        executor.ExecuteParams(sql, {std::to_string(static_cast<uint8_t>(loot_mode)), std::to_string(party_id)});

        spdlog::info("Party loot mode updated: party_id={}, mode={}",
                     party_id, static_cast<int>(loot_mode));

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update loot mode: {}", e.what());
        return false;
    }
}

bool PartyDAO::UpdateMinLootLevel(
    uint64_t party_id,
    uint16_t min_level
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "UPDATE parties "
            "SET min_loot_level = $1 WHERE party_id = $2";
        executor.ExecuteParams(sql, {std::to_string(min_level), std::to_string(party_id)});

        spdlog::info("Party min loot level updated: party_id={}, level={}",
                     party_id, min_level);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update min loot level: {}", e.what());
        return false;
    }
}

// ========== 队伍邀请 ==========

uint64_t PartyDAO::CreatePartyInvite(
    uint64_t party_id,
    uint64_t inviter_id,
    uint64_t invitee_id
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "INSERT INTO party_invites "
            "(party_id, inviter_id, invitee_id, invite_time, expire_time) "
            "VALUES ($1, $2, $3, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP + INTERVAL '5 minutes') "
            "RETURNING invite_id";

        auto result = executor.QueryOneParams(sql, {std::to_string(party_id), std::to_string(inviter_id), std::to_string(invitee_id)});
        uint64_t invite_id = result->Get<uint64_t>(0, "invite_id").value_or(0);

        spdlog::info("Party invite created: invite_id={}, party_id={}, inviter={}, invitee={}",
                     invite_id, party_id, inviter_id, invitee_id);

        return invite_id;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create party invite: {}", e.what());
        return 0;
    }
}

std::vector<Game::PartyInvite> PartyDAO::GetPendingInvites(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<Game::PartyInvite> invites;

    try {
        std::string sql = "SELECT invite_id, party_id, inviter_id, invitee_id, invite_time, expire_time "
            "FROM party_invites "
            "WHERE invitee_id = $1 AND CURRENT_TIMESTAMP < expire_time "
            "ORDER BY invite_time DESC";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        invites.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            Game::PartyInvite invite;
            invite.invite_id = result->Get<uint64_t>(i, "invite_id").value_or(0);
            invite.party_id = result->Get<uint64_t>(i, "party_id").value_or(0);
            invite.inviter_id = result->Get<uint64_t>(i, "inviter_id").value_or(0);
            invite.invitee_id = result->Get<uint64_t>(i, "invitee_id").value_or(0);

            // 解析 invite_time 时间戳 - 对应 legacy 时间戳解析
            std::string time_str = result->GetValue(i, "invite_time");
            if (!time_str.empty()) {
                auto time_point = DatabaseUtils::ParseTimestamp(time_str);
                if (time_point.has_value()) {
                    invite.invite_time = time_point.value();
                }
            }

            invites.push_back(invite);
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get pending invites: {}", e.what());
    }

    return invites;
}

bool PartyDAO::DeleteInvite(uint64_t invite_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "DELETE FROM party_invites "
            "WHERE invite_id = $1";

        executor.ExecuteParams(sql, {std::to_string(invite_id)});

        spdlog::info("Party invite deleted: invite_id={}", invite_id);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to delete party invite: {}", e.what());
        return false;
    }
}

size_t PartyDAO::CleanupExpiredInvites(uint32_t expire_seconds) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // Execute返回影响行数
        size_t deleted_count = executor.Execute(
            "DELETE FROM party_invites "
            "WHERE CURRENT_TIMESTAMP > expire_time"
        );

        spdlog::info("Cleaned up {} expired party invites ({}s)",
                     deleted_count, expire_seconds);

        return deleted_count;

    } catch (const std::exception& e) {
        spdlog::error("Failed to cleanup expired invites: {}", e.what());
        return 0;
    }
}

// ========== 队伍统计 ==========

bool PartyDAO::IsPartyFull(uint64_t party_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT p.max_members, COUNT(m.character_id) as member_count "
            "FROM parties p "
            "LEFT JOIN party_members m ON p.party_id = m.party_id "
            "WHERE p.party_id = $1 "
            "GROUP BY p.max_members";

        auto result = executor.QueryOneParams(sql, {std::to_string(party_id)});
        uint32_t max_members = result->Get<uint32_t>(0, "max_members").value_or(0);
        size_t member_count = result->Get<size_t>(0, "member_count").value_or(0);
        return member_count >= max_members;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check if party is full: {}", e.what());
        return false;
    }
}

bool PartyDAO::IsInParty(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT 1 FROM party_members "
            "WHERE character_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        return result->RowCount() > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check if character is in party: {}", e.what());
        return false;
    }
}

bool PartyDAO::IsPartyLeader(uint64_t party_id, uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT 1 FROM parties "
            "WHERE party_id = $1 AND leader_id = $2";

        auto result = executor.QueryParams(sql, {std::to_string(party_id), std::to_string(character_id)});

        return result->RowCount() > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check if character is party leader: {}", e.what());
        return false;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
