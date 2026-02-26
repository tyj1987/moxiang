// Murim MMORPG - Message Helper
// 辅助类：简化Protocol Buffers消息的序列化和反序列化

#pragma once

#include <string>
#include <vector>
#include <memory>
#include "core/network/ByteBuffer.hpp"

namespace Murim {
namespace MapServer {
namespace Protocol {

/**
 * @brief Protocol Buffers 消息辅助类
 *
 * 提供消息的序列化、反序列化和错误处理
 */
class MessageHelper {
public:
    /**
     * @brief 消息解析结果
     */
    struct ParseResult {
        bool success;
        std::string error_message;

        static ParseResult Ok() {
            return {true, ""};
        }

        static ParseResult Error(const std::string& msg) {
            return {false, msg};
        }
    };

    /**
     * @brief 从ByteBuffer解析Protocol Buffers消息
     *
     * @tparam T 消息类型 (如 murim::network::NPCInteractRequest)
     * @param buffer 消息缓冲区
     * @param message 输出的消息对象
     * @return ParseResult 解析结果
     */
    template<typename T>
    static ParseResult ParseMessage(const Core::Network::ByteBuffer& buffer, T& message) {
        try {
            // 获取数据指针和大小
            const char* data = buffer.GetData();
            size_t size = buffer.GetSize();

            if (data == nullptr || size == 0) {
                return ParseResult::Error("Buffer is empty");
            }

            // 解析消息
            if (!message.ParseFromArray(data, static_cast<int>(size))) {
                return ParseResult::Error("Failed to parse message from buffer");
            }

            return ParseResult::Ok();

        } catch (const std::exception& e) {
            return ParseResult::Error(std::string("Exception during parsing: ") + e.what());
        }
    }

    /**
     * @brief 从字节数组解析Protocol Buffers消息
     *
     * @tparam T 消息类型
     * @param data 数据指针
     * @param size 数据大小
     * @param message 输出的消息对象
     * @return ParseResult 解析结果
     */
    template<typename T>
    static ParseResult ParseMessage(const char* data, size_t size, T& message) {
        try {
            if (data == nullptr || size == 0) {
                return ParseResult::Error("Data is null or empty");
            }

            if (!message.ParseFromArray(data, static_cast<int>(size))) {
                return ParseResult::Error("Failed to parse message");
            }

            return ParseResult::Ok();

        } catch (const std::exception& e) {
            return ParseResult::Error(std::string("Exception: ") + e.what());
        }
    }

    /**
     * @brief 序列化Protocol Buffers消息到字符串
     *
     * @tparam T 消息类型
     * @param message 消息对象
     * @param output 输出的字符串
     * @return 是否成功
     */
    template<typename T>
    static bool SerializeMessage(const T& message, std::string& output) {
        try {
            if (!message.SerializeToString(&output)) {
                return false;
            }
            return true;

        } catch (const std::exception& e) {
            // Log error
            return false;
        }
    }

    /**
     * @brief 序列化Protocol Buffers消息到字节数组
     *
     * @tparam T 消息类型
     * @param message 消息对象
     * @param output 输出的字节数组
     * @return 是否成功
     */
    template<typename T>
    static bool SerializeMessage(const T& message, std::vector<uint8_t>& output) {
        try {
            size_t size = message.ByteSizeLong();
            output.resize(size);

            if (!message.SerializeToArray(output.data(), static_cast<int>(size))) {
                return false;
            }
            return true;

        } catch (const std::exception& e) {
            // Log error
            return false;
        }
    }

    /**
     * @brief 创建ByteBuffer
     *
     * @param data 数据指针
     * @param size 数据大小
     * @return ByteBuffer对象
     */
    static Core::Network::ByteBuffer CreateBuffer(const char* data, size_t size) {
        return Core::Network::ByteBuffer(data, size);
    }

    /**
     * @brief 创建ByteBuffer从字符串
     *
     * @param str 字符串数据
     * @return ByteBuffer对象
     */
    static Core::Network::ByteBuffer CreateBuffer(const std::string& str) {
        return Core::Network::ByteBuffer(str.data(), str.size());
    }

    /**
     * @brief 创建ByteBuffer从Protocol Buffers消息
     *
     * @tparam T 消息类型
     * @param message 消息对象
     * @return ByteBuffer对象
     */
    template<typename T>
    static Core::Network::ByteBuffer CreateBuffer(const T& message) {
        std::string serialized;
        if (!SerializeMessage(message, serialized)) {
            // 返回空缓冲区
            return Core::Network::ByteBuffer();
        }
        return CreateBuffer(serialized);
    }
};

/**
 * @brief 消息发送器
 *
 * 简化响应消息的发送过程
 */
class MessageSender {
public:
    /**
     * @brief 发送Protocol Buffers消息
     *
     * @tparam T 消息类型
     * @param conn 连接对象
     * @param message_id 消息ID
     * @param message 消息对象
     * @return 是否发送成功
     */
    template<typename T>
    static bool SendMessage(
        std::shared_ptr<Core::Network::Connection> conn,
        uint16_t message_id,
        const T& message
    ) {
        if (!conn) {
            return false;
        }

        // 序列化消息
        std::string serialized;
        if (!MessageHelper::SerializeMessage(message, serialized)) {
            return false;
        }

        // 发送消息
        return conn->Send(message_id, serialized.data(), serialized.size());
    }

    /**
     * @brief 发送错误响应
     *
     * @param conn 连接对象
     * @param request_message_id 请求消息ID
     * @param error_code 错误码
     * @param error_message 错误消息
     * @return 是否发送成功
     */
    static bool SendError(
        std::shared_ptr<Core::Network::Connection> conn,
        uint16_t request_message_id,
        uint32_t error_code,
        const std::string& error_message
    ) {
        // 构造错误响应
        murim::network::ErrorResponse response;
        response.set_error_code(error_code);
        response.set_error_message(error_message);

        // 错误响应的消息ID = 请求消息ID + 1
        return SendMessage(conn, request_message_id + 1, response);
    }
};

/**
 * @brief 消息处理器基类
 *
 * 所有消息处理器都应该继承此类
 */
class MessageHandlerBase {
public:
    virtual ~MessageHandlerBase() = default;

    /**
     * @brief 处理消息
     *
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     * @return 是否处理成功
     */
    virtual bool HandleMessage(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer
    ) = 0;

    /**
     * @brief 获取消息ID
     */
    virtual uint16_t GetMessageId() const = 0;
};

/**
 * @brief 消息路由器
 *
 * 管理消息ID到处理器的映射
 */
class MessageRouter {
public:
    /**
     * @brief 注册消息处理器
     *
     * @param handler 处理器指针
     */
    void RegisterHandler(std::shared_ptr<MessageHandlerBase> handler) {
        if (handler) {
            handlers_[handler->GetMessageId()] = handler;
        }
    }

    /**
     * @brief 路由消息到对应的处理器
     *
     * @param conn 客户端连接
     * @param message_id 消息ID
     * @param buffer 消息缓冲区
     * @return 是否处理成功
     */
    bool RouteMessage(
        std::shared_ptr<Core::Network::Connection> conn,
        uint16_t message_id,
        const Core::Network::ByteBuffer& buffer
    ) {
        auto it = handlers_.find(message_id);
        if (it != handlers_.end()) {
            return it->second->HandleMessage(conn, buffer);
        }

        // 未找到处理器
        spdlog::warn("No handler found for message ID: 0x{:04X}", message_id);
        return false;
    }

    /**
     * @brief 检查是否有对应的消息处理器
     *
     * @param message_id 消息ID
     * @return 是否存在
     */
    bool HasHandler(uint16_t message_id) const {
        return handlers_.find(message_id) != handlers_.end();
    }

private:
    std::unordered_map<uint16_t, std::shared_ptr<MessageHandlerBase>> handlers_;
};

} // namespace Protocol
} // namespace MapServer
} // namespace Murim
