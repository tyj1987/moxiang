#include "VIPHandler.hpp"
#include "VIPManager.hpp"
#include "core/network/PacketSerializer.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

// ========== Helper Functions ==========

void VIPHandler::SendResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    uint16_t message_id,
    const google::protobuf::Message& response) {

    if (!conn) {
        spdlog::error("[VIPHandler] Connection is null");
        return;
    }

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    if (!response.SerializeToArray(data.data(), data.size())) {
        spdlog::error("[VIPHandler] Failed to serialize response: message_id={:#x}", message_id);
        return;
    }

    // 使用 PacketSerializer 创建数据包
    auto buffer = Core::Network::PacketSerializer::Serialize(message_id, data);
    if (!buffer) {
        spdlog::error("[VIPHandler] Failed to create packet: message_id={:#x}", message_id);
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [message_id](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("[VIPHandler] Failed to send response 0x{:04X}: {}", message_id, ec.message());
        } else {
            spdlog::debug("[VIPHandler] Sent response: message_id={:#x}, size={}", message_id, bytes_sent);
        }
    });
}

static uint64_t GetCharacterIdFromConnection(std::shared_ptr<Core::Network::Connection> conn) {
    // TODO: 从 Session 中获取 character_id
    // 暂时返回测试值
    return 1001;
}

// ========== Message Handlers ==========

void VIPHandler::HandleGetVIPInfoRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[VIPHandler] HandleGetVIPInfoRequest");

    // 解析请求
    murim::GetVIPInfoRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[VIPHandler] Failed to parse GetVIPInfoRequest");
        return;
    }

    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 获取VIP数据
    auto& vip_manager = VIPManager::Instance();
    auto* player_data = vip_manager.GetPlayerVIPData(character_id);
    auto* level_info = vip_manager.GetVIPLevelInfo(player_data->vip_level);

    // 获取下一等级信息
    uint32_t next_level = 0;
    uint64_t exp_to_next = 0;
    if (player_data->vip_level < 10) {
        next_level = player_data->vip_level + 1;
        auto* next_info = vip_manager.GetVIPLevelInfo(next_level);
        if (next_info) {
            exp_to_next = next_info->required_exp - player_data->current_exp;
        }
    }

    // 构建响应
    murim::GetVIPInfoResponse response;
    response.mutable_response()->set_code(0);
    response.mutable_response()->set_message("获取VIP信息成功");
    response.mutable_player_data()->CopyFrom(player_data->ToProto());
    if (level_info) {
        response.mutable_level_info()->CopyFrom(level_info->ToProto());
    }
    response.set_next_level(next_level);
    response.set_exp_to_next(exp_to_next);

    // 发送响应
    SendResponse(conn, 0x1502, response);
    spdlog::info("[VIPHandler] Sent VIP info: level={}, exp={}/{}",
                 player_data->vip_level, player_data->current_exp, exp_to_next);
}

void VIPHandler::HandleClaimVIPLevelGiftRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[VIPHandler] HandleClaimVIPLevelGiftRequest");

    // 解析请求
    murim::ClaimVIPLevelGiftRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[VIPHandler] Failed to parse ClaimVIPLevelGiftRequest");
        return;
    }

    uint32_t vip_level = request.vip_level();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 领取等级礼包
    auto& vip_manager = VIPManager::Instance();
    std::vector<uint32_t> item_ids, item_counts;
    bool success = vip_manager.ClaimLevelGift(character_id, vip_level, item_ids, item_counts);

    // 构建响应
    murim::ClaimVIPLevelGiftResponse response;
    if (success) {
        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("领取成功");
        response.set_vip_level(vip_level);

        for (uint32_t item_id : item_ids) {
            response.add_item_ids(item_id);
        }
        for (uint32_t count : item_counts) {
            response.add_item_counts(count);
        }

        spdlog::info("[VIPHandler] VIP level gift claimed: character_id={}, level={}",
                     character_id, vip_level);
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("领取失败：条件不满足或已领取");
        response.set_vip_level(0);

        spdlog::warn("[VIPHandler] Failed to claim level gift: character_id={}, level={}",
                     character_id, vip_level);
    }

    // 发送响应
    SendResponse(conn, 0x1504, response);
}

void VIPHandler::HandleClaimVIPDailyGiftRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[VIPHandler] HandleClaimVIPDailyGiftRequest");

    // 解析请求
    murim::ClaimVIPDailyGiftRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[VIPHandler] Failed to parse ClaimVIPDailyGiftRequest");
        return;
    }

    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 领取每日礼包
    auto& vip_manager = VIPManager::Instance();
    std::vector<uint32_t> item_ids, item_counts;
    bool success = vip_manager.ClaimDailyGift(character_id, item_ids, item_counts);

    // 计算连续领取天数
    auto* player_data = vip_manager.GetPlayerVIPData(character_id);
    uint32_t consecutive_days = 0;
    // TODO: 实现连续天数计算

    // 构建响应
    murim::ClaimVIPDailyGiftResponse response;
    if (success) {
        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("领取成功");

        for (uint32_t item_id : item_ids) {
            response.add_item_ids(item_id);
        }
        for (uint32_t count : item_counts) {
            response.add_item_counts(count);
        }
        response.set_consecutive_days(consecutive_days);

        spdlog::info("[VIPHandler] VIP daily gift claimed: character_id={}", character_id);
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("领取失败：今日已领取");

        spdlog::warn("[VIPHandler] Failed to claim daily gift: character_id={}", character_id);
    }

    // 发送响应
    SendResponse(conn, 0x1506, response);
}

void VIPHandler::HandleClaimVIPMonthlyGiftRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[VIPHandler] HandleClaimVIPMonthlyGiftRequest");

    // 解析请求
    murim::ClaimVIPMonthlyGiftRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[VIPHandler] Failed to parse ClaimVIPMonthlyGiftRequest");
        return;
    }

    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 领取每月礼包
    auto& vip_manager = VIPManager::Instance();
    std::vector<uint32_t> item_ids, item_counts;
    bool success = vip_manager.ClaimMonthlyGift(character_id, item_ids, item_counts);

    // 构建响应
    murim::ClaimVIPMonthlyGiftResponse response;
    if (success) {
        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("领取成功");

        for (uint32_t item_id : item_ids) {
            response.add_item_ids(item_id);
        }
        for (uint32_t count : item_counts) {
            response.add_item_counts(count);
        }

        spdlog::info("[VIPHandler] VIP monthly gift claimed: character_id={}", character_id);
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("领取失败：本月已领取");

        spdlog::warn("[VIPHandler] Failed to claim monthly gift: character_id={}", character_id);
    }

    // 发送响应
    SendResponse(conn, 0x1508, response);
}

void VIPHandler::HandleRechargeRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[VIPHandler] HandleRechargeRequest");

    // 解析请求
    murim::RechargeRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[VIPHandler] Failed to parse RechargeRequest");
        return;
    }

    uint32_t product_id = request.product_id();
    std::string order_id = request.order_id();
    uint32_t amount = request.amount();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 获取原VIP等级
    auto* player_data_before = VIPManager::Instance().GetPlayerVIPData(character_id);
    uint32_t old_level = player_data_before ? player_data_before->vip_level : 0;

    // 充值
    auto& vip_manager = VIPManager::Instance();
    bool success = vip_manager.Recharge(character_id, product_id, amount);

    // 重新获取VIP数据
    auto* player_data = vip_manager.GetPlayerVIPData(character_id);

    // 构建响应
    murim::RechargeResponse response;
    if (success && player_data) {
        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("充值成功");
        response.set_old_vip_level(old_level);
        response.set_new_vip_level(player_data->vip_level);

        // 计算增加的经验
        uint64_t added_exp = amount * vip_manager.GetExpPerYuan();
        response.set_added_exp(added_exp);
        response.set_current_exp(player_data->current_exp);
        response.set_leveled_up(player_data->vip_level > old_level);

        spdlog::info("[VIPHandler] Recharge successful: character_id={}, amount={}, exp_gain={}, level: {} -> {}",
                     character_id, amount, added_exp, old_level, player_data->vip_level);
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("充值失败");
        response.set_old_vip_level(0);
        response.set_new_vip_level(0);
        response.set_added_exp(0);
        response.set_current_exp(0);
        response.set_leveled_up(false);

        spdlog::warn("[VIPHandler] Failed to recharge: character_id={}, amount={}",
                     character_id, amount);
    }

    // 发送响应
    SendResponse(conn, 0x150A, response);
}

void VIPHandler::HandleGetVIPPrivilegesRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[VIPHandler] HandleGetVIPPrivilegesRequest");

    // 解析请求
    murim::GetVIPPrivilegesRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[VIPHandler] Failed to parse GetVIPPrivilegesRequest");
        return;
    }

    // 获取所有VIP等级信息
    auto& vip_manager = VIPManager::Instance();
    auto all_levels = vip_manager.GetAllVIPLevels();

    // 构建响应
    murim::GetVIPPrivilegesResponse response;
    response.mutable_response()->set_code(0);
    response.mutable_response()->set_message("获取VIP特权列表成功");

    for (const auto& level_info : all_levels) {
        auto* proto_info = response.add_level_infos();
        *proto_info = level_info.ToProto();
    }

    // 发送响应
    SendResponse(conn, 0x150C, response);
    spdlog::info("[VIPHandler] Sent VIP privileges list: count={}", all_levels.size());
}

} // namespace Game
} // namespace Murim
