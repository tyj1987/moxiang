#include "ChatHandler.hpp"
#include "Chat.hpp"
#include "protocol/chat.pb.h"
#include "core/network/PacketSerializer.hpp"
#include "core/spdlog_wrapper.hpp"

#ifdef SendMessage
#undef SendMessage
#endif

namespace Murim {
namespace Game {

void ChatHandler::HandleSendMessageRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::SendMessageRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse SendMessageRequest");
        return;
    }

    spdlog::info("HandleSendMessageRequest: sender_id={}, channel_type={}, content={}",
                 request.sender_id(), request.channel_type(), request.content());

    // TODO: 暂时注释掉 SendMessage 调用，避免宏冲突
    /*
    // 发送消息
    auto channel_type = static_cast<ChatChannelType>(request.channel_type());
    auto success = ChatManager::Instance().SendMessage(
        request.sender_id(),
        channel_type,
        request.content(),
        request.receiver_id(),
        request.channel_name()
    );

    if (success) {
    */
        // 构建消息通知
        ChatMessage message;
        message.message_id = 0;  // TODO: 生成唯一消息ID
        message.sender_id = request.sender_id();
        message.sender_name = "";  // TODO: 获取发送者名称
        message.receiver_id = request.receiver_id();
        message.channel_type = static_cast<ChatChannelType>(request.channel_type());
        message.channel_name = request.channel_name();
        message.content = request.content();
        message.send_time = std::chrono::system_clock::now();

        // 广播消息
        SendReceiveMessageNotification(conn, message);
    //}
}

void ChatHandler::HandleChatHistoryRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::ChatHistoryRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse ChatHistoryRequest");
        return;
    }

    spdlog::info("HandleChatHistoryRequest: character_id={}, channel_type={}, count={}",
                 request.character_id(), request.channel_type(), request.count());

    // 获取聊天历史
    auto channel_type = static_cast<ChatChannelType>(request.channel_type());
    auto messages = ChatManager::Instance().GetChatHistory(
        channel_type,
        request.channel_name(),
        request.count()
    );

    // 发送响应
    SendChatHistoryResponse(conn, 0, messages);
}

void ChatHandler::HandleJoinChannelRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::JoinChannelRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse JoinChannelRequest");
        return;
    }

    spdlog::info("HandleJoinChannelRequest: character_id={}, channel_id={}",
                 request.character_id(), request.channel_id());

    // 暂时只记录日志，实际加入频道逻辑需要 ChatManager 支持
    spdlog::info("Character {} wants to join channel {}", request.character_id(), request.channel_id());

    // 发送响应（暂时假设成功）
    SendJoinChannelResponse(conn, 0, request.channel_id(), "", 0);
}

void ChatHandler::HandleLeaveChannelRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::LeaveChannelRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse LeaveChannelRequest");
        return;
    }

    spdlog::info("HandleLeaveChannelRequest: character_id={}, channel_id={}",
                 request.character_id(), request.channel_id());

    // 暂时只记录日志
    spdlog::info("Character {} wants to leave channel {}", request.character_id(), request.channel_id());

    // 发送响应（暂时假设成功）
    SendLeaveChannelResponse(conn, 0, request.channel_id());
}

void ChatHandler::HandleChannelMemberListRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 这个消息在 proto 中不存在，暂时只记录日志
    spdlog::info("HandleChannelMemberListRequest: called");
}

// ========== 响应发送方法 ==========

void ChatHandler::SendReceiveMessageNotification(
    std::shared_ptr<Core::Network::Connection> conn,
    const ChatMessage& message) {

    murim::ReceiveMessageNotification notification;
    notification.set_message_id(message.message_id);
    notification.set_sender_id(message.sender_id);
    notification.set_sender_name(message.sender_name);
    notification.set_receiver_id(message.receiver_id);
    notification.set_receiver_name(message.receiver_name);
    notification.set_channel_type(static_cast<uint32_t>(message.channel_type));
    notification.set_channel_name(message.channel_name);
    notification.set_content(message.content);
    notification.set_sender_level(0);  // TODO: 获取发送者等级
    notification.set_vip_level(0);     // TODO: 获取VIP等级

    // 设置时间戳
    auto timestamp = notification.mutable_timestamp();
    auto time_t_value = std::chrono::system_clock::to_time_t(message.send_time);
    timestamp->set_seconds(time_t_value);

    // 序列化通知
    std::vector<uint8_t> data(notification.ByteSizeLong());
    notification.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0002 = Chat/ReceiveMessageNotification)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0002, data);
    if (!buffer) {
        spdlog::error("Failed to serialize ReceiveMessageNotification");
        return;
    }

    // 发送通知
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send ReceiveMessageNotification: {}", ec.message());
        } else {
            spdlog::debug("ReceiveMessageNotification sent: {} bytes", bytes_sent);
        }
    });
}

void ChatHandler::SendChatHistoryResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    int32_t result_code,
    const std::vector<ChatMessage>& messages) {

    murim::ChatHistoryResponse response;

    // 设置 Response 字段
    auto* response_ptr = response.mutable_response();
    response_ptr->set_code(result_code);
    response_ptr->set_message(result_code == 0 ? "Success" : "Failed");

    // 添加历史消息
    for (const auto& msg : messages) {
        auto* message_ptr = response.add_messages();
        message_ptr->set_message_id(msg.message_id);
        message_ptr->set_sender_id(msg.sender_id);
        message_ptr->set_sender_name(msg.sender_name);
        message_ptr->set_channel_type(static_cast<uint32_t>(msg.channel_type));
        message_ptr->set_content(msg.content);

        // 设置时间戳
        auto timestamp = message_ptr->mutable_timestamp();
        auto time_t_value = std::chrono::system_clock::to_time_t(msg.send_time);
        timestamp->set_seconds(time_t_value);
    }

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0004 = Chat/ChatHistoryResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0004, data);
    if (!buffer) {
        spdlog::error("Failed to serialize ChatHistoryResponse");
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send ChatHistoryResponse: {}", ec.message());
        } else {
            spdlog::debug("ChatHistoryResponse sent: {} bytes", bytes_sent);
        }
    });
}

void ChatHandler::SendJoinChannelResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    int32_t result_code,
    uint64_t channel_id,
    const std::string& channel_name,
    uint32_t member_count) {

    murim::JoinChannelResponse response;

    // 设置 Response 字段
    auto* response_ptr = response.mutable_response();
    response_ptr->set_code(result_code);
    response_ptr->set_message(result_code == 0 ? "Success" : "Failed");

    response.set_channel_id(channel_id);
    response.set_channel_name(channel_name);
    response.set_member_count(member_count);

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0006 = Chat/JoinChannelResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0006, data);
    if (!buffer) {
        spdlog::error("Failed to serialize JoinChannelResponse");
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send JoinChannelResponse: {}", ec.message());
        } else {
            spdlog::debug("JoinChannelResponse sent: {} bytes", bytes_sent);
        }
    });
}

void ChatHandler::SendLeaveChannelResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    int32_t result_code,
    uint64_t channel_id) {

    murim::LeaveChannelResponse response;

    // 设置 Response 字段
    auto* response_ptr = response.mutable_response();
    response_ptr->set_code(result_code);
    response_ptr->set_message(result_code == 0 ? "Success" : "Failed");

    response.set_channel_id(channel_id);

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0008 = Chat/LeaveChannelResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0008, data);
    if (!buffer) {
        spdlog::error("Failed to serialize LeaveChannelResponse");
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send LeaveChannelResponse: {}", ec.message());
        } else {
            spdlog::debug("LeaveChannelResponse sent: {} bytes", bytes_sent);
        }
    });
}

void ChatHandler::SendChannelMemberList(
    std::shared_ptr<Core::Network::Connection> conn,
    uint64_t channel_id,
    const std::vector<uint64_t>& member_ids) {

    // 暂时不实现，因为 proto 中没有对应的响应消息
    spdlog::info("SendChannelMemberList: channel_id={}, member_count={}", channel_id, member_ids.size());
}

void ChatHandler::BroadcastSystemMessage(
    std::shared_ptr<Core::Network::Connection> conn,
    uint32_t message_type,
    const std::string& content,
    const std::vector<uint64_t>& target_players) {

    murim::SystemMessageNotification notification;
    notification.set_message_type(message_type);
    notification.set_content(content);

    // 添加目标玩家
    for (auto player_id : target_players) {
        notification.add_target_players(player_id);
    }

    // 序列化通知
    std::vector<uint8_t> data(notification.ByteSizeLong());
    notification.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x000C = Chat/SystemMessageNotification)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x000C, data);
    if (!buffer) {
        spdlog::error("Failed to serialize SystemMessageNotification");
        return;
    }

    // TODO: 广播给目标玩家
    spdlog::info("BroadcastSystemMessage: type={}, content={}", message_type, content);
}

} // namespace Game
} // namespace Murim
