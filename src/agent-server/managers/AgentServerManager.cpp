#include "AgentServerManager.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include "../../core/database/AccountDAO.hpp"
#include "../../core/database/CharacterDAO.hpp"
#include "../../core/security/RateLimiter.hpp"
#include "../../core/session/SessionManager.hpp"
#include <chrono>
#include <thread>
#include <random>
#include <cstdio>  // for printf

using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace AgentServer {

AgentServerManager& AgentServerManager::Instance() {
    static AgentServerManager instance;
    return instance;
}

bool AgentServerManager::Initialize() {
    spdlog::info("Initializing AgentServer...");

    try {
        // ========== 初始化数据库连接池 ==========
        spdlog::info("Initializing database connection pool...");
        // AgentServer 连接到 mh_game（可以直接访问 characters 表）
        // accounts 表通过 FDW 从 mh_member 映射
        std::string conn_str = "host=localhost port=5432 dbname=mh_game user=postgres";
        ConnectionPool::Instance().Initialize(conn_str, 10);  // 10个连接
        spdlog::info("Database connection pool initialized successfully (connected to mh_game)");

        // 获取Manager单例实例
        login_manager_ = &Game::LoginManager::Instance();
        character_manager_ = &Game::CharacterManager::Instance();

        // 初始化速率限制器
        Core::Security::RateLimiter::Instance().Initialize();

        // 初始化 SessionManager
        // 清理间隔: 60秒, 超时时间: 1800秒 (30分钟)
        Core::Session::SessionManager::Instance().Initialize(60, 1800);

        spdlog::info("AgentServer initialized successfully");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize AgentServer: {}", e.what());
        return false;
    }
}

bool AgentServerManager::Start() {
    if (status_ != ServerStatus::kStopped) {
        spdlog::warn("AgentServer is not in stopped state, current status: {}",
                     static_cast<int>(status_));
        return false;
    }

    status_ = ServerStatus::kStarting;
    spdlog::info("Starting AgentServer on port {}...", server_port_);

    try {
        // 1. 初始化网络层
        spdlog::info("Initializing network layer...");
        io_context_ = std::make_unique<Core::Network::IOContext>();

        // 2. 创建Acceptor
        acceptor_ = std::make_unique<Core::Network::Acceptor>(
            *io_context_,
            "0.0.0.0",  // 监听所有接口
            server_port_
        );

        // 3. 设置连接回调
        acceptor_->Start([this](Core::Network::Connection::Ptr conn) {
            spdlog::info("New connection accepted from: {}",
                        conn->GetRemoteAddress());

            uint64_t session_id = conn->GetConnectionId();

            // 保存网络连接
            network_connections_[session_id] = conn;
            AddConnection(session_id);

            // 设置消息处理器
            conn->SetMessageHandler([this, session_id](const Core::Network::ByteBuffer& buffer) {
                HandleMessage(session_id, buffer);
            });

            // 开始接收数据
            conn->AsyncReceive();
        });

        spdlog::info("Network acceptor started on port {}", server_port_);

        // 4. 启动IO线程
        io_running_ = true;
        io_thread_ = std::thread([this]() {
            spdlog::info("IO thread started");
            try {
                // Create a dummy timer to keep io_context running
                auto dummy_timer = std::make_shared<boost::asio::steady_timer>(io_context_->GetIOContext());
                dummy_timer->expires_after(std::chrono::hours(999999));
                dummy_timer->async_wait([](const boost::system::error_code&) {});

                // Directly call boost::asio::io_context::run() which blocks
                io_context_->GetIOContext().run();
            } catch (const std::exception& e) {
                spdlog::error("IO thread error: {}", e.what());
            }
            spdlog::info("IO thread stopped");
        });

        // 5. 加载服务器列表
        LoadServerList();

        // 6. 启动 SessionManager 清理线程
        Core::Session::SessionManager::Instance().Start();

        // 7. 标记为运行中
        status_ = ServerStatus::kRunning;

        spdlog::info("AgentServer started successfully on port {}", server_port_);
        spdlog::info("Server ID: {}, Map Servers: {}", server_id_, map_servers_.size());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to start AgentServer: {}", e.what());
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
            io_thread_.join();
        }

        return false;
    }
}

void AgentServerManager::Stop() {
    if (status_ != ServerStatus::kRunning) {
        return;
    }

    spdlog::info("Stopping AgentServer...");
    status_ = ServerStatus::kStopping;

    // 1. 停止 SessionManager 清理线程
    Core::Session::SessionManager::Instance().Stop();

    // 2. 停止接受新连接
    if (acceptor_) {
        acceptor_->Stop();
        acceptor_.reset();
    }

    // 3. 停止IO线程
    if (io_context_) {
        io_context_->Stop();
    }

    io_running_ = false;

    // 4. 等待IO线程结束
    if (io_thread_.joinable()) {
        io_thread_.join();
    }

    // 5. 保存所有连接状态
    for (auto& [session_id, conn_info] : connections_) {
        if (conn_info.character_id > 0) {
            PlayerLogout(conn_info.account_id, conn_info.character_id);
        }
    }

    status_ = ServerStatus::kStopped;
    spdlog::info("AgentServer stopped successfully");
}

void AgentServerManager::Shutdown() {
    spdlog::info("Shutting down AgentServer...");

    if (status_ != ServerStatus::kStopped) {
        Stop();
    }

    status_ = ServerStatus::kStopped;
    spdlog::info("AgentServer shutdown complete");
}

// ========== 认证相关 ==========

AgentServerManager::AuthResult AgentServerManager::Authenticate(
    const std::string& username,
    const std::string& password
) {
    spdlog::info("Authentication attempt: username={}", username);

    try {
        // 对应 legacy: 账号认证逻辑
        auto login_response = login_manager_->Login(username, password, "127.0.0.1", server_id_);

        AuthResult result;
        if (login_response.result == Game::LoginResult::kSuccess) {
            result.success = true;
            result.account_id = 0; // Login response doesn't include account_id directly
            result.session_token = login_response.session_token;
        } else {
            result.success = false;
            result.account_id = 0;
            result.session_token = "";
        }

        return result;

    } catch (const std::exception& e) {
        spdlog::error("Authentication failed: {}", e.what());
        return {false, 0, ""};
    }
}

std::vector<AgentServerManager::CharacterInfo> AgentServerManager::GetCharacterList(
    uint64_t account_id
) {
    spdlog::debug("Getting character list for account: {}", account_id);

    try {
        // 对应 legacy: 获取角色列表逻辑
        auto characters = character_manager_->GetAccountCharacters(account_id);

        std::vector<CharacterInfo> result;
        result.reserve(characters.size());

        for (const auto& character : characters) {
            CharacterInfo info;
            info.character_id = character.character_id;
            info.name = character.name;
            info.level = character.level;
            info.initial_weapon = character.initial_weapon;
            info.gender = character.gender;
            result.push_back(info);
        }

        spdlog::info("Account {} has {} characters", account_id, result.size());
        return result;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get character list: {}", e.what());
        return {};
    }
}

bool AgentServerManager::SelectCharacter(
    uint64_t account_id,
    uint64_t character_id,
    uint16_t map_server_id
) {
    spdlog::info("SelectCharacter: account_id={}, character_id={}, map_server_id={}",
                account_id, character_id, map_server_id);

    try {
        // 简化实现 - 暂时跳过数据库验证
        spdlog::info("[DEBUG] Skipping database validation for now");

        // 1. 验证角色所有权（暂时跳过）
        // TODO: 重新启用数据库验证
        /*
        if (!character_manager_->ValidateOwnership(character_id, account_id)) {
            spdlog::warn("Character {} does not belong to account {}", character_id, account_id);
            return false;
        }

        // 2. 更新角色在线状态
        character_manager_->CharacterLogin(character_id);
        */

        // 3. 保存会话信息
        spdlog::info("[DEBUG] Updating session info for account {}", account_id);
        for (auto& [sid, conn_info] : connections_) {
            if (conn_info.account_id == account_id) {
                conn_info.character_id = character_id;
                spdlog::info("[DEBUG] Updated session {}: character_id={}", sid, character_id);
                break;
            }
        }

        spdlog::info("Character {} selected successfully", character_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to select character: {}", e.what());
        return false;
    }
}

void AgentServerManager::PlayerLogout(uint64_t account_id, uint64_t character_id) {
    spdlog::info("Player logout: account_id={}, character_id={}", account_id, character_id);

    try {
        // 更新角色离线状态
        character_manager_->CharacterLogout(character_id, 0);

        // 移除所有会话
        for (auto& [session_id, conn_info] : connections_) {
            if (conn_info.account_id == account_id &&
                conn_info.character_id == character_id) {
                RemoveConnection(session_id);
                break;
            }
        }

        spdlog::info("Player logout successful: account_id={}, character_id={}",
                    account_id, character_id);

    } catch (const std::exception& e) {
        spdlog::error("Failed to logout player: {}", e.what());
    }
}

// ========== 连接管理 ==========

void AgentServerManager::AddConnection(uint64_t session_id, uint64_t account_id) {
    ConnectionInfo conn_info;
    conn_info.session_id = session_id;
    conn_info.account_id = account_id;
    conn_info.character_id = 0;
    conn_info.connect_time = std::chrono::system_clock::now();

    connections_[session_id] = conn_info;

    spdlog::debug("Connection added: session_id={}, account_id={}",
                 session_id, account_id);
}

void AgentServerManager::RemoveConnection(uint64_t session_id) {
    connections_.erase(session_id);
    spdlog::debug("Connection removed: session_id={}", session_id);
}

uint64_t AgentServerManager::GetAccountId(uint64_t session_id) const {
    auto it = connections_.find(session_id);
    if (it != connections_.end()) {
        return it->second.account_id;
    }
    return 0;
}

bool AgentServerManager::IsSessionAuthenticated(uint64_t session_id) const {
    auto it = connections_.find(session_id);
    if (it != connections_.end()) {
        return it->second.account_id != 0;
    }
    return false;
}

// ========== 负载均衡 ==========

uint16_t AgentServerManager::GetBestMapServer(uint16_t map_id) {
    // 对应 legacy: 负载均衡逻辑
    // 简化版本: 返回第一个可用的MapServer

    for (const auto& [server_id, server_info] : map_servers_) {
        if (server_info.map_id == map_id && server_info.available &&
            server_info.player_count < server_info.max_players) {
            return server_id;
        }
    }

    // 如果没有可用的,创建新的MapServer实例
    uint16_t new_server_id = 9000 + map_id;
    MapServerInfo new_server;
    new_server.server_id = new_server_id;
    new_server.map_id = map_id;
    new_server.server_name = "MapServer-" + std::to_string(map_id);
    new_server.player_count = 0;
    new_server.max_players = 1000;
    new_server.available = true;

    map_servers_[new_server_id] = new_server;

    spdlog::info("Created new MapServer: server_id={}, map_id={}",
                new_server_id, map_id);

    return new_server_id;
}

bool AgentServerManager::IsMapServerAvailable(uint16_t server_id) {
    auto it = map_servers_.find(server_id);
    if (it != map_servers_.end()) {
        return it->second.available;
    }
    return false;
}

// ========== 服务器循环 ==========

void AgentServerManager::MainLoop() {
    spdlog::info("AgentServer main loop started");

    const std::chrono::milliseconds tick_interval(100);

    while (status_ == ServerStatus::kRunning) {
        auto start = std::chrono::steady_clock::now();

        // 处理服务器tick
        ProcessTick();

        // 计算处理时间
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // 如果处理时间小于tick间隔,则sleep剩余时间
        if (elapsed < tick_interval) {
            std::this_thread::sleep_for(tick_interval - elapsed);
        }
    }

    spdlog::info("AgentServer main loop stopped");
}

void AgentServerManager::ProcessTick() {
    // 处理服务器tick逻辑
    // 更新服务器状态等
}

void AgentServerManager::Update(uint32_t delta_time) {
    // 对应 legacy AgentServer更新逻辑

    // 1. 检查MapServer健康状态
    CheckMapServerHealth();

    // 2. 更新MapServer状态
    UpdateMapServerStatus();

    // 3. 广播服务器状态
    BroadcastServerStatus();
}

// ========== 辅助方法 ==========

void AgentServerManager::CheckMapServerHealth() {
    // 对应 legacy: MapServer健康检查
    // 简化版本: 标记所有服务器为可用
    for (auto& [server_id, server_info] : map_servers_) {
        if (!server_info.available) {
            server_info.available = true;
            spdlog::info("MapServer {} ({}) is now available",
                        server_id, server_info.server_name);
        }
    }
}

void AgentServerManager::UpdateMapServerStatus() {
    // 对应 legacy: ServerTable定期更新MapServer状态
    // legacy中通过定期ping/心跳检查MapServer可用性

    auto current_time = std::chrono::system_clock::now();

    // 遍历所有MapServer,更新状态
    for (auto& [server_id, server_info] : map_servers_) {
        // 检查最后一次更新时间
        auto time_since_last_update = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - server_info.last_update
        ).count();

        // 如果超过30秒没有更新,标记为不可用
        // 对应 legacy: 心跳超时检测
        if (time_since_last_update > 30) {
            if (server_info.available) {
                server_info.available = false;
                spdlog::warn("MapServer {} ({}) is now unavailable (timeout)",
                            server_id, server_info.server_name);
            }
        }

        // 更新玩家数量统计
        // 对应 legacy: MapServer玩家数量同步
        // 实际实现将通过MapServer心跳消息获取
    }

    spdlog::trace("MapServer status updated: {} servers", map_servers_.size());
}

void AgentServerManager::BroadcastServerStatus() {
    // 对应 legacy: 广播服务器状态
    spdlog::trace("Broadcast server status to {} connections", connections_.size());
}

void AgentServerManager::RedirectPlayer(uint64_t account_id, uint16_t target_server_id) {
    // 对应 legacy: 重定向玩家到指定MapServer
    spdlog::info("Redirecting account {} to MapServer {}", account_id, target_server_id);

    // 1. 查找目标服务器信息
    auto it = map_servers_.find(target_server_id);
    if (it == map_servers_.end()) {
        spdlog::error("Target MapServer {} not found", target_server_id);
        return;
    }

    const auto& server_info = it->second;

    // 2. 查找账号的会话
    uint64_t session_id = 0;
    for (const auto& [sid, conn_info] : connections_) {
        if (conn_info.account_id == account_id) {
            session_id = sid;
            break;
        }
    }

    if (session_id == 0) {
        spdlog::error("No active session found for account {}", account_id);
        return;
    }

    // 3. 发送重定向消息给客户端
    // 对应 legacy: 发送MP_USERCONN_REDIRECT消息
    // 消息格式:
    // - Category: MP_USERCONN
    // - Protocol: MP_USERCONN_REDIRECT
    // - Data: target_server_ip, target_server_port, auth_token

    spdlog::debug("Sending redirect message to session {}: {}:{}",
                 session_id, server_info.server_ip, server_info.server_port);

    // 4. 生成认证token
    // 对应 legacy: 生成MapServer登录token
    std::string auth_token = GenerateSessionToken(account_id);

    // 5. 更新连接状态
    auto& conn_info = connections_[session_id];
    conn_info.redirect_server_id = target_server_id;
    conn_info.status = ConnectionInfo::ConnectionStatus::kRedirecting;

    // 6. 实际发送网络消息将在Network模块中实现
    // 这里仅记录日志和更新状态
    spdlog::info("Account {} redirected to MapServer {} ({}:{})",
                account_id, target_server_id,
                server_info.server_ip, server_info.server_port);
}

void AgentServerManager::SaveConnectionState(uint64_t session_id) {
    // 保存连接状态到数据库
    // 对应 legacy: 保存会话状态
}

void AgentServerManager::SendServerList(uint64_t session_id) {
    // 发送服务器列表给客户端
    // 对应 legacy: 发送服务器列表消息

    std::vector<MapServerInfo> server_list;
    for (const auto& [server_id, server_info] : map_servers_) {
        // 只发送可用的服务器
        if (server_info.available) {
            server_list.push_back(server_info);
        }
    }

    spdlog::debug("Sending server list to session {}: {} available servers",
                 session_id, server_list.size());

    // 对应 legacy: 发送MP_SERVER_LIST消息
    // 消息格式:
    // - Category: MP_SERVER
    // - Protocol: MP_SERVER_LIST
    // - Data: server_count, [server_id, server_name, player_count, server_status]

    // 序列化服务器列表
    // 注意: 实际网络发送将在Network模块中实现
    // 这里仅记录日志

    for (const auto& server : server_list) {
        spdlog::trace("  - Server {}: {} ({} players, {})",
                     server.server_id, server.server_name,
                     server.player_count,
                     server.available ? "available" : "unavailable");
    }

    // 更新连接状态
    if (connections_.find(session_id) != connections_.end()) {
        connections_[session_id].status = ConnectionInfo::ConnectionStatus::kServerListSent;
    }
}

void AgentServerManager::SendCharacterList(uint64_t account_id) {
    // 发送角色列表给客户端
    // 对应 legacy: 发送角色列表消息

    auto characters = GetCharacterList(account_id);

    spdlog::debug("Sending character list to account {}: {} characters",
                 account_id, characters.size());

    // 对应 legacy: 发送MP_CHARACTER_LIST消息
    // 消息格式:
    // - Category: MP_CHARACTER
    // - Protocol: MP_CHARACTER_LIST
    // - Data: character_count, [character_id, name, level, initial_weapon, gender, appearance...]

    // 序列化角色列表
    // 注意: 实际网络发送将在Network模块中实现
    // 这里仅记录日志

    for (const auto& character : characters) {
        spdlog::trace("  - Character {}: {} (Level {}, weapon={})",
                     character.character_id, character.name,
                     character.level, character.initial_weapon);
    }

    // 查找对应的会话
    uint64_t session_id = 0;
    for (const auto& [sid, conn_info] : connections_) {
        if (conn_info.account_id == account_id) {
            session_id = sid;
            break;
        }
    }

    if (session_id != 0) {
        // 更新连接状态
        connections_[session_id].status = ConnectionInfo::ConnectionStatus::kCharacterListSent;
        // 关联角色列表
        connections_[session_id].account_id = account_id;
    }

    spdlog::info("Character list sent for account {}: {} characters",
                account_id, characters.size());
}

// ========== 辅助方法 ==========

void AgentServerManager::LoadServerList() {
    // 简化实现 - 添加默认MapServer
    // 对应 legacy: ServerTable加载

    MapServerInfo server;
    server.server_id = 9001;
    server.map_id = 1;
    server.server_name = "Main World";
    server.server_ip = "127.0.0.1";
    server.server_port = 9001;
    server.player_count = 0;
    server.max_players = 1000;
    server.available = true;
    server.last_update = std::chrono::system_clock::now();

    map_servers_[server.server_id] = server;

    spdlog::info("Loaded server list: {} MapServer(s)", map_servers_.size());
}

std::string AgentServerManager::GenerateSessionToken(uint64_t account_id) {
    // 简化实现 - 生成基于时间戳和account_id的token
    // 对应 legacy: session token生成逻辑

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    std::string token = "TOKEN_" + std::to_string(account_id) + "_" + std::to_string(timestamp);
    return token;
}

// ========== 消息处理 ==========

void AgentServerManager::HandleMessage(uint64_t session_id, const Core::Network::ByteBuffer& buffer) {
    // 客户端格式: [MessageType:2][BodySize:4][Sequence:4][Reserved:2][Body:N]
    constexpr size_t HEADER_SIZE = 12;

    if (buffer.GetSize() < HEADER_SIZE) {
        spdlog::warn("Incomplete message from session {} (size: {})",
                     session_id, buffer.GetSize());
        return;
    }

    const uint8_t* data = buffer.GetData();

    // 读取 MessageType (2 bytes, little-endian)
    uint16_t message_type = data[0] | (data[1] << 8);

    // 读取 BodySize (4 bytes, little-endian)
    uint32_t body_size = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);

    // 读取 Sequence (4 bytes, 跳过)
    // Sequence 在 data[6-9]

    // Reserved (2 bytes, 跳过)
    // Reserved 在 data[10-11]

    // 提取消息体（跳过头部）
    Core::Network::ByteBuffer body_buffer;

    // 使用从header中解析出的body_size，而不是buffer.GetSize() - HEADER_SIZE
    // 因为buffer可能包含额外的数据或者数据不完整
    if (buffer.GetSize() < HEADER_SIZE + body_size) {
        spdlog::warn("Incomplete message body: expected {} bytes, got {} bytes",
                     body_size, buffer.GetSize() - HEADER_SIZE);
        return;
    }

    body_buffer.Write(data + HEADER_SIZE, body_size);

    spdlog::info("[DEBUG] Received message: type=0x{:04X}, body_size={}, total={}",
                 message_type, body_size, buffer.GetSize());

    // 根据消息类型分发
    switch (message_type) {
        case 0x0003:  // LoginRequest
            HandleLoginRequest(session_id, body_buffer);
            break;
        case 0x0007:  // CreateCharacterRequest
            HandleCreateCharacterRequest(session_id, body_buffer);
            break;
        case 0x0009:  // CharacterListRequest
            HandleCharacterListRequest(session_id, body_buffer);
            break;
        case 0x000B:  // SelectCharacterRequest
            HandleSelectCharacterRequest(session_id, body_buffer);
            break;
        case 0x000D:  // DeleteCharacterRequest
            // HandleDeleteCharacterRequest(session_id, body_buffer);
            spdlog::warn("DeleteCharacterRequest not implemented yet");
            break;
        default:
            spdlog::warn("Unknown message type: 0x{:04X}", message_type);
            break;
    }
}

void AgentServerManager::HandleLoginRequest(uint64_t session_id, const Core::Network::ByteBuffer& buffer) {
    spdlog::info("HandleLoginRequest: session={}, buffer_size={}", session_id, buffer.GetSize());

    // 解析登录请求 (Body: [UsernameLen:4][Username:N][PasswordLen:4][Password:N][VersionLen:4][Version:N][MacLen:4][Mac:N])
    const uint8_t* data = buffer.GetData();
    size_t offset = 0;
    size_t buffer_size = buffer.GetSize();

    // 读取用户名
    if (offset + 4 > buffer_size) {
        spdlog::warn("Invalid login request: missing username length");
        SendLoginResponse(session_id, Game::LoginResult::kSystemError, 0, "", "");
        return;
    }
    uint32_t username_len = data[offset] | (data[offset+1] << 8) | (data[offset+2] << 16) | (data[offset+3] << 24);
    offset += 4;

    if (offset + username_len > buffer_size) {
        spdlog::warn("Invalid login request: username too long");
        SendLoginResponse(session_id, Game::LoginResult::kSystemError, 0, "", "");
        return;
    }
    std::string username((char*)(data + offset), username_len);
    offset += username_len;

    // 读取密码
    if (offset + 4 > buffer_size) {
        spdlog::warn("Invalid login request: missing password length");
        SendLoginResponse(session_id, Game::LoginResult::kSystemError, 0, "", "");
        return;
    }
    uint32_t password_len = data[offset] | (data[offset+1] << 8) | (data[offset+2] << 16) | (data[offset+3] << 24);
    offset += 4;

    if (offset + password_len > buffer_size) {
        spdlog::warn("Invalid login request: password too long");
        SendLoginResponse(session_id, Game::LoginResult::kSystemError, 0, "", "");
        return;
    }
    std::string password((char*)(data + offset), password_len);

    // 获取客户端 IP（简化版本，实际应从连接中获取）
    std::string client_ip = "127.0.0.1";
    auto conn_it = network_connections_.find(session_id);
    if (conn_it != network_connections_.end() && conn_it->second) {
        client_ip = conn_it->second->GetRemoteAddress();
    }

    spdlog::info("Login attempt: username={}, password_len={}, ip={}", username, password_len, client_ip);

    // ========== 速率限制检查 ==========
    // 限制：每个 IP 最多 5 次尝试/分钟
    constexpr int MAX_LOGIN_ATTEMPTS = 5;
    constexpr int LOGIN_WINDOW_SECONDS = 60;

    auto& rate_limiter = Core::Security::RateLimiter::Instance();
    if (!rate_limiter.IsAllowed(client_ip, MAX_LOGIN_ATTEMPTS, LOGIN_WINDOW_SECONDS)) {
        spdlog::warn("Login rate limit exceeded: ip={}, username={}", client_ip, username);
        SendLoginResponse(session_id, Game::LoginResult::kTooManyAttempts, 0, "", "");
        return;
    }

    // 记录尝试
    rate_limiter.RecordAttempt(client_ip);

    // 调用 LoginManager 进行认证
    auto login_response = login_manager_->Login(username, password, client_ip, server_id_);

    // 获取 account_id (需要从数据库查询)
    uint64_t account_id = 0;
    if (login_response.result == Game::LoginResult::kSuccess) {
        auto account_opt = Core::Database::AccountDAO::LoadByUsername(username);
        if (account_opt.has_value()) {
            account_id = account_opt->account_id;
        }
    }

    // 更新会话状态
    if (login_response.result == Game::LoginResult::kSuccess) {
        auto it = connections_.find(session_id);
        if (it != connections_.end()) {
            it->second.account_id = account_id;
            it->second.status = ConnectionInfo::ConnectionStatus::kAuthenticated;
            spdlog::info("Login successful: account_id={}, session_id={}, username={}",
                account_id, session_id, username);
        }

        // ========== 添加到 SessionManager ==========
        Core::Session::SessionInfo session_info;
        session_info.session_id = login_response.session_id;
        session_info.account_id = account_id;
        session_info.session_token = login_response.session_token;
        session_info.client_ip = client_ip;
        session_info.server_id = server_id_;
        session_info.character_id = 0;
        session_info.login_time = std::chrono::system_clock::now();
        session_info.last_active_time = std::chrono::system_clock::now();
        session_info.is_active = true;

        Core::Session::SessionManager::Instance().AddOrUpdateSession(session_info);

        spdlog::info("Session added to SessionManager: session_id={}, account_id={}",
                     login_response.session_id, account_id);

        // 登录成功后清除速率限制记录
        rate_limiter.Clear(client_ip);
    } else {
        spdlog::warn("Login failed: username={}, result={}, remaining_attempts={}",
            username, static_cast<int>(login_response.result),
            rate_limiter.GetRemainingAttempts(client_ip, MAX_LOGIN_ATTEMPTS, LOGIN_WINDOW_SECONDS));
    }

    // 发送响应
    SendLoginResponse(session_id, login_response.result,
                     account_id,
                     login_response.session_token,
                     "TestServer");
}

void AgentServerManager::SendData(uint64_t session_id, const uint8_t* data, size_t length) {
    auto it = network_connections_.find(session_id);
    if (it == network_connections_.end()) {
        spdlog::error("Session %zu not found in network connections", session_id);
        return;
    }

    auto& conn = it->second;
    if (!conn || !conn->IsOpen()) {
        spdlog::warn("Connection for session %zu is closed", session_id);
        network_connections_.erase(it);
        return;
    }

    // 创建ByteBuffer
    auto buffer = std::make_shared<Core::Network::ByteBuffer>();
    buffer->Write(data, length);

    // 异步发送
    conn->AsyncSend(buffer, [this, session_id](const auto& ec, size_t sent) {
        if (ec) {
            spdlog::error("Failed to send data to session %zu: %s",
                        session_id, ec.message().c_str());
            // 移除断开的连接
            network_connections_.erase(session_id);
            RemoveConnection(session_id);
        } else {
            spdlog::debug("Sent %zu bytes to session %zu", sent, session_id);
        }
    });
}

void AgentServerManager::HandleCharacterListRequest(uint64_t session_id, const Core::Network::ByteBuffer& buffer) {
    spdlog::info("HandleCharacterListRequest: session={}, buffer_size={}", session_id, buffer.GetSize());

    // 获取账号ID（从session中获取）
    uint64_t account_id = 0;
    auto it = connections_.find(session_id);
    if (it != connections_.end()) {
        account_id = it->second.account_id;
    }

    if (account_id == 0) {
        spdlog::warn("Character list: unauthenticated session {}", session_id);
        // 返回错误响应
        std::vector<uint8_t> response;
        int32_t result_code = 1;  // 错误
        response.push_back((result_code >> 0) & 0xFF);
        response.push_back((result_code >> 8) & 0xFF);
        response.push_back((result_code >> 16) & 0xFF);
        response.push_back((result_code >> 24) & 0xFF);
        int32_t character_count = 0;
        response.push_back((character_count >> 0) & 0xFF);
        response.push_back((character_count >> 8) & 0xFF);
        response.push_back((character_count >> 16) & 0xFF);
        response.push_back((character_count >> 24) & 0xFF);

        uint32_t body_size = response.size();
        std::vector<uint8_t> header;
        header.push_back(0x0A); header.push_back(0x00);
        header.push_back((body_size >> 0) & 0xFF);
        header.push_back((body_size >> 8) & 0xFF);
        header.push_back((body_size >> 16) & 0xFF);
        header.push_back((body_size >> 24) & 0xFF);
        header.push_back(0x01); header.push_back(0x00);
        header.push_back(0x00); header.push_back(0x00);
        header.push_back(0x00); header.push_back(0x00);

        std::vector<uint8_t> complete_response;
        complete_response.insert(complete_response.end(), header.begin(), header.end());
        complete_response.insert(complete_response.end(), response.begin(), response.end());

        SendData(session_id, complete_response.data(), complete_response.size());
        return;
    }

    // 查询该账号的角色列表（使用真实数据库）
    spdlog::info("[DEBUG] Querying characters for account_id={}", account_id);
    auto characters = Murim::Core::Database::CharacterDAO::LoadByAccount(account_id);

    spdlog::info("Account {} has {} characters (from database)", account_id, characters.size());

    // 构造角色列表响应
    // Header: [MessageType:2bytes][BodySize:4bytes][Sequence:4bytes][Reserved:2bytes]
    // Body: [ResultCode:4bytes][CharacterCount:4bytes][Characters...]

    std::vector<uint8_t> response;
    int32_t result_code = 0;  // 成功
    int32_t character_count = characters.size();

    // === Body ===
    // ResultCode (4 bytes)
    response.push_back((result_code >> 0) & 0xFF);
    response.push_back((result_code >> 8) & 0xFF);
    response.push_back((result_code >> 16) & 0xFF);
    response.push_back((result_code >> 24) & 0xFF);

    // CharacterCount (4 bytes)
    response.push_back((character_count >> 0) & 0xFF);
    response.push_back((character_count >> 8) & 0xFF);
    response.push_back((character_count >> 16) & 0xFF);
    response.push_back((character_count >> 24) & 0xFF);

    // 添加每个角色的数据
    for (const auto& char_data : characters) {
        // CharacterID (8 bytes, little-endian)
        response.push_back((char_data.character_id >> 0) & 0xFF);
        response.push_back((char_data.character_id >> 8) & 0xFF);
        response.push_back((char_data.character_id >> 16) & 0xFF);
        response.push_back((char_data.character_id >> 24) & 0xFF);
        response.push_back((char_data.character_id >> 32) & 0xFF);
        response.push_back((char_data.character_id >> 40) & 0xFF);
        response.push_back((char_data.character_id >> 48) & 0xFF);
        response.push_back((char_data.character_id >> 56) & 0xFF);

        // NameLength (4 bytes) + Name
        uint32_t name_length = char_data.name.length();
        response.push_back((name_length >> 0) & 0xFF);
        response.push_back((name_length >> 8) & 0xFF);
        response.push_back((name_length >> 16) & 0xFF);
        response.push_back((name_length >> 24) & 0xFF);
        // 使用 const_pointer 来避免迭代器类型问题
        const char* name_ptr = char_data.name.c_str();
        response.insert(response.end(), name_ptr, name_ptr + name_length);

        // Level (4 bytes)
        response.push_back((char_data.level >> 0) & 0xFF);
        response.push_back((char_data.level >> 8) & 0xFF);
        response.push_back((char_data.level >> 16) & 0xFF);
        response.push_back((char_data.level >> 24) & 0xFF);

        // InitialWeapon (1 byte)
        response.push_back(char_data.initial_weapon);

        // Gender (4 bytes)
        response.push_back((char_data.gender >> 0) & 0xFF);
        response.push_back((char_data.gender >> 8) & 0xFF);
        response.push_back((char_data.gender >> 16) & 0xFF);
        response.push_back((char_data.gender >> 24) & 0xFF);

        spdlog::info("  - Character: id=%lu, name=%s, level=%u, weapon=%u, gender=%u",
                    char_data.character_id, char_data.name.c_str(), char_data.level,
                    char_data.initial_weapon, char_data.gender);
    }

    // === Header ===
    uint32_t body_size = response.size();
    uint32_t sequence = 1;

    std::vector<uint8_t> header;

    // MessageType (0x0A = CharacterListResponse, 2 bytes little-endian)
    header.push_back(0x0A);
    header.push_back(0x00);

    // BodySize (4 bytes little-endian)
    header.push_back((body_size >> 0) & 0xFF);
    header.push_back((body_size >> 8) & 0xFF);
    header.push_back((body_size >> 16) & 0xFF);
    header.push_back((body_size >> 24) & 0xFF);

    // Sequence (4 bytes little-endian)
    header.push_back((sequence >> 0) & 0xFF);
    header.push_back((sequence >> 8) & 0xFF);
    header.push_back((sequence >> 16) & 0xFF);
    header.push_back((sequence >> 24) & 0xFF);

    // Reserved (2 bytes, must be 0 for client compatibility)
    header.push_back(0x00);
    header.push_back(0x00);

    // Combine header + body
    std::vector<uint8_t> complete_response;
    complete_response.insert(complete_response.end(), header.begin(), header.end());
    complete_response.insert(complete_response.end(), response.begin(), response.end());

    printf("[DEBUG] Sending character list response: header_size=%zu, body_size=%u, total=%zu\n",
           header.size(), body_size, complete_response.size());
    fflush(stdout);

    // 发送响应
    SendData(session_id, complete_response.data(), complete_response.size());

    spdlog::info("Sent character list response to session %zu, count=%d",
                session_id, character_count);
}

void AgentServerManager::HandleSelectCharacterRequest(uint64_t session_id, const Core::Network::ByteBuffer& buffer) {
    spdlog::info("HandleSelectCharacterRequest: session={}, buffer_size={}", session_id, buffer.GetSize());

    // 解析请求 (Body: [CharacterID:8bytes])
    if (buffer.GetSize() < 8) {
        spdlog::error("Invalid select character request: size too small (got {})", buffer.GetSize());
        return;
    }

    const uint8_t* data = buffer.GetData();

    // 读取 CharacterID (8 bytes little-endian)
    uint64_t character_id = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24) |
                            (data[4] << 32) | (data[5] << 40) | (data[6] << 48) | (data[7] << 56);

    spdlog::info("Select character: character_id={}", character_id);

    // 获取账号ID（从session中获取）
    uint64_t account_id = 0;
    auto it = connections_.find(session_id);
    if (it != connections_.end()) {
        account_id = it->second.account_id;
    }

    if (account_id == 0) {
        spdlog::warn("Invalid session: no account_id found");
        // 返回错误响应
        SendSelectCharacterResponse(session_id, 1, "127.0.0.1", 0);
        return;
    }

    // 调用选择角色逻辑
    bool success = SelectCharacter(account_id, character_id, 0);

    // 构造响应
    int32_t result_code = success ? 0 : 1;
    std::string map_server_ip = "127.0.0.1";
    int32_t map_server_port = 9001;  // MapServer 端口

    SendSelectCharacterResponse(session_id, result_code, map_server_ip, map_server_port);

    spdlog::info("Sent select character response: result=%d, server=%s:%d",
                result_code, map_server_ip.c_str(), map_server_port);
}

void AgentServerManager::SendSelectCharacterResponse(
    uint64_t session_id,
    int32_t result_code,
    const std::string& map_server_ip,
    int32_t map_server_port) {

    // 构造响应 body
    std::vector<uint8_t> response;

    // ResultCode (4 bytes little-endian)
    response.push_back((result_code >> 0) & 0xFF);
    response.push_back((result_code >> 8) & 0xFF);
    response.push_back((result_code >> 16) & 0xFF);
    response.push_back((result_code >> 24) & 0xFF);

    // MapServerIP (FString) - 4 bytes length + string data
    uint32_t ip_length = static_cast<uint32_t>(map_server_ip.size());
    response.push_back((ip_length >> 0) & 0xFF);
    response.push_back((ip_length >> 8) & 0xFF);
    response.push_back((ip_length >> 16) & 0xFF);
    response.push_back((ip_length >> 24) & 0xFF);

    // IP string data
    response.insert(response.end(), map_server_ip.begin(), map_server_ip.end());

    // MapServerPort (4 bytes little-endian)
    response.push_back((map_server_port >> 0) & 0xFF);
    response.push_back((map_server_port >> 8) & 0xFF);
    response.push_back((map_server_port >> 16) & 0xFF);
    response.push_back((map_server_port >> 24) & 0xFF);

    // MapID (4 bytes little-endian) - 默认值: 1
    uint32_t map_id = 1;
    response.push_back((map_id >> 0) & 0xFF);
    response.push_back((map_id >> 8) & 0xFF);
    response.push_back((map_id >> 16) & 0xFF);
    response.push_back((map_id >> 24) & 0xFF);

    // Position (X, Y, Z) - 默认出生点 (1000.0f, 1000.0f, 0.0f)
    float pos_x = 1000.0f, pos_y = 1000.0f, pos_z = 0.0f;
    auto pos_x_bytes = reinterpret_cast<uint8_t*>(&pos_x);
    auto pos_y_bytes = reinterpret_cast<uint8_t*>(&pos_y);
    auto pos_z_bytes = reinterpret_cast<uint8_t*>(&pos_z);

    response.insert(response.end(), pos_x_bytes, pos_x_bytes + 4);
    response.insert(response.end(), pos_y_bytes, pos_y_bytes + 4);
    response.insert(response.end(), pos_z_bytes, pos_z_bytes + 4);

    // 构造 header
    uint32_t body_size = static_cast<uint32_t>(response.size());
    std::vector<uint8_t> header;

    // MessageType (0x0C = SelectCharacterResponse, 2 bytes little-endian)
    header.push_back(0x0C);
    header.push_back(0x00);

    // BodySize (4 bytes little-endian)
    header.push_back((body_size >> 0) & 0xFF);
    header.push_back((body_size >> 8) & 0xFF);
    header.push_back((body_size >> 16) & 0xFF);
    header.push_back((body_size >> 24) & 0xFF);

    // Sequence (4 bytes little-endian)
    uint32_t sequence = 1;
    header.push_back((sequence >> 0) & 0xFF);
    header.push_back((sequence >> 8) & 0xFF);
    header.push_back((sequence >> 16) & 0xFF);
    header.push_back((sequence >> 24) & 0xFF);

    // Reserved (2 bytes)
    header.push_back(0x00);
    header.push_back(0x00);

    // Combine header + body
    std::vector<uint8_t> complete_response;
    complete_response.insert(complete_response.end(), header.begin(), header.end());
    complete_response.insert(complete_response.end(), response.begin(), response.end());

    // 发送响应
    SendData(session_id, complete_response.data(), complete_response.size());
}

void AgentServerManager::HandleCreateCharacterRequest(uint64_t session_id, const Core::Network::ByteBuffer& buffer) {
    spdlog::info("HandleCreateCharacterRequest: session={}, buffer_size={}", session_id, buffer.GetSize());

    // 解析请求 (Body: [NameLength:4][Name:N][InitialWeapon:1][Gender:1])
    if (buffer.GetSize() < 6) {  // 最小: NameLength(4) + InitialWeapon(1) + Gender(1)
        spdlog::error("Invalid create character request: size too small (got {})", buffer.GetSize());
        return;
    }

    const uint8_t* data = buffer.GetData();

    // 读取 NameLength (4 bytes) - 使用小端序逐字节读取
    uint32_t name_length = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

    if (buffer.GetSize() < 4 + name_length + 2) {
        spdlog::error("Invalid create character request: name too long");
        return;
    }

    // 读取 Name
    std::string character_name((char*)(data + 4), name_length);

    // 读取 InitialWeapon (1 byte)
    uint8_t initial_weapon = data[4 + name_length];

    // 读取 Gender (1 byte)
    uint8_t gender = data[4 + name_length + 1];

    spdlog::info("Create character: name={}, weapon={}, gender={}", character_name, initial_weapon, gender);

    // 获取账号ID（从session中获取）
    uint64_t account_id = 0;
    auto it = connections_.find(session_id);
    if (it != connections_.end()) {
        account_id = it->second.account_id;
    }

    if (account_id == 0) {
        spdlog::warn("Create character: unauthenticated session {}", session_id);
        // 返回错误响应
        int32_t result_code = 1;
        std::vector<uint8_t> response;
        response.push_back((result_code >> 0) & 0xFF);
        response.push_back((result_code >> 8) & 0xFF);
        response.push_back((result_code >> 16) & 0xFF);
        response.push_back((result_code >> 24) & 0xFF);

        uint32_t body_size = response.size();
        std::vector<uint8_t> header;
        header.push_back(0x08); header.push_back(0x00);
        header.push_back((body_size >> 0) & 0xFF);
        header.push_back((body_size >> 8) & 0xFF);
        header.push_back((body_size >> 16) & 0xFF);
        header.push_back((body_size >> 24) & 0xFF);
        header.push_back(0x01); header.push_back(0x00);
        header.push_back(0x00); header.push_back(0x00);
        header.push_back(0x00); header.push_back(0x00);

        std::vector<uint8_t> complete_response;
        complete_response.insert(complete_response.end(), header.begin(), header.end());
        complete_response.insert(complete_response.end(), response.begin(), response.end());

        SendData(session_id, complete_response.data(), complete_response.size());
        spdlog::info("Sent create character response: FAILURE (unauthenticated)");
        return;
    }

    // 检查角色名是否已存在
    for (const auto& pair : mock_characters_) {
        if (pair.second.name == character_name) {
            spdlog::warn("Character name already exists: %s", character_name.c_str());
            // 角色名已存在，返回错误
            int32_t result_code = 1;
            std::vector<uint8_t> response;
            response.push_back((result_code >> 0) & 0xFF);
            response.push_back((result_code >> 8) & 0xFF);
            response.push_back((result_code >> 16) & 0xFF);
            response.push_back((result_code >> 24) & 0xFF);

            uint32_t body_size = response.size();
            std::vector<uint8_t> header;
            header.push_back(0x08);  // CreateCharacterResponse
            header.push_back(0x00);
            header.push_back((body_size >> 0) & 0xFF);
            header.push_back((body_size >> 8) & 0xFF);
            header.push_back((body_size >> 16) & 0xFF);
            header.push_back((body_size >> 24) & 0xFF);
            header.push_back(1);  // sequence
            header.push_back(0);
            header.push_back(0);
            header.push_back(0);
            header.push_back(0x00);
            header.push_back(0x00);

            std::vector<uint8_t> complete_response;
            complete_response.insert(complete_response.end(), header.begin(), header.end());
            complete_response.insert(complete_response.end(), response.begin(), response.end());

            SendData(session_id, complete_response.data(), complete_response.size());
            spdlog::info("Sent create character response: FAILURE (name exists)");
            return;
        }
    }

    // 创建新角色并保存到数据库
    Game::Character new_char;
    new_char.account_id = account_id;
    new_char.name = character_name;
    new_char.initial_weapon = initial_weapon;
    new_char.gender = gender;
    new_char.level = 1;
    new_char.exp = 0;
    new_char.money = 0;
    new_char.hp = 100;
    new_char.max_hp = 100;
    new_char.mp = 50;
    new_char.max_mp = 50;
    new_char.stamina = 100;
    new_char.max_stamina = 100;
    new_char.face_style = 0;
    new_char.hair_style = 0;
    new_char.hair_color = 0;
    new_char.map_id = 1;
    new_char.x = 1000.0f;
    new_char.y = 1000.0f;
    new_char.z = 0.0f;
    new_char.direction = 0;
    new_char.create_time = std::chrono::system_clock::now();
    new_char.total_play_time = 0;
    new_char.status = Murim::Game::CharacterStatus::kAlive;

    // 调用 CharacterDAO 保存到数据库
    uint64_t character_id = Murim::Core::Database::CharacterDAO::Create(new_char);

    if (character_id == 0) {
        spdlog::error("Failed to create character in database");
        // 返回错误响应
        int32_t result_code = 1;
        std::vector<uint8_t> response;
        response.push_back((result_code >> 0) & 0xFF);
        response.push_back((result_code >> 8) & 0xFF);
        response.push_back((result_code >> 16) & 0xFF);
        response.push_back((result_code >> 24) & 0xFF);

        uint32_t body_size = response.size();
        std::vector<uint8_t> header;
        header.push_back(0x08); header.push_back(0x00);
        header.push_back((body_size >> 0) & 0xFF);
        header.push_back((body_size >> 8) & 0xFF);
        header.push_back((body_size >> 16) & 0xFF);
        header.push_back((body_size >> 24) & 0xFF);
        header.push_back(0x01); header.push_back(0x00);
        header.push_back(0x00); header.push_back(0x00);
        header.push_back(0x00); header.push_back(0x00);

        std::vector<uint8_t> complete_response;
        complete_response.insert(complete_response.end(), header.begin(), header.end());
        complete_response.insert(complete_response.end(), response.begin(), response.end());

        SendData(session_id, complete_response.data(), complete_response.size());
        spdlog::info("Sent create character response: FAILURE (database error)");
        return;
    }

    // 保存到内存缓存
    CharacterData cache_char;
    cache_char.character_id = character_id;
    cache_char.account_id = account_id;
    cache_char.name = character_name;
    cache_char.initial_weapon = initial_weapon;
    cache_char.gender = gender;
    cache_char.level = 1;
    mock_characters_[character_id] = cache_char;
    mock_account_characters_[account_id].push_back(character_id);

    spdlog::info("Created character: id=%lu, name=%s, weapon=%d, gender=%d",
                character_id, character_name.c_str(), initial_weapon, gender);

    // 构造成功响应
    int32_t result_code = 0;  // 0 = 成功

    std::vector<uint8_t> response;

    // === Body ===
    // ResultCode (4 bytes little-endian)
    response.push_back((result_code >> 0) & 0xFF);
    response.push_back((result_code >> 8) & 0xFF);
    response.push_back((result_code >> 16) & 0xFF);
    response.push_back((result_code >> 24) & 0xFF);

    // CharacterID (8 bytes little-endian)
    response.push_back((character_id >> 0) & 0xFF);
    response.push_back((character_id >> 8) & 0xFF);
    response.push_back((character_id >> 16) & 0xFF);
    response.push_back((character_id >> 24) & 0xFF);
    response.push_back((character_id >> 32) & 0xFF);
    response.push_back((character_id >> 40) & 0xFF);
    response.push_back((character_id >> 48) & 0xFF);
    response.push_back((character_id >> 56) & 0xFF);

    // === Header ===
    uint32_t body_size = response.size();
    uint32_t sequence = 1;

    std::vector<uint8_t> header;

    // MessageType (0x08 = CreateCharacterResponse, 2 bytes little-endian)
    header.push_back(0x08);
    header.push_back(0x00);

    // BodySize (4 bytes little-endian)
    header.push_back((body_size >> 0) & 0xFF);
    header.push_back((body_size >> 8) & 0xFF);
    header.push_back((body_size >> 16) & 0xFF);
    header.push_back((body_size >> 24) & 0xFF);

    // Sequence (4 bytes little-endian)
    header.push_back((sequence >> 0) & 0xFF);
    header.push_back((sequence >> 8) & 0xFF);
    header.push_back((sequence >> 16) & 0xFF);
    header.push_back((sequence >> 24) & 0xFF);

    // Reserved (2 bytes, must be 0 for client compatibility)
    header.push_back(0x00);
    header.push_back(0x00);

    // Combine header + body
    std::vector<uint8_t> complete_response;
    complete_response.insert(complete_response.end(), header.begin(), header.end());
    complete_response.insert(complete_response.end(), response.begin(), response.end());

    printf("[DEBUG] Sending create character response: result=%d, header_size=%zu, body_size=%u, total=%zu\n",
           result_code, header.size(), body_size, complete_response.size());
    fflush(stdout);

    // 发送响应
    SendData(session_id, complete_response.data(), complete_response.size());

    spdlog::info("Sent create character response to session %zu, result=%d, character=%s",
                session_id, result_code, character_name.c_str());
}

void AgentServerManager::SendLoginResponse(
    uint64_t session_id,
    Game::LoginResult result_code,
    uint64_t account_id,
    const std::string& session_token,
    const std::string& server_name
) {
    std::vector<uint8_t> response;

    // ResultCode (4 bytes, little-endian)
    uint32_t result = static_cast<uint32_t>(result_code);
    response.push_back(result & 0xFF);
    response.push_back((result >> 8) & 0xFF);
    response.push_back((result >> 16) & 0xFF);
    response.push_back((result >> 24) & 0xFF);

    if (result_code == Game::LoginResult::kSuccess) {
        // SessionToken (长度 + 数据)
        uint32_t token_len = session_token.length();

        // Debug: 打印token长度的字节
        std::stringstream token_len_hex;
        token_len_hex << std::hex << std::setfill('0');
        token_len_hex << std::setw(2) << (token_len & 0xFF) << " ";
        token_len_hex << std::setw(2) << ((token_len >> 8) & 0xFF) << " ";
        token_len_hex << std::setw(2) << ((token_len >> 16) & 0xFF) << " ";
        token_len_hex << std::setw(2) << ((token_len >> 24) & 0xFF);
        spdlog::debug("[SEND] Token length bytes: {}", token_len_hex.str());

        response.push_back(token_len & 0xFF);
        response.push_back((token_len >> 8) & 0xFF);
        response.push_back((token_len >> 16) & 0xFF);
        response.push_back((token_len >> 24) & 0xFF);
        response.insert(response.end(), session_token.begin(), session_token.end());

        // AccountID (8 bytes, little-endian)
        for (int i = 0; i < 8; i++) {
            response.push_back((account_id >> (i * 8)) & 0xFF);
        }

        // ServerName (长度 + 数据)
        uint32_t server_len = server_name.length();
        response.push_back(server_len & 0xFF);
        response.push_back((server_len >> 8) & 0xFF);
        response.push_back((server_len >> 16) & 0xFF);
        response.push_back((server_len >> 24) & 0xFF);
        response.insert(response.end(), server_name.begin(), server_name.end());
    }

    // 添加头部: [MessageType:2][BodySize:4][Sequence:4][Reserved:2]
    std::vector<uint8_t> complete_msg;
    // MessageType: 0x0004 (LoginResponse)
    complete_msg.push_back(0x04); complete_msg.push_back(0x00);
    // BodySize
    uint32_t body_size = response.size();
    complete_msg.push_back(body_size & 0xFF);
    complete_msg.push_back((body_size >> 8) & 0xFF);
    complete_msg.push_back((body_size >> 16) & 0xFF);
    complete_msg.push_back((body_size >> 24) & 0xFF);
    // Sequence: 1
    complete_msg.push_back(0x01); complete_msg.push_back(0x00);
    complete_msg.push_back(0x00); complete_msg.push_back(0x00);
    // Reserved: 0
    complete_msg.push_back(0x00); complete_msg.push_back(0x00);

    // 附加消息体
    complete_msg.insert(complete_msg.end(), response.begin(), response.end());

    spdlog::info("Sending login response: result={}, account_id={}, token_len={}, server_name={}",
                 static_cast<int>(result_code), account_id, session_token.length(), server_name);

    // 发送
    SendData(session_id, complete_msg.data(), complete_msg.size());
}

} // namespace AgentServer
} // namespace Murim
