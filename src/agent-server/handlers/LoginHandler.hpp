#pragma once

#include <memory>
#include "../../core/network/Connection.hpp"
#include "protocol/account.pb.h"

namespace Murim {
namespace AgentServer {

/**
 * @brief 登录消息处理器
 *
 * 处理客户端登录请求
 */
class LoginHandler {
public:
    /**
     * @brief 处理登录请求
     * @param connection 客户端连接
     * @param request 登录请求
     */
    static void HandleLoginRequest(
        std::shared_ptr<Core::Network::Connection> connection,
        const murim::LoginRequest& request);

private:
    /**
     * @brief 验证账号密码
     * @param username 用户名
     * @param password 密码 (已哈希)
     * @return 账户 ID，验证失败返回 0
     */
    static uint64_t ValidateAccount(const std::string& username, const std::string& password);

    /**
     * @brief 创建会话
     * @param account_id 账户 ID
     * @return 会话 ID
     */
    static uint64_t CreateSession(uint64_t account_id);

    /**
     * @brief 生成认证令牌
     * @param account_id 账户 ID
     * @return 认证令牌
     */
    static std::string GenerateAuthToken(uint64_t account_id);

    /**
     * @brief 加载角色列表
     * @param account_id 账户 ID
     * @return 角色列表
     */
    static std::vector<murim::CharacterInfo> LoadCharacterList(uint64_t account_id);

    /**
     * @brief 发送登录响应
     * @param connection 客户端连接
     * @param response 登录响应
     */
    static void SendLoginResponse(
        std::shared_ptr<Core::Network::Connection> connection,
        const murim::LoginResponse& response);
};

} // namespace AgentServer
} // namespace Murim
