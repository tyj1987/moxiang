#include "AchievementHandler.hpp"
#include "core/network/PacketSerializer.hpp"
#include "protocol/achievement.pb.h"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

void AchievementHandler::HandleGetAchievementListRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleGetAchievementListRequest: called");

    murim::GetAchievementListRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse GetAchievementListRequest");
        return;
    }

    spdlog::info("GetAchievementList: category={}, page={}",
                 static_cast<uint32_t>(request.category()), request.page());

    // 限制每页大小
    uint32_t page_size = request.page_size();
    if (page_size == 0 || page_size > 100) {
        page_size = 20;
    }

    // 获取成就列表
    std::vector<AchievementDefinition> achievements;
    if (request.category() == murim::AchievementCategory::COMBAT ||
        request.category() == murim::AchievementCategory::QUEST ||
        request.category() == murim::AchievementCategory::LEVEL ||
        request.category() == murim::AchievementCategory::SOCIAL ||
        request.category() == murim::AchievementCategory::ITEM ||
        request.category() == murim::AchievementCategory::DUNGEON ||
        request.category() == murim::AchievementCategory::WEALTH ||
        request.category() == murim::AchievementCategory::SPECIAL) {

        achievements = AchievementManager::Instance().GetAchievementsByCategory(
            request.category()
        );
    } else {
        // 获取所有成就
        // TODO: 实现分页逻辑
        achievements.clear(); // 暂时返回空列表
    }

    // 发送响应
    murim::GetAchievementListResponse response;
    response.mutable_response()->set_code(0);
    response.set_total_count(static_cast<uint32_t>(achievements.size()));
    response.set_page(request.page());

    uint32_t total_pages = (achievements.size() + page_size - 1) / page_size;
    response.set_total_pages(total_pages);

    for (const auto& definition : achievements) {
        auto* proto_def = response.add_achievements();
        *proto_def = definition.ToProto();
    }

    SendResponse(conn, 0x1102, response);
}

void AchievementHandler::HandleGetAchievementDetailRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleGetAchievementDetailRequest: called");

    murim::GetAchievementDetailRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse GetAchievementDetailRequest");
        return;
    }

    spdlog::info("GetAchievementDetail: achievement_id={}", request.achievement_id());

    // 获取成就定义
    auto* definition = AchievementManager::Instance().GetAchievementDefinition(
        request.achievement_id()
    );

    // TODO: 获取玩家进度
    uint64_t character_id = 0;  // TODO: 从连接中获取
    auto* progress = AchievementManager::Instance().GetAchievementProgress(
        character_id,
        request.achievement_id()
    );

    // 发送响应
    murim::GetAchievementDetailResponse response;
    response.mutable_response()->set_code(0);

    if (definition) {
        auto* proto_def = response.mutable_achievement();
        *proto_def = definition->ToProto();
    }

    if (progress) {
        auto* proto_progress = response.mutable_progress();
        *proto_progress = progress->ToProto();
    }

    SendResponse(conn, 0x1104, response);
}

void AchievementHandler::HandleGetMyAchievementRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleGetMyAchievementRequest: called");

    // TODO: 从连接中获取 character_id
    uint64_t character_id = 0;

    // 获取玩家成就数据
    auto* player_ach = AchievementManager::Instance().GetPlayerAchievement(character_id);

    // 发送响应
    murim::GetMyAchievementResponse response;
    response.mutable_response()->set_code(0);

    if (player_ach) {
        auto* proto_player_ach = response.mutable_player_achievement();
        proto_player_ach->set_character_id(player_ach->character_id);
        proto_player_ach->set_total_points(player_ach->total_points);
        proto_player_ach->set_completed_count(player_ach->completed_count);

        for (const auto& [id, prog] : player_ach->progress) {
            auto* proto_prog = proto_player_ach->add_progress();
            *proto_prog = prog.ToProto();
        }
    }

    SendResponse(conn, 0x1106, response);
}

void AchievementHandler::HandleClaimAchievementRewardRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleClaimAchievementRewardRequest: called");

    murim::ClaimAchievementRewardRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse ClaimAchievementRewardRequest");
        return;
    }

    spdlog::info("ClaimAchievementReward: achievement_id={}", request.achievement_id());

    // TODO: 从连接中获取 character_id
    uint64_t character_id = 0;

    // 领取奖励
    bool success = AchievementManager::Instance().ClaimReward(
        character_id,
        request.achievement_id()
    );

    // 发送响应
    murim::ClaimAchievementRewardResponse response;
    response.mutable_response()->set_code(success ? 0 : 1);
    response.set_achievement_id(request.achievement_id());

    if (!success) {
        response.mutable_response()->set_message("Failed to claim reward");
    }

    SendResponse(conn, 0x1108, response);
}

// ========== 辅助方法 ==========

void AchievementHandler::SendResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    uint16_t message_id,
    const google::protobuf::Message& response) {

    std::vector<uint8_t> data(response.ByteSizeLong());
    if (!response.SerializeToArray(data.data(), data.size())) {
        spdlog::error("Failed to serialize achievement protobuf response: message_id=0x{:04X}", message_id);
        return;
    }

    auto buffer = Core::Network::PacketSerializer::Serialize(message_id, data);
    if (!buffer) {
        spdlog::error("Failed to serialize achievement response: message_id=0x{:04X}", message_id);
        return;
    }

    conn->AsyncSend(buffer, [message_id](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send achievement response 0x{:04X}: {}", message_id, ec.message());
        } else {
            spdlog::debug("Achievement response 0x{:04X} sent: {} bytes", message_id, bytes_sent);
        }
    });
}

} // namespace Game
} // namespace Murim
