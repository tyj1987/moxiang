#include "MountHandler.hpp"
#include "MountManager.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

// ========== Helper Functions ==========

static void SendResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    uint16_t message_id,
    const google::protobuf::Message& response) {

    if (!conn) {
        spdlog::error("[MountHandler] Connection is null");
        return;
    }

    // 序列化响应
    std::string serialized = response.SerializeAsString();
    size_t total_size = 2 + serialized.size();  // 长度(2字节) + 数据

    // 创建缓冲区
    Core::Network::ByteBuffer buffer(total_size);
    buffer.WriteUInt16(static_cast<uint16_t>(serialized.size()));
    buffer.WriteBytes(reinterpret_cast<const uint8_t*>(serialized.data()), serialized.size());

    // 发送响应
    conn->Send(message_id, buffer);
    spdlog::debug("[MountHandler] Sent response: message_id={:#x}, size={}", message_id, total_size);
}

static uint64_t GetCharacterIdFromConnection(std::shared_ptr<Core::Network::Connection> conn) {
    // TODO: 从 Session 中获取 character_id
    // 暂时返回测试值
    return 1001;
}

// ========== Message Handlers ==========

void MountHandler::HandleGetMountListRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[MountHandler] HandleGetMountListRequest");

    // 解析请求
    murim::GetMountListRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[MountHandler] Failed to parse GetMountListRequest");
        return;
    }

    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 获取坐骑列表
    auto& mount_manager = MountManager::Instance();
    auto* mounts = mount_manager.GetPlayerMounts(character_id);

    // 获取当前骑乘的坐骑
    uint32_t current_mount_id = 0;
    auto* ridden_mount = mount_manager.GetRiddenMount(character_id);
    if (ridden_mount) {
        current_mount_id = ridden_mount->mount_id;
    }

    // 构建响应
    murim::GetMountListResponse response;
    response.mutable_response()->set_code(0);
    response.mutable_response()->set_message("获取坐骑列表成功");
    response.set_current_mount_id(current_mount_id);

    for (const auto& mount : *mounts) {
        auto* proto_mount = response.add_mounts();
        *proto_mount = mount.ToProto();
    }

    // 发送响应
    SendResponse(conn, 0x1402, response);
    spdlog::info("[MountHandler] Sent mount list: count={}", mounts->size());
}

void MountHandler::HandleGetMountDetailRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[MountHandler] HandleGetMountDetailRequest");

    // 解析请求
    murim::GetMountDetailRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[MountHandler] Failed to parse GetMountDetailRequest");
        return;
    }

    uint32_t mount_id = request.mount_id();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 获取坐骑数据
    auto& mount_manager = MountManager::Instance();
    auto* mount = mount_manager.GetMount(character_id, mount_id);

    if (!mount) {
        // 坐骑不存在
        murim::GetMountDetailResponse response;
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("坐骑不存在");
        SendResponse(conn, 0x1404, response);
        spdlog::warn("[MountHandler] Mount not found: mount_id={}", mount_id);
        return;
    }

    // 获取坐骑定义
    auto* definition = mount_manager.GetMountDefinition(mount->mount_id);

    // 计算总速度加成
    uint32_t total_speed_bonus = mount_manager.CalculateTotalSpeedBonus(character_id, *mount);

    // 构建响应
    murim::GetMountDetailResponse response;
    response.mutable_response()->set_code(0);
    response.mutable_response()->set_message("获取坐骑详情成功");
    response.mutable_mount()->CopyFrom(mount->ToProto());
    if (definition) {
        response.mutable_definition()->CopyFrom(definition->ToProto());
    }
    response.set_total_speed_bonus(total_speed_bonus);

    // 发送响应
    SendResponse(conn, 0x1404, response);
    spdlog::info("[MountHandler] Sent mount detail: mount_id={}, speed_bonus={}",
                 mount_id, total_speed_bonus);
}

void MountHandler::HandleRideMountRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[MountHandler] HandleRideMountRequest");

    // 解析请求
    murim::RideMountRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[MountHandler] Failed to parse RideMountRequest");
        return;
    }

    uint32_t mount_id = request.mount_id();
    bool ride = request.ride();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    auto& mount_manager = MountManager::Instance();

    // 构建响应
    murim::RideMountResponse response;

    if (ride) {
        // 骑乘坐骑
        bool success = mount_manager.RideMount(character_id, mount_id);

        if (success) {
            uint32_t speed_bonus = mount_manager.GetCurrentSpeedBonus(character_id);

            response.mutable_response()->set_code(0);
            response.mutable_response()->set_message("骑乘成功");
            response.set_mount_id(mount_id);
            response.set_riding(true);
            response.set_current_speed_bonus(speed_bonus);

            spdlog::info("[MountHandler] Mount ridden: mount_id={}, speed_bonus={}",
                         mount_id, speed_bonus);
        } else {
            response.mutable_response()->set_code(1);
            response.mutable_response()->set_message("骑乘失败");
            response.set_mount_id(0);
            response.set_riding(false);
            response.set_current_speed_bonus(0);

            spdlog::warn("[MountHandler] Failed to ride mount: mount_id={}", mount_id);
        }
    } else {
        // 下马
        bool success = mount_manager.UnrideMount(character_id);

        if (success) {
            response.mutable_response()->set_code(0);
            response.mutable_response()->set_message("下马成功");
            response.set_mount_id(0);
            response.set_riding(false);
            response.set_current_speed_bonus(0);

            spdlog::info("[MountHandler] Mount unriden");
        } else {
            response.mutable_response()->set_code(1);
            response.mutable_response()->set_message("下马失败");
            response.set_mount_id(mount_id);
            response.set_riding(true);
            response.set_current_speed_bonus(mount_manager.GetCurrentSpeedBonus(character_id));

            spdlog::warn("[MountHandler] Failed to unride mount");
        }
    }

    // 发送响应
    SendResponse(conn, 0x1406, response);
}

void MountHandler::HandleEvolveMountRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[MountHandler] HandleEvolveMountRequest");

    // 解析请求
    murim::EvolveMountRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[MountHandler] Failed to parse EvolveMountRequest");
        return;
    }

    uint32_t mount_id = request.mount_id();
    uint32_t material_count = request.material_count();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 获取当前星级
    auto* mount_before = mount_manager.GetMount(character_id, mount_id);
    uint32_t old_star = mount_before ? mount_before->star_level : 0;

    // 进化坐骑
    auto& mount_manager = MountManager::Instance();
    bool success = mount_manager.EvolveMount(character_id, mount_id, material_count);

    // 重新获取坐骑数据
    auto* mount = mount_manager.GetMount(character_id, mount_id);

    // 构建响应
    murim::EvolveMountResponse response;
    if (success && mount) {
        uint32_t total_speed_bonus = mount_manager.CalculateTotalSpeedBonus(character_id, *mount);

        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("进化成功");
        response.set_mount_id(mount_id);
        response.set_old_star(old_star);
        response.set_new_star(mount->star_level);
        response.set_total_speed_bonus(total_speed_bonus);

        spdlog::info("[MountHandler] Mount evolved: mount_id={}, star: {} -> {}, speed={}",
                     mount_id, old_star, mount->star_level, total_speed_bonus);
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("进化失败");
        response.set_mount_id(0);
        response.set_old_star(0);
        response.set_new_star(0);
        response.set_total_speed_bonus(0);

        spdlog::warn("[MountHandler] Failed to evolve mount: mount_id={}", mount_id);
    }

    // 发送响应
    SendResponse(conn, 0x1408, response);
}

void MountHandler::HandleEquipMountSaddleRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[MountHandler] HandleEquipMountSaddleRequest");

    // 解析请求
    murim::EquipMountSaddleRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[MountHandler] Failed to parse EquipMountSaddleRequest");
        return;
    }

    uint32_t mount_id = request.mount_id();
    uint32_t saddle_id = request.saddle_id();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 装备鞍具
    auto& mount_manager = MountManager::Instance();
    bool success = mount_manager.EquipSaddle(character_id, mount_id, saddle_id);

    // 构建响应
    murim::EquipMountSaddleResponse response;
    if (success) {
        uint32_t added_speed_bonus = 5;  // TODO: 从鞍具定义获取

        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("装备成功");
        response.set_mount_id(mount_id);
        response.set_saddle_id(saddle_id);
        response.set_added_speed_bonus(added_speed_bonus);

        spdlog::info("[MountHandler] Mount saddle equipped: mount_id={}, saddle_id={}",
                     mount_id, saddle_id);
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("装备失败");
        response.set_mount_id(0);
        response.set_saddle_id(0);
        response.set_added_speed_bonus(0);

        spdlog::warn("[MountHandler] Failed to equip saddle: mount_id={}", mount_id);
    }

    // 发送响应
    SendResponse(conn, 0x140A, response);
}

void MountHandler::HandleUnequipMountSaddleRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[MountHandler] HandleUnequipMountSaddleRequest");

    // 解析请求
    murim::UnequipMountSaddleRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[MountHandler] Failed to parse UnequipMountSaddleRequest");
        return;
    }

    uint32_t mount_id = request.mount_id();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 卸下鞍具
    auto& mount_manager = MountManager::Instance();
    bool success = mount_manager.UnequipSaddle(character_id, mount_id);

    // 构建响应
    murim::UnequipMountSaddleResponse response;
    if (success) {
        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("卸下成功");
        response.set_mount_id(mount_id);

        spdlog::info("[MountHandler] Mount saddle unequipped: mount_id={}", mount_id);
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("卸下失败");
        response.set_mount_id(0);

        spdlog::warn("[MountHandler] Failed to unequip saddle: mount_id={}", mount_id);
    }

    // 发送响应
    SendResponse(conn, 0x140C, response);
}

void MountHandler::HandleEquipMountDecorationRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[MountHandler] HandleEquipMountDecorationRequest");

    // 解析请求
    murim::EquipMountDecorationRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[MountHandler] Failed to parse EquipMountDecorationRequest");
        return;
    }

    uint32_t mount_id = request.mount_id();
    uint32_t decoration_id = request.decoration_id();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 装备装饰
    auto& mount_manager = MountManager::Instance();
    bool success = mount_manager.EquipDecoration(character_id, mount_id, decoration_id);

    // 构建响应
    murim::EquipMountDecorationResponse response;
    if (success) {
        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("装备成功");
        response.set_mount_id(mount_id);
        response.set_decoration_id(decoration_id);

        spdlog::info("[MountHandler] Mount decoration equipped: mount_id={}, decoration_id={}",
                     mount_id, decoration_id);
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("装备失败");
        response.set_mount_id(0);
        response.set_decoration_id(0);

        spdlog::warn("[MountHandler] Failed to equip decoration: mount_id={}", mount_id);
    }

    // 发送响应
    SendResponse(conn, 0x140E, response);
}

void MountHandler::HandleUnequipMountDecorationRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[MountHandler] HandleUnequipMountDecorationRequest");

    // 解析请求
    murim::UnequipMountDecorationRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[MountHandler] Failed to parse UnequipMountDecorationRequest");
        return;
    }

    uint32_t mount_id = request.mount_id();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 卸下装饰
    auto& mount_manager = MountManager::Instance();
    bool success = mount_manager.UnequipDecoration(character_id, mount_id);

    // 构建响应
    murim::UnequipMountDecorationResponse response;
    if (success) {
        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("卸下成功");
        response.set_mount_id(mount_id);

        spdlog::info("[MountHandler] Mount decoration unequipped: mount_id={}", mount_id);
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("卸下失败");
        response.set_mount_id(0);

        spdlog::warn("[MountHandler] Failed to unequip decoration: mount_id={}", mount_id);
    }

    // 发送响应
    SendResponse(conn, 0x1410, response);
}

} // namespace Game
} // namespace Murim
