#include "MessageRouter.hpp"
#include "spdlog_wrapper.hpp"
#include <algorithm>

namespace Murim {
namespace Core {
namespace Network {

MessageRouter& MessageRouter::Instance() {
    static MessageRouter instance;
    return instance;
}

// ========== 消息处理器注册 ==========

void MessageRouter::RegisterHandler(uint32_t message_type, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);

    handlers_[message_type] = handler;
    stats_.handlers_registered = handlers_.size();

    spdlog::debug("Message handler registered: type={}, total_handlers={}",
                  message_type, stats_.handlers_registered);
}

void MessageRouter::UnregisterHandler(uint32_t message_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = handlers_.find(message_type);
    if (it != handlers_.end()) {
        handlers_.erase(it);
        stats_.handlers_registered = handlers_.size();

        spdlog::debug("Message handler unregistered: type={}, total_handlers={}",
                      message_type, stats_.handlers_registered);
    } else {
        spdlog::warn("Attempted to unregister non-existent handler: type={}", message_type);
    }
}

void MessageRouter::ClearAllHandlers() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = handlers_.size();
    handlers_.clear();
    stats_.handlers_registered = 0;

    spdlog::info("Cleared all message handlers: count={}", count);
}

// ========== 消息路由 ==========

bool MessageRouter::RouteMessage(
    uint64_t sender_id,
    uint32_t message_type,
    const std::vector<uint8_t>& data
) {
    std::lock_guard<std::mutex> lock(mutex_);

    stats_.messages_routed++;

    // 查找处理器
    auto it = handlers_.find(message_type);
    if (it == handlers_.end()) {
        spdlog::debug("No handler registered for message type: {}", message_type);
        stats_.messages_failed++;
        return false;
    }

    // 调用处理器
    try {
        bool result = it->second(sender_id, message_type, data);

        if (result) {
            stats_.messages_processed++;
            spdlog::trace("Message processed: sender={}, type={}, size={}",
                         sender_id, message_type, data.size());
        } else {
            stats_.messages_failed++;
            spdlog::debug("Message handler returned failure: sender={}, type={}",
                         sender_id, message_type);
        }

        return result;
    } catch (const std::exception& e) {
        stats_.messages_failed++;
        spdlog::error("Exception in message handler: sender={}, type={}, error={}",
                     sender_id, message_type, e.what());
        return false;
    }
}

bool MessageRouter::RouteMessageWithPriority(
    uint64_t sender_id,
    uint32_t message_type,
    const std::vector<uint8_t>& data,
    uint8_t priority
) {
    // 高优先级消息直接处理,不进入队列
    if (priority >= 200) {
        return RouteMessage(sender_id, message_type, data);
    }

    // 中低优先级消息加入队列
    EnqueueMessage(sender_id, message_type, data, priority);
    return true;
}

// ========== 消息队列 ==========

void MessageRouter::EnqueueMessage(
    uint64_t sender_id,
    uint32_t message_type,
    const std::vector<uint8_t>& data,
    uint8_t priority
) {
    std::lock_guard<std::mutex> lock(mutex_);

    message_queue_.emplace_back(sender_id, message_type, data, priority);
    stats_.queue_size = message_queue_.size();

    spdlog::trace("Message enqueued: sender={}, type={}, priority={}, queue_size={}",
                 sender_id, message_type, priority, stats_.queue_size);
}

size_t MessageRouter::ProcessQueue() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (message_queue_.empty()) {
        return 0;
    }

    // 按优先级排序 (高优先级在前)
    std::sort(message_queue_.begin(), message_queue_.end(),
        [](const QueuedMessage& a, const QueuedMessage& b) {
            return a.priority > b.priority;
        });

    size_t processed = 0;

    // 处理所有消息
    for (const auto& msg : message_queue_) {
        auto it = handlers_.find(msg.message_type);
        if (it != handlers_.end()) {
            try {
                bool result = it->second(msg.sender_id, msg.message_type, msg.data);
                if (result) {
                    stats_.messages_processed++;
                    processed++;
                } else {
                    stats_.messages_failed++;
                }
            } catch (const std::exception& e) {
                stats_.messages_failed++;
                spdlog::error("Exception processing queued message: sender={}, type={}, error={}",
                             msg.sender_id, msg.message_type, e.what());
            }
        } else {
            spdlog::debug("No handler for queued message: type={}", msg.message_type);
            stats_.messages_failed++;
        }
    }

    // 清空队列
    message_queue_.clear();
    stats_.queue_size = 0;

    if (processed > 0) {
        spdlog::debug("Processed {} queued messages", processed);
    }

    return processed;
}

size_t MessageRouter::ProcessQueue(size_t max_count) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (message_queue_.empty()) {
        return 0;
    }

    // 按优先级排序
    std::sort(message_queue_.begin(), message_queue_.end(),
        [](const QueuedMessage& a, const QueuedMessage& b) {
            return a.priority > b.priority;
        });

    size_t processed = 0;
    size_t count = std::min(max_count, message_queue_.size());

    // 处理前 max_count 条消息
    for (size_t i = 0; i < count; ++i) {
        const auto& msg = message_queue_[i];

        auto it = handlers_.find(msg.message_type);
        if (it != handlers_.end()) {
            try {
                bool result = it->second(msg.sender_id, msg.message_type, msg.data);
                if (result) {
                    stats_.messages_processed++;
                    processed++;
                } else {
                    stats_.messages_failed++;
                }
            } catch (const std::exception& e) {
                stats_.messages_failed++;
                spdlog::error("Exception processing queued message: sender={}, type={}, error={}",
                             msg.sender_id, msg.message_type, e.what());
            }
        } else {
            spdlog::debug("No handler for queued message: type={}", msg.message_type);
            stats_.messages_failed++;
        }
    }

    // 移除已处理的消息
    message_queue_.erase(message_queue_.begin(), message_queue_.begin() + count);
    stats_.queue_size = message_queue_.size();

    if (processed > 0) {
        spdlog::debug("Processed {}/{} queued messages", processed, count);
    }

    return processed;
}

void MessageRouter::ClearQueue() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = message_queue_.size();
    message_queue_.clear();
    stats_.queue_size = 0;

    spdlog::debug("Cleared message queue: count={}", count);
}

size_t MessageRouter::GetQueueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return message_queue_.size();
}

// ========== 统计信息 ==========

MessageRouter::RouterStats MessageRouter::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void MessageRouter::ResetStats() {
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t handlers_count = stats_.handlers_registered;
    stats_.Reset();
    stats_.handlers_registered = handlers_count;
    stats_.queue_size = message_queue_.size();

    spdlog::debug("Message router stats reset");
}

} // namespace Network
} // namespace Core
} // namespace Murim
