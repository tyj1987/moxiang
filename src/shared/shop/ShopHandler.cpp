#include "ShopHandler.hpp"
#include "ShopManager.hpp"
#include "core/network/PacketSerializer.hpp"
#include "protocol/npc.pb.h"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

void ShopHandler::HandleOpenShopRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::network::OpenShopRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse OpenShopRequest");
        return;
    }

    spdlog::info("HandleOpenShopRequest: player_id={}, shop_id={}",
                 request.player_id(), request.shop_id());

    // 获取商店
    auto shop = ShopManager::Instance().GetShop(request.shop_id());
    if (!shop) {
        spdlog::warn("Shop {} not found", request.shop_id());
        SendOpenShopResponse(conn, 1, request.shop_id());
        return;
    }

    spdlog::info("Opened shop {} ({}) for player {}",
                 shop->shop_id, shop->shop_name, request.player_id());

    // 发送成功响应
    SendOpenShopResponse(conn, 0, shop->shop_id);
}

void ShopHandler::HandleBuyItemRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::network::BuyItemRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse BuyItemRequest");
        return;
    }

    spdlog::info("HandleBuyItemRequest: player_id={}, shop_id={}, item_id={}, quantity={}",
                 request.player_id(), request.shop_id(), request.item_id(), request.quantity());

    // 处理购买
    auto result = ShopManager::Instance().HandleBuyRequest(
        request.player_id(),
        request.shop_id(),
        request.item_id(),
        static_cast<uint32_t>(request.quantity())
    );

    // 发送响应
    SendBuyItemResponse(conn, result);
}

void ShopHandler::HandleSellItemRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::network::SellItemRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse SellItemRequest");
        return;
    }

    spdlog::info("HandleSellItemRequest: player_id={}, shop_id={}, item_instance_id={}, quantity={}",
                 request.player_id(), request.shop_id(), request.item_instance_id(), request.quantity());

    // 处理出售 (note: using item_instance_id as item_id for now)
    auto result = ShopManager::Instance().HandleSellRequest(
        request.player_id(),
        request.shop_id(),
        static_cast<uint32_t>(request.item_instance_id()),  // Temporary: cast instance_id to item_id
        static_cast<uint32_t>(request.quantity())
    );

    // 发送响应
    SendSellItemResponse(conn, result);
}

void ShopHandler::HandleCloseShopNotify(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 目前没有CloseShopNotify协议定义
    // 暂时只记录日志
    spdlog::info("HandleCloseShopNotify called (protocol not defined yet)");

    // 清理商店状态
    // TODO: 清理玩家的商店状态
}

void ShopHandler::SendOpenShopResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    int32_t result_code,
    uint32_t shop_id) {

    murim::network::OpenShopResponse response;
    response.set_result_code(result_code);

    if (result_code == 0) {
        // 获取商店信息
        auto shop = ShopManager::Instance().GetShop(shop_id);
        if (shop) {
            response.set_shop_id(shop->shop_id);
            response.set_shop_name(shop->shop_name);
            response.set_player_gold(0);  // TODO: Get actual player gold

            // 添加商品列表
            for (const auto& pair : shop->items) {
                auto* item_info = response.add_items();
                item_info->set_item_id(pair.second.item_id);
                item_info->set_item_name("");  // TODO: Get item name from ItemManager
                item_info->set_stock(pair.second.stock);
                item_info->set_price_multiplier(1.0f);  // TODO: Calculate based on shop discounts
                item_info->set_base_price(pair.second.price);
            }
        }
    }

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0702 = Shop/OpenShopResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0702, data);
    if (!buffer) {
        spdlog::error("Failed to serialize OpenShopResponse");
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send OpenShopResponse: {}", ec.message());
        } else {
            spdlog::debug("OpenShopResponse sent: {} bytes", bytes_sent);
        }
    });
}

void ShopHandler::SendBuyItemResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    const TradeResult& result) {

    murim::network::BuyItemResponse response;
    response.set_result_code(result.error_code);
    response.set_item_instance_id(result.item_id);  // Use item_id as instance_id for now
    response.set_remaining_gold(result.new_money);

    // Set message based on success/failure
    if (result.success) {
        response.set_message("购买成功");
    } else if (!result.error_message.empty()) {
        response.set_message(result.error_message);
    } else {
        response.set_message("购买失败");
    }

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0704 = Shop/BuyItemResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0704, data);
    if (!buffer) {
        spdlog::error("Failed to serialize BuyItemResponse");
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send BuyItemResponse: {}", ec.message());
        } else {
            spdlog::debug("BuyItemResponse sent: {} bytes", bytes_sent);
        }
    });
}

void ShopHandler::SendSellItemResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    const TradeResult& result) {

    murim::network::SellItemResponse response;
    response.set_result_code(result.error_code);
    response.set_gold_gained(result.item_id);  // Use item_id as gold_gained for now (TODO: calculate actual gold)
    response.set_total_gold(result.new_money);

    // Set message based on success/failure
    if (result.success) {
        response.set_message("出售成功");
    } else if (!result.error_message.empty()) {
        response.set_message(result.error_message);
    } else {
        response.set_message("出售失败");
    }

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0706 = Shop/SellItemResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0706, data);
    if (!buffer) {
        spdlog::error("Failed to serialize SellItemResponse");
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send SellItemResponse: {}", ec.message());
        } else {
            spdlog::debug("SellItemResponse sent: {} bytes", bytes_sent);
        }
    });
}

} // namespace Game
} // namespace Murim
