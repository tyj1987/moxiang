#include "MailHandler.hpp"
#include "core/network/PacketSerializer.hpp"
#include "protocol/mail.pb.h"
#include "core/spdlog_wrapper.hpp"
#include <vector>

namespace Murim {
namespace Game {

void MailHandler::HandleSendMailRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleSendMailRequest: called");

    // 解析请求
    murim::SendMailRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse SendMailRequest");
        SendResponse(conn, 0x0D02, 1, {});  // 错误码 1: 解析失败
        return;
    }

    spdlog::info("SendMail: sender_id={}, receiver={}, title={}",
                 request.sender_id(), request.receiver_name(), request.title());

    // 转换附件列表
    std::vector<MailAttachment> attachments;
    for (const auto& item : request.items()) {
        MailAttachment attachment;
        attachment.item_instance_id = 0;  // 新物品，稍后生成
        attachment.item_id = item.item_id();
        attachment.count = item.count();
        attachments.push_back(attachment);
    }

    // 调用 MailManager 发送邮件
    auto result = MailManager::Instance().SendMail(
        request.sender_id(),
        request.receiver_name(),
        request.title(),
        request.content(),
        request.gold(),
        attachments
    );

    // 发送响应
    murim::SendMailResponse response;
    response.mutable_response()->set_code(result.success ? 0 : 1);
    response.set_mail_id(result.mail_id);

    if (!result.success) {
        response.mutable_response()->set_message(result.error_message);
    }

    SendResponse(conn, 0x0D02, response);
}

void MailHandler::HandleReadMailRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleReadMailRequest: called");

    // 解析请求
    murim::ReadMailRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse ReadMailRequest");
        SendResponse(conn, 0x0D04, 1, {});
        return;
    }

    spdlog::info("ReadMail: mail_id={}", request.mail_id());

    // 获取邮件
    auto* mail = MailManager::Instance().GetMail(request.mail_id());
    if (!mail) {
        SendResponse(conn, 0x0D04, 2, {});  // 错误码 2: 邮件不存在
        return;
    }

    // 标记为已读
    MailManager::Instance().MarkAsRead(request.mail_id());

    // 构建响应
    murim::ReadMailResponse response;
    response.mutable_response()->set_code(0);

    auto* mail_info = response.mutable_mail_info();
    mail_info->set_mail_id(mail->mail_id);
    mail_info->set_sender_id(mail->sender_id);
    mail_info->set_sender_name(mail->sender_name);
    mail_info->set_receiver_id(mail->receiver_id);
    mail_info->set_title(mail->title);
    mail_info->set_content(mail->content);
    mail_info->set_mail_type(mail->mail_type);
    mail_info->set_status(mail->status);
    mail_info->set_send_time(mail->send_time);
    mail_info->set_has_attachment(mail->has_attachment);

    // 附件信息
    if (mail->has_attachment) {
        mail_info->set_attachment_gold(mail->attachment_gold);

        for (const auto& item : mail->attachment_items) {
            auto* attachment = mail_info->add_attachment_items();
            attachment->set_item_id(item.item_id);
            attachment->set_count(item.count);
        }
    }

    SendResponse(conn, 0x0D04, response);
}

void MailHandler::HandleTakeAttachmentRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleTakeAttachmentRequest: called");

    // 解析请求
    murim::TakeAttachmentRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse TakeAttachmentRequest");
        SendResponse(conn, 0x0D06, 1, {});
        return;
    }

    spdlog::info("TakeAttachment: character_id={}, mail_id={}",
                 request.character_id(), request.mail_id());

    // 领取附件
    bool success = MailManager::Instance().TakeAttachment(
        request.character_id(),
        request.mail_id()
    );

    // 发送响应
    murim::TakeAttachmentResponse response;
    response.mutable_response()->set_code(success ? 0 : 3);  // 0=成功, 3=领取失败

    if (success) {
        auto* mail = MailManager::Instance().GetMail(request.mail_id());
        if (mail && mail->has_attachment) {
            response.set_received_gold(mail->attachment_gold);

            for (const auto& item : mail->attachment_items) {
                auto* attachment_info = response.add_received_items();
                attachment_info->set_item_instance_id(item.item_instance_id);
                attachment_info->set_item_id(item.item_id);
                attachment_info->set_count(item.count);
            }
        }
    } else {
        response.mutable_response()->set_message("Failed to take attachment");
    }

    SendResponse(conn, 0x0D06, response);
}

void MailHandler::HandleDeleteMailRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleDeleteMailRequest: called");

    // 解析请求
    murim::DeleteMailRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse DeleteMailRequest");
        SendResponse(conn, 0x0D08, 1, {});
        return;
    }

    spdlog::info("DeleteMail: character_id={}, mail_id={}",
                 request.character_id(), request.mail_id());

    // 删除邮件
    bool success = MailManager::Instance().DeleteMail(request.mail_id());

    // 发送响应
    murim::DeleteMailResponse response;
    response.mutable_response()->set_code(success ? 0 : 4);  // 0=成功, 4=删除失败

    if (!success) {
        response.mutable_response()->set_message("Failed to delete mail");
    }

    SendResponse(conn, 0x0D08, response);
}

void MailHandler::HandleGetMailListRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleGetMailListRequest: called");

    // 解析请求
    murim::GetMailListRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse GetMailListRequest");
        SendResponse(conn, 0x0D0A, 1, {});
        return;
    }

    spdlog::info("GetMailList: character_id={}, page={}, page_size={}",
                 request.character_id(), request.page(), request.page_size());

    // 获取邮件列表
    auto mails = MailManager::Instance().GetMailList(
        request.character_id(),
        request.page(),
        request.page_size()
    );

    // 获取未读数量
    uint32_t unread_count = MailManager::Instance().GetUnreadCount(request.character_id());

    // 构建响应
    murim::GetMailListResponse response;
    response.mutable_response()->set_code(0);
    response.set_total_count(mails.size());
    response.set_unread_count(unread_count);

    for (const auto& mail : mails) {
        auto* mail_info = response.add_mails();
        mail_info->set_mail_id(mail.mail_id);
        mail_info->set_sender_id(mail.sender_id);
        mail_info->set_sender_name(mail.sender_name);
        mail_info->set_title(mail.title);
        mail_info->set_mail_type(mail.mail_type);
        mail_info->set_status(mail.status);
        mail_info->set_send_time(mail.send_time);
        mail_info->set_has_attachment(mail.has_attachment);

        if (mail.has_attachment) {
            mail_info->set_attachment_gold(mail.attachment_gold);
        }
    }

    SendResponse(conn, 0x0D0A, response);
}

void MailHandler::SendResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    uint16_t message_id,
    uint32_t result_code,
    const std::vector<uint8_t>& data) {

    // 创建简单响应（仅用于错误情况）
    std::vector<uint8_t> response;
    response.push_back(static_cast<uint8_t>(result_code));
    response.insert(response.end(), data.begin(), data.end());

    auto buffer = Core::Network::PacketSerializer::Serialize(message_id, response);
    if (!buffer) {
        spdlog::error("Failed to serialize mail response: message_id=0x{:04X}", message_id);
        return;
    }

    conn->AsyncSend(buffer, [message_id](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send mail response 0x{:04X}: {}", message_id, ec.message());
        } else {
            spdlog::debug("Mail response 0x{:04X} sent: {} bytes", message_id, bytes_sent);
        }
    });
}

void MailHandler::SendResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    uint16_t message_id,
    const google::protobuf::Message& response) {

    // 序列化 protobuf 消息
    std::vector<uint8_t> data(response.ByteSizeLong());
    if (!response.SerializeToArray(data.data(), data.size())) {
        spdlog::error("Failed to serialize mail protobuf response: message_id=0x{:04X}", message_id);
        return;
    }

    // 使用 PacketSerializer 包装
    auto buffer = Core::Network::PacketSerializer::Serialize(message_id, data);
    if (!buffer) {
        spdlog::error("Failed to serialize mail response: message_id=0x{:04X}", message_id);
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [message_id](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send mail response 0x{:04X}: {}", message_id, ec.message());
        } else {
            spdlog::debug("Mail response 0x{:04X} sent: {} bytes", message_id, bytes_sent);
        }
    });
}

} // namespace Game
} // namespace Murim
