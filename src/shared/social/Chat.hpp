#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <deque>

namespace Murim {
namespace Game {

// Forward declaration
struct Position;

/**
 * @brief 聊天频道类型
 */
enum class ChatChannelType : uint8_t {
    kWorld = 1,           // 世界频道
    kGuild = 2,           // 公会频道
    kParty = 3,           // 队伍频道
    kPrivate = 4,         // 私聊
    kNearby = 5,          // 附近频道
    kSystem = 6,          // 系统频道
    kShout = 7,           // 喊喊频道
    kTrade = 8            // 交易频道
};

/**
 * @brief 聊天消息
 */
struct ChatMessage {
    uint64_t message_id;            // 消息ID
    uint64_t sender_id;             // 发送者ID
    std::string sender_name;        // 发送者名称
    uint64_t receiver_id;           // 接收者ID (私聊使用)
    std::string receiver_name;      // 接收者名称 (私聊使用)
    ChatChannelType channel_type;  // 频道类型
    std::string channel_name;        // 频道名称 (公会名/队伍名)
    std::string content;             // 消息内容
    std::chrono::system_clock::time_point send_time;  // 发送时间

    // 附加信息
    uint32_t item_id;               // 关联物品ID (交易频道)
    uint32_t item_count;            // 物品数量

    /**
     * @brief 是否是私聊
     */
    bool IsPrivate() const {
        return channel_type == ChatChannelType::kPrivate;
    }

    /**
     * @brief 是否需要过滤
     */
    bool NeedsFilter() const {
        return channel_type != ChatChannelType::kPrivate &&
               channel_type != ChatChannelType::kSystem;
    }
};

/**
 * @brief 聊天频道数据
 */
struct ChatChannel {
    ChatChannelType type;           // 频道类型
    std::string name;                // 频道名称
    std::string description;         // 描述
    uint16_t min_level;              // 最低等级限制
    uint16_t min_vip_level;          // 最低VIP等级
    uint32_t cooldown_seconds;       // 发言冷却时间
    uint32_t message_max_length;     // 消息最大长度

    /**
     * @brief 是否可以发言
     */
    bool CanSpeak(uint16_t player_level, uint16_t vip_level) const {
        return player_level >= min_level && vip_level >= min_vip_level;
    }
};

/**
 * @brief 在线角色信息 (用于快速访问)
 */
struct OnlineCharacter {
    uint64_t character_id;
    std::string name;
    uint16_t level;
    uint16_t vip_level;
    uint16_t map_id;
    float x, y, z;
    uint8_t status;  // 0=offline, 1=online, 2=busy, 3=away
    std::chrono::system_clock::time_point login_time;

    OnlineCharacter() : character_id(0), level(1), vip_level(0),
                       map_id(0), x(0), y(0), z(0), status(0) {}
};

/**
 * @brief 聊天管理器
 *
 * 负责聊天消息的发送、接收、过滤
 */
class ChatManager {
public:
    /**
     * @brief 获取单例实例
     */
    static ChatManager& Instance();

    // ========== 初始化 ==========

    /**
     * @brief 初始化聊天管理器
     */
    void Initialize();

    /**
     * @brief 处理聊天逻辑
     */
    void Process();

    /**
     * @brief 加载敏感词列表
     */
    void LoadBannedWords();

    // ========== 消息发送 ==========

    /**
     * @brief 发送聊天消息
     * @param sender_id 发送者ID
     * @param channel_type 频道类型
     * @param content 消息内容
     * @param receiver_id 接收者ID (私聊使用,0=无)
     * @param channel_name 频道名称 (公会/队伍名)
     * @return 是否成功
     */
    bool SendMessage(
        uint64_t sender_id,
        ChatChannelType channel_type,
        const std::string& content,
        uint64_t receiver_id = 0,
        const std::string& channel_name = ""
    );

    /**
     * @brief 发送系统消息
     * @param channel_type 频道类型
     * @param message 消息内容
     * @param target_id 目标ID (0=全部)
     */
    bool SendSystemMessage(
        ChatChannelType channel_type,
        const std::string& message,
        uint64_t target_id = 0
    );

    // ========== 频道管理 ==========

    /**
     * @brief 创建频道
     * @param type 频道类型
     * @param name 频道名称
     * @return 频道ID
     */
    uint64_t CreateChannel(
        ChatChannelType type,
        const std::string& name
    );

    /**
     * @brief 加入频道
     * @param character_id 角色ID
     * @param channel_id 频道ID
     * @return 是否成功
     */
    bool JoinChannel(
        uint64_t character_id,
        uint64_t channel_id
    );

    /**
     * @brief 离开频道
     * @param character_id 角色ID
     * @param channel_id 频道ID
     * @return 是否成功
     */
    bool LeaveChannel(
        uint64_t character_id,
        uint64_t channel_id
    );

    // ========== 消息历史 ==========

    /**
     * @brief 获取聊天历史
     * @param channel_type 频道类型
     * @param channel_name 频道名称
     * @param count 数量
     * @param before_message_id 起始消息ID (0=最新)
     * @return 消息列表
     */
    std::vector<ChatMessage> GetChatHistory(
        ChatChannelType channel_type,
        const std::string& channel_name,
        uint32_t count = 50,
        uint64_t before_message_id = 0
    );

    /**
     * @brief 获取私聊历史
     * @param character_id 角色ID
     * @param target_id 对方ID
     * @param count 数量
     * @return 消息列表
     */
    std::vector<ChatMessage> GetPrivateHistory(
        uint64_t character_id,
        uint64_t target_id,
        uint32_t count = 50
    );

    // ========== 聊天频道查询 ==========

    /**
     * @brief 获取角色所在频道
     * @param character_id 角色ID
     * @return 频道ID列表
     */
    std::vector<uint64_t> GetCharacterChannels(uint64_t character_id);

    /**
     * @brief 获取频道成员
     * @param channel_id 频道ID
     * @return 成员ID列表
     */
    std::vector<uint64_t> GetChannelMembers(uint64_t channel_id);

    // ========== 冷却管理 ==========

    /**
     * @brief 检查发言冷却
     * @param character_id 角色ID
     * @param channel_type 频道类型
     * @return 剩余冷却时间(秒), 0表示可发言
     */
    uint16_t GetChatCooldown(uint64_t character_id, ChatChannelType channel_type);

    /**
     * @brief 重置冷却
     * @param character_id 角色ID
     * @param channel_type 频道类型
     */
    void ResetChatCooldown(uint64_t character_id, ChatChannelType channel_type);

private:
    ChatManager() = default;
    ~ChatManager() = default;
    ChatManager(const ChatManager&) = delete;
    ChatManager& operator=(const ChatManager&) = delete;

    // 消息存储: channel_type -> (name -> messages)
    std::unordered_map<ChatChannelType,
        std::unordered_map<std::string, std::deque<ChatMessage>>> channel_messages_;

    // 敏感词列表
    std::vector<std::string> banned_words_;

    // 频道配置
    std::unordered_map<ChatChannelType, ChatChannel> channel_configs_;

    // 角色到频道的映射: character_id -> channel_id
    std::unordered_map<uint64_t, std::vector<uint64_t>> character_channels_;

    // 频道成员: channel_id -> members
    std::unordered_map<uint64_t, std::vector<uint64_t>> channel_members_;

    // 发言冷却: character_id -> (channel_type -> last_send_time)
    std::unordered_map<uint64_t,
        std::unordered_map<ChatChannelType,
            std::chrono::system_clock::time_point>> chat_cooldowns_;

    // 在线角色缓存: character_id -> OnlineCharacter
    std::unordered_map<uint64_t, OnlineCharacter> online_characters_;

    // 全局消息队列 (用于附近频道等)
    std::deque<ChatMessage> message_queue_;

    // 消息ID生成器
    uint64_t next_message_id_ = 1;

    // 频道ID生成器
    uint64_t next_channel_id_ = 1;

    /**
     * @brief 生成消息ID
     */
    uint64_t GenerateMessageId();

    /**
     * @brief 生成频道ID
     */
    uint64_t GenerateChannelId();

    /**
     * @brief 过滤敏感词
     */
    std::string FilterBannedWords(const std::string& content);

    /**
     * @brief 检查是否可以发送
     */
    bool CanSend(
        uint64_t character_id,
        ChatChannelType channel_type,
        const std::string& content
    );

    /**
     * @brief 广播消息到频道
     */
    void BroadcastMessage(
        const ChatMessage& message,
        uint64_t exclude_id = 0
    );

    /**
     * @brief 保存消息到历史
     */
    void SaveMessageToHistory(const ChatMessage& message);

    /**
     * @brief 获取附近玩家 (用于附近频道)
     * @param center_pos 中心位置
     * @param range 范围
     * @param map_id 地图ID
     * @return 附近角色ID列表
     */
    std::vector<uint64_t> GetNearbyPlayers(
        const Position& center_pos,
        float range,
        uint16_t map_id
    );

    /**
     * @brief 检查角色VIP等级
     * @param character_id 角色ID
     * @return VIP等级 (0=普通)
     */
    uint16_t GetCharacterVipLevel(uint64_t character_id);
};

} // namespace Game
} // namespace Murim
