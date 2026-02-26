#include "LoginHandler.hpp"
#include "../../core/network/PacketSerializer.hpp"
#include "../../core/database/dao/AccountDAO.hpp"
#include "../../core/database/dao/CharacterDAO.hpp"
#include "core/spdlog_wrapper.hpp"
#include <random>

namespace Murim {
namespace AgentServer {

void LoginHandler::HandleLoginRequest(
    std::shared_ptr<Core::Network::Connection> connection,
    const murim::LoginRequest& request) {

    spdlog::info("Processing login request for user: {}", request.username());

    murim::LoginResponse response;
    response.set_response(murim::Response::SUCCESS);

    // 验证账号密码
    uint64_t account_id = ValidateAccount(request.username(), request.password());
    if (account_id == 0) {
        spdlog::warn("Login failed for user: {} (invalid credentials)", request.username());
        response.set_response(murim::Response::FAILURE);
        response.set_message("Invalid username or password");
        SendLoginResponse(connection, response);
        return;
    }

    // 检查客户端版本
    if (request.version() != "1.0.0") {  // TODO: 从配置读取
        spdlog::warn("Login failed for user: {} (invalid version: {})",
                      request.username(), request.version());
        response.set_response(murim::Response::FAILURE);
        response.set_message("Invalid client version");
        SendLoginResponse(connection, response);
        return;
    }

    // 创建会话
    uint64_t session_id = CreateSession(account_id);
    if (session_id == 0) {
        spdlog::error("Failed to create session for account: {}", account_id);
        response.set_response(murim::Response::FAILURE);
        response.set_message("Failed to create session");
        SendLoginResponse(connection, response);
        return;
    }

    // 生成认证令牌
    std::string auth_token = GenerateAuthToken(account_id);

    // 加载角色列表
    auto characters = LoadCharacterList(account_id);
    for (const auto& character : characters) {
        auto* character_info = response.add_characters();
        *character_info = character;
    }

    // 填充响应
    response.set_account_id(account_id);
    response.set_auth_token(auth_token);
    response.set_session_id(session_id);

    spdlog::info("Login successful for user: {} (account_id: {}, session_id: {})",
                  request.username(), account_id, session_id);

    // 发送响应
    SendLoginResponse(connection, response);
}

uint64_t LoginHandler::ValidateAccount(const std::string& username, const std::string& password) {
    // TODO: 实现数据库查询
    // 现在返回测试账户
    if (username == "test" && password == "test") {
        return 1001;  // 测试账户 ID
    }
    return 0;
}

uint64_t LoginHandler::CreateSession(uint64_t account_id) {
    // TODO: 实现会话创建
    // 生成唯一会话 ID
    static std::atomic<uint64_t> session_counter{10000};
    return session_counter.fetch_add(1);
}

std::string LoginHandler::GenerateAuthToken(uint64_t account_id) {
    // TODO: 实现安全的令牌生成
    // 现在生成简单令牌
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::string token = "AUTH_";
    for (int i = 0; i < 32; ++i) {
        token += "0123456789ABCDEF"[dis(gen)];
    }
    return token;
}

std::vector<murim::CharacterInfo> LoginHandler::LoadCharacterList(uint64_t account_id) {
    // TODO: 实现数据库查询
    // 现在返回空列表
    std::vector<murim::CharacterInfo> characters;

    // 测试数据
    murim::CharacterInfo test_char;
    test_char.set_character_id(2001);
    test_char.set_name("TestCharacter");
    test_char.set_class_type(1);  // 武士
    test_char.set_level(10);
    test_char.set_gender(1);
    characters.push_back(test_char);

    return characters;
}

void LoginHandler::SendLoginResponse(
    std::shared_ptr<Core::Network::Connection> connection,
    const murim::LoginResponse& response) {

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0201 = Account/LoginResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0201, data);
    if (!buffer) {
        spdlog::error("Failed to serialize login response");
        return;
    }

    // 发送响应
    connection->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send login response: {}", ec.message());
        } else {
            spdlog::debug("Login response sent: {} bytes", bytes_sent);
        }
    });
}

} // namespace AgentServer
} // namespace Murim
