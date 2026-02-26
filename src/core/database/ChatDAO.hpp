#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include "shared/social/Chat.hpp"
#include "shared/character/Character.hpp"  // For Character type

namespace Murim {
namespace Core {
namespace Database {

/**
 * @brief Chat DAO (Data Access Object)
 *
 * 负责聊天系统的数据库操作
 */
class ChatDAO {
public:
    // ========== 聊天消息 ==========

    /**
     * @brief 保存聊天消息
     * @param message 聊天消息
     * @return 是否成功
     */
    static bool SaveMessage(const Game::ChatMessage& message);

    /**
     * @brief 获取聊天历史
     * @param channel_type 频道类型
     * @param channel_name 频道名称
     * @param count 消息数量
     * @param before_message_id 起始消息ID（0表示从最新开始）
     * @return 消息列表
     */
    static std::vector<Game::ChatMessage> GetChatHistory(
        Game::ChatChannelType channel_type,
        const std::string& channel_name,
        uint32_t count,
        uint64_t before_message_id
    );

    /**
     * @brief 获取私聊历史
     * @param character_id 角色ID
     * @param target_id 目标角色ID
     * @param count 消息数量
     * @return 消息列表
     */
    static std::vector<Game::ChatMessage> GetPrivateHistory(
        uint64_t character_id,
        uint64_t target_id,
        uint32_t count
    );

    /**
     * @brief 删除旧聊天消息
     * @param days 保留天数
     * @return 删除数量
     */
    static size_t DeleteOldMessages(uint32_t days);

    // ========== 聊天频道 ==========

    /**
     * @brief 创建聊天频道
     * @param type 频道类型
     * @param name 频道名称
     * @param description 描述
     * @param min_level 最低等级
     * @param min_vip_level 最低VIP等级
     * @param cooldown_seconds 冷却时间
     * @param message_max_length 消息最大长度
     * @return 频道ID，失败返回0
     */
    static uint64_t CreateChannel(
        Game::ChatChannelType type,
        const std::string& name,
        const std::string& description,
        uint16_t min_level,
        uint8_t min_vip_level,
        uint32_t cooldown_seconds,
        uint32_t message_max_length
    );

    /**
     * @brief 创建聊天频道 (简化版)
     * @param channel_id 频道ID
     * @param type 频道类型
     * @param name 频道名称
     * @param description 描述
     * @return 是否成功
     */
    static bool CreateChannel(
        uint64_t channel_id,
        uint32_t type,
        const std::string& name,
        const std::string& description
    );

    /**
     * @brief 获取角色信息 (用于聊天)
     * @param character_id 角色ID
     * @return 角色信息
     */
    static std::optional<Murim::Game::Character> GetCharacterInfo(uint64_t character_id);

    /**
     * @brief 获取频道信息
     * @param channel_id 频道ID
     * @return 频道信息
     */
    static std::optional<Game::ChatChannel> GetChannelInfo(uint64_t channel_id);

    /**
     * @brief 获取频道配置
     * @param type 频道类型
     * @return 频道配置
     */
    static std::optional<Game::ChatChannel> GetChannelConfig(Game::ChatChannelType type);

    /**
     * @brief 更新频道配置
     * @param channel 频道配置
     * @return 是否成功
     */
    static bool UpdateChannelConfig(const Game::ChatChannel& channel);

    /**
     * @brief 加入频道
     * @param character_id 角色ID
     * @param channel_id 频道ID
     * @return 是否成功
     */
    static bool JoinChannel(uint64_t character_id, uint64_t channel_id);

    /**
     * @brief 离开频道
     * @param character_id 角色ID
     * @param channel_id 频道ID
     * @return 是否成功
     */
    static bool LeaveChannel(uint64_t character_id, uint64_t channel_id);

    /**
     * @brief 获取角色所在频道
     * @param character_id 角色ID
     * @return 频道ID列表
     */
    static std::vector<uint64_t> GetCharacterChannels(uint64_t character_id);

    /**
     * @brief 获取频道成员
     * @param channel_id 频道ID
     * @return 角色ID列表
     */
    static std::vector<uint64_t> GetChannelMembers(uint64_t channel_id);

    // ========== 敏感词过滤 ==========

    /**
     * @brief 加载敏感词列表
     * @return 敏感词列表
     */
    static std::vector<std::string> LoadBannedWords();

    /**
     * @brief 添加敏感词
     * @param word 敏感词
     * @return 是否成功
     */
    static bool AddBannedWord(const std::string& word);

    /**
     * @brief 删除敏感词
     * @param word 敏感词
     * @return 是否成功
     */
    static bool RemoveBannedWord(const std::string& word);

    // ========== 聊天统计 ==========

    /**
     * @brief 获取角色今日发言次数
     * @param character_id 角色ID
     * @param channel_type 频道类型
     * @return 发言次数
     */
    static size_t GetTodayMessageCount(uint64_t character_id, Game::ChatChannelType channel_type);

    /**
     * @brief 检查角色是否被禁言
     * @param character_id 角色ID
     * @return 禁言剩余秒数，0表示未被禁言
     */
    static uint32_t GetMuteRemainingSeconds(uint64_t character_id);

    /**
     * @brief 禁言角色
     * @param character_id 角色ID
     * @param duration_seconds 禁言时长（秒）
     * @return 是否成功
     */
    static bool MuteCharacter(uint64_t character_id, uint32_t duration_seconds);

    /**
     * @brief 解除禁言
     * @param character_id 角色ID
     * @return 是否成功
     */
    static bool UnmuteCharacter(uint64_t character_id);

private:
    ChatDAO() = default;
};

} // namespace Database
} // namespace Core
} // namespace Murim
