#include "AchievementManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>
#include <chrono>

namespace Murim {
namespace Game {

// ========== AchievementDefinition ==========

murim::AchievementDefinition AchievementDefinition::ToProto() const {
    murim::AchievementDefinition proto;
    proto.set_achievement_id(achievement_id);
    proto.set_name(name);
    proto.set_description(description);
    proto.set_category(category);
    proto.set_points(points);
    proto.set_prerequisite_id(prerequisite_id);
    proto.set_hidden(hidden);
    proto.set_serial(serial);

    // 设置条件
    switch (condition_type) {
        case ConditionType::LEVEL: {
            auto* cond = proto.mutable_level();
            cond->set_target_level(condition.target_level);
            break;
        }
        case ConditionType::KILL: {
            auto* cond = proto.mutable_kill();
            cond->set_target_type_id(condition.target_type_id);
            cond->set_target_count(condition.target_count);
            cond->set_min_level(condition.min_level);
            break;
        }
        case ConditionType::QUEST: {
            auto* cond = proto.mutable_quest();
            cond->set_quest_count(condition.quest_count);
            cond->set_quest_type(condition.quest_type);
            break;
        }
        case ConditionType::ITEM: {
            auto* cond = proto.mutable_item();
            cond->set_item_count(condition.item_count);
            cond->set_item_quality(condition.item_quality);
            break;
        }
        case ConditionType::WEALTH: {
            auto* cond = proto.mutable_wealth();
            cond->set_target_gold(condition.target_gold);
            break;
        }
        case ConditionType::DUNGEON: {
            auto* cond = proto.mutable_dungeon();
            cond->set_dungeon_id(condition.dungeon_id);
            cond->set_clear_count(condition.clear_count);
            cond->set_max_clear_time(condition.max_clear_time);
            break;
        }
        case ConditionType::SOCIAL: {
            auto* cond = proto.mutable_social();
            cond->set_friend_count(condition.friend_count);
            cond->set_guild_member_count(condition.guild_member_count);
            break;
        }
        default:
            break;
    }

    // 设置奖励
    for (const auto& reward : rewards) {
        auto* proto_reward = proto.add_rewards();
        *proto_reward = reward;
    }

    return proto;
}

// ========== AchievementProgress ==========

murim::AchievementProgress AchievementProgress::ToProto() const {
    murim::AchievementProgress proto;
    proto.set_achievement_id(achievement_id);
    proto.set_status(status);
    proto.set_current_value(current_value);
    proto.set_target_value(target_value);
    proto.set_complete_time(complete_time);
    proto.set_update_time(update_time);
    return proto;
}

// ========== AchievementManager ==========

AchievementManager& AchievementManager::Instance() {
    static AchievementManager instance;
    return instance;
}

AchievementManager::AchievementManager() {
    // 预加载成就定义（TODO: 从数据库加载）
    spdlog::debug("AchievementManager initialized");
}

bool AchievementManager::Initialize() {
    spdlog::info("Initializing AchievementManager...");

    // TODO: 从数据库或配置文件加载成就定义

    // 示例：注册一些基础成就
    AchievementDefinition level_10;
    level_10.achievement_id = 1001;
    level_10.name = "初出茅庐";
    level_10.description = "达到10级";
    level_10.category = murim::AchievementCategory::LEVEL;
    level_10.points = 10;
    level_10.prerequisite_id = 0;
    level_10.hidden = false;
    level_10.serial = false;
    level_10.condition_type = AchievementDefinition::ConditionType::LEVEL;
    level_10.condition.target_level = 10;

    murim::AchievementReward reward1;
    reward1.set_type(murim::AchievementRewardType::REWARD_EXPERIENCE);
    reward1.set_exp(1000);
    level_10.rewards.push_back(reward1);

    RegisterAchievement(level_10);

    AchievementDefinition level_50;
    level_50.achievement_id = 1002;
    level_50.name = "小有所成";
    level_50.description = "达到50级";
    level_50.category = murim::AchievementCategory::LEVEL;
    level_50.points = 50;
    level_50.prerequisite_id = 1001;  // 需要先完成"初出茅庐"
    level_50.hidden = false;
    level_50.serial = false;
    level_50.condition_type = AchievementDefinition::ConditionType::LEVEL;
    level_50.condition.target_level = 50;

    murim::AchievementReward reward2;
    reward2.set_type(murim::AchievementRewardType::REWARD_GOLD);
    reward2.set_gold(10000);
    level_50.rewards.push_back(reward2);

    RegisterAchievement(level_50);

    spdlog::info("AchievementManager initialized successfully");
    return true;
}

// ========== 成就定义管理 ==========

bool AchievementManager::RegisterAchievement(const AchievementDefinition& definition) {
    if (achievement_definitions_.find(definition.achievement_id) !=
        achievement_definitions_.end()) {
        spdlog::warn("Achievement already registered: {}", definition.achievement_id);
        return false;
    }

    achievement_definitions_[definition.achievement_id] = definition;
    spdlog::debug("Registered achievement: {} - {}",
                  definition.achievement_id, definition.name);
    return true;
}

AchievementDefinition* AchievementManager::GetAchievementDefinition(uint32_t achievement_id) {
    auto it = achievement_definitions_.find(achievement_id);
    if (it != achievement_definitions_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<AchievementDefinition> AchievementManager::GetAchievementsByCategory(
    murim::AchievementCategory category) {

    std::vector<AchievementDefinition> result;
    for (const auto& [id, definition] : achievement_definitions_) {
        if (definition.category == category) {
            result.push_back(definition);
        }
    }

    // 按 achievement_id 排序
    std::sort(result.begin(), result.end(),
        [](const AchievementDefinition& a, const AchievementDefinition& b) {
            return a.achievement_id < b.achievement_id;
        });

    return result;
}

// ========== 成就进度查询 ==========

PlayerAchievement* AchievementManager::GetPlayerAchievement(uint64_t character_id) const {
    auto it = player_achievements_.find(character_id);
    if (it != player_achievements_.end()) {
        return const_cast<PlayerAchievement*>(&it->second);
    }
    return nullptr;
}

PlayerAchievement& AchievementManager::GetOrCreatePlayerAchievement(uint64_t character_id) {
    auto it = player_achievements_.find(character_id);
    if (it == player_achievements_.end()) {
        PlayerAchievement player_ach;
        player_ach.character_id = character_id;
        player_ach.total_points = 0;
        player_ach.completed_count = 0;

        player_achievements_[character_id] = player_ach;
        return player_achievements_[character_id];
    }
    return it->second;
}

AchievementProgress* AchievementManager::GetAchievementProgress(
    uint64_t character_id,
    uint32_t achievement_id) {

    auto* player_ach = GetPlayerAchievement(character_id);
    if (!player_ach) {
        return nullptr;
    }

    return player_ach->GetProgress(achievement_id);
}

// ========== 成就进度更新 ==========

void AchievementManager::UpdateLevelProgress(
    uint64_t character_id,
    uint32_t level) {

    auto& player_ach = GetOrCreatePlayerAchievement(character_id);

    // 查找所有等级类成就
    for (const auto& [id, definition] : achievement_definitions_) {
        if (definition.category != murim::AchievementCategory::LEVEL) {
            continue;
        }

        if (definition.condition_type != AchievementDefinition::ConditionType::LEVEL) {
            continue;
        }

        // 检查前置条件
        if (definition.prerequisite_id != 0) {
            auto* prereq_progress = player_ach.GetProgress(definition.prerequisite_id);
            if (!prereq_progress ||
                prereq_progress->status != murim::AchievementStatus::COMPLETED) {
                continue;  // 前置成就未完成
            }
        }

        // 获取或创建进度
        auto* progress = player_ach.GetProgress(definition.achievement_id);
        if (!progress) {
            AchievementProgress new_progress;
            new_progress.achievement_id = definition.achievement_id;
            new_progress.status = murim::AchievementStatus::LOCKED;
            new_progress.current_value = 0;
            new_progress.target_value = definition.condition.target_level;
            new_progress.complete_time = 0;
            new_progress.update_time = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());

            player_ach.AddOrUpdateProgress(new_progress);
            progress = player_ach.GetProgress(definition.achievement_id);
        }

        // 检查是否解锁
        if (progress->status == murim::AchievementStatus::LOCKED) {
            // TODO: 检查其他解锁条件
            progress->status = murim::AchievementStatus::IN_PROGRESS;
        }

        // 更新进度
        if (progress->status == murim::AchievementStatus::IN_PROGRESS) {
            uint32_t old_value = progress->current_value;
            progress->current_value = level;
            progress->update_time = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());

            // 检查是否完成
            if (progress->current_value >= progress->target_value) {
                CheckAndCompleteAchievement(character_id, definition.achievement_id);
            } else {
                NotifyProgressUpdate(character_id, definition, old_value, level);
            }
        }
    }
}

void AchievementManager::UpdateKillProgress(
    uint64_t character_id,
    uint32_t monster_type_id,
    uint32_t count) {

    auto& player_ach = GetOrCreatePlayerAchievement(character_id);

    // 查找击杀类成就
    for (const auto& [id, definition] : achievement_definitions_) {
        if (definition.category != murim::AchievementCategory::COMBAT) {
            continue;
        }

        if (definition.condition_type != AchievementDefinition::ConditionType::KILL) {
            continue;
        }

        // 检查目标类型匹配
        if (definition.condition.target_type_id != 0 &&
            definition.condition.target_type_id != monster_type_id) {
            continue;
        }

        auto* progress = player_ach.GetProgress(definition.achievement_id);
        if (!progress) {
            AchievementProgress new_progress;
            new_progress.achievement_id = definition.achievement_id;
            new_progress.status = murim::AchievementStatus::IN_PROGRESS;
            new_progress.current_value = 0;
            new_progress.target_value = definition.condition.target_count;
            new_progress.complete_time = 0;
            new_progress.update_time = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());

            player_ach.AddOrUpdateProgress(new_progress);
            progress = player_ach.GetProgress(definition.achievement_id);
        }

        if (progress->status == murim::AchievementStatus::IN_PROGRESS) {
            uint32_t old_value = progress->current_value;
            progress->current_value += count;
            progress->update_time = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());

            if (progress->current_value >= progress->target_value) {
                CheckAndCompleteAchievement(character_id, definition.achievement_id);
            } else {
                NotifyProgressUpdate(character_id, definition, old_value,
                                    progress->current_value);
            }
        }
    }
}

void AchievementManager::UpdateQuestProgress(uint64_t character_id) {
    // TODO: 实现任务成就进度更新
    spdlog::debug("UpdateQuestProgress: character_id={}", character_id);
}

void AchievementManager::UpdateWealthProgress(uint64_t character_id, uint64_t gold) {
    auto& player_ach = GetOrCreatePlayerAchievement(character_id);

    // 查找财富类成就
    for (const auto& [id, definition] : achievement_definitions_) {
        if (definition.category != murim::AchievementCategory::WEALTH) {
            continue;
        }

        if (definition.condition_type != AchievementDefinition::ConditionType::WEALTH) {
            continue;
        }

        auto* progress = player_ach.GetProgress(definition.achievement_id);
        if (!progress) {
            AchievementProgress new_progress;
            new_progress.achievement_id = definition.achievement_id;
            new_progress.status = murim::AchievementStatus::IN_PROGRESS;
            new_progress.current_value = 0;
            new_progress.target_value = static_cast<uint32_t>(definition.condition.target_gold);
            new_progress.complete_time = 0;
            new_progress.update_time = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());

            player_ach.AddOrUpdateProgress(new_progress);
            progress = player_ach.GetProgress(definition.achievement_id);
        }

        if (progress->status == murim::AchievementStatus::IN_PROGRESS) {
            uint32_t old_value = progress->current_value;
            progress->current_value = static_cast<uint32_t>(gold);
            progress->update_time = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());

            if (progress->current_value >= progress->target_value) {
                CheckAndCompleteAchievement(character_id, definition.achievement_id);
            } else {
                NotifyProgressUpdate(character_id, definition, old_value,
                                    progress->current_value);
            }
        }
    }
}

void AchievementManager::UpdateDungeonProgress(
    uint64_t character_id,
    uint32_t dungeon_id,
    uint32_t clear_time) {

    // TODO: 实现副本成就进度更新
    spdlog::debug("UpdateDungeonProgress: character_id={}, dungeon_id={}, clear_time={}",
                  character_id, dungeon_id, clear_time);
}

void AchievementManager::UpdateSocialProgress(
    uint64_t character_id,
    uint32_t friend_count,
    uint32_t guild_member_count) {

    // TODO: 实现社交成就进度更新
    spdlog::debug("UpdateSocialProgress: character_id={}, friends={}, guild_members={}",
                  character_id, friend_count, guild_member_count);
}

// ========== 成就完成检查 ==========

bool AchievementManager::CheckAndCompleteAchievement(
    uint64_t character_id,
    uint32_t achievement_id) {

    auto* definition = GetAchievementDefinition(achievement_id);
    if (!definition) {
        spdlog::warn("Achievement not found: {}", achievement_id);
        return false;
    }

    auto* player_ach = GetPlayerAchievement(character_id);
    if (!player_ach) {
        return false;
    }

    auto* progress = player_ach->GetProgress(achievement_id);
    if (!progress) {
        return false;
    }

    if (progress->status == murim::AchievementStatus::COMPLETED ||
        progress->status == murim::AchievementStatus::CLAIMED) {
        return true;  // 已完成
    }

    // 检查条件
    if (!CheckCondition(*definition, progress->current_value)) {
        return false;
    }

    // 标记为已完成
    progress->status = murim::AchievementStatus::COMPLETED;
    progress->complete_time = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    progress->update_time = progress->complete_time;

    // 更新统计
    player_ach->total_points += definition->points;
    player_ach->completed_count++;

    spdlog::info("Achievement completed: character_id={}, achievement_id={}, name={}",
                 character_id, achievement_id, definition->name);

    // 发送完成通知
    NotifyCompleted(character_id, *definition);

    // 解锁后续成就
    UnlockDependentAchievements(character_id, achievement_id);

    return true;
}

bool AchievementManager::ClaimReward(uint64_t character_id, uint32_t achievement_id) {
    auto* definition = GetAchievementDefinition(achievement_id);
    if (!definition) {
        return false;
    }

    auto* player_ach = GetPlayerAchievement(character_id);
    if (!player_ach) {
        return false;
    }

    auto* progress = player_ach->GetProgress(achievement_id);
    if (!progress) {
        return false;
    }

    if (progress->status != murim::AchievementStatus::COMPLETED) {
        spdlog::warn("Achievement not completed: {}", achievement_id);
        return false;
    }

    if (progress->status == murim::AchievementStatus::CLAIMED) {
        spdlog::warn("Achievement reward already claimed: {}", achievement_id);
        return false;
    }

    // TODO: 发放奖励
    spdlog::info("Claiming achievement reward: character_id={}, achievement_id={}",
                 character_id, achievement_id);

    // 标记为已领取
    progress->status = murim::AchievementStatus::CLAIMED;

    return true;
}

bool AchievementManager::IsCompleted(uint64_t character_id, uint32_t achievement_id) const {
    auto* player_ach = GetPlayerAchievement(character_id);
    if (!player_ach) {
        return false;
    }

    auto* progress = player_ach->GetProgress(achievement_id);
    if (!progress) {
        return false;
    }

    return progress->status == murim::AchievementStatus::COMPLETED ||
           progress->status == murim::AchievementStatus::CLAIMED;
}

bool AchievementManager::IsClaimed(uint64_t character_id, uint32_t achievement_id) const {
    auto* player_ach = GetPlayerAchievement(character_id);
    if (!player_ach) {
        return false;
    }

    auto* progress = player_ach->GetProgress(achievement_id);
    if (!progress) {
        return false;
    }

    return progress->status == murim::AchievementStatus::CLAIMED;
}

// ========== 成就统计 ==========

uint32_t AchievementManager::GetTotalPoints(uint64_t character_id) {
    auto* player_ach = GetPlayerAchievement(character_id);
    if (!player_ach) {
        return 0;
    }
    return player_ach->total_points;
}

uint32_t AchievementManager::GetCompletedCount(uint64_t character_id) {
    auto* player_ach = GetPlayerAchievement(character_id);
    if (!player_ach) {
        return 0;
    }
    return player_ach->completed_count;
}

uint32_t AchievementManager::GetCategoryCount(
    uint64_t character_id,
    murim::AchievementCategory category) {

    auto* player_ach = GetPlayerAchievement(character_id);
    if (!player_ach) {
        return 0;
    }

    uint32_t count = 0;
    for (const auto& [id, progress] : player_ach->progress) {
        auto* definition = GetAchievementDefinition(id);
        if (definition && definition->category == category) {
            if (progress.status == murim::AchievementStatus::COMPLETED ||
                progress.status == murim::AchievementStatus::CLAIMED) {
                count++;
            }
        }
    }

    return count;
}

// ========== 维护操作 ==========

bool AchievementManager::SavePlayerAchievement(uint64_t character_id) {
    // TODO: 保存到数据库
    spdlog::debug("Saving player achievement: character_id={}", character_id);
    return true;
}

bool AchievementManager::LoadPlayerAchievement(uint64_t character_id) {
    // TODO: 从数据库加载
    spdlog::debug("Loading player achievement: character_id={}", character_id);
    return true;
}

// ========== 辅助方法 ==========

bool AchievementManager::CheckCondition(
    const AchievementDefinition& definition,
    uint32_t current_value) {

    switch (definition.condition_type) {
        case AchievementDefinition::ConditionType::LEVEL:
            return current_value >= definition.condition.target_level;
        case AchievementDefinition::ConditionType::KILL:
            return current_value >= definition.condition.target_count;
        case AchievementDefinition::ConditionType::QUEST:
            return current_value >= definition.condition.quest_count;
        case AchievementDefinition::ConditionType::WEALTH:
            return current_value >= static_cast<uint32_t>(definition.condition.target_gold);
        default:
            return false;
    }
}

void AchievementManager::NotifyCompleted(
    uint64_t character_id,
    const AchievementDefinition& definition) {

    // TODO: 发送成就完成通知给客户端
    spdlog::info("Notification: Achievement completed - character_id={}, name={}",
                 character_id, definition.name);
}

void AchievementManager::NotifyProgressUpdate(
    uint64_t character_id,
    const AchievementDefinition& definition,
    uint32_t old_value,
    uint32_t new_value) {

    // TODO: 发送进度更新通知给客户端
    spdlog::debug("Notification: Achievement progress - character_id={}, name={}, {}/{}",
                 character_id, definition.name, new_value, definition.condition.target_level);
}

void AchievementManager::UnlockDependentAchievements(
    uint64_t character_id,
    uint32_t achievement_id) {

    auto& player_ach = GetOrCreatePlayerAchievement(character_id);

    // 查找以当前成就为前置的成就
    for (const auto& [id, definition] : achievement_definitions_) {
        if (definition.prerequisite_id == achievement_id) {
            auto* progress = player_ach.GetProgress(definition.achievement_id);
            if (progress && progress->status == murim::AchievementStatus::LOCKED) {
                progress->status = murim::AchievementStatus::IN_PROGRESS;
                spdlog::info("Unlocked achievement: character_id={}, achievement_id={}",
                            character_id, definition.achievement_id);
            }
        }
    }
}

} // namespace Game
} // namespace Murim
