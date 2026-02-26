#pragma once

#include <functional>
#include <unordered_map>
#include <memory>
#include <google/protobuf/message.h>
#include "messages.pb.h"

namespace Murim {
namespace Core {
namespace Protocol {

// 消息类型 ID 定义
enum class MessageType : uint32_t {
    // 连接管理 (0x01)
    PING_REQUEST = 0x0101,
    PING_RESPONSE = 0x0102,
    LOGIN_REQUEST = 0x0103,
    LOGIN_RESPONSE = 0x0104,
    LOGOUT_REQUEST = 0x0105,
    LOGOUT_RESPONSE = 0x0106,

    // 角色管理 (0x02)
    CREATE_CHARACTER_REQUEST = 0x0201,
    CREATE_CHARACTER_RESPONSE = 0x0202,
    CHARACTER_LIST_REQUEST = 0x0203,
    CHARACTER_LIST_RESPONSE = 0x0204,
    SELECT_CHARACTER_REQUEST = 0x0205,
    SELECT_CHARACTER_RESPONSE = 0x0206,
    DELETE_CHARACTER_REQUEST = 0x0207,
    DELETE_CHARACTER_RESPONSE = 0x0208,

    // 移动系统 (0x0A)
    MOVE_REQUEST = 0x0A01,
    MOVE_RESPONSE = 0x0A02,
    MOVE_BROADCAST = 0x0A03,

    // 战斗系统 (0x04)
    ATTACK_REQUEST = 0x0401,
    ATTACK_RESPONSE = 0x0402,
    ATTACK_BROADCAST = 0x0403,
    DEATH_NOTIFY = 0x0404,
    REVIVE_REQUEST = 0x0405,
    REVIVE_RESPONSE = 0x0406,

    // 技能系统 (0x05)
    CAST_SKILL_REQUEST = 0x0501,
    CAST_SKILL_RESPONSE = 0x0502,
    CAST_SKILL_BROADCAST = 0x0503,
    SKILL_EFFECT = 0x0504,
    SKILL_COOLDOWN_NOTIFY = 0x0505,

    // 聊天系统 (0x09)
    CHAT_MESSAGE = 0x0901,

    // 物品系统 (0x03)
    USE_ITEM_REQUEST = 0x0301,
    USE_ITEM_RESPONSE = 0x0302,
    EQUIP_ITEM_REQUEST = 0x0303,
    EQUIP_ITEM_RESPONSE = 0x0304,
    UNEQUIP_ITEM_REQUEST = 0x0305,
    UNEQUIP_ITEM_RESPONSE = 0x0306,
    PICKUP_ITEM_REQUEST = 0x0307,
    PICKUP_ITEM_RESPONSE = 0x0308,
    ITEM_UPDATE_NOTIFY = 0x0309,
};

// 消息处理器类型
using MessageHandler = std::function<void(uint64_t session_id,
                                       const google::protobuf::Message&)>;

// 消息工厂函数
using MessageFactory = std::function<google::protobuf::Message*()>;

/**
 * @brief 消息注册表
 *
 * 管理消息类型和处理器的映射关系
 */
class MessageRegistry {
public:
    static MessageRegistry& Instance();

    /**
     * @brief 注册消息处理器
     * @tparam T 消息类型
     * @param msg_type 消息类型 ID
     * @param handler 消息处理函数
     */
    template<typename T>
    void RegisterHandler(MessageType msg_type, MessageHandler handler);

    /**
     * @brief 处理消息
     * @param session_id 会话 ID
     * @param msg_type 消息类型 ID
     * @param data 消息数据
     */
    void HandleMessage(uint64_t session_id,
                      MessageType msg_type,
                      const std::string& data);

    /**
     * @brief 创建消息实例
     * @param msg_type 消息类型 ID
     * @return 消息对象指针，调用方负责释放
     */
    google::protobuf::Message* CreateMessage(MessageType msg_type);

private:
    MessageRegistry() = default;
    ~MessageRegistry() = default;

    // 禁止拷贝
    MessageRegistry(const MessageRegistry&) = delete;
    MessageRegistry& operator=(const MessageRegistry&) = delete;

    std::unordered_map<uint32_t, std::pair<MessageHandler, MessageFactory>> handlers_;
};

// 模板方法实现
template<typename T>
void MessageRegistry::RegisterHandler(MessageType msg_type, MessageHandler handler) {
    uint32_t type_id = static_cast<uint32_t>(msg_type);

    handlers_[type_id] = std::make_pair(
        handler,
        []() -> google::protobuf::Message* {
            return new T();
        }
    );
}

} // namespace Protocol
} // namespace Core
} // namespace Murim
