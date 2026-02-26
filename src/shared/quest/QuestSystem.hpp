// MurimServer - Quest System
// 任务系统 - 主线、支线、日常、成就任务
// 对应legacy: Quest相关系统

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class Player;
class GameObject;
class Monster;

// ========== Quest Constants ==========
constexpr uint16_t MAX_QUEST_LEVEL = 100;
constexpr uint8_t MAX_QUEST_OBJECTIVES = 10;
constexpr uint32_t MAX_ACTIVE_QUESTS = 20;

// ========== Quest State ==========
// 任务状态
enum class QuestState : uint8_t {
    NotStarted = 0,     // 未开始
    InProgress = 1,     // 进行中
    Completed = 2,       // 已完成
    TurnedIn = 3,        // 已提交
    Failed = 4,          // 失败
    Abandoned = 5        // 已放弃
};

// ========== Quest Type ==========
// 任务类型
enum class QuestType : uint8_t {
    Main = 0,            // 主线任务
    Sub = 1,             // 支线任务
    Daily = 2,           // 日常任务
    Weekly = 3,          // 周常任务
    Achievement = 4,     // 成就任务
    Guild = 5,           // 公会任务
    Party = 6,           // 队伍任务
    Repeat = 7           // 可重复任务
};

// ========== Quest Objective Type ==========
// 任务目标类型
enum class ObjectiveType : uint8_t {
    KillMonster = 0,     // 击杀怪物
    CollectItem = 1,      // 收集物品
    TalkToNPC = 2,        // 与NPC对话
    ReachLocation = 3,    // 到达地点
    UseSkill = 4,         // 使用技能
    CraftItem = 5,        // 制作物品
    Escort = 6,           // 护送NPC
    Minigame = 7,         // 小游戏
    Custom = 8            // 自定义条件
};

// ========== Quest Reward Type ==========
// 任务奖励类型
enum class RewardType : uint8_t {
    Experience = 0,       // 经验
    Gold = 1,             // 金币
    Item = 2,             // 物品
    SkillPoint = 3,       // 技能点
    Buff = 4,             // Buff效果
    Reputation = 5,       // 声望
    Custom = 6            // 自定义奖励
};

// ========== Quest Objective ==========
// 任务目标
struct QuestObjective {
    uint32_t objective_id;       // 目标ID
    ObjectiveType type;         // 目标类型
    uint32_t target_id;          // 目标参数（怪物ID、物品ID等）
    uint16_t target_count;       // 需要的数量
    uint16_t current_count;      // 当前数量
    bool is_completed;          // 是否完成
    std::string description;    // 描述

    QuestObjective()
        : objective_id(0), type(ObjectiveType::KillMonster),
          target_id(0), target_count(1), current_count(0),
          is_completed(false) {}

    bool CheckCompletion() const {
        return current_count >= target_count;
    }

    void Progress(uint16_t count = 1) {
        current_count += count;
        if (current_count > target_count) {
            current_count = target_count;
        }
        is_completed = CheckCompletion();
    }

    float GetProgress() const {
        if (target_count == 0) return 1.0f;
        return static_cast<float>(current_count) / target_count;
    }
};

// ========== Quest Reward ==========
// 任务奖励
struct QuestReward {
    RewardType type;
    uint32_t reward_id;           // 奖励ID（物品ID等）
    uint32_t count;               // 数量
    float value1;                // 值1（经验值、金币量等）
    float value2;                // 值2
    std::string description;    // 描述

    QuestReward()
        : type(RewardType::Experience), reward_id(0),
          count(1), value1(0), value2(0) {}
};

// ========== Quest Prerequisite ==========
// 任务前置条件
struct QuestPrerequisite {
    enum class Type : uint8_t {
        Level = 0,           // 等级要求
        Quest = 1,           // 前置任务
        Item = 2,             // 拥有物品
        Skill = 3,            // 技能要求
        Reputation = 4,       // 声望要求
        Custom = 5            // 自定义条件
    };

    Type type;
    uint32_t requirement_id;     // 需求ID
    uint32_t requirement_value;   // 需求值（等级、数量等）

    QuestPrerequisite()
        : type(Type::Level), requirement_id(0), requirement_value(0) {}

    bool Check(uint64_t player_id) const;
};

// ========== Quest Info ==========
// 任务信息
struct QuestInfo {
    uint32_t quest_id;           // 任务ID
    std::string quest_name;       // 任务名称
    std::string description;     // 任务描述
    QuestType type;              // 任务类型
    uint16_t required_level;     // 需求等级
    uint16_t suggested_level;    // 建议等级
    uint32_t previous_quest_id;   // 前置任务ID

    // 目标
    std::vector<QuestObjective> objectives;

    // 前置条件
    std::vector<QuestPrerequisite> prerequisites;

    // 奖励
    std::vector<QuestReward> rewards;

    // 下一个任务
    uint32_t next_quest_id;

    // 任务NPC
    uint32_t start_npc_id;        // 接取NPC
    uint32_t end_npc_id;          // 提交NPC

    // 时间限制
    uint32_t time_limit;         // 时间限制（秒），0表示无限制
    bool is_shareable;            // 是否可共享

    // 重复任务
    bool is_repeatable;           // 是否可重复
    uint32_t repeat_interval;     // 重复间隔（秒）
    uint32_t max_repeat_count;    // 最大重复次数

    QuestInfo()
        : quest_id(0), type(QuestType::Main),
          required_level(1), suggested_level(1),
          previous_quest_id(0), next_quest_id(0),
          start_npc_id(0), end_npc_id(0),
          time_limit(0), is_shareable(false),
          is_repeatable(false), repeat_interval(0),
          max_repeat_count(0) {}

    // ========== 任务状态检查 ==========
    bool CanAccept(uint64_t player_id) const;
    bool IsCompleted() const;

    // ========== 目标管理 ==========
    void AddObjective(const QuestObjective& objective);
    QuestObjective* GetObjective(uint32_t objective_id);
    bool UpdateObjective(uint32_t objective_id, uint16_t count);
    float GetCompletionRate() const;

    // ========== 前置条件检查 ==========
    bool CheckPrerequisites(uint64_t player_id) const;
};

// ========== Player Quest Data ==========
// 玩家任务数据
struct PlayerQuestData {
    uint32_t quest_id;           // 任务ID
    QuestState state;             // 任务状态
    uint32_t accept_time;         // 接取时间
    uint32_t complete_time;       // 完成时间
    uint32_t last_update_time;    // 最后更新时间

    // 进度缓存
    std::map<uint32_t, uint16_t> objective_progress;

    // 重复任务计数
    uint32_t repeat_count;        // 已完成次数

    PlayerQuestData()
        : quest_id(0), state(QuestState::NotStarted),
          accept_time(0), complete_time(0),
          last_update_time(0), repeat_count(0) {}

    bool IsInProgress() const { return state == QuestState::InProgress; }
    bool IsCompleted() const { return state >= QuestState::Completed; }
};

// ========== Quest ==========
// 任务类
class Quest {
private:
    QuestInfo quest_info_;

public:
    Quest();
    explicit Quest(const QuestInfo& info);
    virtual ~Quest();

    // ========== 信息访问 ==========
    const QuestInfo& GetQuestInfo() const { return quest_info_; }
    void SetQuestInfo(const QuestInfo& info) { quest_info_ = info; }

    // ========== 任务操作 ==========
    virtual bool OnAccept(uint64_t player_id);
    virtual bool OnAbandon(uint64_t player_id);
    virtual bool OnComplete(uint64_t player_id);
    virtual bool OnTurnIn(uint64_t player_id);

    // ========== 目标更新 ==========
    virtual bool OnKillMonster(uint64_t player_id, uint64_t monster_id);
    virtual bool OnCollectItem(uint64_t player_id, uint32_t item_id, uint16_t count);
    virtual bool OnTalkToNPC(uint64_t player_id, uint32_t npc_id);
    virtual bool OnReachLocation(uint64_t player_id, float x, float y, float z);
    virtual bool OnUseSkill(uint64_t player_id, uint32_t skill_id);
    virtual bool OnCraftItem(uint64_t player_id, uint32_t item_id);
    virtual bool OnEscort(uint64_t player_id, uint64_t npc_id);

    // ========== 状态检查 ==========
    virtual bool CheckCompletion(uint64_t player_id) const;
    virtual float GetProgress(uint64_t player_id) const;

    // ========== 奖励发放 ==========
    virtual void GiveRewards(uint64_t player_id);

protected:
    void UpdateObjectiveProgress(uint64_t player_id, uint32_t objective_id, uint16_t count);
};

// ========== Quest Manager ==========
// 任务管理器
class QuestManager {
private:
    std::map<uint32_t, QuestInfo> quest_database_;        // 任务数据库
    std::map<uint64_t, std::vector<PlayerQuestData>> player_quests_;  // 玩家任务

    // 任务链
    std::map<uint32_t, std::vector<uint32_t>> quest_chains_;  // quest_id -> next_quest_ids

    // 自动接取任务
    std::map<uint64_t, std::set<uint32_t>> auto_quests_;

public:
    QuestManager();
    ~QuestManager();

    void Init();
    void Release();

    // ========== 任务数据库 ==========
    bool LoadQuestDatabase();
    const QuestInfo* GetQuestInfo(uint32_t quest_id) const;
    std::vector<uint32_t> GetQuestsByType(QuestType type) const;
    std::vector<uint32_t> GetQuestsByLevel(uint16_t min_level, uint16_t max_level) const;

    // ========== 玩家任务 ==========
    std::vector<PlayerQuestData> GetPlayerQuests(uint64_t player_id) const;
    PlayerQuestData* GetPlayerQuestData(uint64_t player_id, uint32_t quest_id);
    bool HasQuest(uint64_t player_id, uint32_t quest_id) const;

    // ========== 任务操作 ==========
    bool AcceptQuest(uint64_t player_id, uint32_t quest_id);
    bool AbandonQuest(uint64_t player_id, uint32_t quest_id);
    bool CompleteQuest(uint64_t player_id, uint32_t quest_id);
    bool TurnInQuest(uint64_t player_id, uint32_t quest_id);

    // ========== 目标更新 ==========
    bool NotifyKillMonster(uint64_t player_id, uint64_t monster_id);
    bool NotifyCollectItem(uint64_t player_id, uint32_t item_id, uint16_t count);
    bool NotifyTalkToNPC(uint64_t player_id, uint32_t npc_id);
    bool NotifyReachLocation(uint64_t player_id, float x, float y, float z);
    bool NotifyUseSkill(uint64_t player_id, uint32_t skill_id);
    bool NotifyCraftItem(uint64_t player_id, uint32_t item_id);

    // ========== 任务查询 ==========
    std::vector<uint32_t> GetAvailableQuests(uint64_t player_id) const;
    std::vector<uint32_t> GetCompletedQuests(uint64_t player_id) const;
    std::vector<uint32_t> GetActiveQuests(uint64_t player_id) const;

    // ========== 自动任务 ==========
    void AddAutoQuest(uint64_t player_id, uint32_t quest_id);
    void RemoveAutoQuest(uint64_t player_id, uint32_t quest_id);
    bool HasAutoQuest(uint64_t player_id, uint32_t quest_id) const;

    // ========== 任务链 ==========
    std::vector<uint32_t> GetQuestChain(uint32_t quest_id) const;
    uint32_t GetNextQuestInChain(uint32_t quest_id) const;

    // ========== 日常/周常任务 ==========
    void ResetDailyQuests();
    void ResetWeeklyQuests();
};

// ========== Quest Factory ==========
// 任务工厂 - 创建不同类型的任务实例
class QuestFactory {
public:
    static std::unique_ptr<Quest> CreateQuest(const QuestInfo& info);

    static std::unique_ptr<Quest> CreateKillMonsterQuest(const QuestInfo& info);
    static std::unique_ptr<Quest> CreateCollectItemQuest(const QuestInfo& info);
    static std::unique_ptr<Quest> CreateEscortQuest(const QuestInfo& info);
    static std::unique_ptr<Quest> CreateReachLocationQuest(const QuestInfo& info);
};

// ========== Helper Functions ==========

// 检查任务是否可接取
bool CanAcceptQuest(const QuestInfo& quest, uint64_t player_id);

// 检查任务是否可共享
bool CanShareQuest(const QuestInfo& quest);

// 获取任务奖励文本
std::string GetRewardText(const QuestReward& reward);

// 格式化任务描述
std::string FormatQuestDescription(const QuestInfo& quest);

} // namespace Game
} // namespace Murim
