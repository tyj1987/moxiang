// MurimServer - Damage Calculator
// 伤害计算器 - 计算技能和普通攻击的伤害
// 对应legacy: 伤害计算相关函数

#pragma once

#include "Battle.hpp"  // For DamageType, DamageResult, BattleStats

#include <cstdint>

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class GameObject;
class SkillInfo;

// ========== Attack Parameters ==========
// 攻击参数
struct AttackParameters {
    // 攻击者属性
    uint32_t attack_power;        // 攻击力
    uint32_t magic_power;         // 魔法攻击力
    float critical_rate;          // 暴击率(0-100)
    float critical_damage;        // 暴击伤害倍率
    float hit_rate;               // 命中率(0-100)
    float armor_penetration;      // 护甲穿透(0-100)
    float magic_penetration;      // 魔法穿透(0-100)

    // 技能相关
    uint32_t skill_id;            // 技能ID(0表示普通攻击)
    uint16_t skill_level;         // 技能等级
    float skill_damage_rate;      // 技能伤害加成比例
    float skill_base_damage;      // 技能基础伤害

    // 额外修正
    float damage_multiplier;      // 伤害倍率
    float damage_bonus;           // 额外伤害加成

    AttackParameters()
        : attack_power(100),
          magic_power(100),
          critical_rate(5.0f),
          critical_damage(1.5f),
          hit_rate(95.0f),
          armor_penetration(0.0f),
          magic_penetration(0.0f),
          skill_id(0),
          skill_level(1),
          skill_damage_rate(1.0f),
          skill_base_damage(0.0f),
          damage_multiplier(1.0f),
          damage_bonus(0.0f) {}
};

// ========== Defense Parameters ==========
// 防御参数
struct DefenseParameters {
    // 防御者属性
    uint32_t armour;              // 护甲(物理防御)
    uint32_t magic_resist;        // 魔法抗性
    float dodge_rate;             // 闪避率(0-100)
    float block_rate;             // 格挡率(0-100)
    float block_reduction;        // 格挡减伤比例

    // 状态影响
    float damage_taken_multiplier; // 承受伤害倍率
    float physical_reduction;     // 物理伤害减免
    float magic_reduction;        // 魔法伤害减免

    DefenseParameters()
        : armour(0),
          magic_resist(0),
          dodge_rate(5.0f),
          block_rate(0.0f),
          block_reduction(0.5f),
          damage_taken_multiplier(1.0f),
          physical_reduction(0.0f),
          magic_reduction(0.0f) {}
};

// ========== Damage Calculator ==========
// 伤害计算器
class DamageCalculator {
public:
    // ========== 基础伤害计算 ==========

    // 计算普通攻击伤害
    static DamageResult CalculateNormalAttack(
        const GameObject* attacker,
        const GameObject* target,
        const AttackParameters& atk_params,
        const DefenseParameters& def_params
    );

    // 计算技能伤害
    static DamageResult CalculateSkillDamage(
        const GameObject* attacker,
        const GameObject* target,
        const SkillInfo* skill_info,
        uint16_t skill_level,
        const AttackParameters& atk_params,
        const DefenseParameters& def_params
    );

    // ========== 伤害类型计算 ==========

    // 计算物理伤害
    static uint32_t CalculatePhysicalDamage(
        uint32_t attack_power,
        uint32_t armour,
        float armour_penetration,
        float damage_multiplier,
        float damage_bonus
    );

    // 计算魔法伤害
    static uint32_t CalculateMagicDamage(
        uint32_t magic_power,
        uint32_t magic_resist,
        float magic_penetration,
        float damage_multiplier,
        float damage_bonus
    );

    // 计算真实伤害(无视防御)
    static uint32_t CalculateTrueDamage(
        uint32_t base_damage,
        float damage_multiplier,
        float damage_bonus
    );

    // ========== 命中与闪避 ==========

    // 检查是否命中
    static bool CheckHit(
        const GameObject* attacker,
        const GameObject* target,
        float hit_rate,
        float dodge_rate
    );

    // 检查是否暴击（对用legacy: AttackCalc.cpp:79-105）
    static bool CheckCritical(
        const GameObject* attacker,
        const GameObject* target,
        float critical_rate
    );

    // 检查是否格挡
    static bool CheckBlock(float block_rate);

    // 检查是否闪避
    static bool CheckDodge(float dodge_rate);

    // ========== 暴击计算 ==========

    // 计算暴击伤害
    static uint32_t CalculateCriticalDamage(
        uint32_t base_damage,
        float critical_multiplier
    );

    // ========== 防御减伤计算 ==========

    // 计算护甲减伤比例
    static float CalculateArmorReduction(uint32_t armour, uint32_t attacker_level);

    // 计算魔法抗性减伤比例
    static float CalculateMagicResistReduction(uint32_t magic_resist, uint32_t attacker_level);

    // ========== 最终伤害计算 ==========

    // 应用防御减伤
    static uint32_t ApplyDefenseReduction(
        uint32_t damage,
        float defense_reduction,
        float damage_taken_multiplier
    );

    // ========== DoT伤害计算 ==========

    // 计算DoT伤害(持续伤害)
    static uint32_t CalculateDotDamage(
        uint32_t base_damage,
        uint32_t tick_count,
        float tick_rate
    );

    // ========== 反射伤害计算 ==========

    // 计算反射伤害
    static uint32_t CalculateReflectedDamage(
        uint32_t incoming_damage,
        float reflect_rate
    );

    // ========== 伤害修正 ==========

    // 应用伤害倍率
    static uint32_t ApplyDamageMultiplier(
        uint32_t damage,
        float multiplier
    );

    // 应用伤害加成
    static uint32_t ApplyDamageBonus(
        uint32_t damage,
        float bonus
    );

    // ========== 辅助函数 ==========

    // 获取攻击参数(从GameObject)
    static AttackParameters GetAttackParameters(const GameObject* attacker);

    // 获取防御参数(从GameObject)
    static DefenseParameters GetDefenseParameters(const GameObject* target);

    // 计算等级差修正
    static float CalculateLevelModifier(int attacker_level, int defender_level);
};

// ========== Helper Functions ==========

// 快速计算伤害(简化版本)
uint32_t QuickCalculateDamage(
    uint32_t attack_power,
    uint32_t defence,
    float skill_damage_rate = 1.0f
);

// 快速计算暴击伤害
uint32_t QuickCalculateCriticalDamage(
    uint32_t base_damage,
    float critical_rate = 5.0f,
    float critical_multiplier = 1.5f
);

// 检查伤害是否有效
bool IsValidDamageResult(const DamageResult& result);

} // namespace Game
} // namespace Murim
