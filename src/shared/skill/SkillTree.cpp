// MurimServer - SkillTree Implementation
// 技能树系统实现

#include "SkillTree.hpp"
#include "SkillInfo.hpp"
#include "../game/GameObject.hpp"
#include "core/spdlog_wrapper.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace Murim {
namespace Game {

// ========== Helper Functions ==========

uint32_t CalculateExpForLevel(uint8_t level) {
    if (level == 0 || level > MAX_SKILL_LEVEL) {
        return 0;
    }
    // 经验公式：100 * level^1.5
    return static_cast<uint32_t>(100.0f * std::pow(level, 1.5f));
}

float CalculateSkillTreeAmp(const SkillTree* tree, uint32_t skill_id) {
    if (!tree) {
        return 1.0f;
    }

    // 基础加成：每个已学技能提供0.5%加成
    float amp = 1.0f + (tree->GetSkillCount() * 0.005f);

    // 技能链加成：同一技能链的技能越多，加成越高
    // 实现技能链检测
    uint32_t skill_chain_count = tree->GetSkillChainCount(skill_id);
    float chain_bonus = 1.0f + (skill_chain_count * 0.01f);  // 每个链技能+1%
    amp *= chain_bonus;

    return std::min(amp, 2.0f);  // 最高100%加成
}

SkillKind GetSkillKindFromId(uint32_t skill_id) {
    // 从SkillInfo获取技能类型
    const SkillInfo* skill_info = SkillDatabase::GetSkillInfo(skill_id);
    if (!skill_info) {
        spdlog::warn("[SkillTree] Skill info not found for skill_id {}", skill_id);
        return SkillKind::Combo;  // 默认类型
    }

    // 根据技能类型映射到SkillKind
    SkillType skill_type = skill_info->GetSkillType();
    switch (skill_type) {
        case SkillType::Active:
            return SkillKind::Active;
        case SkillType::Passive:
            return SkillKind::Passive;
        case SkillType::Toggle:
            return SkillKind::Toggle;
        case SkillType::Channel:
            return SkillKind::Channel;
        default:
            return SkillKind::Combo;
    }
}

bool IsSkillKindMatch(uint32_t skill_id, SkillKind required_kind) {
    SkillKind kind = GetSkillKindFromId(skill_id);
    return kind == required_kind;
}

// ========== SkillTree ==========

SkillTree::SkillTree()
    : owner_id_(0),
      available_skill_points_(0),
      total_skill_points_earned_(0),
      global_damage_amp_(1.0f),
      global_cooldown_reduction_(0.0f),
      global_mana_cost_reduction_(0.0f) {
}

SkillTree::SkillTree(uint64_t owner_id)
    : owner_id_(owner_id),
      available_skill_points_(0),
      total_skill_points_earned_(0),
      global_damage_amp_(1.0f),
      global_cooldown_reduction_(0.0f),
      global_mana_cost_reduction_(0.0f) {
}

SkillTree::~SkillTree() {
    Release();
}

void SkillTree::Init(uint64_t owner_id) {
    owner_id_ = owner_id;
    spdlog::debug("SkillTree initialized for owner {}", owner_id);
}

void SkillTree::Release() {
    skill_nodes_.clear();
    available_skill_points_ = 0;
    total_skill_points_earned_ = 0;
    global_damage_amp_ = 1.0f;
    global_cooldown_reduction_ = 0.0f;
    global_mana_cost_reduction_ = 0.0f;

    spdlog::debug("SkillTree released for owner {}", owner_id_);
}

bool SkillTree::AddSkillNode(std::unique_ptr<SkillNode> node) {
    if (!node) {
        return false;
    }

    uint32_t skill_id = node->skill_id;
    skill_nodes_[skill_id] = std::move(node);

    spdlog::debug("SkillTree: Added skill node {} for owner {}", skill_id, owner_id_);

    return true;
}

SkillNode* SkillTree::GetSkillNode(uint32_t skill_id) {
    auto it = skill_nodes_.find(skill_id);
    return (it != skill_nodes_.end()) ? it->second.get() : nullptr;
}

const SkillNode* SkillTree::GetSkillNode(uint32_t skill_id) const {
    auto it = skill_nodes_.find(skill_id);
    return (it != skill_nodes_.end()) ? it->second.get() : nullptr;
}

std::vector<uint32_t> SkillTree::GetAllSkillIds() const {
    std::vector<uint32_t> skill_ids;
    skill_ids.reserve(skill_nodes_.size());

    for (const auto& pair : skill_nodes_) {
        skill_ids.push_back(pair.first);
    }

    return skill_ids;
}

bool SkillTree::CanLearnSkill(uint32_t skill_id) const {
    const SkillNode* node = GetSkillNode(skill_id);
    if (!node) {
        spdlog::warn("SkillTree: Skill {} not found", skill_id);
        return false;
    }

    // 检查是否已学习
    if (HasSkill(skill_id)) {
        return false;
    }

    // 检查技能点
    if (available_skill_points_ < node->required_skill_points) {
        return false;
    }

    // 检查前置关系
    if (!CheckPrerequisites(skill_id)) {
        return false;
    }

    // 从SkillInfo获取并检查角色等级
    const SkillInfo* skill_info = SkillDatabase::GetSkillInfo(skill_id);
    if (skill_info) {
        uint32_t required_level = skill_info->GetRequireLevel();

        // 获取玩家当前等级（通过owner_id查找Player对象）
        uint32_t player_level = 1;  // 默认值，实际应从Player对象获取

        // 尝试从PlayerSystem获取等级
        // Player* player = PlayerManager::GetInstance().GetPlayer(owner_id_);
        // if (player) {
        //     player_level = player->GetLevel();
        // }

        if (player_level < required_level) {
            spdlog::debug("[SkillTree] Player level {} < required {}", player_level, required_level);
            return false;
        }

        // 检查其他条件（职业、任务等）
        uint32_t required_quest_id = skill_info->GetRequireQuestID();
        if (required_quest_id != 0) {
            // TODO [需其他系统]: 检查任务是否完成
            // 需要QuestSystem提供：
            // 1. QuestManager::IsQuestCompleted(uint64_t player_id, uint32_t quest_id)
            // 2. QuestManager::GetQuestStatus(uint64_t player_id, uint32_t quest_id)
            // 3. 任务状态枚举：NotStarted, InProgress, Completed, TurnedIn
            //
            // 示例：
            // if (!QuestManager::GetInstance().IsQuestCompleted(owner_id_, required_quest_id)) {
            //     spdlog::debug("[SkillTree] Required quest {} not completed", required_quest_id);
            //     return false;
            // }
            //
            // 临时跳过任务检查
            spdlog::debug("[SkillTree] Quest check not implemented, skipping quest {}", required_quest_id);
        }

        uint32_t required_skill_id = skill_info->GetRequireSkillID();
        if (required_skill_id != 0) {
            uint16_t required_skill_level = skill_info->GetRequireSkillLevel();
            // 检查前置技能等级
            if (!HasSkill(required_skill_id)) {
                spdlog::debug("[SkillTree] Required skill {} not learned", required_skill_id);
                return false;
            }

            const SkillNode* required_node = GetSkillNode(required_skill_id);
            if (required_node && required_node->current_level < required_skill_level) {
                spdlog::debug("[SkillTree] Required skill level {} < {}",
                            required_node->current_level, required_skill_level);
                return false;
            }
        }
    }

    return true;
}

bool SkillTree::LearnSkill(uint32_t skill_id) {
    if (!CanLearnSkill(skill_id)) {
        return false;
    }

    SkillNode* node = GetSkillNode(skill_id);
    if (!node) {
        return false;
    }

    // 消耗技能点
    if (!UseSkillPoints(node->required_skill_points)) {
        return false;
    }

    // 学习技能
    node->skill_level = 1;
    node->current_exp = 0;

    // 更新技能树加成
    UpdateSkillTreeBonus();

    spdlog::info("SkillTree: Learned skill {} for owner {}", skill_id, owner_id_);

    return true;
}

bool SkillTree::CanUpgradeSkill(uint32_t skill_id) const {
    const SkillNode* node = GetSkillNode(skill_id);
    if (!node || node->skill_level == 0) {
        return false;  // 未学习
    }

    if (node->skill_level >= node->max_level) {
        return false;  // 已满级
    }

    uint32_t required_exp = CalculateExpForLevel(node->skill_level + 1);
    if (node->current_exp < required_exp) {
        return false;  // 经验不足
    }

    return true;
}

bool SkillTree::UpgradeSkill(uint32_t skill_id) {
    if (!CanUpgradeSkill(skill_id)) {
        return false;
    }

    SkillNode* node = GetSkillNode(skill_id);
    if (!node) {
        return false;
    }

    // 消耗经验
    uint32_t required_exp = CalculateExpForLevel(node->skill_level + 1);
    node->current_exp -= required_exp;

    // 升级
    node->skill_level++;

    spdlog::info("SkillTree: Upgraded skill {} to level {} for owner {}",
                skill_id, node->skill_level, owner_id_);

    return true;
}

bool SkillTree::AddSkillExp(uint32_t skill_id, uint32_t exp) {
    SkillNode* node = GetSkillNode(skill_id);
    if (!node || node->skill_level == 0) {
        return false;
    }

    node->current_exp += exp;

    spdlog::debug("SkillTree: Added {} exp to skill {} (total: {})",
                 exp, skill_id, node->current_exp);

    return true;
}

bool SkillTree::CheckPrerequisites(uint32_t skill_id) const {
    const SkillNode* node = GetSkillNode(skill_id);
    if (!node) {
        return false;
    }

    // 检查所有前置条件
    for (const auto& prereq : node->all_prerequisites) {
        const SkillNode* prereq_node = GetSkillNode(prereq.skill_id);
        if (!prereq_node) {
            return false;
        }

        if (prereq_node->skill_level < prereq.required_level) {
            return false;
        }
    }

    return true;
}

std::vector<uint32_t> SkillTree::GetMissingPrerequisites(uint32_t skill_id) const {
    std::vector<uint32_t> missing;

    const SkillNode* node = GetSkillNode(skill_id);
    if (!node) {
        return missing;
    }

    for (const auto& prereq : node->all_prerequisites) {
        const SkillNode* prereq_node = GetSkillNode(prereq.skill_id);
        if (!prereq_node || prereq_node->skill_level < prereq.required_level) {
            missing.push_back(prereq.skill_id);
        }
    }

    return missing;
}

std::vector<uint32_t> SkillTree::GetNextSkills(uint32_t skill_id) const {
    const SkillNode* node = GetSkillNode(skill_id);
    if (!node) {
        return {};
    }

    return node->next_skill_ids;
}

bool SkillTree::HasNextSkills(uint32_t skill_id) const {
    const SkillNode* node = GetSkillNode(skill_id);
    return node ? !node->next_skill_ids.empty() : false;
}

void SkillTree::AddSkillPoints(uint16_t points) {
    available_skill_points_ += points;
    total_skill_points_earned_ += points;

    spdlog::debug("SkillTree: Added {} skill points (total available: {})",
                 points, available_skill_points_);
}

bool SkillTree::UseSkillPoints(uint16_t points) {
    if (available_skill_points_ < points) {
        return false;
    }

    available_skill_points_ -= points;

    spdlog::debug("SkillTree: Used {} skill points (remaining: {})",
                 points, available_skill_points_);

    return true;
}

void SkillTree::UpdateSkillTreeBonus() {
    // 计算全局加成
    size_t learned_count = GetLearnedSkills().size();

    // 每10个技能提供1%伤害加成
    global_damage_amp_ = 1.0f + (learned_count * 0.001f);

    // 每20个技能提供1%冷却缩减
    global_cooldown_reduction_ = std::min(learned_count * 0.0005f, 0.5f);

    // 每15个技能提供1%蓝耗减少
    global_mana_cost_reduction_ = std::min(learned_count * 0.00067f, 0.5f);

    spdlog::debug("SkillTree: Updated bonus - DmgAmp={:.3f}, CDR={:.3f}, MCR={:.3f}",
                 global_damage_amp_, global_cooldown_reduction_, global_mana_cost_reduction_);
}

float SkillTree::GetSkillTreeAmp(uint32_t skill_id) const {
    const SkillNode* node = GetSkillNode(skill_id);
    if (!node) {
        return 1.0f;
    }

    return node->skill_tree_amp;
}

bool SkillTree::HasSkill(uint32_t skill_id) const {
    const SkillNode* node = GetSkillNode(skill_id);
    return node && node->skill_level > 0;
}

uint16_t SkillTree::GetSkillLevel(uint32_t skill_id) const {
    const SkillNode* node = GetSkillNode(skill_id);
    return node ? node->skill_level : 0;
}

uint32_t SkillTree::GetSkillExp(uint32_t skill_id) const {
    const SkillNode* node = GetSkillNode(skill_id);
    return node ? node->current_exp : 0;
}

std::vector<uint32_t> SkillTree::GetLearnedSkills() const {
    std::vector<uint32_t> learned;

    for (const auto& pair : skill_nodes_) {
        if (pair.second->skill_level > 0) {
            learned.push_back(pair.first);
        }
    }

    return learned;
}

std::vector<uint32_t> SkillTree::GetAvailableSkills() const {
    std::vector<uint32_t> available;

    for (const auto& pair : skill_nodes_) {
        if (pair.second->skill_level == 0 && CanLearnSkill(pair.first)) {
            available.push_back(pair.first);
        }
    }

    return available;
}

std::vector<uint32_t> SkillTree::GetSkillsByKind(SkillKind kind) const {
    std::vector<uint32_t> skills;

    for (const auto& pair : skill_nodes_) {
        if (pair.second->skill_kind == kind) {
            skills.push_back(pair.first);
        }
    }

    return skills;
}

std::vector<uint32_t> SkillTree::GetSkillsByTreeIndex(int tree_index) const {
    std::vector<uint32_t> skills;

    for (const auto& pair : skill_nodes_) {
        if (pair.second->tree_index == tree_index) {
            skills.push_back(pair.first);
        }
    }

    return skills;
}

bool SkillTree::SaveToFile(const std::string& file_path) const {
    // 实现序列化 - 将技能树保存到文件
    std::ofstream out_file(file_path, std::ios::binary);
    if (!out_file.is_open()) {
        spdlog::error("[SkillTree] Failed to open file for writing: {}", file_path);
        return false;
    }

    try {
        // 写入文件头
        uint32_t file_version = 1;
        uint64_t owner_id = owner_id_;
        out_file.write(reinterpret_cast<const char*>(&file_version), sizeof(file_version));
        out_file.write(reinterpret_cast<const char*>(&owner_id), sizeof(owner_id));

        // 写入技能点信息
        out_file.write(reinterpret_cast<const char*>(&available_skill_points_), sizeof(available_skill_points_));
        out_file.write(reinterpret_cast<const char*>(&total_skill_points_earned_), sizeof(total_skill_points_earned_));

        // 写入全局加成
        out_file.write(reinterpret_cast<const char*>(&global_damage_amp_), sizeof(global_damage_amp_));
        out_file.write(reinterpret_cast<const char*>(&global_cooldown_reduction_), sizeof(global_cooldown_reduction_));
        out_file.write(reinterpret_cast<const char*>(&global_mana_cost_reduction_), sizeof(global_mana_cost_reduction_));

        // 写入技能数量
        uint32_t skill_count = static_cast<uint32_t>(skill_nodes_.size());
        out_file.write(reinterpret_cast<const char*>(&skill_count), sizeof(skill_count));

        // 写入每个技能
        for (const auto& pair : skill_nodes_) {
            uint32_t skill_id = pair.first;
            const SkillNode& node = *pair.second;

            out_file.write(reinterpret_cast<const char*>(&skill_id), sizeof(skill_id));
            out_file.write(reinterpret_cast<const char*>(&node.current_level), sizeof(node.current_level));
            out_file.write(reinterpret_cast<const char*>(&node.required_skill_points), sizeof(node.required_skill_points));
        }

        out_file.close();
        spdlog::info("[SkillTree] Saved {} skills to file: {}", skill_count, file_path);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[SkillTree] Error saving to file: {}", e.what());
        out_file.close();
        return false;
    }
}

bool SkillTree::LoadFromFile(const std::string& file_path) {
    // 实现反序列化 - 从文件加载技能树
    std::ifstream in_file(file_path, std::ios::binary);
    if (!in_file.is_open()) {
        spdlog::error("[SkillTree] Failed to open file for reading: {}", file_path);
        return false;
    }

    try {
        // 读取文件头
        uint32_t file_version;
        uint64_t owner_id;
        in_file.read(reinterpret_cast<char*>(&file_version), sizeof(file_version));
        in_file.read(reinterpret_cast<char*>(&owner_id), sizeof(owner_id));

        if (file_version != 1) {
            spdlog::warn("[SkillTree] Unsupported file version: {}", file_version);
            in_file.close();
            return false;
        }

        owner_id_ = owner_id;

        // 读取技能点信息
        in_file.read(reinterpret_cast<char*>(&available_skill_points_), sizeof(available_skill_points_));
        in_file.read(reinterpret_cast<char*>(&total_skill_points_earned_), sizeof(total_skill_points_earned_));

        // 读取全局加成
        in_file.read(reinterpret_cast<char*>(&global_damage_amp_), sizeof(global_damage_amp_));
        in_file.read(reinterpret_cast<char*>(&global_cooldown_reduction_), sizeof(global_cooldown_reduction_));
        in_file.read(reinterpret_cast<char*>(&global_mana_cost_reduction_), sizeof(global_mana_cost_reduction_));

        // 读取技能数量
        uint32_t skill_count;
        in_file.read(reinterpret_cast<char*>(&skill_count), sizeof(skill_count));

        // 清空当前技能
        skill_nodes_.clear();

        // 读取每个技能
        for (uint32_t i = 0; i < skill_count; ++i) {
            uint32_t skill_id;
            SkillNode node;

            in_file.read(reinterpret_cast<char*>(&skill_id), sizeof(skill_id));
            in_file.read(reinterpret_cast<char*>(&node.current_level), sizeof(node.current_level));
            in_file.read(reinterpret_cast<char*>(&node.required_skill_points), sizeof(node.required_skill_points));

            // 创建新的SkillNode并插入
            auto new_node = std::make_unique<SkillNode>();
            new_node->current_level = node.current_level;
            new_node->required_skill_points = node.required_skill_points;
            skill_nodes_[skill_id] = std::move(new_node);
        }

        in_file.close();
        spdlog::info("[SkillTree] Loaded {} skills from file: {}", skill_count, file_path);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[SkillTree] Error loading from file: {}", e.what());
        in_file.close();
        return false;
    }
}

// ========== SkillOptionManager ==========

SkillOptionManager::SkillOptionManager()
    : owner_id_(0) {
}

SkillOptionManager::SkillOptionManager(uint64_t owner_id)
    : owner_id_(owner_id) {
}

SkillOptionManager::~SkillOptionManager() {
    Release();
}

void SkillOptionManager::Init(uint64_t owner_id) {
    owner_id_ = owner_id;
    spdlog::debug("SkillOptionManager initialized for owner {}", owner_id);
}

void SkillOptionManager::Release() {
    skill_options_.clear();
    spdlog::debug("SkillOptionManager released for owner {}", owner_id_);
}

void SkillOptionManager::AddOption(const SkillOption& option) {
    skill_options_.push_back(option);

    spdlog::debug("SkillOptionManager: Added option type={}, skill_id={}, value={:.2f}",
                static_cast<int>(option.type), option.skill_id, option.value);
}

void SkillOptionManager::RemoveOption(uint32_t option_index) {
    if (option_index >= skill_options_.size()) {
        return;
    }

    skill_options_.erase(skill_options_.begin() + option_index);

    spdlog::debug("SkillOptionManager: Removed option at index {}", option_index);
}

void SkillOptionManager::ClearOptions() {
    skill_options_.clear();
    spdlog::debug("SkillOptionManager: Cleared all options");
}

float SkillOptionManager::CalculateSkillBonus(uint32_t skill_id, SkillOption::Type type) const {
    float total_bonus = 0.0f;

    for (const auto& option : skill_options_) {
        if (option.type == type) {
            // 检查是否适用于该技能
            if (option.skill_id == 0 || option.skill_id == skill_id) {
                total_bonus += option.value;
            }
        }

        // All类型适用于所有技能
        if (option.type == SkillOption::Type::All) {
            total_bonus += option.value;
        }
    }

    return total_bonus;
}

float SkillOptionManager::CalculateDamageBonus(uint32_t skill_id) const {
    return CalculateSkillBonus(skill_id, SkillOption::Type::Damage);
}

float SkillOptionManager::CalculateCooldownBonus(uint32_t skill_id) const {
    return CalculateSkillBonus(skill_id, SkillOption::Type::Cooldown);
}

float SkillOptionManager::CalculateManaCostBonus(uint32_t skill_id) const {
    return CalculateSkillBonus(skill_id, SkillOption::Type::ManaCost);
}

float SkillOptionManager::CalculateDurationBonus(uint32_t skill_id) const {
    return CalculateSkillBonus(skill_id, SkillOption::Type::Duration);
}

float SkillOptionManager::CalculateRangeBonus(uint32_t skill_id) const {
    return CalculateSkillBonus(skill_id, SkillOption::Type::Range);
}

float SkillOptionManager::CalculateCastTimeBonus(uint32_t skill_id) const {
    return CalculateSkillBonus(skill_id, SkillOption::Type::CastTime);
}

float SkillOptionManager::CalculateCriticalBonus(uint32_t skill_id) const {
    return CalculateSkillBonus(skill_id, SkillOption::Type::Critical);
}

bool SkillOptionManager::SaveToFile(const std::string& file_path) const {
    // 实现序列化 - 保存技能选项到文件
    std::ofstream out_file(file_path, std::ios::binary);
    if (!out_file.is_open()) {
        spdlog::error("[SkillOptionManager] Failed to open file for writing: {}", file_path);
        return false;
    }

    try {
        // 写入文件头
        uint32_t file_version = 1;
        uint64_t owner_id = owner_id_;
        out_file.write(reinterpret_cast<const char*>(&file_version), sizeof(file_version));
        out_file.write(reinterpret_cast<const char*>(&owner_id), sizeof(owner_id));

        // 写入选项数量
        uint32_t option_count = static_cast<uint32_t>(skill_options_.size());
        out_file.write(reinterpret_cast<const char*>(&option_count), sizeof(option_count));

        // 写入每个选项
        for (const SkillOption& option : skill_options_) {
            out_file.write(reinterpret_cast<const char*>(&option), sizeof(option));
        }

        out_file.close();
        spdlog::info("[SkillOptionManager] Saved {} options to file: {}", option_count, file_path);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[SkillOptionManager] Error saving to file: {}", e.what());
        out_file.close();
        return false;
    }
}

bool SkillOptionManager::LoadFromFile(const std::string& file_path) {
    // 实现反序列化 - 从文件加载技能选项
    std::ifstream in_file(file_path, std::ios::binary);
    if (!in_file.is_open()) {
        spdlog::error("[SkillOptionManager] Failed to open file for reading: {}", file_path);
        return false;
    }

    try {
        // 读取文件头
        uint32_t file_version;
        uint64_t owner_id;
        in_file.read(reinterpret_cast<char*>(&file_version), sizeof(file_version));
        in_file.read(reinterpret_cast<char*>(&owner_id), sizeof(owner_id));

        if (file_version != 1) {
            spdlog::warn("[SkillOptionManager] Unsupported file version: {}", file_version);
            in_file.close();
            return false;
        }

        owner_id_ = owner_id;

        // 读取选项数量
        uint32_t option_count;
        in_file.read(reinterpret_cast<char*>(&option_count), sizeof(option_count));

        // 清空当前选项
        skill_options_.clear();

        // 读取每个选项
        for (uint32_t i = 0; i < option_count; ++i) {
            SkillOption option;
            in_file.read(reinterpret_cast<char*>(&option), sizeof(option));
            skill_options_.push_back(option);
        }

        in_file.close();
        spdlog::info("[SkillOptionManager] Loaded {} options from file: {}", option_count, file_path);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[SkillOptionManager] Error loading from file: {}", e.what());
        in_file.close();
        return false;
    }
}

// ========== SkillUpgradeManager ==========

SkillUpgradeManager::SkillUpgradeManager()
    : owner_id_(0) {
}

SkillUpgradeManager::SkillUpgradeManager(uint64_t owner_id)
    : owner_id_(owner_id) {
}

SkillUpgradeManager::~SkillUpgradeManager() {
    Release();
}

void SkillUpgradeManager::Init(uint64_t owner_id) {
    owner_id_ = owner_id;
    spdlog::debug("SkillUpgradeManager initialized for owner {}", owner_id);
}

void SkillUpgradeManager::Release() {
    skill_exp_map_.clear();
    spdlog::debug("SkillUpgradeManager released for owner {}", owner_id_);
}

void SkillUpgradeManager::AddSkillExp(uint32_t skill_id, uint32_t exp) {
    skill_exp_map_[skill_id] += exp;

    spdlog::debug("SkillUpgradeManager: Added {} exp to skill {} (total: {})",
                 exp, skill_id, skill_exp_map_[skill_id]);
}

uint32_t SkillUpgradeManager::GetSkillExp(uint32_t skill_id) const {
    auto it = skill_exp_map_.find(skill_id);
    return (it != skill_exp_map_.end()) ? it->second : 0;
}

void SkillUpgradeManager::SetSkillExp(uint32_t skill_id, uint32_t exp) {
    skill_exp_map_[skill_id] = exp;
}

uint32_t SkillUpgradeManager::GetRequiredExpForLevel(uint32_t skill_id, uint8_t level) const {
    return CalculateExpForLevel(level);
}

bool SkillUpgradeManager::CanUpgradeSkill(uint32_t skill_id, uint8_t target_level) const {
    uint32_t current_exp = GetSkillExp(skill_id);
    uint32_t required_exp = GetRequiredExpForLevel(skill_id, target_level);

    return current_exp >= required_exp;
}

SkillUpgradeInfo SkillUpgradeManager::GetUpgradeInfo(uint32_t skill_id, uint8_t target_level) const {
    SkillUpgradeInfo info;
    info.skill_id = skill_id;

    // 从SkillTree获取当前等级
    const SkillTree* tree = GetOwnerSkillTree();
    if (tree) {
        const SkillNode* node = tree->GetSkillNode(skill_id);
        info.current_level = node ? node->current_level : 0;
    } else {
        info.current_level = 0;
    }

    info.target_level = target_level;
    info.required_exp = GetRequiredExpForLevel(skill_id, target_level);
    info.current_exp = GetSkillExp(skill_id);

    // 从SkillInfo获取属性成长
    const SkillInfo* skill_info = SkillDatabase::GetSkillInfo(skill_id);
    if (skill_info) {
        // 计算属性差异
        float level_diff = static_cast<float>(target_level - info.current_level);

        // 伤害成长
        uint32_t base_damage_low = skill_info->GetBaseDamage(info.current_level);
        uint32_t base_damage_high = skill_info->GetBaseDamage(target_level);
        info.damage_increase = static_cast<float>(base_damage_high - base_damage_low);

        // 蓝耗成长
        uint32_t mp_cost_low = skill_info->GetMPCost(info.current_level);
        uint32_t mp_cost_high = skill_info->GetMPCost(target_level);
        info.mana_cost_increase = static_cast<float>(mp_cost_high - mp_cost_low);

        // 冷却变化
        uint32_t cd_low = skill_info->GetCooldown(info.current_level);
        uint32_t cd_high = skill_info->GetCooldown(target_level);
        info.cooldown_change = static_cast<float>(cd_high - cd_low);

        // 持续时间成长
        float duration_low = skill_info->GetDuration(info.current_level);
        float duration_high = skill_info->GetDuration(target_level);
        info.duration_increase = duration_high - duration_low;

        spdlog::debug("[SkillUpgrade] Skill {}: lvl {}->{}, dmg+{:.0f}, mp+{:.0f}, cd{:+.0f}, dur{:+.1f}s",
                    skill_id, info.current_level, target_level,
                    info.damage_increase, info.mana_cost_increase,
                    info.cooldown_change, info.duration_increase);
    } else {
        // 使用默认成长公式
        float level_diff = static_cast<float>(target_level - info.current_level);
        info.damage_increase = level_diff * 0.1f;     // 每级+10%伤害
        info.mana_cost_increase = level_diff * 0.05f; // 每级+5%蓝耗
        info.cooldown_change = -level_diff * 0.02f;   // 每级-2%冷却
        info.duration_increase = level_diff * 0.05f;  // 每级+5%持续时间
    }

    return info;
}

float SkillUpgradeManager::GetDamageAtLevel(uint32_t skill_id, uint8_t level) const {
    // 从SkillInfo获取基础伤害
    const SkillInfo* skill_info = SkillDatabase::GetSkillInfo(skill_id);
    if (skill_info) {
        uint32_t base_damage = skill_info->GetBaseDamage(level);
        float damage_rate = skill_info->GetDamageRate(level);
        return static_cast<float>(base_damage) * damage_rate;
    }
    // 默认公式
    float base_damage = 100.0f;
    return base_damage * (1.0f + (level - 1) * 0.1f);
}

float SkillUpgradeManager::GetManaCostAtLevel(uint32_t skill_id, uint8_t level) const {
    // 从SkillInfo获取基础蓝耗
    const SkillInfo* skill_info = SkillDatabase::GetSkillInfo(skill_id);
    if (skill_info) {
        return static_cast<float>(skill_info->GetMPCost(level));
    }
    // 默认公式
    float base_mana_cost = 50.0f;
    return base_mana_cost * (1.0f + (level - 1) * 0.05f);
}

float SkillUpgradeManager::GetCooldownAtLevel(uint32_t skill_id, uint8_t level) const {
    // 从SkillInfo获取基础冷却
    const SkillInfo* skill_info = SkillDatabase::GetSkillInfo(skill_id);
    if (skill_info) {
        return static_cast<float>(skill_info->GetCooldown(level));
    }
    // 默认公式
    float base_cooldown = 3.0f;
    return base_cooldown * (1.0f - (level - 1) * 0.02f);  // 每级减少2%
}

float SkillUpgradeManager::GetDurationAtLevel(uint32_t skill_id, uint8_t level) const {
    // 从SkillInfo获取基础持续时间
    const SkillInfo* skill_info = SkillDatabase::GetSkillInfo(skill_id);
    if (skill_info) {
        return skill_info->GetDuration(level);
    }
    // 默认公式
    float base_duration = 5.0f;
    return base_duration * (1.0f + (level - 1) * 0.05f);
}

bool SkillUpgradeManager::SaveToFile(const std::string& file_path) const {
    // 实现序列化 - 保存技能经验到文件
    std::ofstream out_file(file_path, std::ios::binary);
    if (!out_file.is_open()) {
        spdlog::error("[SkillUpgradeManager] Failed to open file for writing: {}", file_path);
        return false;
    }

    try {
        // 写入文件头
        uint32_t file_version = 1;
        uint64_t owner_id = owner_id_;
        out_file.write(reinterpret_cast<const char*>(&file_version), sizeof(file_version));
        out_file.write(reinterpret_cast<const char*>(&owner_id), sizeof(owner_id));

        // 写入技能经验数量
        uint32_t exp_count = static_cast<uint32_t>(skill_exp_map_.size());
        out_file.write(reinterpret_cast<const char*>(&exp_count), sizeof(exp_count));

        // 写入每个技能的经验
        for (const auto& pair : skill_exp_map_) {
            uint32_t skill_id = pair.first;
            uint32_t exp = pair.second;

            out_file.write(reinterpret_cast<const char*>(&skill_id), sizeof(skill_id));
            out_file.write(reinterpret_cast<const char*>(&exp), sizeof(exp));
        }

        out_file.close();
        spdlog::info("[SkillUpgradeManager] Saved {} skill exp to file: {}", exp_count, file_path);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[SkillUpgradeManager] Error saving to file: {}", e.what());
        out_file.close();
        return false;
    }
}

bool SkillUpgradeManager::LoadFromFile(const std::string& file_path) {
    // 实现反序列化 - 从文件加载技能经验
    std::ifstream in_file(file_path, std::ios::binary);
    if (!in_file.is_open()) {
        spdlog::error("[SkillUpgradeManager] Failed to open file for reading: {}", file_path);
        return false;
    }

    try {
        // 读取文件头
        uint32_t file_version;
        uint64_t owner_id;
        in_file.read(reinterpret_cast<char*>(&file_version), sizeof(file_version));
        in_file.read(reinterpret_cast<char*>(&owner_id), sizeof(owner_id));

        if (file_version != 1) {
            spdlog::warn("[SkillUpgradeManager] Unsupported file version: {}", file_version);
            in_file.close();
            return false;
        }

        owner_id_ = owner_id;

        // 读取技能经验数量
        uint32_t exp_count;
        in_file.read(reinterpret_cast<char*>(&exp_count), sizeof(exp_count));

        // 清空当前经验
        skill_exp_map_.clear();

        // 读取每个技能的经验
        for (uint32_t i = 0; i < exp_count; ++i) {
            uint32_t skill_id;
            uint32_t exp;

            in_file.read(reinterpret_cast<char*>(&skill_id), sizeof(skill_id));
            in_file.read(reinterpret_cast<char*>(&exp), sizeof(exp));

            skill_exp_map_[skill_id] = exp;
        }

        in_file.close();
        spdlog::info("[SkillUpgradeManager] Loaded {} skill exp from file: {}", exp_count, file_path);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[SkillUpgradeManager] Error loading from file: {}", e.what());
        in_file.close();
        return false;
    }
}

} // namespace Game
} // namespace Murim
