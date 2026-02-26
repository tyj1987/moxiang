#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <unordered_map>
#include <cmath>
#include "shared/battle/Battle.hpp"
#include "shared/character/Character.hpp"
#include "shared/movement/Movement.hpp"  // Use Position from Movement

namespace Murim {
namespace Game {

// Position is now defined in Movement.hpp, not here

/**
 * @brief 技能类型
 */
enum class SkillType : uint8_t {
    kPassive = 0,        // 被动技能
    kActive = 1,         // 主动技能
    kToggle = 2,         // 开关技能
    kChannel = 3         // 引导技能
};

/**
 * @brief 技能目标类型
 */
enum class SkillTargetType : uint8_t {
    kSelf = 0,           // 自身
    kSingle = 1,         // 单体目标
    kArea = 2,           // 范围内所有敌人
    kAreaCircle = 3,     // 圆形范围
    kAreaSector = 4,     // 扇形范围
    kAreaLine = 5        // 直线范围
};

/**
 * @brief 技能范围形状
 */
enum class SkillAreaShape : uint8_t {
    kNone = 0,           // 无范围 (单体)
    kCircle = 1,         // 圆形
    kSector = 2,         // 扇形
    kRectangle = 3,      // 矩形
    kLine = 4             // 直线
};

/**
 * @brief 技能状态效果
 */
struct SkillEffect {
    uint32_t effect_id;             // 效果ID
    uint32_t value;                  // 效果值
    uint32_t duration;               // 持续时间(秒)
    uint32_t tick_interval;          // 间隔时间(秒)

    /**
     * @brief 是否是Buff
     */
    bool IsBuff() const {
        return value > 0;
    }
};

/**
 * @brief 技能数据
 *
 * 对应 legacy: [CC]Skill/skillmanager_server.h
 */
struct Skill {
    uint32_t skill_id;               // 技能ID
    std::string name;                 // 技能名称
    std::string description;          // 描述

    // 基本信息
    SkillType skill_type;            // 技能类型
    SkillTargetType target_type;      // 目标类型
    SkillAreaShape area_shape;       // 范围形状

    // 伤害信息
    DamageType damage_type;          // 伤害类型
    uint32_t base_damage;             // 基础伤害
    float damage_multiplier;         // 伤害倍数
    AttributeType attribute;          // 元素属性

    // 范围信息
    float range;                     // 施法距离
    float radius;                    // 范围半径
    float angle;                      // 扇形角度(度)

    // 消耗
    uint32_t mp_cost;                 // 内力消耗
    uint32_t stamina_cost;            // 体力消耗

    // 冷却
    uint16_t cooldown;                 // 冷却时间(秒)
    uint16_t global_cooldown;         // 全局冷却(秒)

    // 学习要求
    uint16_t required_level;          // 需求等级
    std::vector<uint32_t> required_skills;  // 前置技能

    // 施法条件
    bool requires_weapon;             // 是否需要武器

    // 效果
    std::vector<SkillEffect> effects; // 技能效果

    // 构造函数 - 提供默认值
    Skill()
        : skill_id(0)
        , name("")
        , description("")
        , skill_type(SkillType::kActive)
        , target_type(SkillTargetType::kSingle)
        , area_shape(SkillAreaShape::kNone)
        , damage_type(DamageType::kPhysical)
        , base_damage(0)
        , damage_multiplier(1.0f)
        , attribute(AttributeType::kNone)
        , range(0.0f)
        , radius(0.0f)
        , angle(0.0f)
        , mp_cost(0)
        , stamina_cost(0)
        , cooldown(0)
        , global_cooldown(0)
        , required_level(1)
        , required_skills()
        , requires_weapon(false)  // 默认不需要武器
        , effects() {
    }

    /**
     * @brief 是否需要武器
     */
    bool RequiresWeapon() const {
        return requires_weapon;
    }

    /**
     * @brief 是否可以施放
     */
    bool CanCast(uint16_t player_level, uint32_t current_mp) const {
        return player_level >= required_level && current_mp >= mp_cost;
    }

    /**
     * @brief 计算实际伤害
     */
    uint32_t CalculateDamage(const BattleStats& attacker_stats) const {
        return static_cast<uint32_t>(
            (base_damage + attacker_stats.physical_attack * damage_multiplier)
        );
    }
};

} // namespace Game
} // namespace Murim
