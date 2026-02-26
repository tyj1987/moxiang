#include "Quest.hpp"
#include "core/network/NotificationService.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>
#include <fmt/core.h>
#include "core/database/QuestDAO.hpp"
#include "shared/character/CharacterManager.hpp"
#include "shared/item/ItemManager.hpp"
#include "shared/skill/SkillManager.hpp"
#include "shared/skill/Skill.hpp"

// using Database = Murim::Core::Database;  // Removed - causes parsing errors
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Game {

// ========== QuestManager ==========

QuestManager& QuestManager::Instance() {
    static QuestManager instance;
    return instance;
}

void QuestManager::Process() {
    // 处理任务时间检查、事件处理等 (暂时为空)
}

void QuestManager::Initialize() {
    if (initialized_) {
        return;
    }

    spdlog::info("Initializing QuestManager...");

    LoadQuests();

    initialized_ = true;
    spdlog::info("QuestManager initialized: {} quests loaded", quests_.size());
}

void QuestManager::LoadQuests() {
    // 暂时跳过数据库加载，直接使用默认任务（避免数据库问题导致启动失败）
    /*
    // 从数据库加载所有任务模板
    auto all_quests = Murim::Core::Database::QuestDAO::LoadAll();

    for (const auto& quest : all_quests) {
        quests_[quest.quest_id] = quest;
    }
    */

    // 使用默认任务 (用于测试)
    if (quests_.empty()) {
        spdlog::warn("Using default hardcoded quests (database loading disabled)");

        // 示例: 新手任务
        Quest newbie_quest;
        newbie_quest.quest_id = 1;
        newbie_quest.name = "新手训练";
        newbie_quest.description = "完成基础训练任务";
        newbie_quest.type = QuestType::kMain;
        newbie_quest.required_level = 1;
        newbie_quest.status = QuestStatus::kNotStarted;
        newbie_quest.current_sub_quest = 0;

        // 子任务 1: 击杀 5 只史莱姆
        SubQuest sub1;
        sub1.sub_quest_id = 101;
        sub1.title = "击杀史莱姆";
        sub1.description = "击杀 5 只史莱姆";
        sub1.status = QuestStatus::kNotStarted;

        QuestCondition cond1;
        cond1.condition_id = 1;
        cond1.event_type = QuestEventType::kMonsterKill;
        cond1.target_id = 1001;  // 史莱姆 ID
        cond1.required_count = 5;
        cond1.current_count = 0;

        sub1.conditions.push_back(cond1);

        // 奖励
        QuestReward reward1;
        reward1.exp = 100;
        reward1.money = 50;
        sub1.rewards.push_back(reward1);

        newbie_quest.sub_quests.push_back(sub1);

        quests_[newbie_quest.quest_id] = newbie_quest;
    }

    spdlog::info("Loaded {} quests", quests_.size());
}

// ========== 任务查询 ==========

std::optional<Quest> QuestManager::GetQuest(uint32_t quest_id) {
    auto it = quests_.find(quest_id);
    if (it != quests_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<Quest> QuestManager::GetCharacterQuests(uint64_t character_id) {
    // 检查缓存是否存在且未过期
    auto cache_it = character_quests_cache_.find(character_id);
    if (cache_it != character_quests_cache_.end() && !IsCacheExpired(character_id)) {
        cache_hits_++;
        spdlog::debug("Using cached quests for character {}", character_id);
        return cache_it->second;
    }

    // 缓存未命中，从数据库加载
    cache_misses_++;
    auto quests = Murim::Core::Database::QuestDAO::LoadCharacterQuests(character_id);

    // 更新缓存
    character_quests_cache_[character_id] = quests;
    cache_timestamps_[character_id] = std::chrono::system_clock::now();

    spdlog::debug("Loaded {} quests for character {} from database", quests.size(), character_id);

    return quests;
}

std::vector<Quest> QuestManager::GetAvailableQuests(uint64_t character_id, uint16_t player_level) {
    std::vector<Quest> available_quests;

    for (const auto& [quest_id, quest] : quests_) {
        if (quest.CanAccept(player_level)) {
            available_quests.push_back(quest);
        }
    }

    return available_quests;
}

// ========== 任务操作 ==========

bool QuestManager::AcceptQuest(uint64_t character_id, uint32_t quest_id) {
    auto quest_opt = GetQuest(quest_id);
    if (!quest_opt.has_value()) {
        spdlog::warn("Quest not found: {}", quest_id);
        return false;
    }

    try {
        // 从数据库加载角色信息 - 对应 legacy 角色数据加载
        auto character = Murim::Core::Database::CharacterDAO::Load(character_id);
        if (!character.has_value()) {
            spdlog::error("Character not found: {}", character_id);
            return false;
        }

        uint16_t player_level = character->level;

        // 检查角色等级 - 对应 legacy 等级检查
        if (player_level < quest_opt->required_level) {
            spdlog::warn("Character level {} < required level {}",
                         player_level, quest_opt->required_level);
            return false;
        }

        // 检查前置任务 - 对应 legacy 前置任务检查
        if (!quest_opt->required_quests.empty()) {
            for (uint32_t prev_quest_id : quest_opt->required_quests) {
                auto prev_quest_opt = Murim::Core::Database::QuestDAO::LoadCharacterQuest(character_id, prev_quest_id);
                if (!prev_quest_opt.has_value() || prev_quest_opt->status != QuestStatus::kCompleted) {
                    spdlog::warn("Prev quest {} not completed", prev_quest_id);
                    return false;
                }
            }
        }

        Quest quest = *quest_opt;
        quest.status = QuestStatus::kInProgress;
        quest.accept_time = std::chrono::system_clock::now();

        // 设置时间限制
        if (quest.time_limit_seconds.has_value()) {
            quest.deadline = quest.accept_time + std::chrono::seconds(quest.time_limit_seconds.value());
        }

        // 保存到数据库 - 对应 legacy 数据库保存
        Murim::Core::Database::QuestDAO::SaveCharacterQuest(character_id, quest);

        // 更新缓存：添加新任务
        auto& cached_quests = character_quests_cache_[character_id];
        cached_quests.push_back(quest);
        cache_timestamps_[character_id] = std::chrono::system_clock::now();
        cache_updates_++;

        spdlog::info("Character {} accepted quest {}", character_id, quest_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to accept quest: {}", e.what());
        return false;
    }
}

bool QuestManager::AbandonQuest(uint64_t character_id, uint32_t quest_id) {
    try {
        // 从数据库加载角色任务 - 对应 legacy 任务加载
        auto quests = GetCharacterQuests(character_id);

        auto it = std::find_if(quests.begin(), quests.end(),
            [quest_id](const Quest& q) { return q.quest_id == quest_id; });

        if (it == quests.end()) {
            spdlog::warn("Character {} does not have quest {}", character_id, quest_id);
            return false;
        }

        if (!it->CanAbandon()) {
            spdlog::warn("Cannot abandon quest {}", quest_id);
            return false;
        }

        // 从数据库删除 - 对应 legacy 数据库删除
        Murim::Core::Database::QuestDAO::DeleteCharacterQuest(character_id, quest_id);

        spdlog::info("Character {} abandoned quest {}", character_id, quest_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to abandon quest: {}", e.what());
        return false;
    }
}

bool QuestManager::CompleteQuest(uint64_t character_id, uint32_t quest_id) {
    try {
        // 从数据库加载角色任务 - 对应 legacy 任务加载
        auto quests = GetCharacterQuests(character_id);

        auto it = std::find_if(quests.begin(), quests.end(),
            [quest_id](const Quest& q) { return q.quest_id == quest_id; });

        if (it == quests.end()) {
            spdlog::warn("Character {} does not have quest {}", character_id, quest_id);
            return false;
        }

        // 检查所有子任务是否完成
        bool all_completed = true;
        for (const auto& sub_quest : it->sub_quests) {
            if (!sub_quest.IsCompleted()) {
                all_completed = false;
                break;
            }
        }

        if (!all_completed) {
            spdlog::warn("Quest {} is not completed yet", quest_id);
            return false;
        }

        it->status = QuestStatus::kCompleted;
        it->complete_time = std::chrono::system_clock::now();

        // 保存到数据库 - 对应 legacy 数据库保存
        Murim::Core::Database::QuestDAO::UpdateQuestStatus(
            character_id,
            quest_id,
            QuestStatus::kCompleted
        );

        spdlog::info("Character {} completed quest {}", character_id, quest_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to complete quest: {}", e.what());
        return false;
    }
}

bool QuestManager::TurnInQuest(uint64_t character_id, uint32_t quest_id, uint64_t npc_id) {
    auto quests = GetCharacterQuests(character_id);

    auto it = std::find_if(quests.begin(), quests.end(),
        [quest_id](const Quest& q) { return q.quest_id == quest_id; });

    if (it == quests.end()) {
        spdlog::warn("Character {} does not have quest {}", character_id, quest_id);
        return false;
    }

    if (it->status != QuestStatus::kCompleted) {
        spdlog::warn("Quest {} is not completed yet", quest_id);
        return false;
    }

    // 给予奖励
    for (const auto& sub_quest : it->sub_quests) {
        GiveQuestRewards(character_id, *it, sub_quest);
    }

    spdlog::info("Character {} turned in quest {} to NPC {}",
                 character_id, quest_id, npc_id);

    return true;
}

// ========== 任务进度 ==========

void QuestManager::UpdateQuestProgress(
    uint64_t character_id,
    QuestEventType event_type,
    uint32_t target_id,
    uint32_t count
) {
    auto quests = GetCharacterQuests(character_id);

    bool updated = false;

    for (auto& quest : quests) {
        if (quest.status != QuestStatus::kInProgress) {
            continue;
        }

        SubQuest* current_sub = quest.GetCurrentSubQuest();
        if (!current_sub) {
            continue;
        }

        // 更新匹配的条件
        for (auto& condition : current_sub->conditions) {
            if (condition.event_type == event_type &&
                condition.target_id == target_id) {
                condition.current_count += count;
                updated = true;

                spdlog::debug("Quest progress updated: quest={}, sub_quest={}, "
                              "condition={}/{}/{}",
                              quest.quest_id, current_sub->sub_quest_id,
                              condition.condition_id, condition.current_count,
                              condition.required_count);
            }
        }

        if (updated) {
            // 检查子任务是否完成
            if (current_sub->IsCompleted()) {
                spdlog::info("Sub quest {} completed", current_sub->sub_quest_id);
                current_sub->status = QuestStatus::kCompleted;
                current_sub->complete_time = std::chrono::system_clock::now();

                // 移动到下一个子任务
                quest.current_sub_quest++;

                // 检查是否所有子任务都完成
                if (quest.current_sub_quest >= quest.sub_quests.size()) {
                    quest.status = QuestStatus::kCompleted;
                    spdlog::info("Quest {} completed", quest.quest_id);
                }
            }

            // 保存进度到数据库（无论子任务是否完成都要保存）
            SaveQuestProgress(character_id, quest);

            // 更新缓存：直接修改内存中的任务
            auto& cached_quests = character_quests_cache_[character_id];
            for (auto& cached_quest : cached_quests) {
                if (cached_quest.quest_id == quest.quest_id) {
                    cached_quest = quest;  // 更新缓存中的任务
                    break;
                }
            }
            cache_timestamps_[character_id] = std::chrono::system_clock::now();
            cache_updates_++;
        }
    }
}

void QuestManager::CheckQuestCompletion(uint64_t character_id, uint32_t quest_id) {
    auto quests = GetCharacterQuests(character_id);

    auto it = std::find_if(quests.begin(), quests.end(),
        [quest_id](const Quest& q) { return q.quest_id == quest_id; });

    if (it == quests.end()) {
        return;
    }

    SubQuest* current_sub = it->GetCurrentSubQuest();
    if (current_sub && current_sub->IsCompleted()) {
        current_sub->status = QuestStatus::kCompleted;
        it->current_sub_quest++;

        if (it->current_sub_quest >= it->sub_quests.size()) {
            it->status = QuestStatus::kCompleted;
        }

        SaveQuestProgress(character_id, *it);
    }
}

// ========== 任务事件 ==========

void QuestManager::OnNpcTalk(uint64_t character_id, uint64_t npc_id) {
    UpdateQuestProgress(character_id, QuestEventType::kNpcTalk, static_cast<uint32_t>(npc_id), 1);
}

void QuestManager::OnMonsterKill(uint64_t character_id, uint32_t monster_id) {
    UpdateQuestProgress(character_id, QuestEventType::kMonsterKill, monster_id, 1);
}

void QuestManager::OnItemCollect(uint64_t character_id, uint32_t item_id, uint32_t count) {
    UpdateQuestProgress(character_id, QuestEventType::kItemCollect, item_id, count);
}

void QuestManager::OnLevelUp(uint64_t character_id, uint16_t new_level) {
    UpdateQuestProgress(character_id, QuestEventType::kLevelReach, new_level, 1);
}

void QuestManager::OnMapEnter(uint64_t character_id, uint32_t map_id) {
    UpdateQuestProgress(character_id, QuestEventType::kMapEnter, map_id, 1);
}

// ========== 任务执行 ==========

bool QuestManager::ExecuteQuestAction(
    uint64_t character_id,
    QuestExecuteType action_type,
    const std::vector<uint32_t>& params
) {
    switch (action_type) {
        case QuestExecuteType::kStartQuest:
            if (params.size() < 1) {
                return false;
            }
            return AcceptQuest(character_id, params[0]);

        case QuestExecuteType::kEndQuest:
            if (params.size() < 1) {
                return false;
            }
            return CompleteQuest(character_id, params[0]);

        case QuestExecuteType::kGiveItem:
            // 给予物品 - 对应 legacy 物品给予
            if (params.size() < 2) {
                return false;
            }
            {
                // 对应 legacy 物品给予逻辑
                uint32_t item_id = params[0];
                uint16_t count = static_cast<uint16_t>(params[1]);
                uint64_t instance_id = ItemManager::Instance().ObtainItem(character_id, item_id, count);
                bool success = (instance_id != 0);
                spdlog::info("Give item {} x{} to character {} - {}", item_id, count, character_id, success ? "success" : "failed");
                return success;
            }

        case QuestExecuteType::kTakeItem:
            // 拿走物品 - 对应 legacy 物品拿走
            if (params.size() < 2) {
                return false;
            }
            {
                // 对应 legacy 物品拿走逻辑
                uint32_t item_id = params[0];
                uint16_t count = static_cast<uint16_t>(params[1]);
                uint16_t removed = ItemManager::Instance().RemoveItem(character_id, item_id, count);
                bool success = (removed >= count);
                spdlog::info("Take item {} x{} from character {} - removed {}/{}", item_id, count, character_id, removed, count);
                return success;
            }

        case QuestExecuteType::kGiveExp:
            if (params.size() < 1) {
                return false;
            }
            // 给予经验 - 对应 legacy 经验给予
            try {
                uint64_t exp = params[0];
                bool success = CharacterManager::Instance().GiveExperience(character_id, exp);
                spdlog::info("Give {} exp to character {} - {}", exp, character_id, success ? "success" : "failed");
                return success;
            } catch (const std::exception& e) {
                spdlog::error("Failed to give exp: {}", e.what());
                return false;
            }

        case QuestExecuteType::kGiveMoney:
            if (params.size() < 1) {
                return false;
            }
            // 给予金钱 - 对应 legacy 金钱给予
            try {
                uint64_t amount = params[0];
                bool success = CharacterManager::Instance().GiveMoney(character_id, amount);
                spdlog::info("Give {} money to character {} - {}", amount, character_id, success ? "success" : "failed");
                return success;
            } catch (const std::exception& e) {
                spdlog::error("Failed to give money: {}", e.what());
                return false;
            }

        case QuestExecuteType::kTeleport:
            if (params.size() < 3) {
                return false;
            }
            // 传送角色 - 对应 legacy 角色传送
            {
                uint16_t map_id = static_cast<uint16_t>(params[0]);
                float x = static_cast<float>(params[1]);
                float y = static_cast<float>(params[2]);
                float z = params.size() >= 4 ? static_cast<float>(params[3]) : 0.0f;
                bool success = CharacterManager::Instance().Teleport(character_id, map_id, x, y, z);
                spdlog::info("Teleport character {} to map({},{},{},{}) - {}",
                            character_id, map_id, x, y, z, success ? "success" : "failed");
                return success;
            }

        case QuestExecuteType::kShowMessage:
            // 显示消息 - 对应 legacy 消息显示
            // 发送系统消息到客户端
            Core::Network::NotificationService::Instance().SendSystemMessage(
                character_id, "Quest message");  // TODO: 从params获取实际消息内容
            spdlog::info("Show message to character {}", character_id);
            return true;

        default:
            spdlog::warn("Unknown quest execute type: {}", static_cast<int>(action_type));
            return false;
    }
}

// ========== 辅助方法 ==========

void QuestManager::GiveQuestRewards(
    uint64_t character_id,
    const Quest& quest,
    const SubQuest& sub_quest
) {
    // 对应 legacy 任务奖励发放
    for (const auto& reward : sub_quest.rewards) {
        try {
            // 给予经验 - 对应 legacy 经验给予
            if (reward.exp > 0) {
                bool success = CharacterManager::Instance().GiveExperience(character_id, reward.exp);
                spdlog::info("Reward: {} exp to character {} - {}", reward.exp, character_id, success ? "success" : "failed");
                if (!success) {
                    spdlog::warn("Failed to give exp reward in quest");
                }
            }

            // 给予金钱 - 对应 legacy 金钱给予
            if (reward.money > 0) {
                bool success = CharacterManager::Instance().GiveMoney(character_id, reward.money);
                spdlog::info("Reward: {} money to character {} - {}", reward.money, character_id, success ? "success" : "failed");
                if (!success) {
                    spdlog::warn("Failed to give money reward in quest");
                }
            }

            // 给予物品 - 对应 legacy 物品给予
            if (reward.item_id > 0 && reward.item_count > 0) {
                uint64_t instance_id = ItemManager::Instance().ObtainItem(character_id, reward.item_id, static_cast<uint16_t>(reward.item_count));
                bool success = (instance_id != 0);
                spdlog::info("Reward: item {} x{} to character {} - {}",
                            reward.item_id, reward.item_count, character_id, success ? "success" : "failed");
                if (!success) {
                    spdlog::warn("Failed to give item reward in quest");
                }
            }

            // 学习技能 - 对应 legacy 技能学习
            if (reward.skill_id > 0) {
                bool success = SkillManager::LearnSkill(character_id, reward.skill_id);
                spdlog::info("Reward: skill {} to character {} - {}", reward.skill_id, character_id, success ? "success" : "failed");
                if (!success) {
                    spdlog::warn("Failed to learn skill reward in quest");
                }
            }

        } catch (const std::exception& e) {
            spdlog::error("Failed to give quest reward: {}", e.what());
        }
    }
}

void QuestManager::SaveQuestProgress(uint64_t character_id, const Quest& quest) {
    // 保存到数据库
    bool success = Murim::Core::Database::QuestDAO::SaveCharacterQuest(character_id, quest);
    if (!success) {
        spdlog::error("Failed to save quest progress: character={}, quest={}", character_id, quest.quest_id);
    } else {
        spdlog::debug("Saved quest progress: character={}, quest={}", character_id, quest.quest_id);
    }
}

void QuestManager::LoadQuestProgress(uint64_t character_id) {
    // 从数据库加载会在 GetCharacterQuests 中自动处理
    spdlog::debug("Load quest progress for character {}", character_id);
}

void QuestManager::InvalidateCharacterQuestCache(uint64_t character_id) {
    character_quests_cache_.erase(character_id);
    cache_timestamps_.erase(character_id);
    spdlog::debug("Invalidated quest cache for character {}", character_id);
}

bool QuestManager::IsCacheExpired(uint64_t character_id) const {
    auto it = cache_timestamps_.find(character_id);
    if (it == cache_timestamps_.end()) {
        return true;  // 没有缓存时间戳，视为过期
    }

    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second);
    return age.count() >= CACHE_EXPIRY_SECONDS;
}

QuestManager::CacheStats QuestManager::GetCacheStats() const {
    CacheStats stats;
    stats.hits = cache_hits_;
    stats.misses = cache_misses_;
    stats.updates = cache_updates_;
    stats.total_requests = cache_hits_ + cache_misses_;

    if (stats.total_requests > 0) {
        stats.hit_rate = static_cast<double>(cache_hits_) / stats.total_requests;
    } else {
        stats.hit_rate = 0.0;
    }

    return stats;
}

void QuestManager::ResetCacheStats() {
    cache_hits_ = 0;
    cache_misses_ = 0;
    cache_updates_ = 0;
    spdlog::info("Cache stats reset");
}

void QuestManager::PrintCacheStats() const {
    auto stats = GetCacheStats();

    auto hit_rate_pct = stats.hit_rate * 100.0;
    auto miss_rate_pct = (1.0 - stats.hit_rate) * 100.0;

    spdlog::info("========== Quest Cache Statistics ==========");
    spdlog::info("Total Requests: {}", stats.total_requests);
    spdlog::info("Cache Hits:     {} ({}%)", stats.hits, static_cast<int>(hit_rate_pct));
    spdlog::info("Cache Misses:   {} ({}%)", stats.misses, static_cast<int>(miss_rate_pct));
    spdlog::info("Cache Updates:  {}", stats.updates);
    spdlog::info("Cache Size:     {} characters", character_quests_cache_.size());
    spdlog::info("===========================================");
}

} // namespace Game
} // namespace Murim
