// MurimServer - Heal Calculator
// 治疗计算器 - 计算技能和物品的治疗量
// 对应legacy: 治疗计算相关函数

#pragma once

#include <cstdint>

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class GameObject;
class SkillInfo;

// ========== Heal Type ==========
// 治疗类型
enum class HealType : uint8_t {
    HP = 0,             // 生命值
    MP = 1,             // 魔法值
    Both = 2            // 同时恢复HP和MP
};

// ========== Heal Result ==========
// 治疗结果
struct HealResult {
    uint32_t heal_amount;         // 治疗量
    uint32_t base_heal;           // 基础治疗量
    HealType heal_type;           // 治疗类型
    bool is_critical;             // 是否暴击
    float critical_multiplier;    // 暴击倍率
    bool over_heal;               // 是否溢出治疗
    uint32_t actual_heal;         // 实际治疗量(考虑溢出)
    uint32_t mp_heal;             // MP恢复量

    HealResult()
        : heal_amount(0),
          base_heal(0),
          heal_type(HealType::HP),
          is_critical(false),
          critical_multiplier(1.0f),
          over_heal(false),
          actual_heal(0),
          mp_heal(0) {}
};

// ========== Heal Parameters ==========
// 治疗参数
struct HealParameters {
    // 施法者属性
    uint32_t heal_power;          // 治疗强度
    float critical_rate;          // 暴击率(0-100)
    float critical_heal_bonus;    // 暴击治疗倍率
    uint32_t max_hp;               // 目标最大HP(用于计算溢出)
    uint32_t current_hp;           // 目标当前HP
    uint32_t max_mp;               // 目标最大MP(用于计算溢出)
    uint32_t current_mp;           // 目标当前MP

    // 技能相关
    uint32_t skill_id;             // 技能ID
    uint16_t skill_level;          // 技能等级
    float skill_heal_rate;         // 技能治疗加成比例
    float skill_base_heal;         // 技能基础治疗量

    // 额外修正
    float heal_multiplier;        // 治疗倍率
    float heal_bonus;             // 额外治疗加成
    float heal_received_multiplier; // 承受治疗倍率

    HealParameters()
        : heal_power(100),
          critical_rate(5.0f),
          critical_heal_bonus(1.5f),
          max_hp(100),
          current_hp(0),
          max_mp(100),
          current_mp(0),
          skill_id(0),
          skill_level(1),
          skill_heal_rate(1.0f),
          skill_base_heal(0.0f),
          heal_multiplier(1.0f),
          heal_bonus(0.0f),
          heal_received_multiplier(1.0f) {}
};

// ========== Heal Calculator ==========
// 治疗计算器
class HealCalculator {
public:
    // ========== 基础治疗计算 ==========

    // 计算技能治疗
    static HealResult CalculateSkillHeal(
        const GameObject* caster,
        const GameObject* target,
        const SkillInfo* skill_info,
        uint16_t skill_level,
        const HealParameters& heal_params
    );

    // 计算物品治疗
    static HealResult CalculateItemHeal(
        const GameObject* user,
        const GameObject* target,
        uint32_t item_id,
        const HealParameters& heal_params
    );

    // ========== HP治疗计算 ==========

    // 计算HP治疗量
    static uint32_t CalculateHPHeal(
        uint32_t heal_power,
        float skill_heal_rate,
        float heal_multiplier,
        float heal_bonus
    );

    // ========== MP恢复计算 ==========

    // 计算MP恢复量
    static uint32_t CalculateMPHeal(
        uint32_t heal_power,
        float skill_heal_rate,
        float heal_multiplier,
        float heal_bonus
    );

    // ========== HoT治疗计算 ==========

    // 计算HoT总治疗量(持续治疗)
    static uint32_t CalculateHotTotalHeal(
        uint32_t base_heal,
        uint32_t tick_count,
        float tick_rate
    );

    // 计算HoT单次Tick治疗量
    static uint32_t CalculateHotTickHeal(
        uint32_t total_heal,
        uint32_t tick_count
    );

    // ========== 暴击治疗计算 ==========

    // 检查治疗是否暴击
    static bool CheckHealCritical(float critical_rate);

    // 计算暴击治疗量
    static uint32_t CalculateCriticalHeal(
        uint32_t base_heal,
        float critical_multiplier
    );

    // ========== 溢出治疗计算 ==========

    // 计算实际治疗量(考虑溢出)
    static uint32_t CalculateActualHeal(
        uint32_t heal_amount,
        uint32_t current_hp,
        uint32_t max_hp
    );

    // 检查是否溢出
    static bool IsOverHeal(
        uint32_t heal_amount,
        uint32_t current_hp,
        uint32_t max_hp
    );

    // ========== 治疗修正 ==========

    // 应用治疗倍率
    static uint32_t ApplyHealMultiplier(
        uint32_t heal,
        float multiplier
    );

    // 应用治疗加成
    static uint32_t ApplyHealBonus(
        uint32_t heal,
        float bonus
    );

    // 应用承治疗倍率
    static uint32_t ApplyHealReceivedMultiplier(
        uint32_t heal,
        float multiplier
    );

    // ========== 辅助函数 ==========

    // 获取治疗参数(从GameObject)
    static HealParameters GetHealParameters(const GameObject* caster);

    // 快速计算治疗(简化版本)
    static uint32_t QuickCalculateHeal(
        uint32_t heal_power,
        float skill_heal_rate = 1.0f
    );

    // 快速计算暴击治疗
    static uint32_t QuickCalculateCriticalHeal(
        uint32_t base_heal,
        float critical_rate = 5.0f,
        float critical_multiplier = 1.5f
    );

    // 检查治疗结果是否有效
    static bool IsValidHealResult(const HealResult& result);
};

// ========== Helper Functions ==========

// 快速计算HP治疗
uint32_t QuickCalculateHPHeal(
    uint32_t heal_power,
    uint32_t current_hp,
    uint32_t max_hp,
    float skill_heal_rate = 1.0f
);

// 快速计算MP恢复
uint32_t QuickCalculateMPHeal(
    uint32_t heal_power,
    uint32_t current_mp,
    uint32_t max_mp,
    float skill_heal_rate = 1.0f
);

} // namespace Game
} // namespace Murim
