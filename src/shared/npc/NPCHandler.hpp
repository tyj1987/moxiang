#pragma once

#include <memory>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "NPCManager.hpp"

namespace Murim {
namespace Game {

/**
 * @brief NPC交互处理器
 *
 * 职责：
 * - 处理NPC交互请求 (0x0601)
 * - 处理对话请求 (0x0801, 0x0803)
 * - 发送NPC交互响应 (0x0602)
 * - 发送对话响应 (0x0802, 0x0804)
 */
class NPCHandler {
public:
    /**
     * @brief 处理NPC交互请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleNPCInteractRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理开始对话请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleStartDialogueRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理选择对话选项请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleSelectDialogueOptionRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理结束对话通知
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleEndDialogueNotify(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

private:
    /**
     * @brief 发送NPC交互响应
     * @param conn 客户端连接
     * @param result 交互结果
     */
    static void SendNPCInteractResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        const NPCManager::InteractResult& result);

    /**
     * @brief 发送对话响应
     * @param conn 客户端连接
     * @param dialogue 对话节点
     */
    static void SendDialogueResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        const std::shared_ptr<DialogueNode>& dialogue);
};

} // namespace Game
} // namespace Murim
