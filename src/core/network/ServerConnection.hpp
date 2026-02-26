#pragma once

#include <memory>
#include <string>
#include <functional>
#include <queue>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include "Connection.hpp"
#include "ServerMessage.hpp"
#include "IOContext.hpp"

namespace Murim {
namespace Core {
namespace Network {

/**
 * @brief Connection state
 */
enum class ServerConnectionState : uint32_t {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    AUTHENTICATED = 3,
    RECONNECTING = 4,
    STATE_ERROR = 5
};

/**
 * @brief Server connection statistics
 */
struct ServerConnectionStats {
    uint64_t messages_sent = 0;
    uint64_t messages_received = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t reconnect_count = 0;
    std::chrono::system_clock::time_point last_message_time;
    std::chrono::system_clock::time_point connect_time;
    uint32_t ping_ms = 0;

    void Reset() {
        messages_sent = 0;
        messages_received = 0;
        bytes_sent = 0;
        bytes_received = 0;
        reconnect_count = 0;
        ping_ms = 0;
    }
};

/**
 * @brief Message handler callback type
 */
using ServerMessageHandler = std::function<void(ServerMessage::Ptr)>;

/**
 * @brief Server connection class
 *
 * Manages TCP connections between servers with:
 * - Automatic reconnection with exponential backoff
 * - Heartbeat/ping for connection health monitoring
 * - Message queue for reliable delivery
 * - Thread-safe message sending
 * - Connection state management
 */
class ServerConnection : public std::enable_shared_from_this<ServerConnection> {
public:
    using Ptr = std::shared_ptr<ServerConnection>;
    using MessageQueue = std::queue<ServerMessage::Ptr>;

    /**
     * @brief Constructor for outgoing connection
     * @param io_context I/O context
     * @param server_id Local server ID
     * @param target_server_id Remote server ID
     * @param host Remote host address
     * @param port Remote port
     */
    ServerConnection(
        IOContext& io_context,
        const ServerId& server_id,
        const ServerId& target_server_id,
        const std::string& host,
        uint16_t port);

    /**
     * @brief Constructor for incoming connection
     * @param connection Accepted connection
     * @param server_id Local server ID
     * @param target_server_id Remote server ID (known after registration)
     */
    ServerConnection(
        Connection::Ptr connection,
        const ServerId& server_id,
        const ServerId& target_server_id);

    /**
     * @brief Destructor
     */
    ~ServerConnection();

    /**
     * @brief Connect to remote server
     * @return true if connection initiated successfully
     */
    bool Connect();

    /**
     * @brief Disconnect from remote server
     * @param graceful If true, send shutdown notification
     */
    void Disconnect(bool graceful = true);

    /**
     * @brief Send message to remote server
     * @param message Message to send
     * @return true if message queued successfully
     */
    bool SendMessage(ServerMessage::Ptr message);

    /**
     * @brief Send message and wait for response
     * @param request Request message
     * @param timeout_ms Timeout in milliseconds
     * @return Response message or nullptr on timeout/error
     */
    ServerMessage::Ptr SendRequest(
        ServerMessage::Ptr request,
        uint32_t timeout_ms = 5000);

    /**
     * @brief Register message handler
     * @param handler Message handler function
     */
    void SetMessageHandler(ServerMessageHandler handler) {
        message_handler_ = handler;
    }

    /**
     * @brief Get connection state
     */
    ServerConnectionState GetState() const {
        return state_.load();
    }

    /**
     * @brief Get local server ID
     */
    const ServerId& GetServerId() const { return server_id_; }

    /**
     * @brief Get remote server ID
     */
    const ServerId& GetTargetServerId() const { return target_server_id_; }

    /**
     * @brief Get remote address
     */
    std::string GetRemoteAddress() const;

    /**
     * @brief Check if connected
     */
    bool IsConnected() const {
        auto s = state_.load();
        return s == ServerConnectionState::CONNECTED ||
               s == ServerConnectionState::AUTHENTICATED;
    }

    /**
     * @brief Check if authenticated
     */
    bool IsAuthenticated() const {
        return state_.load() == ServerConnectionState::AUTHENTICATED;
    }

    /**
     * @brief Get connection statistics
     */
    const ServerConnectionStats& GetStats() const { return stats_; }

    /**
     * @brief Reset statistics
     */
    void ResetStats() { stats_.Reset(); }

    /**
     * @brief Get current ping in milliseconds
     */
    uint32_t GetPing() const { return stats_.ping_ms; }

    /**
     * @brief Set heartbeat interval
     * @param interval_ms Heartbeat interval in milliseconds
     */
    void SetHeartbeatInterval(uint32_t interval_ms) {
        heartbeat_interval_ = std::chrono::milliseconds(interval_ms);
    }

    /**
     * @brief Set reconnection settings
     * @param enabled Enable auto-reconnect
     * @param max_attempts Maximum reconnect attempts (0 = infinite)
     * @param initial_delay_ms Initial delay in milliseconds
     * @param max_delay_ms Maximum delay in milliseconds
     */
    void SetReconnectConfig(
        bool enabled,
        uint32_t max_attempts = 0,
        uint32_t initial_delay_ms = 1000,
        uint32_t max_delay_ms = 30000);

    /**
     * @brief Update connection state
     * @param new_state New connection state
     */
    void SetState(ServerConnectionState new_state);

    /**
     * @brief Handle received data
     * @param buffer Received data buffer
     */
    void HandleReceivedData(ByteBuffer::Ptr buffer);

    /**
     * @brief Start heartbeat timer
     */
    void StartHeartbeat();

    /**
     * @brief Stop heartbeat timer
     */
    void StopHeartbeat();

private:
    /**
     * @brief Parse message from buffer
     * @param buffer Data buffer
     * @return Parsed message or nullptr if incomplete
     */
    ServerMessage::Ptr ParseMessage(ByteBuffer::Ptr buffer);

    /**
     * @brief Process message queue
     */
    void ProcessMessageQueue();

    /**
     * @brief Send next message in queue
     */
    void SendNextMessage();

    /**
     * @brief Handle send completion
     * @param ec Error code
     * @param bytes_sent Bytes sent
     */
    void HandleSend(const boost::system::error_code& ec, size_t bytes_sent);

    /**
     * @brief Attempt to reconnect
     */
    void Reconnect();

    /**
     * @brief Start reconnect timer
     * @param delay_ms Delay before reconnect attempt
     */
    void ScheduleReconnect(uint32_t delay_ms);

    /**
     * @brief Send heartbeat
     */
    void SendHeartbeat();

    /**
     * @brief Update statistics
     * @param bytes_sent Bytes sent
     * @param bytes_received Bytes received
     */
    void UpdateStats(size_t bytes_sent = 0, size_t bytes_received = 0);

    /**
     * @brief Get next sequence number
     */
    uint64_t NextSequence() { return ++sequence_counter_; }

private:
    IOContext& io_context_;
    Connection::Ptr connection_;          // TCP connection

    // Server identification
    ServerId server_id_;                  // Local server ID
    ServerId target_server_id_;           // Remote server ID
    std::string host_;                   // Remote host (for outgoing)
    uint16_t port_;                      // Remote port (for outgoing)

    // Connection state
    std::atomic<ServerConnectionState> state_;
    std::atomic<bool> reconnect_enabled_;
    std::atomic<uint32_t> reconnect_attempts_;
    std::atomic<uint32_t> max_reconnect_attempts_;
    std::atomic<uint64_t> sequence_counter_;

    // Timers
    std::chrono::milliseconds heartbeat_interval_;
    std::chrono::milliseconds reconnect_initial_delay_;
    std::chrono::milliseconds reconnect_max_delay_;
    std::unique_ptr<boost::asio::steady_timer> heartbeat_timer_;
    std::unique_ptr<boost::asio::steady_timer> reconnect_timer_;

    // Message handling
    ServerMessageHandler message_handler_;
    MessageQueue send_queue_;
    std::mutex queue_mutex_;
    std::mutex stats_mutex_;

    // Receive buffer
    std::vector<uint8_t> recv_buffer_;
    size_t recv_pos_;
    size_t expected_size_;

    // Statistics
    ServerConnectionStats stats_;

    // Pending requests (for request/response pattern)
    struct PendingRequest {
        ServerMessage::Ptr request;
        std::function<void(ServerMessage::Ptr)> callback;
        std::chrono::steady_clock::time_point timeout;
    };
    std::unordered_map<uint64_t, PendingRequest> pending_requests_;
    std::mutex requests_mutex_;
    std::unique_ptr<boost::asio::steady_timer> request_timeout_timer_;
};

/**
 * @brief Server connection manager
 *
 * Manages multiple server connections for routing messages
 */
class ServerConnectionManager {
public:
    using ConnectionMap = std::unordered_map<uint32_t, ServerConnection::Ptr>;
    using ConnectionCallback = std::function<void(ServerConnection::Ptr)>;

    /**
     * @brief Constructor
     * @param io_context I/O context
     * @param server_id Local server ID
     */
    ServerConnectionManager(IOContext& io_context, const ServerId& server_id);

    /**
     * @brief Destructor
     */
    ~ServerConnectionManager();

    /**
     * @brief Add connection
     * @param connection Connection to add
     */
    void AddConnection(ServerConnection::Ptr connection);

    /**
     * @brief Remove connection
     * @param server_id Server ID to remove
     */
    void RemoveConnection(const ServerId& server_id);

    /**
     * @brief Get connection by server ID
     * @param server_id Server ID
     * @return Connection or nullptr if not found
     */
    ServerConnection::Ptr GetConnection(const ServerId& server_id);

    /**
     * @brief Get all connections
     */
    const ConnectionMap& GetAllConnections() const { return connections_; }

    /**
     * @brief Send message to specific server
     * @param destination Destination server ID
     * @param message Message to send
     * @return true if message sent successfully
     */
    bool SendToServer(const ServerId& destination, ServerMessage::Ptr message);

    /**
     * @brief Broadcast message to all connected servers
     * @param message Message to broadcast
     * @param exclude_server Server to exclude from broadcast
     */
    void Broadcast(ServerMessage::Ptr message, const ServerId* exclude_server = nullptr);

    /**
     * @brief Broadcast to specific server type
     * @param type Server type
     * @param message Message to broadcast
     */
    void BroadcastToType(ServerId::Type type, ServerMessage::Ptr message);

    /**
     * @brief Disconnect all connections
     * @param graceful If true, send shutdown notification
     */
    void DisconnectAll(bool graceful = true);

    /**
     * @brief Set connection callback
     * @param callback Callback function for connection events
     */
    void SetConnectionCallback(ConnectionCallback callback) {
        connection_callback_ = callback;
    }

    /**
     * @brief Get connection count
     */
    size_t GetConnectionCount() const { return connections_.size(); }

    /**
     * @brief Check if connected to server
     */
    bool IsConnected(const ServerId& server_id) const;

    /**
     * @brief Get all connected servers of specific type
     */
    std::vector<ServerConnection::Ptr> GetConnectionsByType(ServerId::Type type);

private:
    IOContext& io_context_;
    ServerId server_id_;
    ConnectionMap connections_;
    mutable std::mutex connections_mutex_;  // mutable for const functions
    ConnectionCallback connection_callback_;
};

/**
 * @brief Create connection key for storage
 */
uint32_t MakeConnectionKey(const ServerId& server_id);

} // namespace Network
} // namespace Core
} // namespace Murim
