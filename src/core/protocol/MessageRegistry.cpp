#include "MessageRegistry.hpp"
#include "spdlog_wrapper.hpp"

a
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Core {
namespace Protocol {

MessageRegistry& MessageRegistry::Instance() {
    static MessageRegistry instance;
    return instance;
}

void MessageRegistry::HandleMessage(uint64_t session_id,
                                   MessageType msg_type,
                                   const std::string& data) {
    uint32_t type_id = static_cast<uint32_t>(msg_type);

    auto it = handlers_.find(type_id);
    if (it == handlers_.end()) {
        spdlog::warn("No handler registered for message type: 0x{:04X}", type_id);
        return;
    }

    // 创建消息对象
    auto* msg = CreateMessage(msg_type);
    if (!msg) {
        spdlog::error("Failed to create message for type: 0x{:04X}", type_id);
        return;
    }

    // 解析消息数据
    if (!msg->ParseFromString(data)) {
        spdlog::error("Failed to parse message for type: 0x{:04X}", type_id);
        delete msg;
        return;
    }

    // 调用处理器
    try {
        it->second.first(session_id, *msg);
    } catch (const std::exception& e) {
        spdlog::error("Error handling message 0x{:04X}: {}", type_id, e.what());
    }

    delete msg;
}

google::protobuf::Message* MessageRegistry::CreateMessage(MessageType msg_type) {
    uint32_t type_id = static_cast<uint32_t>(msg_type);

    auto it = handlers_.find(type_id);
    if (it == handlers_.end()) {
        return nullptr;
    }

    return it->second.second();
}

} // namespace Protocol
} // namespace Core
} // namespace Murim
