#include "SignInManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include "core/database/DatabasePool.hpp"  // File is DatabasePool.hpp, class is ConnectionPool
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <libpq-fe.h>

namespace Murim {
namespace Game {

namespace {
    // 签到配置常量
    constexpr uint32_t SIGNIN_TICK_INTERVAL = 1000;       // 更新间隔（毫秒）
    constexpr uint32_t DAYS_IN_MONTH = 31;               // 一个月最多31天
    constexpr uint32_t MAX_CONSECUTIVE_DAYS = 365;       // 最大连续天数

    // 默认补签消耗
    constexpr uint32_t DEFAULT_REPLENISH_COST_ITEM_ID = 10001;  // 补签卡物品ID
    constexpr uint32_t DEFAULT_REPLENISH_COST_COUNT = 1;        // 补签消耗数量

    // VIP加成倍数（每级VIP额外10%）
    constexpr uint32_t VIP_BONUS_PER_LEVEL = 10;  // 10% per VIP level
}

// ========== 单例实现 ==========

SignInManager& SignInManager::Instance() {
    static SignInManager instance;
    return instance;
}

// ========== 初始化 ==========

bool SignInManager::Initialize(std::shared_ptr<Core::Database::ConnectionPool> db_pool) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_pool) {
        db_pool_ = db_pool;
        spdlog::info("[SignInManager] Database pool set");

        // 从数据库加载奖励配置
        if (!LoadRewardConfigsFromDB()) {
            spdlog::warn("[SignInManager] Failed to load reward configs from database, using defaults");

            // 使用默认配置
            for (uint32_t day = 1; day <= DAYS_IN_MONTH; ++day) {
                SignInRewardConfig config;
                config.day = day;

                murim::SignInRewardItem exp_reward;
                exp_reward.set_type(murim::SignInRewardType::SIGNIN_EXPERIENCE);
                exp_reward.set_id(0);
                exp_reward.set_count(100 * day);  // 每天递增
                exp_reward.set_vip_bonus(100);

                murim::SignInRewardItem gold_reward;
                gold_reward.set_type(murim::SignInRewardType::SIGNIN_GOLD);
                gold_reward.set_id(0);
                gold_reward.set_count(50 * day);  // 每天递增
                gold_reward.set_vip_bonus(100);

                config.rewards.push_back(exp_reward);
                config.rewards.push_back(gold_reward);

                daily_rewards_[day] = config;
            }

            // 连续签到奖励
            uint32_t consecutive_milestones[] = {7, 14, 21, 28};
            for (uint32_t days : consecutive_milestones) {
                ConsecutiveRewardConfig config;
                config.consecutive_days = days;
                config.claimed = false;

                murim::SignInRewardItem item_reward;
                item_reward.set_type(murim::SignInRewardType::SIGNIN_ITEM);
                item_reward.set_id(20000 + days);  // 示例物品ID
                item_reward.set_count(1);
                item_reward.set_vip_bonus(100);

                config.rewards.push_back(item_reward);
                consecutive_rewards_[days] = config;
            }
        }
    }

    tick_counter_ = 0;
    daily_reset_counter_ = 0;

    spdlog::info("[SignInManager] Initialized with {} daily rewards, {} consecutive rewards",
        daily_rewards_.size(), consecutive_rewards_.size());
    return true;
}

void SignInManager::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 保存所有签到记录
    // TODO: 批量保存到数据库

    spdlog::info("[SignInManager] Shutdown complete");
}

// ========== 签到记录管理 ==========

const SignInRecord* SignInManager::GetSignInRecord(
    uint64_t character_id,
    uint32_t year,
    uint32_t month) const {

    std::lock_guard<std::mutex> lock(mutex_);

    // 查找记录
    for (const auto& pair : records_) {
        const SignInRecord& record = pair.second;
        if (record.character_id == character_id && record.year == year && record.month == month) {
            return &record;
        }
    }
    return nullptr;
}

const SignInRecord* SignInManager::GetCurrentMonthRecord(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t year, month, day;
    GetCurrentDate(year, month, day);

    return GetSignInRecord(character_id, year, month);
}

bool SignInManager::LoadSignInRecord(uint64_t character_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!db_pool_) {
        spdlog::error("[SignInManager] Database pool not initialized");
        return false;
    }

    auto connection = db_pool_->GetConnection();
    if (!connection) {
        spdlog::error("[SignInManager] Failed to get database connection");
        return false;
    }

    PGconn* conn = connection->GetPGConn();

    uint32_t current_year, current_month, current_day;
    GetCurrentDate(current_year, current_month, current_day);

    // 构建查询
    char query[512];
    snprintf(query, sizeof(query),
        "SELECT character_id, year, month, total_sign_in_days, consecutive_days, "
        "last_sign_in_day, signed_days, total_replenish_count, month_replenish_count, "
        "claimed_monthly_reward FROM sign_in_records "
        "WHERE character_id = %lu AND year = %u AND month = %u",
        (unsigned long)character_id, current_year, current_month);

    PGresult* result = PQexec(conn, query);
    bool success = false;

    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
        SignInRecord record;
        record.character_id = character_id;
        record.year = current_year;
        record.month = current_month;
        record.total_sign_in_days = strtoul(PQgetvalue(result, 0, 3), nullptr, 10);
        record.consecutive_days = strtoul(PQgetvalue(result, 0, 4), nullptr, 10);
        record.last_sign_in_day = strtoul(PQgetvalue(result, 0, 5), nullptr, 10);

        // 解析已签到日期列表
        char* signed_days_str = PQgetvalue(result, 0, 6);
        std::istringstream iss(signed_days_str);
        std::string token;
        while (std::getline(iss, token, ',')) {
            if (!token.empty()) {
                record.signed_days.push_back(static_cast<uint32_t>(std::stoi(token)));
            }
        }

        record.total_replenish_count = strtoul(PQgetvalue(result, 0, 7), nullptr, 10);
        record.month_replenish_count = strtoul(PQgetvalue(result, 0, 8), nullptr, 10);
        record.claimed_monthly_reward = (strcmp(PQgetvalue(result, 0, 9), "t") == 0);

        records_[character_id] = record;
        success = true;
        spdlog::debug("[SignInManager] Loaded sign-in record for character {}", character_id);
    } else {
        // 创建新记录
        SignInRecord new_record = CreateSignInRecord(character_id, current_year, current_month);
        records_[character_id] = new_record;
        success = true;
    }

    PQclear(result);
    return success;
}

bool SignInManager::SaveSignInRecord(const SignInRecord& record) {
    if (!db_pool_) {
        spdlog::error("[SignInManager] Database pool not initialized");
        return false;
    }

    auto connection = db_pool_->GetConnection();
    if (!connection) {
        spdlog::error("[SignInManager] Failed to get database connection");
        return false;
    }

    PGconn* conn = connection->GetPGConn();

    // 序列化已签到日期列表
    std::ostringstream oss;
    for (size_t i = 0; i < record.signed_days.size(); ++i) {
        if (i > 0) oss << ",";
        oss << record.signed_days[i];
    }
    std::string signed_days_str = oss.str();

    // 构建INSERT查询（使用ON CONFLICT）
    char query[1024];
    snprintf(query, sizeof(query),
        "INSERT INTO sign_in_records "
        "(character_id, year, month, total_sign_in_days, consecutive_days, "
        "last_sign_in_day, signed_days, total_replenish_count, month_replenish_count, "
        "claimed_monthly_reward) "
        "VALUES (%lu, %u, %u, %u, %u, %u, '%s', %u, %u, %s) "
        "ON CONFLICT (character_id, year, month) "
        "DO UPDATE SET total_sign_in_days = %u, consecutive_days = %u, "
        "last_sign_in_day = %u, signed_days = '%s', total_replenish_count = %u, "
        "month_replenish_count = %u, claimed_monthly_reward = %s",
        (unsigned long)record.character_id, record.year, record.month,
        record.total_sign_in_days, record.consecutive_days, record.last_sign_in_day,
        signed_days_str.c_str(),
        record.total_replenish_count, record.month_replenish_count,
        record.claimed_monthly_reward ? "true" : "false",
        record.total_sign_in_days, record.consecutive_days, record.last_sign_in_day,
        signed_days_str.c_str(),
        record.total_replenish_count, record.month_replenish_count,
        record.claimed_monthly_reward ? "true" : "false"
    );

    PGresult* pg_result = PQexec(conn, query);
    bool success = (PQresultStatus(pg_result) == PGRES_COMMAND_OK);

    if (!success) {
        spdlog::error("[SignInManager] Failed to save sign-in record for character {}: {}",
            record.character_id, PQerrorMessage(conn));
    }

    PQclear(pg_result);
    return success;
}

// ========== 每日签到 ==========

bool SignInManager::CanSignInToday(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t current_year, current_month, current_day;
    GetCurrentDate(current_year, current_month, current_day);

    auto it = records_.find(character_id);
    if (it == records_.end()) {
        return true;  // 没有记录，可以签到
    }

    const SignInRecord& record = it->second;

    // 检查是否是本月记录
    if (record.year != current_year || record.month != current_month) {
        return true;  // 不是本月记录，可以签到
    }

    // 检查今天是否已签到
    for (uint32_t day : record.signed_days) {
        if (day == current_day) {
            return false;  // 今天已签到
        }
    }

    return true;
}

bool SignInManager::DailySignIn(
    uint64_t character_id,
    uint32_t& out_day,
    uint32_t& out_consecutive_days,
    std::vector<uint32_t>& out_items,
    std::vector<uint32_t>& out_item_counts,
    uint32_t& out_exp,
    uint32_t& out_gold) {

    std::lock_guard<std::mutex> lock(mutex_);

    if (!CanSignInToday(character_id)) {
        spdlog::warn("[SignInManager] Character {} cannot sign in today", character_id);
        return false;
    }

    uint32_t current_year, current_month, current_day;
    GetCurrentDate(current_year, current_month, current_day);

    // 获取或创建签到记录
    auto it = records_.find(character_id);
    SignInRecord* record = nullptr;

    if (it == records_.end() || it->second.year != current_year || it->second.month != current_month) {
        // 创建新记录
        SignInRecord new_record = CreateSignInRecord(character_id, current_year, current_month);
        records_[character_id] = new_record;
        record = &records_[character_id];
    } else {
        record = &it->second;
    }

    // 检查是否连续签到
    if (record->signed_days.empty() || IsConsecutiveDay(record->last_sign_in_day, current_day)) {
        record->consecutive_days++;
    } else {
        // 中断了，重新开始
        record->consecutive_days = 1;
    }

    // 更新签到记录
    record->signed_days.push_back(current_day);
    record->last_sign_in_day = current_day;
    record->total_sign_in_days++;

    out_day = current_day;
    out_consecutive_days = record->consecutive_days;

    // 分配签到奖励
    // TODO: 获取VIP等级
    uint32_t vip_level = 0;
    if (!DistributeSignInReward(character_id, current_day, vip_level, out_items, out_item_counts, out_exp, out_gold)) {
        spdlog::error("[SignInManager] Failed to distribute sign-in reward for character {}", character_id);
        return false;
    }

    // 保存签到记录
    SaveSignInRecord(*record);

    spdlog::info("[SignInManager] Character {} signed in on day {}, consecutive days: {}",
        character_id, current_day, record->consecutive_days);

    return true;
}

// ========== 补签功能 ==========

bool SignInManager::CanReplenish(uint64_t character_id, uint32_t day) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (day < 1 || day > DAYS_IN_MONTH) {
        return false;
    }

    uint32_t current_year, current_month, current_day;
    GetCurrentDate(current_year, current_month, current_day);

    // 不能补签今天或未来的日期
    if (day >= current_day) {
        return false;
    }

    auto it = records_.find(character_id);
    if (it == records_.end()) {
        return false;
    }

    const SignInRecord& record = it->second;

    // 检查是否是本月记录
    if (record.year != current_year || record.month != current_month) {
        return false;
    }

    // 检查是否已签到
    for (uint32_t signed_day : record.signed_days) {
        if (signed_day == day) {
            return false;  // 已签到
        }
    }

    // 检查补签次数
    if (record.month_replenish_count >= max_replenish_count_per_month_) {
        return false;
    }

    return true;
}

uint32_t SignInManager::GetRemainingReplenishCount(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = records_.find(character_id);
    if (it == records_.end()) {
        return max_replenish_count_per_month_;
    }

    const SignInRecord& record = it->second;

    uint32_t remaining = max_replenish_count_per_month_ - record.month_replenish_count;
    return (remaining > 0) ? remaining : 0;
}

bool SignInManager::ReplenishSignIn(
    uint64_t character_id,
    uint32_t day,
    uint32_t& out_consecutive_days) {

    std::lock_guard<std::mutex> lock(mutex_);

    if (!CanReplenish(character_id, day)) {
        spdlog::warn("[SignInManager] Character {} cannot replenish day {}", character_id, day);
        return false;
    }

    auto it = records_.find(character_id);
    if (it == records_.end()) {
        return false;
    }

    SignInRecord& record = it->second;

    // 检查消耗（TODO: 检查补签卡道具）
    uint32_t replenish_item_id = DEFAULT_REPLENISH_COST_ITEM_ID;
    uint32_t replenish_cost = DEFAULT_REPLENISH_COST_COUNT;

    // TODO: 扣除补签卡道具

    // 补签
    record.signed_days.push_back(day);
    record.month_replenish_count++;
    record.total_replenish_count++;

    // 重新计算连续天数（简化处理，不考虑补签对连续天数的影响）
    out_consecutive_days = record.consecutive_days;

    // 保存签到记录
    SaveSignInRecord(record);

    spdlog::info("[SignInManager] Character {} replenished sign-in for day {}", character_id, day);

    return true;
}

// ========== 奖励领取 ==========

bool SignInManager::HasConsecutiveReward(uint64_t character_id, uint32_t consecutive_days) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto reward_it = consecutive_rewards_.find(consecutive_days);
    if (reward_it == consecutive_rewards_.end()) {
        return false;
    }

    return !reward_it->second.claimed;
}

bool SignInManager::ClaimConsecutiveReward(
    uint64_t character_id,
    uint32_t consecutive_days,
    std::vector<uint32_t>& out_items,
    std::vector<uint32_t>& out_item_counts) {

    std::lock_guard<std::mutex> lock(mutex_);

    auto reward_it = consecutive_rewards_.find(consecutive_days);
    if (reward_it == consecutive_rewards_.end()) {
        spdlog::warn("[SignInManager] No consecutive reward for {} days", consecutive_days);
        return false;
    }

    ConsecutiveRewardConfig& reward = reward_it->second;

    if (reward.claimed) {
        spdlog::warn("[SignInManager] Consecutive reward for {} days already claimed", consecutive_days);
        return false;
    }

    // 分配奖励
    out_items.clear();
    out_item_counts.clear();

    for (const auto& reward_item : reward.rewards) {
        if (reward_item.type() == murim::SignInRewardType::SIGNIN_ITEM) {
            out_items.push_back(reward_item.id());
            out_item_counts.push_back(reward_item.count());
        }
        // TODO: 处理其他类型奖励
    }

    // 标记为已领取
    reward.claimed = true;

    // TODO: 保存到数据库

    spdlog::info("[SignInManager] Character {} claimed consecutive reward for {} days",
        character_id, consecutive_days);

    return true;
}

bool SignInManager::CanClaimMonthlyReward(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t current_year, current_month, current_day;
    GetCurrentDate(current_year, current_month, current_day);

    auto it = records_.find(character_id);
    if (it == records_.end()) {
        return false;
    }

    const SignInRecord& record = it->second;

    // 检查是否是本月记录
    if (record.year != current_year || record.month != current_month) {
        return false;
    }

    // 检查是否已领取
    if (record.claimed_monthly_reward) {
        return false;
    }

    // 检查签到天数（要求每月签到满20天）
    constexpr uint32_t MIN_MONTHLY_SIGNIN_DAYS = 20;
    if (record.signed_days.size() < MIN_MONTHLY_SIGNIN_DAYS) {
        return false;
    }

    return true;
}

bool SignInManager::ClaimMonthlyReward(
    uint64_t character_id,
    std::vector<uint32_t>& out_items,
    std::vector<uint32_t>& out_item_counts) {

    std::lock_guard<std::mutex> lock(mutex_);

    if (!CanClaimMonthlyReward(character_id)) {
        spdlog::warn("[SignInManager] Character {} cannot claim monthly reward", character_id);
        return false;
    }

    auto it = records_.find(character_id);
    if (it == records_.end()) {
        return false;
    }

    SignInRecord& record = it->second;

    // 分配月度奖励
    out_items.clear();
    out_item_counts.clear();

    // TODO: 从配置读取月度奖励
    // 示例：稀有物品
    out_items.push_back(30000);  // 月度奖励物品ID
    out_item_counts.push_back(1);

    // 标记为已领取
    record.claimed_monthly_reward = true;

    // 保存签到记录
    SaveSignInRecord(record);

    spdlog::info("[SignInManager] Character {} claimed monthly reward for {}/{}",
        character_id, record.year, record.month);

    return true;
}

// ========== 奖励配置 ==========

const SignInRewardConfig* SignInManager::GetDailyReward(uint32_t day) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = daily_rewards_.find(day);
    if (it != daily_rewards_.end()) {
        return &(it->second);
    }
    return nullptr;
}

const ConsecutiveRewardConfig* SignInManager::GetConsecutiveReward(uint32_t consecutive_days) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = consecutive_rewards_.find(consecutive_days);
    if (it != consecutive_rewards_.end()) {
        return &(it->second);
    }
    return nullptr;
}

bool SignInManager::AddDailyRewardConfig(const SignInRewardConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config.day < 1 || config.day > DAYS_IN_MONTH) {
        spdlog::error("[SignInManager] Invalid day: {}", config.day);
        return false;
    }

    daily_rewards_[config.day] = config;
    spdlog::info("[SignInManager] Added daily reward config for day {}", config.day);
    return true;
}

bool SignInManager::AddConsecutiveRewardConfig(const ConsecutiveRewardConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config.consecutive_days < 1 || config.consecutive_days > MAX_CONSECUTIVE_DAYS) {
        spdlog::error("[SignInManager] Invalid consecutive days: {}", config.consecutive_days);
        return false;
    }

    consecutive_rewards_[config.consecutive_days] = config;
    spdlog::info("[SignInManager] Added consecutive reward config for {} days", config.consecutive_days);
    return true;
}

// ========== 统计信息 ==========

uint32_t SignInManager::GetMonthSignInDays(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = records_.find(character_id);
    if (it == records_.end()) {
        return 0;
    }

    return static_cast<uint32_t>(it->second.signed_days.size());
}

uint32_t SignInManager::GetConsecutiveDays(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = records_.find(character_id);
    if (it == records_.end()) {
        return 0;
    }

    return it->second.consecutive_days;
}

uint32_t SignInManager::GetTotalSignInDays(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = records_.find(character_id);
    if (it == records_.end()) {
        return 0;
    }

    return it->second.total_sign_in_days;
}

// ========== 定时器 ==========

void SignInManager::Update(uint32_t delta_time) {
    std::lock_guard<std::mutex> lock(mutex_);

    tick_counter_ += delta_time;

    // 每秒更新一次
    if (tick_counter_ >= SIGNIN_TICK_INTERVAL) {
        tick_counter_ = 0;

        // 检查每日重置
        daily_reset_counter_++;
        if (daily_reset_counter_ >= 86400) {  // 24小时
            daily_reset_counter_ = 0;
            DailyReset();
        }
    }
}

void SignInManager::DailyReset() {
    spdlog::info("[SignInManager] Performing daily reset");

    // 每日重置通常不需要特殊操作
    // 签到状态由日期判断

    // TODO: 保存统计信息
}

void SignInManager::MonthlyReset() {
    spdlog::info("[SignInManager] Performing monthly reset");

    // 清理上个月的记录（可选）
    // records_.clear();
}

// ========== 数据库操作 ==========

bool SignInManager::LoadRewardConfigsFromDB() {
    if (!db_pool_) {
        spdlog::error("[SignInManager] Database pool not initialized");
        return false;
    }

    auto connection = db_pool_->GetConnection();
    if (!connection) {
        spdlog::error("[SignInManager] Failed to get database connection");
        return false;
    }

    PGconn* conn = connection->GetPGConn();

    // 加载每日奖励配置
    PGresult* daily_result = PQexec(conn, "SELECT * FROM sign_in_daily_rewards ORDER BY day");
    if (PQresultStatus(daily_result) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(daily_result); ++i) {
            SignInRewardConfig config;
            config.day = strtoul(PQgetvalue(daily_result, i, 0), nullptr, 10);

            // TODO: 解析reward_items JSON
            // 简化处理，实际应该使用JSON解析库

            daily_rewards_[config.day] = config;
        }
    }
    PQclear(daily_result);

    // 加载连续签到奖励配置
    PGresult* consecutive_result = PQexec(conn, "SELECT * FROM sign_in_consecutive_rewards ORDER BY consecutive_days");
    if (PQresultStatus(consecutive_result) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(consecutive_result); ++i) {
            ConsecutiveRewardConfig config;
            config.consecutive_days = strtoul(PQgetvalue(consecutive_result, i, 0), nullptr, 10);
            config.claimed = (strcmp(PQgetvalue(consecutive_result, i, 1), "t") == 0);

            // TODO: 解析reward_items JSON

            consecutive_rewards_[config.consecutive_days] = config;
        }
    }
    PQclear(consecutive_result);

    spdlog::info("[SignInManager] Loaded {} daily rewards, {} consecutive rewards from database",
        daily_rewards_.size(), consecutive_rewards_.size());
    return true;
}

bool SignInManager::SaveRewardConfigsToDB() {
    if (!db_pool_) {
        spdlog::error("[SignInManager] Database pool not initialized");
        return false;
    }

    auto connection = db_pool_->GetConnection();
    if (!connection) {
        spdlog::error("[SignInManager] Failed to get database connection");
        return false;
    }

    PGconn* conn = connection->GetPGConn();

    // TODO: 保存奖励配置到数据库

    spdlog::info("[SignInManager] Saved reward configs to database");
    return true;
}

// ========== 辅助方法 ==========

void SignInManager::GetCurrentDate(uint32_t& out_year, uint32_t& out_month, uint32_t& out_day) const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_s(&tm_now, &time_t_now);

    out_year = tm_now.tm_year + 1900;
    out_month = tm_now.tm_mon + 1;
    out_day = tm_now.tm_mday;
}

SignInRecord SignInManager::CreateSignInRecord(uint64_t character_id, uint32_t year, uint32_t month) {
    SignInRecord record;
    record.character_id = character_id;
    record.year = year;
    record.month = month;
    record.total_sign_in_days = 0;
    record.consecutive_days = 0;
    record.last_sign_in_day = 0;
    record.signed_days.clear();
    record.total_replenish_count = 0;
    record.month_replenish_count = 0;
    record.claimed_monthly_reward = false;

    return record;
}

bool SignInManager::IsConsecutiveDay(uint32_t last_sign_in_day, uint32_t current_day) const {
    if (last_sign_in_day == 0) {
        return false;
    }

    // 简化处理：相差1天即为连续
    // 实际应该考虑跨月、跨年的情况
    return (current_day == last_sign_in_day + 1);
}

uint32_t SignInManager::CalculateVIPBonus(uint32_t base_reward, uint32_t vip_level) const {
    uint32_t bonus_percent = vip_level * VIP_BONUS_PER_LEVEL;
    return base_reward + (base_reward * bonus_percent / 100);
}

bool SignInManager::DistributeSignInReward(
    uint64_t character_id,
    uint32_t day,
    uint32_t vip_level,
    std::vector<uint32_t>& out_items,
    std::vector<uint32_t>& out_item_counts,
    uint32_t& out_exp,
    uint32_t& out_gold) {

    out_items.clear();
    out_item_counts.clear();
    out_exp = 0;
    out_gold = 0;

    const SignInRewardConfig* config = GetDailyReward(day);
    if (!config) {
        spdlog::error("[SignInManager] No reward config for day {}", day);
        return false;
    }

    // 分配奖励
    for (const auto& reward_item : config->rewards) {
        uint32_t final_count = reward_item.count();

        // 应用VIP加成
        if (vip_level > 0) {
            final_count = CalculateVIPBonus(final_count, vip_level);
        }

        switch (reward_item.type()) {
            case murim::SignInRewardType::SIGNIN_EXPERIENCE:
                out_exp += final_count;
                break;

            case murim::SignInRewardType::SIGNIN_GOLD:
                out_gold += final_count;
                break;

            case murim::SignInRewardType::SIGNIN_ITEM:
                out_items.push_back(reward_item.id());
                out_item_counts.push_back(final_count);
                break;

            case murim::SignInRewardType::SIGNIN_DIAMOND:
                // TODO: 实现钻石奖励
                break;
        }
    }

    spdlog::debug("[SignInManager] Distributed sign-in reward for character {} day {}: {} exp, {} gold, {} items",
        character_id, day, out_exp, out_gold, out_items.size());

    return true;
}

} // namespace Game
} // namespace Murim
