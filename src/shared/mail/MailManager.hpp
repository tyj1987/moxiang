#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include "protocol/mail.pb.h"

namespace Murim {
namespace Game {

/**
 * @brief 邮件附件信息
 */
struct MailAttachment {
    uint64_t item_instance_id;
    uint32_t item_id;
    uint32_t count;
};

/**
 * @brief 邮件信息
 */
struct Mail {
    uint64_t mail_id;
    uint64_t sender_id;
    std::string sender_name;
    uint64_t receiver_id;
    std::string receiver_name;
    std::string title;
    std::string content;
    uint32_t mail_type;
    uint32_t status;
    uint64_t send_time;
    uint32_t expire_days;
    bool has_attachment;
    uint64_t attachment_gold;
    std::vector<MailAttachment> attachment_items;
    bool was_read;

    /**
     * @brief 检查邮件是否过期
     */
    bool IsExpired() const;

    /**
     * @brief 检查邮件是否有附件
     */
    bool HasAttachment() const {
        return attachment_gold > 0 || !attachment_items.empty();
    }
};

/**
 * @brief 邮件发送结果
 */
struct SendMailResult {
    bool success;
    uint32_t error_code;
    std::string error_message;
    uint64_t mail_id;
};

/**
 * @brief 邮件管理器
 *
 * 负责邮件的发送、接收、读取、删除等操作
 */
class MailManager {
public:
    /**
     * @brief 获取单例实例
     */
    static MailManager& Instance();

    /**
     * @brief 初始化邮件管理器
     */
    bool Initialize();

    // ========== 邮件发送 ==========

    /**
     * @brief 发送邮件
     * @param sender_id 发送者ID
     * @param receiver_name 接收者名称
     * @param title 邮件标题
     * @param content 邮件内容
     * @param gold 金币附件
     * @param items 物品附件
     * @return 发送结果
     */
    SendMailResult SendMail(
        uint64_t sender_id,
        const std::string& receiver_name,
        const std::string& title,
        const std::string& content,
        uint64_t gold = 0,
        const std::vector<MailAttachment>& items = {}
    );

    /**
     * @brief 发送系统邮件
     * @param receiver_id 接收者ID
     * @param title 邮件标题
     * @param content 邮件内容
     * @param gold 金币附件
     * @param items 物品附件
     * @return 发送结果
     */
    SendMailResult SendSystemMail(
        uint64_t receiver_id,
        const std::string& title,
        const std::string& content,
        uint64_t gold = 0,
        const std::vector<MailAttachment>& items = {}
    );

    // ========== 邮件查询 ==========

    /**
     * @brief 获取玩家的邮件列表
     * @param character_id 角色ID
     * @param page 页码
     * @param page_size 每页数量
     * @return 邮件列表
     */
    std::vector<Mail> GetMailList(
        uint64_t character_id,
        uint32_t page = 0,
        uint32_t page_size = 20
    );

    /**
     * @brief 获取邮件详情
     * @param mail_id 邮件ID
     * @return 邮件（如果存在）
     */
    Mail* GetMail(uint64_t mail_id);

    /**
     * @brief 获取玩家的未读邮件数量
     * @param character_id 角色ID
     * @return 未读邮件数量
     */
    uint32_t GetUnreadCount(uint64_t character_id);

    // ========== 邮件操作 ==========

    /**
     * @brief 标记邮件为已读
     * @param mail_id 邮件ID
     * @return 是否成功
     */
    bool MarkAsRead(uint64_t mail_id);

    /**
     * @brief 领取邮件附件
     * @param character_id 角色ID
     * @param mail_id 邮件ID
     * @return 是否成功
     */
    bool TakeAttachment(uint64_t character_id, uint64_t mail_id);

    /**
     * @brief 删除邮件
     * @param mail_id 邮件ID
     * @return 是否成功
     */
    bool DeleteMail(uint64_t mail_id);

    /**
     * @brief 清理过期邮件
     */
    void CleanExpiredMails();

    // ========== 邮件验证 ==========

    /**
     * @brief 验证邮件标题
     * @param title 标题
     * @return 是否有效
     */
    bool ValidateTitle(const std::string& title);

    /**
     * @brief 验证邮件内容
     * @param content 内容
     * @return 是否有效
     */
    bool ValidateContent(const std::string& content);

private:
    MailManager();
    ~MailManager() = default;
    MailManager(const MailManager&) = delete;
    MailManager& operator=(const MailManager&) = delete;

    // ========== 数据存储 ==========

    // player_id -> mails
    std::unordered_map<uint64_t, std::vector<Mail>> player_mails_;

    // mail_id -> mail (快速查找)
    std::unordered_map<uint64_t, Mail> mail_cache_;

    // 邮件ID计数器
    uint64_t next_mail_id_ = 1;

    // ========== 辅助方法 ==========

    /**
     * @brief 生成新的邮件ID
     */
    uint64_t GenerateMailId();

    /**
     * @brief 保存邮件到数据库
     */
    bool SaveMail(const Mail& mail);

    /**
     * @brief 从数据库加载邮件
     */
    bool LoadMails(uint64_t character_id);

    /**
     * @brief 通知玩家有新邮件
     */
    void NotifyNewMail(uint64_t character_id, const Mail& mail);
};

} // namespace Game
} // namespace Murim
