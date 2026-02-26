#include "NetworkPacket.hpp"
#include "spdlog_wrapper.hpp"
#include <cstring>

// 跨平台网络支持
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
#endif

a
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Core {
namespace Protocol {

NetworkPacket::NetworkPacket(uint32_t msg_type, const std::string& data)
    : msg_type_(htonl(msg_type)),  // 转换为网络字节序
      data_(data) {
}

NetworkPacket::NetworkPacket(const uint8_t* bytes, size_t size) {
    if (size < sizeof(uint32_t) + sizeof(uint32_t)) {
        throw std::runtime_error("Packet too small");
    }

    // 读取消息长度 (暂时不使用)
    // uint32_t length = ntohl(*reinterpret_cast<const uint32_t*>(bytes));

    // 读取消息类型
    msg_type_ = *reinterpret_cast<const uint32_t*>(bytes + sizeof(uint32_t));

    // 读取消息数据
    size_t data_size = size - sizeof(uint32_t) - sizeof(uint32_t);
    data_.assign(reinterpret_cast<const char*>(bytes + sizeof(uint32_t) + sizeof(uint32_t)),
                 data_size);
}

std::vector<uint8_t> NetworkPacket::Serialize() const {
    std::vector<uint8_t> packet;

    // 计算总大小
    uint32_t total_size = sizeof(uint32_t) + sizeof(uint32_t) + data_.size();
    packet.reserve(total_size);

    // 写入长度 (网络字节序)
    uint32_t length = htonl(static_cast<uint32_t>(data_.size()));
    const uint8_t* length_bytes = reinterpret_cast<const uint8_t*>(&length);
    packet.insert(packet.end(), length_bytes, length_bytes + sizeof(uint32_t));

    // 写入消息类型 (已经是网络字节序)
    const uint8_t* type_bytes = reinterpret_cast<const uint8_t*>(&msg_type_);
    packet.insert(packet.end(), type_bytes, type_bytes + sizeof(uint32_t));

    // 写入数据
    packet.insert(packet.end(), data_.begin(), data_.end());

    return packet;
}

// ============== PacketParser ==============

PacketParser::PacketParser()
    : read_pos_(0) {
}

void PacketParser::AddData(const uint8_t* data, size_t size) {
    buffer_.insert(buffer_.end(), data, data + size);
}

bool PacketParser::TryParse(NetworkPacket& packet) {
    // 检查是否有足够的数据读取包头
    if (buffer_.size() < sizeof(uint32_t) + sizeof(uint32_t)) {
        return false;
    }

    // 读取消息长度
    uint32_t length = ntohl(*reinterpret_cast<uint32_t*>(buffer_.data() + read_pos_));

    // 检查消息是否完整
    if (buffer_.size() < sizeof(uint32_t) + sizeof(uint32_t) + length) {
        return false;
    }

    // 读取消息类型
    uint32_t msg_type = ntohl(*reinterpret_cast<uint32_t*>(
        buffer_.data() + read_pos_ + sizeof(uint32_t)));

    // 读取消息数据
    const char* data_ptr = reinterpret_cast<const char*>(
        buffer_.data() + read_pos_ + sizeof(uint32_t) + sizeof(uint32_t));
    std::string data(data_ptr, length);

    // 创建包
    packet = NetworkPacket(msg_type, data);

    // 移动读指针
    read_pos_ += sizeof(uint32_t) + sizeof(uint32_t) + length;

    // 移除已处理的数据
    if (read_pos_ > 0) {
        buffer_.erase(buffer_.begin(), buffer_.begin() + read_pos_);
        read_pos_ = 0;
    }

    return true;
}

void PacketParser::Clear() {
    buffer_.clear();
    read_pos_ = 0;
}

} // namespace Protocol
} // namespace Core
} // namespace Murim
