#pragma once

#include <string>
#include <cstdint>
#include <chrono>

namespace Murim {
namespace Game {

struct Account {
    uint64_t account_id = 0;
    std::string username;
    std::string password_hash;
    std::string salt;
    std::string email;
    std::string phone;

    std::chrono::system_clock::time_point create_time;
    std::chrono::system_clock::time_point last_login_time;
    std::string last_login_ip;

    uint8_t status = 0;
    std::string ban_reason;
    std::chrono::system_clock::time_point ban_expire_time;

    bool IsValid() const {
        return status == 0 && account_id != 0;
    }

    bool IsBanned() const {
        if (status != 1) {
            return false;
        }

        if (ban_expire_time.time_since_epoch().count() > 0) {
            auto now = std::chrono::system_clock::now();
            return now < ban_expire_time;
        }

        return true;
    }

    bool CanLogin() const {
        return IsValid() && !IsBanned();
    }
};

struct Session {
    uint64_t session_id = 0;
    uint64_t account_id = 0;
    std::string session_token;
    uint16_t server_id = 0;
    uint64_t character_id = 0;

    std::chrono::system_clock::time_point login_time;
    std::chrono::system_clock::time_point last_active_time;
    std::chrono::system_clock::time_point logout_time;

    uint8_t logout_reason = 0;

    bool IsValid() const {
        return session_id != 0 && logout_time.time_since_epoch().count() == 0;
    }

    bool IsTimeout(uint32_t timeout_seconds = 300) const {
        auto now = std::chrono::system_clock::now();
        auto last_active = last_active_time;

        if (last_active.time_since_epoch().count() == 0) {
            last_active = login_time;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_active);
        return elapsed.count() > timeout_seconds;
    }

    bool IsLoggedIn() const {
        return IsValid() && character_id != 0;
    }
};

enum class LoginResult : uint8_t {
    kSuccess = 0,
    kAccountNotFound = 1,
    kWrongPassword = 2,
    kAccountBanned = 3,
    kAlreadyLoggedIn = 4,
    kServerFull = 5,
    kSystemError = 6,
    kTooManyAttempts = 7,  // 速率限制：尝试次数过多
    kUnknown = 99
};

enum class LogoutResult : uint8_t {
    kSuccess = 0,
    kSessionNotFound = 1,
    kAlreadyLoggedOut = 2,
    kSystemError = 3,
    kUnknown = 99
};

enum class CharacterSelectResult : uint8_t {
    kSuccess = 0,
    kCharacterNotFound = 1,
    kNotOwned = 2,
    kCharacterDeleted = 3,
    kAlreadyLoggedIn = 4,
    kSystemError = 5,
    kUnknown = 99
};

} // namespace Game
} // namespace Murim
