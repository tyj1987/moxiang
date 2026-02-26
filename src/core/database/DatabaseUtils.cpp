#include "DatabaseUtils.hpp"
#include "../spdlog_wrapper.hpp"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <regex>

namespace Murim {
namespace Core {
namespace Database {

std::optional<std::chrono::system_clock::time_point> DatabaseUtils::ParseTimestamp(
    const std::string& timestamp_str
) {
    if (timestamp_str.empty()) {
        return std::nullopt;
    }

    // 尝试解析为Unix时间戳(秒或毫秒)
    auto unix_result = TryParseUnixTimestamp(timestamp_str);
    if (unix_result.has_value()) {
        return unix_result;
    }

    // 尝试解析为PostgreSQL时间戳格式
    auto postgres_result = TryParsePostgresTimestamp(timestamp_str);
    if (postgres_result.has_value()) {
        return postgres_result;
    }

    spdlog::warn("Failed to parse timestamp: {}", timestamp_str);
    return std::nullopt;
}

std::string DatabaseUtils::FormatTimestamp(
    const std::chrono::system_clock::time_point& time_point
) {
    // 转换为Unix时间戳(秒)
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        time_point.time_since_epoch()
    );

    return std::to_string(seconds.count());
}

std::chrono::system_clock::time_point DatabaseUtils::ParsePostgresTimestamp(
    const std::string& timestamp_str
) {
    // PostgreSQL时间戳格式: "2024-01-01 00:00:00" 或 "2024-01-01 00:00:00.123456"
    // 使用istringstream解析

    std::istringstream ss(timestamp_str);
    std::tm tm = {};
    char sep;

    // 解析日期部分: YYYY-MM-DD
    ss >> tm.tm_year >> sep >> tm.tm_mon >> sep >> tm.tm_mday;

    // 调整tm_year和tm_mon (tm_year是从1900年开始,tm_mon是0-11)
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;

    // 解析时间部分: HH:MM:SS
    ss >> sep; // 跳过空格
    ss >> tm.tm_hour >> sep >> tm.tm_min >> sep >> tm.tm_sec;

    // 转换为time_t
    std::time_t time_t_value = std::mktime(&tm);

    // 转换为time_point
    return std::chrono::system_clock::from_time_t(time_t_value);
}

std::string DatabaseUtils::GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    );
    return std::to_string(seconds.count());
}

std::optional<std::chrono::system_clock::time_point> DatabaseUtils::TryParseUnixTimestamp(
    const std::string& timestamp_str
) {
    try {
        // Unix时间戳(秒): 10位数字
        // Unix时间戳(毫秒): 13位数字
        // Legacy格式: 14位数字 (YYYYMMDDHHMMSS)

        if (timestamp_str.length() == 14) {
            // Legacy格式: YYYYMMDDHHMMSS
            // 对应 legacy: std::stoi(row["create_time"].substr(0, 14))
            // 但这个格式实际上是秒,需要转换
            std::tm tm = {};
            tm.tm_year = std::stoi(timestamp_str.substr(0, 4)) - 1900;
            tm.tm_mon = std::stoi(timestamp_str.substr(4, 2)) - 1;
            tm.tm_mday = std::stoi(timestamp_str.substr(6, 2));
            tm.tm_hour = std::stoi(timestamp_str.substr(8, 2));
            tm.tm_min = std::stoi(timestamp_str.substr(10, 2));
            tm.tm_sec = std::stoi(timestamp_str.substr(12, 2));

            std::time_t time_t_value = std::mktime(&tm);
            return std::chrono::system_clock::from_time_t(time_t_value);
        }

        // 检查是否为纯数字
        if (timestamp_str.find_first_not_of("0123456789") == std::string::npos) {
            int64_t timestamp = std::stoll(timestamp_str);

            // 判断是秒还是毫秒
            // 秒: 10位数字 (如1704067200 = 2024-01-01)
            // 毫秒: 13位数字 (如1704067200000)

            if (timestamp_str.length() >= 13) {
                // 毫秒时间戳
                std::chrono::milliseconds ms(timestamp);
                return std::chrono::system_clock::time_point(ms);
            } else {
                // 秒时间戳
                std::chrono::seconds sec(timestamp);
                return std::chrono::system_clock::time_point(sec);
            }
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        spdlog::debug("Failed to parse Unix timestamp '{}': {}", timestamp_str, e.what());
        return std::nullopt;
    }
}

std::optional<std::chrono::system_clock::time_point> DatabaseUtils::TryParsePostgresTimestamp(
    const std::string& timestamp_str
) {
    try {
        // PostgreSQL常见格式:
        // "2024-01-01 00:00:00"
        // "2024-01-01 00:00:00.123456"
        // "2024-01-01T00:00:00Z"
        // "2024-01-01T00:00:00.123456Z"

        // 简单的正则表达式匹配
        std::regex pattern(R"((\d{4})-(\d{2})-(\d{2})[T ](\d{2}):(\d{2}):(\d{2}))");
        std::smatch match;

        if (std::regex_search(timestamp_str, match, pattern)) {
            std::tm tm = {};
            tm.tm_year = std::stoi(match[1].str()) - 1900;
            tm.tm_mon = std::stoi(match[2].str()) - 1;
            tm.tm_mday = std::stoi(match[3].str());
            tm.tm_hour = std::stoi(match[4].str());
            tm.tm_min = std::stoi(match[5].str());
            tm.tm_sec = std::stoi(match[6].str());

            std::time_t time_t_value = std::mktime(&tm);
            return std::chrono::system_clock::from_time_t(time_t_value);
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        spdlog::debug("Failed to parse PostgreSQL timestamp '{}': {}", timestamp_str, e.what());
        return std::nullopt;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
