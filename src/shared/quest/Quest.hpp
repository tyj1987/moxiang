#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <chrono>

namespace Murim {
namespace Game {

/**
 * @brief 任务类型
 */
enum class QuestType : uint8_t {
    kMain = 1,           // 主线任务
    kSub = 2,            // 支线任务
    kDaily = 3,          // 日常任务
    kWeekly = 4,         // 周常任务
    kGuild = 5,          // 公会任务
    kInstance = 6,       // 副本任务
    kEvent = 7           // 活动任务
};

/**
 * @brief 任务状态
 */
enum class QuestStatus : uint8_t {
    kNotStarted = 0,     // 未开始
    kInProgress = 1,     // 进行中
    kCompleted = 2,      // 已完成
    kFailed = 3,         // 已失败
    kAbandoned = 4       // 已放弃
};

/**
 * @brief 任务事件类型
 */
enum class QuestEventType : uint8_t {
    kNone = 0,
    kNpcTalk = 1,        // NPC 对话
    kMonsterKill = 2,    // 击杀怪物
    kItemCollect = 3,    // 收集物品
    kItemUse = 4,        // 使用物品
    kLevelReach = 5,     // 达到等级
    kMapEnter = 6,       // 进入地图
    kPlayerDie = 7,      // 玩家死亡
    kTimeElapsed = 8,    // 时间流逝
    kSkillLearn = 9,     // 学习技能
    kCustom = 99         // 自定义事件
};

/**
 * @brief 任务执行动作
 */
enum class QuestExecuteType : uint8_t {
    kNone = 0,
    kStartQuest = 1,     // 开始任务
    kEndQuest = 2,       // 结束任务
    kStartSub = 3,       // 开始子任务
    kEndSub = 4,         // 结束子任务
    kGiveItem = 5,       // 给予物品
    kTakeItem = 6,       // 拿走物品
    kGiveExp = 7,        // 给予经验
    kGiveMoney = 8,      // 给予金钱
    kTeleport = 9,       // 传送
    kShowMessage = 10,   // 显示消息
    kLearnSkill = 11,    // 学习技能
    kCustom = 99         // 自定义动作
};

/**
 * @brief 任务条件
 */
struct QuestCondition {
    uint32_t condition_id;          // 条件ID
    QuestEventType event_type;      // 事件类型
    uint32_t target_id;             // 目标ID (NPC ID, 怪物 ID, 物品 ID 等)
    uint32_t required_count;        // 需要的数量
    uint32_t current_count;         // 当前进度

    /**
     * @brief 检查条件是否满足
     */
    bool IsSatisfied() const {
        return current_count >= required_count;
    }

    /**
     * @brief 获取进度百分比
     */
    float GetProgress() const {
        if (required_count == 0) {
            return 1.0f;
        }
        return static_cast<float>(current_count) / static_cast<float>(required_count);
    }
};

/**
 * @brief 任务奖励
 */
struct QuestReward {
    uint32_t item_id;               // 物品ID
    uint32_t item_count;            // 物品数量
    uint32_t exp;                   // 经验值
    uint32_t money;                 // 金钱
    uint32_t skill_id;              // 技能ID (学习技能奖励)
    uint32_t reputation;            // 声望值
};

/**
 * @brief 子任务
 */
struct SubQuest {
    uint32_t sub_quest_id;          // 子任务ID
    std::string title;              // 标题
    std::string description;        // 描述

    std::vector<QuestCondition> conditions;  // 完成条件
    std::vector<QuestReward> rewards;        // 奖励

    QuestStatus status;             // 状态
    std::chrono::system_clock::time_point start_time;  // 开始时间
    std::chrono::system_clock::time_point complete_time; // 完成时间

    /**
     * @brief 检查是否完成
     */
    bool IsCompleted() const {
        for (const auto& condition : conditions) {
            if (!condition.IsSatisfied()) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief 获取总进度
     */
    float GetTotalProgress() const {
        if (conditions.empty()) {
            return 0.0f;
        }

        float total_progress = 0.0f;
        for (const auto& condition : conditions) {
            total_progress += condition.GetProgress();
        }
        return total_progress / static_cast<float>(conditions.size());
    }
};

/**
 * @brief 任务数据
 */
struct Quest {
    uint32_t quest_id;              // 任务ID
    std::string name;               // 任务名称
    std::string description;        // 任务描述
    QuestType type;                 // 任务类型

    uint16_t required_level;        // 需求等级
    std::vector<uint32_t> required_quests;  // 前置任务

    std::vector<SubQuest> sub_quests;  // 子任务列表

    QuestStatus status;             // 任务状态
    uint32_t current_sub_quest;     // 当前子任务索引

    std::chrono::system_clock::time_point accept_time;   // 接受时间
    std::chrono::system_clock::time_point complete_time; // 完成时间

    // 时间限制
    std::optional<uint32_t> time_limit_seconds;  // 时间限制(秒)
    std::optional<std::chrono::system_clock::time_point> deadline;  // 截止时间

    /**
     * @brief 是否可以接受
     */
    bool CanAccept(uint16_t player_level) const {
        // 检查等级
        if (player_level < required_level) {
            return false;
        }

        // 检查状态
        if (status != QuestStatus::kNotStarted) {
            return false;
        }

        return true;
    }

    /**
     * @brief 是否可以放弃
     */
    bool CanAbandon() const {
        return status == QuestStatus::kInProgress;
    }

    /**
     * @brief 获取当前子任务
     */
    SubQuest* GetCurrentSubQuest() {
        if (current_sub_quest >= sub_quests.size()) {
            return nullptr;
        }
        return &sub_quests[current_sub_quest];
    }

    /**
     * @brief 检查是否超时
     */
    bool IsTimeout() const {
        if (!deadline.has_value()) {
            return false;
        }
        return std::chrono::system_clock::now() > deadline.value();
    }

    /**
     * @brief 获取剩余时间(秒)
     */
    std::optional<uint32_t> GetRemainingTime() const {
        if (!deadline.has_value()) {
            return std::nullopt;
        }

        auto now = std::chrono::system_clock::now();
        auto deadline_value = deadline.value();
        if (now >= deadline_value) {
            return 0;
        }

        auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
            deadline_value - now
        ).count();

        return static_cast<uint32_t>(remaining);
    }
};

/**
 * @brief 任务管理器
 *
 * 负责任务的接受、完成、进度追踪
 * 对应 legacy: QuestManager.cpp
 */
class QuestManager {
public:
    /**
     * @brief 获取单例实例
     */
    static QuestManager& Instance();

    /**
     * @brief 处理任务逻辑
     */
    void Process();

    // ========== 任务加载 ==========

    /**
     * @brief 初始化任务管理器
     */
    void Initialize();

    /**
     * @brief 加载任务数据
     */
    void LoadQuests();

    // ========== 任务查询 ==========

    /**
     * @brief 获取任务信息
     */
    std::optional<Quest> GetQuest(uint32_t quest_id);

    /**
     * @brief 获取角色的所有任务
     */
    std::vector<Quest> GetCharacterQuests(uint64_t character_id);

    /**
     * @brief 获取角色的可接任务列表
     */
    std::vector<Quest> GetAvailableQuests(uint64_t character_id, uint16_t player_level);

    // ========== 任务操作 ==========

    /**
     * @brief 接受任务
     */
    bool AcceptQuest(uint64_t character_id, uint32_t quest_id);

    /**
     * @brief 放弃任务
     */
    bool AbandonQuest(uint64_t character_id, uint32_t quest_id);

    /**
     * @brief 完成任务
     */
    bool CompleteQuest(uint64_t character_id, uint32_t quest_id);

    /**
     * @brief 提交任务
     */
    bool TurnInQuest(uint64_t character_id, uint32_t quest_id, uint64_t npc_id);

    // ========== 任务进度 ==========

    /**
     * @brief 更新任务进度
     */
    void UpdateQuestProgress(
        uint64_t character_id,
        QuestEventType event_type,
        uint32_t target_id,
        uint32_t count = 1
    );

    /**
     * @brief 检查任务完成
     */
    void CheckQuestCompletion(uint64_t character_id, uint32_t quest_id);

    // ========== 任务事件 ==========

    /**
     * @brief 触发任务事件 (NPC 对话)
     */
    void OnNpcTalk(uint64_t character_id, uint64_t npc_id);

    /**
     * @brief 触发任务事件 (击杀怪物)
     */
    void OnMonsterKill(uint64_t character_id, uint32_t monster_id);

    /**
     * @brief 触发任务事件 (收集物品)
     */
    void OnItemCollect(uint64_t character_id, uint32_t item_id, uint32_t count);

    /**
     * @brief 触发任务事件 (等级提升)
     */
    void OnLevelUp(uint64_t character_id, uint16_t new_level);

    /**
     * @brief 触发任务事件 (进入地图)
     */
    void OnMapEnter(uint64_t character_id, uint32_t map_id);

    // ========== 任务执行 ==========

    /**
     * @brief 执行任务动作
     */
    bool ExecuteQuestAction(
        uint64_t character_id,
        QuestExecuteType action_type,
        const std::vector<uint32_t>& params
    );

    // ========== 缓存管理 ==========

    /**
     * @brief 缓存统计信息
     */
    struct CacheStats {
        uint64_t hits;
        uint64_t misses;
        uint64_t updates;
        double hit_rate;  // hits / (hits + misses)
        uint64_t total_requests;
    };

    /**
     * @brief 获取缓存统计信息
     */
    CacheStats GetCacheStats() const;

    /**
     * @brief 重置缓存统计
     */
    void ResetCacheStats();

    /**
     * @brief 打印缓存统计信息
     */
    void PrintCacheStats() const;

private:
    QuestManager() = default;
    ~QuestManager() = default;
    QuestManager(const QuestManager&) = delete;
    QuestManager& operator=(const QuestManager&) = delete;

    // 任务缓存
    std::unordered_map<uint32_t, Quest> quests_;

    // 角色任务缓存 (character_id -> quests)
    std::unordered_map<uint64_t, std::vector<Quest>> character_quests_cache_;

    // 缓存时间戳 (character_id -> last_load_time)
    std::unordered_map<uint64_t, std::chrono::system_clock::time_point> cache_timestamps_;

    // 缓存有效期（秒）- 5分钟后过期
    static constexpr int CACHE_EXPIRY_SECONDS = 300;

    // 缓存统计
    uint64_t cache_hits_ = 0;
    uint64_t cache_misses_ = 0;
    uint64_t cache_updates_ = 0;

    bool initialized_ = false;

    /**
     * @brief 给予任务奖励
     */
    void GiveQuestRewards(
        uint64_t character_id,
        const Quest& quest,
        const SubQuest& sub_quest
    );

    /**
     * @brief 保存任务进度到数据库
     */
    void SaveQuestProgress(uint64_t character_id, const Quest& quest);

    /**
     * @brief 从数据库加载任务进度
     */
    void LoadQuestProgress(uint64_t character_id);

    /**
     * @brief 使角色任务缓存失效
     */
    void InvalidateCharacterQuestCache(uint64_t character_id);

    /**
     * @brief 检查缓存是否过期
     */
    bool IsCacheExpired(uint64_t character_id) const;
};

} // namespace Game
} // namespace Murim
