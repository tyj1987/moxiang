#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <functional>
#include "protocol/achievement.pb.h"

namespace Murim {
namespace Game {

/**
 * @brief 成就定义（C++版本）
 */
struct AchievementDefinition {
    uint32_t achievement_id;
    std::string name;
    std::string description;
    murim::AchievementCategory category;
    uint32_t points;
    uint32_t prerequisite_id;
    bool hidden;
    bool serial;

    // 条件类型
    enum class ConditionType {
        NONE,
        LEVEL,
        KILL,
        QUEST,
        ITEM,
        WEALTH,
        DUNGEON,
        SOCIAL
    };
    ConditionType condition_type;

    // 条件数据
    struct ConditionData {
        uint32_t target_level;
        uint32_t target_type_id;
        uint32_t target_count;
        uint32_t min_level;
        uint32_t quest_count;
        uint32_t quest_type;
        uint32_t item_count;
        uint32_t item_quality;
        uint64_t target_gold;
        uint32_t dungeon_id;
        uint32_t clear_count;
        uint32_t max_clear_time;
        uint32_t friend_count;
        uint32_t guild_member_count;
    } condition;

    // 奖励列表
    std::vector<murim::AchievementReward> rewards;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::AchievementDefinition ToProto() const;
};

/**
 * @brief 成就进度（C++版本）
 */
struct AchievementProgress {
    uint32_t achievement_id;
    murim::AchievementStatus status;
    uint32_t current_value;
    uint32_t target_value;
    uint64_t complete_time;
    uint64_t update_time;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::AchievementProgress ToProto() const;
};

/**
 * @brief 玩家成就数据
 */
struct PlayerAchievement {
    uint64_t character_id;
    uint32_t total_points;
    uint32_t completed_count;
    std::unordered_map<uint32_t, AchievementProgress> progress;  // achievement_id -> progress

    /**
     * @brief 获取成就进度
     */
    AchievementProgress* GetProgress(uint32_t achievement_id) {
        auto it = progress.find(achievement_id);
        if (it != progress.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /**
     * @brief 添加或更新成就进度
     */
    void AddOrUpdateProgress(const AchievementProgress& prog) {
        progress[prog.achievement_id] = prog;
    }
};

/**
 * @brief 成就系统管理器
 *
 * 负责成就的注册、进度跟踪、完成检查、奖励发放
 */
class AchievementManager {
public:
    static AchievementManager& Instance();

    bool Initialize();

    // ========== 成就定义管理 ==========

    /**
     * @brief 注册成就定义
     */
    bool RegisterAchievement(const AchievementDefinition& definition);

    /**
     * @brief 获取成就定义
     */
    AchievementDefinition* GetAchievementDefinition(uint32_t achievement_id);

    /**
     * @brief 获取所有成就定义
     */
    std::vector<AchievementDefinition> GetAchievementsByCategory(
        murim::AchievementCategory category);

    // ========== 成就进度查询 ==========

    /**
     * @brief 获取玩家成就数据
     */
    PlayerAchievement* GetPlayerAchievement(uint64_t character_id) const;

    /**
     * @brief 获取或创建玩家成就数据
     */
    PlayerAchievement& GetOrCreatePlayerAchievement(uint64_t character_id);

    /**
     * @brief 获取成就进度
     */
    AchievementProgress* GetAchievementProgress(
        uint64_t character_id,
        uint32_t achievement_id);

    // ========== 成就进度更新 ==========

    /**
     * @brief 更新等级成就进度
     */
    void UpdateLevelProgress(uint64_t character_id, uint32_t level);

    /**
     * @brief 更新击杀成就进度
     */
    void UpdateKillProgress(
        uint64_t character_id,
        uint32_t monster_type_id,
        uint32_t count);

    /**
     * @brief 更新任务成就进度
     */
    void UpdateQuestProgress(uint64_t character_id);

    /**
     * @brief 更新财富成就进度
     */
    void UpdateWealthProgress(uint64_t character_id, uint64_t gold);

    /**
     * @brief 更新副本成就进度
     */
    void UpdateDungeonProgress(
        uint64_t character_id,
        uint32_t dungeon_id,
        uint32_t clear_time);

    /**
     * @brief 更新社交成就进度
     */
    void UpdateSocialProgress(
        uint64_t character_id,
        uint32_t friend_count,
        uint32_t guild_member_count);

    // ========== 成就完成检查 ==========

    /**
     * @brief 检查并更新成就状态
     */
    bool CheckAndCompleteAchievement(
        uint64_t character_id,
        uint32_t achievement_id);

    /**
     * @brief 领取成就奖励
     */
    bool ClaimReward(uint64_t character_id, uint32_t achievement_id);

    /**
     * @brief 检查成就是否已完成
     */
    bool IsCompleted(uint64_t character_id, uint32_t achievement_id) const;

    /**
     * @brief 检查成就是否已领取奖励
     */
    bool IsClaimed(uint64_t character_id, uint32_t achievement_id) const;

    // ========== 成就统计 ==========

    /**
     * @brief 获取玩家总成就点数
     */
    uint32_t GetTotalPoints(uint64_t character_id);

    /**
     * @brief 获取玩家已完成成就数量
     */
    uint32_t GetCompletedCount(uint64_t character_id);

    /**
     * @brief 获取玩家指定类别的成就数量
     */
    uint32_t GetCategoryCount(
        uint64_t character_id,
        murim::AchievementCategory category);

    // ========== 维护操作 ==========

    /**
     * @brief 保存玩家成就数据到数据库
     */
    bool SavePlayerAchievement(uint64_t character_id);

    /**
     * @brief 从数据库加载玩家成就数据
     */
    bool LoadPlayerAchievement(uint64_t character_id);

private:
    AchievementManager();
    ~AchievementManager() = default;
    AchievementManager(const AchievementManager&) = delete;
    AchievementManager& operator=(const AchievementManager&) = delete;

    // ========== 成就定义存储 ==========
    // achievement_id -> AchievementDefinition
    std::unordered_map<uint32_t, AchievementDefinition> achievement_definitions_;

    // ========== 玩家成就数据 ==========
    // character_id -> PlayerAchievement
    std::unordered_map<uint64_t, PlayerAchievement> player_achievements_;

    // ========== 辅助方法 ==========

    /**
     * @brief 检查完成条件
     */
    bool CheckCondition(
        const AchievementDefinition& definition,
        uint32_t current_value);

    /**
     * @brief 发送成就完成通知
     */
    void NotifyCompleted(
        uint64_t character_id,
        const AchievementDefinition& definition);

    /**
     * @brief 发送成就进度更新通知
     */
    void NotifyProgressUpdate(
        uint64_t character_id,
        const AchievementDefinition& definition,
        uint32_t old_value,
        uint32_t new_value);

    /**
     * @brief 解锁后续成就
     */
    void UnlockDependentAchievements(
        uint64_t character_id,
        uint32_t achievement_id);
};

} // namespace Game
} // namespace Murim
