#pragma once

#include <memory>
#include <cstdint>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "ShopManager.hpp"  // For TradeResult

namespace Murim {
namespace Game {

class ShopHandler {
public:
    /**
     * @brief 处理打开商店请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleOpenShopRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理购买请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleBuyItemRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理出售请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleSellItemRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理关闭商店通知
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleCloseShopNotify(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

private:
    /**
     * @brief 发送打开商店响应
     * @param conn 客户端连接
     * @param result_code 结果码 (0=成功, 1=商店不存在, 2=其他错误)
     * @param shop_id 商店ID
     */
    static void SendOpenShopResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        int32_t result_code,
        uint32_t shop_id);

    /**
     * @brief 发送购买响应
     * @param conn 客户端连接
     * @param result 交易结果
     */
    static void SendBuyItemResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        const TradeResult& result);

    /**
     * @brief 发送出售响应
     * @param conn 客户端连接
     * @param result 交易结果
     */
    static void SendSellItemResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        const TradeResult& result);
};

} // namespace Game
} // namespace Murim
