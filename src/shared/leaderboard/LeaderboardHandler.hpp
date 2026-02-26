#pragma once

#include <memory>
#include <cstdint>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "LeaderboardManager.hpp"

namespace google {
namespace protobuf {
class Message;
}
}

namespace Murim {
namespace Game {

/**
 * @brief 排行榜系统消息处理器
 *
 * 处理所有排行榜相关的网络消息
 */
class LeaderboardHandler {
public:
    /**
     * @brief 处理获取排行榜请求
     */
    static void HandleGetLeaderboardRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理获取我的排名请求
     */
    static void HandleGetMyRankRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理搜索排行请求
     */
    static void HandleSearchLeaderboardRequest(
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
