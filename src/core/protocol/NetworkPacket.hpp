#pragma once

#include <string>
#include <vector>
#include <cstdint>

// 跨平台网络头文件
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
#endif

namespace Murim {
namespace Core {
namespace Protocol {

/**
 * @brief 网络数据包
 *
 * 封装网络消息的发送和接收格式
 */
class NetworkPacket {
public:
    /**
     * @brief 构造网络包
     * @param msg_type 消息类型 ID
     * @param data Protocol Buffers 序列化后的数据
     */
    NetworkPacket(uint32_t msg_type, const std::string& data);

    /**
     * @brief 从字节数组构造网络包
     * @param bytes 字节数组
     * @param size 字节数组大小
     */
    NetworkPacket(const uint8_t* bytes, size_t size);

    /**
     * @brief 获取消息类型 ID
     * @return 消息类型 ID
     */
    uint32_t GetMessageType() const { return msg_type_; }

    /**
     * @brief 获取消息数据
     * @return 消息数据
     */
    const std::string& GetData() const { return data_; }

    /**
     * @brief 序列化为字节数组
     * @return 字节数组
     */
    std::vector<uint8_t> Serialize() const;

    /**
     * @brief 获取完整包大小
     * @return 包大小 (字节)
     */
    size_t GetTotalSize() const {
        return sizeof(uint32_t) + sizeof(uint32_t) + data_.size();
    }

private:
    uint32_t msg_type_;      // 消息类型 ID (网络字节序)
    std::string data_;       // Protocol Buffers 数据
};

/**
 * @brief 网络包解析器
 *
 * 从字节流中解析出完整的网络包
 */
class PacketParser {
public:
    /**
     * @brief 构造函数
     */
    PacketParser();

    /**
     * @brief 添加接收到的数据
     * @param data 数据指针
     * @param size 数据大小
     */
    void AddData(const uint8_t* data, size_t size);

    /**
     * @brief 尝试解析一个完整的包
     * @param packet 输出参数，解析成功的包
     * @return true 如果解析成功
     */
    bool TryParse(NetworkPacket& packet);

    /**
     * @brief 清空缓冲区
     */
    void Clear();

    /**
     * @brief 获取缓冲区大小
     * @return 缓冲区大小
     */
    size_t GetBufferSize() const { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;
    size_t read_pos_;
};

} // namespace Protocol
} // namespace Core
} // namespace Murim
