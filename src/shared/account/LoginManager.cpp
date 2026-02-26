#include "LoginManager.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <random>
#include <sstream>
#include <iomanip>
#include "core/database/CharacterDAO.hpp"

using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Game {

// 为Database namespace创建别名
namespace DB = Murim::Core::Database;

LoginManager& LoginManager::Instance() {
    static LoginManager instance;
    return instance;
}

// ========== 账号管理 ==========

LoginManager::CreateAccountResult LoginManager::CreateAccount(
    const std::string& username,
    const std::string& password,
    const std::string& email
) {
    CreateAccountResult result;
    result.result = LoginResult::kSystemError;
    result.account_id = 0;

    // 验证用户名格式
    if (username.empty() || username.length() < 3 || username.length() > 32) {
        spdlog::warn("Invalid username length: {}", username.length());
        result.result = LoginResult::kSystemError;
        return result;
    }

    // 验证密码格式
    if (password.length() < 6 || password.length() > 32) {
        spdlog::warn("Invalid password length: {}", password.length());
        result.result = LoginResult::kSystemError;
        return result;
    }

    // 检查用户名是否已存在
    if (DB::AccountDAO::Exists(username)) {
        spdlog::warn("Username already exists: {}", username);
        result.result = LoginResult::kAccountNotFound;  // 复用这个错误码
        return result;
    }

    // 生成密码哈希和盐值
    auto [hash, salt] = HashPassword(password);

    // 创建账号
    uint64_t account_id = DB::AccountDAO::Create(username, hash, salt, email);

    if (account_id == 0) {
        spdlog::error("Failed to create account: {}", username);
        return result;
    }

    spdlog::info("Account created successfully: id={}, username={}", account_id, username);

    result.result = LoginResult::kSuccess;
    result.account_id = account_id;
    return result;
}

// ========== 登录/登出 ==========

LoginManager::LoginResponse LoginManager::Login(
    const std::string& username,
    const std::string& password,
    const std::string& client_ip,
    uint16_t server_id
) {
    LoginResponse response;
    response.result = LoginResult::kSystemError;
    response.session_id = 0;

    // 加载账号
    auto account = DB::AccountDAO::LoadByUsername(username);
    if (!account.has_value()) {
        spdlog::warn("Login failed: account not found - {}", username);
        response.result = LoginResult::kAccountNotFound;
        return response;
    }

    // 检查账号状态
    if (!account->CanLogin()) {
        if (account->IsBanned()) {
            spdlog::warn("Login failed: account banned - {}", username);
            response.result = LoginResult::kAccountBanned;
        } else {
            spdlog::warn("Login failed: account invalid - {}", username);
            response.result = LoginResult::kSystemError;
        }
        return response;
    }

    // 验证密码
    if (!VerifyPassword(password, account->password_hash, account->salt)) {
        spdlog::warn("Login failed: wrong password - {}", username);
        response.result = LoginResult::kWrongPassword;
        return response;
    }

    // 检查是否已有活跃会话
    auto active_sessions = DB::AccountDAO::GetActiveSessions(account->account_id);
    if (!active_sessions.empty()) {
        spdlog::info("Account {} already has {} active sessions, kicking...",
                    username, active_sessions.size());

        // 踢出所有现有会话
        DB::AccountDAO::KickAllSessions(account->account_id, 3);  // reason=3:重复登录
    }

    // 生成会话令牌
    std::string session_token = GenerateSessionToken();

    // 创建会话
    uint64_t session_id = DB::AccountDAO::CreateSession(
        account->account_id,
        session_token,
        server_id
    );

    if (session_id == 0) {
        spdlog::error("Failed to create session for account: {}", username);
        response.result = LoginResult::kSystemError;
        return response;
    }

    // 更新最后登录时间
    DB::AccountDAO::UpdateLastLoginTime(account->account_id, client_ip);

    spdlog::info("Login successful: username={}, session_id={}", username, session_id);

    response.result = LoginResult::kSuccess;
    response.session_id = session_id;
    response.session_token = session_token;
    return response;
}

LoginManager::LoginResponse LoginManager::LoginByToken(
    const std::string& session_token,
    const std::string& client_ip
) {
    LoginResponse response;
    response.result = LoginResult::kSystemError;
    response.session_id = 0;

    // 加载会话
    auto session = DB::AccountDAO::LoadSessionByToken(session_token);
    if (!session.has_value()) {
        spdlog::warn("Login by token failed: session not found");
        response.result = LoginResult::kAccountNotFound;
        return response;
    }

    // 检查会话有效性
    if (!session->IsValid()) {
        spdlog::warn("Login by token failed: session invalid - {}", session->session_id);
        response.result = LoginResult::kAccountNotFound;
        return response;
    }

    // 检查会话超时
    if (session->IsTimeout(300)) {  // 5分钟超时
        spdlog::warn("Login by token failed: session timeout - {}", session->session_id);

        // 登出超时会话
        DB::AccountDAO::LogoutSession(session->session_id, 1);  // reason=1:超时
        response.result = LoginResult::kAccountNotFound;
        return response;
    }

    // 更新会话活跃时间
    DB::AccountDAO::UpdateSessionActiveTime(session->session_id);

    // 加载账号
    auto account = DB::AccountDAO::Load(session->account_id);
    if (!account.has_value() || !account->CanLogin()) {
        spdlog::warn("Login by token failed: account invalid - {}", session->account_id);
        response.result = LoginResult::kAccountBanned;
        return response;
    }

    spdlog::info("Login by token successful: session_id={}", session->session_id);

    response.result = LoginResult::kSuccess;
    response.session_id = session->session_id;
    response.session_token = session_token;
    return response;
}

LogoutResult LoginManager::Logout(
    uint64_t session_id,
    uint8_t reason
) {
    // 检查会话是否存在
    auto session = DB::AccountDAO::LoadSession(session_id);
    if (!session.has_value()) {
        spdlog::warn("Logout failed: session not found - {}", session_id);
        return LogoutResult::kSessionNotFound;
    }

    // 检查是否已登出
    if (!session->IsValid()) {
        spdlog::warn("Logout failed: already logged out - {}", session_id);
        return LogoutResult::kAlreadyLoggedOut;
    }

    // 登出会话
    bool success = DB::AccountDAO::LogoutSession(session_id, reason);

    if (success) {
        spdlog::info("Logout successful: session_id={}, reason={}", session_id, reason);
    }

    return success ? LogoutResult::kSuccess : LogoutResult::kSystemError;
}

bool LoginManager::Heartbeat(uint64_t session_id) {
    // 检查会话是否有效
    if (!IsSessionValid(session_id)) {
        return false;
    }

    // 更新活跃时间
    return DB::AccountDAO::UpdateSessionActiveTime(session_id);
}

// ========== 角色选择 ==========

CharacterSelectResult LoginManager::SelectCharacter(
    uint64_t session_id,
    uint64_t character_id
) {
    // 加载会话
    auto session = DB::AccountDAO::LoadSession(session_id);
    if (!session.has_value()) {
        spdlog::warn("Select character failed: session not found - {}", session_id);
        return CharacterSelectResult::kSystemError;
    }

    // 检查会话是否有效
    if (!session->IsValid()) {
        spdlog::warn("Select character failed: session invalid - {}", session_id);
        return CharacterSelectResult::kSystemError;
    }

    // 加载角色
    auto character = Murim::Core::Database::CharacterDAO::Load(character_id);
    if (!character.has_value()) {
        spdlog::warn("Select character failed: character not found - {}", character_id);
        return CharacterSelectResult::kCharacterNotFound;
    }

    // 检查角色状态
    if (!character->IsValid()) {
        spdlog::warn("Select character failed: character deleted - {}", character_id);
        return CharacterSelectResult::kCharacterDeleted;
    }

    // 检查角色归属
    if (character->account_id != session->account_id) {
        spdlog::warn("Select character failed: character not owned - char_id={}, account_id={}",
                     character_id, character->account_id);
        return CharacterSelectResult::kNotOwned;
    }

    // TODO: 检查角色是否已在其他会话中登录

    // 选择角色
    bool success = DB::AccountDAO::SelectCharacter(session_id, character_id);

    if (success) {
        spdlog::info("Character selected: session_id={}, character_id={}", session_id, character_id);
    }

    return success ? CharacterSelectResult::kSuccess : CharacterSelectResult::kSystemError;
}

bool LoginManager::DeselectCharacter(uint64_t session_id) {
    // 取消选择角色 = 设置 character_id = 0
    return DB::AccountDAO::SelectCharacter(session_id, 0);
}

// ========== 验证辅助 ==========

bool LoginManager::VerifyPassword(
    const std::string& password,
    const std::string& hash,
    const std::string& salt
) {
    // 将十六进制 salt 转换回字节
    std::vector<unsigned char> salt_bytes;
    for (size_t i = 0; i < salt.length(); i += 2) {
        std::string byte_string = salt.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::strtol(byte_string.c_str(), nullptr, 16));
        salt_bytes.push_back(byte);
    }

    // 使用 SHA-256 哈希 (password + salt)
    unsigned char input_hash_bytes[32];
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(mdctx, password.c_str(), password.length());
    EVP_DigestUpdate(mdctx, salt_bytes.data(), salt_bytes.size());
    EVP_DigestFinal_ex(mdctx, input_hash_bytes, nullptr);
    EVP_MD_CTX_free(mdctx);

    // 转换为十六进制字符串
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < 32; ++i) {
        ss << std::setw(2) << static_cast<unsigned int>(input_hash_bytes[i]);
    }
    std::string input_hash = ss.str();

    // 调试日志
    spdlog::debug("[VERIFY] Password: {}, salt_len: {}, input_hash: {}", password, salt.length(), input_hash);
    spdlog::debug("[VERIFY] Expected hash: {}", hash);

    // 比较哈希值
    bool match = (input_hash == hash);
    spdlog::debug("[VERIFY] Hash match: {}", match);
    return match;
}

std::pair<std::string, std::string> LoginManager::HashPassword(const std::string& password) {
    // 生成随机盐值 (16 bytes)
    unsigned char salt[16];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        spdlog::error("Failed to generate random salt");
        return {"", ""};
    }

    // 使用 SHA-256 哈希
    unsigned char hash[32];
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);

    // 添加密码
    EVP_DigestUpdate(mdctx, password.c_str(), password.length());

    // 添加盐值
    EVP_DigestUpdate(mdctx, salt, sizeof(salt));

    EVP_DigestFinal_ex(mdctx, hash, nullptr);
    EVP_MD_CTX_free(mdctx);

    // 转换为十六进制字符串
    auto to_hex = [](const unsigned char* data, size_t len) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < len; ++i) {
            ss << std::setw(2) << static_cast<unsigned int>(data[i]);
        }
        return ss.str();
    };

    std::string hash_str = to_hex(hash, sizeof(hash));
    std::string salt_str = to_hex(salt, sizeof(salt));

    return {hash_str, salt_str};
}

std::string LoginManager::GenerateSessionToken() {
    // 生成随机 UUID v4
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis(0, UINT64_MAX);

    uint64_t a = dis(gen);
    uint64_t b = dis(gen);

    // 设置 UUID 版本和变体
    a = (a & 0xFFFFFFFFFFFF0FFF) | 0x0000000000004000;  // version 4
    b = (b & 0x3FFFFFFFFFFFFFFF) | 0x8000000000000000;  // variant 1

    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    // 格式: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    ss << std::setw(8) << (a >> 32) << "-";
    ss << std::setw(4) << ((a >> 16) & 0xFFFF) << "-4";
    ss << std::setw(3) << (a & 0x0FFF) << "-";
    ss << std::setw(4) << ((b >> 48) | 0x8000) << "-";
    ss << std::setw(12) << (b & 0x0FFFFFFFFFFFF);

    return ss.str();
}

bool LoginManager::IsSessionValid(uint64_t session_id) {
    auto session = DB::AccountDAO::LoadSession(session_id);
    return session.has_value() && session->IsValid();
}

bool LoginManager::IsSessionTimeout(uint64_t session_id, uint32_t timeout_seconds) {
    auto session = DB::AccountDAO::LoadSession(session_id);
    if (!session.has_value()) {
        return true;  // 不存在的会话视为超时
    }

    return session->IsTimeout(timeout_seconds);
}

// ========== 会话查询 ==========

std::optional<Session> LoginManager::GetSession(uint64_t session_id) {
    return DB::AccountDAO::LoadSession(session_id);
}

std::vector<Session> LoginManager::GetAccountSessions(uint64_t account_id) {
    return DB::AccountDAO::GetActiveSessions(account_id);
}

// ========== 踢出功能 ==========

bool LoginManager::KickSession(uint64_t session_id, uint8_t reason) {
    return DB::AccountDAO::LogoutSession(session_id, reason);
}

size_t LoginManager::KickAccount(uint64_t account_id, uint8_t reason) {
    return DB::AccountDAO::KickAllSessions(account_id, reason);
}

} // namespace Game
} // namespace Murim
