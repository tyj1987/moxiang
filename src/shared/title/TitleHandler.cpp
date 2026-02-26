#include "TitleHandler.hpp"
#include "TitleManager.hpp"
#include "core/network/PacketSerializer.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

// ========== Helper Functions ==========

void TitleHandler::SendResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    uint16_t message_id,
    const google::protobuf::Message& response) {

    if (!conn) {
        spdlog::error("[TitleHandler] Connection is null");
        return;
    }

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    if (!response.SerializeToArray(data.data(), data.size())) {
        spdlog::error("[TitleHandler] Failed to serialize response: message_id={:#x}", message_id);
        return;
    }

    // 使用 PacketSerializer 创建数据包
    auto buffer = Core::Network::PacketSerializer::Serialize(message_id, data);
    if (!buffer) {
        spdlog::error("[TitleHandler] Failed to create packet: message_id={:#x}", message_id);
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [message_id](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("[TitleHandler] Failed to send response 0x{:04X}: {}", message_id, ec.message());
        } else {
            spdlog::debug("[TitleHandler] Sent response: message_id={:#x}, size={}", message_id, bytes_sent);
        }
    });
}

static uint64_t GetCharacterIdFromConnection(std::shared_ptr<Core::Network::Connection> conn) {
    // TODO: 从 Session 中获取 character_id
    // 暂时返回测试值
    return 1001;
}

// ========== Message Handlers ==========

void TitleHandler::HandleGetTitleListRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[TitleHandler] HandleGetTitleListRequest");

    // 解析请求
    murim::GetTitleListRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[TitleHandler] Failed to parse GetTitleListRequest");
        return;
    }

    // 获取所有称号定义
    auto& title_manager = TitleManager::Instance();
    auto titles = title_manager.GetAllTitles();

    // 构建响应
    murim::GetTitleListResponse response;
    response.mutable_response()->set_code(0);
    response.mutable_response()->set_message("获取称号列表成功");

    for (const auto& definition : titles) {
        auto* proto_def = response.add_titles();
        *proto_def = definition.ToProto();
    }

    // 发送响应
    SendResponse(conn, 0x1202, response);
    spdlog::info("[TitleHandler] Sent title list: count={}", titles.size());
}

void TitleHandler::HandleGetTitleDetailRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[TitleHandler] HandleGetTitleDetailRequest");

    // 解析请求
    murim::GetTitleDetailRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[TitleHandler] Failed to parse GetTitleDetailRequest");
        return;
    }

    uint32_t title_id = request.title_id();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 获取称号定义
    auto& title_manager = TitleManager::Instance();
    auto* definition = title_manager.GetTitleDefinition(title_id);

    if (!definition) {
        // 称号不存在
        murim::GetTitleDetailResponse response;
        response.mutable_response()->set_code(1);  // 错误码
        response.mutable_response()->set_message("称号不存在");
        SendResponse(conn, 0x1204, response);
        spdlog::warn("[TitleHandler] Title not found: title_id={}", title_id);
        return;
    }

    // 检查是否拥有
    bool owned = title_manager.HasTitle(character_id, title_id);

    // 检查是否可装备
    bool can_equip = owned;
    if (owned) {
        // TODO: 检查等级要求等其他条件
        can_equip = true;
    }

    // 构建响应
    murim::GetTitleDetailResponse response;
    response.mutable_response()->set_code(0);
    response.mutable_response()->set_message("获取称号详情成功");
    response.mutable_title()->CopyFrom(definition->ToProto());
    response.set_owned(owned);
    response.set_can_equip(can_equip);

    // 发送响应
    SendResponse(conn, 0x1204, response);
    spdlog::info("[TitleHandler] Sent title detail: title_id={}, owned={}, can_equip={}",
                 title_id, owned, can_equip);
}

void TitleHandler::HandleGetMyTitlesRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[TitleHandler] HandleGetMyTitlesRequest");

    // 解析请求
    murim::GetMyTitlesRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[TitleHandler] Failed to parse GetMyTitlesRequest");
        return;
    }

    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 获取玩家称号列表
    auto& title_manager = TitleManager::Instance();
    auto* titles = title_manager.GetPlayerTitles(character_id);

    // 获取当前装备的称号
    uint32_t equipped_title_id = title_manager.GetEquippedTitle(character_id);

    // 构建响应
    murim::GetMyTitlesResponse response;
    response.mutable_response()->set_code(0);
    response.mutable_response()->set_message("获取我的称号成功");
    response.set_equipped_title_id(equipped_title_id);

    for (const auto& title : *titles) {
        auto* proto_title = response.add_titles();
        *proto_title = title.ToProto();
    }

    // 发送响应
    SendResponse(conn, 0x1206, response);
    spdlog::info("[TitleHandler] Sent my titles: character_id={}, count={}, equipped={}",
                 character_id, titles->size(), equipped_title_id);
}

void TitleHandler::HandleEquipTitleRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[TitleHandler] HandleEquipTitleRequest");

    // 解析请求
    murim::EquipTitleRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[TitleHandler] Failed to parse EquipTitleRequest");
        return;
    }

    uint32_t title_id = request.title_id();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 装备称号
    auto& title_manager = TitleManager::Instance();
    bool success = title_manager.EquipTitle(character_id, title_id);

    // 构建响应
    murim::EquipTitleResponse response;
    if (success) {
        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("装备称号成功");
        response.set_title_id(title_id);

        // 获取显示名称
        auto* definition = title_manager.GetTitleDefinition(title_id);
        std::string display_name = title_manager.GetDisplayName(character_id, "玩家");
        response.set_display_name(display_name);

        spdlog::info("[TitleHandler] Title equipped: character_id={}, title_id={}", character_id, title_id);
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("装备称号失败：未拥有该称号或无法装备");
        response.set_title_id(0);
        response.set_display_name("");

        spdlog::warn("[TitleHandler] Failed to equip title: character_id={}, title_id={}", character_id, title_id);
    }

    // 发送响应
    SendResponse(conn, 0x1208, response);
}

void TitleHandler::HandleUnequipTitleRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[TitleHandler] HandleUnequipTitleRequest");

    // 解析请求
    murim::UnequipTitleRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[TitleHandler] Failed to parse UnequipTitleRequest");
        return;
    }

    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 卸载称号
    auto& title_manager = TitleManager::Instance();
    bool success = title_manager.UnequipTitle(character_id);

    // 构建响应
    murim::UnequipTitleResponse response;
    if (success) {
        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("卸载称号成功");

        // 获取显示名称（不带称号）
        std::string display_name = title_manager.GetDisplayName(character_id, "玩家");
        response.set_display_name(display_name);

        spdlog::info("[TitleHandler] Title unequipped: character_id={}", character_id);
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("卸载称号失败：当前未装备称号");
        response.set_display_name("");

        spdlog::warn("[TitleHandler] Failed to unequip title: character_id={}", character_id);
    }

    // 发送响应
    SendResponse(conn, 0x120A, response);
}

} // namespace Game
} // namespace Murim
