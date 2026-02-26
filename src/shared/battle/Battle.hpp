#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <chrono>

namespace Murim {
namespace Game {

// 前向声明
struct Skill;

/**
 * @brief 攻击类型
 */
enum class AttackType : uint8_t {
    kPhysical = 1,      // 物理攻击
    kMugong = 2,         // 武功攻击
    kMagic = 3,          // 魔法攻击
    kPoison = 4,         // 毒素攻击
    kAll = 5              // 全类型
};

/**
 * @brief 伤害类型
 */
enum class DamageType : uint8_t {
    kPhysical = 1,       // 物理伤害
    kMagic = 2,          // 魔法伤害
    kTrue = 3,           // 真实伤害 (无视防御)
    kReflect = 4         // 反射伤害
};

/**
 * @brief 属性类型 (元素)
 */
enum class AttributeType : uint8_t {
    kNone = 0,
    kFire = 1,           // 火
    kIce = 2,            // 冰
    kLightning = 3,      // 雷
    kPoison = 4,         // 毒
    kHoly = 5            // 圣
};

/**
 * @brief 战斗属性 (对应 legacy 的各种战斗属性)
 */
struct BattleStats {
    // ========== 基础属性 ==========
    uint16_t level = 1;               // 等级 (AttackCalculator需要)
    uint16_t strength = 10;           // 力量
    uint16_t intelligence = 10;       // 智力
    uint16_t dexterity = 10;          // 敏捷
    uint16_t vitality = 10;           // 体力
    uint16_t wisdom = 10;             // 智慧

    // ========== 物理攻击 (AttackCalculator需要) ==========
    uint32_t physical_attack_min = 0; // 物理攻击最小值
    uint32_t physical_attack_max = 0; // 物理攻击最大值
    uint32_t physical_attack = 0;     // 物理攻击力 (平均值,兼容旧代码)
    uint32_t physical_defense = 0;    // 物理防御力

    // ========== 属性攻击 (AttackCalculator需要) ==========
    uint16_t simmek = 0;              // 内功值 (对应legacy SimMek)
    uint32_t attrib_attack_min = 0;    // 属性攻击最小值
    uint32_t attrib_attack_max = 0;    // 属性攻击最大值
    uint32_t attrib_plus_percent = 0;  // 属性加成百分比

    // ========== 技能属性 (AttackCalculator需要) ==========
    float skill_base_atk = 0.0f;      // 技能基础攻击加成
    float skill_phy_def = 0.0f;       // 技能物理防御加成

    // ========== 防御属性 (AttackCalculator需要) ==========
    float regist_phys = 0.0f;         // 物理抵抗 (对应legacy RegistPhys)
    float enemy_defen = 0.0f;         // 敌人防御修正 (对应legacy EnemyDefen)
    float party_defence_rate = 1.0f;  // 队伍防御率 (对应legacy PartyDefenceRate)

    // ========== 魔法攻击 ==========
    uint32_t magic_attack = 0;        // 魔法攻击力
    uint32_t magic_defense = 0;       // 魔法防御力

    // ========== 其他属性 ==========
    uint16_t attack_speed = 100;      // 攻击速度
    uint16_t move_speed = 100;        // 移动速度
    uint16_t critical_rate = 5;       // 暴击率 (%)
    int16_t unique_item_cri_rate = 0; // 独特物品暴击率修正 (AttackCalculator需要)
    uint16_t dodge_rate = 5;          // 闪避率 (%)
    uint16_t hit_rate = 90;           // 命中率 (%)
};

/**
 * @brief 伤害计算结果
 */
struct DamageResult {
    uint64_t attacker_id = 0;       // 攻击者ID
    uint64_t target_id = 0;         // 目标ID
    AttackType attack_type = AttackType::kPhysical;  // 攻击类型
    DamageType damage_type = DamageType::kPhysical;    // 伤害类型

    // 伤害值
    uint32_t damage = 0;            // 最终伤害
    uint32_t raw_damage = 0;        // 原始伤害
    uint32_t defense = 0;           // 防御力

    // 暴击相关
    bool is_critical = false;        // 是否暴击
    uint32_t critical_damage = 0;    // 暴击伤害

    // 命中相关
    bool is_hit = true;              // 是否命中
    bool is_dodged = false;          // 是否闪避

    // 元素攻击
    AttributeType attribute = AttributeType::kNone;
    uint32_t attribute_damage = 0;  // 元素伤害

    /**
     * @brief 是否为有效伤害
     */
    bool IsValidDamage() const {
        return is_hit && damage > 0;
    }
};

/**
 * @brief 战斗配置
 */
struct BattleConfig {
    // 暴击倍率
    float critical_multiplier = 2.0f;     // 暴击伤害倍数
    float critical_rate_bonus = 0.0f;     // 额外暴击率

    // 命中/闪避
    float hit_rate_base = 0.9f;           // 基础命中率
    float dodge_rate_base = 0.05f;        // 基础闪避率

    // 伤害计算系数
    float attack_power_coefficient = 1.0f; // 攻击力系数
    float defense_reduction_rate = 0.5f;   // 防御减伤率

    // 等级差影响
    float level_diff_penalty = 0.1f;       // 等级差惩罚

    // 最小伤害
    uint32_t min_damage = 1;               // 最小伤害
};

// Skill结构定义在Skill.hpp中以避免重复定义

/**
 * @brief 战斗管理器
 *
 * 负责伤害计算和战斗逻辑
 * 对应 legacy: AttackCalc.cpp
 */
class BattleManager {
public:
    /**
     * @brief 获取单例实例
     */
    static BattleManager& Instance();

private:
    /**
     * @brief 私有构造函数 (单例模式)
     */
    BattleManager() = default;
    ~BattleManager() = default;
    BattleManager(const BattleManager&) = delete;
    BattleManager& operator=(const BattleManager&) = delete;

    /**
     * @brief 默认战斗配置
     */
    static BattleConfig default_config_;
    static bool config_initialized_;

public:

    // ========== 伤害计算 ==========

    /**
     * @brief 计算物理攻击伤害
     * @param attacker_stats 攻击者属性
     * @param defender_stats 防御者属性
     * @param config 战斗配置
     * @return 伤害结果
     */
    static DamageResult CalculatePhysicalDamage(
        const BattleStats& attacker_stats,
        const BattleStats& defender_stats,
        const BattleConfig& config = GetDefaultConfig()
    );

    /**
     * @brief 计算武功攻击伤害
     * @param attacker_stats 攻击者属性
     * @param defender_stats 防御者属性
     * @param skill_id 武功ID
     * @param config 战斗配置
     * @return 伤害结果
     */
    static DamageResult CalculateMugongDamage(
        const BattleStats& attacker_stats,
        const BattleStats& defender_stats,
        uint32_t skill_id,
        const BattleConfig& config = GetDefaultConfig()
    );

    /**
     * @brief 计算魔法攻击伤害
     * @param attacker_stats 攻击者属性
     * @param defender_stats 防御者属性
     * @param skill_id 魔法ID
     * @param config 战斗配置
     * @return 伤害结果
     */
    static DamageResult CalculateMagicDamage(
        const BattleStats& attacker_stats,
        const BattleStats& defender_stats,
        uint32_t skill_id,
        const BattleConfig& config = GetDefaultConfig()
    );

    // ========== 命中判定 ==========

    /**
     * @brief 判断是否命中
     * @param attacker_stats 攻击者属性
     * @param defender_stats 防御者属性
     * @param config 战斗配置
     * @return 是否命中
     */
    static bool CheckHit(
        const BattleStats& attacker_stats,
        const BattleStats& defender_stats,
        const BattleConfig& config = GetDefaultConfig()
    );

    /**
     * @brief 判断是否暴击
     * @param attacker_stats 攻击者属性
     * @param defender_stats 防御者属性
     * @param config 战斗配置
     * @return 是否暴击
     */
    static bool CheckCritical(
        const BattleStats& attacker_stats,
        const BattleStats& defender_stats,
        const BattleConfig& config = GetDefaultConfig()
    );

    /**
     * @brief 判断是否闪避
     * @param attacker_stats 攻击者属性
     * @param defender_stats 防御者属性
     * @param config 战斗配置
     * @return 是否闪避
     */
    static bool CheckDodge(
        const BattleStats& attacker_stats,
        const BattleStats& defender_stats,
        const BattleConfig& config = GetDefaultConfig()
    );

    // ========== 防御计算 ==========

    /**
     * @brief 计算防御力
     * @param defender_stats 防御者属性
     * @param attack_type 攻击类型
     * @return 防御力
     */
    static uint32_t CalculateDefense(
        const BattleStats& defender_stats,
        AttackType attack_type
    );

    /**
     * @brief 计算最终伤害
     * @param raw_damage 原始伤害
     * @param defense 防御力
     * @param level_difference 等级差
     * @param config 战斗配置
     * @return 最终伤害
     */
    static uint32_t CalculateFinalDamage(
        uint32_t raw_damage,
        uint32_t defense,
        int16_t level_difference,
        const BattleConfig& config
    );

    // ========== 配置管理 ==========

    /**
     * @brief 获取默认战斗配置
     */
    static const BattleConfig& GetDefaultConfig();

    /**
     * @brief 设置战斗配置
     */
    static void SetBattleConfig(const BattleConfig& config);

    // ========== 辅助计算 ==========

    /**
     * @brief 计算经验值奖励
     * @param player_level 玩家等级
     * @param monster_level 怪物等级
     * @param monster_exp 怪物基础经验值
     * @return 经验值奖励
     */
    static uint32_t CalculateExpReward(
        uint16_t player_level,
        uint16_t monster_level,
        uint32_t monster_exp
    );

    /**
     * @brief 检查是否致命一击
     * @param attacker_stats 攻击者属性
     * @param defender_stats 防御者属性
     * @param config 战斗配置
     * @return 是否致命一击
     */
    static bool CheckDecisive(
        const BattleStats& attacker_stats,
        const BattleStats& defender_stats,
        const BattleConfig& config = GetDefaultConfig()
    );

    /**
     * @brief 计算技能伤害
     * @param skill 技能对象
     * @param attacker_stats 攻击者属性
     * @param defender_stats 防御者属性
     * @return 伤害值
     */
    static uint32_t CalculateSkillDamage(
        const Skill& skill,
        const BattleStats& attacker_stats,
        const BattleStats& defender_stats
    );

    /**
     * @brief 计算等级差惩罚
     * @param attacker_level 攻击者等级
     * @param defender_level 防御者等级
     * @return 惩罚系数 (0.0 ~ 1.0)
     */
    static float CalculateLevelPenalty(
        uint16_t attacker_level,
        uint16_t defender_level
    );

    /**
     * @brief 计算元素伤害
     * @param base_damage 基础伤害
     * @param attribute 元素类型
     * @param attacker_stats 攻击者属性
     * @param defender_stats 防御者属性
     * @return 元素伤害加成
     */
    static uint32_t CalculateAttributeDamage(
        uint32_t base_damage,
        AttributeType attribute,
        const BattleStats& attacker_stats,
        const BattleStats& defender_stats
    );
};

/**
 * @brief 战斗事件
 */
struct BattleEvent {
    uint64_t event_id;              // 事件ID
    uint64_t attacker_id;           // 攻击者ID
    uint64_t target_id;             // 目标ID
    AttackType attack_type;         // 攻击类型
    uint32_t damage;                // 伤害
    bool is_critical;               // 是否暴击
    std::chrono::system_clock::time_point battle_time;  // 战斗时间

    /**
     * @brief 转换为日志格式
     */
    std::string ToLogString() const;
};

} // namespace Game
} // namespace Murim
