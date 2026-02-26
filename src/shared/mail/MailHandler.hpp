#pragma once

#include <memory>
#include <cstdint>
#include <vector>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "MailManager.hpp"

namespace google {
namespace protobuf {
class Message;
}
}

namespace Murim {
namespace Game {

/**
 * @brief 邮件系统消息处理器
 *
 * 处理所有邮件相关的网络消息（完整 Protobuf 实现）
 */
class MailHandler {
public:
    /**
     * @brief 处理发送邮件请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleSendMailRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理读取邮件请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleReadMailRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理领取附件请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleTakeAttachmentRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理删除邮件请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleDeleteMailRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理获取邮件列表请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleGetMailListRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

private:
    /**
     * @brief 发送简单响应（仅用于错误情况）
     * @param conn 客户端连接
     * @param message_id 消息ID
     * @param result_code 结果码
     * @param data 响应数据
     */
    static void SendResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        uint16_t message_id,
        uint32_t result_code,
        const std::vector<uint8_t>& data);

    /**
     * @brief 发送 Protobuf 响应
     * @param conn 客户端连接
     * @param message_id 消息ID
     * @param response Protobuf 响应消息
     */
    static void SendResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        uint16_t message_id,
        const google::protobuf::Message& response);
};

} // namespace Game
} // namespace Murim
