/**
 * ServerProtocol.hpp
 *
 * 服务器间通信协议定义
 * 用于 AgentServer、DistributeServer、MurimNetServer、MapServer 之间的通信
 */

#pragma once

#include <cstdint>
#include <string>

namespace Murim {
namespace Core {
namespace Protocol {

/**
 * 服务器类型
 */
enum class ServerType : uint8_t {
    kAgent = 0,          // AgentServer (认证服务器)
    kDistribute = 1,     // DistributeServer (负载均衡)
    kMurimNet = 2,       // MurimNetServer (社交服务器)
    kMap = 3             // MapServer (游戏世界服务器)
};

/**
 * 服务器间消息类型
 */
enum class ServerMessageType : uint32_t {
    // 连接管理 (0x1000)
    SERVER_REGISTER = 0x1001,           // 服务器注册
    REGISTER_ACK = 0x1002,              // 注册确认
    SERVER_HEARTBEAT = 0x1003,          // 心跳
    HEARTBEAT_ACK = 0x1004,             // 心跳响应
    SERVER_SHUTDOWN = 0x1005,           // 服务器关闭通知

    // 玩家转移 (0x2000)
    PLAYER_TRANSFER_REQUEST = 0x2001,   // 玩家转移请求 (Agent → Map)
    PLAYER_TRANSFER_RESPONSE = 0x2002,  // 玩家转移响应 (Map → Agent)
    PLAYER_DATA_SYNC = 0x2003,          // 玩家数据同步
    PLAYER_ENTER_NOTIFY = 0x2004,       // 玩家进入通知
    PLAYER_LEAVE_NOTIFY = 0x2005,       // 玩家离开通知

    // 负载均衡 (0x3000)
    LOAD_STATUS_REPORT = 0x3001,        // 负载状态报告
    LOAD_STATUS_REQUEST = 0x3002,       // 负载状态查询
    PLAYER_COUNT_NOTIFY = 0x3003,       // 玩家数量通知
    REDIRECT_PLAYER = 0x3004,           // 重定向玩家

    // 社交系统同步 (0x4000)
    GUILD_UPDATE_NOTIFY = 0x4001,       // 公会更新通知 (Map → MurimNet)
    FRIEND_STATUS_NOTIFY = 0x4002,      // 好友状态变更通知
    PARTY_UPDATE_NOTIFY = 0x4003,       // 队伍更新通知
    CHAT_MESSAGE_FORWARD = 0x4004,      // 聊天消息转发

    // 数据同步 (0x5000)
    CHARACTER_SAVE_REQUEST = 0x5001,    // 角色保存请求
    CHARACTER_SAVE_RESPONSE = 0x5002,   // 角色保存响应
    CHARACTER_LOAD_REQUEST = 0x5003,    // 角色加载请求
    CHARACTER_LOAD_RESPONSE = 0x5004,   // 角色加载响应
};

/**
 * 服务器注册信息
 */
struct ServerInfo {
    ServerType type;              // 服务器类型
    uint16_t server_id;           // 服务器ID
    uint16_t map_id;              // 地图ID (仅MapServer)
    std::string ip;               // IP地址
    uint16_t port;                // 端口
    uint32_t max_players;         // 最大玩家数
    uint32_t current_players;     // 当前玩家数
    uint64_t last_heartbeat;      // 最后心跳时间

    ServerInfo()
        : type(ServerType::kMap)
        , server_id(0)
        , map_id(0)
        , port(0)
        , max_players(1000)
        , current_players(0)
        , last_heartbeat(0)
    {}
};

/**
 * 玩家转移请求
 */
struct PlayerTransferRequest {
    uint64_t account_id;          // 账户ID
    uint64_t character_id;        // 角色ID
    std::string character_name;   // 角色名称
    uint32_t level;               // 等级
    uint64_t session_id;          // 会话ID
    ServerType from_server;       // 来源服务器
    ServerType to_server;         // 目标服务器
};

/**
 * 玩家转移响应
 */
struct PlayerTransferResponse {
    uint64_t character_id;        // 角色ID
    uint32_t result_code;         // 结果码
    std::string message;          // 消息
    uint16_t map_server_id;       // MapServer ID
    uint16_t map_id;              // 地图ID
    float position_x;             // X坐标
    float position_y;             // Y坐标
    float position_z;             // Z坐标
};

/**
 * 负载状态报告
 */
struct LoadStatusReport {
    uint16_t server_id;           // 服务器ID
    uint32_t current_players;     // 当前玩家数
    uint32_t max_players;         // 最大玩家数
    float cpu_usage;              // CPU使用率
    float memory_usage;           // 内存使用率
    uint32_t network_in;          // 网络入流量 (bytes/s)
    uint32_t network_out;         // 网络出流量 (bytes/s)
};

/**
 * 结果码定义
 */
enum class ServerResultCode : uint32_t {
    kSuccess = 0,                 // 成功
    kServerError = 1,             // 服务器错误
    kInvalidServer = 2,           // 无效的服务器
    kServerFull = 3,              // 服务器已满
    kPlayerNotFound = 4,          // 玩家不存在
    kPlayerAlreadyOnline = 5,     // 玩家已在线
    kInvalidSession = 6,          // 无效的会话
    kTransferFailed = 7,          // 转移失败
    kDatabaseError = 8,           // 数据库错误
};

} // namespace Protocol
} // namespace Core
} // namespace Murim
