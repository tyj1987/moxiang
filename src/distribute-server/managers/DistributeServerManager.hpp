#pragma once

#include <memory>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include "core/network/SessionManager.hpp"
#include "core/network/Acceptor.hpp"
#include "core/network/IOContext.hpp"

namespace Murim {
namespace DistributeServer {

/**
 * @brief DistributeServer管理器
 *
 * 负责DistributeServer的核心逻辑
 * - 负载均衡
 * - 服务器发现
 * - 健康检查
 * - 连接分发
 *
 * 对应 legacy: DistributeServer主逻辑
 */
class DistributeServerManager {
public:
    /**
     * @brief 获取单例实例
     */
    static DistributeServerManager& Instance();

    // ========== 初始化 ==========

    /**
     * @brief 初始化DistributeServer
     * @return 是否初始化成功
     */
    bool Initialize();

    /**
     * @brief 启动DistributeServer
     * @return 是否启动成功
     */
    bool Start();

    /**
     * @brief 停止DistributeServer
     */
    void Stop();

    /**
     * @brief 关闭DistributeServer
     */
    void Shutdown();

    // ========== 服务器注册 ==========

    /**
     * @brief MapServer注册信息
     */
    struct MapServerInfo {
        uint16_t server_id;
        uint16_t map_id;
        std::string server_name;
        std::string listen_ip;
        uint16_t listen_port;
        uint32_t player_count;
        uint32_t max_players;
        uint32_t cpu_usage;
        uint32_t memory_usage;
        bool available;
        std::chrono::system_clock::time_point last_heartbeat;
    };

    /**
     * @brief AgentServer注册信息
     */
    struct AgentServerInfo {
        uint16_t server_id;
        std::string server_name;
        std::string listen_ip;
        uint16_t listen_port;
        uint32_t connection_count;
        uint32_t max_connections;
        bool available;
        std::chrono::system_clock::time_point last_heartbeat;
    };

    /**
     * @brief 注册MapServer
     * @param server_id 服务器ID
     * @param map_id 地图ID
     * @param server_name 服务器名称
     * @param listen_ip 监听IP
     * @param listen_port 监听端口
     * @param max_players 最大玩家数
     * @return 是否注册成功
     */
    bool RegisterMapServer(
        uint16_t server_id,
        uint16_t map_id,
        const std::string& server_name,
        const std::string& listen_ip,
        uint16_t listen_port,
        uint32_t max_players
    );

    /**
     * @brief 注册AgentServer
     * @param server_id 服务器ID
     * @param server_name 服务器名称
     * @param listen_ip 监听IP
     * @param listen_port 监听端口
     * @param max_connections 最大连接数
     * @return 是否注册成功
     */
    bool RegisterAgentServer(
        uint16_t server_id,
        const std::string& server_name,
        const std::string& listen_ip,
        uint16_t listen_port,
        uint32_t max_connections
    );

    /**
     * @brief 注销服务器
     * @param server_id 服务器ID
     * @param server_type 服务器类型 ("map" 或 "agent")
     */
    void UnregisterServer(uint16_t server_id, const std::string& server_type);

    /**
     * @brief 更新服务器心跳
     * @param server_id 服务器ID
     * @param server_type 服务器类型
     */
    void UpdateHeartbeat(uint16_t server_id, const std::string& server_type);

    /**
     * @brief 更新MapServer负载信息
     * @param server_id 服务器ID
     * @param player_count 玩家数量
     * @param cpu_usage CPU使用率
     * @param memory_usage 内存使用率
     */
    void UpdateMapServerLoad(
        uint16_t server_id,
        uint32_t player_count,
        uint32_t cpu_usage,
        uint32_t memory_usage
    );

    /**
     * @brief 更新AgentServer负载信息
     * @param server_id 服务器ID
     * @param connection_count 连接数量
     */
    void UpdateAgentServerLoad(
        uint16_t server_id,
        uint32_t connection_count
    );

    // ========== 负载均衡 ==========

    /**
     * @brief 获取最佳MapServer
     * @param map_id 地图ID
     * @return MapServer ID (无可用服务器返回0)
     */
    uint16_t GetBestMapServer(uint16_t map_id);

    /**
     * @brief 获取最佳AgentServer
     * @return AgentServer ID (无可用服务器返回0)
     */
    uint16_t GetBestAgentServer();

    /**
     * @brief 获取负载最低的MapServer
     * @param map_id 地图ID
     * @return MapServer信息
     */
    std::optional<MapServerInfo> GetLeastLoadedMapServer(uint16_t map_id);

    /**
     * @brief 获取负载最低的AgentServer
     * @return AgentServer信息
     */
    std::optional<AgentServerInfo> GetLeastLoadedAgentServer();

    // ========== 服务器查询 ==========

    /**
     * @brief 获取所有MapServer列表
     * @return MapServer列表
     */
    std::vector<MapServerInfo> GetAllMapServers();

    /**
     * @brief 获取所有AgentServer列表
     * @return AgentServer列表
     */
    std::vector<AgentServerInfo> GetAllAgentServers();

    /**
     * @brief 获取指定地图的MapServer列表
     * @param map_id 地图ID
     * @return MapServer列表
     */
    std::vector<MapServerInfo> GetMapServersByMap(uint16_t map_id);

    /**
     * @brief 获取MapServer信息
     * @param server_id 服务器ID
     * @return MapServer信息
     */
    std::optional<MapServerInfo> GetMapServerInfo(uint16_t server_id);

    /**
     * @brief 获取AgentServer信息
     * @param server_id 服务器ID
     * @return AgentServer信息
     */
    std::optional<AgentServerInfo> GetAgentServerInfo(uint16_t server_id);

    /**
     * @brief 检查服务器是否可用
     * @param server_id 服务器ID
     * @param server_type 服务器类型
     * @return 是否可用
     */
    bool IsServerAvailable(uint16_t server_id, const std::string& server_type);

    // ========== 状态查询 ==========

    /**
     * @brief 服务器状态
     */
    enum class ServerStatus {
        kStopped,    // 已停止
        kStarting,   // 启动中
        kRunning,    // 运行中
        kStopping    // 停止中
    };

    /**
     * @brief 获取服务器状态
     */
    ServerStatus GetStatus() const { return status_; }

    /**
     * @brief 获取服务器ID
     */
    uint16_t GetServerId() const { return server_id_; }

    /**
     * @brief 获取注册的MapServer数量
     */
    size_t GetMapServerCount() const { return map_servers_.size(); }

    /**
     * @brief 获取注册的AgentServer数量
     */
    size_t GetAgentServerCount() const { return agent_servers_.size(); }

    // ========== 服务器循环 ==========

    /**
     * @brief 服务器主循环
     */
    void MainLoop();

    /**
     * @brief 更新服务器状态
     * @param delta_time 时间增量(毫秒)
     */
    void Update(uint32_t delta_time);

private:
    DistributeServerManager()
        : session_manager_(std::make_unique<Core::Network::SessionManager>()) {}
    ~DistributeServerManager() = default;
    DistributeServerManager(const DistributeServerManager&) = delete;
    DistributeServerManager& operator=(const DistributeServerManager&) = delete;

    // ========== 服务器状态 ==========

    ServerStatus status_ = ServerStatus::kStopped;
    uint16_t server_id_ = 8000;  // DistributeServer默认ID
    uint16_t server_port_ = 8000;  // DistributeServer监听端口

    // ========== 服务器注册表 ==========

    std::unordered_map<uint16_t, MapServerInfo> map_servers_;
    std::unordered_map<uint16_t, AgentServerInfo> agent_servers_;

    // ========== 网络管理 ==========

    std::unique_ptr<Core::Network::SessionManager> session_manager_;
    std::unique_ptr<Core::Network::IOContext> io_context_;
    std::unique_ptr<Core::Network::Acceptor> acceptor_;
    std::thread io_thread_;
    std::atomic<bool> io_running_{false};

    // ========== 服务器配置 ==========

    uint32_t tick_rate_ = 100;  // 服务器tick率(ms)
    uint32_t heartbeat_timeout_ = 5000;  // 心跳超时(ms)
    std::chrono::steady_clock::time_point last_update_;

    // ========== 负载均衡策略 ==========

    enum class LoadBalanceStrategy {
        kRoundRobin,      // 轮询
        kLeastConnected,  // 最少连接
        kWeighted         // 加权
    };

    LoadBalanceStrategy load_balance_strategy_ = LoadBalanceStrategy::kLeastConnected;

    // ========== 辅助方法 ==========

    /**
     * @brief 处理服务器tick
     */
    void ProcessTick();

    /**
     * @brief 执行健康检查
     */
    void PerformHealthCheck();

    /**
     * @brief 清理过期服务器
     */
    void CleanupStaleServers();

    /**
     * @brief 计算服务器负载分数
     * @param server_info 服务器信息
     * @return 负载分数 (越低越好)
     */
    double CalculateLoadScore(const MapServerInfo& server_info);

    /**
     * @brief 计算服务器负载分数
     * @param server_info 服务器信息
     * @return 负载分数 (越低越好)
     */
    double CalculateLoadScore(const AgentServerInfo& server_info);

    /**
     * @brief 广播服务器状态
     */
    void BroadcastServerStatus();

    /**
     * @brief 轮询索引
     */
    uint16_t round_robin_index_ = 0;
};

} // namespace DistributeServer
} // namespace Murim
