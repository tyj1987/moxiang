// MurimServer - Damage Calculator Implementation
// 伤害计算器实现

#include "DamageCalculator.hpp"
#include "../game/GameObject.hpp"
#include "../skill/SkillInfo.hpp"
#include "../utils/TimeUtils.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Murim {
namespace Game {

// ========== 基础伤害计算 ==========

DamageResult DamageCalculator::CalculateNormalAttack(
    const GameObject* attacker,
    const GameObject* target,
    const AttackParameters& atk_params,
    const DefenseParameters& def_params) {

    DamageResult result;
    result.damage_type = DamageType::kPhysical;

    // 检查是否命中
    if (!CheckHit(attacker, target, atk_params.hit_rate, def_params.dodge_rate)) {
        result.is_hit = false;
        spdlog::trace("[DamageCalc] Attack missed: attacker={}, target={}",
                    attacker ? attacker->GetObjectId() : 0,
                    target ? target->GetObjectId() : 0);
        return result;
    }

    // 检查是否闪避
    if (CheckDodge(def_params.dodge_rate)) {
        result.is_dodged = true;
        spdlog::trace("[DamageCalc] Attack dodged: attacker={}, target={}",
                    attacker ? attacker->GetObjectId() : 0,
                    target ? target->GetObjectId() : 0);
        return result;
    }

    // 计算基础伤害
    result.raw_damage = static_cast<uint32_t>(
        atk_params.attack_power * atk_params.skill_damage_rate
    );

    // 计算物理伤害
    uint32_t physical_damage = CalculatePhysicalDamage(
        atk_params.attack_power,
        def_params.armour,
        atk_params.armor_penetration,
        atk_params.damage_multiplier,
        atk_params.damage_bonus
    );

    // 检查是否格挡
    if (CheckBlock(def_params.block_rate)) {
        // 格挡处理：减少伤害（is_blocked字段已废弃）
        // 格挡减少伤害
        physical_damage = static_cast<uint32_t>(physical_damage * def_params.block_reduction);
        spdlog::trace("[DamageCalc] Attack blocked: damage reduced to {}", physical_damage);
    }

    // 检查是否暴击
    if (CheckCritical(attacker, target, atk_params.critical_rate)) {
        result.is_critical = true;
        // critical_multiplier字段已废弃，使用critical_damage存储暴击伤害
        physical_damage = CalculateCriticalDamage(physical_damage, atk_params.critical_damage);
    }

    // 对应legacy: AttackManager.cpp:105-112
    // PvP伤害调整：玩家对玩家时伤害减少
    if (attacker->GetObjectType() == ObjectType::Player &&
        target->GetObjectType() == ObjectType::Player) {
        // PvP = 25% 或 50% 伤害（根据_JAPAN_LOCAL_宏）
#ifdef _JAPAN_LOCAL_
        physical_damage *= 0.25f;
#else
        physical_damage *= 0.5f;
#endif
    }

    // 应用防御减伤
    result.damage = ApplyDefenseReduction(
        physical_damage,
        def_params.physical_reduction,
        def_params.damage_taken_multiplier
    );

    return result;
}

DamageResult DamageCalculator::CalculateSkillDamage(
    const GameObject* attacker,
    const GameObject* target,
    const SkillInfo* skill_info,
    uint16_t skill_level,
    const AttackParameters& atk_params,
    const DefenseParameters& def_params) {

    DamageResult result;

    // 从skill_info获取伤害类型
    if (skill_info) {
        SkillDamageType skill_damage_type = skill_info->GetDamageType();
        // 转换SkillDamageType到DamageType
        switch (skill_damage_type) {
            case SkillDamageType::Physical:
                result.damage_type = DamageType::kPhysical;
                break;
            case SkillDamageType::Magic:
                result.damage_type = DamageType::kMagic;
                break;
            case SkillDamageType::True:
                result.damage_type = DamageType::kTrue;
                break;
            case SkillDamageType::Hybrid:
                result.damage_type = DamageType::kPhysical;
                break;
            default:
                result.damage_type = DamageType::kPhysical;
                break;
        }
    } else {
        // 默认物理伤害
        result.damage_type = DamageType::kPhysical;
    }

    // 检查是否命中
    if (!CheckHit(attacker, target, atk_params.hit_rate, def_params.dodge_rate)) {
        result.is_hit = false;
        return result;
    }

    // 检查是否闪避
    if (CheckDodge(def_params.dodge_rate)) {
        result.is_dodged = true;
        return result;
    }

    // 计算基础伤害(攻击力 * 技能倍率 + 技能基础伤害)
    result.raw_damage = static_cast<uint32_t>(
        atk_params.attack_power * atk_params.skill_damage_rate + atk_params.skill_base_damage
    );

    uint32_t final_damage = result.raw_damage;

    // 根据伤害类型计算
    switch (result.damage_type) {
        case DamageType::kPhysical:
            final_damage = CalculatePhysicalDamage(
                atk_params.attack_power,
                def_params.armour,
                atk_params.armor_penetration,
                atk_params.damage_multiplier,
                atk_params.damage_bonus
            );
            break;

        case DamageType::kMagic:
            final_damage = CalculateMagicDamage(
                atk_params.magic_power,
                def_params.magic_resist,
                atk_params.magic_penetration,
                atk_params.damage_multiplier,
                atk_params.damage_bonus
            );
            break;

        case DamageType::kTrue:
            final_damage = CalculateTrueDamage(
                result.raw_damage,
                atk_params.damage_multiplier,
                atk_params.damage_bonus
            );
            break;

    }

    // 检查是否格挡
    if (CheckBlock(def_params.block_rate)) {
        // 格挡处理：减少伤害（is_blocked字段已废弃）
        final_damage = static_cast<uint32_t>(final_damage * def_params.block_reduction);
    }

    // 检查是否暴击
    if (CheckCritical(attacker, target, atk_params.critical_rate)) {
        result.is_critical = true;
        // critical_multiplier字段已废弃，使用critical_damage存储暴击伤害
        final_damage = CalculateCriticalDamage(final_damage, atk_params.critical_damage);
    }

    // 应用防御减伤
    result.damage = ApplyDefenseReduction(
        final_damage,
        result.damage_type == DamageType::kMagic ? def_params.magic_reduction : def_params.physical_reduction,
        def_params.damage_taken_multiplier
    );

    return result;
}

// ========== 伤害类型计算 ==========

uint32_t DamageCalculator::CalculatePhysicalDamage(
    uint32_t attack_power,
    uint32_t armour,
    float armour_penetration,
    float damage_multiplier,
    float damage_bonus) {

    // 计算有效护甲(考虑穿透)
    float effective_armour = armour * (1.0f - armour_penetration / 100.0f);

    // 计算护甲减伤
    float armour_reduction = CalculateArmorReduction(static_cast<uint32_t>(effective_armour), 1);

    // 基础伤害
    float damage = static_cast<float>(attack_power);

    // 应用护甲减伤
    damage = damage * (1.0f - armour_reduction);

    // 应用伤害倍率和加成
    damage = damage * damage_multiplier + damage_bonus;

    // 确保伤害至少为1
    return std::max(1, static_cast<int32_t>(damage));
}

uint32_t DamageCalculator::CalculateMagicDamage(
    uint32_t magic_power,
    uint32_t magic_resist,
    float magic_penetration,
    float damage_multiplier,
    float damage_bonus) {

    // 计算有效魔抗(考虑穿透)
    float effective_resist = magic_resist * (1.0f - magic_penetration / 100.0f);

    // 计算魔抗减伤
    float resist_reduction = CalculateMagicResistReduction(static_cast<uint32_t>(effective_resist), 1);

    // 基础伤害
    float damage = static_cast<float>(magic_power);

    // 应用魔抗减伤
    damage = damage * (1.0f - resist_reduction);

    // 应用伤害倍率和加成
    damage = damage * damage_multiplier + damage_bonus;

    // 确保伤害至少为1
    return std::max(1, static_cast<int32_t>(damage));
}

uint32_t DamageCalculator::CalculateTrueDamage(
    uint32_t base_damage,
    float damage_multiplier,
    float damage_bonus) {

    float damage = static_cast<float>(base_damage);
    damage = damage * damage_multiplier + damage_bonus;

    return std::max(1, static_cast<int32_t>(damage));
}

// ========== 命中与闪避 ==========

bool DamageCalculator::CheckHit(
    const GameObject* attacker,
    const GameObject* target,
    float hit_rate,
    float dodge_rate) {

    // 从GameObject获取实际命中率
    float actual_hit_rate = hit_rate;
    if (attacker) {
        actual_hit_rate = attacker->GetHitRate();
    }

    // 从GameObject获取实际闪避率
    float actual_dodge_rate = dodge_rate;
    if (target) {
        actual_dodge_rate = target->GetDodgeRate();
    }

    // 计算实际命中率(基础命中率 - 目标闪避率)
    float final_hit_rate = actual_hit_rate - actual_dodge_rate;
    final_hit_rate = std::max(5.0f, std::min(95.0f, final_hit_rate)); // 限制在5%-95%

    // 随机判定
    float random_value = (std::rand() % 10000) / 100.0f; // 0-100
    return random_value <= final_hit_rate;
}

bool DamageCalculator::CheckCritical(
    const GameObject* attacker,
    const GameObject* target,
    float critical_rate) {

    if (!attacker || !target) {
        spdlog::warn("[DamageCalc] CheckCritical: null attacker or target");
        return false;
    }

    // 对应legacy: AttackCalc.cpp:79-105
    // 获取等级
    uint32_t attacker_level = attacker->GetLevel();
    uint32_t target_level = target->GetLevel();

    // 获取攻击者的暴击属性
    float attacker_critical = attacker->GetCriticalRate();

    // 基础暴击率 = (攻击者暴击 + 20) / (目标等级 * 5 + 100)
    float base_critical = (attacker_critical + 20.0f) /
                          static_cast<float>(target_level * 5 + 100);

    // 上限15%
    if (base_critical > 0.15f) {
        base_critical = 0.15f;
    }

    // 根据等级差异调整
    float final_critical;

    if (attacker_level < target_level) {
        // 等级低：每级差距 -2% 暴击率
        final_critical = base_critical + critical_rate -
                          static_cast<float>(target_level - attacker_level) * 0.02f;
        if (final_critical < 0.0f) {
            final_critical = 0.0f;
        }
    } else {
        // 等级高：每级差距 +0.4% 暴击率
        final_critical = base_critical + critical_rate +
                          static_cast<float>(attacker_level - target_level) * 0.004f;
    }

    // 随机判定 (0-100范围)
    float random_value = (std::rand() % 10000) / 100.0f;
    float critical_percent = final_critical * 100.0f;

    bool is_critical = (random_value <= critical_percent);

    // if (is_critical) {
    //     spdlog::debug("[DamageCalc] Critical! attacker_lv={} target_lv={} final_cr={}%",
    //                  attacker_level, target_level, final_critical);
    // }

    return is_critical;
}

bool DamageCalculator::CheckBlock(float block_rate) {
    // 限制格挡率在0%-100%
    block_rate = std::max(0.0f, std::min(100.0f, block_rate));

    // 随机判定
    float random_value = (std::rand() % 10000) / 100.0f;
    return random_value <= block_rate;
}

bool DamageCalculator::CheckDodge(float dodge_rate) {
    // 限制闪避率在0%-100%
    dodge_rate = std::max(0.0f, std::min(100.0f, dodge_rate));

    // 随机判定
    float random_value = (std::rand() % 10000) / 100.0f;
    return random_value <= dodge_rate;
}

// ========== 暴击计算 ==========

uint32_t DamageCalculator::CalculateCriticalDamage(
    uint32_t base_damage,
    float critical_multiplier) {

    float damage = static_cast<float>(base_damage) * critical_multiplier;
    return static_cast<uint32_t>(damage);
}

// ========== 防御减伤计算 ==========

float DamageCalculator::CalculateArmorReduction(uint32_t armour, uint32_t attacker_level) {
    // 护甲减伤公式: armour / (armour + 50 * attacker_level)
    // 这是一个常见的MMORPG护甲减伤公式

    if (armour == 0) return 0.0f;

    float reduction = static_cast<float>(armour) /
                     static_cast<float>(armour + 50 * attacker_level);

    // 限制减伤在0%-95%
    return std::max(0.0f, std::min(0.95f, reduction));
}

float DamageCalculator::CalculateMagicResistReduction(uint32_t magic_resist, uint32_t attacker_level) {
    // 魔抗减伤公式: magic_resist / (magic_resist + 50 * attacker_level)
    // 与护甲减伤公式类似

    if (magic_resist == 0) return 0.0f;

    float reduction = static_cast<float>(magic_resist) /
                     static_cast<float>(magic_resist + 50 * attacker_level);

    // 限制减伤在0%-95%
    return std::max(0.0f, std::min(0.95f, reduction));
}

// ========== 最终伤害计算 ==========

uint32_t DamageCalculator::ApplyDefenseReduction(
    uint32_t damage,
    float defense_reduction,
    float damage_taken_multiplier) {

    float final_damage = static_cast<float>(damage);

    // 应用防御减伤
    final_damage = final_damage * (1.0f - defense_reduction);

    // 应用承伤倍率
    final_damage = final_damage * damage_taken_multiplier;

    // 确保伤害至少为1
    return std::max(1, static_cast<int32_t>(final_damage));
}

// ========== DoT伤害计算 ==========

uint32_t DamageCalculator::CalculateDotDamage(
    uint32_t base_damage,
    uint32_t tick_count,
    float tick_rate) {

    float total_damage = static_cast<float>(base_damage) * tick_rate * tick_count;
    return static_cast<uint32_t>(total_damage);
}

// ========== 反射伤害计算 ==========

uint32_t DamageCalculator::CalculateReflectedDamage(
    uint32_t incoming_damage,
    float reflect_rate) {

    float reflected = static_cast<float>(incoming_damage) * reflect_rate;
    return static_cast<uint32_t>(reflected);
}

// ========== 伤害修正 ==========

uint32_t DamageCalculator::ApplyDamageMultiplier(
    uint32_t damage,
    float multiplier) {

    float result = static_cast<float>(damage) * multiplier;
    return std::max(1, static_cast<int32_t>(result));
}

uint32_t DamageCalculator::ApplyDamageBonus(
    uint32_t damage,
    float bonus) {

    float result = static_cast<float>(damage) + bonus;
    return std::max(1, static_cast<int32_t>(result));
}

// ========== 辅助函数 ==========

AttackParameters DamageCalculator::GetAttackParameters(const GameObject* attacker) {
    AttackParameters params;

    if (!attacker) return params;

    // 从GameObject获取实际攻击属性
    params.attack_power = attacker->GetAttackPower();
    params.magic_power = attacker->GetMagicPower();
    params.critical_rate = attacker->GetCriticalRate();
    params.critical_damage = attacker->GetCriticalDamage();
    params.hit_rate = attacker->GetHitRate();
    params.armor_penetration = attacker->GetArmorPenetration();
    params.magic_penetration = attacker->GetMagicPenetration();

    spdlog::trace("[DamageCalc] Getting attack params for object {}: atk={}, mag_atk={}, crit={}, hit={}",
                 attacker->GetObjectId(), params.attack_power, params.magic_power,
                 params.critical_rate, params.hit_rate);

    return params;
}

DefenseParameters DamageCalculator::GetDefenseParameters(const GameObject* target) {
    DefenseParameters params;

    if (!target) return params;

    // 从GameObject获取实际防御属性
    params.armour = target->GetArmour();
    params.magic_resist = target->GetMagicResist();
    params.dodge_rate = target->GetDodgeRate();
    params.block_rate = target->GetBlockRate();
    params.block_reduction = target->GetBlockReduction();
    params.damage_taken_multiplier = target->GetDamageTakenMultiplier();
    params.physical_reduction = target->GetPhysicalReduction();
    params.magic_reduction = target->GetMagicReduction();

    spdlog::trace("[DamageCalc] Getting defense params for object {}: armour={}, magic_resist={}, dodge={}, block={}",
                 target->GetObjectId(), params.armour, params.magic_resist,
                 params.dodge_rate, params.block_rate);

    return params;
}

float DamageCalculator::CalculateLevelModifier(int attacker_level, int defender_level) {
    // 计算等级差修正
    // 等级差 = 攻击者等级 - 防御者等级
    // 每级差异影响1%伤害

    int level_diff = attacker_level - defender_level;
    return 1.0f + (level_diff * 0.01f);
}

// ========== Helper Functions ==========

uint32_t QuickCalculateDamage(
    uint32_t attack_power,
    uint32_t defence,
    float skill_damage_rate) {

    // 简化的伤害计算公式
    float damage = attack_power * skill_damage_rate;

    // 简单的防御减伤
    float defence_reduction = static_cast<float>(defence) / static_cast<float>(defence + 100);
    damage = damage * (1.0f - defence_reduction);

    return std::max(1, static_cast<int32_t>(damage));
}

uint32_t QuickCalculateCriticalDamage(
    uint32_t base_damage,
    float critical_rate,
    float critical_multiplier) {

    // 检查是否暴击
    float random_value = (std::rand() % 10000) / 100.0f;

    if (random_value <= critical_rate) {
        // 暴击
        return static_cast<uint32_t>(base_damage * critical_multiplier);
    } else {
        return base_damage;
    }
}

bool IsValidDamageResult(const DamageResult& result) {
    // 有效的伤害结果: 伤害大于0 且 未被闪避/未命中
    return result.damage > 0 && !result.is_dodged && !result.is_hit;
}

} // namespace Game
} // namespace Murim
