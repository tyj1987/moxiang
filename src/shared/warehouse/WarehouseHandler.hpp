#pragma once

#include <memory>
#include <cstdint>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "WarehouseManager.hpp"

namespace Murim {
namespace Game {

class WarehouseHandler {
public:
    /**
     * @brief 处理打开仓库请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleOpenWarehouseRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理存入物品请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleDepositItemRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理取出物品请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleWithdrawItemRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理整理仓库请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleSortWarehouseRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理关闭仓库通知
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    static void HandleCloseWarehouseNotify(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

private:
    /**
     * @brief 发送打开仓库响应
     * @param conn 客户端连接
     * @param result_code 结果码 (0=成功, 1=仓库不存在, 2=其他错误)
     * @param character_id 角色ID
     */
    static void SendOpenWarehouseResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        int32_t result_code,
        uint64_t character_id);

    /**
     * @brief 发送存入物品响应
     * @param conn 客户端连接
     * @param result 仓库操作结果
     */
    static void SendDepositItemResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        const WarehouseResult& result);

    /**
     * @brief 发送取出物品响应
     * @param conn 客户端连接
     * @param result 仓库操作结果
     */
    static void SendWithdrawItemResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        const WarehouseResult& result);

    /**
     * @brief 发送整理仓库响应
     * @param conn 客户端连接
     * @param result_code 结果码
     */
    static void SendSortWarehouseResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        int32_t result_code);
};

} // namespace Game
} // namespace Murim
