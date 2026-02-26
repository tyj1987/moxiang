#include "QuestDAO.hpp"
#include "DatabaseUtils.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"
#include "core/spdlog_wrapper.hpp"
#include <sstream>

namespace Murim {
namespace Core {
namespace Database {

// Shorten type names
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

// ========== 任务模板操作 ==========

std::optional<Game::Quest> QuestDAO::Load(uint32_t quest_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT quest_id, title, description, quest_type, required_level "
            "FROM quest_templates "
            "WHERE quest_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(quest_id)});

        if (result->RowCount() == 0) {
            spdlog::warn("Quest template not found: {}", quest_id);
            return std::nullopt;
        }

        Game::Quest quest;

        quest.quest_id = result->Get<uint32_t>(0, "quest_id").value_or(0);
        quest.name = result->GetValue(0, "title");  // 列名是 title 不是 name
        quest.description = result->GetValue(0, "description");
        quest.type = static_cast<Game::QuestType>(result->Get<uint8_t>(0, "quest_type").value_or(0));
        quest.required_level = result->Get<uint16_t>(0, "required_level").value_or(0);
        quest.status = Game::QuestStatus::kNotStarted;
        quest.current_sub_quest = 0;

        // 加载子任务
        try {
            std::string sql2 = "SELECT sub_quest_id, title, description, status "
                "FROM quest_sub_templates "
                "WHERE quest_id = $1 "
                "ORDER BY sub_quest_id";

            auto sub_result = executor.QueryParams(sql2, {std::to_string(quest_id)});

            for (int i = 0; i < sub_result->RowCount(); ++i) {
                Game::SubQuest sub_quest;
                sub_quest.sub_quest_id = sub_result->Get<uint32_t>(i, "sub_quest_id").value_or(0);
                sub_quest.title = sub_result->GetValue(i, "title");
                sub_quest.description = sub_result->GetValue(i, "description");
                sub_quest.status = static_cast<Game::QuestStatus>(sub_result->Get<uint8_t>(i, "status").value_or(0));

                // 加载条件
                try {
                    std::string sql3 = "SELECT condition_id, event_type, target_id, required_count, current_count "
                        "FROM quest_conditions "
                        "WHERE quest_id = $1 AND sub_quest_id = $2 "
                        "ORDER BY condition_id";

                    auto cond_result = executor.QueryParams(sql3, {std::to_string(quest_id), std::to_string(sub_quest.sub_quest_id)});

                    for (int j = 0; j < cond_result->RowCount(); ++j) {
                        Game::QuestCondition condition;
                        condition.condition_id = cond_result->Get<uint32_t>(j, "condition_id").value_or(0);
                        condition.event_type = static_cast<Game::QuestEventType>(cond_result->Get<uint8_t>(j, "event_type").value_or(0));
                        condition.target_id = cond_result->Get<uint32_t>(j, "target_id").value_or(0);
                        condition.required_count = cond_result->Get<uint32_t>(j, "required_count").value_or(0);
                        condition.current_count = cond_result->Get<uint32_t>(j, "current_count").value_or(0);

                        sub_quest.conditions.push_back(condition);
                    }
                } catch (const std::exception& e) {
                    spdlog::warn("Failed to load conditions for sub_quest {} of quest {}: {}",
                                sub_quest.sub_quest_id, quest_id, e.what());
                }

                quest.sub_quests.push_back(sub_quest);
            }
        } catch (const std::exception& e) {
            spdlog::warn("Failed to load sub_quests for quest {}: {}", quest_id, e.what());
        }

        spdlog::debug("Loaded quest template: {} ({})", quest.name, quest.quest_id);

        return quest;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load quest template {}: {}", quest_id, e.what());
        return std::nullopt;
    }
}

std::vector<Game::Quest> QuestDAO::LoadAll() {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<Game::Quest> quests;

    try {
        auto result = executor.Query(
            "SELECT quest_id FROM quest_templates ORDER BY quest_id"
        );

        quests.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            uint32_t quest_id = result->Get<uint32_t>(i, "quest_id").value_or(0);
            auto quest_opt = Load(quest_id);
            if (quest_opt.has_value()) {
                quests.push_back(*quest_opt);
            }
        }

        spdlog::info("Loaded {} quest templates", quests.size());

    } catch (const std::exception& e) {
        spdlog::error("Failed to load quest templates: {}", e.what());
    }

    return quests;
}

bool QuestDAO::Exists(uint32_t quest_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT 1 FROM quest_templates WHERE quest_id = $1";
        auto result = executor.QueryParams(sql, {std::to_string(quest_id)});

        return result->RowCount() > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check quest existence {}: {}", quest_id, e.what());
        return false;
    }
}

// ========== 角色任务操作 ==========

std::vector<Game::Quest> QuestDAO::LoadCharacterQuests(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<Game::Quest> quests;

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT quest_id FROM character_quests "
            "WHERE character_id = $1 "
            "ORDER BY start_time";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        quests.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            uint32_t quest_id = result->Get<uint32_t>(i, "quest_id").value_or(0);

            // 加载角色任务进度（包含模板数据）
            auto quest_opt = LoadCharacterQuest(character_id, quest_id);

            if (quest_opt.has_value()) {
                quests.push_back(*quest_opt);
            }
        }

        spdlog::debug("Loaded {} quests for character {}", quests.size(), character_id);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load character quests {}: {}", character_id, e.what());
    }

    return quests;
}

std::optional<Game::Quest> QuestDAO::LoadCharacterQuest(uint64_t character_id, uint32_t quest_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT status, current_sub_quest, start_time, complete_time "
            "FROM character_quests "
            "WHERE character_id = $1 AND quest_id = $2";

        auto result = executor.QueryParams(sql, {std::to_string(character_id), std::to_string(quest_id)});

        if (result->RowCount() == 0) {
            return std::nullopt;
        }

        auto quest = Load(quest_id);

        if (!quest.has_value()) {
            return std::nullopt;
        }

        quest->status = static_cast<Game::QuestStatus>(result->Get<uint8_t>(0, "status").value_or(0));
        quest->current_sub_quest = result->Get<uint32_t>(0, "current_sub_quest").value_or(0);

        // 解析时间戳 - start_time 映射到 accept_time
        if (!result->IsNull(0, result->ColumnIndex("start_time"))) {
            auto time_str = result->GetValue(0, "start_time");
            auto time_point = DatabaseUtils::ParseTimestamp(time_str);
            if (time_point.has_value()) {
                quest->accept_time = time_point.value();
            }
        }

        if (!result->IsNull(0, result->ColumnIndex("complete_time"))) {
            auto time_str = result->GetValue(0, "complete_time");
            auto time_point = DatabaseUtils::ParseTimestamp(time_str);
            if (time_point.has_value()) {
                quest->complete_time = time_point.value();
            }
        }

        // 加载子任务进度
        for (auto& sub_quest : quest->sub_quests) {
            // 使用参数化查询防止SQL注入
            std::string sql2 = "SELECT status FROM character_sub_quests "
                "WHERE character_id = $1 AND quest_id = $2 AND sub_quest_id = $3";

            auto sub_result = executor.QueryParams(sql2, {std::to_string(character_id), std::to_string(quest_id), std::to_string(sub_quest.sub_quest_id)});

            if (sub_result->RowCount() > 0) {
                sub_quest.status = static_cast<Game::QuestStatus>(sub_result->Get<uint8_t>(0, "status").value_or(0));
            }

            // 加载条件进度
            for (auto& condition : sub_quest.conditions) {
                // 使用参数化查询防止SQL注入
                std::string sql3 = "SELECT current_count FROM character_quest_progress "
                    "WHERE character_id = $1 AND quest_id = $2 AND sub_quest_id = $3 AND condition_id = $4";

                auto cond_result = executor.QueryParams(sql3, {std::to_string(character_id), std::to_string(quest_id), std::to_string(sub_quest.sub_quest_id), std::to_string(condition.condition_id)});

                if (cond_result->RowCount() > 0) {
                    condition.current_count = cond_result->Get<uint32_t>(0, "current_count").value_or(0);
                }
            }
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        return quest;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to load character quest {} for {}: {}", quest_id, character_id, e.what());
        return std::nullopt;
    }
}

bool QuestDAO::SaveCharacterQuest(uint64_t character_id, const Game::Quest& quest) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        executor.BeginTransaction();

        // 保存任务基本信息
        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO character_quests (character_id, quest_id, status, current_sub_quest, start_time) "
            "VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP) "
            "ON CONFLICT (character_id, quest_id) "
            "DO UPDATE SET status = $3, current_sub_quest = $4";

        executor.ExecuteParams(sql, {std::to_string(character_id), std::to_string(quest.quest_id),
                              std::to_string(static_cast<uint8_t>(quest.status)), std::to_string(quest.current_sub_quest)});

        // 保存子任务进度
        for (const auto& sub_quest : quest.sub_quests) {
            // 使用参数化查询防止SQL注入
            std::string sql2 = "INSERT INTO character_sub_quests (character_id, quest_id, sub_quest_id, status) "
                "VALUES ($1, $2, $3, $4) "
                "ON CONFLICT (character_id, quest_id, sub_quest_id) "
                "DO UPDATE SET status = $4";

            executor.ExecuteParams(sql2, {std::to_string(character_id), std::to_string(quest.quest_id), std::to_string(sub_quest.sub_quest_id),
                                  std::to_string(static_cast<uint8_t>(sub_quest.status))});

            // 保存条件进度
            for (const auto& condition : sub_quest.conditions) {
                // 使用参数化查询防止SQL注入
                std::string sql3 = "INSERT INTO character_quest_progress "
                    "(character_id, quest_id, sub_quest_id, condition_id, current_count) "
                    "VALUES ($1, $2, $3, $4, $5) "
                    "ON CONFLICT (character_id, quest_id, sub_quest_id, condition_id) "
                    "DO UPDATE SET current_count = $5";

                executor.ExecuteParams(sql3, {std::to_string(character_id), std::to_string(quest.quest_id), std::to_string(sub_quest.sub_quest_id),
                                      std::to_string(condition.condition_id), std::to_string(condition.current_count)});
            }
        }

        executor.Commit();

        spdlog::debug("Saved character {} quest {}", character_id, quest.quest_id);

        ConnectionPool::Instance().ReturnConnection(conn);
        return true;

    } catch (const std::exception& e) {
        executor.Rollback();
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to save character quest {} for {}: {}", quest.quest_id, character_id, e.what());
        return false;
    }
}

bool QuestDAO::DeleteCharacterQuest(uint64_t character_id, uint32_t quest_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        executor.BeginTransaction();

        // 删除条件进度
        // 使用参数化查询防止SQL注入
        executor.ExecuteParams(
            "DELETE FROM character_quest_progress "
            "WHERE character_id = $1 AND quest_id = $2",
            {std::to_string(character_id), std::to_string(quest_id)}
        );

        // 删除子任务进度
        // 使用参数化查询防止SQL注入
        executor.ExecuteParams(
            "DELETE FROM character_sub_quests "
            "WHERE character_id = $1 AND quest_id = $2",
            {std::to_string(character_id), std::to_string(quest_id)}
        );

        // 删除任务
        // 使用参数化查询防止SQL注入
        executor.ExecuteParams(
            "DELETE FROM character_quests "
            "WHERE character_id = $1 AND quest_id = $2",
            {std::to_string(character_id), std::to_string(quest_id)}
        );

        executor.Commit();

        spdlog::info("Deleted character {} quest {}", character_id, quest_id);

        ConnectionPool::Instance().ReturnConnection(conn);
        return true;

    } catch (const std::exception& e) {
        executor.Rollback();
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to delete character quest {} for {}: {}", quest_id, character_id, e.what());
        return false;
    }
}

bool QuestDAO::HasCharacterQuest(uint64_t character_id, uint32_t quest_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT 1 FROM character_quests "
            "WHERE character_id = $1 AND quest_id = $2";

        auto result = executor.QueryParams(sql, {std::to_string(character_id), std::to_string(quest_id)});

        return result->RowCount() > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check character quest {} for {}: {}", quest_id, character_id, e.what());
        return false;
    }
}

// ========== 任务进度操作 ==========

bool QuestDAO::UpdateQuestStatus(
    uint64_t character_id,
    uint32_t quest_id,
    Game::QuestStatus status
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE character_quests "
            "SET status = $1 "
            "WHERE character_id = $2 AND quest_id = $3";

        executor.ExecuteParams(sql, {std::to_string(static_cast<uint8_t>(status)), std::to_string(character_id), std::to_string(quest_id)});

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update quest status: {}", e.what());
        return false;
    }
}

bool QuestDAO::UpdateSubQuestProgress(
    uint64_t character_id,
    uint32_t quest_id,
    uint32_t sub_quest_id,
    uint32_t condition_id,
    uint32_t current_count
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO character_quest_progress "
            "(character_id, quest_id, sub_quest_id, condition_id, current_count) "
            "VALUES ($1, $2, $3, $4, $5) "
            "ON CONFLICT (character_id, quest_id, sub_quest_id, condition_id) "
            "DO UPDATE SET current_count = $5";

        executor.ExecuteParams(sql, {std::to_string(character_id), std::to_string(quest_id), std::to_string(sub_quest_id),
                              std::to_string(condition_id), std::to_string(current_count)});

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update sub quest progress: {}", e.what());
        return false;
    }
}

bool QuestDAO::UpdateCurrentSubQuest(
    uint64_t character_id,
    uint32_t quest_id,
    uint32_t current_sub_quest
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE character_quests "
            "SET current_sub_quest = $1 "
            "WHERE character_id = $2 AND quest_id = $3";

        executor.ExecuteParams(sql, {std::to_string(current_sub_quest), std::to_string(character_id), std::to_string(quest_id)});

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update current sub quest: {}", e.what());
        return false;
    }
}

// ========== 任务完成记录 ==========

bool QuestDAO::RecordQuestCompletion(
    uint64_t character_id,
    uint32_t quest_id,
    const std::chrono::system_clock::time_point& complete_time
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO quest_completion_history (character_id, quest_id, complete_time) "
            "VALUES ($1, $2, CURRENT_TIMESTAMP) "
            "ON CONFLICT (character_id, quest_id) "
            "DO UPDATE SET complete_time = CURRENT_TIMESTAMP, completion_count = completion_count + 1";

        executor.ExecuteParams(sql, {std::to_string(character_id), std::to_string(quest_id)});

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to record quest completion: {}", e.what());
        return false;
    }
}

bool QuestDAO::IsQuestCompleted(uint64_t character_id, uint32_t quest_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT 1 FROM quest_completion_history "
            "WHERE character_id = $1 AND quest_id = $2";

        auto result = executor.QueryParams(sql, {std::to_string(character_id), std::to_string(quest_id)});

        return result->RowCount() > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to check quest completion: {}", e.what());
        return false;
    }
}

size_t QuestDAO::GetCompletedQuestCount(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT COUNT(*) as count FROM quest_completion_history "
            "WHERE character_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        if (result->RowCount() > 0) {
            return result->Get<size_t>(0, "count").value_or(0);
        }

        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get completed quest count: {}", e.what());
        return 0;
    }
}

// ========== 日常任务 ==========

size_t QuestDAO::ResetDailyQuests(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        executor.BeginTransaction();

        // 删除所有进行中的日常任务
        // 使用参数化查询防止SQL注入
        std::string sql = "DELETE FROM character_quests "
            "WHERE character_id = $1 "
            "AND quest_id IN "
            "(SELECT quest_id FROM quest_templates WHERE quest_type = $2) "
            "RETURNING *";

        auto result = executor.QueryParams(sql, {std::to_string(character_id),
                                                   std::to_string(static_cast<uint8_t>(Game::QuestType::kDaily))});

        size_t deleted = result->RowCount();

        executor.Commit();

        spdlog::info("Reset {} daily quests for character {}", deleted, character_id);

        return deleted;

    } catch (const std::exception& e) {
        executor.Rollback();
        spdlog::error("Failed to reset daily quests: {}", e.what());
        return 0;
    }
}

uint32_t QuestDAO::GetDailyQuestCompletionCount(
    uint64_t character_id,
    uint32_t quest_id,
    const std::string& date
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT completion_count FROM quest_completion_history "
            "WHERE character_id = $1 AND quest_id = $2 AND completion_date = $3";

        auto result = executor.QueryParams(sql, {std::to_string(character_id), std::to_string(quest_id), date});

        if (result->RowCount() > 0) {
            return result->Get<uint32_t>(0, "completion_count").value_or(0);
        }

        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get daily quest completion count: {}", e.what());
        return 0;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
