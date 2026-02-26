#include "Buff.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>

namespace Murim {
namespace Game {

BuffManager& BuffManager::Instance() {
    static BuffManager instance;
    return instance;
}

// ========== Buff 操作 ==========

uint64_t BuffManager::ApplyBuff(
    uint64_t caster_id,
    uint64_t target_id,
    const Buff& buff
) {
    // 创建 Buff 实例
    Buff new_buff = buff;
    new_buff.buff_id = GenerateBuffId();
    new_buff.caster_id = caster_id;
    new_buff.target_id = target_id;
    new_buff.start_time = std::chrono::system_clock::now();

    // 检查是否已存在同名 Buff (互斥检查)
    auto& target_buffs = buffs_[target_id];
    for (const auto& [id, existing_buff] : target_buffs) {
        if (existing_buff.name == new_buff.name) {
            // 同名 Buff,刷新持续时间并重置过期时间
            spdlog::debug("Refreshed buff: {} on target {}", new_buff.name, target_id);

            // 更新持续时间
            Buff updated_buff = existing_buff;
            updated_buff.start_time = std::chrono::system_clock::now();
            updated_buff.duration = new_buff.duration;

            target_buffs[id] = updated_buff;
            return existing_buff.buff_id;
        }
    }

    // 添加新 Buff
    target_buffs[new_buff.buff_id] = new_buff;

    spdlog::info("Applied buff: {} ({}) to target {}, duration={}s",
                 new_buff.name, new_buff.buff_id, target_id, new_buff.duration);

    return new_buff.buff_id;
}

bool BuffManager::RemoveBuff(
    uint64_t target_id,
    uint64_t buff_id
) {
    auto target_it = buffs_.find(target_id);
    if (target_it == buffs_.end()) {
        return false;
    }

    auto buff_it = target_it->second.find(buff_id);
    if (buff_it == target_it->second.end()) {
        return false;
    }

    spdlog::info("Removed buff: {} ({}) from target {}",
                 buff_it->second.name, buff_id, target_id);

    target_it->second.erase(buff_it);

    // 如果目标没有任何 Buff 了,清理
    if (target_it->second.empty()) {
        buffs_.erase(target_it);
    }

    return true;
}

size_t BuffManager::RemoveAllBuffs(
    uint64_t target_id,
    bool remove_debuff
) {
    auto target_it = buffs_.find(target_id);
    if (target_it == buffs_.end()) {
        return 0;
    }

    size_t removed_count = 0;

    for (auto it = target_it->second.begin(); it != target_it->second.end(); ) {
        if (remove_debuff || it->second.IsBuff()) {
            spdlog::debug("Removed buff: {} from target {}",
                         it->second.name, target_id);
            it = target_it->second.erase(it);
            ++removed_count;
        } else {
            ++it;
        }
    }

    // 清理空容器
    if (target_it->second.empty()) {
        buffs_.erase(target_it);
    }

    spdlog::info("Removed {} buffs from target {}", removed_count, target_id);

    return removed_count;
}

size_t BuffManager::RemoveAllDebuffs(uint64_t target_id) {
    auto target_it = buffs_.find(target_id);
    if (target_it == buffs_.end()) {
        return 0;
    }

    size_t removed_count = 0;

    for (auto it = target_it->second.begin(); it != target_it->second.end(); ) {
        if (it->second.IsDebuff()) {
            spdlog::debug("Removed debuff: {} from target {}",
                         it->second.name, target_id);
            it = target_it->second.erase(it);
            ++removed_count;
        } else {
            ++it;
        }
    }

    if (target_it->second.empty()) {
        buffs_.erase(target_it);
    }

    spdlog::info("Removed {} debuffs from target {}", removed_count, target_id);

    return removed_count;
}

size_t BuffManager::RemoveBuffsByType(
    uint64_t target_id,
    BuffEffectType effect_type
) {
    auto target_it = buffs_.find(target_id);
    if (target_it == buffs_.end()) {
        return 0;
    }

    size_t removed_count = 0;

    for (auto it = target_it->second.begin(); it != target_it->second.end(); ) {
        // 检查是否包含指定效果类型
        bool has_effect = false;
        for (const auto& effect : it->second.effects) {
            if (effect.effect_type == effect_type) {
                has_effect = true;
                break;
            }
        }

        if (has_effect) {
            spdlog::debug("Removed buff with effect type {} from target {}",
                         static_cast<int>(effect_type), target_id);
            it = target_it->second.erase(it);
            ++removed_count;
        } else {
            ++it;
        }
    }

    if (target_it->second.empty()) {
        buffs_.erase(target_it);
    }

    return removed_count;
}

// ========== Buff 查询 ==========

std::vector<Buff> BuffManager::GetBuffs(uint64_t target_id) {
    std::vector<Buff> result;

    auto target_it = buffs_.find(target_id);
    if (target_it == buffs_.end()) {
        return result;
    }

    for (const auto& [id, buff] : target_it->second) {
        if (buff.IsBuff()) {
            result.push_back(buff);
        }
    }

    return result;
}

std::vector<Buff> BuffManager::GetDebuffs(uint64_t target_id) {
    std::vector<Buff> result;

    auto target_it = buffs_.find(target_id);
    if (target_it == buffs_.end()) {
        return result;
    }

    for (const auto& [id, buff] : target_it->second) {
        if (buff.IsDebuff()) {
            result.push_back(buff);
        }
    }

    return result;
}

bool BuffManager::HasBuffType(
    uint64_t target_id,
    BuffEffectType effect_type
) {
    auto buffs = GetBuffs(target_id);

    for (const auto& buff : buffs) {
        for (const auto& effect : buff.effects) {
            if (effect.effect_type == effect_type) {
                return true;
            }
        }
    }

    return false;
}

bool BuffManager::IsCrowdControlled(uint64_t target_id) {
    auto buffs = GetDebuffs(target_id);

    for (const auto& buff : buffs) {
        for (const auto& effect : buff.effects) {
            if (effect.IsCrowdControl() && !buff.IsExpired()) {
                return true;
            }
        }
    }

    return false;
}

// ========== Buff 触发 ==========

std::vector<BuffEffect> BuffManager::UpdateBuffs(
    uint64_t target_id,
    BattleStats& stats
) {
    std::vector<BuffEffect> triggered_effects;

    auto target_it = buffs_.find(target_id);
    if (target_it == buffs_.end()) {
        return triggered_effects;
    }

    // 先清理过期的 Buff
    CleanupExpiredBuffs(target_id);

    // 应用所有 Buff 效果到属性
    stats = ApplyBuffsToStats(target_id, stats);

    // 检查周期触发
    for (auto& [id, buff] : target_it->second) {
        if (buff.ShouldTrigger()) {
            for (const auto& effect : buff.effects) {
                triggered_effects.push_back(effect);
            }
        }
    }

    return triggered_effects;
}

size_t BuffManager::CleanupExpiredBuffs(uint64_t target_id) {
    auto target_it = buffs_.find(target_id);
    if (target_it == buffs_.end()) {
        return 0;
    }

    size_t cleaned_count = 0;

    for (auto it = target_it->second.begin(); it != target_it->second.end(); ) {
        if (it->second.IsExpired()) {
            spdlog::debug("Expired buff: {} ({}) removed from target {}",
                         it->second.name, it->first, target_id);
            it = target_it->second.erase(it);
            ++cleaned_count;
        } else {
            ++it;
        }
    }

    if (target_it->second.empty()) {
        buffs_.erase(target_it);
    }

    return cleaned_count;
}

// ========== 属性计算 ==========

BattleStats BuffManager::ApplyBuffsToStats(
    uint64_t target_id,
    const BattleStats& base_stats
) {
    BattleStats stats = base_stats;

    auto target_it = buffs_.find(target_id);
    if (target_it == buffs_.end()) {
        return stats;
    }

    // 应用所有 Buff 效果
    for (const auto& [id, buff] : target_it->second) {
        if (buff.IsExpired()) {
            continue;
        }

        for (const auto& effect : buff.effects) {
            ApplyBuffEffect(effect, stats);
        }
    }

    return stats;
}

void BuffManager::ApplyBuffEffect(
    const BuffEffect& effect,
    BattleStats& stats
) {
    switch (effect.effect_type) {
        // 固定值增益
        case BuffEffectType::kStrengthBoost:
            stats.strength = static_cast<uint16_t>(stats.strength + effect.value);
            break;

        case BuffEffectType::kIntelligenceBoost:
            stats.intelligence = static_cast<uint16_t>(stats.intelligence + effect.value);
            break;

        case BuffEffectType::kDexterityBoost:
            stats.dexterity = static_cast<uint16_t>(stats.dexterity + effect.value);
            break;

        case BuffEffectType::kVitalityBoost:
            stats.vitality = static_cast<uint16_t>(stats.vitality + effect.value);
            break;

        case BuffEffectType::kWisdomBoost:
            stats.wisdom = static_cast<uint16_t>(stats.wisdom + effect.value);
            break;

        // 战斗属性固定值增益
        case BuffEffectType::kPhysicalAttackBoost:
            stats.physical_attack += effect.value;
            break;

        case BuffEffectType::kPhysicalDefenseBoost:
            stats.physical_defense += effect.value;
            break;

        case BuffEffectType::kMagicAttackBoost:
            stats.magic_attack += effect.value;
            break;

        case BuffEffectType::kMagicDefenseBoost:
            stats.magic_defense += effect.value;
            break;

        case BuffEffectType::kAttackSpeedBoost:
            stats.attack_speed = static_cast<uint16_t>(stats.attack_speed + effect.value);
            break;

        case BuffEffectType::kMoveSpeedBoost:
            stats.move_speed = static_cast<uint16_t>(stats.move_speed + effect.value);
            break;

        case BuffEffectType::kCriticalRateBoost:
            stats.critical_rate = static_cast<uint16_t>(stats.critical_rate + effect.value);
            break;

        case BuffEffectType::kHitRateBoost:
            stats.hit_rate = static_cast<uint16_t>(stats.hit_rate + effect.value);
            break;

        case BuffEffectType::kDodgeRateBoost:
            stats.dodge_rate = static_cast<uint16_t>(stats.dodge_rate + effect.value);
            break;

        // 百分比增益
        case BuffEffectType::kPhysicalAttackPercent:
            stats.physical_attack += static_cast<uint32_t>(stats.physical_attack * effect.percent);
            break;

        case BuffEffectType::kPhysicalDefensePercent:
            stats.physical_defense += static_cast<uint32_t>(stats.physical_defense * effect.percent);
            break;

        case BuffEffectType::kMagicAttackPercent:
            stats.magic_attack += static_cast<uint32_t>(stats.magic_attack * effect.percent);
            break;

        case BuffEffectType::kMagicDefensePercent:
            stats.magic_defense += static_cast<uint32_t>(stats.magic_defense * effect.percent);
            break;

        // Debuff (固定值降低)
        case BuffEffectType::kStrengthDebuff:
            stats.strength = std::max(0, static_cast<int>(stats.strength) - effect.value);
            break;

        case BuffEffectType::kPhysicalAttackDebuff:
            stats.physical_attack = std::max(0u, stats.physical_attack - effect.value);
            break;

        case BuffEffectType::kPhysicalDefenseDebuff:
            stats.physical_defense = std::max(0u, stats.physical_defense - effect.value);
            break;

        case BuffEffectType::kAttackSpeedDebuff:
            stats.attack_speed = std::max(0, static_cast<int>(stats.attack_speed) - effect.value);
            break;

        case BuffEffectType::kMoveSpeedDebuff:
            stats.move_speed = std::max(0, static_cast<int>(stats.move_speed) - effect.value);
            break;

        default:
            // 其他效果类型 (持续伤害、控制效果等) 不直接修改属性
            break;
    }
}

uint64_t BuffManager::GenerateBuffId() {
    return next_buff_id_++;
}

} // namespace Game
} // namespace Murim
