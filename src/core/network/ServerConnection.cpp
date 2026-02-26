#include "ServerConnection.hpp"
#include "IOContext.hpp"
#include "spdlog_wrapper.hpp"
#include <boost/asio/placeholders.hpp>
#include <thread>
#include <algorithm>
#include <cstring>

namespace Murim {
namespace Core {
namespace Network {

// ============== ServerConnection Implementation ==============

ServerConnection::ServerConnection(
    IOContext& io_context,
    const ServerId& server_id,
    const ServerId& target_server_id,
    const std::string& host,
    uint16_t port)
    : io_context_(io_context)
    , connection_(std::make_shared<Connection>(io_context, 0))
    , server_id_(server_id)
    , target_server_id_(target_server_id)
    , host_(host)
    , port_(port)
    , state_(ServerConnectionState::DISCONNECTED)
    , reconnect_enabled_(false)
    , reconnect_attempts_(0)
    , max_reconnect_attempts_(0)
    , sequence_counter_(0)
    , heartbeat_interval_(std::chrono::milliseconds(30000))  // 30 seconds default
    , reconnect_initial_delay_(std::chrono::milliseconds(1000))
    , reconnect_max_delay_(std::chrono::milliseconds(30000))
    , heartbeat_timer_(std::make_unique<boost::asio::steady_timer>(io_context_.GetIOContext()))
    , reconnect_timer_(std::make_unique<boost::asio::steady_timer>(io_context_.GetIOContext()))
    , recv_buffer_(8192)
    , recv_pos_(0)
    , expected_size_(0)
    , request_timeout_timer_(std::make_unique<boost::asio::steady_timer>(io_context_.GetIOContext())) {

    // Set up connection message handler
    connection_->SetMessageHandler([this](const ByteBuffer& buffer) {
        auto buf_ptr = std::make_shared<ByteBuffer>(buffer.GetData(), buffer.GetSize());
        HandleReceivedData(buf_ptr);
    });

    spdlog::info("ServerConnection created: {}:{} -> {}:{}",
                 ServerTypeToString(server_id_.type), server_id_.id,
                 ServerTypeToString(target_server_id_.type), target_server_id_.id);
}

ServerConnection::ServerConnection(
    Connection::Ptr connection,
    const ServerId& server_id,
    const ServerId& target_server_id)
    : io_context_(*reinterpret_cast<IOContext*>(nullptr))  // Will be set from connection
    , connection_(connection)
    , server_id_(server_id)
    , target_server_id_(target_server_id)
    , port_(0)
    , state_(ServerConnectionState::CONNECTED)
    , reconnect_enabled_(false)
    , reconnect_attempts_(0)
    , max_reconnect_attempts_(0)
    , sequence_counter_(0)
    , heartbeat_interval_(std::chrono::milliseconds(30000))
    , reconnect_initial_delay_(std::chrono::milliseconds(1000))
    , reconnect_max_delay_(std::chrono::milliseconds(30000))
    , recv_buffer_(8192)
    , recv_pos_(0)
    , expected_size_(0) {

    // Set up connection message handler
    connection_->SetMessageHandler([this](const ByteBuffer& buffer) {
        auto buf_ptr = std::make_shared<ByteBuffer>(buffer.GetData(), buffer.GetSize());
        HandleReceivedData(buf_ptr);
    });

    stats_.connect_time = std::chrono::system_clock::now();

    spdlog::info("ServerConnection accepted: {}:{} <- {}:{}",
                 ServerTypeToString(server_id_.type), server_id_.id,
                 ServerTypeToString(target_server_id_.type), target_server_id_.id);
}

ServerConnection::~ServerConnection() {
    spdlog::debug("ServerConnection destroyed: {}:{}",
                  ServerTypeToString(target_server_id_.type), target_server_id_.id);
    Disconnect(false);
}

bool ServerConnection::Connect() {
    if (state_.load() != ServerConnectionState::DISCONNECTED) {
        spdlog::warn("Connection already in progress: {}:{}",
                     ServerTypeToString(target_server_id_.type), target_server_id_.id);
        return false;
    }

    SetState(ServerConnectionState::CONNECTING);

    connection_->AsyncConnect(host_, port_,
        [this, self = shared_from_this()](const boost::system::error_code& ec) {
            if (ec) {
                spdlog::error("Connection failed to {}:{} - {}",
                              host_, port_, ec.message());
                SetState(ServerConnectionState::STATE_ERROR);

                if (reconnect_enabled_) {
                    Reconnect();
                }
                return;
            }

            spdlog::info("Connected to {}:{}", host_, port_);
            SetState(ServerConnectionState::CONNECTED);
            stats_.connect_time = std::chrono::system_clock::now();

            // Send registration message
            auto reg_msg = ServerMessageBuilder::CreateServerRegister(
                server_id_, target_server_id_, host_, port_);
            SendMessage(reg_msg);

            // Start receiving
            connection_->AsyncReceive();

            // Start heartbeat
            StartHeartbeat();
        });

    return true;
}

void ServerConnection::Disconnect(bool graceful) {
    auto prev_state = state_.exchange(ServerConnectionState::DISCONNECTED);

    // Stop timers
    StopHeartbeat();
    if (reconnect_timer_) {
        reconnect_timer_->cancel();
    }
    if (request_timeout_timer_) {
        request_timeout_timer_->cancel();
    }

    // Send graceful shutdown if requested
    if (graceful && (prev_state == ServerConnectionState::CONNECTED ||
                     prev_state == ServerConnectionState::AUTHENTICATED)) {
        auto shutdown_msg = std::make_shared<ServerMessage>(
            ServerMessageType::SERVER_SHUTDOWN, server_id_, target_server_id_);
        SendMessage(shutdown_msg);

        // Give time for message to be sent
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Close connection
    if (connection_) {
        connection_->Close();
    }

    spdlog::info("Disconnected from {}:{}",
                 ServerTypeToString(target_server_id_.type), target_server_id_.id);
}

bool ServerConnection::SendMessage(ServerMessage::Ptr message) {
    if (!IsConnected()) {
        spdlog::warn("Cannot send message - not connected: {}:{}",
                     ServerTypeToString(target_server_id_.type), target_server_id_.id);
        return false;
    }

    // Set sequence number
    message->SetSequence(NextSequence());

    // Add to queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        send_queue_.push(message);
    }

    // Process queue
    ProcessMessageQueue();

    return true;
}

ServerMessage::Ptr ServerConnection::SendRequest(
    ServerMessage::Ptr request,
    uint32_t timeout_ms) {

    // Set sequence number
    request->SetSequence(NextSequence());

    // Store pending request
    {
        std::lock_guard<std::mutex> lock(requests_mutex_);
        PendingRequest pending;
        pending.request = request;
        pending.timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        pending_requests_[request->GetSequence()] = pending;
    }

    // Send request
    if (!SendMessage(request)) {
        std::lock_guard<std::mutex> lock(requests_mutex_);
        pending_requests_.erase(request->GetSequence());
        return nullptr;
    }

    // Wait for response (simplified - in production would use condition variable)
    // For now, return nullptr as async pattern is preferred
    spdlog::warn("SendRequest not fully implemented - use async pattern");
    return nullptr;
}

void ServerConnection::SetState(ServerConnectionState new_state) {
    auto old_state = state_.exchange(new_state);

    if (old_state != new_state) {
        spdlog::info("Connection state changed: {}:{} {} -> {}",
                     ServerTypeToString(target_server_id_.type), target_server_id_.id,
                     static_cast<uint32_t>(old_state),
                     static_cast<uint32_t>(new_state));
    }
}

std::string ServerConnection::GetRemoteAddress() const {
    if (connection_) {
        return connection_->GetRemoteAddress();
    }
    return host_ + ":" + std::to_string(port_);
}

void ServerConnection::SetReconnectConfig(
    bool enabled,
    uint32_t max_attempts,
    uint32_t initial_delay_ms,
    uint32_t max_delay_ms) {

    reconnect_enabled_ = enabled;
    max_reconnect_attempts_ = max_attempts;
    reconnect_initial_delay_ = std::chrono::milliseconds(initial_delay_ms);
    reconnect_max_delay_ = std::chrono::milliseconds(max_delay_ms);

    spdlog::info("Reconnect config: enabled={}, max_attempts={}, initial_delay={}ms, max_delay={}ms",
                 enabled, max_attempts, initial_delay_ms, max_delay_ms);
}

void ServerConnection::HandleReceivedData(ByteBuffer::Ptr buffer) {
    // Append to receive buffer
    size_t buffer_size = buffer->GetSize();
    if (recv_pos_ + buffer_size > recv_buffer_.size()) {
        recv_buffer_.resize(recv_pos_ + buffer_size);
    }
    std::memcpy(recv_buffer_.data() + recv_pos_, buffer->GetData(), buffer_size);
    recv_pos_ += buffer_size;

    // Parse messages
    while (true) {
        // Check if we have message size
        if (expected_size_ == 0 && recv_pos_ >= 4) {
            // Read message size (first 4 bytes)
            uint32_t size;
            std::memcpy(&size, recv_buffer_.data(), sizeof(uint32_t));
            expected_size_ = size;

            // Validate size
            if (expected_size_ > 1024 * 1024) {  // 1MB max message size
                spdlog::error("Message size too large: {} bytes", expected_size_);
                Disconnect(false);
                return;
            }
        }

        // Check if we have complete message
        if (expected_size_ > 0 && recv_pos_ >= expected_size_ + 4) {
            // Parse message
            auto msg_buffer = std::make_shared<ByteBuffer>(
                recv_buffer_.data() + 4, expected_size_);
            auto message = ParseMessage(msg_buffer);

            if (message) {
                // Update statistics
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.messages_received++;
                    stats_.bytes_received += expected_size_;
                    stats_.last_message_time = std::chrono::system_clock::now();
                }

                // Handle message
                if (message_handler_) {
                    message_handler_(message);
                }
            }

            // Remove processed message from buffer
            size_t remaining = recv_pos_ - (expected_size_ + 4);
            if (remaining > 0) {
                std::memmove(recv_buffer_.data(),
                           recv_buffer_.data() + expected_size_ + 4,
                           remaining);
            }
            recv_pos_ = remaining;
            expected_size_ = 0;
        } else {
            // Need more data
            break;
        }
    }
}

ServerMessage::Ptr ServerConnection::ParseMessage(ByteBuffer::Ptr buffer) {
    try {
        // Read message header
        uint32_t type_val = buffer->ReadUInt32();
        uint32_t source_type_val = buffer->ReadUInt32();
        uint32_t source_id = buffer->ReadUInt32();
        uint32_t dest_type_val = buffer->ReadUInt32();
        uint32_t dest_id = buffer->ReadUInt32();
        uint64_t sequence = buffer->ReadUInt64();
        uint64_t timestamp = buffer->ReadUInt64();
        uint32_t payload_size = buffer->ReadUInt32();

        // Create message
        auto msg = std::make_shared<ServerMessage>(
            static_cast<ServerMessageType>(type_val),
            ServerId(static_cast<ServerId::Type>(source_type_val), source_id),
            ServerId(static_cast<ServerId::Type>(dest_type_val), dest_id),
            sequence);

        msg->SetTimestamp(timestamp);

        // Read payload
        if (payload_size > 0) {
            // Payload data is already in buffer
            // Message handler will read it
        }

        spdlog::trace("Received message: {} from {}:{} to {}:{}",
                      MessageTypeToString(msg->GetType()),
                      ServerTypeToString(msg->GetSource().type), msg->GetSource().id,
                      ServerTypeToString(msg->GetDestination().type), msg->GetDestination().id);

        return msg;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse message: {}", e.what());
        return nullptr;
    }
}

void ServerConnection::ProcessMessageQueue() {
    // Try to send next message if not already sending
    SendNextMessage();
}

void ServerConnection::SendNextMessage() {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (send_queue_.empty()) {
        return;
    }

    auto message = send_queue_.front();
    auto buffer = message->Serialize();

    connection_->AsyncSend(buffer,
        [this, self = shared_from_this()](const boost::system::error_code& ec, size_t bytes_sent) {
            HandleSend(ec, bytes_sent);
        });
}

void ServerConnection::HandleSend(const boost::system::error_code& ec, size_t bytes_sent) {
    if (ec) {
        spdlog::error("Send failed: {}", ec.message());
        SetState(ServerConnectionState::STATE_ERROR);

        if (reconnect_enabled_) {
            Reconnect();
        }
        return;
    }

    // Remove sent message from queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!send_queue_.empty()) {
            send_queue_.pop();
        }
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.messages_sent++;
        stats_.bytes_sent += bytes_sent;
    }

    // Send next message
    SendNextMessage();
}

void ServerConnection::Reconnect() {
    if (!reconnect_enabled_) {
        return;
    }

    uint32_t attempt = reconnect_attempts_.fetch_add(1) + 1;

    if (max_reconnect_attempts_ > 0 && attempt > max_reconnect_attempts_) {
        spdlog::error("Max reconnect attempts reached: {}:{}",
                      ServerTypeToString(target_server_id_.type), target_server_id_.id);
        return;
    }

    // Calculate delay with exponential backoff
    uint32_t delay_ms = static_cast<uint32_t>(
        reconnect_initial_delay_.count() * std::pow(2, attempt - 1));
    delay_ms = std::min(delay_ms,
                       static_cast<uint32_t>(reconnect_max_delay_.count()));

    spdlog::info("Scheduling reconnect in {}ms (attempt {})", delay_ms, attempt);

    ScheduleReconnect(delay_ms);
}

void ServerConnection::ScheduleReconnect(uint32_t delay_ms) {
    SetState(ServerConnectionState::RECONNECTING);

    reconnect_timer_->expires_after(std::chrono::milliseconds(delay_ms));
    reconnect_timer_->async_wait(
        [this, self = shared_from_this()](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            spdlog::info("Reconnecting to {}:{} (attempt {})",
                        ServerTypeToString(target_server_id_.type), target_server_id_.id,
                        reconnect_attempts_.load());

            Connect();
        });
}

void ServerConnection::StartHeartbeat() {
    heartbeat_timer_->expires_after(heartbeat_interval_);
    heartbeat_timer_->async_wait(
        [this, self = shared_from_this()](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }

            if (IsConnected()) {
                SendHeartbeat();
            }

            // Schedule next heartbeat
            StartHeartbeat();
        });
}

void ServerConnection::StopHeartbeat() {
    if (heartbeat_timer_) {
        heartbeat_timer_->cancel();
    }
}

void ServerConnection::SendHeartbeat() {
    auto heartbeat = ServerMessageBuilder::CreateHeartbeat(server_id_, target_server_id_);
    SendMessage(heartbeat);

    spdlog::trace("Sent heartbeat to {}:{}",
                  ServerTypeToString(target_server_id_.type), target_server_id_.id);
}

// ============== ServerConnectionManager Implementation ==============

ServerConnectionManager::ServerConnectionManager(
    IOContext& io_context,
    const ServerId& server_id)
    : io_context_(io_context)
    , server_id_(server_id)
    , connection_callback_(nullptr) {

    spdlog::info("ServerConnectionManager created for {}:{}",
                 ServerTypeToString(server_id.type), server_id.id);
}

ServerConnectionManager::~ServerConnectionManager() {
    DisconnectAll(false);
    spdlog::info("ServerConnectionManager destroyed");
}

void ServerConnectionManager::AddConnection(ServerConnection::Ptr connection) {
    std::lock_guard<std::mutex> lock(connections_mutex_);

    uint32_t key = MakeConnectionKey(connection->GetTargetServerId());
    connections_[key] = connection;

    spdlog::info("Connection added: {}:{} (total: {})",
                 ServerTypeToString(connection->GetTargetServerId().type),
                 connection->GetTargetServerId().id,
                 connections_.size());

    if (connection_callback_) {
        connection_callback_(connection);
    }
}

void ServerConnectionManager::RemoveConnection(const ServerId& server_id) {
    std::lock_guard<std::mutex> lock(connections_mutex_);

    uint32_t key = MakeConnectionKey(server_id);
    auto it = connections_.find(key);
    if (it != connections_.end()) {
        it->second->Disconnect(false);
        connections_.erase(it);

        spdlog::info("Connection removed: {}:{} (total: {})",
                     ServerTypeToString(server_id.type), server_id.id,
                     connections_.size());
    }
}

ServerConnection::Ptr ServerConnectionManager::GetConnection(const ServerId& server_id) {
    std::lock_guard<std::mutex> lock(connections_mutex_);

    uint32_t key = MakeConnectionKey(server_id);
    auto it = connections_.find(key);
    if (it != connections_.end()) {
        return it->second;
    }

    return nullptr;
}

bool ServerConnectionManager::SendToServer(
    const ServerId& destination,
    ServerMessage::Ptr message) {

    auto connection = GetConnection(destination);
    if (!connection) {
        spdlog::warn("No connection to {}:{}",
                     ServerTypeToString(destination.type), destination.id);
        return false;
    }

    return connection->SendMessage(message);
}

void ServerConnectionManager::Broadcast(
    ServerMessage::Ptr message,
    const ServerId* exclude_server) {

    std::lock_guard<std::mutex> lock(connections_mutex_);

    for (auto& [key, connection] : connections_) {
        if (exclude_server &&
            connection->GetTargetServerId().type == exclude_server->type &&
            connection->GetTargetServerId().id == exclude_server->id) {
            continue;
        }

        connection->SendMessage(message);
    }

    spdlog::debug("Broadcast message to {} servers", connections_.size());
}

void ServerConnectionManager::BroadcastToType(
    ServerId::Type type,
    ServerMessage::Ptr message) {

    std::lock_guard<std::mutex> lock(connections_mutex_);

    for (auto& [key, connection] : connections_) {
        if (connection->GetTargetServerId().type == type) {
            connection->SendMessage(message);
        }
    }

    spdlog::debug("Broadcast message to {} servers of type {}",
                 connections_.size(), ServerTypeToString(type));
}

void ServerConnectionManager::DisconnectAll(bool graceful) {
    std::lock_guard<std::mutex> lock(connections_mutex_);

    for (auto& [key, connection] : connections_) {
        connection->Disconnect(graceful);
    }

    connections_.clear();

    spdlog::info("All connections disconnected (graceful: {})", graceful);
}

bool ServerConnectionManager::IsConnected(const ServerId& server_id) const {
    std::lock_guard<std::mutex> lock(connections_mutex_);

    uint32_t key = MakeConnectionKey(server_id);
    auto it = connections_.find(key);
    return it != connections_.end() && it->second->IsConnected();
}

std::vector<ServerConnection::Ptr> ServerConnectionManager::GetConnectionsByType(
    ServerId::Type type) {

    std::vector<ServerConnection::Ptr> result;

    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& [key, connection] : connections_) {
        if (connection->GetTargetServerId().type == type) {
            result.push_back(connection);
        }
    }

    return result;
}

uint32_t MakeConnectionKey(const ServerId& server_id) {
    return (static_cast<uint32_t>(server_id.type) << 16) | (server_id.id & 0xFFFF);
}

} // namespace Network
} // namespace Core
} // namespace Murim
