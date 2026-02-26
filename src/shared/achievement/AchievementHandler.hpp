#pragma once

#include <memory>
#include <cstdint>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "AchievementManager.hpp"

namespace google {
namespace protobuf {
class Message;
}
}

namespace Murim {
namespace Game {

/**
 * @brief 成就系统消息处理器
 *
 * 处理所有成就相关的网络消息
 */
class AchievementHandler {
public:
    /**
     * @brief 处理获取成就列表请求
     */
    static void HandleGetAchievementListRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理获取成就详情请求
     */
    static void HandleGetAchievementDetailRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理获取我的成就数据请求
     */
    static void HandleGetMyAchievementRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理领取成就奖励请求
     */
    static void HandleClaimAchievementRewardRequest(
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
