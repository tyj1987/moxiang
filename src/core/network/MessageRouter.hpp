#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>

namespace Murim {
namespace Core {
namespace Network {

/**
 * @brief 消息处理函数类型
 * @param sender_id 发送者ID
 * @param message_type 消息类型
 * @param data 消息数据
 * @return 是否处理成功
 */
using MessageHandler = std::function<bool(
    uint64_t sender_id,
    uint32_t message_type,
    const std::vector<uint8_t>& data
)>;

/**
 * @brief 消息路由器
 *
 * 负责注册消息处理器和路由消息到对应的处理器
 * 用于 AI 系统和其他需要事件驱动的系统
 *
 * 对应 legacy: 事件处理系统
 */
class MessageRouter {
public:
    /**
     * @brief 获取单例实例
     */
    static MessageRouter& Instance();

    // ========== 消息处理器注册 ==========

    /**
     * @brief 注册消息处理器
     * @param message_type 消息类型
     * @param handler 处理函数
     */
    void RegisterHandler(uint32_t message_type, MessageHandler handler);

    /**
     * @brief 取消注册消息处理器
     * @param message_type 消息类型
     */
    void UnregisterHandler(uint32_t message_type);

    /**
     * @brief 清除所有处理器
     */
    void ClearAllHandlers();

    // ========== 消息路由 ==========

    /**
     * @brief 路由消息到对应的处理器
     * @param sender_id 发送者ID
     * @param message_type 消息类型
     * @param data 消息数据
     * @return 是否处理成功
     */
    bool RouteMessage(
        uint64_t sender_id,
        uint32_t message_type,
        const std::vector<uint8_t>& data
    );

    /**
     * @brief 路由消息到对应的处理器 (带优先级)
     * @param sender_id 发送者ID
     * @param message_type 消息类型
     * @param data 消息数据
     * @param priority 优先级 (0=最低, 255=最高)
     * @return 是否处理成功
     */
    bool RouteMessageWithPriority(
        uint64_t sender_id,
        uint32_t message_type,
        const std::vector<uint8_t>& data,
        uint8_t priority
    );

    // ========== 消息队列 ==========

    /**
     * @brief 消息队列项
     */
    struct QueuedMessage {
        uint64_t sender_id;
        uint32_t message_type;
        std::vector<uint8_t> data;
        uint8_t priority;

        QueuedMessage() : sender_id(0), message_type(0), priority(0) {}

        QueuedMessage(
            uint64_t sid,
            uint32_t mt,
            const std::vector<uint8_t>& d,
            uint8_t prio = 0
        ) : sender_id(sid), message_type(mt), data(d), priority(prio) {}
    };

    /**
     * @brief 将消息加入队列
     * @param sender_id 发送者ID
     * @param message_type 消息类型
     * @param data 消息数据
     * @param priority 优先级 (可选)
     */
    void EnqueueMessage(
        uint64_t sender_id,
        uint32_t message_type,
        const std::vector<uint8_t>& data,
        uint8_t priority = 0
    );

    /**
     * @brief 处理队列中的所有消息
     * @return 处理的消息数量
     */
    size_t ProcessQueue();

    /**
     * @brief 处理队列中的指定数量消息
     * @param max_count 最大处理数量
     * @return 实际处理的消息数量
     */
    size_t ProcessQueue(size_t max_count);

    /**
     * @brief 清空消息队列
     */
    void ClearQueue();

    /**
     * @brief 获取队列大小
     */
    size_t GetQueueSize() const;

    // ========== 统计信息 ==========

    /**
     * @brief 路由器统计信息
     */
    struct RouterStats {
        uint64_t messages_routed = 0;      // 总路由消息数
        uint64_t messages_processed = 0;   // 总处理消息数
        uint64_t messages_failed = 0;      // 失败消息数
        uint64_t handlers_registered = 0;  // 注册的处理器数
        uint64_t queue_size = 0;           // 当前队列大小

        void Reset() {
            messages_routed = 0;
            messages_processed = 0;
            messages_failed = 0;
            handlers_registered = 0;
            queue_size = 0;
        }
    };

    /**
     * @brief 获取统计信息
     */
    RouterStats GetStats() const;

    /**
     * @brief 重置统计信息
     */
    void ResetStats();

private:
    MessageRouter() = default;
    ~MessageRouter() = default;
    MessageRouter(const MessageRouter&) = delete;
    MessageRouter& operator=(const MessageRouter&) = delete;

    // 消息处理器: message_type -> handler
    std::unordered_map<uint32_t, MessageHandler> handlers_;

    // 消息队列 (按优先级排序)
    std::vector<QueuedMessage> message_queue_;

    // 统计信息
    RouterStats stats_;

    // 互斥锁 (线程安全)
    mutable std::mutex mutex_;
};

} // namespace Network
} // namespace Core
} // namespace Murim
