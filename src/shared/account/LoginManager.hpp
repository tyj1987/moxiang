#pragma once

#include "Account.hpp"
#include "core/database/AccountDAO.hpp"
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace Murim {
namespace Game {

/**
 * @brief 登录管理器
 *
 * 负责账号认证、会话管理、角色选择等登录流程
 * 对应 legacy 的登录相关逻辑
 */
class LoginManager {
public:
    /**
     * @brief 获取单例实例
     */
    static LoginManager& Instance();

    // ========== 账号管理 ==========

    /**
     * @brief 创建账号
     * @param username 用户名
     * @param password 密码 (明文)
     * @param email 邮箱
     * @return 登录结果
     */
    struct CreateAccountResult {
        LoginResult result;
        uint64_t account_id;
    };

    static CreateAccountResult CreateAccount(
        const std::string& username,
        const std::string& password,
        const std::string& email = ""
    );

    // ========== 登录/登出 ==========

    /**
     * @brief 账号密码登录
     * @param username 用户名
     * @param password 密码 (明文)
     * @param client_ip 客户端IP
     * @param server_id 服务器ID
     * @return 登录结果 + 会话信息
     */
    struct LoginResponse {
        LoginResult result;
        uint64_t session_id;
        std::string session_token;
    };

    static LoginResponse Login(
        const std::string& username,
        const std::string& password,
        const std::string& client_ip,
        uint16_t server_id
    );

    /**
     * @brief 会话令牌登录 (已登录用户重连)
     * @param session_token 会话令牌
     * @return 登录结果 + 会话信息
     */
    static LoginResponse LoginByToken(
        const std::string& session_token,
        const std::string& client_ip
    );

    /**
     * @brief 登出
     * @param session_id 会话ID
     * @param reason 登出原因
     * @return 登出结果
     */
    static LogoutResult Logout(
        uint64_t session_id,
        uint8_t reason = 0
    );

    /**
     * @brief 心跳 (保持会话活跃)
     * @param session_id 会话ID
     * @return 是否成功
     */
    static bool Heartbeat(uint64_t session_id);

    // ========== 角色选择 ==========

    /**
     * @brief 选择角色进入游戏
     * @param session_id 会话ID
     * @param character_id 角色ID
     * @return 选择结果
     */
    static CharacterSelectResult SelectCharacter(
        uint64_t session_id,
        uint64_t character_id
    );

    /**
     * @brief 取消角色选择 (返回角色选择界面)
     * @param session_id 会话ID
     * @return 是否成功
     */
    static bool DeselectCharacter(uint64_t session_id);

    // ========== 验证辅助 ==========

    /**
     * @brief 验证密码
     * @param password 明文密码
     * @param hash 密码哈希
     * @param salt 密码盐值
     * @return 是否正确
     */
    static bool VerifyPassword(
        const std::string& password,
        const std::string& hash,
        const std::string& salt
    );

    /**
     * @brief 生成密码哈希和盐值
     * @param password 明文密码
     * @return (hash, salt)
     */
    static std::pair<std::string, std::string> HashPassword(const std::string& password);

    /**
     * @brief 生成会话令牌 (UUID)
     * @return UUID 字符串
     */
    static std::string GenerateSessionToken();

    /**
     * @brief 检查会话是否有效
     * @param session_id 会话ID
     * @return 是否有效
     */
    static bool IsSessionValid(uint64_t session_id);

    /**
     * @brief 检查会话是否超时
     * @param session_id 会话ID
     * @param timeout_seconds 超时时间(秒)
     * @return 是否超时
     */
    static bool IsSessionTimeout(uint64_t session_id, uint32_t timeout_seconds = 300);

    // ========== 会话查询 ==========

    /**
     * @brief 获取会话信息
     * @param session_id 会话ID
     * @return 会话数据
     */
    static std::optional<Session> GetSession(uint64_t session_id);

    /**
     * @brief 获取账号的所有活跃会话
     * @param account_id 账号ID
     * @return 会话列表
     */
    static std::vector<Session> GetAccountSessions(uint64_t account_id);

    // ========== 踢出功能 ==========

    /**
     * @brief 踢出会话
     * @param session_id 会话ID
     * @param reason 原因
     * @return 是否成功
     */
    static bool KickSession(uint64_t session_id, uint8_t reason = 2);

    /**
     * @brief 踢出账号的所有会话
     * @param account_id 账号ID
     * @param reason 原因
     * @return 踢出的会话数
     */
    static size_t KickAccount(uint64_t account_id, uint8_t reason = 2);

private:
    LoginManager() = default;
    ~LoginManager() = default;
    LoginManager(const LoginManager&) = delete;
    LoginManager& operator=(const LoginManager&) = delete;
};

} // namespace Game
} // namespace Murim
