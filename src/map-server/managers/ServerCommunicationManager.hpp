/**
 * ServerCommunicationManager.hpp
 *
 * 服务器间通信管理器
 * 管理 MapServer 与其他服务器 (AgentServer, MurimNetServer) 之间的通信
 */

#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <functional>
#include <mutex>
#include "core/protocol/ServerProtocol.hpp"
#include "core/network/ServerConnection.hpp"

namespace Murim {
namespace MapServer {

/**
 * 服务器连接信息
 */
struct ServerConnectionInfo {
    uint16_t server_id;
    Core::Protocol::ServerType type;
    std::string ip;
    uint16_t port;
    std::shared_ptr<Core::Network::ServerConnection> connection;
    uint64_t last_heartbeat;
    bool is_connected;

    ServerConnectionInfo()
        : server_id(0)
        , type(Core::Protocol::ServerType::kAgent)
        , port(0)
        , last_heartbeat(0)
        , is_connected(false)
    {}
};

/**
 * 服务器间通信管理器
 */
class ServerCommunicationManager {
public:
    /**
     * @brief 获取单例实例
     */
    static ServerCommunicationManager& Instance();

    /**
     * @brief 初始化通信管理器
     * @return 是否初始化成功
     */
    bool Initialize();

    /**
     * @brief 启动通信管理器
     * @return 是否启动成功
     */
    bool Start();

    /**
     * @brief 停止通信管理器
     */
    void Stop();

    /**
     * @brief 关闭通信管理器
     */
    void Shutdown();

    // ========== 连接管理 ==========

    /**
     * @brief 连接到服务器
     * @param server_id 服务器ID
     * @param type 服务器类型
     * @param ip IP地址
     * @param port 端口
     * @return 是否连接成功
     */
    bool ConnectToServer(uint16_t server_id, Core::Protocol::ServerType type,
                        const std::string& ip, uint16_t port);

    /**
     * @brief 断开服务器连接
     * @param server_id 服务器ID
     */
    void DisconnectFromServer(uint16_t server_id);

    /**
     * @brief 检查服务器是否已连接
     * @param server_id 服务器ID
     * @return 是否已连接
     */
    bool IsServerConnected(uint16_t server_id) const;

    /**
     * @brief 获取服务器连接
     * @param server_id 服务器ID
     * @return 服务器连接指针
     */
    ServerConnectionInfo* GetServerConnection(uint16_t server_id);

    // ========== 消息发送 ==========

    /**
     * @brief 发送服务器注册请求
     * @param server_id 目标服务器ID
     * @param server_info 本服务器信息
     */
    void SendServerRegister(uint16_t server_id, const Core::Protocol::ServerInfo& server_info);

    /**
     * @brief 发送心跳
     * @param server_id 目标服务器ID
     */
    void SendHeartbeat(uint16_t server_id);

    /**
     * @brief 发送负载状态报告
     * @param server_id 目标服务器ID
     * @param load_status 负载状态
     */
    void SendLoadStatusReport(uint16_t server_id, const Core::Protocol::LoadStatusReport& load_status);

    /**
     * @brief 发送玩家转移响应
     * @param server_id 目标服务器ID (AgentServer)
     * @param response 转移响应
     */
    void SendPlayerTransferResponse(uint16_t server_id, const Core::Protocol::PlayerTransferResponse& response);

    /**
     * @brief 发送角色保存请求
     * @param server_id 目标服务器ID
     * @param character_id 角色ID
     */
    void SendCharacterSaveRequest(uint16_t server_id, uint64_t character_id);

    /**
     * @brief 发送聊天消息转发
     * @param server_id 目标服务器ID (MurimNetServer)
     * @param from_player 发送者角色ID
     * @param to_player 接收者角色ID (0 表示广播)
     * @param message 消息内容
     */
    void SendChatMessageForward(uint16_t server_id, uint64_t from_player,
                               uint64_t to_player, const std::string& message);

    // ========== 消息处理 ==========

    /**
     * @brief 处理服务器注册请求
     * @param server_id 来源服务器ID
     * @param server_info 服务器信息
     */
    void HandleServerRegister(uint16_t server_id, const Core::Protocol::ServerInfo& server_info);

    /**
     * @brief 处理心跳
     * @param server_id 来源服务器ID
     */
    void HandleHeartbeat(uint16_t server_id);

    /**
     * @brief 处理玩家转移请求
     * @param server_id 来源服务器ID
     * @param request 转移请求
     */
    void HandlePlayerTransferRequest(uint16_t server_id, const Core::Protocol::PlayerTransferRequest& request);

    /**
     * @brief 处理玩家转移响应
     * @param server_id 来源服务器ID
     * @param response 转移响应
     */
    void HandlePlayerTransferResponse(uint16_t server_id, const Core::Protocol::PlayerTransferResponse& response);

    /**
     * @brief 处理负载状态报告
     * @param server_id 来源服务器ID
     * @param load_status 负载状态
     */
    void HandleLoadStatusReport(uint16_t server_id, const Core::Protocol::LoadStatusReport& load_status);

    // ========== 状态查询 ==========

    /**
     * @brief 获取已连接的服务器数量
     * @return 已连接的服务器数量
     */
    size_t GetConnectedServerCount() const { return connected_servers_.size(); }

    /**
     * @brief 获取所有已连接的服务器
     * @return 服务器连接映射
     */
    const std::unordered_map<uint16_t, ServerConnectionInfo>& GetConnectedServers() const {
        return connected_servers_;
    }

    // ========== 配置 ==========

    /**
     * @brief 设置AgentServer地址
     */
    void SetAgentServerAddress(const std::string& ip, uint16_t port) {
        agent_server_ip_ = ip;
        agent_server_port_ = port;
    }

    /**
     * @brief 设置MurimNetServer地址
     */
    void SetMurimNetServerAddress(const std::string& ip, uint16_t port) {
        murimnet_server_ip_ = ip;
        murimnet_server_port_ = port;
    }

private:
    ServerCommunicationManager() = default;
    ~ServerCommunicationManager() = default;
    ServerCommunicationManager(const ServerCommunicationManager&) = delete;
    ServerCommunicationManager& operator=(const ServerCommunicationManager&) = delete;

    // ========== 成员变量 ==========

    std::unordered_map<uint16_t, ServerConnectionInfo> connected_servers_;  // 已连接的服务器
    mutable std::mutex mutex_;                                          // 线程安全锁

    // 配置
    std::string agent_server_ip_ = "127.0.0.1";
    uint16_t agent_server_port_ = 7000;
    std::string murimnet_server_ip_ = "127.0.0.1";
    uint16_t murimnet_server_port_ = 8500;

    // 心跳配置
    uint32_t heartbeat_interval_ = 5000;  // 心跳间隔 (ms)
    uint32_t heartbeat_timeout_ = 15000;  // 心跳超时 (ms)

    // ========== 辅助方法 ==========

    /**
     * @brief 心跳定时器回调
     */
    void OnHeartbeatTimer();

    /**
     * @brief 检查服务器超时
     */
    void CheckServerTimeouts();
};

} // namespace MapServer
} // namespace Murim
