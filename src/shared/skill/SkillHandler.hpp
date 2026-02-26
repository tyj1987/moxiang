#pragma once

#include "battle/DamageCalculator.hpp"
#include "battle/specialStateManager.hpp"
#include "game/GameObject.hpp"
#include "character/CharacterManager.hpp"
#include "item/ItemManager.hpp"
#include "skill/SkillManager.hpp"
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace Murim {
namespace Shared {
namespace Battle {

/**
 * @brief 技能释放请求
 */
struct SkillCastRequest {
    uint64_t character_id;      // 释放者角色ID
    uint32_t skill_id;         // 技能ID
    uint64_t target_id;        // 目标ID（角色或怪物）
    uint8_t target_type;       // 目标类型 (0=角色, 1=怪物, 2=NPC, 3=物品)
    float target_x;            // 目标位置X
    float target_y;            // 目标位置Y
    float target_z;            // 目标位置Z
};

/**
 * @brief 技能释放响应
 */
struct SkillCastResponse {
    uint32_t result_code;       // 结果码 (0=成功, 1=冷却中, 2=MP不足, 3=技能不存在, 4=目标无效, 5=超出范围, 6=目标死亡)
    uint32_t skill_id;         // 技能ID
    uint32_t damage;           // 造成伤害（如果有）
    uint32_t remaining_mp;     // 剩余MP
};

/**
 * @brief 技能处理器
 *
 * 负责处理技能释放请求：
 * - 验证技能是否可用（冷却、MP、距离等）
 * - 计算伤害（使用 DamageCalculator）
 * - 应用效果（使用 SpecialStateManager）
 * - 广播给周围玩家
 *
 * 对应 legacy: 技能释放处理（多个文件散布在 ServerSystem.cpp）
 */
class SkillHandler {
public:
    /**
     * @brief 构造函数
     */
    SkillHandler(
        std::shared_ptr<Game::Character::Manager> char_manager,
        std::shared_ptr<Game::Skill::Manager> skill_manager,
        std::shared_ptr<Game::Item::Manager> item_manager,
        std::shared_ptr<DamageCalculator> damage_calculator,
        std::shared_ptr<SpecialStateManager> buff_manager);

    /**
     * @brief 处理技能释放请求
     */
    SkillCastResponse HandleSkillCast(const SkillCastRequest& request);

    /**
     * @brief 验证技能是否可用
     */
    bool ValidateSkill(const SkillCastRequest& request, Game::GameObject* caster);

    /**
     * @brief 计算技能伤害
     */
    uint32_t CalculateDamage(const SkillCastRequest& request, Game::GameObject* caster);

    /**
     * @brief 应用技能效果
     */
    void ApplySkillEffect(const SkillCastRequest& request, Game::GameObject* caster, uint32_t damage);

    /**
     * @brief 广播技能释放给周围玩家
     */
    void BroadcastSkillCast(const SkillCastRequest& request, const SkillCastResponse& response);

private:
    std::shared_ptr<Game::Character::Manager> character_manager_;
    std::shared_ptr<Game::Skill::Manager> skill_manager_;
    std::shared_ptr<Game::Item::Manager> item_manager_;
    std::shared_ptr<DamageCalculator> damage_calculator_;
    std::shared_ptr<SpecialStateManager> buff_manager_;
};

} // namespace Battle
} // namespace Shared
} // namespace Murim
