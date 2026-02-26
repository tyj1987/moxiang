#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <mutex>
#include "protocol/signin.pb.h"
#include "core/database/DatabasePool.hpp"

namespace Murim {
namespace Game {

/**
 * @brief 签到奖励配置
 * 每日签到奖励定义
 */
struct SignInRewardConfig {
    uint32_t day = 0;                        // 签到天数
    std::vector<murim::SignInRewardItem> rewards; // 奖励列表
};

/**
 * @brief 连续签到奖励配置
 * 连续签到里程碑奖励
 */
struct ConsecutiveRewardConfig {
    uint32_t consecutive_days = 0;          // 连续签到天数
    std::vector<murim::SignInRewardItem> rewards; // 奖励列表
    bool claimed = false;                   // 是否已领取
};

/**
 * @brief 签到记录
 * 玩家签到数据
 */
struct SignInRecord {
    uint64_t character_id = 0;              // 角色ID
    uint32_t year = 0;                      // 年份
    uint32_t month = 0;                     // 月份
    uint32_t total_sign_in_days = 0;        // 总签到天数
    uint32_t consecutive_days = 0;          // 连续签到天数
    uint32_t last_sign_in_day = 0;          // 上次签到日期（本月第几天）
    std::vector<uint32_t> signed_days;      // 已签到的日期列表（1-31）
    uint32_t total_replenish_count = 0;     // 总补签次数
    uint32_t month_replenish_count = 0;     // 本月补签次数
    bool claimed_monthly_reward = false;    // 是否已领取月度奖励
};

/**
 * @brief 签到管理器
 *
 * 负责签到系统的管理，包括：
 * - 每日签到
 * - 连续签到统计
 * - 补签功能
 * - 签到奖励领取
 * - 月度奖励
 * - 连续签到里程碑奖励
 *
 * 对应 legacy: SignInManager / DailySignIn
 */
class SignInManager {
public:
    /**
     * @brief 获取单例实例
     */
    static SignInManager& Instance();

    // ========== 初始化 ==========

    /**
     * @brief 初始化签到管理器
     * @param db_pool 数据库连接池
     * @return 是否初始化成功
     */
    bool Initialize(std::shared_ptr<Core::Database::ConnectionPool> db_pool);

    /**
     * @brief 关闭签到管理器
     */
    void Shutdown();

    // ========== 签到记录管理 ==========

    /**
     * @brief 获取玩家签到记录
     * @param character_id 角色ID
     * @param year 年份
     * @param month 月份
     * @return 签到记录（如果存在）
     */
    const SignInRecord* GetSignInRecord(
        uint64_t character_id,
        uint32_t year,
        uint32_t month
    ) const;

    /**
     * @brief 获取当前月份签到记录
     * @param character_id 角色ID
     * @return 签到记录（如果存在）
     */
    const SignInRecord* GetCurrentMonthRecord(uint64_t character_id) const;

    /**
     * @brief 加载玩家签到记录
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool LoadSignInRecord(uint64_t character_id);

    /**
     * @brief 保存玩家签到记录
     * @param record 签到记录
     * @return 是否成功
     */
    bool SaveSignInRecord(const SignInRecord& record);

    // ========== 每日签到 ==========

    /**
     * @brief 检查今天是否可以签到
     * @param character_id 角色ID
     * @return 是否可以签到
     */
    bool CanSignInToday(uint64_t character_id) const;

    /**
     * @brief 每日签到
     * @param character_id 角色ID
     * @param out_day 输出：签到日期
     * @param out_consecutive_days 输出：连续签到天数
     * @param out_items 输出：奖励物品ID列表
     * @param out_item_counts 输出：物品数量列表
     * @param out_exp 输出：经验奖励
     * @param out_gold 输出：金币奖励
     * @return 是否成功
     */
    bool DailySignIn(
        uint64_t character_id,
        uint32_t& out_day,
        uint32_t& out_consecutive_days,
        std::vector<uint32_t>& out_items,
        std::vector<uint32_t>& out_item_counts,
        uint32_t& out_exp,
        uint32_t& out_gold
    );

    // ========== 补签功能 ==========

    /**
     * @brief 检查是否可以补签
     * @param character_id 角色ID
     * @param day 补签日期
     * @return 是否可以补签
     */
    bool CanReplenish(uint64_t character_id, uint32_t day) const;

    /**
     * @brief 获取剩余补签次数
     * @param character_id 角色ID
     * @return 剩余补签次数
     */
    uint32_t GetRemainingReplenishCount(uint64_t character_id) const;

    /**
     * @brief 补签
     * @param character_id 角色ID
     * @param day 补签日期
     * @param out_consecutive_days 输出：连续签到天数
     * @return 是否成功
     */
    bool ReplenishSignIn(
        uint64_t character_id,
        uint32_t day,
        uint32_t& out_consecutive_days
    );

    // ========== 奖励领取 ==========

    /**
     * @brief 检查是否有连续签到奖励可领取
     * @param character_id 角色ID
     * @param consecutive_days 连续签到天数
     * @return 是否可领取
     */
    bool HasConsecutiveReward(uint64_t character_id, uint32_t consecutive_days) const;

    /**
     * @brief 领取连续签到奖励
     * @param character_id 角色ID
     * @param consecutive_days 连续签到天数
     * @param out_items 输出：奖励物品ID列表
     * @param out_item_counts 输出：物品数量列表
     * @return 是否成功
     */
    bool ClaimConsecutiveReward(
        uint64_t character_id,
        uint32_t consecutive_days,
        std::vector<uint32_t>& out_items,
        std::vector<uint32_t>& out_item_counts
    );

    /**
     * @brief 检查是否可以领取月度奖励
     * @param character_id 角色ID
     * @return 是否可以领取
     */
    bool CanClaimMonthlyReward(uint64_t character_id) const;

    /**
     * @brief 领取月度奖励
     * @param character_id 角色ID
     * @param out_items 输出：奖励物品ID列表
     * @param out_item_counts 输出：物品数量列表
     * @return 是否成功
     */
    bool ClaimMonthlyReward(
        uint64_t character_id,
        std::vector<uint32_t>& out_items,
        std::vector<uint32_t>& out_item_counts
    );

    // ========== 奖励配置 ==========

    /**
     * @brief 获取每日签到奖励
     * @param day 签到天数
     * @return 签到奖励配置（如果存在）
     */
    const SignInRewardConfig* GetDailyReward(uint32_t day) const;

    /**
     * @brief 获取连续签到奖励
     * @param consecutive_days 连续签到天数
     * @return 连续签到奖励配置（如果存在）
     */
    const ConsecutiveRewardConfig* GetConsecutiveReward(uint32_t consecutive_days) const;

    /**
     * @brief 添加每日签到奖励配置
     * @param config 奖励配置
     * @return 是否成功
     */
    bool AddDailyRewardConfig(const SignInRewardConfig& config);

    /**
     * @brief 添加连续签到奖励配置
     * @param config 奖励配置
     * @return 是否成功
     */
    bool AddConsecutiveRewardConfig(const ConsecutiveRewardConfig& config);

    // ========== 统计信息 ==========

    /**
     * @brief 获取本月签到天数
     * @param character_id 角色ID
     * @return 本月签到天数
     */
    uint32_t GetMonthSignInDays(uint64_t character_id) const;

    /**
     * @brief 获取连续签到天数
     * @param character_id 角色ID
     * @return 连续签到天数
     */
    uint32_t GetConsecutiveDays(uint64_t character_id) const;

    /**
     * @brief 获取总签到天数
     * @param character_id 角色ID
     * @return 总签到天数
     */
    uint32_t GetTotalSignInDays(uint64_t character_id) const;

    // ========== 定时器 ==========

    /**
     * @brief 更新签到系统（定时器）
     * @param delta_time 时间增量（毫秒）
     */
    void Update(uint32_t delta_time);

    /**
     * @brief 每日重置（凌晨0点）
     */
    void DailyReset();

    /**
     * @brief 每月重置（每月1号）
     */
    void MonthlyReset();

    // ========== 公共辅助方法 ==========

    /**
     * @brief 获取当前日期
     * @param out_year 输出：年份
     * @param out_month 输出：月份
     * @param out_day 输出：日期
     */
    void GetCurrentDate(uint32_t& out_year, uint32_t& out_month, uint32_t& out_day) const;

    // ========== 数据库操作 ==========

    /**
     * @brief 从数据库加载奖励配置
     * @return 是否成功
     */
    bool LoadRewardConfigsFromDB();

    /**
     * @brief 保存奖励配置到数据库
     * @return 是否成功
     */
    bool SaveRewardConfigsToDB();

private:
    SignInManager() = default;
    ~SignInManager() = default;
    SignInManager(const SignInManager&) = delete;
    SignInManager& operator=(const SignInManager&) = delete;

    // ========== 数据成员 ==========

    mutable std::mutex mutex_;                                      // 互斥锁

    std::shared_ptr<Core::Database::ConnectionPool> db_pool_;        // 数据库连接池

    std::unordered_map<uint64_t, SignInRecord> records_;           // 签到记录 (character_id -> record)
    std::unordered_map<uint32_t, SignInRewardConfig> daily_rewards_; // 每日奖励 (day -> config)
    std::unordered_map<uint32_t, ConsecutiveRewardConfig> consecutive_rewards_; // 连续奖励 (consecutive_days -> config)

    uint32_t max_replenish_count_per_month_ = 3;  // 每月最大补签次数
    uint32_t tick_counter_ = 0;                    // tick计数器
    uint32_t daily_reset_counter_ = 0;             // 每日重置计数器

    // ========== 私有辅助方法 ==========

    /**
     * @brief 创建签到记录
     * @param character_id 角色ID
     * @param year 年份
     * @param month 月份
     * @return 签到记录
     */
    SignInRecord CreateSignInRecord(uint64_t character_id, uint32_t year, uint32_t month);

    /**
     * @brief 检查是否同一天
     * @param last_sign_in_day 上次签到日期
     * @param current_day 当前日期
     * @return 是否连续（相差1天）
     */
    bool IsConsecutiveDay(uint32_t last_sign_in_day, uint32_t current_day) const;

    /**
     * @brief 计算VIP加成奖励
     * @param base_reward 基础奖励
     * @param vip_level VIP等级
     * @return 加成后的奖励
     */
    uint32_t CalculateVIPBonus(uint32_t base_reward, uint32_t vip_level) const;

    /**
     * @brief 分配签到奖励
     * @param character_id 角色ID
     * @param day 签到天数
     * @param vip_level VIP等级
     * @param out_items 输出：物品ID列表
     * @param out_item_counts 输出：物品数量列表
     * @param out_exp 输出：经验
     * @param out_gold 输出：金币
     * @return 是否成功
     */
    bool DistributeSignInReward(
        uint64_t character_id,
        uint32_t day,
        uint32_t vip_level,
        std::vector<uint32_t>& out_items,
        std::vector<uint32_t>& out_item_counts,
        uint32_t& out_exp,
        uint32_t& out_gold
    );
};

} // namespace Game
} // namespace Murim
