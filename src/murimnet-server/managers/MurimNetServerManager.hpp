#pragma once

#include <memory>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include "shared/guild/GuildManager.hpp"
#include "shared/social/Party.hpp"
#include "shared/social/Chat.hpp"
#include "core/network/SessionManager.hpp"
#include "core/network/Acceptor.hpp"
#include "core/network/IOContext.hpp"

namespace Murim {
namespace MurimNetServer {

/**
 * @brief MurimNetServer管理器
 *
 * 负责MurimNetServer的核心逻辑
 * - 公会(Munpa)管理
 * - 频道(Channel)管理
 * - 游戏房间(PlayRoom)管理
 * - MurimNet对战匹配
 *
 * 对应 legacy: CMurimNetSystem / MurimNetServer主逻辑
 */
class MurimNetServerManager {
public:
    /**
     * @brief 获取单例实例
     */
    static MurimNetServerManager& Instance();

    // ========== 初始化 ==========

    /**
     * @brief 初始化MurimNetServer
     * @return 是否初始化成功
     */
    bool Initialize();

    /**
     * @brief 启动MurimNetServer
     * @return 是否启动成功
     */
    bool Start();

    /**
     * @brief 停止MurimNetServer
     */
    void Stop();

    /**
     * @brief 关闭MurimNetServer
     */
    void Shutdown();

    // ========== 公会管理 ==========

    /**
     * @brief 创建公会
     * @param guild_name 公会名称
     * @param leader_id 会长角色ID
     * @return 公会ID (失败返回0)
     */
    uint64_t CreateGuild(const std::string& guild_name, uint64_t leader_id);

    /**
     * @brief 解散公会
     * @param guild_id 公会ID
     * @return 是否成功
     */
    bool DisbandGuild(uint64_t guild_id);

    /**
     * @brief 加入公会
     * @param guild_id 公会ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool JoinGuild(uint64_t guild_id, uint64_t character_id);

    /**
     * @brief 离开公会
     * @param guild_id 公会ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool LeaveGuild(uint64_t guild_id, uint64_t character_id);

    /**
     * @brief 踢出公会成员
     * @param guild_id 公会ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool KickFromGuild(uint64_t guild_id, uint64_t character_id);

    /**
     * @brief 设置公会职位
     * @param guild_id 公会ID
     * @param character_id 角色ID
     * @param position 职位 (1=会长, 2=副会长, 3=成员)
     * @return 是否成功
     */
    bool SetGuildPosition(uint64_t guild_id, uint64_t character_id, uint8_t position);

    /**
     * @brief 获取公会信息
     * @param guild_id 公会ID
     * @return 公会信息
     */
    std::optional<Game::GuildInfo> GetGuildInfo(uint64_t guild_id);

    /**
     * @brief 获取公会成员列表
     * @param guild_id 公会ID
     * @return 成员列表
     */
    std::vector<Game::GuildMemberInfo> GetGuildMembers(uint64_t guild_id);

    // ========== 频道管理 ==========

    /**
     * @brief 频道信息
     */
    struct ChannelInfo {
        uint32_t channel_id;
        std::string channel_name;
        uint8_t channel_type;       // 1=公共, 2=私人
        uint32_t max_players;
        uint32_t current_players;
        bool available;
    };

    /**
     * @brief 创建频道
     * @param channel_name 频道名称
     * @param channel_type 频道类型 (1=公共, 2=私人)
     * @param max_players 最大玩家数
     * @return 频道ID
     */
    uint32_t CreateChannel(
        const std::string& channel_name,
        uint8_t channel_type,
        uint32_t max_players
    );

    /**
     * @brief 删除频道
     * @param channel_id 频道ID
     * @return 是否成功
     */
    bool DeleteChannel(uint32_t channel_id);

    /**
     * @brief 加入频道
     * @param channel_id 频道ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool JoinChannel(uint32_t channel_id, uint64_t character_id);

    /**
     * @brief 离开频道
     * @param channel_id 频道ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool LeaveChannel(uint32_t channel_id, uint64_t character_id);

    /**
     * @brief 获取所有频道
     * @return 频道列表
     */
    std::vector<ChannelInfo> GetAllChannels();

    /**
     * @brief 获取可用频道
     * @param channel_type 频道类型 (0=全部)
     * @return 频道列表
     */
    std::vector<ChannelInfo> GetAvailableChannels(uint8_t channel_type = 0);

    // ========== 游戏房间管理 ==========

    /**
     * @brief 游戏房间信息
     */
    struct PlayRoomInfo {
        uint32_t room_id;
        std::string room_name;
        uint32_t channel_id;
        uint8_t room_type;          // 1=PvP, 2=公会战, 3=MurimField
        uint8_t max_teams;
        uint8_t max_players_per_team;
        uint32_t host_id;
        bool available;
    };

    /**
     * @brief 创建游戏房间
     * @param room_name 房间名称
     * @param channel_id 频道ID
     * @param room_type 房间类型
     * @param max_teams 最大队伍数
     * @param max_players_per_team 每队最大玩家数
     * @param host_id 房主ID
     * @return 房间ID
     */
    uint32_t CreatePlayRoom(
        const std::string& room_name,
        uint32_t channel_id,
        uint8_t room_type,
        uint8_t max_teams,
        uint8_t max_players_per_team,
        uint64_t host_id
    );

    /**
     * @brief 删除游戏房间
     * @param room_id 房间ID
     * @return 是否成功
     */
    bool DeletePlayRoom(uint32_t room_id);

    /**
     * @brief 加入游戏房间
     * @param room_id 房间ID
     * @param character_id 角色ID
     * @param team_id 队伍ID
     * @return 是否成功
     */
    bool JoinPlayRoom(uint32_t room_id, uint64_t character_id, uint8_t team_id);

    /**
     * @brief 离开游戏房间
     * @param room_id 房间ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool LeavePlayRoom(uint32_t room_id, uint64_t character_id);

    /**
     * @brief 获取频道内的游戏房间
     * @param channel_id 频道ID
     * @return 房间列表
     */
    std::vector<PlayRoomInfo> GetPlayRooms(uint32_t channel_id);

    // ========== 对战匹配 ==========

    /**
     * @brief 匹配请求
     */
    struct MatchRequest {
        uint64_t character_id;
        uint16_t character_level;
        uint8_t match_type;         // 1=1v1, 2=2v2, 3=公会战
        uint32_t min_rating;
        uint32_t max_rating;
        uint32_t channel_id;
    };

    /**
     * @brief 请求匹配
     * @param request 匹配请求
     * @return 是否成功加入匹配队列
     */
    bool RequestMatch(const MatchRequest& request);

    /**
     * @brief 取消匹配
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool CancelMatch(uint64_t character_id);

    /**
     * @brief 处理匹配
     * @param match_type 匹配类型
     * @return 匹配成功的房间ID (0表示无匹配)
     */
    uint32_t ProcessMatch(uint8_t match_type);

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
     * @brief 检查服务器是否正在运行
     */
    bool IsRunning() const { return status_ == ServerStatus::kRunning; }

    /**
     * @brief 获取服务器ID
     */
    uint16_t GetServerId() const { return server_id_; }

    /**
     * @brief 获取在线玩家数
     */
    size_t GetOnlinePlayerCount() const { return online_players_.size(); }

    /**
     * @brief 获取公会数量
     */
    size_t GetGuildCount() const;

    /**
     * @brief 获取频道数量
     */
    size_t GetChannelCount() const { return channels_.size(); }

    /**
     * @brief 获取房间数量
     */
    size_t GetPlayRoomCount() const { return play_rooms_.size(); }

    // ========== Manager访问 ==========

    /**
     * @brief 获取公会管理器
     */
    Game::GuildManager& GetGuildManager() { return guild_manager_; }

    /**
     * @brief 获取队伍管理器
     */
    Game::PartyManager& GetPartyManager() { return party_manager_; }

    /**
     * @brief 获取聊天管理器
     */
    Game::ChatManager& GetChatManager() { return chat_manager_; }

    // ========== 玩家管理 ==========

    /**
     * @brief 玩家登录
     * @param character_id 角色ID
     * @param session_id 会话ID
     * @return 是否成功
     */
    bool OnPlayerLogin(uint64_t character_id, uint64_t session_id);

    /**
     * @brief 玩家登出
     * @param character_id 角色ID
     */
    void OnPlayerLogout(uint64_t character_id);

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
    MurimNetServerManager();
    ~MurimNetServerManager() = default;
    MurimNetServerManager(const MurimNetServerManager&) = delete;
    MurimNetServerManager& operator=(const MurimNetServerManager&) = delete;

    // ========== 服务器状态 ==========

    ServerStatus status_ = ServerStatus::kStopped;
    uint16_t server_id_ = 8500;  // MurimNetServer默认ID
    uint16_t server_port_ = 8500;  // MurimNetServer监听端口

    // ========== Manager实例 ==========

    Game::GuildManager& guild_manager_;
    Game::PartyManager& party_manager_;
    Game::ChatManager& chat_manager_;
    Core::Network::SessionManager& session_manager_;

    // ========== 网络层 ==========

    std::unique_ptr<Core::Network::IOContext> io_context_;
    std::unique_ptr<Core::Network::Acceptor> acceptor_;
    std::thread io_thread_;
    std::atomic<bool> io_running_{false};

    // ========== 玩家管理 ==========

    std::unordered_map<uint64_t, uint64_t> online_players_;  // character_id -> session_id

    // ========== 频道管理 ==========

    std::unordered_map<uint32_t, ChannelInfo> channels_;
    uint32_t next_channel_id_ = 1;

    // ========== 游戏房间管理 ==========

    std::unordered_map<uint32_t, PlayRoomInfo> play_rooms_;
    uint32_t next_room_id_ = 1;

    // ========== 匹配队列 ==========

    std::vector<MatchRequest> match_queue_;

    // ========== 服务器配置 ==========

    uint32_t tick_rate_ = 100;  // 服务器tick率(ms)
    uint32_t match_interval_ = 5000;  // 匹配间隔(ms)
    std::chrono::steady_clock::time_point last_update_;
    std::chrono::steady_clock::time_point last_match_;

    // ========== 辅助方法 ==========

    /**
     * @brief 处理服务器tick
     */
    void ProcessTick();

    /**
     * @brief 处理匹配
     */
    void ProcessMatching();

    /**
     * @brief 查找匹配的玩家
     * @param request 匹配请求
     * @return 匹配的玩家ID (0表示无匹配)
     */
    std::optional<uint64_t> FindMatch(const MatchRequest& request);

    /**
     * @brief 创建默认频道
     */
    void CreateDefaultChannels();

    /**
     * @brief 广播服务器状态
     */
    void BroadcastServerStatus();

    /**
     * @brief 清理空房间
     */
    void CleanupEmptyRooms();
};

} // namespace MurimNetServer
} // namespace Murim
