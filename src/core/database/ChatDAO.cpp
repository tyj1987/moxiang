#include "ChatDAO.hpp"
#include "DatabaseUtils.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"
#include "shared/character/Character.hpp"
#include "../spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Database {

// Shorten type names
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

// ========== 聊天消息 ==========

bool ChatDAO::SaveMessage(const Game::ChatMessage& message) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO chat_messages "
            "(message_id, sender_id, sender_name, receiver_id, channel_type, channel_name, "
            "content, send_time) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, CURRENT_TIMESTAMP)";

        executor.ExecuteParams(sql, {std::to_string(message.message_id), std::to_string(message.sender_id),
                              message.sender_name, std::to_string(message.receiver_id),
                              std::to_string(static_cast<uint8_t>(message.channel_type)),
                              message.channel_name, message.content});

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save chat message: {}", e.what());
        return false;
    }
}

std::vector<Game::ChatMessage> ChatDAO::GetChatHistory(
    Game::ChatChannelType channel_type,
    const std::string& channel_name,
    uint32_t count,
    uint64_t before_message_id
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<Game::ChatMessage> messages;

    try {
        std::string sql =
            "SELECT message_id, sender_id, sender_name, receiver_id, "
            "       channel_type, channel_name, content, send_time "
            "FROM chat_messages "
            "WHERE channel_type = $1 AND channel_name = $2";

        auto result = before_message_id > 0
            ? executor.QueryParams(sql + " AND message_id < $3 ORDER BY message_id DESC LIMIT $4",
                {std::to_string(static_cast<uint8_t>(channel_type)),
                 channel_name,
                 std::to_string(before_message_id),
                 std::to_string(count)})
            : executor.QueryParams(sql + " ORDER BY message_id DESC LIMIT $3",
                {std::to_string(static_cast<uint8_t>(channel_type)),
                 channel_name,
                 std::to_string(count)});

        messages.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            Game::ChatMessage message;
            message.message_id = result->Get<uint64_t>(i, "message_id").value_or(0);
            message.sender_id = result->Get<uint64_t>(i, "sender_id").value_or(0);
            message.sender_name = result->GetValue(i, "sender_name");
            message.receiver_id = result->Get<uint64_t>(i, "receiver_id").value_or(0);
            message.channel_type = static_cast<Game::ChatChannelType>(
                result->Get<uint8_t>(i, "channel_type").value_or(0)
            );
            message.channel_name = result->GetValue(i, "channel_name");
            message.content = result->GetValue(i, "content");

            // 解析 send_time 时间戳 - 对应 legacy 时间戳解析
            std::string send_time_str = result->GetValue(i, "send_time");
            if (!send_time_str.empty()) {
                auto time_point = DatabaseUtils::ParseTimestamp(send_time_str);
                if (time_point.has_value()) {
                    message.send_time = time_point.value();
                }
            }

            messages.push_back(message);
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get chat history: {}", e.what());
    }

    return messages;
}

std::vector<Game::ChatMessage> ChatDAO::GetPrivateHistory(
    uint64_t character_id,
    uint64_t target_id,
    uint32_t count
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<Game::ChatMessage> messages;

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT message_id, sender_id, sender_name, receiver_id, "
            "       channel_type, channel_name, content, send_time "
            "FROM chat_messages "
            "WHERE channel_type = 4 "  // kPrivate
            "  AND ((sender_id = $1 AND receiver_id = $2) "
            "    OR (sender_id = $3 AND receiver_id = $4)) "
            "ORDER BY send_time DESC "
            "LIMIT $5";

        auto result = executor.QueryParams(sql, {std::to_string(character_id), std::to_string(target_id),
                                                  std::to_string(target_id), std::to_string(character_id),
                                                  std::to_string(count)});

        messages.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            Game::ChatMessage message;
            message.message_id = result->Get<uint64_t>(i, "message_id").value_or(0);
            message.sender_id = result->Get<uint64_t>(i, "sender_id").value_or(0);
            message.sender_name = result->GetValue(i, "sender_name");
            message.receiver_id = result->Get<uint64_t>(i, "receiver_id").value_or(0);
            message.channel_type = static_cast<Game::ChatChannelType>(
                result->Get<uint8_t>(i, "channel_type").value_or(0)
            );
            message.channel_name = result->GetValue(i, "channel_name");
            message.content = result->GetValue(i, "content");

            // 解析 send_time 时间戳 - 对应 legacy 时间戳解析
            std::string send_time_str = result->GetValue(i, "send_time");
            if (!send_time_str.empty()) {
                auto time_point = DatabaseUtils::ParseTimestamp(send_time_str);
                if (time_point.has_value()) {
                    message.send_time = time_point.value();
                }
            }

            messages.push_back(message);
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get private chat history: {}", e.what());
    }

    return messages;
}

size_t ChatDAO::DeleteOldMessages(uint32_t days) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "DELETE FROM chat_messages "
            "WHERE send_time < CURRENT_TIMESTAMP - INTERVAL '1 day' * $1 "
            "RETURNING COUNT(*) as count";

        auto result = executor.QueryParams(sql, {std::to_string(days)});

        size_t deleted_count = 0;
        if (result->RowCount() > 0) {
            deleted_count = result->Get<size_t>(0, "count").value_or(0);
        }

        spdlog::info("Deleted {} old chat messages ({} days)",
                     deleted_count, days);

        return deleted_count;

    } catch (const std::exception& e) {
        spdlog::error("Failed to delete old messages: {}", e.what());
        return 0;
    }
}

// ========== 聊天频道 ==========

uint64_t ChatDAO::CreateChannel(
    Game::ChatChannelType type,
    const std::string& name,
    const std::string& description,
    uint16_t min_level,
    uint8_t min_vip_level,
    uint32_t cooldown_seconds,
    uint32_t message_max_length
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO chat_channels "
            "(channel_type, name, description, min_level, min_vip_level, "
            "cooldown_seconds, message_max_length, created_time) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, CURRENT_TIMESTAMP) "
            "RETURNING channel_id";

        auto row = executor.QueryOneParams(sql, {std::to_string(static_cast<uint8_t>(type)), name,
                                          description, std::to_string(min_level), std::to_string(min_vip_level),
                                          std::to_string(cooldown_seconds), std::to_string(message_max_length)});

        uint64_t channel_id = row->Get<uint64_t>(0, "channel_id").value_or(0);

        spdlog::info("Chat channel created: id={}, type={}, name={}",
                     channel_id, static_cast<int>(type), name);

        return channel_id;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create chat channel: {}", e.what());
        return 0;
    }
}

bool ChatDAO::CreateChannel(
    uint64_t channel_id,
    uint32_t type,
    const std::string& name,
    const std::string& description
) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO chat_channels "
            "(channel_id, channel_type, name, description, min_level, min_vip_level, "
            "cooldown_seconds, message_max_length, created_time) "
            "VALUES ($1, $2, $3, $4, 1, 0, 0, 200, CURRENT_TIMESTAMP)";

        executor.ExecuteParams(sql, {std::to_string(channel_id), std::to_string(static_cast<uint8_t>(type)), name, description});

        spdlog::info("Chat channel created: id={}, type={}, name={}",
                     channel_id, type, name);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create chat channel: {}", e.what());
        return false;
    }
}

std::optional<Game::ChatChannel> ChatDAO::GetChannelConfig(Game::ChatChannelType type) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT channel_id, channel_type, name, description, min_level, min_vip_level, "
            "       cooldown_seconds, message_max_length "
            "FROM chat_channels "
            "WHERE channel_type = $1 "
            "ORDER BY created_time DESC "
            "LIMIT 1";

        auto row = executor.QueryOneParams(sql, {std::to_string(static_cast<uint8_t>(type))});

        Game::ChatChannel channel;
        channel.type = static_cast<Game::ChatChannelType>(
            row->Get<uint8_t>(0, "channel_type").value_or(0)
        );
        channel.name = row->GetValue(0, "name");
        channel.description = row->GetValue(0, "description");
        channel.min_level = row->Get<uint16_t>(0, "min_level").value_or(1);
        channel.min_vip_level = row->Get<uint8_t>(0, "min_vip_level").value_or(0);
        channel.cooldown_seconds = row->Get<uint32_t>(0, "cooldown_seconds").value_or(0);
        channel.message_max_length = row->Get<uint32_t>(0, "message_max_length").value_or(200);

        return channel;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get channel config: {}", e.what());
        return std::nullopt;
    }
}

bool ChatDAO::UpdateChannelConfig(const Game::ChatChannel& channel) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "UPDATE chat_channels "
            "SET name = $1, description = $2, "
            "min_level = $3, "
            "min_vip_level = $4, "
            "cooldown_seconds = $5, "
            "message_max_length = $6 "
            "WHERE channel_type = $7";

        size_t affected = executor.ExecuteParams(sql, {channel.name, channel.description,
                                                std::to_string(channel.min_level), std::to_string(channel.min_vip_level),
                                                std::to_string(channel.cooldown_seconds),
                                                std::to_string(channel.message_max_length),
                                                std::to_string(static_cast<uint8_t>(channel.type))});

        spdlog::info("Chat channel config updated: type={}, name={}",
                     static_cast<int>(channel.type), channel.name);

        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update channel config: {}", e.what());
        return false;
    }
}

bool ChatDAO::JoinChannel(uint64_t character_id, uint64_t channel_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO channel_members "
            "(channel_id, character_id, join_time) "
            "VALUES ($1, $2, CURRENT_TIMESTAMP) "
            "ON CONFLICT (channel_id, character_id) DO NOTHING";

        executor.ExecuteParams(sql, {std::to_string(channel_id), std::to_string(character_id)});

        spdlog::info("Character {} joined channel {}", character_id, channel_id);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to join channel: {}", e.what());
        return false;
    }
}

bool ChatDAO::LeaveChannel(uint64_t character_id, uint64_t channel_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "DELETE FROM channel_members "
            "WHERE channel_id = $1 AND character_id = $2";

        executor.ExecuteParams(sql, {std::to_string(channel_id), std::to_string(character_id)});

        spdlog::info("Character {} left channel {}", character_id, channel_id);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to leave channel: {}", e.what());
        return false;
    }
}

std::vector<uint64_t> ChatDAO::GetCharacterChannels(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<uint64_t> channel_ids;

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT channel_id FROM channel_members "
            "WHERE character_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        channel_ids.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            channel_ids.push_back(result->Get<uint64_t>(i, "channel_id").value_or(0));
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get character channels: {}", e.what());
    }

    return channel_ids;
}

std::vector<uint64_t> ChatDAO::GetChannelMembers(uint64_t channel_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<uint64_t> character_ids;

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT character_id FROM channel_members "
            "WHERE channel_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(channel_id)});

        character_ids.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            character_ids.push_back(result->Get<uint64_t>(i, "character_id").value_or(0));
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get channel members: {}", e.what());
    }

    return character_ids;
}

// ========== 敏感词过滤 ==========

std::vector<std::string> ChatDAO::LoadBannedWords() {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    std::vector<std::string> banned_words;

    try {
        auto result = executor.Query(
            "SELECT word FROM banned_words "
            "ORDER BY word"
        );

        banned_words.reserve(result->RowCount());

        for (int i = 0; i < result->RowCount(); ++i) {
            banned_words.push_back(result->GetValue(i, "word"));
        }

        spdlog::info("Loaded {} banned words", banned_words.size());

    } catch (const std::exception& e) {
        spdlog::error("Failed to load banned words: {}", e.what());
    }

    return banned_words;
}

bool ChatDAO::AddBannedWord(const std::string& word) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO banned_words (word) "
            "VALUES ($1) "
            "ON CONFLICT (word) DO NOTHING";

        executor.ExecuteParams(sql, {word});

        spdlog::info("Banned word added: {}", word);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to add banned word: {}", e.what());
        return false;
    }
}

bool ChatDAO::RemoveBannedWord(const std::string& word) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "DELETE FROM banned_words "
            "WHERE word = $1";

        executor.ExecuteParams(sql, {word});

        spdlog::info("Banned word removed: {}", word);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to remove banned word: {}", e.what());
        return false;
    }
}

// ========== 聊天统计 ==========

size_t ChatDAO::GetTodayMessageCount(uint64_t character_id, Game::ChatChannelType channel_type) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT COUNT(*) as count FROM chat_messages "
            "WHERE sender_id = $1 "
            "  AND channel_type = $2 "
            "  AND DATE(send_time) = CURRENT_DATE";

        auto row = executor.QueryOneParams(sql, {std::to_string(character_id),
                                          std::to_string(static_cast<uint8_t>(channel_type))});

        return row->Get<size_t>(0, "count").value_or(0);

    } catch (const std::exception& e) {
        spdlog::error("Failed to get today message count: {}", e.what());
        return 0;
    }
}

uint32_t ChatDAO::GetMuteRemainingSeconds(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "SELECT EXTRACT(EPOCH FROM (mute_end_time - CURRENT_TIMESTAMP)) as remaining_seconds "
            "FROM character_mute "
            "WHERE character_id = $1 AND CURRENT_TIMESTAMP < mute_end_time";

        auto row = executor.QueryOneParams(sql, {std::to_string(character_id)});

        return static_cast<uint32_t>(row->Get<double>(0, "remaining_seconds").value_or(0.0));

    } catch (const std::exception& e) {
        spdlog::error("Failed to get mute remaining seconds: {}", e.what());
        return 0;
    }
}

bool ChatDAO::MuteCharacter(uint64_t character_id, uint32_t duration_seconds) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "INSERT INTO character_mute (character_id, mute_end_time) "
            "VALUES ($1, CURRENT_TIMESTAMP + INTERVAL '1 second' * $2) "
            "ON CONFLICT (character_id) DO UPDATE "
            "SET mute_end_time = CURRENT_TIMESTAMP + INTERVAL '1 second' * $3";

        executor.ExecuteParams(sql, {std::to_string(character_id), std::to_string(duration_seconds), std::to_string(duration_seconds)});

        spdlog::info("Character {} muted for {} seconds", character_id, duration_seconds);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to mute character: {}", e.what());
        return false;
    }
}

bool ChatDAO::UnmuteCharacter(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    QueryExecutor executor(conn);

    try {
        // 使用参数化查询防止SQL注入
        std::string sql = "DELETE FROM character_mute "
            "WHERE character_id = $1";

        executor.ExecuteParams(sql, {std::to_string(character_id)});

        spdlog::info("Character {} unmuted", character_id);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to unmute character: {}", e.what());
        return false;
    }
}

std::optional<Murim::Game::Character> ChatDAO::GetCharacterInfo(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT character_id, name, initial_weapon, level "
                         "FROM characters "
                         "WHERE character_id = $1";

        auto row = executor.QueryOneParams(sql, {std::to_string(character_id)});

        ConnectionPool::Instance().ReturnConnection(conn);

        Murim::Game::Character character;
        character.character_id = row->Get<uint64_t>(0, "character_id").value_or(0);
        character.name = row->GetValue(0, "name");
        character.initial_weapon = row->Get<uint8_t>(0, "initial_weapon").value_or(0);
        character.level = row->Get<uint16_t>(0, "level").value_or(1);

        return character;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get character info: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

std::optional<Game::ChatChannel> ChatDAO::GetChannelInfo(uint64_t channel_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT channel_type, name, description, min_level, min_vip_level, "
                         "cooldown_seconds, message_max_length "
                         "FROM chat_channels "
                         "WHERE channel_id = $1";

        auto row = executor.QueryOneParams(sql, {std::to_string(channel_id)});

        ConnectionPool::Instance().ReturnConnection(conn);

        Game::ChatChannel channel;
        channel.type = static_cast<Game::ChatChannelType>(row->Get<uint8_t>(0, "channel_type").value_or(0));
        channel.name = row->GetValue(0, "name");

        // Check if description exists and is not null
        std::string desc = row->GetValue(0, "description");
        if (!desc.empty()) {
            channel.description = desc;
        }

        channel.min_level = row->Get<uint16_t>(0, "min_level").value_or(1);
        channel.min_vip_level = row->Get<uint8_t>(0, "min_vip_level").value_or(0);
        channel.cooldown_seconds = row->Get<uint32_t>(0, "cooldown_seconds").value_or(0);
        channel.message_max_length = row->Get<uint32_t>(0, "message_max_length").value_or(200);

        return channel;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get channel info: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
