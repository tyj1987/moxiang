#include "WarehouseHandler.hpp"
#include "WarehouseManager.hpp"
#include "core/network/PacketSerializer.hpp"
#include "protocol/npc.pb.h"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

void WarehouseHandler::HandleOpenWarehouseRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::network::OpenWarehouseRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse OpenWarehouseRequest");
        return;
    }

    spdlog::info("HandleOpenWarehouseRequest: player_id={}", request.player_id());

    // 检查仓库是否存在
    bool has_warehouse = WarehouseManager::Instance().HasWarehouse(request.player_id());

    // 发送响应
    SendOpenWarehouseResponse(conn, has_warehouse ? 0 : 1, request.player_id());
}

void WarehouseHandler::HandleDepositItemRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::network::DepositItemRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse DepositItemRequest");
        return;
    }

    spdlog::info("HandleDepositItemRequest: player_id={}, item_instance_id={}",
                 request.player_id(), request.item_instance_id());

    // 处理存入
    auto result = WarehouseManager::Instance().HandleDepositItem(
        request.player_id(),
        request.item_instance_id(),
        0,  // item_id TODO: 从item_instance_id获取
        1   // count TODO: 从物品实例获取
    );

    // 发送响应
    SendDepositItemResponse(conn, result);
}

void WarehouseHandler::HandleWithdrawItemRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::network::WithdrawItemRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse WithdrawItemRequest");
        return;
    }

    spdlog::info("HandleWithdrawItemRequest: player_id={}, slot_id={}, quantity={}",
                 request.player_id(), request.slot_id(), request.quantity());

    // 处理取出
    auto result = WarehouseManager::Instance().HandleWithdrawItem(
        request.player_id(),
        request.slot_id()
    );

    // 发送响应
    SendWithdrawItemResponse(conn, result);
}

void WarehouseHandler::HandleSortWarehouseRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::network::SortWarehouseRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse SortWarehouseRequest");
        return;
    }

    spdlog::info("HandleSortWarehouseRequest: player_id={}", request.player_id());

    // 处理整理
    bool success = WarehouseManager::Instance().HandleSortWarehouse(request.player_id());

    // 发送响应
    SendSortWarehouseResponse(conn, success ? 0 : 1);
}

void WarehouseHandler::HandleCloseWarehouseNotify(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // CloseWarehouseNotify 消息未在 proto 中定义，暂时只记录日志
    spdlog::info("HandleCloseWarehouseNotify: warehouse close notification received");

    // 清理仓库状态
    // TODO: 清理玩家的仓库状态
}

void WarehouseHandler::SendOpenWarehouseResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    int32_t result_code,
    uint64_t character_id) {

    murim::network::OpenWarehouseResponse response;
    response.set_result_code(result_code);

    if (result_code == 0) {
        auto warehouse = WarehouseManager::Instance().GetWarehouse(character_id);
        if (warehouse) {
            auto* warehouse_info = response.mutable_warehouse();
            warehouse_info->set_character_id(warehouse->character_id);
            // warehouse_info->set_character_name(""); // TODO: 获取角色名称
            warehouse_info->set_max_slots(warehouse->capacity);
            warehouse_info->set_used_slots(warehouse->used_slots);
            warehouse_info->set_money(warehouse->gold);

            // 添加物品列表
            for (const auto& item : warehouse->items) {
                auto* slot_info = warehouse_info->add_slots();
                slot_info->set_slot_id(item.position);
                slot_info->set_item_instance_id(item.item_instance_id);
                slot_info->set_item_template_id(item.item_id);
                slot_info->set_quantity(item.count);
                // slot_info->set_item_name(""); // TODO: 获取物品名称
                // slot_info->set_enchant_level(0); // TODO: 获取附魔等级
            }
        }
    }

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0902 = Warehouse/OpenWarehouseResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0902, data);
    if (!buffer) {
        spdlog::error("Failed to serialize OpenWarehouseResponse");
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send OpenWarehouseResponse: {}", ec.message());
        } else {
            spdlog::debug("OpenWarehouseResponse sent: {} bytes", bytes_sent);
        }
    });
}

void WarehouseHandler::SendDepositItemResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    const WarehouseResult& result) {

    murim::network::DepositItemResponse response;
    response.set_result_code(result.error_code);
    response.set_new_slot_id(result.position);
    response.set_message(result.error_message);

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0904 = Warehouse/DepositItemResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0904, data);
    if (!buffer) {
        spdlog::error("Failed to serialize DepositItemResponse");
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send DepositItemResponse: {}", ec.message());
        } else {
            spdlog::debug("DepositItemResponse sent: {} bytes", bytes_sent);
        }
    });
}

void WarehouseHandler::SendWithdrawItemResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    const WarehouseResult& result) {

    murim::network::WithdrawItemResponse response;
    response.set_result_code(result.error_code);
    response.set_item_instance_id(result.item_instance_id);

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0906 = Warehouse/WithdrawItemResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0906, data);
    if (!buffer) {
        spdlog::error("Failed to serialize WithdrawItemResponse");
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send WithdrawItemResponse: {}", ec.message());
        } else {
            spdlog::debug("WithdrawItemResponse sent: {} bytes", bytes_sent);
        }
    });
}

void WarehouseHandler::SendSortWarehouseResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    int32_t result_code) {

    murim::network::SortWarehouseResponse response;
    response.set_result_code(result_code);

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0908 = Warehouse/SortWarehouseResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0908, data);
    if (!buffer) {
        spdlog::error("Failed to serialize SortWarehouseResponse");
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send SortWarehouseResponse: {}", ec.message());
        } else {
            spdlog::debug("SortWarehouseResponse sent: {} bytes", bytes_sent);
        }
    });
}

} // namespace Game
} // namespace Murim
