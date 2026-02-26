#include "Battle.hpp"
#include "AttackCalculator.hpp"
#include "Buff.hpp"
#include "../skill/SkillManager.hpp"
#include "../skill/Skill.hpp"
#include "shared/social/Party.hpp"
#include "core/spdlog_wrapper.hpp"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <memory>

namespace Murim {
namespace Game {

// ========== 静态成员初始化 ==========

BattleConfig BattleManager::default_config_;
bool BattleManager::config_initialized_ = false;

BattleManager& BattleManager::Instance() {
    static BattleManager instance;
    return instance;
}

const BattleConfig& BattleManager::GetDefaultConfig() {
    if (!config_initialized_) {
        default_config_.critical_multiplier = 2.0f;
        default_config_.critical_rate_bonus = 0.0f;
        default_config_.hit_rate_base = 0.9f;
        default_config_.dodge_rate_base = 0.05f;
        default_config_.attack_power_coefficient = 1.0f;
        default_config_.defense_reduction_rate = 0.5f;
        default_config_.level_diff_penalty = 0.1f;
        default_config_.min_damage = 1;
        config_initialized_ = true;
    }
    return default_config_;
}

void BattleManager::SetBattleConfig(const BattleConfig& config) {
    default_config_ = config;
    config_initialized_ = true;
}

// ========== 伤害计算 ==========

DamageResult BattleManager::CalculatePhysicalDamage(
    const BattleStats& attacker_stats,
    const BattleStats& defender_stats,
    const BattleConfig& config
) {
    DamageResult result;
    result.attacker_id = 0;  // 由调用者设置
    result.target_id = 0;
    result.attack_type = AttackType::kPhysical;
    result.damage_type = DamageType::kPhysical;

    spdlog::trace("[PhysicalDamage] Attacker Lv={}, PhyAtk={}-{}, CritRate={}, CriItem={}",
                  attacker_stats.level,
                  attacker_stats.physical_attack_min,
                  attacker_stats.physical_attack_max,
                  attacker_stats.critical_rate,
                  attacker_stats.unique_item_cri_rate);

    spdlog::trace("[PhysicalDamage] Defender Lv={}, PhyDef={}, RegistPhys={}, PartyDef={}",
                  defender_stats.level,
                  defender_stats.physical_defense,
                  defender_stats.regist_phys,
                  defender_stats.party_defence_rate);

    // 1. 暴击判定 (使用 AttackCalculator - 对应 legacy getCritical)
    result.is_critical = AttackCalculator::IsCritical(
        attacker_stats.critical_rate,              // attacker_critical
        attacker_stats.level,                       // attacker_level
        defender_stats.level,                       // target_level
        config.critical_rate_bonus,                 // critical_rate_bonus
        attacker_stats.unique_item_cri_rate         // unique_item_cri_rate (从装备获取)
    );

    // 2. 计算物理攻击力 (使用 AttackCalculator - 对应 legacy getPlayerPhysicalAttackPower)
    double attack_power = AttackCalculator::GetPlayerPhysicalAttackPower(
        attacker_stats.physical_attack_min,         // phy_attack_min
        attacker_stats.physical_attack_max,         // phy_attack_max
        attacker_stats.skill_base_atk,              // skill_base_atk (从技能获取)
        config.attack_power_coefficient,            // phy_attack_rate
        result.is_critical                          // is_critical
    );
    result.raw_damage = static_cast<uint32_t>(attack_power);

    spdlog::trace("[PhysicalDamage] AttackPower={:.2f}, IsCritical={}", attack_power, result.is_critical);

    // 3. 计算防御等级 (使用 AttackCalculator - 对应 legacy getPhyDefenceLevel)
    double defence_level = AttackCalculator::GetPhyDefenceLevel(
        defender_stats.physical_defense,            // phy_defense
        defender_stats.level,                       // defender_level
        attacker_stats.level,                       // attacker_level
        defender_stats.skill_phy_def,               // skill_phy_def (从技能获取)
        defender_stats.regist_phys,                 // regist_phys (从装备获取)
        attacker_stats.enemy_defen,                 // enemy_defen (从装备获取)
        defender_stats.party_defence_rate           // party_defence_rate (从组队获取)
    );
    result.defense = static_cast<uint32_t>(defence_level * 1000);  // 转换为整数用于显示

    spdlog::trace("[PhysicalDamage] DefenceLevel={:.4f} ({}%)", defence_level, defence_level * 100);

    // 4. 计算最终伤害 (对应 legacy 伤害公式: attack_power * (1.0 - defence_level))
    double final_damage = attack_power * (1.0 - defence_level);
    result.damage = static_cast<uint32_t>(final_damage);

    // 5. 确保最小伤害
    if (result.damage < config.min_damage) {
        result.damage = config.min_damage;
    }

    // 6. 命中判定 (使用完整命中公式)
    result.is_hit = CheckHit(attacker_stats, defender_stats, config);
    result.is_dodged = !result.is_hit;

    // 7. 如果未命中，伤害为0
    if (!result.is_hit) {
        result.damage = 0;
        spdlog::debug("[PhysicalDamage] MISSED! Attacker Lv={} vs Defender Lv={}",
                      attacker_stats.level, defender_stats.level);
    } else {
        spdlog::debug("[PhysicalDamage] HIT! RawPower={:.2f}, DefLevel={:.4f}, FinalDamage={}, Critical={}",
                     attack_power, defence_level, result.damage, result.is_critical);
    }

    return result;
}

DamageResult BattleManager::CalculateMugongDamage(
    const BattleStats& attacker_stats,
    const BattleStats& defender_stats,
    uint32_t skill_id,
    const BattleConfig& config
) {
    DamageResult result;
    result.attacker_id = 0;
    result.target_id = 0;
    result.attack_type = AttackType::kMugong;
    result.damage_type = DamageType::kPhysical;

    spdlog::trace("[MugongDamage] SkillID={}, Attacker Lv={}, Defender Lv={}",
                  skill_id, attacker_stats.level, defender_stats.level);

    // 从 SkillManager 获取技能数据
    auto skill_opt = SkillManager::Instance().GetSkill(skill_id);

    if (!skill_opt) {
        spdlog::warn("[MugongDamage] Skill not found: {}", skill_id);
        result.damage = config.min_damage;
        result.is_hit = true;
        return result;
    }

    const auto& skill = *skill_opt;

    spdlog::trace("[MugongDamage] Skill: Name={}, BaseDmg={}, Multiplier={:.2f}, Attr={}",
                  skill.name, skill.base_damage, skill.damage_multiplier,
                  static_cast<int>(skill.attribute));

    // 1. 暴击判定
    result.is_critical = AttackCalculator::IsCritical(
        attacker_stats.critical_rate,              // attacker_critical
        attacker_stats.level,                       // attacker_level
        defender_stats.level,                       // target_level
        config.critical_rate_bonus,                 // critical_rate_bonus
        attacker_stats.unique_item_cri_rate         // unique_item_cri_rate
    );

    // 2. 计算物理攻击力 (对应 legacy getPlayerPhysicalAttackPower)
    // 武功使用技能倍率作为 phy_attack_rate
    double physical_attack = AttackCalculator::GetPlayerPhysicalAttackPower(
        attacker_stats.physical_attack_min,
        attacker_stats.physical_attack_max,
        skill.damage_multiplier,                    // 使用技能倍率作为 skill_base_atk
        skill.damage_multiplier,                    // 技能倍率作为 phy_attack_rate
        result.is_critical
    );

    spdlog::trace("[MugongDamage] PhysicalAttack={:.2f}", physical_attack);

    // 3. 计算属性攻击力 (如果有属性) (对应 legacy getPlayerAttributeAttackPower)
    double attribute_attack = 0.0;
    if (skill.attribute != AttributeType::kNone) {
        attribute_attack = AttackCalculator::GetPlayerAttributeAttackPower(
            attacker_stats.level,
            attacker_stats.simmek,                   // 内功值
            skill.damage_multiplier,                 // 使用技能倍率作为 attrib_attack_rate
            attacker_stats.attrib_attack_min,        // 属性攻击最小值
            attacker_stats.attrib_attack_max,        // 属性攻击最大值
            attacker_stats.attrib_plus_percent       // 属性加成百分比
        );
        result.attribute_damage = static_cast<uint32_t>(attribute_attack);
        result.attribute = skill.attribute;

        spdlog::trace("[MugongDamage] AttributeAttack={:.2f}, SimMek={}, AttribMin={}, AttribMax={}, Plus={}%",
                      attribute_attack, attacker_stats.simmek,
                      attacker_stats.attrib_attack_min,
                      attacker_stats.attrib_attack_max,
                      attacker_stats.attrib_plus_percent);
    }

    // 4. 计算防御等级
    double defence_level = AttackCalculator::GetPhyDefenceLevel(
        defender_stats.physical_defense,
        defender_stats.level,
        attacker_stats.level,
        defender_stats.skill_phy_def,               // skill_phy_def (从防御者技能获取)
        defender_stats.regist_phys,                 // regist_phys (从防御者装备获取)
        attacker_stats.enemy_defen,                 // enemy_defen (从攻击者装备获取)
        defender_stats.party_defence_rate           // party_defence_rate (从防御者组队获取)
    );
    result.defense = static_cast<uint32_t>(defence_level * 1000);

    spdlog::trace("[MugongDamage] DefenceLevel={:.4f} ({}%)", defence_level, defence_level * 100);

    // 5. 武功伤害 = 物理攻击 + 属性攻击
    // 对应 legacy: 武功通常结合物理和属性攻击
    double total_attack = physical_attack + attribute_attack;
    result.raw_damage = static_cast<uint32_t>(total_attack);

    // 6. 计算最终伤害 (减去防御)
    double final_damage = total_attack * (1.0 - defence_level);
    result.damage = static_cast<uint32_t>(final_damage);

    // 7. 确保最小伤害
    if (result.damage < config.min_damage) {
        result.damage = config.min_damage;
    }

    // 8. 命中判定
    result.is_hit = CheckHit(attacker_stats, defender_stats, config);
    result.is_dodged = !result.is_hit;

    // 9. 如果未命中，伤害为0
    if (!result.is_hit) {
        result.damage = 0;
        spdlog::debug("[MugongDamage] MISSED! SkillID={}, Attacker Lv={} vs Defender Lv={}",
                      skill_id, attacker_stats.level, defender_stats.level);
    } else {
        spdlog::debug("[MugongDamage] HIT! Skill={}, Phys={:.2f}, Attr={:.2f}, Total={:.2f}, DefLevel={:.4f}, Final={}, Critical={}",
                      skill.name, physical_attack, attribute_attack, total_attack,
                      defence_level, result.damage, result.is_critical);
    }

    return result;
}

DamageResult BattleManager::CalculateMagicDamage(
    const BattleStats& attacker_stats,
    const BattleStats& defender_stats,
    uint32_t skill_id,
    const BattleConfig& config
) {
    DamageResult result;
    result.attacker_id = 0;
    result.target_id = 0;
    result.attack_type = AttackType::kMagic;
    result.damage_type = DamageType::kMagic;

    spdlog::trace("[MagicDamage] SkillID={}, Attacker Lv={}, Defender Lv={}",
                  skill_id, attacker_stats.level, defender_stats.level);

    // 从 SkillManager 获取技能数据
    auto skill_opt = SkillManager::Instance().GetSkill(skill_id);

    if (!skill_opt) {
        spdlog::warn("[MagicDamage] Skill not found: {}", skill_id);
        result.damage = config.min_damage;
        result.is_hit = true;
        return result;
    }

    const auto& skill = *skill_opt;

    spdlog::trace("[MagicDamage] Skill: Name={}, BaseDmg={}, Multiplier={:.2f}",
                  skill.name, skill.base_damage, skill.damage_multiplier);

    // 1. 魔法攻击使用智力 (对应 legacy 魔法攻击公式)
    // 基础魔法攻击 = 智力 * 技能倍率
    double base_magic_attack = static_cast<double>(attacker_stats.intelligence) * skill.damage_multiplier;

    // 2. 加上技能基础伤害
    double magic_attack = base_magic_attack + static_cast<double>(skill.base_damage);

    // 3. 随机波动 (+/- 10%)
    double variance = magic_attack * 0.1f;
    magic_attack += (static_cast<double>(rand() % 200 - 100) / 100.0f) * variance;

    result.raw_damage = static_cast<uint32_t>(magic_attack);

    spdlog::trace("[MagicDamage] BaseAttack={:.2f}, FinalAttack={:.2f}, Int={}",
                  base_magic_attack, magic_attack, attacker_stats.intelligence);

    // 4. 计算魔法防御等级 (使用与物理防御类似的公式)
    // 对应 legacy: 魔法防御也是基于防御力计算的
    double magic_defence_level = (static_cast<double>(defender_stats.magic_defense) * 2.0 + 50.0) /
                                  (static_cast<double>(attacker_stats.level) * 20.0 + 150.0);

    if (magic_defence_level < 0.0) {
        magic_defence_level = 0.0;
    }
    if (magic_defence_level > 0.9) {
        magic_defence_level = 0.9;
    }

    result.defense = static_cast<uint32_t>(magic_defence_level * 1000);

    spdlog::trace("[MagicDamage] MagicDefLevel={:.4f} ({}%)", magic_defence_level, magic_defence_level * 100);

    // 5. 计算最终伤害 (减去魔法防御)
    double final_damage = magic_attack * (1.0 - magic_defence_level);
    result.damage = static_cast<uint32_t>(final_damage);

    // 6. 确保最小伤害
    if (result.damage < config.min_damage) {
        result.damage = config.min_damage;
    }

    // 7. 魔法攻击不暴击
    result.is_critical = false;

    // 8. 命中判定 (魔法攻击命中率较高)
    result.is_hit = CheckHit(attacker_stats, defender_stats, config);
    result.is_dodged = !result.is_hit;

    // 9. 如果未命中，伤害为0
    if (!result.is_hit) {
        result.damage = 0;
        spdlog::debug("[MagicDamage] MISSED! SkillID={}, Attacker Lv={} vs Defender Lv={}",
                      skill_id, attacker_stats.level, defender_stats.level);
    } else {
        spdlog::debug("[MagicDamage] HIT! Skill={}, Attack={:.2f}, DefLevel={:.4f}, Final={}",
                      skill.name, magic_attack, magic_defence_level, result.damage);
    }

    return result;
}

// ========== 经验值计算 ==========

uint32_t BattleManager::CalculateExpReward(
    uint16_t player_level,
    uint16_t monster_level,
    uint32_t monster_exp
) {
    // 使用 AttackCalculator - 对应 legacy GetPlayerExpPoint
    int level_gap = static_cast<int>(player_level) - static_cast<int>(monster_level);
    uint32_t exp_reward = AttackCalculator::GetPlayerExpPoint(level_gap, monster_exp);

    spdlog::trace("[ExpReward] PlayerLv={}, MonsterLv={}, BaseExp={}, LevelGap={}, FinalExp={}",
                  player_level, monster_level, monster_exp, level_gap, exp_reward);

    return exp_reward;
}

// ========== 命中判定 ==========

bool BattleManager::CheckHit(
    const BattleStats& attacker_stats,
    const BattleStats& defender_stats,
    const BattleConfig& config
) {
    // 对应 legacy 命中计算逻辑
    // 基础命中率 = 90% (config.hit_rate_base = 0.9f)

    // 1. 计算基础命中率
    float hit_rate = config.hit_rate_base;

    // 2. 等级差影响 (对应 legacy GetPercent 逻辑)
    // 使用 GetPercent 公式: SeedVal + (AttackerLevel - TargetLevel) * 0.025
    int level_diff = static_cast<int>(attacker_stats.level) - static_cast<int>(defender_stats.level);
    float level_adjustment = level_diff * 0.025f;

    hit_rate += level_adjustment;

    spdlog::trace("[CheckHit] BaseHitRate={:.4f}, LevelDiff={}, Adjustment={:.4f}, FinalHitRate={:.4f}",
                  config.hit_rate_base, level_diff, level_adjustment, hit_rate);

    // 3. 闪避率修正 (对应 legacy dodge 逻辑)
    // 基础闪避率 = 5% (config.dodge_rate_base = 0.05f)
    float dodge_rate = config.dodge_rate_base;

    // 等级差对闪避的影响 (等级高闪避率低)
    dodge_rate -= level_diff * 0.01f;

    // 4. 从 BattleStats 获取闪避率修正
    dodge_rate += (defender_stats.dodge_rate / 100.0f);

    // 限制闪避率范围 [0, 0.5]
    if (dodge_rate < 0.0f) {
        dodge_rate = 0.0f;
    } else if (dodge_rate > 0.5f) {
        dodge_rate = 0.5f;
    }

    // 5. 实际命中率 = 命中率 - 闪避率
    float actual_hit_rate = hit_rate - dodge_rate;

    // 限制命中率范围 [0.05, 0.95]
    if (actual_hit_rate < 0.05f) {
        actual_hit_rate = 0.05f;
    } else if (actual_hit_rate > 0.95f) {
        actual_hit_rate = 0.95f;
    }

    spdlog::trace("[CheckHit] DodgeRate={:.4f}, ActualHitRate={:.4f}", dodge_rate, actual_hit_rate);

    // 6. 随机判定 (对应 legacy rand()%100)
    float random_value = static_cast<float>(rand() % 10000) / 10000.0f;
    bool is_hit = random_value < actual_hit_rate;

    spdlog::trace("[CheckHit] Random={:.4f}, IsHit={}", random_value, is_hit);

    return is_hit;
}

bool BattleManager::CheckCritical(
    const BattleStats& attacker_stats,
    const BattleStats& defender_stats,
    const BattleConfig& config
) {
    // 使用 AttackCalculator - 对应 legacy getCritical
    return AttackCalculator::IsCritical(
        attacker_stats.critical_rate,              // attacker_critical
        attacker_stats.level,                       // attacker_level
        defender_stats.level,                       // target_level
        config.critical_rate_bonus,                 // critical_rate_bonus
        attacker_stats.unique_item_cri_rate         // unique_item_cri_rate
    );
}

bool BattleManager::CheckDecisive(
    const BattleStats& attacker_stats,
    const BattleStats& defender_stats,
    const BattleConfig& config
) {
    // 使用 AttackCalculator - 对应 legacy getDecisive
    return AttackCalculator::IsDecisive(
        attacker_stats.critical_rate,              // decisive_rate (使用 critical_rate)
        attacker_stats.level,                       // attacker_level
        defender_stats.level,                       // target_level
        config.critical_rate_bonus,                 // critical_rate_bonus
        0                                           // unique_item_dec_rate (装备加成)
    );
}

bool BattleManager::CheckDodge(
    const BattleStats& attacker_stats,
    const BattleStats& defender_stats,
    const BattleConfig& config
) {
    // 闪避判定 = !CheckHit
    return !CheckHit(attacker_stats, defender_stats, config);
}

// ========== 防御计算 ==========

uint32_t BattleManager::CalculateDefense(
    const BattleStats& stats,
    AttackType attack_type
) {
    // 根据攻击类型返回相应的防御力
    switch (attack_type) {
        case AttackType::kPhysical:
            return stats.physical_defense;
        case AttackType::kMagic:
            return stats.magic_defense;
        case AttackType::kMugong:
            // 武功攻击通常无视部分防御 (50%)
            return stats.physical_defense / 2;
        default:
            return stats.physical_defense;
    }
}

// ========== 最终伤害计算 ==========

uint32_t BattleManager::CalculateFinalDamage(
    uint32_t raw_damage,
    uint32_t defense,
    int16_t level_diff,
    const BattleConfig& config
) {
    // 这个方法保留以兼容旧代码，但推荐使用 CalculatePhysicalDamage/CalculateMugongDamage
    // 它们内部使用 AttackCalculator

    // 1. 应用等级惩罚
    float penalty = CalculateLevelPenalty(level_diff, level_diff);

    // 2. 计算减伤后伤害
    int32_t damage = static_cast<int32_t>(raw_damage * penalty) -
                     static_cast<int32_t>(defense * config.defense_reduction_rate);

    // 3. 确保最小伤害
    return static_cast<uint32_t>(std::max(damage, static_cast<int32_t>(config.min_damage)));
}

// ========== 等级惩罚 ==========

float BattleManager::CalculateLevelPenalty(
    uint16_t attacker_level,
    uint16_t defender_level
) {
    // 对应 legacy 中的等级差影响
    // 参考 GetPercent 公式: SeedVal + (OperatorLevel - TargetLevel) * 0.025

    int level_diff = static_cast<int>(attacker_level) - static_cast<int>(defender_level);

    // 等级惩罚系数
    float penalty = 1.0f;

    if (level_diff < -8) {
        // 攻击者比目标低9级或更多: 伤害减半
        penalty = 0.5f;
    } else if (level_diff < -5) {
        // 攻击者比目标低6-8级: 伤害减少 20-40%
        penalty = 0.6f + (level_diff + 8) * 0.1f;
    } else if (level_diff < 0) {
        // 攻击者比目标低1-5级: 伤害减少 0-20%
        penalty = 0.8f + level_diff * 0.04f;
    } else if (level_diff == 0) {
        // 同等级: 无惩罚
        penalty = 1.0f;
    } else if (level_diff <= 5) {
        // 攻击者比目标高1-5级: 伤害增加 0-25%
        penalty = 1.0f + level_diff * 0.05f;
    } else if (level_diff <= 10) {
        // 攻击者比目标高6-10级: 伤害增加 25-40%
        penalty = 1.25f + (level_diff - 5) * 0.03f;
    } else {
        // 攻击者比目标高11级或更多: 伤害增加 40% (上限)
        penalty = 1.4f;
    }

    // 限制范围 [0.5, 1.5]
    if (penalty < 0.5f) {
        penalty = 0.5f;
    } else if (penalty > 1.5f) {
        penalty = 1.5f;
    }

    spdlog::trace("[LevelPenalty] AttackerLv={}, DefenderLv={}, LevelDiff={}, Penalty={:.4f}",
                  attacker_level, defender_level, level_diff, penalty);

    return penalty;
}

// ========== 属性伤害计算 ==========

uint32_t BattleManager::CalculateAttributeDamage(
    uint32_t base_damage,
    AttributeType attribute,
    const BattleStats& attacker_stats,
    const BattleStats& defender_stats
) {
    // 对应 legacy getPlayerAttributeAttackPower
    // 属性伤害 = 内功加成 + 属性攻击加成

    if (attribute == AttributeType::kNone) {
        return 0;
    }

    // 1. 使用 AttackCalculator 计算属性攻击力
    double attribute_attack = AttackCalculator::GetPlayerAttributeAttackPower(
        attacker_stats.level,
        attacker_stats.simmek,                   // 内功值
        1.0f,                                    // attrib_attack_rate (默认100%)
        attacker_stats.attrib_attack_min,        // 属性攻击最小值
        attacker_stats.attrib_attack_max,        // 属性攻击最大值
        attacker_stats.attrib_plus_percent       // 属性加成百分比
    );

    uint32_t attr_damage = static_cast<uint32_t>(attribute_attack);

    spdlog::trace("[AttributeDamage] Base={}, Attr={}, SimMek={}, AttribMin={}, AttribMax={}, Plus={}%, Result={}",
                  base_damage, static_cast<int>(attribute), attacker_stats.simmek,
                  attacker_stats.attrib_attack_min, attacker_stats.attrib_attack_max,
                  attacker_stats.attrib_plus_percent, attr_damage);

    return attr_damage;
}

// ========== 技能伤害计算 ==========

uint32_t BattleManager::CalculateSkillDamage(
    const Skill& skill,
    const BattleStats& attacker_stats,
    const BattleStats& defender_stats
) {
    // 使用 AttackCalculator 实现技能伤害
    // 对应 legacy 中的技能伤害计算

    spdlog::trace("[SkillDamage] SkillID={}, Name={}, BaseDmg={}, Multiplier={:.2f}",
                  skill.skill_id, skill.name, skill.base_damage, skill.damage_multiplier);

    uint32_t damage = 0;

    // 根据技能类型选择伤害计算方式
    if (skill.damage_type == DamageType::kPhysical) {
        // 物理技能
        double physical_attack = AttackCalculator::GetPlayerPhysicalAttackPower(
            attacker_stats.physical_attack_min,
            attacker_stats.physical_attack_max,
            skill.damage_multiplier,                // skill_base_atk
            skill.damage_multiplier,                // phy_attack_rate
            false                                   // 暴击在外部判定
        );

        // 加上技能基础伤害
        damage = static_cast<uint32_t>(physical_attack + skill.base_damage);

    } else if (skill.damage_type == DamageType::kMagic) {
        // 魔法技能
        double magic_attack = static_cast<double>(attacker_stats.intelligence) * skill.damage_multiplier;
        damage = static_cast<uint32_t>(magic_attack + skill.base_damage);

    } else if (skill.damage_type == DamageType::kTrue) {
        // 真实伤害 (无视防御)
        damage = skill.base_damage;
    }

    spdlog::trace("[SkillDamage] Result={}", damage);

    return damage;
}

// ========== BattleEvent 实现 ==========

std::string BattleEvent::ToLogString() const {
    char buffer[512];
    snprintf(buffer, sizeof(buffer),
             "BattleEvent[id=%llu, attacker=%llu, target=%llu, type=%d, damage=%u, critical=%d]",
             event_id, attacker_id, target_id,
             static_cast<int>(attack_type), damage, is_critical);
    return std::string(buffer);
}

} // namespace Game
} // namespace Murim
