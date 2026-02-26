#include "TradeHandler.hpp"
#include "core/network/PacketSerializer.hpp"
#include "protocol/trade.pb.h"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

void TradeHandler::HandleRequestTradeRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleRequestTradeRequest: called");

    murim::RequestTradeRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse RequestTradeRequest");
        return;
    }

    spdlog::info("RequestTrade: initiator_id={}, target_id={}",
                 request.initiator_id(), request.target_id());

    // 调用 TradeManager
    uint64_t trade_id = TradeManager::Instance().RequestTrade(
        request.initiator_id(),
        request.target_id()
    );

    // 发送响应
    murim::RequestTradeResponse response;
    response.mutable_response()->set_code(trade_id > 0 ? 0 : 1);
    response.set_trade_id(trade_id);

    if (trade_id == 0) {
        response.mutable_response()->set_message("Failed to request trade");
    }

    SendResponse(conn, 0x0F02, response);
}

void TradeHandler::HandleRespondTradeRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleRespondTradeRequest: called");

    murim::RespondTradeRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse RespondTradeRequest");
        return;
    }

    spdlog::info("RespondTrade: character_id={}, trade_id={}, accept={}",
                 request.character_id(), request.trade_id(), request.accept());

    // 调用 TradeManager
    bool success = TradeManager::Instance().RespondTrade(
        request.character_id(),
        request.trade_id(),
        request.accept()
    );

    // 发送响应
    murim::RespondTradeResponse response;
    response.mutable_response()->set_code(success ? 0 : 2);
    response.set_trade_id(request.trade_id());
    response.set_accepted(request.accept());

    if (!success) {
        response.mutable_response()->set_message("Failed to respond trade");
    }

    SendResponse(conn, 0x0F04, response);
}

void TradeHandler::HandleAddTradeItemRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleAddTradeItemRequest: called");

    murim::AddTradeItemRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse AddTradeItemRequest");
        return;
    }

    spdlog::info("AddTradeItem: character_id={}, trade_id={}, item_id={}",
                 request.character_id(), request.trade_id(), request.item_id());

    // 构建物品（从请求的各个字段）
    TradeItem item;
    item.item_instance_id = request.item_instance_id();
    item.item_id = request.item_id();
    item.item_name = request.item_name();
    item.quantity = request.quantity();
    item.quality = request.quality();
    item.item_level = request.item_level();

    // 调用 TradeManager
    bool success = TradeManager::Instance().AddItem(
        request.character_id(),
        request.trade_id(),
        item
    );

    // 发送响应
    murim::AddTradeItemResponse response;
    response.mutable_response()->set_code(success ? 0 : 3);
    response.set_trade_id(request.trade_id());

    if (success) {
        auto* proto_item = response.mutable_item();
        *proto_item = ConvertToProto(item);
    } else {
        response.mutable_response()->set_message("Failed to add item");
    }

    SendResponse(conn, 0x0F06, response);
}

void TradeHandler::HandleRemoveTradeItemRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleRemoveTradeItemRequest: called");

    murim::RemoveTradeItemRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse RemoveTradeItemRequest");
        return;
    }

    spdlog::info("RemoveTradeItem: character_id={}, trade_id={}, item_instance_id={}",
                 request.character_id(), request.trade_id(), request.item_instance_id());

    // 调用 TradeManager
    bool success = TradeManager::Instance().RemoveItem(
        request.character_id(),
        request.trade_id(),
        request.item_instance_id()
    );

    // 发送响应
    murim::RemoveTradeItemResponse response;
    response.mutable_response()->set_code(success ? 0 : 4);
    response.set_trade_id(request.trade_id());
    response.set_item_instance_id(request.item_instance_id());

    if (!success) {
        response.mutable_response()->set_message("Failed to remove item");
    }

    SendResponse(conn, 0x0F08, response);
}

void TradeHandler::HandleAddTradeGoldRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleAddTradeGoldRequest: called");

    murim::AddTradeGoldRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse AddTradeGoldRequest");
        return;
    }

    spdlog::info("AddTradeGold: character_id={}, trade_id={}, gold={}",
                 request.character_id(), request.trade_id(), request.gold());

    // 调用 TradeManager
    bool success = TradeManager::Instance().AddGold(
        request.character_id(),
        request.trade_id(),
        request.gold()
    );

    // 发送响应
    murim::AddTradeGoldResponse response;
    response.mutable_response()->set_code(success ? 0 : 5);
    response.set_trade_id(request.trade_id());
    response.set_gold(request.gold());

    if (!success) {
        response.mutable_response()->set_message("Failed to add gold");
    }

    SendResponse(conn, 0x0F0A, response);
}

void TradeHandler::HandleRemoveTradeGoldRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleRemoveTradeGoldRequest: called");

    murim::RemoveTradeGoldRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse RemoveTradeGoldRequest");
        return;
    }

    spdlog::info("RemoveTradeGold: character_id={}, trade_id={}", request.character_id(), request.trade_id());

    // 调用 TradeManager
    bool success = TradeManager::Instance().RemoveGold(
        request.character_id(),
        request.trade_id()
    );

    // 获取当前金币
    // TODO: 从交易会话获取当前金币

    // 发送响应
    murim::RemoveTradeGoldResponse response;
    response.mutable_response()->set_code(success ? 0 : 6);
    response.set_trade_id(request.trade_id());
    response.set_gold(0);  // TODO: 当前金币

    if (!success) {
        response.mutable_response()->set_message("Failed to remove gold");
    }

    SendResponse(conn, 0x0F0C, response);
}

void TradeHandler::HandleConfirmTradeRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleConfirmTradeRequest: called");

    murim::ConfirmTradeRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse ConfirmTradeRequest");
        return;
    }

    spdlog::info("ConfirmTrade: character_id={}, trade_id={}", request.character_id(), request.trade_id());

    // 调用 TradeManager
    bool success = TradeManager::Instance().ConfirmTrade(
        request.character_id(),
        request.trade_id()
    );

    // 检查是否双方都已确认
    auto* trade = TradeManager::Instance().GetTradeSession(request.trade_id());
    bool both_confirmed = trade ? trade->BothConfirmed() : false;

    // 发送响应
    murim::ConfirmTradeResponse response;
    response.mutable_response()->set_code(success ? 0 : 7);
    response.set_trade_id(request.trade_id());
    response.set_both_confirmed(both_confirmed);

    if (!success) {
        response.mutable_response()->set_message("Failed to confirm trade");
    }

    SendResponse(conn, 0x0F0E, response);
}

void TradeHandler::HandleCancelTradeRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("HandleCancelTradeRequest: called");

    murim::CancelTradeRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse CancelTradeRequest");
        return;
    }

    spdlog::info("CancelTrade: character_id={}, trade_id={}, reason={}",
                 request.character_id(), request.trade_id(), request.reason());

    // 调用 TradeManager
    bool success = TradeManager::Instance().CancelTrade(
        request.character_id(),
        request.trade_id(),
        request.reason()
    );

    // 发送响应
    murim::CancelTradeResponse response;
    response.mutable_response()->set_code(success ? 0 : 8);
    response.set_trade_id(request.trade_id());
    response.set_reason(request.reason());

    if (!success) {
        response.mutable_response()->set_message("Failed to cancel trade");
    }

    SendResponse(conn, 0x0F10, response);
}

// ========== 辅助方法 ==========

void TradeHandler::SendResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    uint16_t message_id,
    const google::protobuf::Message& response) {

    std::vector<uint8_t> data(response.ByteSizeLong());
    if (!response.SerializeToArray(data.data(), data.size())) {
        spdlog::error("Failed to serialize trade protobuf response: message_id=0x{:04X}", message_id);
        return;
    }

    auto buffer = Core::Network::PacketSerializer::Serialize(message_id, data);
    if (!buffer) {
        spdlog::error("Failed to serialize trade response: message_id=0x{:04X}", message_id);
        return;
    }

    conn->AsyncSend(buffer, [message_id](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send trade response 0x{:04X}: {}", message_id, ec.message());
        } else {
            spdlog::debug("Trade response 0x{:04X} sent: {} bytes", message_id, bytes_sent);
        }
    });
}

murim::TradeItem TradeHandler::ConvertToProto(const TradeItem& item) {
    murim::TradeItem proto;
    proto.set_item_instance_id(item.item_instance_id);
    proto.set_item_id(item.item_id);
    proto.set_item_name(item.item_name);
    proto.set_quantity(item.quantity);
    proto.set_quality(item.quality);
    proto.set_item_level(item.item_level);
    return proto;
}

TradeItem TradeHandler::ConvertFromProto(const murim::TradeItem& proto) {
    TradeItem item;
    item.item_instance_id = proto.item_instance_id();
    item.item_id = proto.item_id();
    item.item_name = proto.item_name();
    item.quantity = proto.quantity();
    item.quality = proto.quality();
    item.item_level = proto.item_level();
    return item;
}

murim::TradeSession TradeHandler::ConvertToProto(const TradeSession& session) {
    murim::TradeSession proto;
    proto.set_trade_id(session.trade_id);
    proto.set_initiator_id(session.initiator_id);
    proto.set_initiator_name(session.initiator_name);
    proto.set_target_id(session.target_id);
    proto.set_target_name(session.target_name);
    proto.set_status(session.status);
    proto.set_start_time(session.start_time);
    proto.set_timeout_time(session.timeout_time);
    proto.set_initiator_gold(session.initiator_gold);
    proto.set_target_gold(session.target_gold);
    proto.set_initiator_confirmed(session.initiator_confirmed);
    proto.set_target_confirmed(session.target_confirmed);

    for (const auto& item : session.initiator_items) {
        auto* proto_item = proto.add_initiator_items();
        *proto_item = ConvertToProto(item);
    }

    for (const auto& item : session.target_items) {
        auto* proto_item = proto.add_target_items();
        *proto_item = ConvertToProto(item);
    }

    return proto;
}

} // namespace Game
} // namespace Murim
