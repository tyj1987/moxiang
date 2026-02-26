#include "MarketHandler.hpp"
#include "core/network/PacketSerializer.hpp"
#include "protocol/market.pb.h"
#include "core/spdlog_wrapper.hpp"
#include <vector>

namespace Murim {
namespace Game {

void MarketHandler::HandleListItemRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleListItemRequest: called");

    // 解析请求
    murim::ListItemRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse ListItemRequest");
        SendResponse(conn, 0x0E02, 1, {});  // 错误码 1: 解析失败
        return;
    }

    spdlog::info("ListItem: character_id={}, item_id={}, price={}, buyout={}",
                 request.character_id(), request.item_id(), request.price(), request.buyout_price());

    // 调用 MarketManager 上架物品
    auto result = MarketManager::Instance().ListItem(
        request.character_id(),
        "",  // seller_name (TODO: 从 CharacterManager 获取)
        request.item_instance_id(),
        request.item_id(),
        "",  // item_name (TODO: 从 ItemManager 获取)
        request.quantity(),
        request.price(),
        request.buyout_price(),
        request.duration_hours()
    );

    // 发送响应
    murim::ListItemResponse response;
    response.mutable_response()->set_code(result.success ? 0 : 1);
    response.set_listing_id(result.listing_id);
    response.set_list_fee(result.fee);

    if (!result.success) {
        response.mutable_response()->set_message(result.error_message);
    }

    SendResponse(conn, 0x0E02, response);
}

void MarketHandler::HandleDelistItemRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleDelistItemRequest: called");

    // 解析请求
    murim::DelistItemRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse DelistItemRequest");
        SendResponse(conn, 0x0E04, 1, {});
        return;
    }

    spdlog::info("DelistItem: character_id={}, listing_id={}", request.character_id(), request.listing_id());

    // 调用 MarketManager 下架物品
    bool success = MarketManager::Instance().DelistItem(request.character_id(), request.listing_id());

    // 发送响应
    murim::DelistItemResponse response;
    response.mutable_response()->set_code(success ? 0 : 2);  // 0=成功, 2=失败
    response.set_listing_id(request.listing_id());

    if (!success) {
        response.mutable_response()->set_message("Failed to delist item");
    }

    SendResponse(conn, 0x0E04, response);
}

void MarketHandler::HandleBuyItemRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleBuyItemRequest: called");

    // 解析请求
    murim::BuyItemRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse BuyItemRequest");
        SendResponse(conn, 0x0E06, 1, {});
        return;
    }

    spdlog::info("BuyItem: character_id={}, listing_id={}, buyout_price={}",
                 request.character_id(), request.listing_id(), request.buyout_price());

    // 调用 MarketManager 购买物品
    auto result = MarketManager::Instance().BuyItem(
        request.character_id(),
        request.listing_id(),
        request.buyout_price()
    );

    // 发送响应
    murim::BuyItemResponse response;
    response.mutable_response()->set_code(result.success ? 0 : 3);
    response.set_listing_id(request.listing_id());
    response.set_spent_gold(result.spent_gold);

    if (!result.success) {
        response.mutable_response()->set_message(result.error_message);
    }

    SendResponse(conn, 0x0E06, response);
}

void MarketHandler::HandleBidItemRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleBidItemRequest: called");

    // 解析请求
    murim::BidItemRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse BidItemRequest");
        SendResponse(conn, 0x0E08, 1, {});
        return;
    }

    spdlog::info("BidItem: character_id={}, listing_id={}, bid_price={}",
                 request.character_id(), request.listing_id(), request.bid_price());

    // 调用 MarketManager 出价
    auto result = MarketManager::Instance().BidItem(
        request.character_id(),
        "",  // bidder_name (TODO: 从 CharacterManager 获取)
        request.listing_id(),
        request.bid_price()
    );

    // 发送响应
    murim::BidItemResponse response;
    response.mutable_response()->set_code(result.success ? 0 : 4);
    response.set_listing_id(request.listing_id());
    response.set_current_bid(result.current_bid);
    response.set_outbid(result.outbid);

    if (!result.success) {
        response.mutable_response()->set_message(result.error_message);
    }

    SendResponse(conn, 0x0E08, response);
}

void MarketHandler::HandleSearchMarketRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleSearchMarketRequest: called");

    // 解析请求
    murim::SearchMarketRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse SearchMarketRequest");
        SendResponse(conn, 0x0E0A, 1, {});
        return;
    }

    spdlog::info("SearchMarket: character_id={}, search_text={}, item_type={}, page={}",
                 request.character_id(), request.search_text(), request.item_type(), request.page());

    // 调用 MarketManager 搜索物品
    auto listings = MarketManager::Instance().SearchListings(
        request.search_text(),
        request.item_type(),
        request.min_level(),
        request.max_level(),
        request.min_price(),
        request.max_price(),
        request.quality(),
        request.sort_type(),
        request.page(),
        request.page_size()
    );

    // 构建响应
    murim::SearchMarketResponse response;
    response.mutable_response()->set_code(0);
    response.set_total_count(listings.size());

    for (const auto& listing : listings) {
        auto* proto_listing = response.add_listings();
        *proto_listing = ConvertToProto(listing);
    }

    SendResponse(conn, 0x0E0A, response);
}

void MarketHandler::HandleGetMyListingsRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleGetMyListingsRequest: called");

    // 解析请求
    murim::GetMyListingsRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse GetMyListingsRequest");
        SendResponse(conn, 0x0E0C, 1, {});
        return;
    }

    spdlog::info("GetMyListings: character_id={}, page={}", request.character_id(), request.page());

    // 调用 MarketManager 获取我的挂单
    auto listings = MarketManager::Instance().GetMyListings(
        request.character_id(),
        request.page(),
        request.page_size()
    );

    // 构建响应
    murim::GetMyListingsResponse response;
    response.mutable_response()->set_code(0);
    response.set_total_count(listings.size());

    for (const auto& listing : listings) {
        auto* proto_listing = response.add_listings();
        *proto_listing = ConvertToProto(listing);
    }

    SendResponse(conn, 0x0E0C, response);
}

void MarketHandler::HandleGetListingDetailsRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleGetListingDetailsRequest: called");

    // 解析请求
    murim::GetListingDetailsRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse GetListingDetailsRequest");
        SendResponse(conn, 0x0E0E, 1, {});
        return;
    }

    spdlog::info("GetListingDetails: character_id={}, listing_id={}", request.character_id(), request.listing_id());

    // 调用 MarketManager 获取挂单详情
    auto* listing = MarketManager::Instance().GetListing(request.listing_id());

    // 构建响应
    murim::GetListingDetailsResponse response;
    response.mutable_response()->set_code(listing ? 0 : 2);  // 0=成功, 2=不存在

    if (listing) {
        auto* proto_listing = response.mutable_listing();
        *proto_listing = ConvertToProto(*listing);
    } else {
        response.mutable_response()->set_message("Listing not found");
    }

    SendResponse(conn, 0x0E0E, response);
}

// ========== 辅助方法 ==========

void MarketHandler::SendResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    uint16_t message_id,
    uint32_t result_code,
    const std::vector<uint8_t>& data) {

    // 创建简单响应（仅用于错误情况）
    std::vector<uint8_t> response;
    response.push_back(static_cast<uint8_t>(result_code));
    response.insert(response.end(), data.begin(), data.end());

    auto buffer = Core::Network::PacketSerializer::Serialize(message_id, response);
    if (!buffer) {
        spdlog::error("Failed to serialize market response: message_id=0x{:04X}", message_id);
        return;
    }

    conn->AsyncSend(buffer, [message_id](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send market response 0x{:04X}: {}", message_id, ec.message());
        } else {
            spdlog::debug("Market response 0x{:04X} sent: {} bytes", message_id, bytes_sent);
        }
    });
}

void MarketHandler::SendResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    uint16_t message_id,
    const google::protobuf::Message& response) {

    // 序列化 protobuf 消息
    std::vector<uint8_t> data(response.ByteSizeLong());
    if (!response.SerializeToArray(data.data(), data.size())) {
        spdlog::error("Failed to serialize market protobuf response: message_id=0x{:04X}", message_id);
        return;
    }

    // 使用 PacketSerializer 包装
    auto buffer = Core::Network::PacketSerializer::Serialize(message_id, data);
    if (!buffer) {
        spdlog::error("Failed to serialize market response: message_id=0x{:04X}", message_id);
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [message_id](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send market response 0x{:04X}: {}", message_id, ec.message());
        } else {
            spdlog::debug("Market response 0x{:04X} sent: {} bytes", message_id, bytes_sent);
        }
    });
}

murim::MarketListing MarketHandler::ConvertToProto(const MarketListing& listing) {
    murim::MarketListing proto;

    proto.set_listing_id(listing.listing_id);
    proto.set_seller_id(listing.seller_id);
    proto.set_seller_name(listing.seller_name);
    proto.set_item_instance_id(listing.item_instance_id);
    proto.set_item_id(listing.item_id);
    proto.set_item_name(listing.item_name);
    proto.set_item_type(listing.item_type);
    proto.set_quantity(listing.quantity);
    proto.set_price(listing.price);
    proto.set_buyout_price(listing.buyout_price);
    proto.set_bid_price(listing.current_bid);
    proto.set_bidder_id(listing.bidder_id);
    proto.set_bidder_name(listing.bidder_name);
    proto.set_status(listing.status);
    proto.set_list_time(listing.list_time);
    proto.set_duration_hours(listing.duration_hours);
    proto.set_end_time(listing.end_time);
    proto.set_item_level(listing.item_level);
    proto.set_quality(listing.quality);

    return proto;
}

} // namespace Game
} // namespace Murim
