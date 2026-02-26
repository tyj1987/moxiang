#pragma once

#include <memory>
#include <cstdint>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "Chat.hpp"

#ifdef SendMessage
#undef SendMessage
#endif

namespace Murim {
namespace Game {

/**
 * @brief 聊天系统消息处理器
 *
 * 处理所有聊天相关的网络消息
 */
class ChatHandler {
public:
    /**
     * @brief 处理发送消息请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleSendMessageRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理聊天历史请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleChatHistoryRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理加入频道请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleJoinChannelRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理离开频道请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleLeaveChannelRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理频道成员列表请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleChannelMemberListRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

private:
    /**
     * @brief 发送接收消息通知
     * @param conn 客户端连接
     * @param message 聊天消息
     */
    static void SendReceiveMessageNotification(
        std::shared_ptr<Core::Network::Connection> conn,
        const ChatMessage& message);

    /**
     * @brief 发送聊天历史响应
     * @param conn 客户端连接
     * @param result_code 结果码 (0=成功, 1=失败)
     * @param messages 消息列表
     */
    static void SendChatHistoryResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        int32_t result_code,
        const std::vector<ChatMessage>& messages);

    /**
     * @brief 发送加入频道响应
     * @param conn 客户端连接
     * @param result_code 结果码 (0=成功, 1=失败, 2=密码错误, 3=频道已满)
     * @param channel_id 频道ID
     * @param channel_name 频道名称
     * @param member_count 成员数量
     */
    static void SendJoinChannelResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        int32_t result_code,
        uint64_t channel_id,
        const std::string& channel_name,
        uint32_t member_count);

    /**
     * @brief 发送离开频道响应
     * @param conn 客户端连接
     * @param result_code 结果码 (0=成功, 1=失败)
     * @param channel_id 频道ID
     */
    static void SendLeaveChannelResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        int32_t result_code,
        uint64_t channel_id);

    /**
     * @brief 发送频道成员列表
     * @param conn 客户端连接
     * @param channel_id 频道ID
     * @param member_ids 成员ID列表
     */
    static void SendChannelMemberList(
        std::shared_ptr<Core::Network::Connection> conn,
        uint64_t channel_id,
        const std::vector<uint64_t>& member_ids);

    /**
     * @brief 广播系统消息
     * @param conn 客户端连接
     * @param message_type 消息类型 (0=普通, 1=警告, 2=错误, 3=成就)
     * @param content 消息内容
     * @param target_players 目标玩家列表（空表示全员广播）
     */
    static void BroadcastSystemMessage(
        std::shared_ptr<Core::Network::Connection> conn,
        uint32_t message_type,
        const std::string& content,
        const std::vector<uint64_t>& target_players);
};

} // namespace Game
} // namespace Murim
