// MurimServer - Time Utilities
// 时间工具类 - 提供高精度时间获取和时间转换功能
// 对应legacy: 各种时间获取函数的统一封装

#pragma once

#include <cstdint>
#include <chrono>

namespace Murim {
namespace Utils {

// ========== Time Point Types ==========
// 使用std::chrono进行高精度时间测量

// 系统时钟(可调整,受系统时间影响)
using SystemClock = std::chrono::system_clock;

// 稳定时钟(单调递增,不受系统时间调整影响)
using SteadyClock = std::chrono::steady_clock;

// 高精度时钟(最高精度)
using HighResolutionClock = std::chrono::high_resolution_clock;

// ========== Duration Types ==========
// 时间段类型

// 秒
using Seconds = std::chrono::seconds;

// 毫秒
using Milliseconds = std::chrono::milliseconds;

// 微秒
using Microseconds = std::chrono::microseconds;

// 纳秒
using Nanoseconds = std::chrono::nanoseconds;

// ========== Time Utilities Class ==========
// 时间工具类

class TimeUtils {
public:
    // ========== 获取当前时间 ==========

    // 获取当前时间戳(毫秒) - 系统时间
    // 用于: 日志时间戳、数据库记录时间、显示给玩家的时间
    static uint64_t GetCurrentTimeMs();

    // 获取当前时间戳(微秒) - 系统时间
    // 用于: 高精度计时、性能测量
    static uint64_t GetCurrentTimeUs();

    // 获取当前时间戳(秒) - 系统时间
    // 用于: 简单的时间比较
    static uint32_t GetCurrentTimeS();

    // 获取单调时间戳(毫秒) - 不受系统时间调整影响
    // 用于: 技能冷却、Buff持续时间、游戏内计时
    static uint64_t GetMonotonicTimeMs();

    // 获取单调时间戳(微秒) - 不受系统时间调整影响
    // 用于: 高精度游戏内计时
    static uint64_t GetMonotonicTimeUs();

    // ========== 时间转换 ==========

    // 时间戳转换为可读字符串
    // 格式: "2026-01-20 12:34:56"
    static std::string TimeToString(uint64_t timestamp_ms);

    // 时间戳转换为可读字符串(自定义格式)
    // 格式说明符: %Y-年, %m-月, %d-日, %H-时, %M-分, %S-秒
    static std::string TimeToString(uint64_t timestamp_ms, const char* format);

    // 时间戳转换为结构化时间
    static void TimeToTm(uint64_t timestamp_ms, struct tm* tm_time);

    // 结构化时间转换为时间戳
    static uint64_t TmToTime(const struct tm& tm_time);

    // ========== 时间间隔计算 ==========

    // 计算两个时间戳之间的差值(毫秒)
    static int64_t TimeDiffMs(uint64_t time_end_ms, uint64_t time_start_ms);

    // 计算两个时间戳之间的差值(秒)
    static int32_t TimeDiffS(uint64_t time_end_ms, uint64_t time_start_ms);

    // 检查时间是否已到达
    static bool HasTimeArrived(uint64_t target_time_ms);

    // 检查时间是否已超时
    static bool IsTimeOut(uint64_t start_time_ms, uint32_t timeout_ms);

    // ========== 游戏时间相关 ==========

    // 获取游戏运行时间(毫秒)
    static uint64_t GetGameRunningTimeMs();

    // 获取帧时间(毫秒) - 上一帧到当前的时间
    static float GetDeltaTimeMs();

    // 设置帧时间(由引擎调用)
    static void SetDeltaTimeMs(float delta_time);

    // ========== 日期时间相关 ==========

    // 获取当前日期(年-月-日)
    static std::string GetCurrentDate();

    // 获取当前时间(时:分:秒)
    static std::string GetCurrentClock();

    // 获取当前日期时间(完整格式)
    static std::string GetCurrentDateTime();

    // 检查是否是同一天
    static bool IsSameDay(uint64_t timestamp1_ms, uint64_t timestamp2_ms);

    // 检查是否是同一周
    static bool IsSameWeek(uint64_t timestamp1_ms, uint64_t timestamp2_ms);

    // 检查是否是同一月
    static bool IsSameMonth(uint64_t timestamp1_ms, uint64_t timestamp2_ms);

    // 获取今天的开始时间戳(00:00:00)
    static uint64_t GetTodayStartTime();

    // 获取本周的开始时间戳(周一 00:00:00)
    static uint64_t GetWeekStartTime();

    // 获取本月的开始时间戳(1号 00:00:00)
    static uint64_t GetMonthStartTime();

    // ========== 时间格式化 ==========

    // 格式化持续时间(毫秒转可读格式)
    // 例如: 3661000 -> "1h 1m 1s"
    static std::string FormatDuration(uint64_t duration_ms);

    // 格式化持续时间(秒转可读格式)
    // 例如: 3661 -> "1h 1m 1s"
    static std::string FormatDurationSeconds(uint32_t duration_s);

    // 格式化倒计时
    // 例如: 3661 -> "01:01:01"
    static std::string FormatCountdown(uint32_t duration_s);

    // ========== 性能测量 ==========

    // 性能计时器 - 用于测量代码执行时间
    class PerformanceTimer {
    private:
        uint64_t start_time_;
        uint64_t end_time_;
        bool is_running_;

    public:
        PerformanceTimer();
        ~PerformanceTimer();

        // 开始计时
        void Start();

        // 停止计时
        void Stop();

        // 重置计时器
        void Reset();

        // 获取经过的时间(毫秒)
        uint64_t GetElapsedTimeMs();

        // 获取经过的时间(微秒)
        uint64_t GetElapsedTimeUs();

        // 获取经过的时间(秒,浮点数)
        double GetElapsedTimeSeconds();
    };

    // ========== 日期时间验证 ==========

    // 检查是否是闰年
    static bool IsLeapYear(uint32_t year);

    // 获取月份的天数
    static uint32_t GetDaysInMonth(uint32_t year, uint32_t month);

    // 检查日期是否有效
    static bool IsValidDate(uint32_t year, uint32_t month, uint32_t day);

    // 检查时间是否有效
    static bool IsValidTime(uint32_t hour, uint32_t minute, uint32_t second);
};

// ========== Helper Functions ==========

// 休眠指定毫秒数
void SleepMs(uint32_t milliseconds);

// 休眠指定微秒数
void SleepUs(uint32_t microseconds);

} // namespace Utils
} // namespace Murim
