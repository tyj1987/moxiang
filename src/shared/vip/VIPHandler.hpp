#pragma once

#include <memory>
#include <cstdint>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "VIPManager.hpp"

namespace google {
namespace protobuf {
class Message;
}
}

namespace Murim {
namespace Game {

/**
 * @brief VIP系统消息处理器
 *
 * 处理所有VIP相关的网络消息
 */
class VIPHandler {
public:
    /**
     * @brief 处理获取VIP信息请求
     */
    static void HandleGetVIPInfoRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理领取等级礼包请求
     */
    static void HandleClaimVIPLevelGiftRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理领取每日礼包请求
     */
    static void HandleClaimVIPDailyGiftRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理领取每月礼包请求
     */
    static void HandleClaimVIPMonthlyGiftRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理充值请求
     */
    static void HandleRechargeRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理获取VIP特权列表请求
     */
    static void HandleGetVIPPrivilegesRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

private:
    static void SendResponse(
        std::shared_ptr<Core::Network::Connection> conn,
        uint16_t message_id,
        const google::protobuf::Message& response);
};

} // namespace Game
} // namespace Murim
