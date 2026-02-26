// Murim MMORPG - Dialogue DAO Implementation

#include "DialogueDAO.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Database {

std::vector<DialogueInfo> DialogueDAO::LoadAll() {
    std::vector<DialogueInfo> dialogues;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return dialogues;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "SELECT dialogue_id, npc_text, npc_id, voice_id, animation_id "
                         "FROM dialogues "
                         "ORDER BY dialogue_id";

        auto result = executor.Query(sql);

        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            spdlog::debug("No dialogues found in database");
            return dialogues;
        }

        for (int i = 0; i < result->RowCount(); ++i) {
            DialogueInfo info;
            info.dialogue_id = result->Get<uint32_t>(i, "dialogue_id").value_or(0);
            info.npc_text = result->Get<std::string>(i, "npc_text").value_or("");
            info.npc_id = result->Get<uint32_t>(i, "npc_id").value_or(0);
            info.voice_id = result->Get<std::string>(i, "voice_id").value_or("");
            info.animation_id = result->Get<std::string>(i, "animation_id").value_or("");

            // Load options for this dialogue
            auto options_result = executor.QueryParams(
                "SELECT option_id, dialogue_id, text, next_dialogue_id, quest_id, action_type, action_data "
                "FROM dialogue_options "
                "WHERE dialogue_id = $1 "
                "ORDER BY option_id",
                {std::to_string(info.dialogue_id)}
            );

            if (options_result && options_result->RowCount() > 0) {
                for (int j = 0; j < options_result->RowCount(); ++j) {
                    DialogueOptionInfo option_info;
                    option_info.option_id = options_result->Get<uint32_t>(j, "option_id").value_or(0);
                    option_info.dialogue_id = options_result->Get<uint32_t>(j, "dialogue_id").value_or(0);
                    option_info.text = options_result->Get<std::string>(j, "text").value_or("");
                    option_info.next_dialogue_id = options_result->Get<uint32_t>(j, "next_dialogue_id").value_or(0);
                    option_info.quest_id = options_result->Get<uint32_t>(j, "quest_id").value_or(0);
                    option_info.action_type = options_result->Get<uint32_t>(j, "action_type").value_or(0);
                    option_info.action_data = options_result->Get<std::string>(j, "action_data").value_or("");

                    info.options.push_back(option_info);
                }
            }

            dialogues.push_back(info);
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Loaded {} dialogues from database", dialogues.size());

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to load dialogues: {}", e.what());
    }

    return dialogues;
}

std::optional<DialogueInfo> DialogueDAO::LoadById(uint32_t dialogue_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return std::nullopt;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "SELECT dialogue_id, npc_text, npc_id, voice_id, animation_id "
                         "FROM dialogues "
                         "WHERE dialogue_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(dialogue_id)});

        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return std::nullopt;
        }

        DialogueInfo info;
        info.dialogue_id = result->Get<uint32_t>(0, "dialogue_id").value_or(0);
        info.npc_text = result->Get<std::string>(0, "npc_text").value_or("");
        info.npc_id = result->Get<uint32_t>(0, "npc_id").value_or(0);
        info.voice_id = result->Get<std::string>(0, "voice_id").value_or("");
        info.animation_id = result->Get<std::string>(0, "animation_id").value_or("");

        // Load options
        auto options_result = executor.QueryParams(
            "SELECT option_id, dialogue_id, text, next_dialogue_id, quest_id, action_type, action_data "
            "FROM dialogue_options "
            "WHERE dialogue_id = $1 "
            "ORDER BY option_id",
            {std::to_string(dialogue_id)}
        );

        if (options_result && options_result->RowCount() > 0) {
            for (int j = 0; j < options_result->RowCount(); ++j) {
                DialogueOptionInfo option_info;
                option_info.option_id = options_result->Get<uint32_t>(j, "option_id").value_or(0);
                option_info.dialogue_id = options_result->Get<uint32_t>(j, "dialogue_id").value_or(0);
                option_info.text = options_result->Get<std::string>(j, "text").value_or("");
                option_info.next_dialogue_id = options_result->Get<uint32_t>(j, "next_dialogue_id").value_or(0);
                option_info.quest_id = options_result->Get<uint32_t>(j, "quest_id").value_or(0);
                option_info.action_type = options_result->Get<uint32_t>(j, "action_type").value_or(0);
                option_info.action_data = options_result->Get<std::string>(j, "action_data").value_or("");

                info.options.push_back(option_info);
            }
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        return info;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to load dialogue {}: {}", dialogue_id, e.what());
        return std::nullopt;
    }
}

std::vector<DialogueInfo> DialogueDAO::LoadByNPC(uint32_t npc_id) {
    std::vector<DialogueInfo> dialogues;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return dialogues;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "SELECT dialogue_id, npc_text, npc_id, voice_id, animation_id "
                         "FROM dialogues "
                         "WHERE npc_id = $1 "
                         "ORDER BY dialogue_id";

        auto result = executor.QueryParams(sql, {std::to_string(npc_id)});

        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return dialogues;
        }

        for (int i = 0; i < result->RowCount(); ++i) {
            DialogueInfo info;
            info.dialogue_id = result->Get<uint32_t>(i, "dialogue_id").value_or(0);
            info.npc_text = result->Get<std::string>(i, "npc_text").value_or("");
            info.npc_id = result->Get<uint32_t>(i, "npc_id").value_or(0);
            info.voice_id = result->Get<std::string>(i, "voice_id").value_or("");
            info.animation_id = result->Get<std::string>(i, "animation_id").value_or("");

            // Load options
            auto options_result = executor.QueryParams(
                "SELECT option_id, dialogue_id, text, next_dialogue_id, quest_id, action_type, action_data "
                "FROM dialogue_options "
                "WHERE dialogue_id = $1 "
                "ORDER BY option_id",
                {std::to_string(info.dialogue_id)}
            );

            if (options_result && options_result->RowCount() > 0) {
                for (int j = 0; j < options_result->RowCount(); ++j) {
                    DialogueOptionInfo option_info;
                    option_info.option_id = options_result->Get<uint32_t>(j, "option_id").value_or(0);
                    option_info.dialogue_id = options_result->Get<uint32_t>(j, "dialogue_id").value_or(0);
                    option_info.text = options_result->Get<std::string>(j, "text").value_or("");
                    option_info.next_dialogue_id = options_result->Get<uint32_t>(j, "next_dialogue_id").value_or(0);
                    option_info.quest_id = options_result->Get<uint32_t>(j, "quest_id").value_or(0);
                    option_info.action_type = options_result->Get<uint32_t>(j, "action_type").value_or(0);
                    option_info.action_data = options_result->Get<std::string>(j, "action_data").value_or("");

                    info.options.push_back(option_info);
                }
            }

            dialogues.push_back(info);
        }

        ConnectionPool::Instance().ReturnConnection(conn);

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to load dialogues for NPC {}: {}", npc_id, e.what());
    }

    return dialogues;
}

bool DialogueDAO::Create(const DialogueInfo& dialogue_info) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "INSERT INTO dialogues (npc_text, npc_id, voice_id, animation_id) "
                         "VALUES ($1, $2, $3, $4)";

        bool success = executor.ExecuteParams(sql, {
            dialogue_info.npc_text,
            std::to_string(dialogue_info.npc_id),
            dialogue_info.voice_id,
            dialogue_info.animation_id
        });

        ConnectionPool::Instance().ReturnConnection(conn);
        return success;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to create dialogue: {}", e.what());
        return false;
    }
}

bool DialogueDAO::Update(const DialogueInfo& dialogue_info) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "UPDATE dialogues SET npc_text = $1, npc_id = $2, voice_id = $3, animation_id = $4 "
                         "WHERE dialogue_id = $5";

        bool success = executor.ExecuteParams(sql, {
            dialogue_info.npc_text,
            std::to_string(dialogue_info.npc_id),
            dialogue_info.voice_id,
            dialogue_info.animation_id,
            std::to_string(dialogue_info.dialogue_id)
        });

        ConnectionPool::Instance().ReturnConnection(conn);
        return success;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to update dialogue: {}", e.what());
        return false;
    }
}

bool DialogueDAO::Delete(uint32_t dialogue_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "DELETE FROM dialogues WHERE dialogue_id = $1";

        bool success = executor.ExecuteParams(sql, {std::to_string(dialogue_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        return success;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to delete dialogue: {}", e.what());
        return false;
    }
}

bool DialogueDAO::CreateOption(const DialogueOptionInfo& option_info) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "INSERT INTO dialogue_options (dialogue_id, text, next_dialogue_id, quest_id, action_type, action_data) "
                         "VALUES ($1, $2, $3, $4, $5, $6)";

        bool success = executor.ExecuteParams(sql, {
            std::to_string(option_info.dialogue_id),
            option_info.text,
            std::to_string(option_info.next_dialogue_id),
            std::to_string(option_info.quest_id),
            std::to_string(option_info.action_type),
            option_info.action_data
        });

        ConnectionPool::Instance().ReturnConnection(conn);
        return success;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to create dialogue option: {}", e.what());
        return false;
    }
}

bool DialogueDAO::UpdateOption(const DialogueOptionInfo& option_info) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "UPDATE dialogue_options SET text = $1, next_dialogue_id = $2, quest_id = $3, action_type = $4, action_data = $5 "
                         "WHERE option_id = $6";

        bool success = executor.ExecuteParams(sql, {
            option_info.text,
            std::to_string(option_info.next_dialogue_id),
            std::to_string(option_info.quest_id),
            std::to_string(option_info.action_type),
            option_info.action_data,
            std::to_string(option_info.option_id)
        });

        ConnectionPool::Instance().ReturnConnection(conn);
        return success;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to update dialogue option: {}", e.what());
        return false;
    }
}

bool DialogueDAO::DeleteOption(uint32_t option_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "DELETE FROM dialogue_options WHERE option_id = $1";

        bool success = executor.ExecuteParams(sql, {std::to_string(option_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        return success;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to delete dialogue option: {}", e.what());
        return false;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
