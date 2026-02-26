#pragma once

#include <memory>
#include <cstdint>
#include <vector>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "MarketManager.hpp"

namespace google {
namespace protobuf {
class Message;
}
}

namespace Murim {
namespace Game {

/**
 * @brief 市场系统消息处理器
 *
 * 处理所有市场相关的网络消息
 */
class MarketHandler {
public:
    /**
     * @brief 处理上架物品请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleListItemRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理下架物品请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleDelistItemRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理购买物品请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleBuyItemRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理出价请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleBidItemRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理搜索市场请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleSearchMarketRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理获取我的挂单请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleGetMyListingsRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理获取挂单详情请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleGetListingDetailsRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

private:
    /**
     * @brief 发送简单响应
     * @param conn 客户端连接
     * @param message_id 消息ID
     * @param result_code 结果码
     * @param data 响应数据
     */
    static void SendResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        uint16_t message_id,
        uint32_t result_code,
        const std::vector<uint8_t>& data);

    /**
     * @brief 发送 Protobuf 响应
     * @param conn 客户端连接
     * @param message_id 消息ID
     * @param response Protobuf 响应消息
     */
    static void SendResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        uint16_t message_id,
        const google::protobuf::Message& response);

    /**
     * @brief 转换 MarketListing 为 Proto 格式
     * @param listing 挂单信息
     * @return Proto 挂单信息
     */
    static murim::MarketListing ConvertToProto(const MarketListing& listing);
};

} // namespace Game
} // namespace Murim
