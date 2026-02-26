#include "SignInHandler.hpp"
#include "core/network/ByteBuffer.hpp"
#include "core/network/PacketSerializer.hpp"
#include "core/spdlog_wrapper.hpp"
#include <google/protobuf/message.h>

namespace Murim {
namespace Game {

// ========== 消息处理器 ==========

void SignInHandler::HandleGetSignInInfoRequest(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    const murim::GetSignInInfoRequest& request) {

    spdlog::debug("[SignInHandler] Handling GetSignInInfoRequest for character {}", character_id);

    // 加载签到记录
    SignInManager::Instance().LoadSignInRecord(character_id);

    // 发送响应
    SendGetSignInInfoResponse(conn, character_id);
}

void SignInHandler::HandleDailySignInRequest(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    const murim::DailySignInRequest& request) {

    spdlog::info("[SignInHandler] Character {} requesting daily sign-in", character_id);

    // 每日签到
    uint32_t day, consecutive_days;
    std::vector<uint32_t> items, item_counts;
    uint32_t exp_gain, gold_gain;

    bool success = SignInManager::Instance().DailySignIn(
        character_id,
        day,
        consecutive_days,
        items,
        item_counts,
        exp_gain,
        gold_gain
    );

    if (!success) {
        spdlog::warn("[SignInHandler] Daily sign-in failed for character {}", character_id);
        // TODO: 发送错误响应
        return;
    }

    // 发送响应
    SendDailySignInResponse(conn, day, consecutive_days, items, item_counts, exp_gain, gold_gain);

    // 发送签到成功通知
    SendSignInSuccessNotification(conn, character_id, day, consecutive_days);

    // 检查是否有连续签到里程碑
    if (SignInManager::Instance().HasConsecutiveReward(character_id, consecutive_days)) {
        spdlog::info("[SignInHandler] Character {} reached consecutive milestone: {} days",
            character_id, consecutive_days);
        // TODO: 通知客户端有奖励可领取
    }
}

void SignInHandler::HandleReplenishSignInRequest(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    const murim::ReplenishSignInRequest& request) {

    spdlog::info("[SignInHandler] Character {} requesting replenish for day {}",
        character_id, request.day());

    // 补签
    uint32_t consecutive_days;
    bool success = SignInManager::Instance().ReplenishSignIn(
        character_id,
        request.day(),
        consecutive_days
    );

    // 获取剩余补签次数
    uint32_t remaining_count = SignInManager::Instance().GetRemainingReplenishCount(character_id);

    // 发送响应
    SendReplenishSignInResponse(conn, request.day(), consecutive_days, remaining_count, success);
}

void SignInHandler::HandleClaimConsecutiveRewardRequest(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    const murim::ClaimConsecutiveRewardRequest& request) {

    spdlog::info("[SignInHandler] Character {} claiming consecutive reward for {} days",
        character_id, request.consecutive_days());

    // 领取连续签到奖励
    std::vector<uint32_t> items, item_counts;
    bool success = SignInManager::Instance().ClaimConsecutiveReward(
        character_id,
        request.consecutive_days(),
        items,
        item_counts
    );

    // 发送响应
    SendClaimConsecutiveRewardResponse(conn, request.consecutive_days(), items, item_counts, success);

    if (success) {
        // 发送连续签到里程碑通知
        SendConsecutiveMilestoneNotification(conn, character_id, request.consecutive_days(), items, item_counts);
    }
}

void SignInHandler::HandleClaimMonthlyRewardRequest(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    const murim::ClaimMonthlyRewardRequest& request) {

    spdlog::info("[SignInHandler] Character {} claiming monthly reward", character_id);

    // 领取月度奖励
    std::vector<uint32_t> items, item_counts;
    bool success = SignInManager::Instance().ClaimMonthlyReward(
        character_id,
        items,
        item_counts
    );

    // 获取当前日期
    uint32_t year, month, day;
    SignInManager::Instance().GetCurrentDate(year, month, day);

    // 获取本月签到天数
    uint32_t sign_in_days = SignInManager::Instance().GetMonthSignInDays(character_id);

    // 发送响应
    SendClaimMonthlyRewardResponse(conn, month, sign_in_days, items, item_counts, success);
}

// ========== 辅助方法 ==========

void SignInHandler::SendGetSignInInfoResponse(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id) {

    murim::GetSignInInfoResponse response;
    response.mutable_response()->set_code(0);

    // 获取当前日期
    uint32_t year, month, day;
    SignInManager::Instance().GetCurrentDate(year, month, day);
    response.set_today(day);

    // 检查今天是否可以签到
    bool can_sign_in = SignInManager::Instance().CanSignInToday(character_id);
    response.set_can_sign_in_today(can_sign_in);

    // 获取签到记录
    const Murim::Game::SignInRecord* record = SignInManager::Instance().GetCurrentMonthRecord(character_id);
    if (record) {
        murim::SignInRecord* record_msg = response.mutable_record();
        record_msg->set_character_id(record->character_id);
        record_msg->set_year(record->year);
        record_msg->set_month(record->month);
        record_msg->set_total_sign_in_days(record->total_sign_in_days);
        record_msg->set_consecutive_days(record->consecutive_days);
        record_msg->set_last_sign_in_day(record->last_sign_in_day);

        for (uint32_t signed_day : record->signed_days) {
            record_msg->add_signed_days(signed_day);
        }

        record_msg->set_total_replenish_count(record->total_replenish_count);
        record_msg->set_month_replenish_count(record->month_replenish_count);
        record_msg->set_claimed_monthly_reward(record->claimed_monthly_reward);
    }

    // 补签消耗配置
    response.set_replenish_cost_item_id(10001);  // TODO: 从配置读取
    response.set_replenish_cost_count(1);

    // 发送响应
    SendResponse(conn, MSG_GET_SIGNIN_INFO_RESPONSE, response);

    spdlog::debug("[SignInHandler] Sent GetSignInInfoResponse for character {}", character_id);
}

void SignInHandler::SendDailySignInResponse(
    Core::Network::Connection::Ptr conn,
    uint32_t day,
    uint32_t consecutive_days,
    const std::vector<uint32_t>& reward_item_ids,
    const std::vector<uint32_t>& reward_counts,
    uint32_t exp_gain,
    uint32_t gold_gain) {

    murim::DailySignInResponse response;
    response.mutable_response()->set_code(0);
    response.set_success(true);
    response.set_day(day);
    response.set_consecutive_days(consecutive_days);

    for (uint32_t item_id : reward_item_ids) {
        response.add_reward_item_ids(item_id);
    }
    for (uint32_t count : reward_counts) {
        response.add_reward_counts(count);
    }
    response.set_exp_gain(exp_gain);
    response.set_gold_gain(gold_gain);

    // 检查是否有连续签到奖励可领取
    // TODO: 需要传递character_id参数
    response.set_has_consecutive_reward(false);

    // 发送响应
    SendResponse(conn, MSG_DAILY_SIGNIN_RESPONSE, response);

    spdlog::info("[SignInHandler] Sent DailySignInResponse: day {}, consecutive {}, exp {}, gold {}",
        day, consecutive_days, exp_gain, gold_gain);
}

void SignInHandler::SendReplenishSignInResponse(
    Core::Network::Connection::Ptr conn,
    uint32_t day,
    uint32_t consecutive_days,
    uint32_t remaining_replenish_count,
    bool success) {

    murim::ReplenishSignInResponse response;
    response.mutable_response()->set_code(success ? 0 : 1);
    response.set_success(success);
    response.set_day(day);
    response.set_consecutive_days(consecutive_days);
    response.set_remaining_replenish_count(remaining_replenish_count);

    // 发送响应
    SendResponse(conn, MSG_REPLENISH_SIGNIN_RESPONSE, response);

    spdlog::info("[SignInHandler] Sent ReplenishSignInResponse: day {}, consecutive {}, remaining {}, success: {}",
        day, consecutive_days, remaining_replenish_count, success);
}

void SignInHandler::SendClaimConsecutiveRewardResponse(
    Core::Network::Connection::Ptr conn,
    uint32_t consecutive_days,
    const std::vector<uint32_t>& reward_item_ids,
    const std::vector<uint32_t>& reward_counts,
    bool success) {

    murim::ClaimConsecutiveRewardResponse response;
    response.mutable_response()->set_code(success ? 0 : 1);
    response.set_success(success);
    response.set_consecutive_days(consecutive_days);

    for (uint32_t item_id : reward_item_ids) {
        response.add_reward_item_ids(item_id);
    }
    for (uint32_t count : reward_counts) {
        response.add_reward_counts(count);
    }

    // 发送响应
    SendResponse(conn, MSG_CLAIM_CONSECUTIVE_REWARD_RESPONSE, response);

    spdlog::info("[SignInHandler] Sent ClaimConsecutiveRewardResponse: {} days, {} items, success: {}",
        consecutive_days, reward_item_ids.size(), success);
}

void SignInHandler::SendClaimMonthlyRewardResponse(
    Core::Network::Connection::Ptr conn,
    uint32_t month,
    uint32_t sign_in_days,
    const std::vector<uint32_t>& reward_item_ids,
    const std::vector<uint32_t>& reward_counts,
    bool success) {

    murim::ClaimMonthlyRewardResponse response;
    response.mutable_response()->set_code(success ? 0 : 1);
    response.set_success(success);
    response.set_month(month);
    response.set_sign_in_days(sign_in_days);

    for (uint32_t item_id : reward_item_ids) {
        response.add_reward_item_ids(item_id);
    }
    for (uint32_t count : reward_counts) {
        response.add_reward_counts(count);
    }

    // 发送响应
    SendResponse(conn, MSG_CLAIM_MONTHLY_REWARD_RESPONSE, response);

    spdlog::info("[SignInHandler] Sent ClaimMonthlyRewardResponse: month {}, {} days, {} items, success: {}",
        month, sign_in_days, reward_item_ids.size(), success);
}

// ========== 通知 ==========

void SignInHandler::SendSignInSuccessNotification(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    uint32_t day,
    uint32_t consecutive_days) {

    murim::SignInSuccessNotification notification;
    notification.set_character_id(character_id);
    notification.set_day(day);
    notification.set_consecutive_days(consecutive_days);

    // 发送通知
    SendResponse(conn, MSG_SIGNIN_SUCCESS_NOTIFICATION, notification);

    spdlog::debug("[SignInHandler] Sent SignInSuccessNotification for character {}, day {}, consecutive {}",
        character_id, day, consecutive_days);
}

void SignInHandler::SendConsecutiveMilestoneNotification(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    uint32_t consecutive_days,
    const std::vector<uint32_t>& reward_item_ids,
    const std::vector<uint32_t>& reward_counts) {

    murim::ConsecutiveMilestoneNotification notification;
    notification.set_character_id(character_id);
    notification.set_consecutive_days(consecutive_days);

    for (uint32_t item_id : reward_item_ids) {
        notification.add_reward_item_ids(item_id);
    }
    for (uint32_t count : reward_counts) {
        notification.add_reward_counts(count);
    }

    // 发送通知
    SendResponse(conn, MSG_CONSECUTIVE_MILESTONE_NOTIFICATION, notification);

    spdlog::info("[SignInHandler] Sent ConsecutiveMilestoneNotification for character {}, {} days",
        character_id, consecutive_days);
}

// ========== 私有辅助方法 ==========

void SignInHandler::SendResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    uint16_t message_id,
    const google::protobuf::Message& response) {

    std::vector<uint8_t> data(response.ByteSizeLong());
    if (!response.SerializeToArray(data.data(), data.size())) {
        spdlog::error("[SignInHandler] Failed to serialize protobuf response: message_id=0x{:04X}", message_id);
        return;
    }

    auto buffer = Core::Network::PacketSerializer::Serialize(message_id, data);
    if (!buffer) {
        spdlog::error("[SignInHandler] Failed to serialize response: message_id=0x{:04X}", message_id);
        return;
    }

    conn->AsyncSend(buffer, [message_id](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("[SignInHandler] Failed to send response 0x{:04X}: {}", message_id, ec.message());
        } else {
            spdlog::debug("[SignInHandler] Response 0x{:04X} sent: {} bytes", message_id, bytes_sent);
        }
    });
}

} // namespace Game
} // namespace Murim
