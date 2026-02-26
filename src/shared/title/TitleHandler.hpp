#pragma once

#include <memory>
#include <cstdint>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "TitleManager.hpp"

namespace google {
namespace protobuf {
class Message;
}
}

namespace Murim {
namespace Game {

/**
 * @brief 称号系统消息处理器
 *
 * 处理所有称号相关的网络消息
 */
class TitleHandler {
public:
    /**
     * @brief 处理获取称号列表请求
     */
    static void HandleGetTitleListRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理获取称号详情请求
     */
    static void HandleGetTitleDetailRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理获取我的称号请求
     */
    static void HandleGetMyTitlesRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理装备称号请求
     */
    static void HandleEquipTitleRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理卸载称号请求
     */
    static void HandleUnequipTitleRequest(
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
