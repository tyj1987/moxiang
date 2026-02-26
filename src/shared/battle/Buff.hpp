#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include "Battle.hpp"

namespace Murim {
namespace Game {

/**
 * @brief Buff/Debuff 效果类型
 */
enum class BuffEffectType : uint8_t {
    kNone = 0,

    // 属性增益
    kStrengthBoost = 1,         // 力量提升
    kIntelligenceBoost = 2,     // 智力提升
    kDexterityBoost = 3,        // 敏捷提升
    kVitalityBoost = 4,         // 体力提升
    kWisdomBoost = 5,           // 智慧提升

    // 战斗属性增益
    kPhysicalAttackBoost = 10,  // 物理攻击提升
    kPhysicalDefenseBoost = 11, // 物理防御提升
    kMagicAttackBoost = 12,     // 魔法攻击提升
    kMagicDefenseBoost = 13,    // 魔法防御提升
    kAttackSpeedBoost = 14,     // 攻击速度提升
    kMoveSpeedBoost = 15,       // 移动速度提升
    kCriticalRateBoost = 16,    // 暴击率提升
    kHitRateBoost = 17,         // 命中率提升
    kDodgeRateBoost = 18,       // 闪避率提升

    // 百分比增益
    kPhysicalAttackPercent = 20,    // 物理攻击百分比提升
    kPhysicalDefensePercent = 21,   // 物理防御百分比提升
    kMagicAttackPercent = 22,       // 魔法攻击百分比提升
    kMagicDefensePercent = 23,      // 魔法防御百分比提升

    // 负面效果 (Debuff)
    kStrengthDebuff = 30,        // 力量降低
    kPhysicalAttackDebuff = 31,  // 物理攻击降低
    kPhysicalDefenseDebuff = 32, // 物理防御降低
    kAttackSpeedDebuff = 33,     // 攻击速度降低
    kMoveSpeedDebuff = 34,       // 移动速度降低

    // 状态效果
    kPoison = 50,                // 中毒 (持续伤害)
    kBurn = 51,                  // 燃烧 (持续伤害)
    kFreeze = 52,                // 冰冻 (无法移动)
    kStun = 53,                  // 眩晕 (无法行动)
    kSilence = 54,               // 沉默 (无法施法)
    kSleep = 55,                 // 睡眠 (无法行动,被打醒)
    kParalyze = 56,              // 麻痹 (无法移动)
    kSlow = 57,                  // 减速
    kBind = 58,                  // 定身

    // 治疗效果
    kRegeneration = 60,          // 生命恢复
    kManaRegeneration = 61,      // 内力恢复

    // 特殊效果
    kInvincible = 70,            // 无敌
    kShield = 71,                // 护盾 (吸收伤害)
    kReflect = 72,               // 反射伤害
    kImmunity = 73               // 免疫控制
};

/**
 * @brief Buff/Debuff 效果数据
 */
struct BuffEffect {
    BuffEffectType effect_type;  // 效果类型
    int32_t value;               // 效果值 (固定值)
    float percent;               // 效果百分比 (0.0 ~ 1.0)

    /**
     * @brief 是否是正面效果 (Buff)
     */
    bool IsBuff() const {
        return static_cast<uint8_t>(effect_type) < 30;
    }

    /**
     * @brief 是否是负面效果 (Debuff)
     */
    bool IsDebuff() const {
        return static_cast<uint8_t>(effect_type) >= 30 &&
               static_cast<uint8_t>(effect_type) < 70;
    }

    /**
     * @brief 是否是控制效果
     */
    bool IsCrowdControl() const {
        uint8_t type = static_cast<uint8_t>(effect_type);
        return type >= 50 && type <= 58;
    }
};

/**
 * @brief Buff/Debuff 实例
 */
struct Buff {
    uint64_t buff_id;             // Buff ID
    uint64_t caster_id;           // 施法者ID
    uint64_t target_id;           // 目标ID
    uint32_t skill_id;            // 来源技能ID

    std::string name;             // Buff 名称
    std::string description;      // 描述

    std::vector<BuffEffect> effects;  // 效果列表

    std::chrono::system_clock::time_point start_time;  // 开始时间
    uint32_t duration;            // 持续时间(秒)
    uint32_t interval;            // 间隔时间(秒) (0表示一次性)

    bool is_purgeable;            // 是否可净化
    bool is_dispellable;          // 是否可驱散

    /**
     * @brief 获取剩余时间(秒)
     */
    uint32_t GetRemainingTime() const {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        if (elapsed >= duration) {
            return 0;
        }
        return static_cast<uint32_t>(duration - elapsed);
    }

    /**
     * @brief 是否已过期
     */
    bool IsExpired() const {
        return GetRemainingTime() == 0;
    }

    /**
     * @brief 是否需要周期触发
     */
    bool IsPeriodic() const {
        return interval > 0;
    }

    /**
     * @brief 检查是否到了触发时间
     */
    bool ShouldTrigger() const {
        if (!IsPeriodic()) {
            return false;
        }

        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        return (elapsed % interval == 0) && (elapsed < duration);
    }

    /**
     * @brief 是否是 Buff
     */
    bool IsBuff() const {
        if (effects.empty()) {
            return true;  // 默认为 Buff
        }
        return effects[0].IsBuff();
    }

    /**
     * @brief 是否是 Debuff
     */
    bool IsDebuff() const {
        if (effects.empty()) {
            return false;
        }
        return effects[0].IsDebuff();
    }
};

/**
 * @brief Buff 管理器
 *
 * 负责 Buff/Debuff 的施加、移除、触发
 */
class BuffManager {
public:
    /**
     * @brief 获取单例实例
     */
    static BuffManager& Instance();

    // ========== Buff 操作 ==========

    /**
     * @brief 施加 Buff
     * @param caster_id 施法者ID
     * @param target_id 目标ID
     * @param buff Buff 数据
     * @return Buff ID
     */
    uint64_t ApplyBuff(
        uint64_t caster_id,
        uint64_t target_id,
        const Buff& buff
    );

    /**
     * @brief 移除 Buff
     * @param target_id 目标ID
     * @param buff_id Buff ID
     * @return 是否成功
     */
    bool RemoveBuff(
        uint64_t target_id,
        uint64_t buff_id
    );

    /**
     * @brief 移除所有 Buff
     * @param target_id 目标ID
     * @param remove_debuff 是否同时移除 Debuff
     * @return 移除的数量
     */
    size_t RemoveAllBuffs(
        uint64_t target_id,
        bool remove_debuff = false
    );

    /**
     * @brief 移除所有 Debuff
     * @param target_id 目标ID
     * @return 移除的数量
     */
    size_t RemoveAllDebuffs(uint64_t target_id);

    /**
     * @brief 根据 Buff 类型移除
     * @param target_id 目标ID
     * @param effect_type 效果类型
     * @return 移除的数量
     */
    size_t RemoveBuffsByType(
        uint64_t target_id,
        BuffEffectType effect_type
    );

    // ========== Buff 查询 ==========

    /**
     * @brief 获取目标的所有 Buff
     * @param target_id 目标ID
     * @return Buff 列表
     */
    std::vector<Buff> GetBuffs(uint64_t target_id);

    /**
     * @brief 获取目标的所有 Debuff
     * @param target_id 目标ID
     * @return Debuff 列表
     */
    std::vector<Buff> GetDebuffs(uint64_t target_id);

    /**
     * @brief 检查是否有特定类型的 Buff
     * @param target_id 目标ID
     * @param effect_type 效果类型
     * @return 是否存在
     */
    bool HasBuffType(
        uint64_t target_id,
        BuffEffectType effect_type
    );

    /**
     * @brief 检查是否有控制效果
     * @param target_id 目标ID
     * @return 是否被控制
     */
    bool IsCrowdControlled(uint64_t target_id);

    // ========== Buff 触发 ==========

    /**
     * @brief 更新所有 Buff (每帧调用)
     * @param target_id 目标ID
     * @param stats 角色属性 (应用 Buff 后的属性)
     * @return 触发的周期效果列表
     */
    std::vector<BuffEffect> UpdateBuffs(
        uint64_t target_id,
        BattleStats& stats
    );

    /**
     * @brief 清理过期的 Buff
     * @param target_id 目标ID
     * @return 清理的数量
     */
    size_t CleanupExpiredBuffs(uint64_t target_id);

    // ========== 属性计算 ==========

    /**
     * @brief 应用所有 Buff 到角色属性
     * @param target_id 目标ID
     * @param base_stats 基础属性
     * @return 应用 Buff 后的属性
     */
    BattleStats ApplyBuffsToStats(
        uint64_t target_id,
        const BattleStats& base_stats
    );

private:
    BuffManager() = default;
    ~BuffManager() = default;
    BuffManager(const BuffManager&) = delete;
    BuffManager& operator=(const BuffManager&) = delete;

    // Buff 存储: target_id -> (buff_id -> Buff)
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, Buff>> buffs_;

    // Buff ID 生成器
    uint64_t next_buff_id_ = 1;

    /**
     * @brief 应用单个 Buff 效果到属性
     */
    void ApplyBuffEffect(
        const BuffEffect& effect,
        BattleStats& stats
    );

    /**
     * @brief 生成唯一 Buff ID
     */
    uint64_t GenerateBuffId();
};

} // namespace Game
} // namespace Murim
