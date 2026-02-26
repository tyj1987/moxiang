#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdio>  // For snprintf
#include "Battle.hpp"  // For BattleStats

namespace Murim {
namespace Game {

/**
 * @brief 特殊状态类型枚举
 *
 * 对应: SPECIAL_STATE_INFO的StateType
 */
enum class SpecialStateType : uint8_t {
    kNone = 0,

    // ========== 控制效果 ==========
    kStun = 1,          // 眩晕 - 无法移动和攻击
    kFreeze,             // 冰冻 - 无法移动和攻击
    kSleep,             // 睡眠 - 受伤唤醒
    kSilence,            // 沉默 - 无法使用技能
    kDisarm,             // 缴械 - 无法普通攻击
    kRoot,               // 定身 - 无法移动
    kTaunt,              // 嘲讽 - 强制攻击特定目标
    kConfusion,          // 混乱 - 移动方向反转
    kFear,               // 恐惧 - 自动远离目标

    // ========== 移动效果 ==========
    kSlow = 11,          // 减速 - 移动速度降低
    kHaste,              // 加速 - 移动速度提升
    kKnockback,          // 击退 - 被击退
    kPull,               // 拉拽 - 被拉向目标
    kPush,               // 推开 - 被推离目标
    kTeleport,           // 传送 - 瞬移到目标位置

    // ========== 持续伤害/治疗 ==========
    kPoison = 21,         // 中毒 - 持续伤害
    kBurn,               // 燃烧 - 持续伤害
    kBleed,              // 流血 - 持续伤害
    kRegeneration,       // 再生 - 持续治疗
    kManaRegeneration,   // 法力恢复 - 持续恢复MP

    // ========== 增益/减益 ==========
    kInvincible = 31,     // 无敌 - 不受伤害
    kShield,             // 护盾 - 吸收伤害
    kReflect,            // 反射 - 反射伤害
    kImmunity,           // 免疫 - 免疫特定效果
    kInvisibility,       // 隐身 - 不可见

    // ========== 特殊 ==========
    kPossessed = 41,      // 附身 - 被怪物控制
    kCharmed,             // 魅惑 - 友好单位变敌人
    kPolymorph,           // 变形 - 变成其他生物
    kClone,              // 分身 - 创建镜像

    kMax
};

/**
 * @brief 特殊状态数据
 *
 * 对应: SPECIAL_STATE_INFO
 */
struct SpecialStateInfo {
    uint32_t state_id;              // 状态ID
    SpecialStateType state_type;   // 状态类型
    std::string name;               // 状态名称
    std::string description;        // 状态描述
    uint32_t icon_id;               // 图标ID

    // 持续时间
    float duration;                // 持续时间（秒）
    float time_remaining;          // 剩余时间
    bool is_permanent;             // 是否永久

    // 强度
    uint8_t level;                  // 强度等级 (1-10)
    float value1;                   // 参数1 (根据类型不同含义)
    float value2;                   // 参数2
    float value3;                   // 参数3

    // 行为标志
    bool dispellable;               // 是否可驱散
    bool stun_on_damage;           // 受伤是否晕眩
    bool break_on_move;             // 移动是否解除

    // 来源
    uint64_t caster_id;             // 施放者ID
    uint32_t skill_id;               // 技能ID
    uint32_t item_id;                // 物品ID

    // 周期效果
    float tick_interval;           // Tick间隔 (秒)
    float tick_value;               // 每次Tick的效果值
    float time_since_last_tick;     // 距上次Tick的时间

    // 粒附效果 (附加状态)
    std::vector<uint32_t> attached_states;  // 附加的其他状态ID

    // 回调函数
    std::function<void(uint64_t, uint32_t)> on_apply;    // 应用时回调
    std::function<void(uint64_t, uint32_t)> on_remove;   // 移除时回调
    std::function<void(uint64_t, uint32_t, float)> on_tick; // Tick回调

    SpecialStateInfo()
        : state_id(0)
        , state_type(SpecialStateType::kNone)
        , duration(0.0f)
        , time_remaining(0.0f)
        , is_permanent(false)
        , level(1)
        , value1(0.0f)
        , value2(0.0f)
        , value3(0.0f)
        , dispellable(true)
        , stun_on_damage(false)
        , break_on_move(false)
        , caster_id(0)
        , skill_id(0)
        , item_id(0)
        , tick_interval(1.0f)
        , tick_value(0.0f)
        , time_since_last_tick(0.0f)
    {
    }

    /**
     * @brief 是否已过期
     */
    bool IsExpired() const {
        return !is_permanent && time_remaining <= 0.0f;
    }

    /**
     * @brief 获取剩余时间百分比
     */
    float GetRemainingPercent() const {
        if (duration <= 0.0f) return 0.0f;
        float ratio = time_remaining / duration;
        return (ratio < 0.0f) ? 0.0f : (ratio > 1.0f) ? 1.0f : ratio;
    }

    /**
     * @brief 更新状态
     * @param delta_time 增量时间
     * @return true if 状态过期
     */
    bool Update(float delta_time) {
        if (is_permanent) {
            return false;
        }

        time_remaining -= delta_time;

        // 更新周期效果
        if (tick_interval > 0.0f) {
            time_since_last_tick += delta_time;
            if (time_since_last_tick >= tick_interval) {
                time_since_last_tick -= tick_interval;
                // TODO: 应用Tick效果
            }
        }

        return IsExpired();
    }

    /**
     * @brief 获取描述
     */
    std::string GetDescription() const {
        if (!description.empty()) {
            return description;
        }

        // 自动生成描述
        char buffer[256];
        switch (state_type) {
        case SpecialStateType::kStun:
            snprintf(buffer, sizeof(buffer), "眩晕 (持续%.1f秒)", time_remaining);
            return std::string(buffer);
        case SpecialStateType::kSlow:
            snprintf(buffer, sizeof(buffer), "减速 %d%% (持续%.1f秒)",
                     static_cast<int32_t>(value1), time_remaining);
            return std::string(buffer);
        case SpecialStateType::kPoison:
            snprintf(buffer, sizeof(buffer), "中毒 (每秒%.0f伤害, 持续%.1f秒)",
                     tick_value, time_remaining);
            return std::string(buffer);
        default:
            return description;
        }
    }
};

/**
 * @brief Buff选项数据
 *
 * 对应: SKILLOPTION
 */
struct SkillOption {
    uint32_t option_id;              // 选项ID
    std::string name;               // 选项名称

    // 加成类型
    enum class Type : uint8_t {
        kDamageAmp,         // 伤害加成
        kDefenseAmp,        // 防御加成
        kCooldownReduction, // 冷却缩减
        kManaCostReduction, // 蓝耗缩减
        kRangeBonus,        // 范围增加
        kDurationBoost,    // 持续时间增加
        kCriticalChance,    // 暴击率提升
        kDodgeChance,      // 闪避率提升
        kHitChance         // 命中率提升
    } type;

    float value;                   // 加成值 (百分比或固定值)
    bool is_percent;             // 是否百分比

    SkillOption()
        : option_id(0)
        , type(Type::kDamageAmp)
        , value(0.0f)
        , is_percent(true)
    {
    }
};


} // namespace Game
} // namespace Murim
