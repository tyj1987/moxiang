#pragma once

#include <memory>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "shared/account/LoginManager.hpp"
#include "shared/character/CharacterManager.hpp"
#include "core/network/SessionManager.hpp"
#include "core/network/Acceptor.hpp"
#include "core/network/IOContext.hpp"

namespace Murim {
namespace AgentServer {

/**
 * @brief AgentServer管理器
 *
 * 负责AgentServer的核心逻辑
 * - 账号认证
 * - 角色选择
 * - 连接管理
 * - 负载均衡
 *
 * 对应 legacy: AgentServer主逻辑
 */
class AgentServerManager {
public:
    /**
     * @brief 获取单例实例
     */
    static AgentServerManager& Instance();

    // ========== 初始化 ==========

    /**
     * @brief 初始化AgentServer
     * @return 是否初始化成功
     */
    bool Initialize();

    /**
     * @brief 启动AgentServer
     * @return 是否启动成功
     */
    bool Start();

    /**
     * @brief 停止AgentServer
     */
    void Stop();

    /**
     * @brief 关闭AgentServer
     */
    void Shutdown();

    // ========== 认证相关 ==========

    /**
     * @brief 处理账号登录请求
     * @param username 用户名
     * @param password 密码
     * @return 认证结果
     */
    struct AuthResult {
        bool success;
        uint64_t account_id;
        std::string session_token;
    };

    AuthResult Authenticate(const std::string& username, const std::string& password);

    /**
     * @brief 获取账号的角色列表
     * @param account_id 账号ID
     * @return 角色列表
     */
    struct CharacterInfo {
        uint64_t character_id;
        std::string name;
        uint16_t level;
        uint8_t initial_weapon;  // 初始武器类型：0=内功, 1=剑, 2=刀, 3=枪, 4=拳套, 5=弓, 6=杖, 7=扇, 8=钩
        uint8_t gender;
    };

    std::vector<CharacterInfo> GetCharacterList(uint64_t account_id);

    /**
     * @brief 选择角色进入游戏
     * @param account_id 账号ID
     * @param character_id 角色ID
     * @param map_server_id 目标MapServer ID
     * @return 是否成功
     */
    bool SelectCharacter(
        uint64_t account_id,
        uint64_t character_id,
        uint16_t map_server_id
    );

    /**
     * @brief 玩家退出游戏
     * @param account_id 账号ID
     * @param character_id 角色ID
     */
    void PlayerLogout(uint64_t account_id, uint64_t character_id);

    // ========== 连接管理 ==========

    /**
     * @brief 添加连接
     * @param session_id 会话ID
     * @param account_id 账号ID (未认证时为0)
     */
    void AddConnection(uint64_t session_id, uint64_t account_id = 0);

    /**
     * @brief 移除连接
     * @param session_id 会话ID
     */
    void RemoveConnection(uint64_t session_id);

    /**
     * @brief 获取会话对应的账号ID
     * @param session_id 会话ID
     * @return 账号ID (未认证返回0)
     */
    uint64_t GetAccountId(uint64_t session_id) const;

    /**
     * @brief 检查会话是否已认证
     * @param session_id 会话ID
     * @return 是否已认证
     */
    bool IsSessionAuthenticated(uint64_t session_id) const;

    // ========== 负载均衡 ==========

    /**
     * @brief 获取最佳MapServer ID
     * @param map_id 地图ID
     * @return MapServer ID
     */
    uint16_t GetBestMapServer(uint16_t map_id);

    /**
     * @brief 获取MapServer状态
     * @param server_id MapServer ID
     * @return 是否可用
     */
    bool IsMapServerAvailable(uint16_t server_id);

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

    /**
     * @brief 广播服务器状态
     */
    void BroadcastServerStatus();

private:
    AgentServerManager() = default;
    ~AgentServerManager() = default;
    AgentServerManager(const AgentServerManager&) = delete;
    AgentServerManager& operator=(const AgentServerManager&) = delete;

    // ========== 服务器状态 ==========

    enum class ServerStatus {
        kStopped,    // 已停止
        kStarting,   // 启动中
        kRunning,    // 运行中
        kStopping    // 停止中
    };

    ServerStatus status_ = ServerStatus::kStopped;
    uint16_t server_port_ = 7000;  // AgentServer监听端口
    uint16_t server_id_ = 1;       // AgentServer ID

    // ========== Manager实例 ==========

    Game::LoginManager* login_manager_;
    Game::CharacterManager* character_manager_;

    // ========== 网络层 ==========

    std::unique_ptr<Core::Network::IOContext> io_context_;
    std::unique_ptr<Core::Network::Acceptor> acceptor_;
    std::thread io_thread_;
    std::atomic<bool> io_running_{false};

    // 网络连接映射: session_id -> Connection::Ptr
    std::unordered_map<uint64_t, Core::Network::Connection::Ptr> network_connections_;

    // Session接收缓冲区（用于累积TCP分片数据）
    std::unordered_map<uint64_t, std::vector<uint8_t>> session_receive_buffers_;

    // ========== Mock角色存储 (暂时使用，等待PostgreSQL实现) ==========

    struct CharacterData {
        uint64_t character_id;
        uint64_t account_id;
        std::string name;
        uint8_t initial_weapon;  // 初始武器类型：0=内功, 1=剑, 2=刀, 3=枪, 4=拳套, 5=弓, 6=杖, 7=扇, 8=钩
        uint8_t gender;
        uint32_t level;
    };

    // 存储所有创建的角色 (character_id -> CharacterData)
    std::unordered_map<uint64_t, CharacterData> mock_characters_;

    // 账号的角色列表 (account_id -> vector of character_ids)
    std::unordered_map<uint64_t, std::vector<uint64_t>> mock_account_characters_;

    // 下一个可用的character_id
    uint64_t next_character_id_ = 1000;

    // ========== 连接管理 ==========

    struct ConnectionInfo {
        uint64_t session_id;
        uint64_t account_id;
        uint64_t character_id;
        uint16_t redirect_server_id;
        std::chrono::system_clock::time_point connect_time;

        enum class ConnectionStatus {
            kConnected,
            kAuthenticating,
            kAuthenticated,
            kServerListSent,
            kCharacterListSent,
            kRedirecting,
            kDisconnected
        };
        ConnectionStatus status = ConnectionStatus::kConnected;
    };

    std::unordered_map<uint64_t, ConnectionInfo> connections_;

    // ========== MapServer状态追踪 ==========

    struct MapServerInfo {
        uint16_t server_id;
        uint16_t map_id;
        std::string server_name;
        std::string server_ip;
        uint16_t server_port;
        uint32_t player_count;
        uint32_t max_players;
        bool available;
        std::chrono::system_clock::time_point last_update;
    };

    std::unordered_map<uint16_t, MapServerInfo> map_servers_;

    // ========== 辅助方法 ==========

    /**
     * @brief 加载服务器列表
     */
    void LoadServerList();

    /**
     * @brief 生成会话令牌
     */
    std::string GenerateSessionToken(uint64_t account_id);

    /**
     * @brief 处理服务器tick
     */
    void ProcessTick();

    /**
     * @brief 检查MapServer健康状态
     */
    void CheckMapServerHealth();

    /**
     * @brief 更新MapServer状态
     */
    void UpdateMapServerStatus();

    /**
     * @brief 重定向玩家到指定MapServer
     */
    void RedirectPlayer(uint64_t account_id, uint16_t target_server_id);

    /**
     * @brief 保存连接状态
     */
    void SaveConnectionState(uint64_t session_id);

    /**
     * @brief 发送服务器列表给客户端
     */
    void SendServerList(uint64_t session_id);

    /**
     * @brief 发送角色列表给客户端
     */
    void SendCharacterList(uint64_t account_id);

    /**
     * @brief 处理客户端消息
     * @param session_id 会话ID
     * @param buffer 消息缓冲区
     */
    void HandleMessage(uint64_t session_id, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 发送数据给客户端
     * @param session_id 会话ID
     * @param data 数据
     * @param length 数据长度
     */
    void SendData(uint64_t session_id, const uint8_t* data, size_t length);

    /**
     * @brief 处理登录请求
     * @param session_id 会话ID
     * @param buffer 消息缓冲区
     */
    void HandleLoginRequest(uint64_t session_id, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理角色列表请求
     * @param session_id 会话ID
     * @param buffer 消息缓冲区
     */
    void HandleCharacterListRequest(uint64_t session_id, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理选择角色请求
     * @param session_id 会话ID
     * @param buffer 消息缓冲区
     */
    void HandleSelectCharacterRequest(uint64_t session_id, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 发送选择角色响应
     * @param session_id 会话ID
     * @param result_code 结果码
     * @param map_server_ip 地图服务器IP
     * @param map_server_port 地图服务器端口
     */
    void SendSelectCharacterResponse(
        uint64_t session_id,
        int32_t result_code,
        const std::string& map_server_ip,
        int32_t map_server_port);

    /**
     * @brief 处理创建角色请求
     * @param session_id 会话ID
     * @param buffer 消息缓冲区
     */
    void HandleCreateCharacterRequest(uint64_t session_id, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 发送登录响应
     * @param session_id 会话ID
     * @param result_code 结果码
     * @param account_id 账号ID
     * @param session_token 会话令牌
     * @param server_name 服务器名称
     */
    void SendLoginResponse(
        uint64_t session_id,
        Game::LoginResult result_code,
        uint64_t account_id,
        const std::string& session_token,
        const std::string& server_name
    );
};

} // namespace AgentServer
} // namespace Murim
