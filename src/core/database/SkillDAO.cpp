#include "SkillDAO.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Database {

// Shorten type names
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

// ========== 技能模板操作 ==========

std::optional<Game::Skill> SkillDAO::Load(uint32_t skill_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT skill_id, name, description, "
            "skill_type, target_type, area_shape, "
            "damage_type, base_damage, damage_multiplier, attribute, "
            "range, radius, angle, "
            "mp_cost, stamina_cost, cooldown, global_cooldown, "
            "required_level "
            "FROM skill_templates "
            "WHERE skill_id = $1";

        auto result = executor.QueryOneParams(sql, {std::to_string(skill_id)});
        Game::Skill skill;

        skill.skill_id = result->Get<uint32_t>(0, "skill_id").value_or(0);
        skill.name = result->GetValue(0, "name");
        skill.description = result->GetValue(0, "description");

        skill.skill_type = static_cast<Game::SkillType>(result->Get<uint8_t>(0, "skill_type").value_or(0));
        skill.target_type = static_cast<Game::SkillTargetType>(result->Get<uint8_t>(0, "target_type").value_or(0));
        skill.area_shape = static_cast<Game::SkillAreaShape>(result->Get<uint8_t>(0, "area_shape").value_or(0));

        skill.damage_type = static_cast<Game::DamageType>(result->Get<uint8_t>(0, "damage_type").value_or(0));
        skill.base_damage = result->Get<uint32_t>(0, "base_damage").value_or(0);
        skill.damage_multiplier = result->Get<float>(0, "damage_multiplier").value_or(0.0f);
        skill.attribute = static_cast<Game::AttributeType>(result->Get<uint8_t>(0, "attribute").value_or(0));

        skill.range = result->Get<float>(0, "range").value_or(0.0f);
        skill.radius = result->Get<float>(0, "radius").value_or(0.0f);
        skill.angle = result->Get<float>(0, "angle").value_or(0.0f);

        skill.mp_cost = result->Get<uint32_t>(0, "mp_cost").value_or(0);
        skill.stamina_cost = result->Get<uint32_t>(0, "stamina_cost").value_or(0);
        skill.cooldown = result->Get<uint16_t>(0, "cooldown").value_or(0);
        skill.global_cooldown = result->Get<uint16_t>(0, "global_cooldown").value_or(0);

        skill.required_level = result->Get<uint16_t>(0, "required_level").value_or(0);

        // 加载技能效果 (如果有)
        // TODO: 从 skill_effects 表加载

        // 加载前置技能 (如果有)
        // TODO: 从 skill_prerequisites 表加载

        return skill;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load skill {}: {}", skill_id, e.what());
        return std::nullopt;
    }
}

std::vector<Game::Skill> SkillDAO::LoadAll() {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<Game::Skill> skills;

    try {
        auto result = executor.Query(
            "SELECT skill_id, name, description, "
            "skill_type, target_type, area_shape, "
            "damage_type, base_damage, damage_multiplier, attribute, "
            "range, radius, angle, "
            "mp_cost, stamina_cost, cooldown, global_cooldown, "
            "required_level "
            "FROM skill_templates "
            "ORDER BY skill_id"
        );

        skills.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            Game::Skill skill;

            skill.skill_id = result->Get<uint32_t>(i, "skill_id").value_or(0);
            skill.name = result->GetValue(i, "name");
            skill.description = result->GetValue(i, "description");

            skill.skill_type = static_cast<Game::SkillType>(result->Get<uint8_t>(i, "skill_type").value_or(0));
            skill.target_type = static_cast<Game::SkillTargetType>(result->Get<uint8_t>(i, "target_type").value_or(0));
            skill.area_shape = static_cast<Game::SkillAreaShape>(result->Get<uint8_t>(i, "area_shape").value_or(0));

            skill.damage_type = static_cast<Game::DamageType>(result->Get<uint8_t>(i, "damage_type").value_or(0));
            skill.base_damage = result->Get<uint32_t>(i, "base_damage").value_or(0);
            skill.damage_multiplier = result->Get<float>(i, "damage_multiplier").value_or(0.0f);
            skill.attribute = static_cast<Game::AttributeType>(result->Get<uint8_t>(i, "attribute").value_or(0));

            skill.range = result->Get<float>(i, "range").value_or(0.0f);
            skill.radius = result->Get<float>(i, "radius").value_or(0.0f);
            skill.angle = result->Get<float>(i, "angle").value_or(0.0f);

            skill.mp_cost = result->Get<uint32_t>(i, "mp_cost").value_or(0);
            skill.stamina_cost = result->Get<uint32_t>(i, "stamina_cost").value_or(0);
            skill.cooldown = result->Get<uint16_t>(i, "cooldown").value_or(0);
            skill.global_cooldown = result->Get<uint16_t>(i, "global_cooldown").value_or(0);

            skill.required_level = result->Get<uint16_t>(i, "required_level").value_or(0);

            skills.push_back(skill);
        }

        spdlog::info("Loaded {} skills", skills.size());

    } catch (const std::exception& e) {
        spdlog::error("Failed to load skills: {}", e.what());
    }

    return skills;
}

bool SkillDAO::Exists(uint32_t skill_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT 1 FROM skill_templates WHERE skill_id = $1";
        auto result = executor.QueryParams(sql, {std::to_string(skill_id)});

        return result->RowCount() > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check skill existence {}: {}", skill_id, e.what());
        return false;
    }
}

// ========== 角色技能操作 ==========

std::vector<uint32_t> SkillDAO::LoadCharacterSkills(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<uint32_t> skill_ids;

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT skill_id FROM character_skills "
            "WHERE character_id = $1 "
            "ORDER BY skill_id";
        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        skill_ids.reserve(result->RowCount());
        for (int i = 0; i < result->RowCount(); ++i) {
            skill_ids.push_back(result->Get<uint32_t>(i, "skill_id").value_or(0));
        }

        spdlog::debug("Loaded {} skills for character {}", skill_ids.size(), character_id);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load character skills {}: {}", character_id, e.what());
    }

    return skill_ids;
}

bool SkillDAO::AddCharacterSkill(
    uint64_t character_id,
    uint32_t skill_id,
    uint16_t skill_level
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        executor.BeginTransaction();

        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO character_skills (character_id, skill_id, skill_level) "
            "VALUES ($1, $2, $3) "
            "ON CONFLICT (character_id, skill_id) "
            "DO UPDATE SET skill_level = $3";

        executor.ExecuteParams(sql, {std::to_string(character_id), std::to_string(skill_id), std::to_string(skill_level)});

        executor.Commit();

        spdlog::info("Added skill {} to character {} (level {})",
                     skill_id, character_id, skill_level);

        return true;

    } catch (const std::exception& e) {
        executor.Rollback();
        spdlog::error("Failed to add skill {} to character {}: {}",
                      skill_id, character_id, e.what());
        return false;
    }
}

bool SkillDAO::RemoveCharacterSkill(
    uint64_t character_id,
    uint32_t skill_id
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "DELETE FROM character_skills "
            "WHERE character_id = $1 AND skill_id = $2";

        executor.ExecuteParams(sql, {std::to_string(character_id), std::to_string(skill_id)});

        spdlog::info("Removed skill {} from character {}", skill_id, character_id);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to remove skill {} from character {}: {}",
                      skill_id, character_id, e.what());
        return false;
    }
}

bool SkillDAO::HasCharacterSkill(
    uint64_t character_id,
    uint32_t skill_id
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT 1 FROM character_skills "
            "WHERE character_id = $1 AND skill_id = $2";
        auto result = executor.QueryParams(sql, {std::to_string(character_id), std::to_string(skill_id)});

        return result->RowCount() > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check character skill {}: {}", character_id, e.what());
        return false;
    }
}

bool SkillDAO::UpdateSkillLevel(
    uint64_t character_id,
    uint32_t skill_id,
    uint16_t skill_level
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE character_skills "
            "SET skill_level = $1 "
            "WHERE character_id = $2 AND skill_id = $3";

        executor.ExecuteParams(sql, {std::to_string(skill_level), std::to_string(character_id), std::to_string(skill_id)});

        spdlog::info("Updated skill {} level to {} for character {}",
                     skill_id, skill_level, character_id);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update skill level {}: {}", character_id, e.what());
        return false;
    }
}

uint16_t SkillDAO::GetSkillLevel(
    uint64_t character_id,
    uint32_t skill_id
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT skill_level FROM character_skills "
            "WHERE character_id = $1 AND skill_id = $2";
        auto result = executor.QueryParams(sql, {std::to_string(character_id), std::to_string(skill_id)});

        if (result->RowCount() == 0) {
            return 0;  // 未学会
        }

        return result->Get<uint16_t>(0, "skill_level").value_or(0);

    } catch (const std::exception& e) {
        spdlog::error("Failed to get skill level {}: {}", character_id, e.what());
        return 0;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
