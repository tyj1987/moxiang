#include "LeaderboardHandler.hpp"
#include "core/network/PacketSerializer.hpp"
#include "protocol/leaderboard.pb.h"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

void LeaderboardHandler::HandleGetLeaderboardRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleGetLeaderboardRequest: called");

    murim::GetLeaderboardRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse GetLeaderboardRequest");
        return;
    }

    spdlog::info("GetLeaderboard: type={}, page={}, page_size={}",
                 static_cast<uint32_t>(request.type()), request.page(), request.page_size());

    // 限制每页大小
    uint32_t page_size = request.page_size();
    if (page_size == 0 || page_size > 100) {
        page_size = 20;  // 默认20条
    }

    // 获取排行榜数据
    std::vector<LeaderboardEntry> entries;
    uint32_t total_entries = 0;

    if (request.type() == murim::LeaderboardType::LEADERBOARD_GUILD) {
        // 帮派排行榜
        auto guild_entries = LeaderboardManager::Instance().GetGuildLeaderboard(
            request.page(),
            page_size
        );

        // 发送响应
        murim::GetLeaderboardResponse response;
        response.mutable_response()->set_code(0);
        response.set_type(request.type());
        response.set_total_entries(static_cast<uint32_t>(guild_entries.size()));
        response.set_page(request.page());

        // 计算总页数
        uint32_t total_pages = (guild_entries.size() + page_size - 1) / page_size;
        response.set_total_pages(total_pages);

        for (const auto& entry : guild_entries) {
            auto* proto_entry = response.add_guild_entries();
            *proto_entry = entry.ToProto();
        }

        SendResponse(conn, 0x1002, response);
        return;
    }

    if (request.type() == murim::LeaderboardType::LEADERBOARD_DUNGEON) {
        // 副本排行榜（需要 dungeon_id）
        // TODO: 从请求中获取 dungeon_id
        // 暂时返回空列表

        murim::GetLeaderboardResponse response;
        response.mutable_response()->set_code(0);
        response.set_type(request.type());
        response.set_total_entries(0);
        response.set_page(request.page());
        response.set_total_pages(0);

        SendResponse(conn, 0x1002, response);
        return;
    }

    // 角色排行榜（等级、财富、战力等）
    entries = LeaderboardManager::Instance().GetLeaderboard(
        request.type(),
        request.page(),
        page_size
    );

    total_entries = LeaderboardManager::Instance().GetTotalEntries(request.type());

    // 发送响应
    murim::GetLeaderboardResponse response;
    response.mutable_response()->set_code(0);
    response.set_type(request.type());
    response.set_total_entries(total_entries);
    response.set_page(request.page());

    // 计算总页数
    uint32_t total_pages = (total_entries + page_size - 1) / page_size;
    response.set_total_pages(total_pages);

    for (const auto& entry : entries) {
        auto* proto_entry = response.add_character_entries();
        *proto_entry = entry.ToProto();
    }

    SendResponse(conn, 0x1002, response);
}

void LeaderboardHandler::HandleGetMyRankRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleGetMyRankRequest: called");

    murim::GetMyRankRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse GetMyRankRequest");
        return;
    }

    spdlog::info("GetMyRank: type={}", static_cast<uint32_t>(request.type()));

    // TODO: 从连接中获取 character_id
    // 暂时使用示例ID
    uint64_t character_id = 0;

    uint32_t rank = LeaderboardManager::Instance().GetMyRank(character_id, request.type());

    // 获取条目详情
    auto* entry = LeaderboardManager::Instance().GetEntry(character_id, request.type());

    // 发送响应
    murim::GetMyRankResponse response;
    response.mutable_response()->set_code(0);
    response.set_type(request.type());
    response.set_rank(rank);
    response.set_total_players(1000);  // TODO: 获取真实玩家数

    if (entry) {
        auto* proto_entry = response.mutable_character_entry();
        *proto_entry = entry->ToProto();
        response.set_score(entry->score);
    }

    SendResponse(conn, 0x1004, response);
}

void LeaderboardHandler::HandleSearchLeaderboardRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleSearchLeaderboardRequest: called");

    murim::SearchLeaderboardRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse SearchLeaderboardRequest");
        return;
    }

    spdlog::info("SearchLeaderboard: type={}, search_term={}",
                 static_cast<uint32_t>(request.type()), request.search_term());

    // 搜索排行榜
    auto results = LeaderboardManager::Instance().SearchLeaderboard(
        request.type(),
        request.search_term()
    );

    // 发送响应
    murim::SearchLeaderboardResponse response;
    response.mutable_response()->set_code(0);
    response.set_type(request.type());

    for (const auto& entry : results) {
        auto* proto_entry = response.add_results();
        *proto_entry = entry.ToProto();
    }

    SendResponse(conn, 0x1006, response);
}

// ========== 辅助方法 ==========

void LeaderboardHandler::SendResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    uint16_t message_id,
    const google::protobuf::Message& response) {

    std::vector<uint8_t> data(response.ByteSizeLong());
    if (!response.SerializeToArray(data.data(), data.size())) {
        spdlog::error("Failed to serialize leaderboard protobuf response: message_id=0x{:04X}", message_id);
        return;
    }

    auto buffer = Core::Network::PacketSerializer::Serialize(message_id, data);
    if (!buffer) {
        spdlog::error("Failed to serialize leaderboard response: message_id=0x{:04X}", message_id);
        return;
    }

    conn->AsyncSend(buffer, [message_id](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send leaderboard response 0x{:04X}: {}", message_id, ec.message());
        } else {
            spdlog::debug("Leaderboard response 0x{:04X} sent: {} bytes", message_id, bytes_sent);
        }
    });
}

} // namespace Game
} // namespace Murim
