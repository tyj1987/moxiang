// MurimServer - Time Utilities Implementation
// 时间工具类实现

#include "TimeUtils.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <ctime>
#include <cstdio>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace Murim {
namespace Utils {

// ========== 静态变量 ==========

static float g_delta_time_ms = 0.0f;
static uint64_t g_game_start_time_ms = 0;

// ========== 获取当前时间 ==========

uint64_t TimeUtils::GetCurrentTimeMs() {
    auto now = SystemClock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<Milliseconds>(duration).count();
}

uint64_t TimeUtils::GetCurrentTimeUs() {
    auto now = SystemClock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<Microseconds>(duration).count();
}

uint32_t TimeUtils::GetCurrentTimeS() {
    return static_cast<uint32_t>(GetCurrentTimeMs() / 1000);
}

uint64_t TimeUtils::GetMonotonicTimeMs() {
    auto now = SteadyClock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<Milliseconds>(duration).count();
}

uint64_t TimeUtils::GetMonotonicTimeUs() {
    auto now = SteadyClock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<Microseconds>(duration).count();
}

// ========== 时间转换 ==========

std::string TimeUtils::TimeToString(uint64_t timestamp_ms) {
    return TimeToString(timestamp_ms, "%Y-%m-%d %H:%M:%S");
}

std::string TimeUtils::TimeToString(uint64_t timestamp_ms, const char* format) {
    time_t timestamp_s = timestamp_ms / 1000;
    struct tm tm_time;

#ifdef _WIN32
    localtime_s(&tm_time, &timestamp_s);
#else
    localtime_r(&timestamp_s, &tm_time);
#endif

    char buffer[64];
    strftime(buffer, sizeof(buffer), format, &tm_time);
    return std::string(buffer);
}

void TimeUtils::TimeToTm(uint64_t timestamp_ms, struct tm* tm_time) {
    time_t timestamp_s = timestamp_ms / 1000;

#ifdef _WIN32
    localtime_s(tm_time, &timestamp_s);
#else
    localtime_r(&timestamp_s, tm_time);
#endif
}

uint64_t TimeUtils::TmToTime(const struct tm& tm_time) {
#ifdef _WIN32
    struct tm tm_copy = tm_time;
    time_t time_s = mktime(&tm_copy);
#else
    time_t time_s = mktime(const_cast<struct tm*>(&tm_time));
#endif
    return static_cast<uint64_t>(time_s) * 1000;
}

// ========== 时间间隔计算 ==========

int64_t TimeUtils::TimeDiffMs(uint64_t time_end_ms, uint64_t time_start_ms) {
    return static_cast<int64_t>(time_end_ms - time_start_ms);
}

int32_t TimeUtils::TimeDiffS(uint64_t time_end_ms, uint64_t time_start_ms) {
    return static_cast<int32_t>((time_end_ms - time_start_ms) / 1000);
}

bool TimeUtils::HasTimeArrived(uint64_t target_time_ms) {
    return GetCurrentTimeMs() >= target_time_ms;
}

bool TimeUtils::IsTimeOut(uint64_t start_time_ms, uint32_t timeout_ms) {
    uint64_t current_time_ms = GetCurrentTimeMs();
    return (current_time_ms - start_time_ms) >= timeout_ms;
}

// ========== 游戏时间相关 ==========

uint64_t TimeUtils::GetGameRunningTimeMs() {
    if (g_game_start_time_ms == 0) {
        g_game_start_time_ms = GetMonotonicTimeMs();
    }
    return GetMonotonicTimeMs() - g_game_start_time_ms;
}

float TimeUtils::GetDeltaTimeMs() {
    return g_delta_time_ms;
}

void TimeUtils::SetDeltaTimeMs(float delta_time) {
    g_delta_time_ms = delta_time;
}

// ========== 日期时间相关 ==========

std::string TimeUtils::GetCurrentDate() {
    return TimeToString(GetCurrentTimeMs(), "%Y-%m-%d");
}

std::string TimeUtils::GetCurrentClock() {
    return TimeToString(GetCurrentTimeMs(), "%H:%M:%S");
}

std::string TimeUtils::GetCurrentDateTime() {
    return TimeToString(GetCurrentTimeMs());
}

bool TimeUtils::IsSameDay(uint64_t timestamp1_ms, uint64_t timestamp2_ms) {
    struct tm tm1, tm2;
    TimeToTm(timestamp1_ms, &tm1);
    TimeToTm(timestamp2_ms, &tm2);

    return (tm1.tm_year == tm2.tm_year &&
            tm1.tm_mon == tm2.tm_mon &&
            tm1.tm_mday == tm2.tm_mday);
}

bool TimeUtils::IsSameWeek(uint64_t timestamp1_ms, uint64_t timestamp2_ms) {
    // 获取周一作为一周的开始
    struct tm tm1, tm2;
    TimeToTm(timestamp1_ms, &tm1);
    TimeToTm(timestamp2_ms, &tm2);

    // 获取本周的周一
    int day_of_week1 = (tm1.tm_wday == 0) ? 6 : (tm1.tm_wday - 1); // 周一=0, 周日=6
    int day_of_week2 = (tm2.tm_wday == 0) ? 6 : (tm2.tm_wday - 1);

    // 计算到周一的天数差
    tm1.tm_mday -= day_of_week1;
    tm2.tm_mday -= day_of_week2;

    // 归一化为周一00:00:00
    tm1.tm_hour = tm1.tm_min = tm1.tm_sec = 0;
    tm2.tm_hour = tm2.tm_min = tm2.tm_sec = 0;

    time_t time1 = std::mktime(&tm1);
    time_t time2 = std::mktime(&tm2);

    // 比较所在周
    return (std::difftime(time1, time2) < 7 * 24 * 3600);
}

bool TimeUtils::IsSameMonth(uint64_t timestamp1_ms, uint64_t timestamp2_ms) {
    struct tm tm1, tm2;
    TimeToTm(timestamp1_ms, &tm1);
    TimeToTm(timestamp2_ms, &tm2);

    return (tm1.tm_year == tm2.tm_year &&
            tm1.tm_mon == tm2.tm_mon);
}

uint64_t TimeUtils::GetTodayStartTime() {
    time_t now = time(nullptr);
    struct tm* tm_now = localtime(&now);
    struct tm tm_today = *tm_now;
    tm_today.tm_hour = 0;
    tm_today.tm_min = 0;
    tm_today.tm_sec = 0;

    time_t today_start = mktime(&tm_today);
    return static_cast<uint64_t>(today_start) * 1000;
}

uint64_t TimeUtils::GetWeekStartTime() {
    uint64_t today_start = GetTodayStartTime();

    // 获取今天是周几
    struct tm tm_now;
    TimeToTm(today_start, &tm_now);

    // 周一=0, 周日=6
    int day_of_week = (tm_now.tm_wday == 0) ? 6 : (tm_now.tm_wday - 1);

    // 回退到周一
    return today_start - (day_of_week * 24 * 3600 * 1000);
}

uint64_t TimeUtils::GetMonthStartTime() {
    time_t now = time(nullptr);
    struct tm* tm_now = localtime(&now);
    struct tm tm_month = *tm_now;
    tm_month.tm_mday = 1;
    tm_month.tm_hour = 0;
    tm_month.tm_min = 0;
    tm_month.tm_sec = 0;

    time_t month_start = mktime(&tm_month);
    return static_cast<uint64_t>(month_start) * 1000;
}

// ========== 时间格式化 ==========

std::string TimeUtils::FormatDuration(uint64_t duration_ms) {
    uint32_t total_seconds = static_cast<uint32_t>(duration_ms / 1000);

    uint32_t hours = total_seconds / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;
    uint32_t seconds = total_seconds % 60;

    char buffer[64];
    if (hours > 0) {
        snprintf(buffer, sizeof(buffer), "%uh %um %us", hours, minutes, seconds);
    } else if (minutes > 0) {
        snprintf(buffer, sizeof(buffer), "%um %us", minutes, seconds);
    } else {
        snprintf(buffer, sizeof(buffer), "%us", seconds);
    }

    return std::string(buffer);
}

std::string TimeUtils::FormatDurationSeconds(uint32_t duration_s) {
    return FormatDuration(static_cast<uint64_t>(duration_s) * 1000);
}

std::string TimeUtils::FormatCountdown(uint32_t duration_s) {
    uint32_t hours = duration_s / 3600;
    uint32_t minutes = (duration_s % 3600) / 60;
    uint32_t seconds = duration_s % 60;

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", hours, minutes, seconds);
    return std::string(buffer);
}

// ========== 性能计时器 ==========

TimeUtils::PerformanceTimer::PerformanceTimer()
    : start_time_(0), end_time_(0), is_running_(false) {
}

TimeUtils::PerformanceTimer::~PerformanceTimer() {
    if (is_running_) {
        Stop();
    }
}

void TimeUtils::PerformanceTimer::Start() {
    start_time_ = GetCurrentTimeUs();
    is_running_ = true;
}

void TimeUtils::PerformanceTimer::Stop() {
    if (is_running_) {
        end_time_ = GetCurrentTimeUs();
        is_running_ = false;
    }
}

void TimeUtils::PerformanceTimer::Reset() {
    start_time_ = 0;
    end_time_ = 0;
    is_running_ = false;
}

uint64_t TimeUtils::PerformanceTimer::GetElapsedTimeMs() {
    uint64_t end = is_running_ ? GetCurrentTimeUs() : end_time_;
    return (end - start_time_) / 1000;
}

uint64_t TimeUtils::PerformanceTimer::GetElapsedTimeUs() {
    uint64_t end = is_running_ ? GetCurrentTimeUs() : end_time_;
    return end - start_time_;
}

double TimeUtils::PerformanceTimer::GetElapsedTimeSeconds() {
    return GetElapsedTimeUs() / 1000000.0;
}

// ========== 日期时间验证 ==========

bool TimeUtils::IsLeapYear(uint32_t year) {
    if (year % 4 != 0) {
        return false;
    } else if (year % 100 != 0) {
        return true;
    } else {
        return (year % 400 == 0);
    }
}

uint32_t TimeUtils::GetDaysInMonth(uint32_t year, uint32_t month) {
    static const uint32_t days_in_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (month < 1 || month > 12) {
        return 0;
    }

    if (month == 2 && IsLeapYear(year)) {
        return 29;
    }

    return days_in_month[month - 1];
}

bool TimeUtils::IsValidDate(uint32_t year, uint32_t month, uint32_t day) {
    if (year < 1900 || year > 2100) {
        return false;
    }

    if (month < 1 || month > 12) {
        return false;
    }

    uint32_t max_days = GetDaysInMonth(year, month);
    if (day < 1 || day > max_days) {
        return false;
    }

    return true;
}

bool TimeUtils::IsValidTime(uint32_t hour, uint32_t minute, uint32_t second) {
    if (hour > 23) {
        return false;
    }

    if (minute > 59) {
        return false;
    }

    if (second > 59) {
        return false;
    }

    return true;
}

// ========== Helper Functions ==========

void SleepMs(uint32_t milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

void SleepUs(uint32_t microseconds) {
#ifdef _WIN32
    // Windows不支持微秒级sleep,使用毫级代替
    Sleep(microseconds / 1000);
#else
    usleep(microseconds);
#endif
}

} // namespace Utils
} // namespace Murim
