#pragma once

#include <memory>
#include <cstdint>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "TradeManager.hpp"

namespace google {
namespace protobuf {
class Message;
}
}

namespace Murim {
namespace Game {

/**
 * @brief 交易系统消息处理器
 *
 * 处理所有交易相关的网络消息
 */
class TradeHandler {
public:
    /**
     * @brief 处理发起交易请求
     */
    static void HandleRequestTradeRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理响应交易请求
     */
    static void HandleRespondTradeRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理添加交易物品
     */
    static void HandleAddTradeItemRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理移除交易物品
     */
    static void HandleRemoveTradeItemRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理添加交易金币
     */
    static void HandleAddTradeGoldRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理移除交易金币
     */
    static void HandleRemoveTradeGoldRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理确认交易
     */
    static void HandleConfirmTradeRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理取消交易
     */
    static void HandleCancelTradeRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

private:
    static void SendResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        uint16_t message_id,
        const google::protobuf::Message& response);

    static murim::TradeItem ConvertToProto(const TradeItem& item);
    static TradeItem ConvertFromProto(const murim::TradeItem& proto);
    static murim::TradeSession ConvertToProto(const TradeSession& session);
};

} // namespace Game
} // namespace Murim
