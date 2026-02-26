#pragma once

#include "../../shared/item/MixManager.hpp"
#include "../session/PlayerSession.hpp"
#include <memory>

namespace Murim {
namespace MapServer {
namespace Handlers {

/**
 * @brief 物品消息处理器
 *
 * 处理所有物品相关的客户端请求，包括：
 * - 物品组合 (Mix)
 * - 背包移动
 * - 物品丢弃
 * - 物品拆分
 */
class ItemMessageHandler {
public:
    static ItemMessageHandler& Instance();

    /**
     * @brief 处理物品组合请求
     *
     * 对应协议: MP_ITEM_MIX_SYN
     * Protocol Buffer: MixItemSyn
     *
     * @param session 玩家会话
     * @param message 消息内容
     */
    void HandleMixItemSyn(
        std::shared_ptr<PlayerSession> session,
        const std::vector<uint8_t>& message
    );

    /**
     * @brief 处理背包物品移动请求
     *
     * 对应协议: MP_ITEM_MOVE_SYN
     * Protocol Buffer: MoveInventoryItemSyn
     *
     * @param session 玩家会话
     * @param message 消息内容
     */
    void HandleMoveInventoryItemSyn(
        std::shared_ptr<PlayerSession> session,
        const std::vector<uint8_t>& message
    );

    /**
     * @brief 处理丢弃物品请求
     *
     * 对应协议: MP_ITEM_DROP_SYN
     * Protocol Buffer: DropItemSyn
     *
     * @param session 玩家会话
     * @param message 消息内容
     */
    void HandleDropItemSyn(
        std::shared_ptr<PlayerSession> session,
        const std::vector<uint8_t>& message
    );

    /**
     * @brief 处理物品拆分请求
     *
     * 对应协议: MP_ITEM_SPLIT_SYN
     * Protocol Buffer: SplitItemSyn
     *
     * @param session 玩家会话
     * @param message 消息内容
     */
    void HandleSplitItemSyn(
        std::shared_ptr<PlayerSession> session,
        const std::vector<uint8_t>& message
    );

private:
    ItemMessageHandler() = default;
    ~ItemMessageHandler() = default;
    ItemMessageHandler(const ItemMessageHandler&) = delete;
    ItemMessageHandler& operator=(const ItemMessageHandler&) = delete;

    /**
     * @brief 发送物品组合成功响应
     */
    void SendMixItemSuccessAck(
        std::shared_ptr<PlayerSession> session,
        uint64_t result_item_instance_id,
        uint32_t result_item_idx,
        uint16_t result_position,
        uint32_t durability,
        uint32_t rare_idx
    );

    /**
     * @brief 发送物品组合失败消息
     */
    void SendMixMsg(
        std::shared_ptr<PlayerSession> session,
        Game::ItemMix::MixResultCode result_code
    );

    /**
     * @brief 发送物品组合响应 (通用)
     */
    void SendMixItemAck(
        std::shared_ptr<PlayerSession> session,
        Game::ItemMix::MixResultCode result_code,
        uint64_t result_item_instance_id = 0,
        uint32_t result_item_idx = 0,
        uint16_t result_position = 0,
        uint32_t durability = 0,
        uint32_t rare_idx = 0
    );
};

} // namespace Handlers
} // namespace MapServer
} // namespace Murim
