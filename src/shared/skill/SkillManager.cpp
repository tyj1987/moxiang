#include "SkillManager.hpp"
#include "Skill.hpp"
#include "shared/character/CharacterManager.hpp"
#include "shared/battle/Battle.hpp"
#include "../movement/Movement.hpp"
#include "core/network/NotificationService.hpp"
#include "core/database/SkillDAO.hpp"
#include "core/spdlog_wrapper.hpp"
#include <unordered_map>
#include <algorithm>
#include <numeric>

// 定义M_PI（Windows需要）
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Game {

// 为Database namespace创建别名
namespace DB = Murim::Core::Database;

SkillManager& SkillManager::Instance() {
    static SkillManager instance;
    return instance;
}

void SkillManager::Update(uint32_t delta_time) {
    // 处理技能冷却、持续效果等 (暂时为空)
    (void)delta_time;
}

// ========== 技能加载 ==========

bool SkillManager::Initialize() {
    if (initialized_) {
        return true;
    }

    spdlog::info("Initializing SkillManager...");

    LoadSkills();

    initialized_ = true;
    spdlog::info("SkillManager initialized: {} skills loaded", skills_.size());
    return true;
}

void SkillManager::LoadSkills() {
    // 从数据库加载所有技能模板
    auto all_skills = DB::SkillDAO::LoadAll();

    for (const auto& skill : all_skills) {
        skills_[skill.skill_id] = std::make_shared<Skill>(skill);
    }

    // 如果数据库为空,使用默认技能(用于测试)
    if (skills_.empty()) {
        spdlog::warn("No skills loaded from database, using default skills");

        // 示例: 基础攻击技能
        auto basic_attack = std::make_shared<Skill>();
        basic_attack->skill_id = 1;
        basic_attack->name = "基础攻击";
        basic_attack->description = "普通物理攻击";
        basic_attack->skill_type = SkillType::kActive;
        basic_attack->target_type = SkillTargetType::kSingle;
        basic_attack->area_shape = SkillAreaShape::kNone;
        basic_attack->damage_type = DamageType::kPhysical;
        basic_attack->base_damage = 10;
        basic_attack->damage_multiplier = 1.0f;
        basic_attack->range = 5.0f;
        basic_attack->mp_cost = 0;
        basic_attack->stamina_cost = 5;
        basic_attack->cooldown = 1;
        basic_attack->global_cooldown = 0;
        basic_attack->required_level = 1;
        basic_attack->attribute = AttributeType::kNone;

        skills_[basic_attack->skill_id] = basic_attack;

        // 示例: 火球术(范围魔法)
        auto fireball = std::make_shared<Skill>();
        fireball->skill_id = 100;
        fireball->name = "火球术";
        fireball->description = "发射火球攻击范围内的敌人";
        fireball->skill_type = SkillType::kActive;
        fireball->target_type = SkillTargetType::kArea;
        fireball->area_shape = SkillAreaShape::kCircle;
        fireball->damage_type = DamageType::kMagic;
        fireball->base_damage = 50;
        fireball->damage_multiplier = 1.5f;
        fireball->attribute = AttributeType::kFire;
        fireball->range = 15.0f;
        fireball->radius = 5.0f;
        fireball->mp_cost = 20;
        fireball->stamina_cost = 0;
        fireball->cooldown = 5;
        fireball->global_cooldown = 1;
        fireball->required_level = 1;  // 修改为1级以便测试

        skills_[fireball->skill_id] = fireball;
    }

    spdlog::info("Loaded {} skills", skills_.size());
}

// ========== 技能查询 ==========

std::shared_ptr<Skill> SkillManager::GetSkill(uint32_t skill_id) const {
    auto it = skills_.find(skill_id);
    if (it != skills_.end()) {
        return it->second;
    }

    spdlog::warn("Skill not found: {}", skill_id);
    return nullptr;
}

std::vector<uint32_t> SkillManager::GetCharacterSkills(uint64_t character_id) {
    // 从数据库加载角色已学技能
    return DB::SkillDAO::LoadCharacterSkills(character_id);
}

std::vector<uint32_t> SkillManager::GetRequiredSkills(uint32_t skill_id) {
    auto skill_opt = GetSkill(skill_id);
    if (skill_opt) {
        return skill_opt->required_skills;
    }
    return {};
}

// ========== 技能学习 ==========

bool SkillManager::LearnSkill(uint64_t character_id, uint32_t skill_id) {
    // 对应 legacy 技能学习逻辑
    try {
        // 检查技能是否存在
        auto skill_opt = Instance().GetSkill(skill_id);
        if (!skill_opt) {
            spdlog::error("Skill {} not found for learning", skill_id);
            return false;
        }

        // 检查角色是否已学会该技能
        if (DB::SkillDAO::HasCharacterSkill(character_id, skill_id)) {
            spdlog::debug("Character {} already learned skill {}", character_id, skill_id);
            return false;
        }

        // 检查前置技能 - 对应 legacy 技能前置要求
        const auto& required_skills = skill_opt->required_skills;
        if (!required_skills.empty()) {
            for (uint32_t required_id : required_skills) {
                if (!DB::SkillDAO::HasCharacterSkill(character_id, required_id)) {
                    spdlog::debug("Character {} missing required skill {} for learning {}",
                                character_id, required_id, skill_id);
                    return false;
                }
            }
        }

        // 获取角色等级并检查 skill_opt->required_level
        auto character = CharacterManager::Instance().GetCharacter(character_id);
        if (!character.has_value()) {
            spdlog::error("Character {} not found for skill learning", character_id);
            return false;
        }

        if (character->level < skill_opt->required_level) {
            spdlog::debug("Character {} level {} too low for skill {} (requires level {})",
                        character_id, character->level, skill_id, skill_opt->required_level);
            return false;
        }

        // 学习技能 - 对应 legacy 技能学习
        bool success = DB::SkillDAO::AddCharacterSkill(character_id, skill_id, 1);

        if (success) {
            spdlog::info("Character {} learned skill {}", character_id, skill_id);

            // 发送技能学习消息到客户端 - 对应 legacy 技能学习通知
            Core::Network::NotificationService::Instance().NotifySkillLearned(character_id, skill_id, 1);

            // 技能栏由客户端UI系统管理,服务端只发送技能列表更新通知
            // 日志已在上方的 spdlog::info 中记录
        }

        return success;
    } catch (const std::exception& e) {
        spdlog::error("Failed to learn skill {} for character {}: {}", skill_id, character_id, e.what());
        return false;
    }
}

// ========== 技能施放 ==========

SkillManager::CastResult SkillManager::CastSkill(
    uint64_t caster_id,
    uint32_t skill_id,
    const std::vector<uint64_t>& target_ids,
    const Position& position,
    uint16_t direction
) {
    CastResult result;
    result.success = false;

    // 获取技能信息
    auto skill_it = Instance().skills_.find(skill_id);
    if (skill_it == Instance().skills_.end()) {
        result.error_message = "Skill not found: " + std::to_string(skill_id);
        return result;
    }

    const auto& skill = skill_it->second;

    // 检查冷却
    uint16_t cooldown = GetSkillCooldown(caster_id, skill_id);
    if (cooldown > 0) {
        result.error_message = "Skill on cooldown: " + std::to_string(cooldown) + "s remaining";
        return result;
    }

    // 获取施法者信息
    auto caster = CharacterManager::Instance().GetCharacter(caster_id);
    if (!caster.has_value()) {
        result.error_message = "Caster not found";
        return result;
    }

    // 检查MP/体力消耗
    if (caster->mp < skill->mp_cost) {
        result.error_message = "Not enough MP (requires " + std::to_string(skill->mp_cost) + ")";
        return result;
    }

    if (caster->stamina < skill->stamina_cost) {
        result.error_message = "Not enough stamina (requires " + std::to_string(skill->stamina_cost) + ")";
        return result;
    }

    // 检查目标有效性 (range, LOS, etc.)
    // 获取施法者位置
    Position caster_pos;
    caster_pos.x = caster->x;
    caster_pos.y = caster->y;
    caster_pos.z = caster->z;

    // 验证目标是否在范围内
    if (skill->target_type == SkillTargetType::kSingle && !target_ids.empty()) {
        auto target = CharacterManager::Instance().GetCharacter(target_ids[0]);
        if (!target.has_value()) {
            result.error_message = "Target not found";
            return result;
        }

        Position target_pos;
        target_pos.x = target->x;
        target_pos.y = target->y;
        target_pos.z = target->z;

        float distance = caster_pos.Distance2D(target_pos);
        if (distance > skill->range) {
            result.error_message = "Target out of range (distance: " + std::to_string(distance) +
                                  ", max: " + std::to_string(skill->range) + ")";
            return result;
        }
    }

    // 处理不同目标类型
    std::vector<BattleEvent> events;

    switch (skill->target_type) {
        case SkillTargetType::kSelf:
            // 自身技能(Buff)
            {
                BattleEvent event;
                event.attacker_id = caster_id;
                event.target_id = caster_id;
                event.attack_type = AttackType::kMagic;
                event.damage = 0;  // Buff不造成伤害
                event.is_critical = false;
                event.battle_time = std::chrono::system_clock::now();
                events.push_back(event);
            }
            break;

        case SkillTargetType::kSingle:
            // 单体技能
            if (target_ids.empty()) {
                result.error_message = "Single target skill requires target";
                return result;
            }

            {
                BattleEvent event;
                event.attacker_id = caster_id;
                event.target_id = target_ids[0];

                // 根据技能伤害类型选择攻击类型
                if (skill->damage_type == DamageType::kPhysical) {
                    event.attack_type = AttackType::kPhysical;
                } else if (skill->damage_type == DamageType::kMagic) {
                    event.attack_type = AttackType::kMagic;
                } else {
                    event.attack_type = AttackType::kMugong;
                }

                // 获取施法者属性用于伤害计算
                auto caster_stats = CharacterManager::Instance().GetCharacterStats(caster_id);
                BattleStats stats;
                if (caster_stats.has_value()) {
                    stats.level = caster->level;
                    stats.strength = caster_stats->strength;
                    stats.intelligence = caster_stats->intelligence;
                    stats.dexterity = caster_stats->dexterity;
                    stats.vitality = caster_stats->vitality;
                    stats.wisdom = caster_stats->wisdom;
                    stats.physical_attack = caster_stats->physical_attack;
                    stats.magic_attack = caster_stats->magic_attack;
                }

                // 获取目标防御属性
                auto target_char_opt = CharacterManager::Instance().GetCharacter(target_ids[0]);
                BattleStats target_stats_battle;
                if (target_char_opt.has_value()) {
                    target_stats_battle.level = target_char_opt->level;
                    // 同时获取stats
                    auto target_stats = CharacterManager::Instance().GetCharacterStats(target_ids[0]);
                    if (target_stats.has_value()) {
                        target_stats_battle.physical_defense = target_stats->physical_defense;
                        target_stats_battle.magic_defense = target_stats->magic_defense;
                    }
                }

                // 计算伤害
                DamageResult damage_result;
                if (skill->damage_type == DamageType::kPhysical) {
                    damage_result = BattleManager::CalculatePhysicalDamage(stats, target_stats_battle);
                } else if (skill->damage_type == DamageType::kMagic) {
                    damage_result = BattleManager::CalculateMagicDamage(stats, target_stats_battle, skill_id);
                } else {
                    damage_result = BattleManager::CalculateMugongDamage(stats, target_stats_battle, skill_id);
                }

                event.damage = damage_result.damage;
                event.is_critical = damage_result.is_critical;
                event.battle_time = std::chrono::system_clock::now();
                events.push_back(event);
            }
            break;

        case SkillTargetType::kArea:
        case SkillTargetType::kAreaCircle:
            // 范围技能 - 获取范围内的所有目标并对每个目标计算伤害
            {
                // 使用传入的target_ids作为潜在目标
                // 在MapServer集成时,这里将使用SpatialIndex查询范围内的实体
                std::vector<uint64_t> targets_in_range = GetTargetsInRange(caster_pos, *skill, target_ids);

                // 获取施法者属性 (复用上方逻辑)
                auto caster_stats = CharacterManager::Instance().GetCharacterStats(caster_id);
                BattleStats stats;
                if (caster_stats.has_value()) {
                    stats.level = caster->level;
                    stats.strength = caster_stats->strength;
                    stats.intelligence = caster_stats->intelligence;
                    stats.dexterity = caster_stats->dexterity;
                    stats.vitality = caster_stats->vitality;
                    stats.wisdom = caster_stats->wisdom;
                    stats.physical_attack = caster_stats->physical_attack;
                    stats.magic_attack = caster_stats->magic_attack;
                }

                // 对范围内的每个目标计算伤害
                for (uint64_t target_id : targets_in_range) {
                    BattleEvent event;
                    event.attacker_id = caster_id;
                    event.target_id = target_id;

                    // 根据技能伤害类型选择攻击类型
                    if (skill->damage_type == DamageType::kPhysical) {
                        event.attack_type = AttackType::kPhysical;
                    } else if (skill->damage_type == DamageType::kMagic) {
                        event.attack_type = AttackType::kMagic;
                    } else {
                        event.attack_type = AttackType::kMugong;
                    }

                    // 获取目标防御属性
                    auto target_stats = CharacterManager::Instance().GetCharacterStats(target_id);
                    BattleStats target_stats_battle;
                    if (target_stats.has_value()) {
                        target_stats_battle.physical_defense = target_stats->physical_defense;
                        target_stats_battle.magic_defense = target_stats->magic_defense;
                    }

                    // 计算伤害
                    DamageResult damage_result;
                    if (skill->damage_type == DamageType::kPhysical) {
                        damage_result = BattleManager::CalculatePhysicalDamage(stats, target_stats_battle);
                    } else if (skill->damage_type == DamageType::kMagic) {
                        damage_result = BattleManager::CalculateMagicDamage(stats, target_stats_battle, skill_id);
                    } else {
                        damage_result = BattleManager::CalculateMugongDamage(stats, target_stats_battle, skill_id);
                    }

                    event.damage = damage_result.damage;
                    event.is_critical = damage_result.is_critical;
                    event.battle_time = std::chrono::system_clock::now();
                    events.push_back(event);
                }
            }
            break;

        case SkillTargetType::kAreaSector:
        case SkillTargetType::kAreaLine:
            // 扇形和直线范围技能
            // 实现与圆形范围类似,但使用不同的范围判定
            {
                std::vector<uint64_t> targets_in_range = GetTargetsInRange(caster_pos, *skill, target_ids);

                auto caster_stats = CharacterManager::Instance().GetCharacterStats(caster_id);
                BattleStats stats;
                if (caster_stats.has_value()) {
                    stats.level = caster->level;
                    stats.strength = caster_stats->strength;
                    stats.intelligence = caster_stats->intelligence;
                    stats.physical_attack = caster_stats->physical_attack;
                    stats.magic_attack = caster_stats->magic_attack;
                }

                for (uint64_t target_id : targets_in_range) {
                    BattleEvent event;
                    event.attacker_id = caster_id;
                    event.target_id = target_id;

                    if (skill->damage_type == DamageType::kPhysical) {
                        event.attack_type = AttackType::kPhysical;
                    } else if (skill->damage_type == DamageType::kMagic) {
                        event.attack_type = AttackType::kMagic;
                    } else {
                        event.attack_type = AttackType::kMugong;
                    }

                    auto target_stats = CharacterManager::Instance().GetCharacterStats(target_id);
                    BattleStats target_stats_battle;
                    if (target_stats.has_value()) {
                        target_stats_battle.physical_defense = target_stats->physical_defense;
                        target_stats_battle.magic_defense = target_stats->magic_defense;
                    }

                    DamageResult damage_result;
                    if (skill->damage_type == DamageType::kPhysical) {
                        damage_result = BattleManager::CalculatePhysicalDamage(stats, target_stats_battle);
                    } else if (skill->damage_type == DamageType::kMagic) {
                        damage_result = BattleManager::CalculateMagicDamage(stats, target_stats_battle, skill_id);
                    } else {
                        damage_result = BattleManager::CalculateMugongDamage(stats, target_stats_battle, skill_id);
                    }

                    event.damage = damage_result.damage;
                    event.is_critical = damage_result.is_critical;
                    event.battle_time = std::chrono::system_clock::now();
                    events.push_back(event);
                }
            }
            break;
    }

    // 扣除MP和体力
    if (skill->mp_cost > 0 || skill->stamina_cost > 0) {
        Character updated_caster = *caster;
        updated_caster.mp -= skill->mp_cost;
        updated_caster.stamina -= skill->stamina_cost;
        CharacterManager::Instance().SaveCharacter(updated_caster);
    }

    // 开始技能冷却
    StartSkillCooldown(caster_id, skill_id);

    result.success = true;
    result.battle_events = events;

    spdlog::info("Skill cast: caster_id={}, skill_id={}, targets={}, damage_dealt={}",
                 caster_id, skill_id, target_ids.size(),
                 std::accumulate(events.begin(), events.end(), 0,
                                [](int sum, const BattleEvent& e) { return sum + e.damage; }));

    return result;
}

// ========== 冷却管理 ==========

uint16_t SkillManager::GetSkillCooldown(uint64_t caster_id, uint32_t skill_id) {
    auto& instance = Instance();

    // 清理过期冷却记录
    instance.CleanupExpiredCooldowns();

    // 检查全局冷却
    auto gcd_it = instance.global_cooldowns_.find(caster_id);
    if (gcd_it != instance.global_cooldowns_.end()) {
        uint16_t gcd_remaining = gcd_it->second.GetRemainingCooldown();
        if (gcd_remaining > 0) {
            return gcd_remaining;
        }
    }

    // 检查技能冷却
    auto caster_it = instance.cooldowns_.find(caster_id);
    if (caster_it != instance.cooldowns_.end()) {
        auto skill_it = caster_it->second.find(skill_id);
        if (skill_it != caster_it->second.end()) {
            return skill_it->second.GetRemainingCooldown();
        }
    }

    return 0;
}

void SkillManager::StartSkillCooldown(uint64_t caster_id, uint32_t skill_id) {
    auto& instance = Instance();

    // 获取技能信息
    auto skill_opt = instance.GetSkill(skill_id);
    if (!skill_opt) {
        spdlog::warn("Failed to start cooldown: skill not found - {}", skill_id);
        return;
    }

    auto now = std::chrono::system_clock::now();

    // 记录技能冷却
    SkillCooldownRecord record;
    record.caster_id = caster_id;
    record.skill_id = skill_id;
    record.start_time = now;
    record.cooldown_duration = skill_opt->cooldown;

    instance.cooldowns_[caster_id][skill_id] = record;

    // 记录全局冷却
    if (skill_opt->global_cooldown > 0) {
        GlobalCooldownRecord gcd_record;
        gcd_record.caster_id = caster_id;
        gcd_record.start_time = now;
        gcd_record.cooldown_duration = skill_opt->global_cooldown;

        instance.global_cooldowns_[caster_id] = gcd_record;
    }

    spdlog::debug("Skill cooldown started: caster_id={}, skill_id={}, cooldown={}s",
                  caster_id, skill_id, skill_opt->cooldown);
}

void SkillManager::CleanupExpiredCooldowns() {
    auto now = std::chrono::system_clock::now();

    // 清理过期的全局冷却
    for (auto it = global_cooldowns_.begin(); it != global_cooldowns_.end(); ) {
        if (it->second.IsReady()) {
            it = global_cooldowns_.erase(it);
        } else {
            ++it;
        }
    }

    // 清理过期的技能冷却
    for (auto caster_it = cooldowns_.begin(); caster_it != cooldowns_.end(); ) {
        auto& skills_map = caster_it->second;

        for (auto skill_it = skills_map.begin(); skill_it != skills_map.end(); ) {
            if (skill_it->second.IsReady()) {
                skill_it = skills_map.erase(skill_it);
            } else {
                ++skill_it;
            }
        }

        if (skills_map.empty()) {
            caster_it = cooldowns_.erase(caster_it);
        } else {
            ++caster_it;
        }
    }
}

// ========== 范围判定 ==========

bool SkillManager::IsTargetInRange(
    const Position& caster_pos,
    const Position& target_pos,
    const Skill& skill
) {
    float distance = caster_pos.Distance2D(target_pos);

    switch (skill.area_shape) {
        case SkillAreaShape::kNone:
            // 单体技能,只检查距离
            return distance <= skill.range;

        case SkillAreaShape::kCircle:
            // 圆形范围
            return distance <= skill.radius;

        case SkillAreaShape::kSector: {
            // 扇形范围
            if (distance > skill.radius) {
                return false;
            }

            // 计算目标相对于施法者的角度
            float dx = target_pos.x - caster_pos.x;
            float dy = target_pos.y - caster_pos.y;

            // 使用 atan2 计算角度 (返回 -π 到 π)
            float angle_to_target = std::atan2(dy, dx);

            // 将角度转换为度数 (0-360)
            float angle_degrees = static_cast<float>(angle_to_target * 180.0 / M_PI);
            if (angle_degrees < 0) {
                angle_degrees += 360.0f;
            }

            // 假设施法者朝向为 0 度 (正右方)
            // 扇形角度范围: [-skill.angle/2, +skill.angle/2]
            float half_angle = skill.angle / 2.0f;

            // 检查目标是否在扇形角度范围内
            // 考虑周期性 (0度 = 360度)
            bool in_sector = (angle_degrees <= half_angle) ||
                            (angle_degrees >= (360.0f - half_angle));

            return in_sector;
        }

        case SkillAreaShape::kRectangle: {
            // 矩形范围 (假设沿施法者朝向)
            float dx = target_pos.x - caster_pos.x;
            float dy = target_pos.y - caster_pos.y;

            // 矩形参数: 宽度 (skill.radius), 长度 (skill.range)
            float half_width = skill.radius / 2.0f;
            float length = skill.range;

            // 简化: 假设矩形沿 X 轴方向
            // 实际应该根据施法者朝向旋转
            bool in_rectangle = (dx >= 0 && dx <= length) &&
                              (std::abs(dy) <= half_width);

            return in_rectangle;
        }

        case SkillAreaShape::kLine: {
            // 直线范围
            if (distance > skill.range) {
                return false;
            }

            // 计算目标到直线的垂直距离
            float dx = target_pos.x - caster_pos.x;
            float dy = target_pos.y - caster_pos.y;

            // 简化: 假设直线沿 X 轴方向 (施法者朝向)
            // 垂直距离 = |dy|
            float perpendicular_distance = std::abs(dy);

            // 直线宽度 = skill.radius
            return perpendicular_distance <= (skill.radius / 2.0f);
        }

        default:
            return false;
    }
}

std::vector<uint64_t> SkillManager::GetTargetsInRange(
    const Position& caster_pos,
    const Skill& skill,
    const std::vector<uint64_t>& all_targets
) {
    std::vector<uint64_t> targets_in_range;

    for (uint64_t target_id : all_targets) {
        // 获取目标位置
        auto target = CharacterManager::Instance().GetCharacter(target_id);
        if (!target.has_value()) {
            continue;  // 目标不存在,跳过
        }

        Position target_pos;
        target_pos.x = target->x;
        target_pos.y = target->y;
        target_pos.z = target->z;

        if (IsTargetInRange(caster_pos, target_pos, skill)) {
            targets_in_range.push_back(target_id);
        }
    }

    return targets_in_range;
}

// ========== 单例实现 ==========

SkillManager::SkillManager() {
    spdlog::info("SkillManager initialized");
}

} // namespace Game
} // namespace Murim
