#include "ItemMessageHandler.hpp"
#include "../../protocol/item.pb.h"
#include "core/spdlog_wrapper.hpp"
#include <stdexcept>

namespace Murim {
namespace MapServer {
namespace Handlers {

using namespace Game::ItemMix;

ItemMessageHandler& ItemMessageHandler::Instance() {
    static ItemMessageHandler instance;
    return instance;
}

void ItemMessageHandler::HandleMixItemSyn(
    std::shared_ptr<PlayerSession> session,
    const std::vector<uint8_t>& message
) {
    if (!session) {
        spdlog::error("HandleMixItemSyn: session is null");
        return;
    }

    // 解析 Protocol Buffer 消息
    protocol::MixItemSyn syn_msg;
    if (!syn_msg.ParseFromArray(message.data(), message.size())) {
        spdlog::error("HandleMixItemSyn: failed to parse message");
        return;
    }

    uint64_t character_id = session->GetCharacterId();

    spdlog::info("HandleMixItemSyn: character_id={}, base_item_idx={}, result_index={}",
                 character_id, syn_msg.base_item_idx(), syn_msg.result_index());

    // 构建材料列表
    std::vector<MixMaterial> materials;
    materials.reserve(syn_msg.materials_size());

    for (const auto& material_proto : syn_msg.materials()) {
        materials.emplace_back(
            material_proto.item_idx(),
            material_proto.position(),
            material_proto.durability()
        );
    }

    // 调用 MixManager 执行物品组合
    auto result_code = MixManager::Instance().MixItem(
        character_id,
        syn_msg.base_item_idx(),
        syn_msg.base_item_pos(),
        syn_msg.result_index(),
        materials,
        syn_msg.money(),
        syn_msg.shop_item_idx(),
        syn_msg.shop_item_pos()
    );

    spdlog::info("MixItem result: {}", static_cast<int>(result_code));

    // 根据结果码发送相应的响应
    switch (result_code) {
        case MixResultCode::kSuccess: {
            // TODO: 获取新创建的物品实例信息
            // 目前先发送成功响应，实际需要从 MixManager 返回物品信息
            SendMixItemSuccessAck(
                session,
                0,  // result_item_instance_id (需要从 MixManager 获取)
                0,  // result_item_idx
                0,  // result_position
                0,  // durability
                0   // rare_idx
            );
            break;
        }

        case MixResultCode::kBigFail:
        case MixResultCode::kSmallFail:
        case MixResultCode::kBigFailWithProtection:
        case MixResultCode::kSmallFailWithProtection:
            // 发送失败消息
            SendMixMsg(session, result_code);
            break;

        default:
            // 其他错误码，发送通用响应
            SendMixItemAck(session, result_code);
            break;
    }
}

void ItemMessageHandler::HandleMoveInventoryItemSyn(
    std::shared_ptr<PlayerSession> session,
    const std::vector<uint8_t>& message
) {
    if (!session) {
        spdlog::error("HandleMoveInventoryItemSyn: session is null");
        return;
    }

    // 解析消息
    protocol::MoveInventoryItemSyn syn_msg;
    if (!syn_msg.ParseFromArray(message.data(), message.size())) {
        spdlog::error("HandleMoveInventoryItemSyn: failed to parse message");
        return;
    }

    uint64_t character_id = session->GetCharacterId();

    spdlog::debug("HandleMoveInventoryItemSyn: character_id={}, from_slot={}, to_slot={}",
                  character_id, syn_msg.from_slot(), syn_msg.to_slot());

    // TODO: 实现物品移动逻辑
    // 1. 验证源槽位和目标槽位有效性
    // 2. 获取源槽位物品
    // 3. 移动到目标槽位
    // 4. 更新数据库
    // 5. 发送响应

    protocol::MoveInventoryItemAck ack_msg;
    ack_msg.set_result_code(0);  // 成功
    ack_msg.set_from_slot(syn_msg.from_slot());
    ack_msg.set_to_slot(syn_msg.to_slot());

    // TODO: 发送响应给客户端
    // session->SendMessage(protocol::MessageCategory::ITEM, protocol::MessageCode::MOVE_INVENTORY_ITEM_ACK, ack_msg);
}

void ItemMessageHandler::HandleDropItemSyn(
    std::shared_ptr<PlayerSession> session,
    const std::vector<uint8_t>& message
) {
    if (!session) {
        spdlog::error("HandleDropItemSyn: session is null");
        return;
    }

    // 解析消息
    protocol::DropItemSyn syn_msg;
    if (!syn_msg.ParseFromArray(message.data(), message.size())) {
        spdlog::error("HandleDropItemSyn: failed to parse message");
        return;
    }

    uint64_t character_id = session->GetCharacterId();

    spdlog::debug("HandleDropItemSyn: character_id={}, item_instance_id={}, slot={}, quantity={}",
                  character_id, syn_msg.item_instance_id(), syn_msg.slot_index(), syn_msg.quantity());

    // TODO: 实现丢弃物品逻辑
    // 1. 验证物品存在
    // 2. 从背包移除
    // 3. 创建地面物品对象
    // 4. 更新数据库
    // 5. 发送响应

    protocol::DropItemAck ack_msg;
    ack_msg.set_result_code(0);  // 成功
    ack_msg.set_item_instance_id(syn_msg.item_instance_id());

    // TODO: 发送响应给客户端
    // session->SendMessage(protocol::MessageCategory::ITEM, protocol::MessageCode::DROP_ITEM_ACK, ack_msg);
}

void ItemMessageHandler::HandleSplitItemSyn(
    std::shared_ptr<PlayerSession> session,
    const std::vector<uint8_t>& message
) {
    if (!session) {
        spdlog::error("HandleSplitItemSyn: session is null");
        return;
    }

    // 解析消息
    protocol::SplitItemSyn syn_msg;
    if (!syn_msg.ParseFromArray(message.data(), message.size())) {
        spdlog::error("HandleSplitItemSyn: failed to parse message");
        return;
    }

    uint64_t character_id = session->GetCharacterId();

    spdlog::debug("HandleSplitItemSyn: character_id={}, source_slot={}, target_slot={}, quantity={}",
                  character_id, syn_msg.source_slot(), syn_msg.target_slot(), syn_msg.quantity());

    // TODO: 实现物品拆分逻辑
    // 1. 验证源物品可堆叠
    // 2. 验证数量足够
    // 3. 验证目标槽位为空
    // 4. 执行拆分
    // 5. 更新数据库
    // 6. 发送响应

    protocol::SplitItemAck ack_msg;
    ack_msg.set_result_code(0);  // 成功
    ack_msg.set_source_slot(syn_msg.source_slot());
    ack_msg.set_target_slot(syn_msg.target_slot());

    // TODO: 发送响应给客户端
    // session->SendMessage(protocol::MessageCategory::ITEM, protocol::MessageCode::SPLIT_ITEM_ACK, ack_msg);
}

void ItemMessageHandler::SendMixItemSuccessAck(
    std::shared_ptr<PlayerSession> session,
    uint64_t result_item_instance_id,
    uint32_t result_item_idx,
    uint16_t result_position,
    uint32_t durability,
    uint32_t rare_idx
) {
    protocol::MixItemSuccessAck ack_msg;
    ack_msg.set_result_item_instance_id(result_item_instance_id);
    ack_msg.set_result_item_idx(result_item_idx);
    ack_msg.set_result_position(result_position);
    ack_msg.set_durability(durability);
    ack_msg.set_rare_idx(rare_idx);

    spdlog::info("SendMixItemSuccessAck: instance_id={}, item_idx={}, pos={}, dur={}, rare={}",
                 result_item_instance_id, result_item_idx, result_position, durability, rare_idx);

    // TODO: 发送消息给客户端
    // session->SendMessage(protocol::MessageCategory::ITEM, protocol::MessageCode::MIX_ITEM_SUCCESS_ACK, ack_msg);
}

void ItemMessageHandler::SendMixMsg(
    std::shared_ptr<PlayerSession> session,
    MixResultCode result_code
) {
    protocol::MixMsg msg;
    msg.set_data1(static_cast<uint32_t>(result_code));
    msg.set_data2(0);  // 预留

    spdlog::info("SendMixMsg: result_code={}", static_cast<int>(result_code));

    // TODO: 发送消息给客户端
    // session->SendMessage(protocol::MessageCategory::ITEM, protocol::MessageCode::MIX_MSG, msg);
}

void ItemMessageHandler::SendMixItemAck(
    std::shared_ptr<PlayerSession> session,
    MixResultCode result_code,
    uint64_t result_item_instance_id,
    uint32_t result_item_idx,
    uint16_t result_position,
    uint32_t durability,
    uint32_t rare_idx
) {
    protocol::MixItemAck ack_msg;
    ack_msg.set_result_code(static_cast<int32_t>(result_code));

    if (result_code == MixResultCode::kSuccess) {
        ack_msg.set_result_item_instance_id(result_item_instance_id);
        ack_msg.set_result_item_idx(result_item_idx);
        ack_msg.set_result_position(result_position);
        ack_msg.set_durability(durability);
        ack_msg.set_rare_idx(rare_idx);
    }

    spdlog::info("SendMixItemAck: result_code={}", static_cast<int>(result_code));

    // TODO: 发送消息给客户端
    // session->SendMessage(protocol::MessageCategory::ITEM, protocol::MessageCode::MIX_ITEM_ACK, ack_msg);
}

} // namespace Handlers
} // namespace MapServer
} // namespace Murim
