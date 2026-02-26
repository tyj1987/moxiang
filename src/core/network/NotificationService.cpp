#include "NotificationService.hpp"
#include "../spdlog_wrapper.hpp"
#include <cstring>

// Windows headers may define macros that conflict with our code
// We avoid including windows.h here since we're not using Windows-specific APIs
// If needed, use specific headers like <winsock2.h> instead

namespace Murim {
namespace Core {
namespace Network {

NotificationService::NotificationService()
    : session_manager_(nullptr) {
}

NotificationService& NotificationService::Instance() {
    static NotificationService instance;
    return instance;
}

void NotificationService::SetMessageSender(MessageSender sender) {
    message_sender_ = std::move(sender);
}

bool NotificationService::SendToSession(uint64_t session_id, MessageType message_type, const void* data, size_t data_size) {
    if (!message_sender_) {
        spdlog::warn("NotificationService: Message sender not set");
        return false;
    }

    try {
        message_sender_(session_id, message_type, data, data_size);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("NotificationService: Failed to send message to session {}: {}", session_id, e.what());
        return false;
    }
}

bool NotificationService::SendToCharacter(uint64_t character_id, MessageType message_type, const void* data, size_t data_size) {
    // TODO: 通过 character_id 查找对应的 session_id
    // 这里需要从 CharacterManager 或 SessionManager 获取映射关系
    spdlog::trace("NotificationService: Send to character {} (type: {})", character_id, static_cast<uint32_t>(message_type));
    return true;  // 暂时返回true
}

void NotificationService::Broadcast(MessageType message_type, const void* data, size_t data_size) {
    // TODO: 实现广播功能
    // auto sessions = session_manager_->GetAllSessions();
    // for (const auto& session : sessions) {
    //     SendToSession(session->GetSessionId(), message_type, data, data_size);
    // }
}

void NotificationService::SendToMap(uint16_t map_id, MessageType message_type, const void* data, size_t data_size) {
    // TODO: 查找指定地图的所有玩家会话
    spdlog::trace("NotificationService: Broadcast to map {} (type: {})", map_id, static_cast<uint32_t>(message_type));
}

// ========== 角色相关通知 ==========

void NotificationService::NotifyLevelUp(uint64_t character_id, uint16_t new_level) {
    struct LevelUpData {
        uint16_t level;
    } data{ new_level };

    SendToCharacter(character_id, MessageType::kLevelUp, &data, sizeof(data));
    spdlog::info("Character {} leveled up to {}", character_id, new_level);
}

void NotificationService::NotifyExpUpdate(uint64_t character_id, uint64_t exp, uint64_t exp_for_level) {
    struct ExpUpdateData {
        uint64_t current_exp;
        uint64_t required_exp;
    } data{ exp, exp_for_level };

    SendToCharacter(character_id, MessageType::kExpUpdate, &data, sizeof(data));
}

void NotificationService::NotifyMoneyUpdate(uint64_t character_id, uint64_t money) {
    struct MoneyUpdateData {
        uint64_t money;
    } data{ money };

    SendToCharacter(character_id, MessageType::kMoneyUpdate, &data, sizeof(data));
    spdlog::debug("Character {} money updated to {}", character_id, money);
}

void NotificationService::NotifyTeleport(uint64_t character_id, uint16_t map_id, float x, float y, float z) {
    struct TeleportData {
        uint16_t map_id;
        float x;
        float y;
        float z;
    } data{ map_id, x, y, z };

    SendToCharacter(character_id, MessageType::kTeleport, &data, sizeof(data));
    spdlog::info("Character {} teleported to map ({}, {}, {}, {})", character_id, map_id, x, y, z);
}

void NotificationService::NotifyHpMpUpdate(uint64_t character_id, uint32_t hp, uint32_t max_hp, uint32_t mp, uint32_t max_mp) {
    struct HpMpUpdateData {
        uint32_t hp;
        uint32_t max_hp;
        uint32_t mp;
        uint32_t max_mp;
    } data{ hp, max_hp, mp, max_mp };

    SendToCharacter(character_id, MessageType::kHpMpUpdate, &data, sizeof(data));
}

// ========== 技能相关通知 ==========

void NotificationService::NotifySkillLearned(uint64_t character_id, uint32_t skill_id, uint16_t skill_level) {
    struct SkillLearnedData {
        uint32_t skill_id;
        uint16_t skill_level;
    } data{ skill_id, skill_level };

    SendToCharacter(character_id, MessageType::kSkillLearned, &data, sizeof(data));
    spdlog::info("Character {} learned skill {} level {}", character_id, skill_id, skill_level);
}

// ========== 物品相关通知 ==========

void NotificationService::NotifyItemObtained(uint64_t character_id, uint32_t item_id, uint16_t quantity) {
    struct ItemObtainedData {
        uint32_t item_id;
        uint16_t quantity;
    } data{ item_id, quantity };

    SendToCharacter(character_id, MessageType::kItemObtained, &data, sizeof(data));
    spdlog::info("Character {} obtained item {} x{}", character_id, item_id, quantity);
}

// ========== 任务相关通知 ==========

void NotificationService::NotifyQuestCompleted(uint64_t character_id, uint32_t quest_id) {
    struct QuestCompletedData {
        uint32_t quest_id;
    } data{ quest_id };

    SendToCharacter(character_id, MessageType::kQuestCompleted, &data, sizeof(data));
    spdlog::info("Character {} completed quest {}", character_id, quest_id);
}

void NotificationService::NotifyQuestProgress(uint64_t character_id, uint32_t quest_id, uint32_t objective_id, uint32_t progress, uint32_t required) {
    struct QuestProgressData {
        uint32_t quest_id;
        uint32_t objective_id;
        uint32_t progress;
        uint32_t required;
    } data{ quest_id, objective_id, progress, required };

    SendToCharacter(character_id, MessageType::kQuestProgress, &data, sizeof(data));
}

// ========== 社交相关通知 ==========

void NotificationService::NotifyFriendOnline(uint64_t character_id, uint64_t friend_id, const std::string& friend_name) {
    struct FriendOnlineData {
        uint64_t friend_id;
        char name[32];  // 固定长度名称
    } data{ friend_id, {} };

    strncpy(data.name, friend_name.c_str(), sizeof(data.name) - 1);
    data.name[sizeof(data.name) - 1] = '\0';
    SendToCharacter(character_id, MessageType::kFriendOnline, &data, sizeof(data));
    spdlog::info("Notify character {}: friend {} ({}) is online", character_id, friend_id, friend_name);
}

void NotificationService::NotifyFriendOffline(uint64_t character_id, uint64_t friend_id) {
    struct FriendOfflineData {
        uint64_t friend_id;
    } data{ friend_id };

    SendToCharacter(character_id, MessageType::kFriendOffline, &data, sizeof(data));
    spdlog::info("Notify character {}: friend {} is offline", character_id, friend_id);
}

void NotificationService::NotifyFriendRequest(uint64_t character_id, uint64_t requester_id, const std::string& requester_name) {
    struct FriendRequestData {
        uint64_t requester_id;
        char name[32];
    } data{ requester_id, {} };

    strncpy(data.name, requester_name.c_str(), sizeof(data.name) - 1);
    data.name[sizeof(data.name) - 1] = '\0';
    SendToCharacter(character_id, MessageType::kFriendRequest, &data, sizeof(data));
    spdlog::info("Character {} received friend request from {} ({})", character_id, requester_id, requester_name);
}

// ========== 队伍相关通知 ==========

void NotificationService::NotifyPartyInvite(uint64_t character_id, uint64_t inviter_id, const std::string& inviter_name, uint64_t party_id) {
    struct PartyInviteData {
        uint64_t inviter_id;
        uint64_t party_id;
        char name[32];
    } data{ inviter_id, party_id, {} };

    strncpy(data.name, inviter_name.c_str(), sizeof(data.name) - 1);
    data.name[sizeof(data.name) - 1] = '\0';
    SendToCharacter(character_id, MessageType::kPartyInvite, &data, sizeof(data));
    spdlog::info("Character {} received party invite from {} ({}) to party {}", character_id, inviter_id, inviter_name, party_id);
}

void NotificationService::NotifyPartyJoined(uint64_t character_id, uint64_t party_id) {
    struct PartyJoinedData {
        uint64_t party_id;
    } data{ party_id };

    SendToCharacter(character_id, MessageType::kPartyJoined, &data, sizeof(data));
    spdlog::info("Character {} joined party {}", character_id, party_id);
}

void NotificationService::NotifyPartyLeft(uint64_t character_id) {
    SendToCharacter(character_id, MessageType::kPartyLeft, nullptr, 0);
    spdlog::info("Character {} left party", character_id);
}

// ========== 聊天相关通知 ==========

void NotificationService::SendChatMessage(uint64_t character_id, uint8_t channel, uint64_t sender_id, const std::string& sender_name, const std::string& message_param)
{
    struct ChatMessageData {
        uint8_t channel;
        uint64_t sender_id;
        char sender_name[32];
        char msg_content[256];
    } chat_data;

    chat_data.channel = channel;
    chat_data.sender_id = sender_id;
    memset(chat_data.sender_name, 0, sizeof(chat_data.sender_name));
    memset(chat_data.msg_content, 0, sizeof(chat_data.msg_content));

    strncpy(chat_data.sender_name, sender_name.c_str(), sizeof(chat_data.sender_name) - 1);
    chat_data.sender_name[sizeof(chat_data.sender_name) - 1] = '\0';
    strncpy(chat_data.msg_content, message_param.c_str(), sizeof(chat_data.msg_content) - 1);
    chat_data.msg_content[sizeof(chat_data.msg_content) - 1] = '\0';

    SendToCharacter(character_id, MessageType::kChatMessage, &chat_data, sizeof(chat_data));
}

// ========== 系统相关通知 ==========

void NotificationService::SendSystemMessage(uint64_t character_id, const std::string& message_param)
{
    struct SystemMessageData {
        char msg_content[512];
    } sys_msg_data;

    memset(sys_msg_data.msg_content, 0, sizeof(sys_msg_data.msg_content));
    strncpy(sys_msg_data.msg_content, message_param.c_str(), sizeof(sys_msg_data.msg_content) - 1);
    sys_msg_data.msg_content[sizeof(sys_msg_data.msg_content) - 1] = '\0';

    if (character_id == 0) {
        Broadcast(MessageType::kSystemMessage, &sys_msg_data, sizeof(sys_msg_data));
    } else {
        SendToCharacter(character_id, MessageType::kSystemMessage, &sys_msg_data, sizeof(sys_msg_data));
    }
    spdlog::info("System message to {}: {}", character_id, message_param);
}

void NotificationService::SendErrorMessage(uint64_t character_id, uint32_t error_code, const std::string& message_param)
{
    struct ErrorMessageData {
        uint32_t error_code;
        char msg_content[256];
    } err_msg_data;

    memset(&err_msg_data, 0, sizeof(err_msg_data));
    err_msg_data.error_code = error_code;
    strncpy(err_msg_data.msg_content, message_param.c_str(), sizeof(err_msg_data.msg_content) - 1);
    err_msg_data.msg_content[sizeof(err_msg_data.msg_content) - 1] = '\0';
    SendToCharacter(character_id, MessageType::kErrorMessage, &err_msg_data, sizeof(err_msg_data));
    spdlog::warn("Error message to {} (code {}): {}", character_id, error_code, message_param);
}

} // namespace Network
} // namespace Core
} // namespace Murim
