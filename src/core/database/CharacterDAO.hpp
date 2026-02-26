#pragma once

#include <vector>
#include <optional>
#include "shared/character/Character.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"

namespace Murim {
namespace Core {
namespace Database {

/**
 * @brief 角色数据访问对象 (DAO)
 *
 * 负责角色的数据库操作
 */
class CharacterDAO {
public:
    /**
     * @brief 创建角色
     * @param character 角色数据
     * @return 新创建的角色 ID
     */
    static     uint64_t Create(const Game::Character& character);

    /**
     * @brief 加载角色
     * @param character_id 角色 ID
     * @return 角色数据
     */
    static     std::optional<Game::Character> Load(uint64_t character_id);

    /**
     * @brief 加载角色属性
     * @param character_id 角色 ID
     * @return 角色属性
     */
    static     std::optional<Game::CharacterStats> LoadStats(uint64_t character_id);

    /**
     * @brief 保存角色
     * @param character 角色数据
     * @return 是否成功
     */
    static     bool Save(const Game::Character& character);

    /**
     * @brief 保存角色属性
     * @param character_id 角色 ID
     * @param stats 角色属性
     * @return 是否成功
     */
    static     bool SaveStats(uint64_t character_id, const Game::CharacterStats& stats);

    /**
     * @brief 删除角色（软删除）
     * @param character_id 角色 ID
     * @return 是否成功
     */
    static     bool Delete(uint64_t character_id);

    /**
     * @brief 查询账号下的所有角色
     * @param account_id 账号 ID
     * @return 角色列表
     */
    static     std::vector<Game::Character> LoadByAccount(uint64_t account_id);

    /**
     * @brief 更新角色位置
     * @param character_id 角色 ID
     * @param x, y, z 位置坐标
     * @param direction 朝向
     * @return 是否成功
     */
    static     bool UpdatePosition(uint64_t character_id, float x, float y, float z,
                        uint16_t direction);

    /**
     * @brief 更新登录时间
     * @param character_id 角色 ID
     * @return 是否成功
     */
    static     bool UpdateLastLoginTime(uint64_t character_id);

    /**
     * @brief 更新总游戏时间
     * @param character_id 角色 ID
     * @param play_time 游戏时间（秒）
     * @return 是否成功
     */
    static     bool UpdatePlayTime(uint64_t character_id, uint32_t play_time);

    /**
     * @brief 检查角色名是否存在
     * @param name 角色名
     * @return 是否存在
     */
    static     bool Exists(const std::string& name);

    /**
     * @brief 检查角色是否属于指定账号
     * @param character_id 角色 ID
     * @param account_id 账号 ID
     * @return 是否属于
     */
    static     bool BelongsToAccount(uint64_t character_id, uint64_t account_id);

private:
    /**
     * @brief 从数据库行构建角色对象
     */
    static std::optional<Game::Character> RowToCharacter(Result& result, int row);

    /**
     * @brief 从数据库行构建角色属性对象
     */
    static std::optional<Game::CharacterStats> RowToStats(Result& result, int row);
};

} // namespace Database
} // namespace Core
} // namespace Murim
