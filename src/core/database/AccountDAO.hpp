#pragma once

#include <vector>
#include <optional>
#include "shared/account/Account.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"

namespace Murim {
namespace Core {
namespace Database {

/**
 * @brief 账号数据访问对象 (DAO)
 *
 * 负责账号和会话的数据库操作
 */
class AccountDAO {
public:
    // ========== 账号操作 ==========

    /**
     * @brief 创建账号
     * @param username 用户名
     * @param password_hash 密码哈希
     * @param salt 密码盐值
     * @param email 邮箱
     * @return 新创建的账号ID (失败返回 0)
     */
    static uint64_t Create(const std::string& username,
                          const std::string& password_hash,
                          const std::string& salt,
                          const std::string& email = "");

    /**
     * @brief 根据用户名查找账号
     * @param username 用户名
     * @return 账号数据
     */
    static std::optional<Game::Account> LoadByUsername(const std::string& username);

    /**
     * @brief 根据ID查找账号
     * @param account_id 账号ID
     * @return 账号数据
     */
    static std::optional<Game::Account> Load(uint64_t account_id);

    /**
     * @brief 检查用户名是否存在
     * @param username 用户名
     * @return 是否存在
     */
    static bool Exists(const std::string& username);

    /**
     * @brief 更新最后登录时间
     * @param account_id 账号ID
     * @param ip IP地址
     * @return 是否成功
     */
    static bool UpdateLastLoginTime(uint64_t account_id, const std::string& ip);

    /**
     * @brief 封禁账号
     * @param account_id 账号ID
     * @param reason 封禁原因
     * @param expire_time 过期时间 (可选)
     * @return 是否成功
     */
    static bool Ban(uint64_t account_id, const std::string& reason,
                   const std::chrono::system_clock::time_point& expire_time = {});

    /**
     * @brief 解封账号
     * @param account_id 账号ID
     * @return 是否成功
     */
    static bool Unban(uint64_t account_id);

    // ========== 会话操作 ==========

    /**
     * @brief 创建会话
     * @param account_id 账号ID
     * @param session_token 会话令牌
     * @param server_id 服务器ID
     * @return 新创建的会话ID (失败返回 0)
     */
    static uint64_t CreateSession(uint64_t account_id,
                                 const std::string& session_token,
                                 uint16_t server_id);

    /**
     * @brief 加载会话
     * @param session_id 会话ID
     * @return 会话数据
     */
    static std::optional<Game::Session> LoadSession(uint64_t session_id);

    /**
     * @brief 根据会话令牌加载会话
     * @param session_token 会话令牌
     * @return 会话数据
     */
    static std::optional<Game::Session> LoadSessionByToken(const std::string& session_token);

    /**
     * @brief 更新会话的最后活跃时间
     * @param session_id 会话ID
     * @return 是否成功
     */
    static bool UpdateSessionActiveTime(uint64_t session_id);

    /**
     * @brief 选择角色
     * @param session_id 会话ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    static bool SelectCharacter(uint64_t session_id, uint64_t character_id);

    /**
     * @brief 登出会话
     * @param session_id 会话ID
     * @param reason 登出原因
     * @return 是否成功
     */
    static bool LogoutSession(uint64_t session_id, uint8_t reason = 0);

    /**
     * @brief 账号的所有活跃会话
     * @param account_id 账号ID
     * @return 会话列表
     */
    static std::vector<Game::Session> GetActiveSessions(uint64_t account_id);

    /**
     * @brief 踢出账号的所有会话 (重复登录时使用)
     * @param account_id 账号ID
     * @param reason 登出原因
     * @return 受影响的会话数
     */
    static size_t KickAllSessions(uint64_t account_id, uint8_t reason = 3);

private:
    /**
     * @brief 从数据库行构建账号对象
     */
    static std::optional<Game::Account> RowToAccount(Result& result, int row);

    /**
     * @brief 从数据库行构建会话对象
     */
    static std::optional<Game::Session> RowToSession(Result& result, int row);
};

} // namespace Database
} // namespace Core
} // namespace Murim
