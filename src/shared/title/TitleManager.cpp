#include "TitleManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>
#include <chrono>

namespace Murim {
namespace Game {

// ========== TitleDefinition ==========

murim::TitleDefinition TitleDefinition::ToProto() const {
    murim::TitleDefinition proto;
    proto.set_title_id(title_id);
    proto.set_name(name);
    proto.set_description(description);
    proto.set_category(category);
    proto.set_display_location(display_location);
    proto.set_permanent(permanent);
    proto.set_duration_days(duration_days);

    // 属性加成
    proto.set_hp_bonus(hp_bonus);
    proto.set_mp_bonus(mp_bonus);
    proto.set_attack_bonus(attack_bonus);
    proto.set_defense_bonus(defense_bonus);
    proto.set_magic_attack_bonus(magic_attack_bonus);
    proto.set_magic_defense_bonus(magic_defense_bonus);
    proto.set_speed_bonus(speed_bonus);
    proto.set_critical_bonus(critical_bonus);
    proto.set_all_stats_bonus(all_stats_bonus);

    // 设置条件
    switch (requirement_type) {
        case RequirementType::ACHIEVEMENT: {
            auto* req = proto.mutable_achievement();
            req->set_achievement_id(requirement.achievement_id);
            break;
        }
        case RequirementType::LEVEL: {
            auto* req = proto.mutable_level();
            req->set_target_level(requirement.target_level);
            break;
        }
        case RequirementType::WEALTH: {
            auto* req = proto.mutable_wealth();
            req->set_target_gold(requirement.target_gold);
            break;
        }
        case RequirementType::KILL: {
            auto* req = proto.mutable_kill();
            req->set_monster_type_id(requirement.monster_type_id);
            req->set_kill_count(requirement.kill_count);
            break;
        }
        case RequirementType::QUEST: {
            auto* req = proto.mutable_quest();
            req->set_quest_id(requirement.quest_id);
            break;
        }
        case RequirementType::SPECIAL: {
            auto* req = proto.mutable_special();
            req->set_condition(requirement.condition);
            break;
        }
        default:
            break;
    }

    return proto;
}

bool TitleDefinition::CheckRequirement(uint64_t character_id) const {
    // TODO: 检查获取条件
    switch (requirement_type) {
        case RequirementType::ACHIEVEMENT:
            // 检查成就是否完成
            break;
        case RequirementType::LEVEL:
            // 检查角色等级
            break;
        case RequirementType::WEALTH:
            // 检查金币数量
            break;
        case RequirementType::KILL:
            // 检查击杀数量
            break;
        case RequirementType::QUEST:
            // 检查任务完成
            break;
        case RequirementType::SPECIAL:
            // 特殊条件
            break;
        default:
            return true;
    }

    return false;
}

// ========== PlayerTitle ==========

murim::PlayerTitle PlayerTitle::ToProto() const {
    murim::PlayerTitle proto;
    proto.set_title_id(title_id);
    proto.set_owned(owned);
    proto.set_obtain_time(obtain_time);
    proto.set_expire_time(expire_time);
    return proto;
}

// ========== TitleManager ==========

TitleManager& TitleManager::Instance() {
    static TitleManager instance;
    return instance;
}

TitleManager::TitleManager() {
    // 预加载称号定义（TODO: 从数据库加载）
    spdlog::debug("TitleManager initialized");
}

bool TitleManager::Initialize() {
    spdlog::info("Initializing TitleManager...");

    // TODO: 从数据库或配置文件加载称号定义

    // 示例：注册一些基础称号
    TitleDefinition newb_title;
    newb_title.title_id = 2001;
    newb_title.name = "初入江湖";
    newb_title.description = "初入武林世界的新人";
    newb_title.category = murim::TitleCategory::TITLE_NORMAL;
    newb_title.display_location = murim::TitleDisplayLocation::NAME_PREFIX;
    newb_title.permanent = true;
    newb_title.duration_days = 0;
    newb_title.all_stats_bonus = 5;  // 全属性+5

    RegisterTitle(newb_title);

    TitleDefinition elite_title;
    elite_title.title_id = 2002;
    elite_title.name = "武林高手";
    elite_title.description = "武功高强的武林人士";
    elite_title.category = murim::TitleCategory::TITLE_RARE;
    elite_title.display_location = murim::TitleDisplayLocation::NAME_SUFFIX;
    elite_title.permanent = true;
    elite_title.duration_days = 0;
    elite_title.attack_bonus = 10;
    elite_title.defense_bonus = 10;
    elite_title.magic_attack_bonus = 10;
    elite_title.magic_defense_bonus = 10;

    RegisterTitle(elite_title);

    spdlog::info("TitleManager initialized successfully");
    return true;
}

// ========== 称号定义管理 ==========

bool TitleManager::RegisterTitle(const TitleDefinition& definition) {
    if (title_definitions_.find(definition.title_id) != title_definitions_.end()) {
        spdlog::warn("Title already registered: {}", definition.title_id);
        return false;
    }

    title_definitions_[definition.title_id] = definition;
    spdlog::debug("Registered title: {} - {}", definition.title_id, definition.name);
    return true;
}

TitleDefinition* TitleManager::GetTitleDefinition(uint32_t title_id) {
    auto it = title_definitions_.find(title_id);
    if (it != title_definitions_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<TitleDefinition> TitleManager::GetAllTitles() {
    std::vector<TitleDefinition> result;
    for (const auto& [id, definition] : title_definitions_) {
        result.push_back(definition);
    }

    // 按 title_id 排序
    std::sort(result.begin(), result.end(),
        [](const TitleDefinition& a, const TitleDefinition& b) {
            return a.title_id < b.title_id;
        });

    return result;
}

std::vector<TitleDefinition> TitleManager::GetTitlesByCategory(
    murim::TitleCategory category) {

    std::vector<TitleDefinition> result;
    for (const auto& [id, definition] : title_definitions_) {
        if (definition.category == category) {
            result.push_back(definition);
        }
    }

    // 按 title_id 排序
    std::sort(result.begin(), result.end(),
        [](const TitleDefinition& a, const TitleDefinition& b) {
            return a.title_id < b.title_id;
        });

    return result;
}

// ========== 玩家称号管理 ==========

std::vector<PlayerTitle>* TitleManager::GetPlayerTitles(uint64_t character_id) {
    auto it = player_titles_.find(character_id);
    if (it != player_titles_.end()) {
        return &it->second;
    }

    // 创建新列表
    player_titles_[character_id] = std::vector<PlayerTitle>();
    return &player_titles_[character_id];
}

bool TitleManager::HasTitle(uint64_t character_id, uint32_t title_id) {
    auto* titles = GetPlayerTitles(character_id);
    if (!titles) {
        return false;
    }

    for (const auto& title : *titles) {
        if (title.title_id == title_id && title.owned) {
            // 检查是否过期
            if (title.expire_time != 0) {
                auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                if (now >= static_cast<time_t>(title.expire_time)) {
                    return false;  // 已过期
                }
            }
            return true;
        }
    }

    return false;
}

bool TitleManager::GrantTitle(uint64_t character_id, uint32_t title_id, uint32_t duration_days) {
    auto* definition = GetTitleDefinition(title_id);
    if (!definition) {
        spdlog::warn("Title not found: {}", title_id);
        return false;
    }

    auto* titles = GetPlayerTitles(character_id);

    // 检查是否已拥有
    if (HasTitle(character_id, title_id)) {
        spdlog::debug("Character already has title: character_id={}, title_id={}",
                     character_id, title_id);
        return true;
    }

    // 创建新称号
    PlayerTitle new_title;
    new_title.title_id = title_id;
    new_title.owned = true;
    new_title.obtain_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    if (!definition->permanent && duration_days > 0) {
        // 计算过期时间
        new_title.expire_time = new_title.obtain_time + (duration_days * 24 * 3600);
    } else {
        new_title.expire_time = 0;  // 永久
    }

    titles->push_back(new_title);

    spdlog::info("Granted title: character_id={}, title_id={}, name={}",
                 character_id, title_id, definition->name);

    // 发送通知
    NotifyTitleObtained(character_id, *definition);

    return true;
}

bool TitleManager::RemoveTitle(uint64_t character_id, uint32_t title_id) {
    auto* titles = GetPlayerTitles(character_id);
    if (!titles) {
        return false;
    }

    auto it = std::find_if(titles->begin(), titles->end(),
        [title_id](const PlayerTitle& title) {
            return title.title_id == title_id;
        });

    if (it != titles->end()) {
        titles->erase(it);
        spdlog::info("Removed title: character_id={}, title_id={}", character_id, title_id);
        return true;
    }

    return false;
}

// ========== 称号装备 ==========

bool TitleManager::EquipTitle(uint64_t character_id, uint32_t title_id) {
    if (!HasTitle(character_id, title_id)) {
        spdlog::warn("Character doesn't have title: character_id={}, title_id={}",
                     character_id, title_id);
        return false;
    }

    if (!CanEquip(character_id, title_id)) {
        spdlog::warn("Cannot equip title: character_id={}, title_id={}",
                     character_id, title_id);
        return false;
    }

    // 移除旧称号的属性加成
    uint32_t old_title_id = GetEquippedTitle(character_id);
    if (old_title_id != 0) {
        RemoveTitleBonus(character_id);
    }

    // 装备新称号
    equipped_titles_[character_id] = title_id;

    // 应用新称号的属性加成
    ApplyTitleBonus(character_id);

    spdlog::info("Equipped title: character_id={}, title_id={}", character_id, title_id);

    return true;
}

bool TitleManager::UnequipTitle(uint64_t character_id) {
    auto it = equipped_titles_.find(character_id);
    if (it == equipped_titles_.end() || it->second == 0) {
        spdlog::warn("No title equipped: character_id={}", character_id);
        return false;
    }

    // 移除属性加成
    RemoveTitleBonus(character_id);

    // 清除装备
    equipped_titles_.erase(it);

    spdlog::info("Unequipped title: character_id={}", character_id);

    return true;
}

uint32_t TitleManager::GetEquippedTitle(uint64_t character_id) {
    auto it = equipped_titles_.find(character_id);
    if (it != equipped_titles_.end()) {
        return it->second;
    }
    return 0;  // 未装备称号
}

std::string TitleManager::GetDisplayName(uint64_t character_id, const std::string& character_name) {
    uint32_t equipped_title_id = GetEquippedTitle(character_id);
    if (equipped_title_id == 0) {
        return character_name;
    }

    auto* definition = GetTitleDefinition(equipped_title_id);
    if (!definition) {
        return character_name;
    }

    // 根据显示位置组合名称
    std::string display_name;
    if (definition->display_location == murim::TitleDisplayLocation::NAME_PREFIX) {
        display_name = "[" + definition->name + "] " + character_name;
    } else if (definition->display_location == murim::TitleDisplayLocation::NAME_SUFFIX) {
        display_name = character_name + " [" + definition->name + "]";
    } else {
        display_name = character_name;
    }

    return display_name;
}

// ========== 称号属性加成 ==========

void TitleManager::CalculateTitleBonus(uint64_t character_id,
                                      int32_t& hp_bonus,
                                      int32_t& mp_bonus,
                                      int32_t& attack_bonus,
                                      int32_t& defense_bonus) {

    uint32_t equipped_title_id = GetEquippedTitle(character_id);
    if (equipped_title_id == 0) {
        return;
    }

    auto* definition = GetTitleDefinition(equipped_title_id);
    if (!definition) {
        return;
    }

    // 累加属性加成
    hp_bonus += definition->hp_bonus;
    mp_bonus += definition->mp_bonus;
    attack_bonus += definition->attack_bonus;
    defense_bonus += definition->defense_bonus;

    // 全属性加成
    if (definition->all_stats_bonus != 0) {
        hp_bonus += definition->all_stats_bonus;
        mp_bonus += definition->all_stats_bonus;
        attack_bonus += definition->all_stats_bonus;
        defense_bonus += definition->all_stats_bonus;
    }
}

void TitleManager::ApplyTitleBonus(uint64_t character_id) {
    // TODO: 应用称号属性加成到角色
    spdlog::debug("Applying title bonus: character_id={}", character_id);
}

void TitleManager::RemoveTitleBonus(uint64_t character_id) {
    // TODO: 移除称号属性加成
    spdlog::debug("Removing title bonus: character_id={}", character_id);
}

// ========== 维护操作 ==========

void TitleManager::CheckExpiredTitles() {
    spdlog::debug("Checking expired titles...");

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    for (auto& [character_id, titles] : player_titles_) {
        std::vector<uint32_t> expired_titles;

        for (auto& title : titles) {
            if (title.expire_time != 0 && now >= static_cast<time_t>(title.expire_time)) {
                expired_titles.push_back(title.title_id);
            }
        }

        // 移除过期称号
        for (uint32_t title_id : expired_titles) {
            auto* definition = GetTitleDefinition(title_id);
            spdlog::info("Title expired: character_id={}, title_id={}",
                        character_id, title_id);

            RemoveTitle(character_id, title_id);

            if (definition) {
                NotifyTitleExpired(character_id, *definition);
            }

            // 如果装备的是过期称号，自动卸载
            if (GetEquippedTitle(character_id) == title_id) {
                UnequipTitle(character_id);
            }
        }
    }
}

bool TitleManager::SavePlayerTitles(uint64_t character_id) {
    // TODO: 保存到数据库
    spdlog::debug("Saving player titles: character_id={}", character_id);
    return true;
}

bool TitleManager::LoadPlayerTitles(uint64_t character_id) {
    // TODO: 从数据库加载
    spdlog::debug("Loading player titles: character_id={}", character_id);
    return true;
}

// ========== 辅助方法 ==========

bool TitleManager::CanEquip(uint64_t character_id, uint32_t title_id) {
    // TODO: 检查称号是否可装备（等级要求等）
    return true;
}

void TitleManager::NotifyTitleObtained(uint64_t character_id, const TitleDefinition& title) {
    // TODO: 发送称号获取通知给客户端
    spdlog::info("Notification: Title obtained - character_id={}, name={}",
                character_id, title.name);
}

void TitleManager::NotifyTitleExpired(uint64_t character_id, const TitleDefinition& title) {
    // TODO: 发送称号过期通知给客户端
    spdlog::info("Notification: Title expired - character_id={}, name={}",
                character_id, title.name);
}

} // namespace Game
} // namespace Murim
