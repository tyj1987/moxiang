#pragma once

#include <memory>
#include "core/network/Connection.hpp"
#include "protocol/signin.pb.h"
#include "SignInManager.hpp"

namespace Murim {
namespace Game {

/**
 * @brief 签到系统消息处理器
 *
 * 负责处理签到相关的客户端请求，包括：
 * - 获取签到信息
 * - 每日签到
 * - 补签
 * - 领取连续签到奖励
 * - 领取月度奖励
 *
 * 消息ID范围：0x1701 - 0x1708
 */
class SignInHandler {
public:
    // ========== 消息处理器 ==========

    /**
     * @brief 处理获取签到信息请求
     * @param conn 客户端连接
     * @param character_id 角色ID
     * @param request 请求数据
     */
    static void HandleGetSignInInfoRequest(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        const murim::GetSignInInfoRequest& request
    );

    /**
     * @brief 处理每日签到请求
     * @param conn 客户端连接
     * @param character_id 角色ID
     * @param request 请求数据
     */
    static void HandleDailySignInRequest(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        const murim::DailySignInRequest& request
    );

    /**
     * @brief 处理补签请求
     * @param conn 客户端连接
     * @param character_id 角色ID
     * @param request 请求数据
     */
    static void HandleReplenishSignInRequest(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        const murim::ReplenishSignInRequest& request
    );

    /**
     * @brief 处理领取连续签到奖励请求
     * @param conn 客户端连接
     * @param character_id 角色ID
     * @param request 请求数据
     */
    static void HandleClaimConsecutiveRewardRequest(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        const murim::ClaimConsecutiveRewardRequest& request
    );

    /**
     * @brief 处理领取月度奖励请求
     * @param conn 客户端连接
     * @param character_id 角色ID
     * @param request 请求数据
     */
    static void HandleClaimMonthlyRewardRequest(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        const murim::ClaimMonthlyRewardRequest& request
    );

    // ========== 辅助方法 ==========

    /**
     * @brief 发送获取签到信息响应
     * @param conn 客户端连接
     * @param character_id 角色ID
     */
    static void SendGetSignInInfoResponse(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id
    );

    /**
     * @brief 发送每日签到响应
     * @param conn 客户端连接
     * @param day 签到日期
     * @param consecutive_days 连续签到天数
     * @param reward_item_ids 奖励物品ID列表
     * @param reward_counts 物品数量列表
     * @param exp_gain 经验奖励
     * @param gold_gain 金币奖励
     */
    static void SendDailySignInResponse(
        Core::Network::Connection::Ptr conn,
        uint32_t day,
        uint32_t consecutive_days,
        const std::vector<uint32_t>& reward_item_ids,
        const std::vector<uint32_t>& reward_counts,
        uint32_t exp_gain,
        uint32_t gold_gain
    );

    /**
     * @brief 发送补签响应
     * @param conn 客户端连接
     * @param day 补签日期
     * @param consecutive_days 连续签到天数
     * @param remaining_replenish_count 剩余补签次数
     * @param success 是否成功
     */
    static void SendReplenishSignInResponse(
        Core::Network::Connection::Ptr conn,
        uint32_t day,
        uint32_t consecutive_days,
        uint32_t remaining_replenish_count,
        bool success
    );

    /**
     * @brief 发送领取连续签到奖励响应
     * @param conn 客户端连接
     * @param consecutive_days 连续签到天数
     * @param reward_item_ids 奖励物品ID列表
     * @param reward_counts 物品数量列表
     * @param success 是否成功
     */
    static void SendClaimConsecutiveRewardResponse(
        Core::Network::Connection::Ptr conn,
        uint32_t consecutive_days,
        const std::vector<uint32_t>& reward_item_ids,
        const std::vector<uint32_t>& reward_counts,
        bool success
    );

    /**
     * @brief 发送领取月度奖励响应
     * @param conn 客户端连接
     * @param month 月份
     * @param sign_in_days 本月签到天数
     * @param reward_item_ids 奖励物品ID列表
     * @param reward_counts 物品数量列表
     * @param success 是否成功
     */
    static void SendClaimMonthlyRewardResponse(
        Core::Network::Connection::Ptr conn,
        uint32_t month,
        uint32_t sign_in_days,
        const std::vector<uint32_t>& reward_item_ids,
        const std::vector<uint32_t>& reward_counts,
        bool success
    );

    // ========== 通知 ==========

    /**
     * @brief 发送签到成功通知
     * @param conn 客户端连接
     * @param character_id 角色ID
     * @param day 签到日期
     * @param consecutive_days 连续签到天数
     */
    static void SendSignInSuccessNotification(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        uint32_t day,
        uint32_t consecutive_days
    );

    /**
     * @brief 发送连续签到里程碑通知
     * @param conn 客户端连接
     * @param character_id 角色ID
     * @param consecutive_days 连续签到天数
     * @param reward_item_ids 奖励物品ID列表
     * @param reward_counts 物品数量列表
     */
    static void SendConsecutiveMilestoneNotification(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        uint32_t consecutive_days,
        const std::vector<uint32_t>& reward_item_ids,
        const std::vector<uint32_t>& reward_counts
    );

private:
    // ========== 消息ID定义 ==========

    // 签到系统消息ID范围：0x1701 - 0x1708
    static constexpr uint16_t MSG_GET_SIGNIN_INFO_REQUEST = 0x1701;
    static constexpr uint16_t MSG_GET_SIGNIN_INFO_RESPONSE = 0x1702;
    static constexpr uint16_t MSG_DAILY_SIGNIN_REQUEST = 0x1703;
    static constexpr uint16_t MSG_DAILY_SIGNIN_RESPONSE = 0x1704;
    static constexpr uint16_t MSG_REPLENISH_SIGNIN_REQUEST = 0x1705;
    static constexpr uint16_t MSG_REPLENISH_SIGNIN_RESPONSE = 0x1706;
    static constexpr uint16_t MSG_CLAIM_CONSECUTIVE_REWARD_REQUEST = 0x1707;
    static constexpr uint16_t MSG_CLAIM_CONSECUTIVE_REWARD_RESPONSE = 0x1708;
    static constexpr uint16_t MSG_CLAIM_MONTHLY_REWARD_REQUEST = 0x1709;
    static constexpr uint16_t MSG_CLAIM_MONTHLY_REWARD_RESPONSE = 0x170A;

    // 通知消息ID
    static constexpr uint16_t MSG_SIGNIN_SUCCESS_NOTIFICATION = 0x170B;
    static constexpr uint16_t MSG_CONSECUTIVE_MILESTONE_NOTIFICATION = 0x170C;

    // ========== 私有辅助方法 ==========

    /**
     * @brief 发送响应消息
     * @param conn 客户端连接
     * @param message_id 消息ID
     * @param response Protobuf响应消息
     */
    static void SendResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        uint16_t message_id,
        const google::protobuf::Message& response);
};

} // namespace Game
} // namespace Murim
