#include "DistributeServerManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>

namespace Murim {
namespace DistributeServer {

DistributeServerManager& DistributeServerManager::Instance() {
    static DistributeServerManager instance;
    return instance;
}

bool DistributeServerManager::Initialize() {
    spdlog::info("[DistributeServer] Initializing DistributeServer...");

    if (status_ != ServerStatus::kStopped) {
        spdlog::error("[DistributeServer] Server already initialized!");
        return false;
    }

    status_ = ServerStatus::kStarting;

    // 初始化SessionManager
    if (!session_manager_->Initialize()) {
        spdlog::error("[DistributeServer] Failed to initialize SessionManager!");
        status_ = ServerStatus::kStopped;
        return false;
    }

    spdlog::info("[DistributeServer] DistributeServer initialized successfully.");
    status_ = ServerStatus::kStarting;
    return true;
}

bool DistributeServerManager::Start() {
    spdlog::info("[DistributeServer] Starting DistributeServer...");

    if (status_ != ServerStatus::kStarting) {
        spdlog::error("[DistributeServer] Server not in starting state!");
        return false;
    }

    try {
        // 1. 初始化网络层
        spdlog::info("[DistributeServer] Initializing network layer...");
        io_context_ = std::make_unique<Core::Network::IOContext>();

        // 2. 创建Acceptor
        acceptor_ = std::make_unique<Core::Network::Acceptor>(
            *io_context_,
            "0.0.0.0",  // 监听所有接口
            server_port_
        );

        // 3. 设置连接回调
        acceptor_->Start([this](Core::Network::Connection::Ptr conn) {
            spdlog::info("[DistributeServer] New connection from: {}",
                        conn->GetRemoteAddress());

            // DistributeServer 接受其他服务器（AgentServer, MapServer）的连接
            // TODO: 实现服务器间通信协议

            // 简单处理：保持连接但不做特殊处理
            conn->SetMessageHandler([this](const Core::Network::ByteBuffer& buffer) {
                // TODO: 处理来自其他服务器的消息（心跳、注册等）
                spdlog::debug("[DistributeServer] Received message from connected server");
            });

            // 开始接收数据
            conn->AsyncReceive();
        });

        spdlog::info("[DistributeServer] Network acceptor started on port {}", server_port_);

        // 4. 启动IO线程
        io_running_ = true;
        io_thread_ = std::thread([this]() {
            spdlog::info("[DistributeServer] IO thread started");
            try {
                // Create a dummy timer to keep io_context running
                auto dummy_timer = std::make_shared<boost::asio::steady_timer>(io_context_->GetIOContext());
                dummy_timer->expires_after(std::chrono::hours(999999));
                dummy_timer->async_wait([](const boost::system::error_code&) {});

                // Directly call boost::asio::io_context::run() which blocks
                io_context_->GetIOContext().run();
            } catch (const std::exception& e) {
                spdlog::error("[DistributeServer] IO thread error: {}", e.what());
            }
            spdlog::info("[DistributeServer] IO thread stopped");
        });

        // 5. 启动SessionManager
        if (!session_manager_->Start()) {
            spdlog::error("[DistributeServer] Failed to start SessionManager!");
            throw std::runtime_error("Failed to start SessionManager");
        }

        status_ = ServerStatus::kRunning;
        last_update_ = std::chrono::steady_clock::now();

        spdlog::info("[DistributeServer] DistributeServer started successfully.");
        spdlog::info("[DistributeServer] Server ID: {}", server_id_);
        spdlog::info("[DistributeServer] Listening port: {}", server_port_);
        spdlog::info("[DistributeServer] Tick rate: {}ms", tick_rate_);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("[DistributeServer] Failed to start: {}", e.what());
        status_ = ServerStatus::kStopped;

        // 清理资源
        if (acceptor_) {
            acceptor_->Stop();
            acceptor_.reset();
        }
        if (io_context_) {
            io_context_->Stop();
        }
        if (io_thread_.joinable()) {
            io_running_ = false;
            io_context_->Stop();
            io_thread_.join();
        }
        io_context_.reset();

        return false;
    }
}

void DistributeServerManager::Stop() {
    spdlog::info("[DistributeServer] Stopping DistributeServer...");

    if (status_ != ServerStatus::kRunning) {
        spdlog::warn("[DistributeServer] Server not running!");
        return;
    }

    status_ = ServerStatus::kStopping;

    // 1. 停止 Acceptor
    if (acceptor_) {
        spdlog::info("[DistributeServer] Stopping network acceptor...");
        acceptor_->Stop();
        acceptor_.reset();
    }

    // 2. 停止 IO 线程
    if (io_running_) {
        spdlog::info("[DistributeServer] Stopping IO thread...");
        io_running_ = false;
        if (io_context_) {
            io_context_->Stop();
        }
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }

    // 3. 清理 IO context
    if (io_context_) {
        io_context_.reset();
    }

    // 4. 停止SessionManager
    session_manager_->Stop();

    spdlog::info("[DistributeServer] DistributeServer stopped.");
}

void DistributeServerManager::Shutdown() {
    spdlog::info("[DistributeServer] Shutting down DistributeServer...");

    Stop();

    // 清理所有服务器注册
    map_servers_.clear();
    agent_servers_.clear();

    status_ = ServerStatus::kStopped;

    spdlog::info("[DistributeServer] DistributeServer shutdown complete.");
}

// ========== 服务器注册 ==========

bool DistributeServerManager::RegisterMapServer(
    uint16_t server_id,
    uint16_t map_id,
    const std::string& server_name,
    const std::string& listen_ip,
    uint16_t listen_port,
    uint32_t max_players
) {
    spdlog::info("[DistributeServer] Registering MapServer: {} (ID: {}, Map: {})",
                 server_name, server_id, map_id);

    if (map_servers_.find(server_id) != map_servers_.end()) {
        spdlog::warn("[DistributeServer] MapServer {} already registered, updating...",
                     server_id);
    }

    MapServerInfo info;
    info.server_id = server_id;
    info.map_id = map_id;
    info.server_name = server_name;
    info.listen_ip = listen_ip;
    info.listen_port = listen_port;
    info.player_count = 0;
    info.max_players = max_players;
    info.cpu_usage = 0;
    info.memory_usage = 0;
    info.available = true;
    info.last_heartbeat = std::chrono::system_clock::now();

    map_servers_[server_id] = info;

    spdlog::info("[DistributeServer] MapServer {} registered successfully.", server_id);
    BroadcastServerStatus();

    return true;
}

bool DistributeServerManager::RegisterAgentServer(
    uint16_t server_id,
    const std::string& server_name,
    const std::string& listen_ip,
    uint16_t listen_port,
    uint32_t max_connections
) {
    spdlog::info("[DistributeServer] Registering AgentServer: {} (ID: {})",
                 server_name, server_id);

    if (agent_servers_.find(server_id) != agent_servers_.end()) {
        spdlog::warn("[DistributeServer] AgentServer {} already registered, updating...",
                     server_id);
    }

    AgentServerInfo info;
    info.server_id = server_id;
    info.server_name = server_name;
    info.listen_ip = listen_ip;
    info.listen_port = listen_port;
    info.connection_count = 0;
    info.max_connections = max_connections;
    info.available = true;
    info.last_heartbeat = std::chrono::system_clock::now();

    agent_servers_[server_id] = info;

    spdlog::info("[DistributeServer] AgentServer {} registered successfully.", server_id);
    BroadcastServerStatus();

    return true;
}

void DistributeServerManager::UnregisterServer(uint16_t server_id, const std::string& server_type) {
    spdlog::info("[DistributeServer] Unregistering {} server: {}",
                 server_type, server_id);

    if (server_type == "map") {
        auto it = map_servers_.find(server_id);
        if (it != map_servers_.end()) {
            map_servers_.erase(it);
            spdlog::info("[DistributeServer] MapServer {} unregistered.", server_id);
        } else {
            spdlog::warn("[DistributeServer] MapServer {} not found.", server_id);
        }
    } else if (server_type == "agent") {
        auto it = agent_servers_.find(server_id);
        if (it != agent_servers_.end()) {
            agent_servers_.erase(it);
            spdlog::info("[DistributeServer] AgentServer {} unregistered.", server_id);
        } else {
            spdlog::warn("[DistributeServer] AgentServer {} not found.", server_id);
        }
    } else {
        spdlog::error("[DistributeServer] Invalid server type: {}", server_type);
        return;
    }

    BroadcastServerStatus();
}

void DistributeServerManager::UpdateHeartbeat(uint16_t server_id, const std::string& server_type) {
    if (server_type == "map") {
        auto it = map_servers_.find(server_id);
        if (it != map_servers_.end()) {
            it->second.last_heartbeat = std::chrono::system_clock::now();
            it->second.available = true;
        }
    } else if (server_type == "agent") {
        auto it = agent_servers_.find(server_id);
        if (it != agent_servers_.end()) {
            it->second.last_heartbeat = std::chrono::system_clock::now();
            it->second.available = true;
        }
    }
}

void DistributeServerManager::UpdateMapServerLoad(
    uint16_t server_id,
    uint32_t player_count,
    uint32_t cpu_usage,
    uint32_t memory_usage
) {
    auto it = map_servers_.find(server_id);
    if (it != map_servers_.end()) {
        it->second.player_count = player_count;
        it->second.cpu_usage = cpu_usage;
        it->second.memory_usage = memory_usage;
        it->second.last_heartbeat = std::chrono::system_clock::now();

        // 如果负载过高,标记为不可用
        if (player_count >= it->second.max_players ||
            cpu_usage > 90 || memory_usage > 90) {
            it->second.available = false;
            spdlog::warn("[DistributeServer] MapServer {} overloaded, marking unavailable.",
                         server_id);
        } else {
            it->second.available = true;
        }
    }
}

void DistributeServerManager::UpdateAgentServerLoad(
    uint16_t server_id,
    uint32_t connection_count
) {
    auto it = agent_servers_.find(server_id);
    if (it != agent_servers_.end()) {
        it->second.connection_count = connection_count;
        it->second.last_heartbeat = std::chrono::system_clock::now();

        // 如果连接数过高,标记为不可用
        if (connection_count >= it->second.max_connections) {
            it->second.available = false;
            spdlog::warn("[DistributeServer] AgentServer {} full, marking unavailable.",
                         server_id);
        } else {
            it->second.available = true;
        }
    }
}

// ========== 负载均衡 ==========

uint16_t DistributeServerManager::GetBestMapServer(uint16_t map_id) {
    auto server_info = GetLeastLoadedMapServer(map_id);
    if (server_info.has_value()) {
        return server_info->server_id;
    }
    return 0;
}

uint16_t DistributeServerManager::GetBestAgentServer() {
    auto server_info = GetLeastLoadedAgentServer();
    if (server_info.has_value()) {
        return server_info->server_id;
    }
    return 0;
}

std::optional<DistributeServerManager::MapServerInfo>
DistributeServerManager::GetLeastLoadedMapServer(uint16_t map_id) {
    std::vector<MapServerInfo> candidates;

    // 收集所有匹配地图且可用的服务器
    for (const auto& pair : map_servers_) {
        const auto& server = pair.second;
        if (server.map_id == map_id && server.available) {
            candidates.push_back(server);
        }
    }

    if (candidates.empty()) {
        spdlog::warn("[DistributeServer] No available MapServer for map {}", map_id);
        return std::nullopt;
    }

    // 根据策略选择服务器
    if (load_balance_strategy_ == LoadBalanceStrategy::kLeastConnected) {
        // 选择玩家最少的服务器
        auto it = std::min_element(candidates.begin(), candidates.end(),
            [](const MapServerInfo& a, const MapServerInfo& b) {
                return a.player_count < b.player_count;
            });
        return *it;
    } else if (load_balance_strategy_ == LoadBalanceStrategy::kWeighted) {
        // 根据负载分数选择
        auto it = std::min_element(candidates.begin(), candidates.end(),
            [this](const MapServerInfo& a, const MapServerInfo& b) {
                return CalculateLoadScore(a) < CalculateLoadScore(b);
            });
        return *it;
    } else {
        // 轮询
        if (round_robin_index_ >= candidates.size()) {
            round_robin_index_ = 0;
        }
        return candidates[round_robin_index_++];
    }
}

std::optional<DistributeServerManager::AgentServerInfo>
DistributeServerManager::GetLeastLoadedAgentServer() {
    std::vector<AgentServerInfo> candidates;

    // 收集所有可用的AgentServer
    for (const auto& pair : agent_servers_) {
        const auto& server = pair.second;
        if (server.available) {
            candidates.push_back(server);
        }
    }

    if (candidates.empty()) {
        spdlog::warn("[DistributeServer] No available AgentServer");
        return std::nullopt;
    }

    // 选择连接数最少的服务器
    auto it = std::min_element(candidates.begin(), candidates.end(),
        [](const AgentServerInfo& a, const AgentServerInfo& b) {
            return a.connection_count < b.connection_count;
        });
    return *it;
}

// ========== 服务器查询 ==========

std::vector<DistributeServerManager::MapServerInfo>
DistributeServerManager::GetAllMapServers() {
    std::vector<MapServerInfo> servers;
    for (const auto& pair : map_servers_) {
        servers.push_back(pair.second);
    }
    return servers;
}

std::vector<DistributeServerManager::AgentServerInfo>
DistributeServerManager::GetAllAgentServers() {
    std::vector<AgentServerInfo> servers;
    for (const auto& pair : agent_servers_) {
        servers.push_back(pair.second);
    }
    return servers;
}

std::vector<DistributeServerManager::MapServerInfo>
DistributeServerManager::GetMapServersByMap(uint16_t map_id) {
    std::vector<MapServerInfo> servers;
    for (const auto& pair : map_servers_) {
        if (pair.second.map_id == map_id) {
            servers.push_back(pair.second);
        }
    }
    return servers;
}

std::optional<DistributeServerManager::MapServerInfo>
DistributeServerManager::GetMapServerInfo(uint16_t server_id) {
    auto it = map_servers_.find(server_id);
    if (it != map_servers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<DistributeServerManager::AgentServerInfo>
DistributeServerManager::GetAgentServerInfo(uint16_t server_id) {
    auto it = agent_servers_.find(server_id);
    if (it != agent_servers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool DistributeServerManager::IsServerAvailable(uint16_t server_id, const std::string& server_type) {
    if (server_type == "map") {
        auto it = map_servers_.find(server_id);
        return it != map_servers_.end() && it->second.available;
    } else if (server_type == "agent") {
        auto it = agent_servers_.find(server_id);
        return it != agent_servers_.end() && it->second.available;
    }
    return false;
}

// ========== 服务器循环 ==========

void DistributeServerManager::MainLoop() {
    auto tick_interval = std::chrono::milliseconds(tick_rate_);

    while (status_ == ServerStatus::kRunning) {
        // 计算时间增量
        auto now = std::chrono::steady_clock::now();
        auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_update_
        ).count();

        // 更新服务器
        Update(static_cast<uint32_t>(delta_time));

        // 等待下一个tick
        std::this_thread::sleep_for(tick_interval);
        last_update_ = std::chrono::steady_clock::now();
    }
}

void DistributeServerManager::Update(uint32_t delta_time) {
    // 处理网络消息
    session_manager_->Update(delta_time);

    // 执行健康检查
    PerformHealthCheck();

    // 清理过期服务器
    CleanupStaleServers();

    // 处理服务器tick
    ProcessTick();

    // 定期广播状态 (每5秒)
    static uint32_t broadcast_timer = 0;
    broadcast_timer += delta_time;
    if (broadcast_timer >= 5000) {
        broadcast_timer = 0;
        BroadcastServerStatus();
    }
}

// ========== 辅助方法 ==========

void DistributeServerManager::ProcessTick() {
    // 对应 legacy: DistributeServer主循环的每帧处理

    // 定期日志输出 (暂时禁用,避免日志格式化问题)
    // static uint32_t log_counter = 0;
    // log_counter++;

    // if (log_counter >= 10) {  // 每1秒 (10 ticks * 100ms)
    //     log_counter = 0;
    //     spdlog::debug("[DistributeServer] MapServers: {}, AgentServers: {}",
    //                   map_servers_.size(), agent_servers_.size());
    // }
}

void DistributeServerManager::PerformHealthCheck() {
    // 对应 legacy: DistributeServer健康检查

    auto now = std::chrono::system_clock::now();

    // 检查MapServer健康状态
    for (auto& pair : map_servers_) {
        auto& server = pair.second;
        auto heartbeat_age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - server.last_heartbeat
        ).count();

        // 如果心跳超时,标记为不可用
        if (heartbeat_age > heartbeat_timeout_) {
            if (server.available) {
                spdlog::warn("[DistributeServer] MapServer {} heartbeat timeout ({}ms)",
                             server.server_id, heartbeat_age);
                server.available = false;
            }
        }
    }

    // 检查AgentServer健康状态
    for (auto& pair : agent_servers_) {
        auto& server = pair.second;
        auto heartbeat_age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - server.last_heartbeat
        ).count();

        if (heartbeat_age > heartbeat_timeout_) {
            if (server.available) {
                spdlog::warn("[DistributeServer] AgentServer {} heartbeat timeout ({}ms)",
                             server.server_id, heartbeat_age);
                server.available = false;
            }
        }
    }
}

void DistributeServerManager::CleanupStaleServers() {
    // 对应 legacy: DistributeServer清理过期服务器

    auto now = std::chrono::system_clock::now();
    const uint32_t stale_timeout = 30000;  // 30秒无心跳则删除

    // 清理过期MapServer
    std::vector<uint16_t> stale_map_servers;
    for (const auto& pair : map_servers_) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - pair.second.last_heartbeat
        ).count();
        if (age > stale_timeout) {
            stale_map_servers.push_back(pair.first);
        }
    }

    for (uint16_t server_id : stale_map_servers) {
        spdlog::warn("[DistributeServer] Removing stale MapServer {}", server_id);
        map_servers_.erase(server_id);
    }

    // 清理过期AgentServer
    std::vector<uint16_t> stale_agent_servers;
    for (const auto& pair : agent_servers_) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - pair.second.last_heartbeat
        ).count();
        if (age > stale_timeout) {
            stale_agent_servers.push_back(pair.first);
        }
    }

    for (uint16_t server_id : stale_agent_servers) {
        spdlog::warn("[DistributeServer] Removing stale AgentServer {}", server_id);
        agent_servers_.erase(server_id);
    }
}

double DistributeServerManager::CalculateLoadScore(const MapServerInfo& server_info) {
    // 综合计算负载分数
    // 考虑因素: 玩家数量 (40%), CPU使用 (30%), 内存使用 (30%)

    double player_ratio = static_cast<double>(server_info.player_count) /
                         static_cast<double>(server_info.max_players);
    double cpu_ratio = static_cast<double>(server_info.cpu_usage) / 100.0;
    double memory_ratio = static_cast<double>(server_info.memory_usage) / 100.0;

    return player_ratio * 0.4 + cpu_ratio * 0.3 + memory_ratio * 0.3;
}

double DistributeServerManager::CalculateLoadScore(const AgentServerInfo& server_info) {
    // AgentServer负载分数
    double connection_ratio = static_cast<double>(server_info.connection_count) /
                             static_cast<double>(server_info.max_connections);
    return connection_ratio;
}

void DistributeServerManager::BroadcastServerStatus() {
    // 对应 legacy: DistributeServer广播服务器状态

    spdlog::info("[DistributeServer] Server Status Broadcast:");
    spdlog::info("  MapServers: {}", map_servers_.size());
    for (const auto& pair : map_servers_) {
        const auto& server = pair.second;
        spdlog::info("    [{}] {} - Players: {}/{} CPU: {}% Mem: {}% {}",
                     server.server_id, server.server_name,
                     server.player_count, server.max_players,
                     server.cpu_usage, server.memory_usage,
                     server.available ? "[ONLINE]" : "[OFFLINE]");
    }

    spdlog::info("  AgentServers: {}", agent_servers_.size());
    for (const auto& pair : agent_servers_) {
        const auto& server = pair.second;
        spdlog::info("    [{}] {} - Connections: {}/{} {}",
                     server.server_id, server.server_name,
                     server.connection_count, server.max_connections,
                     server.available ? "[ONLINE]" : "[OFFLINE]");
    }
}

} // namespace DistributeServer
} // namespace Murim
