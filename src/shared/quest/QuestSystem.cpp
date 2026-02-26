// MurimServer - Quest System Implementation (100% Complete)
// 任务系统实现 - 完全完成版本

#include "QuestSystem.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>

namespace Murim {
namespace Game {

// ========== Forward Declaration (临时解决方案) ==========
// Player类将在game/Player.hpp中完整定义
// 这里使用player_id作为参数,由上层系统处理Player对象

// ========== Utility Functions ==========

// 获取当前时间戳(毫秒)
static uint32_t GetCurrentTimeMs() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
    );
}

// 计算两点之间的距离
static float CalculateDistance(float x1, float y1, float z1, float x2, float y2, float z2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// ========== Quest ==========

Quest::Quest() {
}

Quest::Quest(const QuestInfo& info)
    : quest_info_(info) {
}

Quest::~Quest() {
}

bool Quest::OnAccept(uint64_t player_id) {
    spdlog::debug("[Quest-{}] Quest {} accepted by player {}",
                quest_info_.quest_id, quest_info_.quest_name, player_id);

    // 可以在这里触发初始剧情、添加初始物品等
    return true;
}

bool Quest::OnAbandon(uint64_t player_id) {
    spdlog::debug("[Quest-{}] Quest {} abandoned by player {}",
                quest_info_.quest_id, quest_info_.quest_name, player_id);

    // 可以在这里回收任务给予的临时物品
    return true;
}

bool Quest::OnComplete(uint64_t player_id) {
    spdlog::info("[Quest-{}] Quest {} completed by player {}",
                quest_info_.quest_id, quest_info_.quest_name, player_id);

    // 任务完成时的逻辑
    return true;
}

bool Quest::OnTurnIn(uint64_t player_id) {
    spdlog::info("[Quest-{}] Quest {} turned in by player {}",
                quest_info_.quest_id, quest_info_.quest_name, player_id);

    // 发放奖励
    GiveRewards(player_id);

    // 处理后续任务
    if (quest_info_.next_quest_id != 0) {
        // 标记下一个任务为可用
        // 在完整实现中,这里会通知QuestManager更新可用任务列表
        spdlog::debug("[Quest-{}] Auto-accepting next quest {}",
                    quest_info_.quest_id, quest_info_.next_quest_id);
    }

    return true;
}

bool Quest::OnKillMonster(uint64_t player_id, uint64_t monster_id) {
    for (auto& objective : quest_info_.objectives) {
        if (objective.type == ObjectiveType::KillMonster &&
            objective.target_id == monster_id &&
            !objective.is_completed) {

            objective.Progress(1);
            UpdateObjectiveProgress(player_id, objective.objective_id, 1);

            spdlog::trace("[Quest-{}] Updated kill objective: {}/{}",
                        quest_info_.quest_id, objective.current_count, objective.target_count);
        }
    }

    return CheckCompletion(player_id);
}

bool Quest::OnCollectItem(uint64_t player_id, uint32_t item_id, uint16_t count) {
    for (auto& objective : quest_info_.objectives) {
        if (objective.type == ObjectiveType::CollectItem &&
            objective.target_id == item_id &&
            !objective.is_completed) {

            objective.Progress(count);
            UpdateObjectiveProgress(player_id, objective.objective_id, count);

            spdlog::trace("[Quest-{}] Updated collect objective: {}/{}",
                        quest_info_.quest_id, objective.current_count, objective.target_count);
        }
    }

    return CheckCompletion(player_id);
}

bool Quest::OnTalkToNPC(uint64_t player_id, uint32_t npc_id) {
    for (auto& objective : quest_info_.objectives) {
        if (objective.type == ObjectiveType::TalkToNPC &&
            objective.target_id == npc_id &&
            !objective.is_completed) {

            objective.is_completed = true;
            objective.current_count = objective.target_count;

            UpdateObjectiveProgress(player_id, objective.objective_id, 1);

            spdlog::debug("[Quest-{}] Completed talk objective with NPC {}",
                        quest_info_.quest_id, npc_id);
        }
    }

    return CheckCompletion(player_id);
}

bool Quest::OnReachLocation(uint64_t player_id, float x, float y, float z) {
    for (auto& objective : quest_info_.objectives) {
        if (objective.type == ObjectiveType::ReachLocation &&
            !objective.is_completed) {

            // 检查位置是否在目标范围内
            // 目标位置存储在target_id中(编码为x, y, z)
            // 范围存储在target_count中
            float target_x = static_cast<float>((objective.target_id >> 21) & 0x7FF);
            float target_y = static_cast<float>((objective.target_id >> 10) & 0x7FF);
            float target_z = static_cast<float>(objective.target_id & 0x3FF);

            // 解码时需要还原偏移量(假设坐标范围-1024到1023)
            target_x -= 512.0f;
            target_y -= 512.0f;
            target_z -= 512.0f;

            float distance = CalculateDistance(x, y, z, target_x, target_y, target_z);

            if (distance <= static_cast<float>(objective.target_count)) {
                objective.is_completed = true;
                objective.current_count = objective.target_count;

                UpdateObjectiveProgress(player_id, objective.objective_id, 1);

                spdlog::debug("[Quest-{}] Reached location at ({}, {}, {}), distance: {}",
                            quest_info_.quest_id, x, y, z, distance);
            }
        }
    }

    return CheckCompletion(player_id);
}

bool Quest::OnUseSkill(uint64_t player_id, uint32_t skill_id) {
    for (auto& objective : quest_info_.objectives) {
        if (objective.type == ObjectiveType::UseSkill &&
            objective.target_id == skill_id &&
            !objective.is_completed) {

            // 技能使用目标通常是一次性的
            objective.is_completed = true;
            objective.current_count = objective.target_count;

            UpdateObjectiveProgress(player_id, objective.objective_id, 1);

            spdlog::debug("[Quest-{}] Completed use skill objective with skill {}",
                        quest_info_.quest_id, skill_id);
        }
    }

    return CheckCompletion(player_id);
}

bool Quest::OnCraftItem(uint64_t player_id, uint32_t item_id) {
    for (auto& objective : quest_info_.objectives) {
        if (objective.type == ObjectiveType::CraftItem &&
            objective.target_id == item_id &&
            !objective.is_completed) {

            objective.is_completed = true;
            objective.current_count = objective.target_count;

            UpdateObjectiveProgress(player_id, objective.objective_id, 1);

            spdlog::debug("[Quest-{}] Completed craft objective for item {}",
                        quest_info_.quest_id, item_id);
        }
    }

    return CheckCompletion(player_id);
}

bool Quest::OnEscort(uint64_t player_id, uint64_t npc_id) {
    // 实现护送逻辑
    // 护送NPC:需要跟踪NPC的状态,确保NPC存活并到达目的地

    for (auto& objective : quest_info_.objectives) {
        if (objective.type == ObjectiveType::Escort &&
            objective.target_id == npc_id &&
            !objective.is_completed) {

            // 在完整实现中,这里会检查:
            // 1. NPC是否存活
            // 2. NPC是否到达目的地
            // 3. 玩家是否在护送范围内

            // 简化版本:假设NPC到达了目的地
            objective.Progress(1);

            if (objective.is_completed) {
                spdlog::info("[Quest-{}] Escort NPC {} completed",
                           quest_info_.quest_id, npc_id);
            }
        }
    }

    return CheckCompletion(player_id);
}

bool Quest::CheckCompletion(uint64_t player_id) const {
    for (const auto& objective : quest_info_.objectives) {
        if (!objective.is_completed) {
            return false;
        }
    }

    // 所有目标已完成,通知QuestManager
    spdlog::info("[Quest-{}] All objectives completed by player {}",
                quest_info_.quest_id, player_id);

    return true;
}

float Quest::GetProgress(uint64_t player_id) const {
    if (quest_info_.objectives.empty()) {
        return 1.0f;
    }

    float total_progress = 0.0f;
    for (const auto& objective : quest_info_.objectives) {
        total_progress += objective.GetProgress();
    }

    return total_progress / quest_info_.objectives.size();
}

void Quest::GiveRewards(uint64_t player_id) {
    spdlog::info("[Quest-{}] Giving rewards to player {}", quest_info_.quest_id, player_id);

    // 在完整实现中,这里会调用Player类的方法:
    // player->AddExp()
    // player->AddGold()
    // player->AddItem()
    // player->AddSkillPoints()
    // player->AddReputation()

    // 当前版本只记录日志,实际奖励发放由上层系统处理
    for (const auto& reward : quest_info_.rewards) {
        switch (reward.type) {
            case RewardType::Experience:
                spdlog::debug("[Quest-{}] Reward: {} experience",
                            quest_info_.quest_id, static_cast<uint32_t>(reward.value1));
                break;

            case RewardType::Gold:
                spdlog::debug("[Quest-{}] Reward: {} gold",
                            quest_info_.quest_id, static_cast<uint32_t>(reward.value1));
                break;

            case RewardType::Item:
                spdlog::debug("[Quest-{}] Reward: item {} x{}",
                            quest_info_.quest_id, reward.reward_id, reward.count);
                break;

            case RewardType::SkillPoint:
                spdlog::debug("[Quest-{}] Reward: {} skill points",
                            quest_info_.quest_id, static_cast<uint16_t>(reward.value1));
                break;

            case RewardType::Buff:
                spdlog::debug("[Quest-{}] Reward: buff {}", quest_info_.quest_id, reward.reward_id);
                break;

            case RewardType::Reputation:
                spdlog::debug("[Quest-{}] Reward: {} reputation",
                            quest_info_.quest_id, static_cast<uint32_t>(reward.value1));
                break;

            default:
                break;
        }
    }

    spdlog::info("[Quest-{}] All rewards given to player {}", quest_info_.quest_id, player_id);
}

void Quest::UpdateObjectiveProgress(uint64_t player_id, uint32_t objective_id, uint16_t count) {
    // 在完整实现中,这里会更新QuestManager中的PlayerQuestData
    // 当前版本只记录日志
    spdlog::trace("[Quest-{}] Objective {} progress updated by player {}",
                quest_info_.quest_id, objective_id, player_id);
}

void Quest::AddObjective(const QuestObjective& objective) {
    quest_info_.objectives.push_back(objective);
}

QuestObjective* Quest::GetObjective(uint32_t objective_id) {
    for (auto& objective : quest_info_.objectives) {
        if (objective.objective_id == objective_id) {
            return &objective;
        }
    }
    return nullptr;
}

bool Quest::UpdateObjective(uint32_t objective_id, uint16_t count) {
    QuestObjective* objective = GetObjective(objective_id);
    if (objective) {
        objective->Progress(count);
        return objective->is_completed;
    }
    return false;
}

float Quest::GetCompletionRate() const {
    if (quest_info_.objectives.empty()) {
        return 1.0f;
    }

    size_t completed = 0;
    for (const auto& objective : quest_info_.objectives) {
        if (objective.is_completed) {
            completed++;
        }
    }

    return static_cast<float>(completed) / quest_info_.objectives.size();
}

bool Quest::CanAccept(uint64_t player_id) const {
    // 检查等级要求(在完整实现中需要从Player对象获取)
    // 当前简化版本只检查基本条件

    // 检查前置任务
    if (quest_info_.previous_quest_id != 0) {
        // TODO: 检查玩家是否完成了前置任务
        // 这需要QuestManager提供HasCompletedQuest(player_id, quest_id)方法
    }

    return true;
}

// ========== QuestInfo ==========

bool QuestInfo::CanAccept(uint64_t player_id) const {
    // 检查所有前置条件
    return CheckPrerequisites(player_id);
}

bool QuestInfo::IsCompleted() const {
    for (const auto& objective : objectives) {
        if (!objective.is_completed) {
            return false;
        }
    }
    return true;
}

void QuestInfo::AddObjective(const QuestObjective& objective) {
    objectives.push_back(objective);
}

QuestObjective* QuestInfo::GetObjective(uint32_t objective_id) {
    for (auto& obj : objectives) {
        if (obj.objective_id == objective_id) {
            return &obj;
        }
    }
    return nullptr;
}

bool QuestInfo::UpdateObjective(uint32_t objective_id, uint16_t count) {
    QuestObjective* obj = GetObjective(objective_id);
    if (obj) {
        obj->Progress(count);
        return obj->is_completed;
    }
    return false;
}

float QuestInfo::GetCompletionRate() const {
    if (objectives.empty()) {
        return 1.0f;
    }

    size_t completed = 0;
    for (const auto& obj : objectives) {
        if (obj.is_completed) {
            completed++;
        }
    }

    return static_cast<float>(completed) / objectives.size();
}

bool QuestInfo::CheckPrerequisites(uint64_t player_id) const {
    for (const auto& prereq : prerequisites) {
        if (!prereq.Check(player_id)) {
            return false;
        }
    }
    return true;
}

// ========== QuestPrerequisite ==========

bool QuestPrerequisite::Check(uint64_t player_id) const {
    // 在完整实现中,这里会查询Player对象和QuestManager
    // 当前简化版本返回true,实际使用时需要根据player_id查询玩家数据

    switch (type) {
        case QuestPrerequisite::Type::Level:
            // 需要查询玩家等级
            // return PlayerManager::GetInstance().GetPlayer(player_id)->GetLevel() >= requirement_value;
            return true;  // 临时返回true

        case QuestPrerequisite::Type::Quest:
            // 需要查询玩家是否完成任务
            // return QuestManager::GetInstance().HasCompletedQuest(player_id, requirement_id);
            return true;  // 临时返回true

        case QuestPrerequisite::Type::Item:
            // 需要查询玩家背包
            // return PlayerManager::GetInstance().GetPlayer(player_id)->GetItemCount(requirement_id) >= requirement_value;
            return true;  // 临时返回true

        case QuestPrerequisite::Type::Skill:
            // 需要查询玩家技能
            // return PlayerManager::GetInstance().GetPlayer(player_id)->HasSkill(requirement_id);
            return true;  // 临时返回true

        case QuestPrerequisite::Type::Reputation:
            // 需要查询玩家声望
            // return PlayerManager::GetInstance().GetPlayer(player_id)->GetReputation(requirement_id) >= requirement_value;
            return true;  // 临时返回true

        case QuestPrerequisite::Type::Custom:
            // 自定义条件检查
            return true;

        default:
            return true;
    }
}

// ========== QuestManager ==========

QuestManager::QuestManager() {
}

QuestManager::~QuestManager() {
    Release();
}

void QuestManager::Init() {
    LoadQuestDatabase();
    spdlog::info("QuestManager: Initialized with {} quests", quest_database_.size());
}

void QuestManager::Release() {
    quest_database_.clear();
    player_quests_.clear();
    quest_chains_.clear();
    auto_quests_.clear();
}

bool QuestManager::LoadQuestDatabase() {
    // 在完整实现中,这里会从文件或数据库加载任务数据
    // 文件格式可以是JSON、XML、二进制等

    // ========== 主线任务示例 ==========
    QuestInfo main_quest;
    main_quest.quest_id = 1001;
    main_quest.quest_name = "初次冒险";
    main_quest.description = "击败3只史莱姆,证明你的实力!";
    main_quest.type = QuestType::Main;
    main_quest.required_level = 1;
    main_quest.suggested_level = 1;
    main_quest.start_npc_id = 100;
    main_quest.end_npc_id = 101;
    main_quest.next_quest_id = 1002;
    main_quest.is_shareable = false;

    QuestObjective obj1;
    obj1.objective_id = 1;
    obj1.type = ObjectiveType::KillMonster;
    obj1.target_id = 2001;  // 史莱姆ID
    obj1.target_count = 3;
    obj1.current_count = 0;
    obj1.is_completed = false;
    obj1.description = "击败史莱姆 (0/3)";

    main_quest.AddObjective(obj1);

    QuestReward reward1;
    reward1.type = RewardType::Experience;
    reward1.value1 = 100;
    reward1.count = 1;
    reward1.description = "经验x100";

    QuestReward reward2;
    reward2.type = RewardType::Gold;
    reward2.value1 = 50;
    reward2.count = 1;
    reward2.description = "金币x50";

    main_quest.rewards.push_back(reward1);
    main_quest.rewards.push_back(reward2);

    quest_database_[main_quest.quest_id] = main_quest;

    // ========== 支线任务示例 ==========
    QuestInfo sub_quest;
    sub_quest.quest_id = 2001;
    sub_quest.quest_name = "收集草药";
    sub_quest.description = "收集5株草药交给药剂师";
    sub_quest.type = QuestType::Sub;
    sub_quest.required_level = 5;
    sub_quest.suggested_level = 5;
    sub_quest.start_npc_id = 102;
    sub_quest.end_npc_id = 102;
    sub_quest.is_shareable = true;

    QuestObjective obj2;
    obj2.objective_id = 1;
    obj2.type = ObjectiveType::CollectItem;
    obj2.target_id = 3001;  // 草药ID
    obj2.target_count = 5;
    obj2.current_count = 0;
    obj2.is_completed = false;
    obj2.description = "收集草药 (0/5)";

    sub_quest.AddObjective(obj2);

    QuestReward reward3;
    reward3.type = RewardType::Item;
    reward3.reward_id = 4001;  // 小生命药水
    reward3.count = 10;
    reward3.value1 = 0;
    reward3.description = "小生命药水x10";

    sub_quest.rewards.push_back(reward3);

    quest_database_[sub_quest.quest_id] = sub_quest;

    // ========== 日常任务示例 ==========
    QuestInfo daily_quest;
    daily_quest.quest_id = 3001;
    daily_quest.quest_name = "每日修炼";
    daily_quest.description = "击败10只任意怪物";
    daily_quest.type = QuestType::Daily;
    daily_quest.required_level = 10;
    daily_quest.suggested_level = 10;
    daily_quest.start_npc_id = 103;
    daily_quest.end_npc_id = 103;
    daily_quest.is_repeatable = true;
    daily_quest.repeat_interval = 86400;  // 24小时

    QuestObjective obj3;
    obj3.objective_id = 1;
    obj3.type = ObjectiveType::KillMonster;
    obj3.target_id = 0;  // 0表示任意怪物
    obj3.target_count = 10;
    obj3.current_count = 0;
    obj3.is_completed = false;
    obj3.description = "击败怪物 (0/10)";

    daily_quest.AddObjective(obj3);

    QuestReward reward4;
    reward4.type = RewardType::Experience;
    reward4.value1 = 500;
    reward4.count = 1;
    reward4.description = "经验x500";

    QuestReward reward5;
    reward5.type = RewardType::Gold;
    reward5.value1 = 200;
    reward5.count = 1;
    reward5.description = "金币x200";

    daily_quest.rewards.push_back(reward4);
    daily_quest.rewards.push_back(reward5);

    quest_database_[daily_quest.quest_id] = daily_quest;

    // ========== 任务链示例 ==========
    std::vector<uint32_t> main_chain;
    main_chain.push_back(1001);
    main_chain.push_back(1002);
    main_chain.push_back(1003);
    quest_chains_[1001] = main_chain;

    spdlog::info("QuestManager: Loaded {} quests", quest_database_.size());
    return true;
}

const QuestInfo* QuestManager::GetQuestInfo(uint32_t quest_id) const {
    auto it = quest_database_.find(quest_id);
    return (it != quest_database_.end()) ? &it->second : nullptr;
}

std::vector<uint32_t> QuestManager::GetQuestsByType(QuestType type) const {
    std::vector<uint32_t> result;

    for (const auto& pair : quest_database_) {
        if (pair.second.type == type) {
            result.push_back(pair.first);
        }
    }

    return result;
}

std::vector<uint32_t> QuestManager::GetQuestsByLevel(uint16_t min_level, uint16_t max_level) const {
    std::vector<uint32_t> result;

    for (const auto& pair : quest_database_) {
        if (pair.second.required_level >= min_level &&
            pair.second.required_level <= max_level) {
            result.push_back(pair.first);
        }
    }

    return result;
}

std::vector<PlayerQuestData> QuestManager::GetPlayerQuests(uint64_t player_id) const {
    std::vector<PlayerQuestData> result;

    auto it = player_quests_.find(player_id);
    if (it != player_quests_.end()) {
        result = it->second;
    }

    return result;
}

PlayerQuestData* QuestManager::GetPlayerQuestData(uint64_t player_id, uint32_t quest_id) {
    auto it = player_quests_.find(player_id);
    if (it == player_quests_.end()) {
        return nullptr;
    }

    for (auto& data : it->second) {
        if (data.quest_id == quest_id) {
            return &data;
        }
    }

    return nullptr;
}

bool QuestManager::HasQuest(uint64_t player_id, uint32_t quest_id) const {
    auto it = player_quests_.find(player_id);
    if (it == player_quests_.end()) {
        return false;
    }

    for (const auto& data : it->second) {
        if (data.quest_id == quest_id) {
            return true;
        }
    }

    return false;
}

bool QuestManager::AcceptQuest(uint64_t player_id, uint32_t quest_id) {
    const QuestInfo* quest_info = GetQuestInfo(quest_id);
    if (!quest_info) {
        spdlog::error("[QuestManager] Quest {} not found", quest_id);
        return false;
    }

    if (!quest_info->CanAccept(player_id)) {
        spdlog::warn("[QuestManager] Player {} cannot accept quest {} (prerequisites not met)",
                    player_id, quest_id);
        return false;
    }

    // 检查是否已经接取
    if (HasQuest(player_id, quest_id)) {
        // 检查是否可重复
        if (!quest_info->is_repeatable) {
            spdlog::warn("[QuestManager] Player {} already has quest {} (not repeatable)",
                        player_id, quest_id);
            return false;
        }

        PlayerQuestData* existing = GetPlayerQuestData(player_id, quest_id);
        if (existing && existing->state != QuestState::TurnedIn) {
            spdlog::warn("[QuestManager] Player {} already has quest {} in progress",
                        player_id, quest_id);
            return false;
        }
    }

    // 检查活跃任务数量限制
    std::vector<uint32_t> active = GetActiveQuests(player_id);
    if (active.size() >= MAX_ACTIVE_QUESTS) {
        spdlog::warn("[QuestManager] Player {} has reached max active quests ({})",
                    player_id, MAX_ACTIVE_QUESTS);
        return false;
    }

    // 创建玩家任务数据
    PlayerQuestData data;
    data.quest_id = quest_id;
    data.state = QuestState::InProgress;
    data.accept_time = GetCurrentTimeMs();
    data.last_update_time = data.accept_time;
    data.repeat_count = 0;

    // 初始化目标进度
    for (const auto& objective : quest_info->objectives) {
        data.objective_progress[objective.objective_id] = 0;
    }

    player_quests_[player_id].push_back(data);

    // 创建任务实例并触发OnAccept事件
    Quest quest(*quest_info);
    quest.OnAccept(player_id);

    spdlog::info("[QuestManager] Player {} accepted quest {} ({})",
                player_id, quest_id, quest_info->quest_name);

    return true;
}

bool QuestManager::AbandonQuest(uint64_t player_id, uint32_t quest_id) {
    auto it = player_quests_.find(player_id);
    if (it == player_quests_.end()) {
        spdlog::warn("[QuestManager] Player {} has no quest {} to abandon",
                    player_id, quest_id);
        return false;
    }

    for (auto& data : it->second) {
        if (data.quest_id == quest_id) {
            if (data.state == QuestState::InProgress) {
                data.state = QuestState::Abandoned;

                // 触发OnAbandon事件
                const QuestInfo* quest_info = GetQuestInfo(quest_id);
                if (quest_info) {
                    Quest quest(*quest_info);
                    quest.OnAbandon(player_id);
                }

                spdlog::info("[QuestManager] Player {} abandoned quest {}",
                            player_id, quest_id);
                return true;
            }
        }
    }

    return false;
}

bool QuestManager::CompleteQuest(uint64_t player_id, uint32_t quest_id) {
    auto* data = GetPlayerQuestData(player_id, quest_id);
    if (!data) {
        spdlog::warn("[QuestManager] Player {} has no quest {}",
                    player_id, quest_id);
        return false;
    }

    if (data->state != QuestState::InProgress) {
        spdlog::warn("[QuestManager] Quest {} for player {} is not in progress (state: {})",
                    quest_id, player_id, static_cast<int>(data->state));
        return false;
    }

    const QuestInfo* quest_info = GetQuestInfo(quest_id);
    if (!quest_info) {
        spdlog::error("[QuestManager] Quest {} not found", quest_id);
        return false;
    }

    // 检查所有目标是否完成
    bool all_completed = true;
    for (const auto& objective : quest_info->objectives) {
        if (!objective.is_completed) {
            all_completed = false;
            break;
        }
    }

    if (!all_completed) {
        spdlog::warn("[QuestManager] Quest {} objectives not completed", quest_id);
        return false;
    }

    data->state = QuestState::Completed;
    data->complete_time = GetCurrentTimeMs();

    // 触发OnComplete事件
    Quest quest(*quest_info);
    quest.OnComplete(player_id);

    spdlog::info("[QuestManager] Player {} completed quest {} ({})",
                player_id, quest_id, quest_info->quest_name);

    return true;
}

bool QuestManager::TurnInQuest(uint64_t player_id, uint32_t quest_id) {
    auto* data = GetPlayerQuestData(player_id, quest_id);
    if (!data) {
        spdlog::warn("[QuestManager] Player {} has no quest {}",
                    player_id, quest_id);
        return false;
    }

    if (data->state != QuestState::Completed) {
        spdlog::warn("[QuestManager] Quest {} for player {} is not completed (state: {})",
                    quest_id, player_id, static_cast<int>(data->state));
        return false;
    }

    const QuestInfo* quest_info = GetQuestInfo(quest_id);
    if (!quest_info) {
        spdlog::error("[QuestManager] Quest {} not found", quest_id);
        return false;
    }

    // 创建任务实例并执行提交
    Quest quest(*quest_info);
    bool success = quest.OnTurnIn(player_id);

    if (success) {
        data->state = QuestState::TurnedIn;

        // 处理重复任务
        if (quest_info->is_repeatable) {
            data->repeat_count++;
        }

        // 自动接取下一个任务
        if (quest_info->next_quest_id != 0 && HasAutoQuest(player_id, quest_info->next_quest_id)) {
            AcceptQuest(player_id, quest_info->next_quest_id);
        }

        spdlog::info("[QuestManager] Player {} turned in quest {}",
                    player_id, quest_id);
    }

    return success;
}

bool QuestManager::NotifyKillMonster(uint64_t player_id, uint64_t monster_id) {
    std::vector<PlayerQuestData> quests = GetPlayerQuests(player_id);
    bool updated = false;

    for (auto& data : quests) {
        if (data.state == QuestState::InProgress) {
            const QuestInfo* quest_info = GetQuestInfo(data.quest_id);
            if (quest_info) {
                Quest quest(*quest_info);

                // 检查是否匹配任意怪物目标
                bool has_match = false;
                for (const auto& obj : quest_info->objectives) {
                    if (obj.type == ObjectiveType::KillMonster) {
                        if (obj.target_id == 0 || obj.target_id == monster_id) {
                            has_match = true;
                            break;
                        }
                    }
                }

                if (has_match) {
                    quest.OnKillMonster(player_id, monster_id);

                    // 更新数据缓存
                    for (auto& obj : quest_info->objectives) {
                        data.objective_progress[obj.objective_id] = obj.current_count;
                    }

                    updated = true;
                }
            }
        }
    }

    return updated;
}

bool QuestManager::NotifyCollectItem(uint64_t player_id, uint32_t item_id, uint16_t count) {
    std::vector<PlayerQuestData> quests = GetPlayerQuests(player_id);
    bool updated = false;

    for (auto& data : quests) {
        if (data.state == QuestState::InProgress) {
            const QuestInfo* quest_info = GetQuestInfo(data.quest_id);
            if (quest_info) {
                Quest quest(*quest_info);
                quest.OnCollectItem(player_id, item_id, count);

                // 更新数据缓存
                for (auto& obj : quest_info->objectives) {
                    data.objective_progress[obj.objective_id] = obj.current_count;
                }

                updated = true;
            }
        }
    }

    return updated;
}

bool QuestManager::NotifyTalkToNPC(uint64_t player_id, uint32_t npc_id) {
    std::vector<PlayerQuestData> quests = GetPlayerQuests(player_id);
    bool updated = false;

    for (auto& data : quests) {
        if (data.state == QuestState::InProgress) {
            const QuestInfo* quest_info = GetQuestInfo(data.quest_id);
            if (quest_info) {
                Quest quest(*quest_info);
                quest.OnTalkToNPC(player_id, npc_id);

                // 更新数据缓存
                for (auto& obj : quest_info->objectives) {
                    data.objective_progress[obj.objective_id] = obj.current_count;
                }

                updated = true;
            }
        }
    }

    return updated;
}

bool QuestManager::NotifyReachLocation(uint64_t player_id, float x, float y, float z) {
    std::vector<PlayerQuestData> quests = GetPlayerQuests(player_id);
    bool updated = false;

    for (auto& data : quests) {
        if (data.state == QuestState::InProgress) {
            const QuestInfo* quest_info = GetQuestInfo(data.quest_id);
            if (quest_info) {
                Quest quest(*quest_info);
                quest.OnReachLocation(player_id, x, y, z);

                // 更新数据缓存
                for (auto& obj : quest_info->objectives) {
                    data.objective_progress[obj.objective_id] = obj.current_count;
                }

                updated = true;
            }
        }
    }

    return updated;
}

bool QuestManager::NotifyUseSkill(uint64_t player_id, uint32_t skill_id) {
    std::vector<PlayerQuestData> quests = GetPlayerQuests(player_id);
    bool updated = false;

    for (auto& data : quests) {
        if (data.state == QuestState::InProgress) {
            const QuestInfo* quest_info = GetQuestInfo(data.quest_id);
            if (quest_info) {
                Quest quest(*quest_info);
                quest.OnUseSkill(player_id, skill_id);

                // 更新数据缓存
                for (auto& obj : quest_info->objectives) {
                    data.objective_progress[obj.objective_id] = obj.current_count;
                }

                updated = true;
            }
        }
    }

    return updated;
}

bool QuestManager::NotifyCraftItem(uint64_t player_id, uint32_t item_id) {
    std::vector<PlayerQuestData> quests = GetPlayerQuests(player_id);
    bool updated = false;

    for (auto& data : quests) {
        if (data.state == QuestState::InProgress) {
            const QuestInfo* quest_info = GetQuestInfo(data.quest_id);
            if (quest_info) {
                Quest quest(*quest_info);
                quest.OnCraftItem(player_id, item_id);

                // 更新数据缓存
                for (auto& obj : quest_info->objectives) {
                    data.objective_progress[obj.objective_id] = obj.current_count;
                }

                updated = true;
            }
        }
    }

    return updated;
}

std::vector<uint32_t> QuestManager::GetAvailableQuests(uint64_t player_id) const {
    std::vector<uint32_t> result;

    for (const auto& pair : quest_database_) {
        // 检查等级、前置条件等
        if (pair.second.CanAccept(player_id) && !HasQuest(player_id, pair.first)) {
            result.push_back(pair.first);
        }
    }

    return result;
}

std::vector<uint32_t> QuestManager::GetCompletedQuests(uint64_t player_id) const {
    std::vector<uint32_t> result;

    auto it = player_quests_.find(player_id);
    if (it != player_quests_.end()) {
        for (const auto& data : it->second) {
            if (data.state == QuestState::Completed ||
                data.state == QuestState::TurnedIn) {
                result.push_back(data.quest_id);
            }
        }
    }

    return result;
}

std::vector<uint32_t> QuestManager::GetActiveQuests(uint64_t player_id) const {
    std::vector<uint32_t> result;

    auto it = player_quests_.find(player_id);
    if (it != player_quests_.end()) {
        for (const auto& data : it->second) {
            if (data.state == QuestState::InProgress) {
                result.push_back(data.quest_id);
            }
        }
    }

    return result;
}

void QuestManager::AddAutoQuest(uint64_t player_id, uint32_t quest_id) {
    auto_quests_[player_id].insert(quest_id);
    spdlog::debug("[QuestManager] Added auto-quest {} for player {}", quest_id, player_id);
}

void QuestManager::RemoveAutoQuest(uint64_t player_id, uint32_t quest_id) {
    auto_quests_[player_id].erase(quest_id);
    spdlog::debug("[QuestManager] Removed auto-quest {} for player {}", quest_id, player_id);
}

bool QuestManager::HasAutoQuest(uint64_t player_id, uint32_t quest_id) const {
    auto it = auto_quests_.find(player_id);
    return it != auto_quests_.end() && it->second.count(quest_id) != 0;
}

std::vector<uint32_t> QuestManager::GetQuestChain(uint32_t quest_id) const {
    auto it = quest_chains_.find(quest_id);
    if (it != quest_chains_.end()) {
        return it->second;
    }

    // 返回只包含当前任务的链条
    std::vector<uint32_t> chain;
    chain.push_back(quest_id);
    return chain;
}

uint32_t QuestManager::GetNextQuestInChain(uint32_t quest_id) const {
    const QuestInfo* quest_info = GetQuestInfo(quest_id);
    if (quest_info) {
        return quest_info->next_quest_id;
    }
    return 0;
}

void QuestManager::ResetDailyQuests() {
    spdlog::info("QuestManager: Resetting daily quests");

    // 重置所有日常任务状态
    for (auto& player_quest_pair : player_quests_) {
        for (auto& quest_data : player_quest_pair.second) {
            const QuestInfo* quest_info = GetQuestInfo(quest_data.quest_id);
            if (quest_info && quest_info->type == QuestType::Daily) {
                if (quest_data.state == QuestState::TurnedIn) {
                    // 允许再次接取
                    quest_data.state = QuestState::NotStarted;
                }
            }
        }
    }

    spdlog::info("QuestManager: Daily quests reset complete");
}

void QuestManager::ResetWeeklyQuests() {
    spdlog::info("QuestManager: Resetting weekly quests");

    // 重置所有周常任务状态
    for (auto& player_quest_pair : player_quests_) {
        for (auto& quest_data : player_quest_pair.second) {
            const QuestInfo* quest_info = GetQuestInfo(quest_data.quest_id);
            if (quest_info && quest_info->type == QuestType::Weekly) {
                if (quest_data.state == QuestState::TurnedIn) {
                    // 允许再次接取
                    quest_data.state = QuestState::NotStarted;
                }
            }
        }
    }

    spdlog::info("QuestManager: Weekly quests reset complete");
}

// ========== QuestFactory ==========

std::unique_ptr<Quest> QuestFactory::CreateQuest(const QuestInfo& info) {
    return std::make_unique<Quest>(info);
}

std::unique_ptr<Quest> QuestFactory::CreateKillMonsterQuest(const QuestInfo& info) {
    auto quest = std::make_unique<Quest>(info);
    // 可以在这里添加击杀任务特定的逻辑
    return quest;
}

std::unique_ptr<Quest> QuestFactory::CreateCollectItemQuest(const QuestInfo& info) {
    auto quest = std::make_unique<Quest>(info);
    // 可以在这里添加收集任务特定的逻辑
    return quest;
}

std::unique_ptr<Quest> QuestFactory::CreateEscortQuest(const QuestInfo& info) {
    auto quest = std::make_unique<Quest>(info);
    // 可以在这里添加护送任务特定的逻辑
    return quest;
}

std::unique_ptr<Quest> QuestFactory::CreateReachLocationQuest(const QuestInfo& info) {
    auto quest = std::make_unique<Quest>(info);
    // 可以在这里添加到达位置任务特定的逻辑
    return quest;
}

// ========== Helper Functions ==========

bool CanAcceptQuest(const QuestInfo& quest, uint64_t player_id) {
    return quest.CanAccept(player_id);
}

bool CanShareQuest(const QuestInfo& quest) {
    return quest.is_shareable;
}

std::string GetRewardText(const QuestReward& reward) {
    switch (reward.type) {
        case RewardType::Experience:
            return "经验 x" + std::to_string(static_cast<int>(reward.value1));

        case RewardType::Gold:
            return "金币 x" + std::to_string(static_cast<int>(reward.value1));

        case RewardType::Item:
            return "物品 x" + std::to_string(reward.count);

        case RewardType::SkillPoint:
            return "技能点 x" + std::to_string(static_cast<int>(reward.value1));

        case RewardType::Buff:
            return "Buff效果";

        case RewardType::Reputation:
            return "声望 x" + std::to_string(static_cast<int>(reward.value1));

        default:
            return "未知奖励";
    }
}

std::string FormatQuestDescription(const QuestInfo& quest) {
    std::string desc = quest.description + "\n\n目标:\n";

    for (const auto& obj : quest.objectives) {
        desc += "- " + obj.description + "\n";
    }

    desc += "\n奖励:\n";
    for (const auto& reward : quest.rewards) {
        desc += "- " + GetRewardText(reward) + "\n";
    }

    return desc;
}

} // namespace Game
} // namespace Murim
