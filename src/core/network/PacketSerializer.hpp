#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "ByteBuffer.hpp"

namespace Murim {
namespace Core {
namespace Network {

/**
 * @brief 数据包序列化器
 *
 * 负责在 Protobuf 消息和网络数据包之间转换
 *
 * 数据包格式:
 * [4B 长度][2B 消息 ID][NB Protobuf 数据]
 */
class PacketSerializer {
public:
    /**
     * @brief 序列化 Protobuf 消息到数据包
     * @param message_id 消息 ID
     * @param data Protobuf 消息数据
     * @return 数据包缓冲区
     */
    static std::shared_ptr<ByteBuffer> Serialize(uint16_t message_id, const std::vector<uint8_t>& data);

    /**
     * @brief 序列化 Protobuf 消息到数据包
     * @param message_id 消息 ID
     * @param data Protobuf 消息数据
     * @param length 数据长度
     * @return 数据包缓冲区
     */
    static std::shared_ptr<ByteBuffer> Serialize(uint16_t message_id, const void* data, size_t length);

    /**
     * @brief 从数据包反序列化消息头
     * @param buffer 数据包缓冲区
     * @param out_message_id 输出消息 ID
     * @param out_data_length 输出数据长度
     * @return true if successful
     */
    static bool DeserializeHeader(ByteBuffer& buffer, uint16_t& out_message_id, uint32_t& out_data_length);

    /**
     * @brief 从数据包提取数据部分
     * @param buffer 数据包缓冲区
     * @return 数据部分
     */
    static std::vector<uint8_t> ExtractData(ByteBuffer& buffer);

    /**
     * @brief 验证数据包格式
     * @param buffer 数据包缓冲区
     * @return true if valid
     */
    static bool ValidatePacket(ByteBuffer& buffer);

    /**
     * @brief 获取数据包头大小
     * @return 头大小 (字节)
     */
    static constexpr size_t GetHeaderSize() {
        return sizeof(uint32_t) + sizeof(uint16_t);  // 长度 + 消息 ID
    }

private:
    // 禁止实例化
    PacketSerializer() = delete;
    ~PacketSerializer() = delete;
};

} // namespace Network
} // namespace Core
} // namespace Murim
