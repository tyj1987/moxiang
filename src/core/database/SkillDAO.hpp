#pragma once

#include "shared/skill/Skill.hpp"
#include <vector>
#include <optional>
#include <unordered_map>

namespace Murim {
namespace Core {
namespace Database {

/**
 * @brief 技能数据访问对象
 *
 * 负责技能数据的数据库操作
 * 对应 legacy: [CC]Skill/skillmanager_server.h
 */
class SkillDAO {
public:
    // ========== 技能模板操作 ==========

    /**
     * @brief 加载技能模板
     * @param skill_id 技能ID
     * @return 技能数据
     */
    static std::optional<Game::Skill> Load(uint32_t skill_id);

    /**
     * @brief 加载所有技能模板
     * @return 所有技能数据
     */
    static std::vector<Game::Skill> LoadAll();

    /**
     * @brief 检查技能是否存在
     * @param skill_id 技能ID
     * @return 是否存在
     */
    static bool Exists(uint32_t skill_id);

    // ========== 角色技能操作 ==========

    /**
     * @brief 加载角色的已学技能
     * @param character_id 角色ID
     * @return 技能ID列表
     */
    static std::vector<uint32_t> LoadCharacterSkills(uint64_t character_id);

    /**
     * @brief 添加角色技能
     * @param character_id 角色ID
     * @param skill_id 技能ID
     * @param skill_level 技能等级
     * @return 是否成功
     */
    static bool AddCharacterSkill(
        uint64_t character_id,
        uint32_t skill_id,
        uint16_t skill_level = 1
    );

    /**
     * @brief 移除角色技能
     * @param character_id 角色ID
     * @param skill_id 技能ID
     * @return 是否成功
     */
    static bool RemoveCharacterSkill(
        uint64_t character_id,
        uint32_t skill_id
    );

    /**
     * @brief 检查角色是否已学会技能
     * @param character_id 角色ID
     * @param skill_id 技能ID
     * @return 是否已学会
     */
    static bool HasCharacterSkill(
        uint64_t character_id,
        uint32_t skill_id
    );

    /**
     * @brief 更新技能等级
     * @param character_id 角色ID
     * @param skill_id 技能ID
     * @param skill_level 技能等级
     * @return 是否成功
     */
    static bool UpdateSkillLevel(
        uint64_t character_id,
        uint32_t skill_id,
        uint16_t skill_level
    );

    /**
     * @brief 获取技能等级
     * @param character_id 角色ID
     * @param skill_id 技能ID
     * @return 技能等级, 未学会返回 0
     */
    static uint16_t GetSkillLevel(
        uint64_t character_id,
        uint32_t skill_id
    );
};

} // namespace Database
} // namespace Core
} // namespace Murim
