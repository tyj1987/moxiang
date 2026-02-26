#include "Chat.hpp"
#include "core/network/NotificationService.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>
#include <regex>
#include "core/database/ChatDAO.hpp"
#include "shared/character/CharacterManager.hpp"

#ifdef SendMessage
#undef SendMessage
#endif

// using Database = Murim::Core::Database;  // Removed - causes parsing errors
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Game {

// ========== ChatManager ==========

ChatManager& ChatManager::Instance() {
    static ChatManager instance;
    return instance;
}

// ========== 初始化 ==========

void ChatManager::Initialize() {
    spdlog::info("Initializing ChatManager...");

    // 初始化频道配置
    ChatChannel world_channel;
    world_channel.type = ChatChannelType::kWorld;
    world_channel.name = "World";
    world_channel.description = "世界频道";
    world_channel.min_level = 10;
    world_channel.min_vip_level = 0;
    world_channel.cooldown_seconds = 10;
    world_channel.message_max_length = 200;
    channel_configs_[ChatChannelType::kWorld] = world_channel;

    ChatChannel shout_channel;
    shout_channel.type = ChatChannelType::kShout;
    shout_channel.name = "Shout";
    shout_channel.description = "呐喊频道";
    shout_channel.min_level = 1;
    shout_channel.min_vip_level = 0;
    shout_channel.cooldown_seconds = 30;
    shout_channel.message_max_length = 100;
    channel_configs_[ChatChannelType::kShout] = shout_channel;

    ChatChannel trade_channel;
    trade_channel.type = ChatChannelType::kTrade;
    trade_channel.name = "Trade";
    trade_channel.description = "交易频道";
    trade_channel.min_level = 1;
    trade_channel.min_vip_level = 0;
    trade_channel.cooldown_seconds = 5;
    trade_channel.message_max_length = 200;
    channel_configs_[ChatChannelType::kTrade] = trade_channel;

    ChatChannel nearby_channel;
    nearby_channel.type = ChatChannelType::kNearby;
    nearby_channel.name = "Nearby";
    nearby_channel.description = "附近频道";
    nearby_channel.min_level = 1;
    nearby_channel.min_vip_level = 0;
    nearby_channel.cooldown_seconds = 2;
    nearby_channel.message_max_length = 200;
    channel_configs_[ChatChannelType::kNearby] = nearby_channel;

    // 暂时禁用敏感词加载，避免数据库连接阻塞
    // LoadBannedWords();

    spdlog::info("ChatManager initialized (banned words loading disabled)");
}

void ChatManager::Process() {
    // 处理聊天逻辑 (暂时为空)
}

void ChatManager::LoadBannedWords() {
    // 从数据库加载敏感词
    try {
        banned_words_ = Murim::Core::Database::ChatDAO::LoadBannedWords();
        spdlog::info("Loaded {} banned words from database", banned_words_.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to load banned words from database: {}", e.what());
        // 使用默认的硬编码列表作为备用
        banned_words_ = {
            "fuck",
            "shit",
            "damn",
            "ass"
        };
        spdlog::info("Using default banned words list ({} words)", banned_words_.size());
    }
}

// ========== 消息发送 ==========

bool ChatManager::SendMessage(
    uint64_t sender_id,
    ChatChannelType channel_type,
    const std::string& content,
    uint64_t receiver_id,
    const std::string& channel_name
) {
    // 检查是否可以发送
    if (!CanSend(sender_id, channel_type, content)) {
        return false;
    }

    // 过滤敏感词
    std::string filtered_content = FilterBannedWords(content);

    // 创建消息
    ChatMessage message;
    message.message_id = GenerateMessageId();
    message.sender_id = sender_id;
    message.receiver_id = receiver_id;
    message.channel_type = channel_type;
    message.channel_name = channel_name;
    message.content = filtered_content;
    message.send_time = std::chrono::system_clock::now();

    // 私聊消息需要设置接收者
    if (channel_type == ChatChannelType::kPrivate) {
        if (receiver_id == 0) {
            spdlog::warn("Private chat requires receiver_id");
            return false;
        }
    }

    // 获取发送者名字 - 对应 legacy 角色信息获取
    try {
        // 从数据库或CharacterManager获取
        auto it = online_characters_.find(sender_id);
        if (it != online_characters_.end()) {
            message.sender_name = it->second.name;
        } else {
            // 从数据库加载
            auto character = Murim::Core::Database::ChatDAO::GetCharacterInfo(sender_id);
            if (character.has_value()) {
                message.sender_name = character->name;
            } else {
                message.sender_name = "Unknown";
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to load sender name: {}", e.what());
        message.sender_name = "Unknown";
    }

    // 保存到历史
    SaveMessageToHistory(message);

    // 广播消息
    BroadcastMessage(message, message.message_id);

    spdlog::info("Chat message sent: sender={}, channel={}, content={}",
                 sender_id, static_cast<int>(channel_type), filtered_content);

    return true;
}

bool ChatManager::SendSystemMessage(
    ChatChannelType channel_type,
    const std::string& message,
    uint64_t target_id
) {
    ChatMessage sys_message;
    sys_message.message_id = GenerateMessageId();
    sys_message.sender_id = 0;  // 系统消息发送者ID为0
    sys_message.sender_name = "System";
    sys_message.channel_type = channel_type;
    sys_message.content = message;
    sys_message.send_time = std::chrono::system_clock::now();

    if (target_id > 0) {
        // 发送给特定玩家 - 对应 legacy 系统消息
        Core::Network::NotificationService::Instance().SendSystemMessage(target_id, message);
        spdlog::info("System message to {}: {}", target_id, message);
    } else {
        // 广播到频道
        BroadcastMessage(sys_message);
    }

    return true;
}

// ========== 频道管理 ==========

uint64_t ChatManager::CreateChannel(
    ChatChannelType type,
    const std::string& name
) {
    ChatChannel channel;
    channel.type = type;
    channel.name = name;

    uint64_t channel_id = GenerateChannelId();

    // 保存到数据库
    try {
        Murim::Core::Database::ChatDAO::CreateChannel(
            channel_id,
            static_cast<uint32_t>(type),
            name,
            "Custom channel"
        );
    } catch (const std::exception& e) {
        spdlog::error("Failed to save channel to database: {}", e.what());
    }

    spdlog::info("Chat channel created: id={}, type={}, name={}",
                 channel_id, static_cast<int>(type), name);

    return channel_id;
}

bool ChatManager::JoinChannel(
    uint64_t character_id,
    uint64_t channel_id
) {
    // 检查频道是否存在
    try {
        auto channel = Murim::Core::Database::ChatDAO::GetChannelInfo(channel_id);
        if (!channel.has_value()) {
            spdlog::warn("Channel {} not found", channel_id);
            return false;
        }

        // TODO: 检查是否有权限加入
        // 暂时不检查权限，所有玩家都可以加入
    } catch (const std::exception& e) {
        spdlog::error("Failed to check channel: {}", e.what());
        return false;
    }

    character_channels_[character_id].push_back(channel_id);
    channel_members_[channel_id].push_back(character_id);

    spdlog::info("Character {} joined channel {}", character_id, channel_id);

    return true;
}

bool ChatManager::LeaveChannel(
    uint64_t character_id,
    uint64_t channel_id
) {
    // 从角色的频道列表中移除
    auto it = character_channels_.find(character_id);
    if (it != character_channels_.end()) {
        it->second.erase(
            std::remove(it->second.begin(), it->second.end(), channel_id)
        );
    }

    // 从频道成员列表中移除
    auto members_it = channel_members_.find(channel_id);
    if (members_it != channel_members_.end()) {
        members_it->second.erase(
            std::remove(members_it->second.begin(),
                     members_it->second.end(), character_id)
        );
    }

    spdlog::info("Character {} left channel {}", character_id, channel_id);

    return true;
}

// ========== 消息历史 ==========

std::vector<ChatMessage> ChatManager::GetChatHistory(
    ChatChannelType channel_type,
    const std::string& channel_name,
    uint32_t count,
    uint64_t before_message_id
) {
    std::vector<ChatMessage> messages;

    auto& message_queue = channel_messages_[channel_type][channel_name];

    size_t start_index = 0;
    if (before_message_id > 0) {
        // 找到起始消息
        auto it = std::find_if(message_queue.begin(), message_queue.end(),
            [before_message_id](const ChatMessage& msg) {
                return msg.message_id == before_message_id;
            });

        if (it != message_queue.end()) {
            start_index = std::distance(message_queue.begin(), it) + 1;
        }
    }

    // 获取最近count条消息
    size_t end_index = std::min(start_index + count, message_queue.size());

    for (size_t i = start_index; i < end_index; ++i) {
        messages.push_back(message_queue[i]);
    }

    spdlog::debug("Retrieved {} messages from channel {}: {}",
                 messages.size(), static_cast<int>(channel_type), channel_name);

    return messages;
}

std::vector<ChatMessage> ChatManager::GetPrivateHistory(
    uint64_t character_id,
    uint64_t target_id,
    uint32_t count
) {
    // 从数据库加载私聊历史
    try {
        auto db_messages = Murim::Core::Database::ChatDAO::GetPrivateHistory(
            character_id,
            target_id,
            count
        );

        std::vector<ChatMessage> messages;
        for (const auto& db_msg : db_messages) {
            ChatMessage msg;
            msg.message_id = db_msg.message_id;
            msg.sender_id = db_msg.sender_id;
            msg.receiver_id = db_msg.receiver_id;
            msg.channel_type = ChatChannelType::kPrivate;
            msg.content = db_msg.content;
            msg.send_time = db_msg.send_time;
            messages.push_back(msg);
        }

        spdlog::debug("Loaded {} private messages from database", messages.size());
        return messages;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load private chat history: {}", e.what());
        return {};
    }
}

// ========== 聊天频道查询 ==========

std::vector<uint64_t> ChatManager::GetCharacterChannels(uint64_t character_id) {
    auto it = character_channels_.find(character_id);
    if (it != character_channels_.end()) {
        return it->second;
    }
    return {};
}

std::vector<uint64_t> ChatManager::GetChannelMembers(uint64_t channel_id) {
    auto it = channel_members_.find(channel_id);
    if (it != channel_members_.end()) {
        return it->second;
    }
    return {};
}

// ========== 冷却管理 ==========

uint16_t ChatManager::GetChatCooldown(uint64_t character_id, ChatChannelType channel_type) {
    auto it = chat_cooldowns_.find(character_id);
    if (it == chat_cooldowns_.end()) {
        return 0;
    }

    auto cooldown_it = it->second.find(channel_type);
    if (cooldown_it == it->second.end()) {
        return 0;
    }

    auto now = std::chrono::system_clock::now();
    auto last_send = cooldown_it->second;

    auto channel_config_it = channel_configs_.find(channel_type);
    if (channel_config_it != channel_configs_.end()) {
        return 0;
    }

    uint32_t cooldown_seconds = channel_config_it->second.cooldown_seconds;

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_send
    ).count();

    if (elapsed >= cooldown_seconds) {
        // 冷却完成
        it->second.erase(cooldown_it);
        return 0;
    }

    return static_cast<uint16_t>(cooldown_seconds - elapsed);
}

void ChatManager::ResetChatCooldown(uint64_t character_id, ChatChannelType channel_type) {
    auto it = chat_cooldowns_.find(character_id);
    if (it != chat_cooldowns_.end()) {
        it->second.erase(channel_type);
    }

    spdlog::debug("Chat cooldown reset: character={}, channel={}",
                 character_id, static_cast<int>(channel_type));
}

// ========== 辅助方法 ==========

uint64_t ChatManager::GenerateMessageId() {
    return next_message_id_++;
}

uint64_t ChatManager::GenerateChannelId() {
    return next_channel_id_++;
}

std::string ChatManager::FilterBannedWords(const std::string& content) {
    std::string filtered = content;

    // 简单的敏感词替换
    for (const auto& word : banned_words_) {
        std::regex pattern(word, std::regex_constants::icase);
        filtered = std::regex_replace(filtered, pattern, "***");
    }

    return filtered;
}

bool ChatManager::CanSend(
    uint64_t character_id,
    ChatChannelType channel_type,
    const std::string& content
) {
    // 检查内容长度
    auto config_it = channel_configs_.find(channel_type);
    if (config_it != channel_configs_.end()) {
        if (content.length() > config_it->second.message_max_length) {
            spdlog::warn("Message too long for channel {}: {} > {}",
                         static_cast<int>(channel_type), content.length(),
                         config_it->second.message_max_length);
            return false;
        }
    }

    // 检查冷却
    uint16_t cooldown = GetChatCooldown(character_id, channel_type);
    if (cooldown > 0) {
        spdlog::warn("Chat cooldown: character={}, channel={}, remaining={}s",
                     character_id, static_cast<int>(channel_type), cooldown);
        return false;
    }

    // 检查等级限制 - 对应 legacy 等级检查
    // config_it 已经在上面定义了，这里复用
    if (config_it != channel_configs_.end()) {
        try {
            // 从数据库获取角色等级
            auto character = Murim::Core::Database::ChatDAO::GetCharacterInfo(character_id);
            if (character.has_value()) {
                uint16_t character_level = character->level;
                if (character_level < config_it->second.min_level) {
                    spdlog::warn("Character level too low for channel {}: {} < {}",
                                  static_cast<int>(channel_type), character_level, config_it->second.min_level);
                    return false;
                }
            }

            // 检查VIP等级限制 - 对应 legacy VIP检查
            if (config_it->second.min_vip_level > 0) {
                uint16_t vip_level = GetCharacterVipLevel(character_id);
                if (vip_level < config_it->second.min_vip_level) {
                    spdlog::warn("VIP level too low for channel {}: {} < {}",
                                  static_cast<int>(channel_type), vip_level, config_it->second.min_vip_level);
                    return false;
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to check character level: {}", e.what());
        }
    }

    return true;
}

void ChatManager::BroadcastMessage(
    const ChatMessage& message,
    uint64_t exclude_id
) {
    ChatChannelType channel_type = message.channel_type;

    // 根据频道类型广播
    if (channel_type == ChatChannelType::kPrivate) {
        //         // 私聊: 发送给发送者和接收者 - 对应 legacy 私聊消息
        if (message.receiver_id > 0) {
            Core::Network::NotificationService::Instance().SendChatMessage(
                message.receiver_id, static_cast<uint8_t>(channel_type),
                message.sender_id, message.sender_name, message.content);
        }
    } else if (channel_type == ChatChannelType::kNearby) {
        // 附近频道: 发送给范围内玩家
        // 获取发送者位置
        auto sender_it = online_characters_.find(message.sender_id);
        if (sender_it != online_characters_.end()) {
            Position sender_pos(sender_it->second.x, sender_it->second.y, sender_it->second.z);
            uint16_t map_id = sender_it->second.map_id;

            // 查询附近玩家 (简单距离查询, 使用50米范围)
            // 未来可以升级到 SpatialIndex 实现更高效的空间查询
            std::vector<uint64_t> nearby_players = GetNearbyPlayers(sender_pos, 50.0f, map_id);

            // 发送给附近玩家
            for (uint64_t player_id : nearby_players) {
                if (player_id != message.sender_id) {  // 不发送给自己
                    Core::Network::NotificationService::Instance().SendChatMessage(
                        player_id, static_cast<uint8_t>(channel_type),
                        message.sender_id, message.sender_name, message.content);
                }
            }

            spdlog::debug("Broadcast to nearby: {} players in range", nearby_players.size());
        } else {
            spdlog::warn("Sender {} not found in online cache for nearby chat", message.sender_id);
        }
    } else {
        // 其他频道: 发送给所有订阅该频道的玩�?        spdlog::debug("Broadcast to channel {}: {}", static_cast<int>(channel_type), message.content);
    }

    // 记录冷却
    chat_cooldowns_[message.sender_id][channel_type] = std::chrono::system_clock::now();
}

void ChatManager::SaveMessageToHistory(const ChatMessage& message) {
    // 保存到数据库
    try {
        Murim::Core::Database::ChatDAO::SaveMessage(message);
    } catch (const std::exception& e) {
        spdlog::error("Failed to save message to database: {}", e.what());
    }

    // 同时保存到内存缓存
    auto& message_queue = channel_messages_[message.channel_type][message.channel_name];
    message_queue.push_back(message);

    // 限制历史记录数量
    const size_t max_history = 1000;
    if (message_queue.size() > max_history) {
        message_queue.erase(message_queue.begin(),
                      message_queue.begin() + (message_queue.size() - max_history));
    }
}

// ========== 辅助方法实现 ==========

std::vector<uint64_t> ChatManager::GetNearbyPlayers(
    const Position& center_pos,
    float range,
    uint16_t map_id
) {
    std::vector<uint64_t> nearby_players;

    // 简单的线性搜索 - O(n) 复杂度
    // 未来可以使用 SpatialIndex (Quadtree/Grid) 优化到 O(log n)
    for (const auto& [character_id, online_char] : online_characters_) {
        // 只查询同一地图的玩家
        if (online_char.map_id != map_id) {
            continue;
        }

        // 检查是否在范围内
        Position player_pos(online_char.x, online_char.y, online_char.z);
        if (center_pos.IsInRange(player_pos, range)) {
            nearby_players.push_back(character_id);
        }
    }

    return nearby_players;
}

uint16_t ChatManager::GetCharacterVipLevel(uint64_t character_id) {
    // 先从在线缓存中查找
    auto it = online_characters_.find(character_id);
    if (it != online_characters_.end()) {
        return it->second.vip_level;
    }

    // 从数据库或CharacterManager获取
    try {
        auto character = CharacterManager::Instance().GetCharacter(character_id);
        if (character.has_value()) {
            // 从角色数据中获取VIP等级
            // 假设Character结构中有vip_level字段,如果没有则从账号信息获取
            // 这里暂时返回0,实际实现需要根据数据库结构
            return 0;  // TODO: 从账号表或角色扩展表获取VIP等级
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to get VIP level for character {}: {}", character_id, e.what());
    }

    return 0;  // 默认普通玩家
}

} // namespace Game
} // namespace Murim
