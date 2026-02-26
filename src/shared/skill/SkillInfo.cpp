// MurimServer - Skill Info Implementation
// 技能信息实现

#include "SkillInfo.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>

namespace Murim {
namespace Game {

// ========== 静态成员初始化 ==========

std::map<uint32_t, SkillInfo*> SkillDatabase::skill_db_;

// ========== SkillInfo ==========

SkillInfo::SkillInfo()
    : skill_id_(0),
      skill_name_(""),
      skill_description_(""),
      skill_type_(SkillType::Active),
      element_(SkillElement::None),
      damage_type_(SkillDamageType::Physical),
      target_type_(SkillTargetType::Single),
      cast_range_(5.0f),
      effect_range_(0.0f),
      icon_id_(0),
      max_level_(1),
      require_level_(1),
      require_skill_id_(0),
      require_skill_level_(0),
      require_quest_id_(0),
      gold_cost_learn(0),
      exp_cost_learn(0),
      skill_area_id_(0),
      max_target_count_(1),
      state_id_self_(0),
      state_id_target_(0),
      effect_id_(0) {
}

SkillInfo::~SkillInfo() {
    Release();
}

bool SkillInfo::Init() {
    spdlog::debug("[SkillInfo] Initialized: skill_id={}, name={}", skill_id_, skill_name_);
    return true;
}

void SkillInfo::Release() {
    level_info_.clear();
    spdlog::debug("[SkillInfo] Released: skill_id={}, name={}", skill_id_, skill_name_);
}

// ========== 等级信息访问 ==========

const SkillLevelInfo* SkillInfo::GetLevelInfo(uint16_t level) const {
    auto it = level_info_.find(level);
    if (it != level_info_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool SkillInfo::IsValidLevel(uint16_t level) const {
    return level >= 1 && level <= max_level_ && level_info_.count(level) > 0;
}

// ========== 根据等级获取属性 ==========

uint32_t SkillInfo::GetCooldown(uint16_t level) const {
    const SkillLevelInfo* info = GetLevelInfo(level);
    if (!info) {
        // 如果没有等级信息,返回默认值
        spdlog::warn("[SkillInfo] No level info for skill {} level {}, using default",
                    skill_id_, level);
        return 3000;  // 默认3秒冷却
    }
    return info->cooldown_ms;
}

uint32_t SkillInfo::GetCastTime(uint16_t level) const {
    const SkillLevelInfo* info = GetLevelInfo(level);
    if (!info) return 0;  // 默认瞬发
    return info->cast_time_ms;
}

uint32_t SkillInfo::GetMPCost(uint16_t level) const {
    const SkillLevelInfo* info = GetLevelInfo(level);
    if (!info) return 0;
    return info->mp_cost;
}

uint32_t SkillInfo::GetHPCost(uint16_t level) const {
    const SkillLevelInfo* info = GetLevelInfo(level);
    if (!info) return 0;
    return info->hp_cost;
}

float SkillInfo::GetDuration(uint16_t level) const {
    const SkillLevelInfo* info = GetLevelInfo(level);
    if (!info) return 0.0f;
    return info->duration_sec;
}

float SkillInfo::GetRange(uint16_t level) const {
    const SkillLevelInfo* info = GetLevelInfo(level);
    if (!info) return cast_range_;  // 默认使用施法范围
    return info->range > 0 ? info->range : cast_range_;
}

uint32_t SkillInfo::GetBaseDamage(uint16_t level) const {
    const SkillLevelInfo* info = GetLevelInfo(level);
    if (!info) return 0;
    return info->base_damage;
}

uint32_t SkillInfo::GetBaseHeal(uint16_t level) const {
    const SkillLevelInfo* info = GetLevelInfo(level);
    if (!info) return 0;
    return info->base_heal;
}

float SkillInfo::GetDamageRate(uint16_t level) const {
    const SkillLevelInfo* info = GetLevelInfo(level);
    if (!info) return 1.0f;
    return info->damage_rate;
}

float SkillInfo::GetHealRate(uint16_t level) const {
    const SkillLevelInfo* info = GetLevelInfo(level);
    if (!info) return 1.0f;
    return info->heal_rate;
}

// ========== 检查函数 ==========

bool SkillInfo::NeedsTarget() const {
    return target_type_ == SkillTargetType::Single ||
           target_type_ == SkillTargetType::Enemy ||
           target_type_ == SkillTargetType::Friend;
}

bool SkillInfo::IsAreaSkill() const {
    return target_type_ == SkillTargetType::Area ||
           effect_range_ > 0.0f;
}

bool SkillInfo::IsDamageSkill() const {
    return damage_type_ == SkillDamageType::Physical ||
           damage_type_ == SkillDamageType::Magic ||
           damage_type_ == SkillDamageType::True ||
           damage_type_ == SkillDamageType::Hybrid;
}

bool SkillInfo::IsHealSkill() const {
    // 检查是否有治疗效果
    for (const auto& pair : level_info_) {
        if (pair.second.base_heal > 0 || pair.second.heal_rate > 0.0f) {
            return true;
        }
    }
    return false;
}

bool SkillInfo::IsBuffSkill() const {
    // 检查是否给予状态
    return state_id_target_ > 0 || state_id_self_ > 0;
}

// ========== 等级信息管理 ==========

void SkillInfo::AddLevelInfo(const SkillLevelInfo& info) {
    level_info_[info.level] = info;
    spdlog::trace("[SkillInfo] Added level info: skill={}, level={}", skill_id_, info.level);
}

void SkillInfo::RemoveLevelInfo(uint16_t level) {
    level_info_.erase(level);
    spdlog::trace("[SkillInfo] Removed level info: skill={}, level={}", skill_id_, level);
}

std::vector<uint16_t> SkillInfo::GetAllLevels() const {
    std::vector<uint16_t> levels;
    levels.reserve(level_info_.size());

    for (const auto& pair : level_info_) {
        levels.push_back(pair.first);
    }

    std::sort(levels.begin(), levels.end());
    return levels;
}

// ========== Skill Database ==========

bool SkillDatabase::Init() {
    spdlog::info("SkillDatabase: Initializing with {} skills", skill_db_.size());
    return true;
}

void SkillDatabase::Release() {
    for (auto& pair : skill_db_) {
        delete pair.second;
    }
    skill_db_.clear();
    spdlog::info("SkillDatabase: Released");
}

const SkillInfo* SkillDatabase::GetSkillInfo(uint32_t skill_id) {
    auto it = skill_db_.find(skill_id);
    if (it != skill_db_.end()) {
        return it->second;
    }
    return nullptr;
}

SkillInfo* SkillDatabase::GetSkillInfoMutable(uint32_t skill_id) {
    auto it = skill_db_.find(skill_id);
    if (it != skill_db_.end()) {
        return it->second;
    }
    return nullptr;
}

bool SkillDatabase::AddSkill(SkillInfo* skill_info) {
    if (!skill_info) {
        spdlog::error("[SkillDatabase] Failed to add skill: null pointer");
        return false;
    }

    uint32_t skill_id = skill_info->GetSkillID();

    if (skill_db_.find(skill_id) != skill_db_.end()) {
        spdlog::warn("[SkillDatabase] Skill already exists: {}", skill_id);
        return false;
    }

    skill_info->Init();
    skill_db_[skill_id] = skill_info;

    spdlog::info("[SkillDatabase] Skill added: id={}, name={}",
                skill_id, skill_info->GetSkillName());

    return true;
}

bool SkillDatabase::RemoveSkill(uint32_t skill_id) {
    auto it = skill_db_.find(skill_id);
    if (it == skill_db_.end()) {
        spdlog::warn("[SkillDatabase] Skill not found: {}", skill_id);
        return false;
    }

    delete it->second;
    skill_db_.erase(it);

    spdlog::info("[SkillDatabase] Skill removed: id={}", skill_id);

    return true;
}

std::vector<uint32_t> SkillDatabase::GetAllSkillIDs() {
    std::vector<uint32_t> ids;
    ids.reserve(skill_db_.size());

    for (const auto& pair : skill_db_) {
        ids.push_back(pair.first);
    }

    std::sort(ids.begin(), ids.end());
    return ids;
}

bool SkillDatabase::LoadFromConfig(const char* config_file) {
    // TODO: 从配置文件加载技能数据
    spdlog::info("[SkillDatabase] Loading skills from config: {}", config_file);

    // 这里添加一些示例技能

    // 技能1: 火球术
    SkillInfo* fireball = new SkillInfo();
    fireball->SetSkillID(1001);
    fireball->SetSkillName("Fireball");
    fireball->SetDescription("Launches a fireball at the target");
    fireball->SetSkillType(SkillType::Active);
    fireball->SetElement(SkillElement::Fire);
    fireball->SetDamageType(SkillDamageType::Magic);
    fireball->SetTargetType(SkillTargetType::Single);
    fireball->SetCastRange(15.0f);
    fireball->SetMaxLevel(10);
    fireball->SetRequireLevel(1);

    // 添加等级1信息
    SkillLevelInfo level1;
    level1.level = 1;
    level1.mp_cost = 20;
    level1.cooldown_ms = 3000;
    level1.damage_rate = 1.5f;
    level1.base_damage = 100;
    level1.range = 15.0f;
    fireball->AddLevelInfo(level1);

    // 添加等级2信息
    SkillLevelInfo level2;
    level2.level = 2;
    level2.mp_cost = 25;
    level2.cooldown_ms = 2900;
    level2.damage_rate = 1.6f;
    level2.base_damage = 150;
    level2.range = 16.0f;
    fireball->AddLevelInfo(level2);

    AddSkill(fireball);

    // 技能2: 治疗法术
    SkillInfo* heal = new SkillInfo();
    heal->SetSkillID(2001);
    heal->SetSkillName("Heal");
    heal->SetDescription("Restores HP to the target");
    heal->SetSkillType(SkillType::Active);
    heal->SetElement(SkillElement::Light);
    heal->SetTargetType(SkillTargetType::Friend);
    heal->SetCastRange(10.0f);
    heal->SetMaxLevel(10);
    heal->SetRequireLevel(1);

    // 添加等级1信息
    SkillLevelInfo heal_level1;
    heal_level1.level = 1;
    heal_level1.mp_cost = 30;
    heal_level1.cooldown_ms = 5000;
    heal_level1.heal_rate = 1.0f;
    heal_level1.base_heal = 200;
    heal_level1.range = 10.0f;
    heal->AddLevelInfo(heal_level1);

    AddSkill(heal);

    spdlog::info("[SkillDatabase] Loaded {} skills from config", skill_db_.size());

    return true;
}

// ========== Helper Functions ==========

SkillLevelInfo CreateSkillLevelInfo(
    uint16_t level,
    uint32_t mp_cost,
    uint32_t cooldown_ms,
    float damage_rate,
    float range) {

    SkillLevelInfo info;
    info.level = level;
    info.mp_cost = mp_cost;
    info.cooldown_ms = cooldown_ms;
    info.damage_rate = damage_rate;
    info.range = range;
    return info;
}

bool IsValidSkillType(SkillType type) {
    return type == SkillType::Passive ||
           type == SkillType::Active ||
           type == SkillType::Toggle ||
           type == SkillType::Channel;
}

const char* GetSkillTypeString(SkillType type) {
    switch (type) {
        case SkillType::Passive: return "Passive";
        case SkillType::Active: return "Active";
        case SkillType::Toggle: return "Toggle";
        case SkillType::Channel: return "Channel";
        default: return "Unknown";
    }
}

const char* GetSkillElementString(SkillElement elem) {
    switch (elem) {
        case SkillElement::None: return "None";
        case SkillElement::Fire: return "Fire";
        case SkillElement::Ice: return "Ice";
        case SkillElement::Lightning: return "Lightning";
        case SkillElement::Earth: return "Earth";
        case SkillElement::Wind: return "Wind";
        case SkillElement::Light: return "Light";
        case SkillElement::Dark: return "Dark";
        case SkillElement::Physical: return "Physical";
        case SkillElement::Magic: return "Magic";
        default: return "Unknown";
    }
}

} // namespace Game
} // namespace Murim
