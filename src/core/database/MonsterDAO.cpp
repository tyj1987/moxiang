#include "MonsterDAO.hpp"
#include "../spdlog_wrapper.hpp"
#include <sstream>

using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Core {
namespace Database {

// 辅助函数：从Result行转换为MonsterTemplate
static Game::MonsterTemplate RowToMonsterTemplate(Result& result, int row) {
    Game::MonsterTemplate monster_template;

    monster_template.monster_id = result.Get<uint32_t>(row, 0).value_or(0);
    monster_template.monster_name = result.Get<std::string>(row, 1).value_or("");
    monster_template.eng_name = result.Get<std::string>(row, 2).value_or("");
    monster_template.level = result.Get<uint16_t>(row, 3).value_or(1);
    monster_template.hp = result.Get<uint32_t>(row, 4).value_or(100);
    monster_template.shield = result.Get<uint32_t>(row, 5).value_or(0);
    monster_template.exp_reward = result.Get<uint32_t>(row, 6).value_or(10);
    monster_template.attack_power_min = result.Get<uint16_t>(row, 7).value_or(5);
    monster_template.attack_power_max = result.Get<uint16_t>(row, 8).value_or(8);
    monster_template.critical_percent = result.Get<uint16_t>(row, 9).value_or(5);
    monster_template.physical_defense = result.Get<uint16_t>(row, 10).value_or(3);
    monster_template.walk_speed = result.Get<uint16_t>(row, 11).value_or(100);
    monster_template.run_speed = result.Get<uint16_t>(row, 12).value_or(200);
    monster_template.scale = result.Get<float>(row, 13).value_or(1.0f);
    monster_template.model_file = result.Get<std::string>(row, 14).value_or("");

    // 攻击系统（P0字段）
    monster_template.attack_kind = result.Get<uint8_t>(row, 15).value_or(0);
    monster_template.attack_num = result.Get<uint8_t>(row, 16).value_or(1);
    monster_template.default_attack_index = result.Get<uint32_t>(row, 17).value_or(0);

    // AI行为参数（P0字段）
    monster_template.search_range = result.Get<uint32_t>(row, 18).value_or(500);
    monster_template.target_select = result.Get<uint8_t>(row, 19).value_or(0);
    monster_template.domain_range = result.Get<uint32_t>(row, 20).value_or(1000);

    // 追击参数（P0字段）
    monster_template.pursuit_forgive_time = result.Get<uint32_t>(row, 21).value_or(5000);
    monster_template.pursuit_forgive_distance = result.Get<uint32_t>(row, 22).value_or(1500);

    return monster_template;
}

// 辅助函数：从Result行转换为SpawnPoint
static Game::SpawnPoint RowToSpawnPoint(Result& result, int row) {
    Game::SpawnPoint sp;

    sp.spawn_id = result.Get<uint64_t>(row, 0).value_or(0);
    sp.map_id = result.Get<uint16_t>(row, 1).value_or(1);
    sp.monster_id = result.Get<uint32_t>(row, 2).value_or(0);
    sp.x = result.Get<float>(row, 3).value_or(0.0f);
    sp.y = result.Get<float>(row, 4).value_or(0.0f);
    sp.z = result.Get<float>(row, 5).value_or(0.0f);
    sp.max_count = result.Get<uint16_t>(row, 6).value_or(1);
    sp.respawn_time = result.Get<uint32_t>(row, 7).value_or(30000);

    return sp;
}

std::optional<Game::MonsterTemplate> MonsterDAO::LoadMonsterTemplate(uint32_t monster_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        std::stringstream query;
        query << "SELECT monster_id, monster_name, eng_name, level, "
              << "hp, shield, exp_point, "  // 数据库列名: exp_point
              << "attack_min, attack_max, critical_percent, physical_defense, "  // 数据库列名: attack_min/max
              << "walk_speed, run_speed, scale, COALESCE(chx_name, '') AS model_file, "
              << "attack_kind, attack_num, default_attack_index, "
              << "search_range, target_select, domain_range, "
              << "pursuit_forgive_time, pursuit_forgive_distance "
              << "FROM monster_templates WHERE monster_id = " << monster_id;

        auto result = executor.Query(query.str());
        if (!result || result->RowCount() == 0) {
            spdlog::warn("Monster template not found: {}", monster_id);
            ConnectionPool::Instance().ReturnConnection(conn);
            return std::nullopt;
        }

        auto monster_template = RowToMonsterTemplate(*result, 0);
        ConnectionPool::Instance().ReturnConnection(conn);

        spdlog::debug("Loaded monster template: {} ({})", monster_template.monster_name, monster_template.monster_id);
        return monster_template;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load monster template {}: {}", monster_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

std::unordered_map<uint32_t, Game::MonsterTemplate> MonsterDAO::LoadAllMonsterTemplates() {
    std::unordered_map<uint32_t, Game::MonsterTemplate> templates;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return templates;
    }

    QueryExecutor executor(conn);

    try {
        std::string query = "SELECT monster_id, monster_name, eng_name, level, "
                          "hp, shield, exp_reward, "  // 数据库列名: exp_reward
                          "attack_min, attack_max, critical_percent, physical_defense, "  // 数据库列名: attack_min/max
                          "walk_speed, run_speed, scale, COALESCE(chx_name, '') AS model_file, "
                          "attack_kind, attack_num, default_attack_index, "
                          "search_range, target_select, domain_range, "
                          "pursuit_forgive_time, pursuit_forgive_distance "
                          "FROM monster_templates ORDER BY monster_id";

        auto result = executor.Query(query);
        if (!result) {
            spdlog::error("Failed to load monster templates");
            ConnectionPool::Instance().ReturnConnection(conn);
            return templates;
        }

        for (int i = 0; i < result->RowCount(); ++i) {
            auto monster_template = RowToMonsterTemplate(*result, i);
            templates[monster_template.monster_id] = monster_template;
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Loaded {} monster templates from database", templates.size());
        return templates;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load monster templates: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return templates;
    }
}

std::vector<Game::SpawnPoint> MonsterDAO::LoadSpawnPoints(uint16_t map_id) {
    std::vector<Game::SpawnPoint> spawn_points;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return spawn_points;
    }

    QueryExecutor executor(conn);

    try {
        std::stringstream query;
        query << "SELECT spawn_id, map_id, monster_id, x, y, z, max_count, "
              << "respawn_time "
              << "FROM spawn_points WHERE map_id = " << map_id
              << " ORDER BY spawn_id";

        auto result = executor.Query(query.str());
        if (!result) {
            spdlog::error("Failed to load spawn points for map {}", map_id);
            ConnectionPool::Instance().ReturnConnection(conn);
            return spawn_points;
        }

        for (int i = 0; i < result->RowCount(); ++i) {
            auto sp = RowToSpawnPoint(*result, i);
            spawn_points.push_back(sp);
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::debug("Loaded {} spawn points for map {}", spawn_points.size(), map_id);
        return spawn_points;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load spawn points for map {}: {}", map_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return spawn_points;
    }
}

std::unordered_map<uint16_t, std::vector<Game::SpawnPoint>> MonsterDAO::LoadAllSpawnPoints() {
    std::unordered_map<uint16_t, std::vector<Game::SpawnPoint>> all_spawn_points;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return all_spawn_points;
    }

    QueryExecutor executor(conn);

    try {
        std::string query = "SELECT spawn_id, map_id, monster_id, x, y, z, max_count, "
                          "respawn_delay AS respawn_time "  // 数据库列名: respawn_delay
                          "FROM spawn_points ORDER BY map_id, spawn_id";

        auto result = executor.Query(query);
        if (!result) {
            spdlog::error("Failed to load spawn points");
            ConnectionPool::Instance().ReturnConnection(conn);
            return all_spawn_points;
        }

        for (int i = 0; i < result->RowCount(); ++i) {
            auto sp = RowToSpawnPoint(*result, i);
            all_spawn_points[sp.map_id].push_back(sp);
        }

        size_t total_points = 0;
        for (const auto& [map_id, points] : all_spawn_points) {
            total_points += points.size();
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Loaded {} spawn points for {} maps", total_points, all_spawn_points.size());

        return all_spawn_points;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load spawn points: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return all_spawn_points;
    }
}

bool MonsterDAO::MonsterExists(uint32_t monster_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::stringstream query;
        query << "SELECT COUNT(*) FROM monster_templates WHERE monster_id = " << monster_id;

        auto result = executor.Query(query.str());
        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return false;
        }

        auto count = result->Get<int64_t>(0, 0).value_or(0);
        ConnectionPool::Instance().ReturnConnection(conn);
        return count > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check monster existence: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

size_t MonsterDAO::GetMonsterTemplateCount() {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return 0;
    }

    QueryExecutor executor(conn);

    try {
        auto result = executor.Query("SELECT COUNT(*) FROM monster_templates");

        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return 0;
        }

        auto count = result->Get<int64_t>(0, 0).value_or(0);
        ConnectionPool::Instance().ReturnConnection(conn);
        return static_cast<size_t>(count);

    } catch (const std::exception& e) {
        spdlog::error("Failed to get monster template count: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return 0;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
