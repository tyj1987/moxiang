#pragma once

#include <chrono>
#include <string>
#include <optional>

namespace Murim {
namespace Core {
namespace Database {

/**
 * @brief 数据库工具类
 *
 * 提供数据库操作的通用工具函数
 */
class DatabaseUtils {
public:
    /**
     * @brief 解析数据库时间戳字符串为time_point
     *
     * 支持多种时间戳格式:
     * - Unix时间戳 (秒): "1704067200"
     * - Unix时间戳 (毫秒): "1704067200000"
     * - PostgreSQL时间戳字符串: "2024-01-01 00:00:00"
     * - ISO 8601格式: "2024-01-01T00:00:00Z"
     *
     * @param timestamp_str 时间戳字符串
     * @return time_point时间点 (解析失败返回nullopt)
     */
    static std::optional<std::chrono::system_clock::time_point> ParseTimestamp(
        const std::string& timestamp_str
    );

    /**
     * @brief 将time_point格式化为数据库时间戳字符串
     *
     * @param time_point 时间点
     * @return 格式化的时间戳字符串
     */
    static std::string FormatTimestamp(
        const std::chrono::system_clock::time_point& time_point
    );

    /**
     * @brief 解析PostgreSQL TIMESTAMPTZ字段
     *
     * @param timestamp_str PostgreSQL时间戳字符串
     * @return time_point时间点
     */
    static std::chrono::system_clock::time_point ParsePostgresTimestamp(
        const std::string& timestamp_str
    );

    /**
     * @brief 获取当前时间戳字符串
     *
     * @return 当前时间的Unix时间戳(秒)
     */
    static std::string GetCurrentTimestamp();

private:
    /**
     * @brief 尝试解析为Unix时间戳(秒)
     */
    static std::optional<std::chrono::system_clock::time_point> TryParseUnixTimestamp(
        const std::string& timestamp_str
    );

    /**
     * @brief 尝试解析为PostgreSQL时间戳格式
     */
    static std::optional<std::chrono::system_clock::time_point> TryParsePostgresTimestamp(
        const std::string& timestamp_str
    );
};

} // namespace Database
} // namespace Core
} // namespace Murim
