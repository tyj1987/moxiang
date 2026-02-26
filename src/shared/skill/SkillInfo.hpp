// MurimServer - Skill Info
// 技能信息 - 定义技能的所有属性和效果
// 对应legacy: SKILL_INFO及相关结构

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class GameObject;

// ========== Skill Type ==========
// 技能类型
enum class SkillType : uint8_t {
    Passive = 0,        // 被动技能
    Active = 1,          // 主动技能
    Toggle = 2,          // 开关技能
    Channel = 3          // 引导技能
};

// ========== Skill Target Type ==========
// 技能目标类型
enum class SkillTargetType : uint8_t {
    None = 0,            // 无目标(自身)
    Self = 1,            // 自身
    Single = 2,          // 单体目标
    Area = 3,            // 范围目标
    Direction = 4,       // 方向
    Position = 5,        // 指定位置
    Friend = 6,          // 友方
    Enemy = 7,           // 敌方
    Party = 8,           // 队伍
    Guild = 9            // 公会
};

// ========== Skill Element ==========
// 技能元素
enum class SkillElement : uint8_t {
    None = 0,            // 无属性
    Fire = 1,            // 火
    Ice = 2,             // 冰
    Lightning = 3,       // 雷
    Earth = 4,           // 土
    Wind = 5,            // 风
    Light = 6,           // 光
    Dark = 7,            // 暗
    Physical = 8,        // 物理
    Magic = 9            // 魔法
};

// ========== Damage Type ==========
// 伤害类型
enum class SkillDamageType : uint8_t {
    Physical = 0,        // 物理伤害
    Magic = 1,           // 魔法伤害
    True = 2,            // 真实伤害
    Hybrid = 3           // 混合伤害
};

// ========== Skill Level Info ==========
// 技能等级信息
struct SkillLevelInfo {
    uint16_t level;               // 等级
    uint32_t require_player_level;// 需求玩家等级
    uint32_t mp_cost;             // MP消耗
    uint32_t hp_cost;             // HP消耗
    uint32_t cooldown_ms;         // 冷却时间(毫秒)
    uint32_t cast_time_ms;        // 施法时间(毫秒)
    uint32_t channel_time_ms;     // 引导时间(毫秒)

    float damage_rate;            // 伤害倍率(攻击力百分比)
    float heal_rate;              // 治疗倍率
    uint32_t base_damage;         // 基础伤害值
    uint32_t base_heal;           // 基础治疗值

    float duration_sec;           // 持续时间(秒)
    float range;                  // 范围(米)

    uint32_t skill_id_next_level; // 下一等级技能ID(用于学习)

    SkillLevelInfo()
        : level(1),
          require_player_level(1),
          mp_cost(0),
          hp_cost(0),
          cooldown_ms(0),
          cast_time_ms(0),
          channel_time_ms(0),
          damage_rate(1.0f),
          heal_rate(1.0f),
          base_damage(0),
          base_heal(0),
          duration_sec(0.0f),
          range(5.0f),
          skill_id_next_level(0) {}
};

// ========== Skill Info ==========
// 技能信息
class SkillInfo {
private:
    // 基础信息
    uint32_t skill_id_;             // 技能ID
    std::string skill_name_;        // 技能名称
    std::string skill_description_; // 技能描述
    SkillType skill_type_;          // 技能类型
    SkillElement element_;          // 技能元素
    SkillDamageType damage_type_;   // 伤害类型

    // 目标信息
    SkillTargetType target_type_;   // 目标类型
    float cast_range_;              // 施法范围
    float effect_range_;            // 效果范围

    // 图标
    uint32_t icon_id_;              // 图标ID

    // 学习要求
    uint16_t max_level_;            // 最大等级
    uint32_t require_level_;        // 需求等级
    uint32_t require_skill_id_;     // 前置技能ID
    uint16_t require_skill_level_;  // 前置技能等级
    uint32_t require_quest_id_;     // 前置任务ID
    uint64_t gold_cost_learn;       // 学习金币消耗
    uint64_t exp_cost_learn;        // 学习经验消耗

    // 范围信息
    uint32_t skill_area_id_;         // 技能范围ID
    uint8_t max_target_count_;      // 最大目标数量

    // 效果信息
    uint32_t state_id_self_;        // 施法者状态ID
    uint32_t state_id_target_;      // 目标状态ID
    uint32_t effect_id_;            // 效果ID

    // 等级信息
    std::map<uint16_t, SkillLevelInfo> level_info_;

public:
    SkillInfo();
    virtual ~SkillInfo();

    // ========== 初始化 ==========
    bool Init();
    void Release();

    // ========== 基础信息访问 ==========
    uint32_t GetSkillID() const { return skill_id_; }
    void SetSkillID(uint32_t id) { skill_id_ = id; }

    const std::string& GetSkillName() const { return skill_name_; }
    void SetSkillName(const std::string& name) { skill_name_ = name; }

    const std::string& GetDescription() const { return skill_description_; }
    void SetDescription(const std::string& desc) { skill_description_ = desc; }

    SkillType GetSkillType() const { return skill_type_; }
    void SetSkillType(SkillType type) { skill_type_ = type; }

    SkillElement GetElement() const { return element_; }
    void SetElement(SkillElement elem) { element_ = elem; }

    SkillDamageType GetDamageType() const { return damage_type_; }
    void SetDamageType(SkillDamageType type) { damage_type_ = type; }

    // ========== 目标信息 ==========
    SkillTargetType GetTargetType() const { return target_type_; }
    void SetTargetType(SkillTargetType type) { target_type_ = type; }

    float GetCastRange() const { return cast_range_; }
    void SetCastRange(float range) { cast_range_ = range; }

    float GetEffectRange() const { return effect_range_; }
    void SetEffectRange(float range) { effect_range_ = range; }

    // ========== 学习要求 ==========
    uint16_t GetMaxLevel() const { return max_level_; }
    void SetMaxLevel(uint16_t level) { max_level_ = level; }

    uint32_t GetRequireLevel() const { return require_level_; }
    void SetRequireLevel(uint32_t level) { require_level_ = level; }

    uint32_t GetRequireSkillID() const { return require_skill_id_; }
    void SetRequireSkillID(uint32_t id) { require_skill_id_ = id; }

    // ========== 等级信息访问 ==========

    // 获取指定等级的信息
    const SkillLevelInfo* GetLevelInfo(uint16_t level) const;

    // 检查等级是否有效
    bool IsValidLevel(uint16_t level) const;

    // ========== 根据等级获取属性 ==========

    // 获取冷却时间(毫秒)
    uint32_t GetCooldown(uint16_t level) const;

    // 获取施法时间(毫秒)
    uint32_t GetCastTime(uint16_t level) const;

    // 获取MP消耗
    uint32_t GetMPCost(uint16_t level) const;

    // 获取HP消耗
    uint32_t GetHPCost(uint16_t level) const;

    // 获取持续时间(秒)
    float GetDuration(uint16_t level) const;

    // 获取范围
    float GetRange(uint16_t level) const;

    // 获取基础伤害
    uint32_t GetBaseDamage(uint16_t level) const;

    // 获取基础治疗
    uint32_t GetBaseHeal(uint16_t level) const;

    // 获取伤害倍率
    float GetDamageRate(uint16_t level) const;

    // 获取治疗倍率
    float GetHealRate(uint16_t level) const;

    // 获取技能范围ID
    uint32_t GetSkillAreaID() const { return skill_area_id_; }

    // 获取最大目标数量
    uint8_t GetMaxTargetCount() const { return max_target_count_; }

    // 获取施法者状态ID
    uint32_t GetStateIDSelf() const { return state_id_self_; }

    // 获取目标状态ID
    uint32_t GetStateIDTarget() const { return state_id_target_; }

    // 获取效果ID
    uint32_t GetEffectID() const { return effect_id_; }

    // ========== 检查函数 ==========

    // 检查是否为被动技能
    bool IsPassive() const { return skill_type_ == SkillType::Passive; }

    // 检查是否为主动技能
    bool IsActive() const { return skill_type_ == SkillType::Active; }

    // 检查是否需要目标
    bool NeedsTarget() const;

    // 检查是否为范围技能
    bool IsAreaSkill() const;

    // 检查是否为伤害技能
    bool IsDamageSkill() const;

    // 检查是否为治疗技能
    bool IsHealSkill() const;

    // 检查是否为Buff技能
    bool IsBuffSkill() const;

    // ========== 等级信息管理 ==========

    // 添加等级信息
    void AddLevelInfo(const SkillLevelInfo& info);

    // 移除等级信息
    void RemoveLevelInfo(uint16_t level);

    // 获取所有等级
    std::vector<uint16_t> GetAllLevels() const;
};

// ========== Skill Database ==========
// 技能数据库
class SkillDatabase {
private:
    static std::map<uint32_t, SkillInfo*> skill_db_;

public:
    // 初始化
    static bool Init();
    static void Release();

    // 查询技能
    static const SkillInfo* GetSkillInfo(uint32_t skill_id);
    static SkillInfo* GetSkillInfoMutable(uint32_t skill_id);

    // 添加技能
    static bool AddSkill(SkillInfo* skill_info);

    // 移除技能
    static bool RemoveSkill(uint32_t skill_id);

    // 获取所有技能ID
    static std::vector<uint32_t> GetAllSkillIDs();

    // 加载技能数据
    static bool LoadFromConfig(const char* config_file);
};

// ========== Helper Functions ==========

// 创建技能等级信息
SkillLevelInfo CreateSkillLevelInfo(
    uint16_t level,
    uint32_t mp_cost,
    uint32_t cooldown_ms,
    float damage_rate,
    float range
);

// 检查技能类型是否有效
bool IsValidSkillType(SkillType type);

// 获取技能类型字符串
const char* GetSkillTypeString(SkillType type);

// 获取技能元素字符串
const char* GetSkillElementString(SkillElement elem);

} // namespace Game
} // namespace Murim
