#include "MailManager.hpp"
#include "core/database/CharacterDAO.hpp"
#include "core/localization/LocalizationManager.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>
#include <chrono>

namespace Murim {
namespace Game {

// ========== Mail ==========

bool Mail::IsExpired() const {
    auto now = std::chrono::system_clock::now();
    auto send_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::time_point{} + std::chrono::seconds(send_time));
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto diff_days = (now_time_t - send_time_t) / (24 * 3600);
    return diff_days >= expire_days;
}

// ========== MailManager ==========

MailManager& MailManager::Instance() {
    static MailManager instance;
    return instance;
}

bool MailManager::Initialize() {
    spdlog::info("Initializing MailManager...");

    // TODO: 从数据库加载所有邮件
    // 暂时只记录日志

    spdlog::info("MailManager initialized successfully");
    return true;
}

// ========== 邮件发送 ==========

SendMailResult MailManager::SendMail(
    uint64_t sender_id,
    const std::string& receiver_name,
    const std::string& title,
    const std::string& content,
    uint64_t gold,
    const std::vector<MailAttachment>& items) {

    SendMailResult result;
    result.success = false;
    result.error_code = 1;
    result.error_message = "Unknown error";
    result.mail_id = 0;

    // 验证标题和内容
    if (!ValidateTitle(title)) {
        result.error_code = 1;
        result.error_message = "Invalid title";
        return result;
    }

    if (!ValidateContent(content)) {
        result.error_code = 2;
        result.error_message = "Invalid content";
        return result;
    }

    // TODO: 查找接收者ID
    uint64_t receiver_id = 0;  // 从数据库查找
    /*
    auto character = CharacterDAO::GetByName(receiver_name);
    if (!character) {
        result.error_code = 3;
        result.error_message = "Receiver not found";
        return result;
    }
    receiver_id = character->character_id;
    */

    // 创建邮件
    Mail mail;
    mail.mail_id = GenerateMailId();
    mail.sender_id = sender_id;
    mail.sender_name = "";  // TODO: 获取发送者名称
    mail.receiver_id = receiver_id;
    mail.receiver_name = receiver_name;
    mail.title = title;
    mail.content = content;
    mail.mail_type = static_cast<uint32_t>(murim::MailType::NORMAL);
    mail.status = static_cast<uint32_t>(murim::MailStatus::UNREAD);
    mail.send_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    mail.expire_days = 30;  // 默认30天有效期
    mail.has_attachment = (gold > 0 || !items.empty());
    mail.attachment_gold = gold;
    mail.attachment_items = items;
    mail.was_read = false;

    // 保存邮件
    if (!SaveMail(mail)) {
        result.error_code = 4;
        result.error_message = "Failed to save mail";
        return result;
    }

    // 添加到玩家的邮件列表
    player_mails_[receiver_id].push_back(mail);
    mail_cache_[mail.mail_id] = mail;

    // 通知接收者
    NotifyNewMail(receiver_id, mail);

    result.success = true;
    result.error_code = 0;
    result.error_message = "Success";
    result.mail_id = mail.mail_id;

    spdlog::info("Mail sent: from={} to={}, title={}", sender_id, receiver_id, title);

    return result;
}

SendMailResult MailManager::SendSystemMail(
    uint64_t receiver_id,
    const std::string& title,
    const std::string& content,
    uint64_t gold,
    const std::vector<MailAttachment>& items) {

    SendMailResult result;
    result.success = false;
    result.error_code = 1;
    result.error_message = "Unknown error";
    result.mail_id = 0;

    // 创建系统邮件
    Mail mail;
    mail.mail_id = GenerateMailId();
    mail.sender_id = 0;  // 系统邮件发送者为0
    mail.sender_name = "SYSTEM";
    mail.receiver_id = receiver_id;
    mail.receiver_name = "";  // TODO: 获取接收者名称
    mail.title = title;
    mail.content = content;
    mail.mail_type = static_cast<uint32_t>(murim::MailType::SYSTEM);
    mail.status = static_cast<uint32_t>(murim::MailStatus::UNREAD);
    mail.send_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    mail.expire_days = 30;
    mail.has_attachment = (gold > 0 || !items.empty());
    mail.attachment_gold = gold;
    mail.attachment_items = items;
    mail.was_read = false;

    // 保存邮件
    if (!SaveMail(mail)) {
        result.error_code = 4;
        result.error_message = "Failed to save mail";
        return result;
    }

    // 添加到玩家的邮件列表
    player_mails_[receiver_id].push_back(mail);
    mail_cache_[mail.mail_id] = mail;

    // 通知接收者
    NotifyNewMail(receiver_id, mail);

    result.success = true;
    result.error_code = 0;
    result.error_message = "Success";
    result.mail_id = mail.mail_id;

    spdlog::info("System mail sent: to={}, title={}", receiver_id, title);

    return result;
}

// ========== 邮件查询 ==========

std::vector<Mail> MailManager::GetMailList(
    uint64_t character_id,
    uint32_t page,
    uint32_t page_size) {

    std::vector<Mail> result;

    // 查找玩家的邮件
    auto it = player_mails_.find(character_id);
    if (it == player_mails_.end()) {
        // 尝试从数据库加载
        LoadMails(character_id);
        it = player_mails_.find(character_id);
        if (it == player_mails_.end()) {
            return result;  // 没有邮件
        }
    }

    // 分页
    auto& mails = it->second;
    uint32_t start = page * page_size;
    uint32_t end = start + page_size;

    for (uint32_t i = start; i < end && i < mails.size(); ++i) {
        result.push_back(mails[i]);
    }

    spdlog::info("GetMailList: character_id={}, count={}", character_id, result.size());

    return result;
}

Mail* MailManager::GetMail(uint64_t mail_id) {
    auto it = mail_cache_.find(mail_id);
    if (it != mail_cache_.end()) {
        return &it->second;
    }
    return nullptr;
}

uint32_t MailManager::GetUnreadCount(uint64_t character_id) {
    auto it = player_mails_.find(character_id);
    if (it == player_mails_.end()) {
        return 0;
    }

    uint32_t count = 0;
    for (const auto& mail : it->second) {
        if (mail.status == static_cast<uint32_t>(murim::MailStatus::UNREAD)) {
            ++count;
        }
    }

    return count;
}

// ========== 邮件操作 ==========

bool MailManager::MarkAsRead(uint64_t mail_id) {
    auto* mail = GetMail(mail_id);
    if (!mail) {
        spdlog::error("MarkAsRead: mail not found, mail_id={}", mail_id);
        return false;
    }

    mail->was_read = true;
    mail->status = static_cast<uint32_t>(murim::MailStatus::READ);

    // TODO: 更新数据库

    spdlog::info("Mail marked as read: mail_id={}", mail_id);
    return true;
}

bool MailManager::TakeAttachment(uint64_t character_id, uint64_t mail_id) {
    auto* mail = GetMail(mail_id);
    if (!mail) {
        spdlog::error("TakeAttachment: mail not found, mail_id={}", mail_id);
        return false;
    }

    if (mail->receiver_id != character_id) {
        spdlog::error("TakeAttachment: mail does not belong to character");
        return false;
    }

    if (!mail->has_attachment) {
        spdlog::error("TakeAttachment: mail has no attachment");
        return false;
    }

    if (mail->status == static_cast<uint32_t>(murim::MailStatus::ATTACHMENT_TAKEN)) {
        spdlog::error("TakeAttachment: attachment already taken");
        return false;
    }

    // TODO: 将金币和物品添加到玩家的背包
    // TODO: 验证背包空间

    mail->status = static_cast<uint32_t>(murim::MailStatus::ATTACHMENT_TAKEN);

    // TODO: 更新数据库

    spdlog::info("Attachment taken: mail_id={}, gold={}, items={}",
                 mail_id, mail->attachment_gold, mail->attachment_items.size());

    return true;
}

bool MailManager::DeleteMail(uint64_t mail_id) {
    auto* mail = GetMail(mail_id);
    if (!mail) {
        spdlog::error("DeleteMail: mail not found, mail_id={}", mail_id);
        return false;
    }

    mail->status = static_cast<uint32_t>(murim::MailStatus::DELETED);

    // TODO: 从数据库删除

    spdlog::info("Mail deleted: mail_id={}", mail_id);
    return true;
}

void MailManager::CleanExpiredMails() {
    spdlog::info("Cleaning expired mails...");

    uint32_t cleaned_count = 0;

    for (auto& [character_id, mails] : player_mails_) {
        auto it = mails.begin();
        while (it != mails.end()) {
            if (it->IsExpired() || it->status == static_cast<uint32_t>(murim::MailStatus::DELETED)) {
                it = mails.erase(it);
                ++cleaned_count;
            } else {
                ++it;
            }
        }
    }

    spdlog::info("Cleaned {} mails", cleaned_count);
}

// ========== 邮件验证 ==========

bool MailManager::ValidateTitle(const std::string& title) {
    if (title.empty() || title.length() > 100) {
        return false;
    }
    return true;
}

bool MailManager::ValidateContent(const std::string& content) {
    if (content.empty() || content.length() > 1000) {
        return false;
    }
    return true;
}

// ========== 辅助方法 ==========

uint64_t MailManager::GenerateMailId() {
    return next_mail_id_++;
}

bool MailManager::SaveMail(const Mail& mail) {
    // TODO: 保存到数据库
    spdlog::debug("Saving mail to database: mail_id={}", mail.mail_id);
    return true;
}

// ========== Singleton 构造函数 ==========

MailManager::MailManager()
    : next_mail_id_(1) {
}

// 析构函数在头文件中定义为 default

bool MailManager::LoadMails(uint64_t character_id) {
    // TODO: 从数据库加载
    spdlog::debug("Loading mails from database: character_id={}", character_id);
    return true;
}

void MailManager::NotifyNewMail(uint64_t character_id, const Mail& mail) {
    // TODO: 通知玩家有新邮件
    spdlog::info("New mail notification: character_id={}, mail_id={}", character_id, mail.mail_id);
}

} // namespace Game
} // namespace Murim
