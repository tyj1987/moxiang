#include "MurimNetServerManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>

namespace Murim {
namespace MurimNetServer {

MurimNetServerManager::MurimNetServerManager()
    : guild_manager_(Game::GuildManager::Instance())
    , party_manager_(Game::PartyManager::Instance())
    , chat_manager_(Game::ChatManager::Instance())
    , session_manager_(Core::Network::SessionManager::Instance())
{
}

MurimNetServerManager& MurimNetServerManager::Instance() {
    static MurimNetServerManager instance;
    return instance;
}

bool MurimNetServerManager::Initialize() {
    spdlog::info("[MurimNetServer] Initializing MurimNetServer...");

    if (status_ != ServerStatus::kStopped) {
        spdlog::error("[MurimNetServer] Server already initialized!");
        return false;
    }

    status_ = ServerStatus::kStarting;

    // 初始化SessionManager
    auto& session_manager = Core::Network::SessionManager::Instance();
    if (!session_manager.Initialize()) {
        spdlog::error("[MurimNetServer] Failed to initialize SessionManager!");
        status_ = ServerStatus::kStopped;
        return false;
    }

    // 创建默认频道
    CreateDefaultChannels();

    spdlog::info("[MurimNetServer] MurimNetServer initialized successfully.");
    spdlog::info("[MurimNetServer] Default channels created: {}", channels_.size());

    status_ = ServerStatus::kStarting;
    return true;
}

bool MurimNetServerManager::Start() {
    spdlog::info("[MurimNetServer] Starting MurimNetServer...");

    if (status_ != ServerStatus::kStarting) {
        spdlog::error("[MurimNetServer] Server not in starting state!");
        return false;
    }

    try {
        // 1. 初始化网络层
        spdlog::info("[MurimNetServer] Initializing network layer...");
        io_context_ = std::make_unique<Core::Network::IOContext>();

        // 2. 创建Acceptor
        acceptor_ = std::make_unique<Core::Network::Acceptor>(
            *io_context_,
            "0.0.0.0",  // 监听所有接口
            server_port_
        );

        // 3. 设置连接回调
        acceptor_->Start([this](Core::Network::Connection::Ptr conn) {
            spdlog::info("[MurimNetServer] New connection from: {}",
                        conn->GetRemoteAddress());

            // MurimNetServer 接受客户端连接（公会、组队、聊天功能）
            // TODO: 实现完整的客户端协议处理

            // 简单处理：保持连接
            conn->SetMessageHandler([this](const Core::Network::ByteBuffer& buffer) {
                // TODO: 处理客户端消息（公会操作、组队请求、聊天等）
                spdlog::debug("[MurimNetServer] Received message from client");
            });

            // 开始接收数据
            conn->AsyncReceive();
        });

        spdlog::info("[MurimNetServer] Network acceptor started on port {}", server_port_);

        // 4. 启动IO线程
        io_running_ = true;
        io_thread_ = std::thread([this]() {
            spdlog::info("[MurimNetServer] IO thread started");
            try {
                // Create a dummy timer to keep io_context running
                auto dummy_timer = std::make_shared<boost::asio::steady_timer>(io_context_->GetIOContext());
                dummy_timer->expires_after(std::chrono::hours(999999));
                dummy_timer->async_wait([](const boost::system::error_code&) {});

                // Directly call boost::asio::io_context::run() which blocks
                io_context_->GetIOContext().run();
            } catch (const std::exception& e) {
                spdlog::error("[MurimNetServer] IO thread error: {}", e.what());
            }
            spdlog::info("[MurimNetServer] IO thread stopped");
        });

        // 5. 启动SessionManager
        auto& session_manager = Core::Network::SessionManager::Instance();
        if (!session_manager.Start()) {
            spdlog::error("[MurimNetServer] Failed to start SessionManager!");
            throw std::runtime_error("Failed to start SessionManager");
        }

        // 6. 创建默认频道
        CreateDefaultChannels();

        status_ = ServerStatus::kRunning;
        last_update_ = std::chrono::steady_clock::now();
        last_match_ = std::chrono::steady_clock::now();

        spdlog::info("[MurimNetServer] MurimNetServer started successfully.");
        spdlog::info("[MurimNetServer] Server ID: {}", server_id_);
        spdlog::info("[MurimNetServer] Listening port: {}", server_port_);
        spdlog::info("[MurimNetServer] Tick rate: {}ms", tick_rate_);

        return true;

    } catch (const std::exception& e) {
        spdlog::error("[MurimNetServer] Failed to start: {}", e.what());
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

void MurimNetServerManager::Stop() {
    spdlog::info("[MurimNetServer] Stopping MurimNetServer...");

    if (status_ != ServerStatus::kRunning) {
        spdlog::warn("[MurimNetServer] Server not running!");
        return;
    }

    status_ = ServerStatus::kStopping;

    // 1. 停止 Acceptor
    if (acceptor_) {
        spdlog::info("[MurimNetServer] Stopping network acceptor...");
        acceptor_->Stop();
        acceptor_.reset();
    }

    // 2. 停止 IO 线程
    if (io_running_) {
        spdlog::info("[MurimNetServer] Stopping IO thread...");
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
    auto& session_manager = Core::Network::SessionManager::Instance();
    session_manager.Stop();

    spdlog::info("[MurimNetServer] MurimNetServer stopped.");
}

void MurimNetServerManager::Shutdown() {
    spdlog::info("[MurimNetServer] Shutting down MurimNetServer...");

    Stop();

    // 清理所有频道和房间
    channels_.clear();
    play_rooms_.clear();
    match_queue_.clear();
    online_players_.clear();

    status_ = ServerStatus::kStopped;

    spdlog::info("[MurimNetServer] MurimNetServer shutdown complete.");
}

// ========== 公会管理 ==========

uint64_t MurimNetServerManager::CreateGuild(const std::string& guild_name, uint64_t leader_id) {
    spdlog::info("[MurimNetServer] Creating guild: {} (Leader: {})", guild_name, leader_id);

    // 调用GuildManager创建公会
    auto guild_id = guild_manager_.CreateGuild(guild_name, leader_id);

    if (guild_id > 0) {
        spdlog::info("[MurimNetServer] Guild created successfully: {} (ID: {})", guild_name, guild_id);
        BroadcastServerStatus();
    } else {
        spdlog::error("[MurimNetServer] Failed to create guild: {}", guild_name);
    }

    return guild_id;
}

bool MurimNetServerManager::DisbandGuild(uint64_t guild_id) {
    spdlog::info("[MurimNetServer] Disbanding guild: {}", guild_id);

    // 调用GuildManager解散公会
    bool success = guild_manager_.DisbandGuild(guild_id);

    if (success) {
        spdlog::info("[MurimNetServer] Guild disbanded: {}", guild_id);
        BroadcastServerStatus();
    } else {
        spdlog::error("[MurimNetServer] Failed to disband guild: {}", guild_id);
    }

    return success;
}

bool MurimNetServerManager::JoinGuild(uint64_t guild_id, uint64_t character_id) {
    spdlog::info("[MurimNetServer] Character {} joining guild {}", character_id, guild_id);

    // 调用GuildManager加入公会
    bool success = guild_manager_.AddMember(guild_id, character_id);

    if (success) {
        spdlog::info("[MurimNetServer] Character {} joined guild {}", character_id, guild_id);
    } else {
        spdlog::error("[MurimNetServer] Failed to join guild: {}", guild_id);
    }

    return success;
}

bool MurimNetServerManager::LeaveGuild(uint64_t guild_id, uint64_t character_id) {
    spdlog::info("[MurimNetServer] Character {} leaving guild {}", character_id, guild_id);

    // 调用GuildManager离开公会
    bool success = guild_manager_.RemoveMember(guild_id, character_id);

    if (success) {
        spdlog::info("[MurimNetServer] Character {} left guild {}", character_id, guild_id);
    } else {
        spdlog::error("[MurimNetServer] Failed to leave guild: {}", guild_id);
    }

    return success;
}

bool MurimNetServerManager::KickFromGuild(uint64_t guild_id, uint64_t character_id) {
    spdlog::info("[MurimNetServer] Kicking character {} from guild {}", character_id, guild_id);

    // 调用GuildManager踢出成员
    bool success = guild_manager_.RemoveMember(guild_id, character_id);

    if (success) {
        spdlog::info("[MurimNetServer] Character {} kicked from guild {}", character_id, guild_id);
    } else {
        spdlog::error("[MurimNetServer] Failed to kick from guild: {}", guild_id);
    }

    return success;
}

bool MurimNetServerManager::SetGuildPosition(uint64_t guild_id, uint64_t character_id, uint8_t position) {
    spdlog::info("[MurimNetServer] Setting character {} position in guild {} to {}",
                 character_id, guild_id, position);

    // 调用GuildManager设置职位 - 转换uint8_t为GuildPosition
    bool success = guild_manager_.SetMemberPosition(guild_id, character_id,
        static_cast<Game::GuildPosition>(position));

    if (success) {
        spdlog::info("[MurimNetServer] Position set successfully");
    } else {
        spdlog::error("[MurimNetServer] Failed to set position");
    }

    return success;
}

std::optional<Game::GuildInfo> MurimNetServerManager::GetGuildInfo(uint64_t guild_id) {
    return guild_manager_.GetGuildInfo(guild_id);
}

std::vector<Game::GuildMemberInfo> MurimNetServerManager::GetGuildMembers(uint64_t guild_id) {
    return guild_manager_.GetGuildMembers(guild_id);
}

// ========== 频道管理 ==========

uint32_t MurimNetServerManager::CreateChannel(
    const std::string& channel_name,
    uint8_t channel_type,
    uint32_t max_players
) {
    spdlog::info("[MurimNetServer] Creating channel: {} (Type: {}, Max: {})",
                 channel_name, channel_type, max_players);

    uint32_t channel_id = next_channel_id_++;

    ChannelInfo info;
    info.channel_id = channel_id;
    info.channel_name = channel_name;
    info.channel_type = channel_type;
    info.max_players = max_players;
    info.current_players = 0;
    info.available = true;

    channels_[channel_id] = info;

    spdlog::info("[MurimNetServer] Channel created: {} (ID: {})", channel_name, channel_id);
    BroadcastServerStatus();

    return channel_id;
}

bool MurimNetServerManager::DeleteChannel(uint32_t channel_id) {
    spdlog::info("[MurimNetServer] Deleting channel: {}", channel_id);

    auto it = channels_.find(channel_id);
    if (it == channels_.end()) {
        spdlog::error("[MurimNetServer] Channel not found: {}", channel_id);
        return false;
    }

    channels_.erase(it);
    spdlog::info("[MurimNetServer] Channel deleted: {}", channel_id);
    BroadcastServerStatus();

    return true;
}

bool MurimNetServerManager::JoinChannel(uint32_t channel_id, uint64_t character_id) {
    auto it = channels_.find(channel_id);
    if (it == channels_.end()) {
        spdlog::error("[MurimNetServer] Channel not found: {}", channel_id);
        return false;
    }

    auto& channel = it->second;
    if (channel.current_players >= channel.max_players) {
        spdlog::warn("[MurimNetServer] Channel full: {}", channel_id);
        return false;
    }

    channel.current_players++;
    spdlog::info("[MurimNetServer] Character {} joined channel {}", character_id, channel_id);

    return true;
}

bool MurimNetServerManager::LeaveChannel(uint32_t channel_id, uint64_t character_id) {
    auto it = channels_.find(channel_id);
    if (it == channels_.end()) {
        spdlog::error("[MurimNetServer] Channel not found: {}", channel_id);
        return false;
    }

    auto& channel = it->second;
    if (channel.current_players > 0) {
        channel.current_players--;
    }

    spdlog::info("[MurimNetServer] Character {} left channel {}", character_id, channel_id);

    return true;
}

std::vector<MurimNetServerManager::ChannelInfo> MurimNetServerManager::GetAllChannels() {
    std::vector<ChannelInfo> channels;
    for (const auto& pair : channels_) {
        channels.push_back(pair.second);
    }
    return channels;
}

std::vector<MurimNetServerManager::ChannelInfo> MurimNetServerManager::GetAvailableChannels(uint8_t channel_type) {
    std::vector<ChannelInfo> channels;
    for (const auto& pair : channels_) {
        const auto& channel = pair.second;
        if (channel.available && channel.current_players < channel.max_players) {
            if (channel_type == 0 || channel.channel_type == channel_type) {
                channels.push_back(channel);
            }
        }
    }
    return channels;
}

// ========== 游戏房间管理 ==========

uint32_t MurimNetServerManager::CreatePlayRoom(
    const std::string& room_name,
    uint32_t channel_id,
    uint8_t room_type,
    uint8_t max_teams,
    uint8_t max_players_per_team,
    uint64_t host_id
) {
    spdlog::info("[MurimNetServer] Creating play room: {} in channel {}", room_name, channel_id);

    uint32_t room_id = next_room_id_++;

    PlayRoomInfo info;
    info.room_id = room_id;
    info.room_name = room_name;
    info.channel_id = channel_id;
    info.room_type = room_type;
    info.max_teams = max_teams;
    info.max_players_per_team = max_players_per_team;
    info.host_id = host_id;
    info.available = true;

    play_rooms_[room_id] = info;

    spdlog::info("[MurimNetServer] Play room created: {} (ID: {})", room_name, room_id);
    BroadcastServerStatus();

    return room_id;
}

bool MurimNetServerManager::DeletePlayRoom(uint32_t room_id) {
    spdlog::info("[MurimNetServer] Deleting play room: {}", room_id);

    auto it = play_rooms_.find(room_id);
    if (it == play_rooms_.end()) {
        spdlog::error("[MurimNetServer] Play room not found: {}", room_id);
        return false;
    }

    play_rooms_.erase(it);
    spdlog::info("[MurimNetServer] Play room deleted: {}", room_id);
    BroadcastServerStatus();

    return true;
}

bool MurimNetServerManager::JoinPlayRoom(uint32_t room_id, uint64_t character_id, uint8_t team_id) {
    auto it = play_rooms_.find(room_id);
    if (it == play_rooms_.end()) {
        spdlog::error("[MurimNetServer] Play room not found: {}", room_id);
        return false;
    }

    spdlog::info("[MurimNetServer] Character {} joined play room {} (Team: {})",
                 character_id, room_id, team_id);

    return true;
}

bool MurimNetServerManager::LeavePlayRoom(uint32_t room_id, uint64_t character_id) {
    auto it = play_rooms_.find(room_id);
    if (it == play_rooms_.end()) {
        spdlog::error("[MurimNetServer] Play room not found: {}", room_id);
        return false;
    }

    spdlog::info("[MurimNetServer] Character {} left play room {}", character_id, room_id);

    return true;
}

std::vector<MurimNetServerManager::PlayRoomInfo> MurimNetServerManager::GetPlayRooms(uint32_t channel_id) {
    std::vector<PlayRoomInfo> rooms;
    for (const auto& pair : play_rooms_) {
        if (pair.second.channel_id == channel_id) {
            rooms.push_back(pair.second);
        }
    }
    return rooms;
}

// ========== 对战匹配 ==========

bool MurimNetServerManager::RequestMatch(const MatchRequest& request) {
    spdlog::info("[MurimNetServer] Character {} requesting match (Type: {})",
                 request.character_id, request.match_type);

    // 检查是否已在队列中
    auto it = std::find_if(match_queue_.begin(), match_queue_.end(),
        [&request](const MatchRequest& req) {
            return req.character_id == request.character_id;
        });

    if (it != match_queue_.end()) {
        spdlog::warn("[MurimNetServer] Character already in match queue: {}", request.character_id);
        return false;
    }

    match_queue_.push_back(request);
    spdlog::info("[MurimNetServer] Character {} added to match queue (Size: {})",
                 request.character_id, match_queue_.size());

    return true;
}

bool MurimNetServerManager::CancelMatch(uint64_t character_id) {
    spdlog::info("[MurimNetServer] Character {} canceling match", character_id);

    auto it = std::find_if(match_queue_.begin(), match_queue_.end(),
        [character_id](const MatchRequest& req) {
            return req.character_id == character_id;
        });

    if (it != match_queue_.end()) {
        match_queue_.erase(it);
        spdlog::info("[MurimNetServer] Character {} removed from match queue", character_id);
        return true;
    }

    spdlog::warn("[MurimNetServer] Character not in match queue: {}", character_id);
    return false;
}

uint32_t MurimNetServerManager::ProcessMatch(uint8_t match_type) {
    // 筛选匹配类型的请求
    std::vector<MatchRequest> type_queue;
    for (const auto& req : match_queue_) {
        if (req.match_type == match_type) {
            type_queue.push_back(req);
        }
    }

    if (type_queue.size() < 2) {
        return 0;  // 玩家不足,无法匹配
    }

    // 简单匹配: 选择评分最接近的两个玩家
    std::sort(type_queue.begin(), type_queue.end(),
        [](const MatchRequest& a, const MatchRequest& b) {
            return a.min_rating < b.min_rating;
        });

    // 创建游戏房间
    uint32_t room_id = CreatePlayRoom(
        "Matched Room",
        type_queue[0].channel_id,
        match_type,
        2,  // 2队
        1,  // 每队1人
        type_queue[0].character_id
    );

    // 从队列中移除已匹配的玩家
    match_queue_.erase(
        std::remove_if(match_queue_.begin(), match_queue_.end(),
            [&type_queue](const MatchRequest& req) {
                return req.character_id == type_queue[0].character_id ||
                       req.character_id == type_queue[1].character_id;
            }),
        match_queue_.end()
    );

    spdlog::info("[MurimNetServer] Match created: Room {} for players {} and {}",
                 room_id, type_queue[0].character_id, type_queue[1].character_id);

    return room_id;
}

std::optional<uint64_t> MurimNetServerManager::FindMatch(const MatchRequest& request) {
    // 查找评分范围内的玩家
    for (const auto& req : match_queue_) {
        if (req.character_id != request.character_id &&
            req.match_type == request.match_type) {
            // 检查评分范围
            if (req.min_rating >= request.min_rating &&
                req.min_rating <= request.max_rating) {
                return req.character_id;
            }
        }
    }
    return std::nullopt;
}

// ========== 状态查询 ==========

size_t MurimNetServerManager::GetGuildCount() const {
    // 通过GuildManager获取公会数量
    // 这里简化处理,实际应该调用GuildManager的方法
    return 0;
}

// ========== 玩家管理 ==========

bool MurimNetServerManager::OnPlayerLogin(uint64_t character_id, uint64_t session_id) {
    spdlog::info("[MurimNetServer] Player login: {} (Session: {})", character_id, session_id);

    online_players_[character_id] = session_id;
    BroadcastServerStatus();

    return true;
}

void MurimNetServerManager::OnPlayerLogout(uint64_t character_id) {
    spdlog::info("[MurimNetServer] Player logout: {}", character_id);

    online_players_.erase(character_id);

    // 从匹配队列中移除
    CancelMatch(character_id);

    BroadcastServerStatus();
}

// ========== 服务器循环 ==========

void MurimNetServerManager::MainLoop() {
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

void MurimNetServerManager::Update(uint32_t delta_time) {
    // 处理网络消息
    auto& session_manager = Core::Network::SessionManager::Instance();
    session_manager.Update(delta_time);

    // 处理匹配
    auto now = std::chrono::steady_clock::now();
    auto match_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_match_
    ).count();

    if (match_elapsed >= match_interval_) {
        last_match_ = now;
        ProcessMatching();
    }

    // 清理空房间
    CleanupEmptyRooms();

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

void MurimNetServerManager::ProcessTick() {
    // 对应 legacy: MurimNetServer主循环的每帧处理

    // 定期日志输出
    static uint32_t log_counter = 0;
    log_counter++;

    if (log_counter >= 10) {  // 每1秒 (10 ticks * 100ms)
        log_counter = 0;
        spdlog::debug("[MurimNetServer] Guilds: {}, Channels: {}, Rooms: {}, Online: {}",
                      GetGuildCount(), channels_.size(), play_rooms_.size(),
                      online_players_.size());
    }
}

void MurimNetServerManager::ProcessMatching() {
    // 对应 legacy: MurimNetServer匹配处理

    spdlog::debug("[MurimNetServer] Processing matching... Queue size: {}", match_queue_.size());

    // 处理1v1匹配
    ProcessMatch(1);

    // 处理2v2匹配
    ProcessMatch(2);

    // 处理公会战匹配
    ProcessMatch(3);
}

void MurimNetServerManager::CreateDefaultChannels() {
    spdlog::info("[MurimNetServer] Creating default channels...");

    // 创建默认公共频道
    CreateChannel("Default Channel", 1, 100);

    // 创建游戏频道
    for (int i = 1; i <= 10; ++i) {
        char channel_name[64];
        snprintf(channel_name, sizeof(channel_name), "Game Channel %d", i);
        CreateChannel(channel_name, 1, 50);
    }

    spdlog::info("[MurimNetServer] Default channels created: {}", channels_.size());
}

void MurimNetServerManager::BroadcastServerStatus() {
    // 对应 legacy: MurimNetServer广播服务器状态

    spdlog::info("[MurimNetServer] Server Status Broadcast:");
    spdlog::info("  Online Players: {}", online_players_.size());
    spdlog::info("  Guilds: {}", GetGuildCount());
    spdlog::info("  Channels: {}", channels_.size());
    spdlog::info("  Play Rooms: {}", play_rooms_.size());
    spdlog::info("  Match Queue: {}", match_queue_.size());
}

void MurimNetServerManager::CleanupEmptyRooms() {
    // 清理没有玩家的房间
    // 这里简化处理,实际应该追踪房间内的玩家数量
}

} // namespace MurimNetServer
} // namespace Murim
