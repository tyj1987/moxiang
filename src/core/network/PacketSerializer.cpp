#include "PacketSerializer.hpp"
#include "spdlog_wrapper.hpp"
#include <algorithm>
#include <cstring>

namespace Murim {
namespace Core {
namespace Network {

std::shared_ptr<ByteBuffer> PacketSerializer::Serialize(uint16_t message_id, const std::vector<uint8_t>& data) {
    if (data.empty()) {
        spdlog::warn("Attempted to serialize empty message, message_id={}", message_id);
        return nullptr;
    }

    return Serialize(message_id, data.data(), data.size());
}

std::shared_ptr<ByteBuffer> PacketSerializer::Serialize(uint16_t message_id, const void* data, size_t length) {
    if (!data || length == 0) {
        spdlog::warn("Attempted to serialize null or empty message, message_id={}", message_id);
        return nullptr;
    }

    // 计算总长度: 4B (长度) + 2B (消息ID) + NB (数据)
    uint32_t total_length = sizeof(uint32_t) + sizeof(uint16_t) + static_cast<uint32_t>(length);

    // 创建缓冲区
    auto buffer = std::make_shared<ByteBuffer>();
    buffer->Reserve(total_length);

    // 写入长度 (不包括长度字段本身)
    uint32_t payload_length = sizeof(uint16_t) + static_cast<uint32_t>(length);
    buffer->WriteUInt32(payload_length);

    // 写入消息 ID
    buffer->WriteUInt16(message_id);

    // 写入数据
    buffer->Write(reinterpret_cast<const uint8_t*>(data), length);

    // 重置读取位置
    buffer->SetReadPosition(0);

    spdlog::debug("Serialized packet: message_id={}, data_length={}, total_length={}",
                  message_id, length, total_length);

    return buffer;
}

bool PacketSerializer::DeserializeHeader(ByteBuffer& buffer, uint16_t& out_message_id, uint32_t& out_data_length) {
    // 检查最小长度
    if (buffer.GetReadableSize() < GetHeaderSize()) {
        spdlog::warn("Packet too small to deserialize header: {} bytes (need at least {} bytes)",
                      buffer.GetReadableSize(), GetHeaderSize());
        return false;
    }

    // 保存当前位置
    size_t saved_pos = buffer.GetReadPosition();

    // 读取长度
    uint32_t payload_length = buffer.ReadUInt32();

    // 读取消息 ID
    out_message_id = buffer.ReadUInt16();

    // 计算数据长度
    out_data_length = payload_length - sizeof(uint16_t);

    // 恢复读取位置
    buffer.SetReadPosition(saved_pos);

    spdlog::debug("Deserialized packet header: message_id={}, data_length={}, payload_length={}",
                  out_message_id, out_data_length, payload_length);

    return true;
}

std::vector<uint8_t> PacketSerializer::ExtractData(ByteBuffer& buffer) {
    // 跳过头 (4B 长度 + 2B 消息 ID)
    size_t header_size = GetHeaderSize();

    if (buffer.GetReadableSize() < header_size) {
        spdlog::warn("Packet too small to extract data");
        return {};
    }

    // 读取长度
    uint32_t payload_length = buffer.ReadUInt32();
    uint16_t message_id = buffer.ReadUInt16();

    // 提取数据
    std::vector<uint8_t> data;
    data.resize(payload_length - sizeof(uint16_t));
    buffer.Read(data.data(), data.size());

    spdlog::debug("Extracted {} bytes of data from packet (message_id={})", data.size(), message_id);

    return data;
}

bool PacketSerializer::ValidatePacket(ByteBuffer& buffer) {
    // 检查最小长度
    if (buffer.GetReadableSize() < GetHeaderSize()) {
        spdlog::debug("Packet validation failed: too small ({} bytes)", buffer.GetReadableSize());
        return false;
    }

    // 保存当前位置
    size_t saved_pos = buffer.GetReadPosition();

    // 读取长度
    uint32_t payload_length = buffer.ReadUInt32();

    // 检查长度合理性
    if (payload_length < sizeof(uint16_t) || payload_length > 65535) {
        spdlog::warn("Packet validation failed: invalid payload length {}", payload_length);
        buffer.SetReadPosition(saved_pos);
        return false;
    }

    // 检查是否有足够数据
    if (buffer.GetReadableSize() < payload_length) {
        spdlog::debug("Packet validation failed: incomplete payload (expected {} bytes, have {} bytes)",
                       payload_length, buffer.GetReadableSize());
        buffer.SetReadPosition(saved_pos);
        return false;
    }

    // 恢复读取位置
    buffer.SetReadPosition(saved_pos);

    return true;
}

} // namespace Network
} // namespace Core
} // namespace Murim
