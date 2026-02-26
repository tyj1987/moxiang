#include "NPCHandler.hpp"
#include "core/network/PacketSerializer.hpp"
#include "protocol/npc.pb.h"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

void NPCHandler::HandleNPCInteractRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::network::NPCInteractRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse NPCInteractRequest");
        return;
    }

    spdlog::info("HandleNPCInteractRequest: player_id={}, npc_id={}",
                 request.player_id(), request.npc_id());

    // 处理交互
    auto result = NPCManager::Instance().HandleInteraction(
        request.player_id(),
        request.npc_id()
    );

    // 发送响应
    SendNPCInteractResponse(conn, result);
}

void NPCHandler::HandleStartDialogueRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::network::StartDialogueRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse StartDialogueRequest");
        return;
    }

    spdlog::info("HandleStartDialogueRequest: player_id={}, npc_id={}",
                 request.player_id(), request.npc_id());

    // 获取NPC信息以确定对话ID
    auto npc = NPCManager::Instance().GetNPC(request.npc_id());
    if (!npc) {
        spdlog::warn("NPC {} not found", request.npc_id());
        // TODO: 发送错误响应
        return;
    }

    // 获取对话节点
    auto dialogue = NPCManager::Instance().GetDialogue(npc->default_dialogue_id);
    if (!dialogue) {
        spdlog::warn("Dialogue {} not found", npc->default_dialogue_id);
        // TODO: 发送错误响应
        return;
    }

    // 发送对话响应
    SendDialogueResponse(conn, dialogue);
}

void NPCHandler::HandleSelectDialogueOptionRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析请求
    murim::network::SelectDialogueOptionRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse SelectDialogueOptionRequest");
        return;
    }

    spdlog::info("HandleSelectDialogueOptionRequest: player_id={}, dialogue_id={}, option_id={}",
                 request.player_id(), request.dialogue_id(), request.option_id());

    // 获取当前对话节点
    auto dialogue = NPCManager::Instance().GetDialogue(request.dialogue_id());
    if (!dialogue) {
        spdlog::warn("Dialogue {} not found", request.dialogue_id());
        return;
    }

    // 查找选项
    const DialogueNode::DialogueOption* selected_option = nullptr;
    for (const auto& option : dialogue->options) {
        if (option.option_id == request.option_id()) {
            selected_option = &option;
            break;
        }
    }

    if (!selected_option) {
        spdlog::warn("Option {} not found in dialogue {}", request.option_id(), request.dialogue_id());
        return;
    }

    // 处理选项
    if (selected_option->next_dialogue_id != 0) {
        // 跳转到下一个对话
        auto next_dialogue = NPCManager::Instance().GetDialogue(selected_option->next_dialogue_id);
        if (next_dialogue) {
            SendDialogueResponse(conn, next_dialogue);
        }
    } else {
        // 执行动作
        switch (selected_option->action_type) {
            case 1:  // 接受任务
                spdlog::info("Player {} accepts quest {}",
                            request.player_id(), selected_option->quest_id);
                // TODO: 调用QuestManager接受任务
                break;
            case 2:  // 打开商店
                spdlog::info("Player {} opens shop: {}",
                            request.player_id(), selected_option->action_data);
                // TODO: 调用ShopManager打开商店
                break;
            case 3:  // 传送
                spdlog::info("Player {} teleports: {}",
                            request.player_id(), selected_option->action_data);
                // TODO: 处理传送
                break;
            default:
                // 结束对话
                break;
        }
    }
}

void NPCHandler::HandleEndDialogueNotify(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    // 解析通知
    murim::network::EndDialogueNotify notify;
    if (!notify.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("Failed to parse EndDialogueNotify");
        return;
    }

    spdlog::info("HandleEndDialogueNotify: player_id={}", notify.player_id());

    // 清理对话状态
    // TODO: 清理玩家的对话状态
}

void NPCHandler::SendNPCInteractResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    const NPCManager::InteractResult& result) {

    murim::network::NPCInteractResponse response;
    response.set_result_code(result.success ? 0 : 1);

    if (result.npc) {
        auto* npc_info = response.mutable_npc();
        npc_info->set_npc_id(result.npc->npc_id);
        npc_info->set_npc_name(result.npc->npc_name);
        npc_info->set_map_id(result.npc->map_id);
        npc_info->set_position_x(result.npc->position_x);
        npc_info->set_position_y(result.npc->position_y);
        npc_info->set_position_z(result.npc->position_z);
        npc_info->set_functions(result.npc->functions);
        response.set_dialogue_id(result.dialogue_id);
    }

    // Note: NPCInteractResponse proto doesn't have a message field
    // Error handling will be done through result_code only

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0602 = NPC/NPCInteractResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0602, data);
    if (!buffer) {
        spdlog::error("Failed to serialize NPCInteractResponse");
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send NPCInteractResponse: {}", ec.message());
        } else {
            spdlog::debug("NPCInteractResponse sent: {} bytes", bytes_sent);
        }
    });
}

void NPCHandler::SendDialogueResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    const std::shared_ptr<DialogueNode>& dialogue) {

    murim::network::StartDialogueResponse response;
    response.set_result_code(0);

    auto* dialogue_info = response.mutable_dialogue();
    dialogue_info->set_dialogue_id(dialogue->dialogue_id);
    dialogue_info->set_npc_text(dialogue->npc_text);
    dialogue_info->set_npc_id(dialogue->npc_id);
    dialogue_info->set_voice_id(dialogue->voice_id);
    dialogue_info->set_animation_id(dialogue->animation_id);

    // 添加选项
    for (const auto& option : dialogue->options) {
        auto* option_info = dialogue_info->add_options();
        option_info->set_option_id(option.option_id);
        option_info->set_text(option.text);
        option_info->set_next_dialogue_id(option.next_dialogue_id);
        option_info->set_quest_id(option.quest_id);
        option_info->set_action_type(option.action_type);
        option_info->set_action_data(option.action_data);
    }

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    response.SerializeToArray(data.data(), data.size());

    // 创建数据包 (消息 ID: 0x0802 = Dialogue/StartDialogueResponse)
    auto buffer = Core::Network::PacketSerializer::Serialize(0x0802, data);
    if (!buffer) {
        spdlog::error("Failed to serialize StartDialogueResponse");
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("Failed to send StartDialogueResponse: {}", ec.message());
        } else {
            spdlog::debug("StartDialogueResponse sent: {} bytes", bytes_sent);
        }
    });
}

} // namespace Game
} // namespace Murim
