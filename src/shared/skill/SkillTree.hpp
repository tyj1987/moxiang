// MurimServer - SkillTree System
// 技能树系统 - 管理技能的前置关系、加成和升级
// 对应legacy: skillinfo.h中的SkillTree相关定义

#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <memory>

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class GameObject;
class SkillInfo;

// ========== Constants ==========
constexpr int MAX_SKILL_TREE_DEPTH = 5;
constexpr int MAX_SKILL_LEVEL = 100;
constexpr uint32_t SKILL_POINT_PER_LEVEL = 1;

// ========== Skill Kind ==========
enum class SkillKind : uint16_t {
    Combo = 0,           // 连击技能
    OuterMugong = 1,     // 外功
    InnerMugong = 2,     // 内功
    Simbub = 3,          // 身法
    Jinbub = 4,          // 金箔
    Mining = 5,          // 采矿
    Collection = 6,      // 采集
    Hunt = 7,            // 狩猎
    Titan = 8,           // 泰坦
    Max = 9
};

// ========== Skill Prerequisite ==========
// 技能前置条件
struct SkillPrerequisite {
    uint32_t skill_id;           // 前置技能ID
    uint8_t required_level;      // 需要的等级
    bool is_mandatory;           // 是否必需（false表示只需其一）

    SkillPrerequisite()
        : skill_id(0), required_level(1), is_mandatory(true) {}
};

// ========== Skill Node ==========
// 技能树节点 - 表示技能树中的一个技能
struct SkillNode {
    uint32_t skill_id;           // 技能ID
    std::string skill_name;      // 技能名称
    SkillKind skill_kind;        // 技能类型
    uint16_t skill_level;        // 当前等级
    uint8_t max_level;           // 最大等级
    uint32_t current_exp;        // 当前经验

    // 前置关系
    uint32_t prerequisite_skill_id;   // 直接前置技能ID
    std::vector<SkillPrerequisite> all_prerequisites;  // 所有前置条件

    // 后续技能
    std::vector<uint32_t> next_skill_ids;  // 后续技能ID列表

    // 技能树位置（用于UI显示）
    int tree_index;              // 在技能树中的索引（0-4）
    int tree_depth;              // 深度（层级）
    float tree_x, tree_y;        // UI坐标

    // 学习条件
    uint16_t required_level;     // 需要的角色等级
    uint32_t required_skill_points;  // 需要的技能点

    // 加成信息
    float skill_tree_amp;        // 技能树加成倍率

    SkillNode()
        : skill_id(0), skill_kind(SkillKind::Combo),
          skill_level(0), max_level(10), current_exp(0),
          prerequisite_skill_id(0), tree_index(0), tree_depth(0),
          tree_x(0), tree_y(0), required_level(1),
          required_skill_points(1), skill_tree_amp(1.0f) {}
};

// ========== Skill Tree ==========
// 技能树 - 管理所有技能节点及其关系
class SkillTree {
private:
    std::map<uint32_t, std::unique_ptr<SkillNode>> skill_nodes_;
    uint64_t owner_id_;           // 拥有者ID

    // 技能点
    uint16_t available_skill_points_;
    uint16_t total_skill_points_earned_;

    // 技能树加成
    float global_damage_amp_;
    float global_cooldown_reduction_;
    float global Mana_cost_reduction_;

public:
    SkillTree();
    explicit SkillTree(uint64_t owner_id);
    ~SkillTree();

    // ========== 初始化 ==========
    void Init(uint64_t owner_id);
    void Release();

    // ========== 技能节点管理 ==========
    bool AddSkillNode(std::unique_ptr<SkillNode> node);
    SkillNode* GetSkillNode(uint32_t skill_id);
    const SkillNode* GetSkillNode(uint32_t skill_id) const;
    std::vector<uint32_t> GetAllSkillIds() const;
    size_t GetSkillCount() const { return skill_nodes_.size(); }

    // ========== 技能学习 ==========
    bool CanLearnSkill(uint32_t skill_id) const;
    bool LearnSkill(uint32_t skill_id);

    // ========== 技能升级 ==========
    bool CanUpgradeSkill(uint32_t skill_id) const;
    bool UpgradeSkill(uint32_t skill_id);
    bool AddSkillExp(uint32_t skill_id, uint32_t exp);

    // ========== 前置关系检查 ==========
    bool CheckPrerequisites(uint32_t skill_id) const;
    std::vector<uint32_t> GetMissingPrerequisites(uint32_t skill_id) const;

    // ========== 后续技能查询 ==========
    std::vector<uint32_t> GetNextSkills(uint32_t skill_id) const;
    bool HasNextSkills(uint32_t skill_id) const;

    // ========== 技能点管理 ==========
    void AddSkillPoints(uint16_t points);
    bool UseSkillPoints(uint16_t points);
    uint16_t GetAvailableSkillPoints() const { return available_skill_points_; }
    uint16_t GetTotalSkillPointsEarned() const { return total_skill_points_; }

    // ========== 技能树加成 ==========
    void UpdateSkillTreeBonus();
    float GetSkillTreeAmp(uint32_t skill_id) const;
    float GetGlobalDamageAmp() const { return global_damage_amp_; }
    float GetGlobalCooldownReduction() const { return global_cooldown_reduction_; }
    float GetGlobalManaCostReduction() const { return global_mana_cost_reduction_; }

    // ========== 技能查询 ==========
    bool HasSkill(uint32_t skill_id) const;
    uint16_t GetSkillLevel(uint32_t skill_id) const;
    uint32_t GetSkillExp(uint32_t skill_id) const;

    // ========== 技能树遍历 ==========
    std::vector<uint32_t> GetLearnedSkills() const;
    std::vector<uint32_t> GetAvailableSkills() const;  // 可以学习但未学习的技能
    std::vector<uint32_t> GetSkillsByKind(SkillKind kind) const;
    std::vector<uint32_t> GetSkillsByTreeIndex(int tree_index) const;

    // ========== 序列化 ==========
    bool SaveToFile(const std::string& file_path) const;
    bool LoadFromFile(const std::string& file_path);
};

// ========== Skill Option ==========
// 技能选项 - 从装备、坐骑等获得的技能加成
struct SkillOption {
    enum class Type : uint8_t {
        None = 0,
        Damage = 1,           // 伤害加成
        Cooldown = 2,         // 冷却缩减
        ManaCost = 3,         // 蓝耗减少
        Duration = 4,         // 持续时间加成
        Range = 5,            // 范围加成
        CastTime = 6,         // 施法时间减少
        Critical = 7,         // 暴击率加成
        All = 8               // 全类型加成
    };

    Type type;
    uint32_t skill_id;        // 0表示对所有技能生效
    float value;              // 加成值（可能是百分比或固定值）
    uint16_t level;           // 等级（有些加成随等级提升）

    SkillOption()
        : type(Type::None), skill_id(0), value(0), level(1) {}

    SkillOption(Type t, uint32_t sid, float v, uint16_t lv = 1)
        : type(t), skill_id(sid), value(v), level(lv) {}
};

// ========== Skill Option Manager ==========
// 技能选项管理器 - 管理所有技能加成选项
class SkillOptionManager {
private:
    std::vector<SkillOption> skill_options_;
    uint64_t owner_id_;

public:
    SkillOptionManager();
    explicit SkillOptionManager(uint64_t owner_id);
    ~SkillOptionManager();

    void Init(uint64_t owner_id);
    void Release();

    // ========== 选项管理 ==========
    void AddOption(const SkillOption& option);
    void RemoveOption(uint32_t option_index);
    void ClearOptions();

    // ========== 加成计算 ==========
    float CalculateSkillBonus(uint32_t skill_id, SkillOption::Type type) const;
    float CalculateDamageBonus(uint32_t skill_id) const;
    float CalculateCooldownBonus(uint32_t skill_id) const;
    float CalculateManaCostBonus(uint32_t skill_id) const;
    float CalculateDurationBonus(uint32_t skill_id) const;
    float CalculateRangeBonus(uint32_t skill_id) const;
    float CalculateCastTimeBonus(uint32_t skill_id) const;
    float CalculateCriticalBonus(uint32_t skill_id) const;

    // ========== 查询 ==========
    const std::vector<SkillOption>& GetAllOptions() const { return skill_options_; }
    size_t GetOptionCount() const { return skill_options_.size(); }

    // ========== 序列化 ==========
    bool SaveToFile(const std::string& file_path) const;
    bool LoadFromFile(const std::string& file_path);
};

// ========== Skill Upgrade Info ==========
// 技能升级信息
struct SkillUpgradeInfo {
    uint32_t skill_id;
    uint8_t current_level;
    uint8_t target_level;
    uint32_t required_exp;
    uint32_t current_exp;
    uint16_t required_skill_points;
    uint16_t available_skill_points;

    // 属性成长
    float damage_increase;
    float mana_cost_increase;
    float cooldown_change;
    float duration_increase;

    SkillUpgradeInfo()
        : skill_id(0), current_level(0), target_level(0),
          required_exp(0), current_exp(0),
          required_skill_points(0), available_skill_points(0),
          damage_increase(0), mana_cost_increase(0),
          cooldown_change(0), duration_increase(0) {}
};

// ========== Skill Upgrade Manager ==========
// 技能升级管理器 - 处理技能经验获取和等级提升
class SkillUpgradeManager {
private:
    std::map<uint32_t, uint32_t> skill_exp_map_;  // 技能ID -> 经验值
    uint64_t owner_id_;

public:
    SkillUpgradeManager();
    explicit SkillUpgradeManager(uint64_t owner_id);
    ~SkillUpgradeManager();

    void Init(uint64_t owner_id);
    void Release();

    // ========== 经验管理 ==========
    void AddSkillExp(uint32_t skill_id, uint32_t exp);
    uint32_t GetSkillExp(uint32_t skill_id) const;
    void SetSkillExp(uint32_t skill_id, uint32_t exp);

    // ========== 升级计算 ==========
    uint32_t GetRequiredExpForLevel(uint32_t skill_id, uint8_t level) const;
    bool CanUpgradeSkill(uint32_t skill_id, uint8_t target_level) const;
    SkillUpgradeInfo GetUpgradeInfo(uint32_t skill_id, uint8_t target_level) const;

    // ========== 属性成长 ==========
    float GetDamageAtLevel(uint32_t skill_id, uint8_t level) const;
    float GetManaCostAtLevel(uint32_t skill_id, uint8_t level) const;
    float GetCooldownAtLevel(uint32_t skill_id, uint8_t level) const;
    float GetDurationAtLevel(uint32_t skill_id, uint8_t level) const;

    // ========== 序列化 ==========
    bool SaveToFile(const std::string& file_path) const;
    bool LoadFromFile(const std::string& file_path);
};

// ========== Helper Functions ==========

// 计算技能等级对应的经验需求
uint32_t CalculateExpForLevel(uint8_t level);

// 计算技能树加成（基于已学技能数量和等级）
float CalculateSkillTreeAmp(const SkillTree* tree, uint32_t skill_id);

// 从技能ID获取技能类型
SkillKind GetSkillKindFromId(uint32_t skill_id);

// 检查技能是否符合类型要求
bool IsSkillKindMatch(uint32_t skill_id, SkillKind required_kind);

} // namespace Game
} // namespace Murim
