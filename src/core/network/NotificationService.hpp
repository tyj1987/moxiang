#pragma once

#include <memory>
#include <functional>
#include <unordered_map>
#include "SessionManager.hpp"

namespace Murim {
namespace Core {
namespace Network {

/**
 * @brief 网络通知服务
 *
 * 负责向客户端发送各类游戏通知
 * 对应 legacy: 客户端消息发送系统
 */
class NotificationService {
public:
    /**
     * @brief 消息类型枚举
     */
    enum class MessageType : uint32_t {
        // 角色相关
        kLevelUp = 1001,           // 升级通知
        kExpUpdate = 1002,         // 经验更新
        kMoneyUpdate = 1003,       // 金钱更新
        kTeleport = 1004,          // 传送通知
        kAttributeUpdate = 1005,   // 属性更新
        kHpMpUpdate = 1006,        // HP/MP更新

        // 技能相关
        kSkillLearned = 2001,      // 技能学习成功
        kSkillUpdate = 2002,       // 技能更新

        // 物品相关
        kItemObtained = 3001,      // 获得物品
        kItemRemoved = 3002,       // 失去物品
        kItemUsed = 3003,          // 使用物品

        // 任务相关
        kQuestAccepted = 4001,     // 接受任务
        kQuestCompleted = 4002,    // 完成任务
        kQuestFailed = 4003,       // 任务失败
        kQuestProgress = 4004,     // 任务进度更新

        // 社交相关
        kFriendOnline = 5001,      // 好友上线
        kFriendOffline = 5002,     // 好友下线
        kFriendRequest = 5003,     // 好友请求
        kFriendAdded = 5004,       // 添加好友成功
        kFriendRemoved = 5005,     // 删除好友成功

        // 队伍相关
        kPartyInvite = 6001,       // 队伍邀请
        kPartyJoined = 6002,       // 加入队伍
        kPartyLeft = 6003,         // 离开队伍
        kPartyMemberUpdate = 6004, // 队伍成员更新

        // 聊天相关
        kChatMessage = 7001,       // 聊天消息
        kChatChannelJoined = 7002, // 加入聊天频道
        kChatChannelLeft = 7003,   // 离开聊天频道

        // 系统相关
        kSystemMessage = 8001,     // 系统消息
        kErrorMessage = 8002,      // 错误消息
    };

    /**
     * @brief 消息发送回调
     *
     * @param session_id 会话ID
     * @param message_type 消息类型
     * @param data 消息数据
     * @param data_size 数据大小
     */
    using MessageSender = std::function<void(uint64_t session_id, MessageType message_type, const void* data, size_t data_size)>;

    /**
     * @brief 获取单例实例
     */
    static NotificationService& Instance();

    /**
     * @brief 设置消息发送器
     * @param sender 消息发送回调
     */
    void SetMessageSender(MessageSender sender);

    /**
     * @brief 发送消息给指定会话
     * @param session_id 会话ID
     * @param message_type 消息类型
     * @param data 消息数据
     * @param data_size 数据大小
     * @return 是否发送成功
     */
    bool SendToSession(uint64_t session_id, MessageType message_type, const void* data, size_t data_size);

    /**
     * @brief 发送消息给指定角色
     * @param character_id 角色ID
     * @param message_type 消息类型
     * @param data 消息数据
     * @param data_size 数据大小
     * @return 是否发送成功
     */
    bool SendToCharacter(uint64_t character_id, MessageType message_type, const void* data, size_t data_size);

    /**
     * @brief 广播消息给所有会话
     * @param message_type 消息类型
     * @param data 消息数据
     * @param data_size 数据大小
     */
    void Broadcast(MessageType message_type, const void* data, size_t data_size);

    /**
     * @brief 发送消息给指定地图的所有玩家
     * @param map_id 地图ID
     * @param message_type 消息类型
     * @param data 消息数据
     * @param data_size 数据大小
     */
    void SendToMap(uint16_t map_id, MessageType message_type, const void* data, size_t data_size);

    // ========== 便捷通知方法 ==========

    /**
     * @brief 通知角色升级
     * @param character_id 角色ID
     * @param new_level 新等级
     */
    void NotifyLevelUp(uint64_t character_id, uint16_t new_level);

    /**
     * @brief 通知经验更新
     * @param character_id 角色ID
     * @param exp 当前经验
     * @param exp_for_level 升级所需经验
     */
    void NotifyExpUpdate(uint64_t character_id, uint64_t exp, uint64_t exp_for_level);

    /**
     * @brief 通知金钱更新
     * @param character_id 角色ID
     * @param money 当前金钱
     */
    void NotifyMoneyUpdate(uint64_t character_id, uint64_t money);

    /**
     * @brief 通知传送
     * @param character_id 角色ID
     * @param map_id 目标地图ID
     * @param x 目标X坐标
     * @param y 目标Y坐标
     * @param z 目标Z坐标
     */
    void NotifyTeleport(uint64_t character_id, uint16_t map_id, float x, float y, float z);

    /**
     * @brief 通知HP/MP更新
     * @param character_id 角色ID
     * @param hp 当前HP
     * @param max_hp 最大HP
     * @param mp 当前MP
     * @param max_mp 最大MP
     */
    void NotifyHpMpUpdate(uint64_t character_id, uint32_t hp, uint32_t max_hp, uint32_t mp, uint32_t max_mp);

    /**
     * @brief 通知技能学习成功
     * @param character_id 角色ID
     * @param skill_id 技能ID
     * @param skill_level 技能等级
     */
    void NotifySkillLearned(uint64_t character_id, uint32_t skill_id, uint16_t skill_level);

    /**
     * @brief 通知物品获得
     * @param character_id 角色ID
     * @param item_id 物品ID
     * @param quantity 数量
     */
    void NotifyItemObtained(uint64_t character_id, uint32_t item_id, uint16_t quantity);

    /**
     * @brief 通知任务完成
     * @param character_id 角色ID
     * @param quest_id 任务ID
     */
    void NotifyQuestCompleted(uint64_t character_id, uint32_t quest_id);

    /**
     * @brief 通知任务进度更新
     * @param character_id 角色ID
     * @param quest_id 任务ID
     * @param objective_id 目标ID
     * @param progress 当前进度
     * @param required 需要的进度
     */
    void NotifyQuestProgress(uint64_t character_id, uint32_t quest_id, uint32_t objective_id, uint32_t progress, uint32_t required);

    /**
     * @brief 通知好友上线
     * @param character_id 角色ID
     * @param friend_id 好友角色ID
     * @param friend_name 好友名称
     */
    void NotifyFriendOnline(uint64_t character_id, uint64_t friend_id, const std::string& friend_name);

    /**
     * @brief 通知好友下线
     * @param character_id 角色ID
     * @param friend_id 好友角色ID
     */
    void NotifyFriendOffline(uint64_t character_id, uint64_t friend_id);

    /**
     * @brief 通知好友请求
     * @param character_id 角色ID
     * @param requester_id 请求者ID
     * @param requester_name 请求者名称
     */
    void NotifyFriendRequest(uint64_t character_id, uint64_t requester_id, const std::string& requester_name);

    /**
     * @brief 通知队伍邀请
     * @param character_id 角色ID
     * @param inviter_id 邀请者ID
     * @param inviter_name 邀请者名称
     * @param party_id 队伍ID
     */
    void NotifyPartyInvite(uint64_t character_id, uint64_t inviter_id, const std::string& inviter_name, uint64_t party_id);

    /**
     * @brief 通知加入队伍
     * @param character_id 角色ID
     * @param party_id 队伍ID
     */
    void NotifyPartyJoined(uint64_t character_id, uint64_t party_id);

    /**
     * @brief 通知离开队伍
     * @param character_id 角色ID
     */
    void NotifyPartyLeft(uint64_t character_id);

    /**
     * @brief 发送聊天消息
     * @param character_id 角色ID
     * @param channel 频道类型
     * @param sender_id 发送者ID
     * @param sender_name 发送者名称
     * @param message 消息内容
     */
    void SendChatMessage(uint64_t character_id, uint8_t channel, uint64_t sender_id, const std::string& sender_name, const std::string& message);

    /**
     * @brief 发送系统消息
     * @param character_id 角色ID (0表示广播给所有人)
     * @param message 消息内容
     */
    void SendSystemMessage(uint64_t character_id, const std::string& message);

    /**
     * @brief 发送错误消息
     * @param character_id 角色ID
     * @param error_code 错误码
     * @param message 错误消息
     */
    void SendErrorMessage(uint64_t character_id, uint32_t error_code, const std::string& message);

private:
    NotificationService();
    ~NotificationService() = default;
    NotificationService(const NotificationService&) = delete;
    NotificationService& operator=(const NotificationService&) = delete;

    MessageSender message_sender_;
    SessionManager* session_manager_;
};

} // namespace Network
} // namespace Core
} // namespace Murim
