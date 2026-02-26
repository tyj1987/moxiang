#pragma once

#include <memory>
#include <cstdint>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "MountManager.hpp"

namespace google {
namespace protobuf {
class Message;
}
}

namespace Murim {
namespace Game {

/**
 * @brief 坐骑系统消息处理器
 *
 * 处理所有坐骑相关的网络消息
 */
class MountHandler {
public:
    /**
     * @brief 处理获取坐骑列表请求
     */
    static void HandleGetMountListRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理获取坐骑详情请求
     */
    static void HandleGetMountDetailRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理骑乘/下马请求
     */
    static void HandleRideMountRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理进化坐骑请求
     */
    static void HandleEvolveMountRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理装备鞍具请求
     */
    static void HandleEquipMountSaddleRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理卸下鞍具请求
     */
    static void HandleUnequipMountSaddleRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理装备装饰请求
     */
    static void HandleEquipMountDecorationRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理卸下装饰请求
     */
    static void HandleUnequipMountDecorationRequest(
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
