#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include "protocol/vip.pb.h"

namespace Murim {
namespace Game {

/**
 * @brief VIP等级信息
 */
struct VIPLevelInfo {
    uint32_t vip_level;
    uint64_t required_exp;
    std::vector<uint32_t> privileges;
    std::vector<uint32_t> gift_item_ids;
    std::vector<uint32_t> gift_item_counts;
    std::string description;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::VIPLevelInfo ToProto() const;
};

/**
 * @brief 玩家VIP数据
 */
struct VIPPlayerData {
    uint32_t vip_level;
    uint64_t current_exp;
    uint64_t total_exp;
    uint64_t monthly_recharge;
    uint64_t total_recharge;

    // 礼包领取状态
    std::vector<bool> level_gift_claimed;
    uint32_t last_daily_gift_day;
    uint32_t last_monthly_gift_month;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::VIPPlayerData ToProto() const;
};

/**
 * @brief VIP系统管理器
 *
 * 负责VIP等级、特权、礼包管理
 */
class VIPManager {
public:
    static VIPManager& Instance();

    bool Initialize();

    // ========== VIP等级管理 ==========

    /**
     * @brief 注册VIP等级
     */
    bool RegisterVIPLevel(const VIPLevelInfo& level_info);

    /**
     * @brief 获取VIP等级信息
     */
    VIPLevelInfo* GetVIPLevelInfo(uint32_t vip_level);

    /**
     * @brief 获取所有VIP等级信息
     */
    std::vector<VIPLevelInfo> GetAllVIPLevels();

    // ========== 玩家VIP数据管理 ==========

    /**
     * @brief 获取玩家VIP数据
     */
    VIPPlayerData* GetPlayerVIPData(uint64_t character_id);

    /**
     * @brief 增加VIP经验
     */
    bool AddExp(uint64_t character_id, uint64_t exp_amount);

    /**
     * @brief 充值（增加VIP经验）
     */
    bool Recharge(uint64_t character_id, uint32_t product_id, uint32_t amount);

    /**
     * @brief 获取每元兑换的VIP经验
     */
    uint64_t GetExpPerYuan() const { return exp_per_yuan_; }

    // ========== VIP特权 ==========

    /**
     * @brief 检查是否拥有特权
     */
    bool HasPrivilege(uint64_t character_id, murim::VIPPrivilegeType privilege);

    /**
     * @brief 获取玩家特权列表
     */
    std::vector<uint32_t> GetPrivileges(uint64_t character_id);

    // ========== VIP礼包 ==========

    /**
     * @brief 领取等级礼包
     */
    bool ClaimLevelGift(uint64_t character_id, uint32_t vip_level, std::vector<uint32_t>& item_ids, std::vector<uint32_t>& item_counts);

    /**
     * @brief 领取每日礼包
     */
    bool ClaimDailyGift(uint64_t character_id, std::vector<uint32_t>& item_ids, std::vector<uint32_t>& item_counts);

    /**
     * @brief 领取每月礼包
     */
    bool ClaimMonthlyGift(uint64_t character_id, std::vector<uint32_t>& item_ids, std::vector<uint32_t>& item_counts);

    // ========== 辅助方法 ==========

    /**
     * @brief 检查是否可以领取等级礼包
     */
    bool CanClaimLevelGift(uint64_t character_id, uint32_t vip_level);

    /**
     * @brief 检查是否可以领取每日礼包
     */
    bool CanClaimDailyGift(uint64_t character_id);

    /**
     * @brief 检查是否可以领取每月礼包
     */
    bool CanClaimMonthlyGift(uint64_t character_id);

    /**
     * @brief 发送VIP升级通知
     */
    void NotifyVIPLevelUp(uint64_t character_id, uint32_t old_level, uint32_t new_level);

private:
    VIPManager();
    ~VIPManager() = default;
    VIPManager(const VIPManager&) = delete;
    VIPManager& operator=(const VIPManager&) = delete;

    // ========== VIP等级信息存储 ==========
    // vip_level -> VIPLevelInfo
    std::unordered_map<uint32_t, VIPLevelInfo> vip_levels_;

    // ========== 玩家VIP数据 ==========
    // character_id -> VIPPlayerData
    std::unordered_map<uint64_t, VIPPlayerData> player_vip_data_;

    // ========== 配置参数 ==========
    uint32_t max_vip_level_;               // 最大VIP等级
    uint64_t exp_per_yuan_;                // 每元兑换的VIP经验
    uint32_t daily_gift_item_id_;          // 每日礼包物品ID
    uint32_t daily_gift_item_count_;       // 每日礼包物品数量
};

} // namespace Game
} // namespace Murim
