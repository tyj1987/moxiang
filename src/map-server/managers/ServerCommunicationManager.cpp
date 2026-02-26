/**
 * ServerCommunicationManager.cpp
 *
 * 服务器间通信管理器实现
 */

#include "ServerCommunicationManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <chrono>

namespace Murim {
namespace MapServer {

ServerCommunicationManager& ServerCommunicationManager::Instance() {
    static ServerCommunicationManager instance;
    return instance;
}

bool ServerCommunicationManager::Initialize() {
    spdlog::info("Initializing ServerCommunicationManager...");

    // 加载配置
    // TODO: 从配置文件读取服务器地址

    spdlog::info("AgentServer: {}:{}", agent_server_ip_, agent_server_port_);
    spdlog::info("MurimNetServer: {}:{}", murimnet_server_ip_, murimnet_server_port_);

    spdlog::info("ServerCommunicationManager initialized successfully");
    return true;
}

bool ServerCommunicationManager::Start() {
    spdlog::info("Starting ServerCommunicationManager...");

    // 连接到 AgentServer
    // TODO: AgentServer 的 server_id 通常为 1
    if (!ConnectToServer(1, Core::Protocol::ServerType::kAgent, agent_server_ip_, agent_server_port_)) {
        spdlog::warn("Failed to connect to AgentServer, will retry later");
    }

    // 连接到 MurimNetServer (如果需要)
    // TODO: MurimNetServer 的 server_id 通常为 2
    // if (!ConnectToServer(2, Core::Protocol::ServerType::kMurimNet, murimnet_server_ip_, murimnet_server_port_)) {
    //     spdlog::warn("Failed to connect to MurimNetServer, will retry later");
    // }

    spdlog::info("ServerCommunicationManager started successfully");
    return true;
}

void ServerCommunicationManager::Stop() {
    spdlog::info("Stopping ServerCommunicationManager...");

    // 断开所有服务器连接
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [server_id, conn] : connected_servers_) {
        if (conn.connection) {
            conn.connection->Disconnect(true);  // graceful = true
        }
    }
    connected_servers_.clear();

    spdlog::info("ServerCommunicationManager stopped");
}

void ServerCommunicationManager::Shutdown() {
    Stop();
    spdlog::info("ServerCommunicationManager shutdown complete");
}

// ========== 连接管理 ==========

bool ServerCommunicationManager::ConnectToServer(uint16_t server_id, Core::Protocol::ServerType type,
                                                const std::string& ip, uint16_t port) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否已连接
    if (connected_servers_.find(server_id) != connected_servers_.end()) {
        spdlog::warn("Already connected to server: id={}", server_id);
        return true;
    }

    spdlog::info("Connecting to server: id={}, type={}, {}:{}", server_id, static_cast<int>(type), ip, port);

    try {
        // 创建服务器连接
        // TODO: 这里需要实际的 IOContext 实例
        // 暂时设置为 nullptr,等后续实现时再完善
        Core::Network::ServerConnection::Ptr connection = nullptr;

        ServerConnectionInfo server_conn;
        server_conn.server_id = server_id;
        server_conn.type = type;
        server_conn.ip = ip;
        server_conn.port = port;
        server_conn.connection = connection;
        server_conn.last_heartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        server_conn.is_connected = true;

        connected_servers_[server_id] = server_conn;

        spdlog::info("Successfully connected to server: id={}", server_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to connect to server: id={}, error={}", server_id, e.what());
        return false;
    }
}

void ServerCommunicationManager::DisconnectFromServer(uint16_t server_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connected_servers_.find(server_id);
    if (it == connected_servers_.end()) {
        spdlog::warn("Server not found: id={}", server_id);
        return;
    }

    spdlog::info("Disconnecting from server: id={}", server_id);

    if (it->second.connection) {
        it->second.connection->Disconnect(true);  // graceful = true
    }

    connected_servers_.erase(it);

    spdlog::info("Disconnected from server: id={}", server_id);
}

bool ServerCommunicationManager::IsServerConnected(uint16_t server_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connected_servers_.find(server_id);
    return it != connected_servers_.end() && it->second.is_connected;
}

ServerConnectionInfo* ServerCommunicationManager::GetServerConnection(uint16_t server_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connected_servers_.find(server_id);
    if (it != connected_servers_.end()) {
        return &it->second;
    }

    return nullptr;
}

// ========== 消息发送 ==========

void ServerCommunicationManager::SendServerRegister(uint16_t server_id, const Core::Protocol::ServerInfo& server_info) {
    auto* conn = GetServerConnection(server_id);
    if (!conn || !conn->is_connected) {
        spdlog::warn("Cannot send server register: server {} not connected", server_id);
        return;
    }

    spdlog::info("Sending server register to server {}: type={}, id={}",
                 server_id, static_cast<int>(server_info.type), server_info.server_id);

    // TODO: 序列化 server_info 并发送
    // conn->connection->Send(...);
}

void ServerCommunicationManager::SendHeartbeat(uint16_t server_id) {
    auto* conn = GetServerConnection(server_id);
    if (!conn || !conn->is_connected) {
        return;
    }

    // 更新最后心跳时间
    conn->last_heartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // TODO: 发送心跳消息
    spdlog::trace("Sent heartbeat to server {}", server_id);
}

void ServerCommunicationManager::SendLoadStatusReport(uint16_t server_id, const Core::Protocol::LoadStatusReport& load_status) {
    auto* conn = GetServerConnection(server_id);
    if (!conn || !conn->is_connected) {
        spdlog::warn("Cannot send load status report: server {} not connected", server_id);
        return;
    }

    spdlog::debug("Sending load status report to server {}: players={}/{}",
                  server_id, load_status.current_players, load_status.max_players);

    // TODO: 序列化 load_status 并发送
}

void ServerCommunicationManager::SendPlayerTransferResponse(uint16_t server_id, const Core::Protocol::PlayerTransferResponse& response) {
    auto* conn = GetServerConnection(server_id);
    if (!conn || !conn->is_connected) {
        spdlog::warn("Cannot send player transfer response: server {} not connected", server_id);
        return;
    }

    spdlog::info("Sending player transfer response to server {}: character_id={}, result={}",
                 server_id, response.character_id, response.result_code);

    // TODO: 序列化 response 并发送
}

void ServerCommunicationManager::SendCharacterSaveRequest(uint16_t server_id, uint64_t character_id) {
    auto* conn = GetServerConnection(server_id);
    if (!conn || !conn->is_connected) {
        spdlog::warn("Cannot send character save request: server {} not connected", server_id);
        return;
    }

    spdlog::info("Sending character save request to server {}: character_id={}",
                 server_id, character_id);

    // TODO: 序列化并发送
}

void ServerCommunicationManager::SendChatMessageForward(uint16_t server_id, uint64_t from_player,
                                                       uint64_t to_player, const std::string& message) {
    auto* conn = GetServerConnection(server_id);
    if (!conn || !conn->is_connected) {
        spdlog::warn("Cannot send chat message forward: server {} not connected", server_id);
        return;
    }

    spdlog::debug("Sending chat message forward to server {}: from={} to={}",
                  server_id, from_player, to_player);

    // TODO: 序列化并发送
}

// ========== 消息处理 ==========

void ServerCommunicationManager::HandleServerRegister(uint16_t server_id, const Core::Protocol::ServerInfo& server_info) {
    spdlog::info("Received server register from server {}: type={}, id={}",
                 server_id, static_cast<int>(server_info.type), server_info.server_id);

    // TODO: 处理服务器注册请求
    // 1. 验证服务器信息
    // 2. 添加到服务器列表
    // 3. 发送注册确认
}

void ServerCommunicationManager::HandleHeartbeat(uint16_t server_id) {
    spdlog::trace("Received heartbeat from server {}", server_id);

    auto* conn = GetServerConnection(server_id);
    if (conn) {
        conn->last_heartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // TODO: 发送心跳响应
}

void ServerCommunicationManager::HandlePlayerTransferRequest(uint16_t server_id, const Core::Protocol::PlayerTransferRequest& request) {
    spdlog::info("Received player transfer request from server {}: account_id={}, character_id={}, name={}",
                 server_id, request.account_id, request.character_id, request.character_name);

    // TODO: 处理玩家转移请求
    // 1. 验证请求
    // 2. 加载玩家数据
    // 3. 创建玩家实体
    // 4. 发送转移响应

    // 临时实现: 发送成功响应
    Core::Protocol::PlayerTransferResponse response;
    response.character_id = request.character_id;
    response.result_code = static_cast<uint32_t>(Core::Protocol::ServerResultCode::kSuccess);
    response.message = "Transfer successful";
    response.map_server_id = 0;  // TODO: 当前服务器ID
    response.map_id = 1;
    response.position_x = 100.0f;
    response.position_y = 100.0f;
    response.position_z = 0.0f;

    SendPlayerTransferResponse(server_id, response);
}

void ServerCommunicationManager::HandlePlayerTransferResponse(uint16_t server_id, const Core::Protocol::PlayerTransferResponse& response) {
    spdlog::info("Received player transfer response from server {}: character_id={}, result={}",
                 server_id, response.character_id, response.result_code);

    // TODO: 处理转移响应
    // MapServer 通常不会收到这个消息(它只发送响应)
}

void ServerCommunicationManager::HandleLoadStatusReport(uint16_t server_id, const Core::Protocol::LoadStatusReport& load_status) {
    spdlog::debug("Received load status report from server {}: players={}/{}",
                  server_id, load_status.current_players, load_status.max_players);

    // TODO: 更新服务器负载信息
    // 用于负载均衡决策
}

// ========== 辅助方法 ==========

void ServerCommunicationManager::OnHeartbeatTimer() {
    // 向所有已连接的服务器发送心跳
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [server_id, conn] : connected_servers_) {
        if (conn.is_connected) {
            SendHeartbeat(server_id);
        }
    }

    // 检查超时
    CheckServerTimeouts();
}

void ServerCommunicationManager::CheckServerTimeouts() {
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    std::vector<uint16_t> timeout_servers;

    for (const auto& [server_id, conn] : connected_servers_) {
        if (conn.is_connected && (now - conn.last_heartbeat) > heartbeat_timeout_) {
            timeout_servers.push_back(server_id);
        }
    }

    for (uint16_t server_id : timeout_servers) {
        spdlog::warn("Server heartbeat timeout: id={}", server_id);
        DisconnectFromServer(server_id);
    }
}

} // namespace MapServer
} // namespace Murim
