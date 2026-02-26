// Murim MMORPG - NPC Message Handler
// NPC交互消息处理器实现示例

#pragma once

#include "protocol/MessageHelper.hpp"
#include "managers/NPCManager.hpp"
#include "managers/ShopManager.hpp"
#include "managers/DialogueManager.hpp"
#include "managers/WarehouseManager.hpp"
#include "core/network/Connection.hpp"
#include "core/spdlog_wrapper.hpp"
#include "npc.pb.h"  // Protocol Buffers 生成的头文件
#include <memory>

namespace Murim {
namespace MapServer {
namespace Handlers {

/**
 * @brief NPC交互请求处理器
 *
 * 处理客户端与NPC交互的请求
 */
class NPCInteractRequestHandler : public Protocol::MessageHandlerBase {
public:
    NPCInteractRequestHandler(
        NPCManager& npc_manager,
        ShopManager& shop_manager,
        DialogueManager& dialogue_manager,
        WarehouseManager& warehouse_manager
    ) : npc_manager_(npc_manager)
      , shop_manager_(shop_manager)
      , dialogue_manager_(dialogue_manager)
      , warehouse_manager_(warehouse_manager)
    {}

    /**
     * @brief 处理NPC交互请求
     */
    bool HandleMessage(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer
    ) override {
        // 1. 解析消息
        murim::network::NPCInteractRequest request;
        auto parse_result = Protocol::MessageHelper::ParseMessage(buffer, request);
        if (!parse_result.success) {
            spdlog::error("Failed to parse NPCInteractRequest: {}", parse_result.error_message);
            return false;
        }

        // 2. 提取参数
        uint64_t player_id = request.player_id();
        uint32_t npc_id = request.npc_id();

        spdlog::info("Processing NPC interact request: player_id={}, npc_id={}", player_id, npc_id);

        // 3. 业务逻辑处理
        auto result = ProcessNPCInteract(conn, player_id, npc_id);

        // 4. 发送响应
        return SendResponse(conn, result);
    }

    uint16_t GetMessageId() const override {
        return 0x0601;  // NPC_INTERACT_REQUEST
    }

private:
    /**
     * @brief 处理NPC交互逻辑
     */
    struct ProcessResult {
        uint32_t result_code;       // 0=成功, 1=失败, 2=NPC不存在, 7=距离太远
        uint32_t npc_id;
        uint32_t dialogue_id;       // 默认对话ID
        std::string message;
    };

    ProcessResult ProcessNPCInteract(
        std::shared_ptr<Core::Network::Connection> conn,
        uint64_t player_id,
        uint32_t npc_id
    ) {
        ProcessResult result;
        result.npc_id = npc_id;
        result.dialogue_id = 0;
        result.result_code = 0;

        // 1. 检查NPC是否存在
        auto npc = npc_manager_.GetNPC(npc_id);
        if (!npc) {
            spdlog::warn("NPC {} not found", npc_id);
            result.result_code = 2;
            result.message = "NPC not found";
            return result;
        }

        // 2. TODO: 检查玩家是否在线
        // auto player = character_manager_.GetPlayer(player_id);
        // if (!player) {
        //     result.result_code = 1;
        //     result.message = "Player not found";
        //     return result;
        // }

        // 3. TODO: 计算距离
        // float distance = CalculateDistance(player->GetPosition(), npc->GetPosition());
        // if (distance > 700.0f) {
        //     result.result_code = 7;
        //     result.message = "Too far from NPC";
        //     return result;
        // }

        spdlog::info("Player {} interacting with NPC {}: {}",
            player_id, npc_id, npc->npc_name);

        // 4. 获取NPC功能
        uint32_t functions = npc->functions;
        spdlog::info("NPC functions: 0x{:08X}", functions);

        // 5. 根据功能触发相应操作
        if (functions & static_cast<uint32_t>(NPCFunction::kShop)) {
            // 自动打开商店
            auto shop_ids = shop_manager_.GetShopIdsByNPC(npc_id);
            if (!shop_ids.empty()) {
                shop_manager_.OpenShop(player_id, shop_ids[0]);
                spdlog::info("Opened shop {} for player {}", shop_ids[0], player_id);
            }
        }

        if (functions & static_cast<uint32_t>(NPCFunction::kWarehouse)) {
            // 自动打开仓库
            warehouse_manager_.OpenWarehouse(player_id, npc_id);
            spdlog::info("Opened warehouse for player {}", player_id);
        }

        // 6. 设置对话ID（如果有对话功能）
        if (functions & static_cast<uint32_t>(NPCFunction::kQuest)) {
            result.dialogue_id = npc->dialogue_id;
            if (result.dialogue_id != 0) {
                // 开始对话
                dialogue_manager_.StartDialogue(player_id, npc_id);
                spdlog::info("Started dialogue {} for player {}", result.dialogue_id, player_id);
            }
        }

        result.result_code = 0;
        result.message = "Success";

        return result;
    }

    /**
     * @brief 发送NPC交互响应
     */
    bool SendResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        const ProcessResult& result
    ) {
        // 构造响应消息
        murim::network::NPCInteractResponse response;
        response.set_result_code(result.result_code);
        response.mutable_npc()->set_npc_id(result.npc_id);
        response.set_dialogue_id(result.dialogue_id);

        // 发送消息
        return Protocol::MessageSender::SendMessage(conn, 0x0602, response);
    }

    NPCManager& npc_manager_;
    ShopManager& shop_manager_;
    DialogueManager& dialogue_manager_;
    WarehouseManager& warehouse_manager_;
};

/**
 * @brief 打开商店请求处理器
 */
class OpenShopRequestHandler : public Protocol::MessageHandlerBase {
public:
    OpenShopRequestHandler(ShopManager& shop_manager)
        : shop_manager_(shop_manager)
    {}

    bool HandleMessage(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer
    ) override {
        murim::network::OpenShopRequest request;
        auto parse_result = Protocol::MessageHelper::ParseMessage(buffer, request);
        if (!parse_result.success) {
            spdlog::error("Failed to parse OpenShopRequest: {}", parse_result.error_message);
            return false;
        }

        uint64_t player_id = request.player_id();
        uint32_t shop_id = request.shop_id();

        spdlog::info("Processing open shop request: player_id={}, shop_id={}", player_id, shop_id);

        // 业务逻辑
        auto result = ProcessOpenShop(conn, player_id, shop_id);

        // 发送响应
        return SendResponse(conn, result);
    }

    uint16_t GetMessageId() const override {
        return 0x0701;
    }

private:
    struct ProcessResult {
        uint32_t result_code;
        uint32_t shop_id;
        std::string shop_name;
        std::vector<murim::network::ShopItemInfo> items;
        uint64_t player_gold;
    };

    ProcessResult ProcessOpenShop(
        std::shared_ptr<Core::Network::Connection> conn,
        uint64_t player_id,
        uint32_t shop_id
    ) {
        ProcessResult result;
        result.shop_id = shop_id;
        result.result_code = 0;

        // TODO: 实现商店打开逻辑
        // 1. 验证商店存在
        // 2. 验证玩家在NPC附近
        // 3. ShopManager::OpenShop()
        // 4. 获取商店物品列表
        // 5. 获取玩家金币

        spdlog::info("Shop {} opened for player {}", shop_id, player_id);

        return result;
    }

    bool SendResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        const ProcessResult& result
    ) {
        murim::network::OpenShopResponse response;
        response.set_result_code(result.result_code);
        response.set_shop_id(result.shop_id);
        response.set_shop_name(result.shop_name);
        response.set_player_gold(result.player_gold);

        for (const auto& item : result.items) {
            auto* shop_item = response.add_items();
            *shop_item = item;
        }

        return Protocol::MessageSender::SendMessage(conn, 0x0702, response);
    }

    ShopManager& shop_manager_;
};

} // namespace Handlers
} // namespace MapServer
} // namespace Murim
