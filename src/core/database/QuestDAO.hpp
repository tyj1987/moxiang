#pragma once

#include "shared/quest/Quest.hpp"
#include <vector>
#include <optional>
#include <unordered_map>

namespace Murim {
namespace Core {
namespace Database {

/**
 * @brief 任务数据访问对象
 *
 * 负责任务数据的数据库操作
 * 对应 legacy: [CC]Quest/QuestManager.cpp
 */
class QuestDAO {
public:
    // ========== 任务模板操作 ==========

    /**
     * @brief 加载任务模板
     * @param quest_id 任务ID
     * @return 任务数据
     */
    static std::optional<Game::Quest> Load(uint32_t quest_id);

    /**
     * @brief 加载所有任务模板
     * @return 所有任务数据
     */
    static std::vector<Game::Quest> LoadAll();

    /**
     * @brief 检查任务是否存在
     * @param quest_id 任务ID
     * @return 是否存在
     */
    static bool Exists(uint32_t quest_id);

    // ========== 角色任务操作 ==========

    /**
     * @brief 加载角色的所有任务
     * @param character_id 角色ID
     * @return 任务列表
     */
    static std::vector<Game::Quest> LoadCharacterQuests(uint64_t character_id);

    /**
     * @brief 加载角色的单个任务
     * @param character_id 角色ID
     * @param quest_id 任务ID
     * @return 任务数据
     */
    static std::optional<Game::Quest> LoadCharacterQuest(uint64_t character_id, uint32_t quest_id);

    /**
     * @brief 保存角色任务
     * @param character_id 角色ID
     * @param quest 任务数据
     * @return 是否成功
     */
    static bool SaveCharacterQuest(uint64_t character_id, const Game::Quest& quest);

    /**
     * @brief 删除角色任务
     * @param character_id 角色ID
     * @param quest_id 任务ID
     * @return 是否成功
     */
    static bool DeleteCharacterQuest(uint64_t character_id, uint32_t quest_id);

    /**
     * @brief 检查角色是否有任务
     * @param character_id 角色ID
     * @param quest_id 任务ID
     * @return 是否存在
     */
    static bool HasCharacterQuest(uint64_t character_id, uint32_t quest_id);

    // ========== 任务进度操作 ==========

    /**
     * @brief 更新任务状态
     * @param character_id 角色ID
     * @param quest_id 任务ID
     * @param status 任务状态
     * @return 是否成功
     */
    static bool UpdateQuestStatus(
        uint64_t character_id,
        uint32_t quest_id,
        Game::QuestStatus status
    );

    /**
     * @brief 更新子任务进度
     * @param character_id 角色ID
     * @param quest_id 任务ID
     * @param sub_quest_id 子任务ID
     * @param condition_id 条件ID
     * @param current_count 当前进度
     * @return 是否成功
     */
    static bool UpdateSubQuestProgress(
        uint64_t character_id,
        uint32_t quest_id,
        uint32_t sub_quest_id,
        uint32_t condition_id,
        uint32_t current_count
    );

    /**
     * @brief 更新当前子任务索引
     * @param character_id 角色ID
     * @param quest_id 任务ID
     * @param current_sub_quest 当前子任务索引
     * @return 是否成功
     */
    static bool UpdateCurrentSubQuest(
        uint64_t character_id,
        uint32_t quest_id,
        uint32_t current_sub_quest
    );

    // ========== 任务完成记录 ==========

    /**
     * @brief 记录任务完成
     * @param character_id 角色ID
     * @param quest_id 任务ID
     * @param complete_time 完成时间
     * @return 是否成功
     */
    static bool RecordQuestCompletion(
        uint64_t character_id,
        uint32_t quest_id,
        const std::chrono::system_clock::time_point& complete_time
    );

    /**
     * @brief 检查任务是否已完成
     * @param character_id 角色ID
     * @param quest_id 任务ID
     * @return 是否已完成
     */
    static bool IsQuestCompleted(uint64_t character_id, uint32_t quest_id);

    /**
     * @brief 获取已完成任务数量
     * @param character_id 角色ID
     * @return 已完成任务数量
     */
    static size_t GetCompletedQuestCount(uint64_t character_id);

    // ========== 日常任务 ==========

    /**
     * @brief 重置日常任务
     * @param character_id 角色ID
     * @return 重置的数量
     */
    static size_t ResetDailyQuests(uint64_t character_id);

    /**
     * @brief 检查日常任务完成次数
     * @param character_id 角色ID
     * @param quest_id 任务ID
     * @param date 日期 (YYYY-MM-DD)
     * @return 完成次数
     */
    static uint32_t GetDailyQuestCompletionCount(
        uint64_t character_id,
        uint32_t quest_id,
        const std::string& date
    );
};

} // namespace Database
} // namespace Core
} // namespace Murim
