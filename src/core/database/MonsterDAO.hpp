#pragma once

#include <vector>
#include <optional>
#include <unordered_map>
#include "shared/character/MonsterTemplate.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"

namespace Murim {
namespace Core {
namespace Database {

/**
 * @brief 怪物数据访问对象 (DAO)
 *
 * 负责怪物模板和刷怪点的数据库操作
 */
class MonsterDAO {
public:
    /**
     * @brief 加载单个怪物模板
     * @param monster_id 怪物ID
     * @return 怪物模板（如果存在）
     */
    static std::optional<Game::MonsterTemplate> LoadMonsterTemplate(uint32_t monster_id);

    /**
     * @brief 加载所有怪物模板
     * @return 怪物模板映射表（monster_id -> MonsterTemplate）
     */
    static std::unordered_map<uint32_t, Game::MonsterTemplate> LoadAllMonsterTemplates();

    /**
     * @brief 加载指定地图的所有刷怪点
     * @param map_id 地图ID
     * @return 刷怪点列表
     */
    static std::vector<Game::SpawnPoint> LoadSpawnPoints(uint16_t map_id);

    /**
     * @brief 加载所有刷怪点
     * @return 刷怪点映射表（map_id -> SpawnPoint列表）
     */
    static std::unordered_map<uint16_t, std::vector<Game::SpawnPoint>> LoadAllSpawnPoints();

    /**
     * @brief 验证怪物模板是否存在
     * @param monster_id 怪物ID
     * @return 是否存在
     */
    static bool MonsterExists(uint32_t monster_id);

    /**
     * @brief 获取怪物模板数量
     * @return 怪物模板总数
     */
    static size_t GetMonsterTemplateCount();
};

} // namespace Database
} // namespace Core
} // namespace Murim
