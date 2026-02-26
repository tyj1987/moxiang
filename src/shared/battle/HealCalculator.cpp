// MurimServer - Heal Calculator Implementation
// 治疗计算器实现

#include "HealCalculator.hpp"
#include "../game/GameObject.hpp"
#include "../skill/SkillInfo.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Murim {
namespace Game {

// ========== 基础治疗计算 ==========

HealResult HealCalculator::CalculateSkillHeal(
    const GameObject* caster,
    const GameObject* target,
    const SkillInfo* skill_info,
    uint16_t skill_level,
    const HealParameters& heal_params) {

    HealResult result;
    result.heal_type = HealType::HP;

    // 计算基础治疗
    result.base_heal = CalculateHPHeal(
        heal_params.heal_power,
        heal_params.skill_heal_rate,
        heal_params.heal_multiplier,
        heal_params.heal_bonus
    );

    // 检查是否暴击
    if (CheckHealCritical(heal_params.critical_rate)) {
        result.is_critical = true;
        result.critical_multiplier = heal_params.critical_heal_bonus;
        result.heal_amount = CalculateCriticalHeal(result.base_heal, heal_params.critical_heal_bonus);
        spdlog::trace("[HealCalc] Critical heal! base={}, critical={}",
                    result.base_heal, result.heal_amount);
    } else {
        result.heal_amount = result.base_heal;
    }

    // 计算实际治疗量(考虑溢出)
    result.actual_heal = CalculateActualHeal(
        result.heal_amount,
        heal_params.current_hp,
        heal_params.max_hp
    );

    // 检查是否溢出
    result.over_heal = IsOverHeal(
        result.heal_amount,
        heal_params.current_hp,
        heal_params.max_hp
    );

    spdlog::debug("[HealCalc] Skill Heal: skill={}, level={}, heal={}, actual={}, over_heal={}",
                heal_params.skill_id, skill_level, result.heal_amount, result.actual_heal, result.over_heal);

    return result;
}

HealResult HealCalculator::CalculateItemHeal(
    const GameObject* user,
    const GameObject* target,
    uint32_t item_id,
    const HealParameters& heal_params) {

    HealResult result;
    result.heal_type = HealType::HP;

    // 计算基础治疗
    result.base_heal = CalculateHPHeal(
        heal_params.heal_power,
        1.0f,  // 物品通常没有技能倍率
        heal_params.heal_multiplier,
        heal_params.heal_bonus
    );

    result.heal_amount = result.base_heal;

    // 计算实际治疗量
    result.actual_heal = CalculateActualHeal(
        result.heal_amount,
        heal_params.current_hp,
        heal_params.max_hp
    );

    result.over_heal = IsOverHeal(
        result.heal_amount,
        heal_params.current_hp,
        heal_params.max_hp
    );

    spdlog::debug("[HealCalc] Item Heal: item={}, heal={}, actual={}",
                item_id, result.heal_amount, result.actual_heal);

    return result;
}

// ========== HP治疗计算 ==========

uint32_t HealCalculator::CalculateHPHeal(
    uint32_t heal_power,
    float skill_heal_rate,
    float heal_multiplier,
    float heal_bonus) {

    float heal = static_cast<float>(heal_power) * skill_heal_rate;
    heal = heal * heal_multiplier + heal_bonus;

    // 确保治疗至少为1
    return std::max(1, static_cast<int32_t>(heal));
}

// ========== MP恢复计算 ==========

uint32_t HealCalculator::CalculateMPHeal(
    uint32_t heal_power,
    float skill_heal_rate,
    float heal_multiplier,
    float heal_bonus) {

    float heal = static_cast<float>(heal_power) * skill_heal_rate;
    heal = heal * heal_multiplier + heal_bonus;

    // 确保恢复至少为1
    return std::max(1, static_cast<int32_t>(heal));
}

// ========== HoT治疗计算 ==========

uint32_t HealCalculator::CalculateHotTotalHeal(
    uint32_t base_heal,
    uint32_t tick_count,
    float tick_rate) {

    float total_heal = static_cast<float>(base_heal) * tick_rate * tick_count;
    return static_cast<uint32_t>(total_heal);
}

uint32_t HealCalculator::CalculateHotTickHeal(
    uint32_t total_heal,
    uint32_t tick_count) {

    if (tick_count == 0) return 0;
    return total_heal / tick_count;
}

// ========== 暴击治疗计算 ==========

bool HealCalculator::CheckHealCritical(float critical_rate) {
    // 限制暴击率在0%-100%
    critical_rate = std::max(0.0f, std::min(100.0f, critical_rate));

    // 随机判定
    float random_value = (std::rand() % 10000) / 100.0f;
    return random_value <= critical_rate;
}

uint32_t HealCalculator::CalculateCriticalHeal(
    uint32_t base_heal,
    float critical_multiplier) {

    float heal = static_cast<float>(base_heal) * critical_multiplier;
    return static_cast<uint32_t>(heal);
}

// ========== 溢出治疗计算 ==========

uint32_t HealCalculator::CalculateActualHeal(
    uint32_t heal_amount,
    uint32_t current_hp,
    uint32_t max_hp) {

    if (current_hp >= max_hp) {
        return 0;  // 已满血,无需治疗
    }

    uint32_t missing_hp = max_hp - current_hp;

    if (heal_amount >= missing_hp) {
        return missing_hp;  // 治疗量大于缺失量,只治疗到满血
    } else {
        return heal_amount;
    }
}

bool HealCalculator::IsOverHeal(
    uint32_t heal_amount,
    uint32_t current_hp,
    uint32_t max_hp) {

    uint32_t missing_hp = max_hp - current_hp;
    return heal_amount > missing_hp;
}

// ========== 治疗修正 ==========

uint32_t HealCalculator::ApplyHealMultiplier(
    uint32_t heal,
    float multiplier) {

    float result = static_cast<float>(heal) * multiplier;
    return std::max(1, static_cast<int32_t>(result));
}

uint32_t HealCalculator::ApplyHealBonus(
    uint32_t heal,
    float bonus) {

    float result = static_cast<float>(heal) + bonus;
    return std::max(1, static_cast<int32_t>(result));
}

uint32_t HealCalculator::ApplyHealReceivedMultiplier(
    uint32_t heal,
    float multiplier) {

    float result = static_cast<float>(heal) * multiplier;
    return std::max(0, static_cast<int32_t>(result));  // 可以为0
}

// ========== 辅助函数 ==========

HealParameters HealCalculator::GetHealParameters(const GameObject* caster) {
    HealParameters params;

    if (!caster) return params;

    // 从GameObject获取实际治疗属性
    params.heal_power = caster->GetHealPower();
    params.critical_rate = caster->GetCriticalRate();

    // 获取目标HP/MP信息(用于计算溢出)
    params.max_hp = caster->GetMaxHP();
    params.current_hp = caster->GetCurrentHP();
    params.max_mp = caster->GetMaxMP();
    params.current_mp = caster->GetCurrentMP();

    spdlog::trace("[HealCalc] Getting heal params for object {}: heal_power={}, crit={}, hp={}/{}",
                 caster->GetObjectId(), params.heal_power, params.critical_rate,
                 params.current_hp, params.max_hp);

    return params;
}

uint32_t HealCalculator::QuickCalculateHeal(
    uint32_t heal_power,
    float skill_heal_rate) {

    float heal = static_cast<float>(heal_power) * skill_heal_rate;
    return std::max(1, static_cast<int32_t>(heal));
}

uint32_t HealCalculator::QuickCalculateCriticalHeal(
    uint32_t base_heal,
    float critical_rate,
    float critical_multiplier) {

    // 检查是否暴击
    float random_value = (std::rand() % 10000) / 100.0f;

    if (random_value <= critical_rate) {
        // 暴击
        return static_cast<uint32_t>(base_heal * critical_multiplier);
    } else {
        return base_heal;
    }
}

bool HealCalculator::IsValidHealResult(const HealResult& result) {
    // 有效的治疗结果: 治疗量大于0
    return result.actual_heal > 0;
}

// ========== Helper Functions ==========

uint32_t QuickCalculateHPHeal(
    uint32_t heal_power,
    uint32_t current_hp,
    uint32_t max_hp,
    float skill_heal_rate) {

    float heal = static_cast<float>(heal_power) * skill_heal_rate;
    uint32_t heal_amount = static_cast<uint32_t>(heal);

    // 计算实际治疗
    if (current_hp >= max_hp) {
        return 0;
    }

    uint32_t missing_hp = max_hp - current_hp;
    if (heal_amount >= missing_hp) {
        return missing_hp;
    } else {
        return heal_amount;
    }
}

uint32_t QuickCalculateMPHeal(
    uint32_t heal_power,
    uint32_t current_mp,
    uint32_t max_mp,
    float skill_heal_rate) {

    float heal = static_cast<float>(heal_power) * skill_heal_rate;
    uint32_t heal_amount = static_cast<uint32_t>(heal);

    // 计算实际恢复
    if (current_mp >= max_mp) {
        return 0;
    }

    uint32_t missing_mp = max_mp - current_mp;
    if (heal_amount >= missing_mp) {
        return missing_mp;
    } else {
        return heal_amount;
    }
}

} // namespace Game
} // namespace Murim
