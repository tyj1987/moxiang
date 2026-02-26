#include "CharacterDAO.hpp"
#include "core/spdlog_wrapper.hpp"
#include <sstream>
#include <chrono>

using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Core {
namespace Database {

uint64_t CharacterDAO::Create(const Game::Character& character) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return 0;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO characters (account_id, name, job_class, initial_weapon, gender, level, exp, "
                         "hp, max_hp, mp, max_mp, stamina, max_stamina, "
                         "face_style, hair_style, hair_color, "
                         "map_id, x, y, z, direction, status) "
                         "VALUES ($1, $2, $3, $4, $5, $6, $7, "
                         "$8, $9, $10, $11, $12, $13, "
                         "$14, $15, $16, "
                         "$17, $18, $19, $20, $21, 0) "
                         "RETURNING character_id";

        auto row = executor.QueryOneParams(sql,
            {std::to_string(character.account_id),
             character.name,
             std::to_string(character.job_class),
             std::to_string(character.initial_weapon),
             std::to_string(character.gender),
             std::to_string(character.level),
             std::to_string(character.exp),
             std::to_string(character.hp),
             std::to_string(character.max_hp),
             std::to_string(character.mp),
             std::to_string(character.max_mp),
             std::to_string(character.stamina),
             std::to_string(character.max_stamina),
             std::to_string(character.face_style),
             std::to_string(character.hair_style),
             std::to_string(character.hair_color),
             std::to_string(character.map_id),
             std::to_string(character.x),
             std::to_string(character.y),
             std::to_string(character.z),
             std::to_string(character.direction)});

        uint64_t character_id = row->Get<uint64_t>(0, "character_id").value_or(0);

        if (character_id == 0) {
            spdlog::error("Failed to get character_id from INSERT");
            ConnectionPool::Instance().ReturnConnection(conn);
            return 0;
        }

        // 创建角色属性记录
        std::string stats_sql = "INSERT INTO character_stats (character_id, strength, intelligence, dexterity, vitality, wisdom, "
                               "physical_attack, physical_defense, magic_attack, magic_defense, "
                               "attack_speed, move_speed, critical_rate, dodge_rate, hit_rate, available_points) "
                               "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16)";
        executor.ExecuteParams(stats_sql,
            {std::to_string(character_id),
             "10",  // strength
             "10",  // intelligence
             "10",  // dexterity
             "10",  // vitality
             "10",  // wisdom
             "0",   // physical_attack
             "0",   // physical_defense
             "0",   // magic_attack
             "0",   // magic_defense
             "100", // attack_speed
             "100", // move_speed
             "5",   // critical_rate
             "5",   // dodge_rate
             "90",  // hit_rate
             "0"}); // available_points

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Created character: {} ({})", character_id, character.name);

        return character_id;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create character {}: {}", character.name, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return 0;
    }
}

std::optional<Game::Character> CharacterDAO::Load(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT character_id, account_id, name, job_class, initial_weapon, gender, level, exp, "
                         "hp, max_hp, mp, max_mp, stamina, max_stamina, "
                         "face_style, hair_style, hair_color, "
                         "map_id, x, y, z, direction, "
                         "create_time, last_login_time, total_play_time, status "
                         "FROM characters WHERE character_id = $1 AND status = 0";  // 只加载未删除的角色

        auto row = executor.QueryOneParams(sql, {std::to_string(character_id)});
        auto character = RowToCharacter(*row, 0);

        ConnectionPool::Instance().ReturnConnection(conn);
        return character;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

std::optional<Game::CharacterStats> CharacterDAO::LoadStats(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT character_id, strength, intelligence, dexterity, vitality, wisdom, "
                         "physical_attack, physical_defense, magic_attack, magic_defense, "
                         "attack_speed, move_speed, critical_rate, dodge_rate, hit_rate, "
                         "available_points "
                         "FROM character_stats WHERE character_id = $1";

        auto row = executor.QueryOneParams(sql, {std::to_string(character_id)});
        auto stats = RowToStats(*row, 0);

        ConnectionPool::Instance().ReturnConnection(conn);
        return stats;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load stats for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

bool CharacterDAO::Save(const Game::Character& character) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE characters SET "
                         "name = $1, initial_weapon = $2, gender = $3, level = $4, exp = $5, "
                         "hp = $6, max_hp = $7, mp = $8, max_mp = $9, stamina = $10, max_stamina = $11, "
                         "face_style = $12, hair_style = $13, hair_color = $14, "
                         "map_id = $15, x = $16, y = $17, z = $18, direction = $19, "
                         "status = $20 "
                         "WHERE character_id = $21";

        size_t affected = executor.ExecuteParams(sql,
            {character.name,
             std::to_string(character.initial_weapon),
             std::to_string(character.gender),
             std::to_string(character.level),
             std::to_string(character.exp),
             std::to_string(character.hp),
             std::to_string(character.max_hp),
             std::to_string(character.mp),
             std::to_string(character.max_mp),
             std::to_string(character.stamina),
             std::to_string(character.max_stamina),
             std::to_string(character.face_style),
             std::to_string(character.hair_style),
             std::to_string(character.hair_color),
             std::to_string(character.map_id),
             std::to_string(character.x),
             std::to_string(character.y),
             std::to_string(character.z),
             std::to_string(character.direction),
             std::to_string(static_cast<uint8_t>(character.status)),
             std::to_string(character.character_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save character {}: {}", character.character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool CharacterDAO::SaveStats(uint64_t character_id, const Game::CharacterStats& stats) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE character_stats SET "
                         "strength = $1, intelligence = $2, dexterity = $3, vitality = $4, wisdom = $5, "
                         "physical_attack = $6, physical_defense = $7, magic_attack = $8, magic_defense = $9, "
                         "attack_speed = $10, move_speed = $11, critical_rate = $12, dodge_rate = $13, "
                         "hit_rate = $14, available_points = $15 "
                         "WHERE character_id = $16";

        size_t affected = executor.ExecuteParams(sql,
            {std::to_string(stats.strength),
             std::to_string(stats.intelligence),
             std::to_string(stats.dexterity),
             std::to_string(stats.vitality),
             std::to_string(stats.wisdom),
             std::to_string(stats.physical_attack),
             std::to_string(stats.physical_defense),
             std::to_string(stats.magic_attack),
             std::to_string(stats.magic_defense),
             std::to_string(stats.attack_speed),
             std::to_string(stats.move_speed),
             std::to_string(stats.critical_rate),
             std::to_string(stats.dodge_rate),
             std::to_string(stats.hit_rate),
             std::to_string(stats.available_points),
             std::to_string(character_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save stats for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool CharacterDAO::Delete(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入 - 软删除
        std::string sql = "UPDATE characters SET status = 1 WHERE character_id = $1";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(character_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        if (affected > 0) {
            spdlog::info("Soft deleted character: {}", character_id);
        }

        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to delete character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

std::vector<Game::Character> CharacterDAO::LoadByAccount(uint64_t account_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return {};
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT character_id, account_id, name, job_class, initial_weapon, gender, level, exp, "
                         "hp, max_hp, mp, max_mp, stamina, max_stamina, "
                         "face_style, hair_style, hair_color, "
                         "map_id, x, y, z, direction, "
                         "create_time, last_login_time, total_play_time, status "
                         "FROM characters WHERE account_id = $1 AND status = 0 "
                         "ORDER BY level DESC, create_time ASC";

        auto result = executor.QueryParams(sql, {std::to_string(account_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        std::vector<Game::Character> characters;
        for (size_t i = 0; i < result->RowCount(); ++i) {
            auto character = RowToCharacter(*result, i);
            if (character.has_value()) {
                characters.push_back(character.value());
            }
        }

        return characters;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load characters for account {}: {}", account_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return {};
    }
}

bool CharacterDAO::UpdatePosition(uint64_t character_id, float x, float y, float z,
                                 uint16_t direction) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE characters SET x = $1, y = $2, z = $3, direction = $4 "
                         "WHERE character_id = $5";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(x), std::to_string(y), std::to_string(z), std::to_string(direction), std::to_string(character_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update position for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool CharacterDAO::UpdateLastLoginTime(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE characters SET last_login_time = NOW() "
                         "WHERE character_id = $1";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(character_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update last login time for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool CharacterDAO::UpdatePlayTime(uint64_t character_id, uint32_t play_time) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE characters SET total_play_time = total_play_time + $1 "
                         "WHERE character_id = $2";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(play_time), std::to_string(character_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update play time for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool CharacterDAO::Exists(const std::string& name) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入（关键：字符串参数必须参数化）
        std::string sql = "SELECT COUNT(*) as count FROM characters WHERE name = $1";

        auto row = executor.QueryOneParams(sql, {name});
        ConnectionPool::Instance().ReturnConnection(conn);

        int count = row->Get<int>(0, "count").value_or(0);
        return count > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check character existence {}: {}", name, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool CharacterDAO::BelongsToAccount(uint64_t character_id, uint64_t account_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT COUNT(*) as count FROM characters "
                         "WHERE character_id = $1 AND account_id = $2 AND status = 0";

        auto row = executor.QueryOneParams(sql, {std::to_string(character_id), std::to_string(account_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        int count = row->Get<int>(0, "count").value_or(0);
        return count > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check ownership for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

std::optional<Game::Character> CharacterDAO::RowToCharacter(Result& result, int row) {
    try {
        Game::Character character;

        character.character_id = result.Get<uint64_t>(row, "character_id").value_or(0);
        character.account_id = result.Get<uint64_t>(row, "account_id").value_or(0);
        character.name = result.GetValue(row, "name");
        character.job_class = result.Get<uint8_t>(row, "job_class").value_or(0);
        character.initial_weapon = result.Get<uint8_t>(row, "initial_weapon").value_or(0);
        character.gender = result.Get<uint8_t>(row, "gender").value_or(0);
        character.level = result.Get<uint16_t>(row, "level").value_or(0);
        character.exp = result.Get<uint64_t>(row, "exp").value_or(0);
        character.hp = result.Get<uint32_t>(row, "hp").value_or(0);
        character.max_hp = result.Get<uint32_t>(row, "max_hp").value_or(0);
        character.mp = result.Get<uint32_t>(row, "mp").value_or(0);
        character.max_mp = result.Get<uint32_t>(row, "max_mp").value_or(0);
        character.stamina = result.Get<uint32_t>(row, "stamina").value_or(0);
        character.max_stamina = result.Get<uint32_t>(row, "max_stamina").value_or(0);
        character.face_style = result.Get<uint16_t>(row, "face_style").value_or(0);
        character.hair_style = result.Get<uint16_t>(row, "hair_style").value_or(0);
        character.hair_color = result.Get<uint32_t>(row, "hair_color").value_or(0);
        character.map_id = result.Get<uint16_t>(row, "map_id").value_or(0);
        character.x = result.Get<float>(row, "x").value_or(0.0f);
        character.y = result.Get<float>(row, "y").value_or(0.0f);
        character.z = result.Get<float>(row, "z").value_or(0.0f);
        character.direction = result.Get<uint16_t>(row, "direction").value_or(0);
        character.status = static_cast<Game::CharacterStatus>(
            result.Get<uint8_t>(row, "status").value_or(static_cast<uint8_t>(Game::CharacterStatus::kAlive))
        );

        // 处理时间戳
        std::string create_time_str = result.GetValue(row, "create_time");
        if (!create_time_str.empty()) {
            character.create_time = std::chrono::system_clock::from_time_t(
                std::stoi(create_time_str.substr(0, 14))
            );
        }
        std::string last_login_time_str = result.GetValue(row, "last_login_time");
        if (!last_login_time_str.empty()) {
            character.last_login_time = std::chrono::system_clock::from_time_t(
                std::stoi(last_login_time_str.substr(0, 14))
            );
        }

        character.total_play_time = result.Get<uint32_t>(row, "total_play_time").value_or(0);

        return character;

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse character row: {}", e.what());
        return std::nullopt;
    }
}

std::optional<Game::CharacterStats> CharacterDAO::RowToStats(Result& result, int row) {
    try {
        Game::CharacterStats stats;

        stats.strength = result.Get<uint16_t>(row, "strength").value_or(0);
        stats.intelligence = result.Get<uint16_t>(row, "intelligence").value_or(0);
        stats.dexterity = result.Get<uint16_t>(row, "dexterity").value_or(0);
        stats.vitality = result.Get<uint16_t>(row, "vitality").value_or(0);
        stats.wisdom = result.Get<uint16_t>(row, "wisdom").value_or(0);

        stats.physical_attack = result.Get<uint32_t>(row, "physical_attack").value_or(0);
        stats.physical_defense = result.Get<uint32_t>(row, "physical_defense").value_or(0);
        stats.magic_attack = result.Get<uint32_t>(row, "magic_attack").value_or(0);
        stats.magic_defense = result.Get<uint32_t>(row, "magic_defense").value_or(0);

        stats.attack_speed = result.Get<uint16_t>(row, "attack_speed").value_or(0);
        stats.move_speed = result.Get<uint16_t>(row, "move_speed").value_or(0);
        stats.critical_rate = result.Get<uint16_t>(row, "critical_rate").value_or(0);
        stats.dodge_rate = result.Get<uint16_t>(row, "dodge_rate").value_or(0);
        stats.hit_rate = result.Get<uint16_t>(row, "hit_rate").value_or(0);

        stats.available_points = result.Get<uint16_t>(row, "available_points").value_or(0);

        return stats;

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse stats row: {}", e.what());
        return std::nullopt;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
