#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include "ByteBuffer.hpp"

namespace Murim {
namespace Core {
namespace Network {

/**
 * @brief Server message types for cross-server communication
 *
 * These messages handle inter-server communication for:
 * - Player login/logout coordination
 * - Cross-map teleportation
 * - Guild synchronization
 * - Party coordination across maps
 * - Server load balancing
 * - Cross-map chat
 */
enum class ServerMessageType : uint32_t {
    // Connection Management (0x1000-0x10FF)
    SERVER_REGISTER = 0x1000,         // Server registration with DistributeServer
    SERVER_DEREGISTER = 0x1001,       // Server deregistration
    SERVER_HEARTBEAT = 0x1002,        // Heartbeat/ping
    SERVER_STATUS_UPDATE = 0x1003,    // Load/status update
    SERVER_SHUTDOWN = 0x1004,         // Graceful shutdown notification

    // Player Management (0x1100-0x11FF)
    PLAYER_LOGIN = 0x1100,            // Agent -> Map: Player entering game
    PLAYER_LOGOUT = 0x1101,           // Map -> Agent: Player leaving
    PLAYER_DATA_REQUEST = 0x1102,     // Request player data
    PLAYER_DATA_RESPONSE = 0x1103,    // Response with player data
    PLAYER_KICK = 0x1104,             // Kick player from server
    PLAYER_SAVE = 0x1105,             // Save player data to DB

    // Movement & Teleportation (0x1200-0x12FF)
    PLAYER_TELEPORT = 0x1200,         // Map -> Map: Player switching maps
    PLAYER_TELEPORT_ACK = 0x1201,     // Teleport acknowledgment
    PLAYER_POSITION_SYNC = 0x1202,    // Position synchronization
    PLAYER_ENTER_MAP = 0x1203,        // Player entered map notification
    PLAYER_LEAVE_MAP = 0x1204,        // Player left map notification

    // Guild System (0x1300-0x13FF)
    GUILD_CREATE = 0x1300,            // Create guild
    GUILD_DISBAND = 0x1301,           // Disband guild
    GUILD_JOIN = 0x1302,              // Player joins guild
    GUILD_LEAVE = 0x1303,             // Player leaves guild
    GUILD_UPDATE = 0x1304,            // Guild info update
    GUILD_MEMBER_UPDATE = 0x1305,     // Guild member update
    GUILD_CHAT = 0x1306,              // Guild chat message
    GUILD_NOTICE = 0x1307,            // Guild notice change
    GUILD_WAR_START = 0x1308,         // Guild war started
    GUILD_WAR_END = 0x1309,           // Guild war ended

    // Party System (0x1400-0x14FF)
    PARTY_CREATE = 0x1400,            // Create party
    PARTY_DISBAND = 0x1401,           // Disband party
    PARTY_JOIN = 0x1402,              // Player joins party
    PARTY_LEAVE = 0x1403,             // Player leaves party
    PARTY_UPDATE = 0x1404,            // Party info update
    PARTY_LEADER_CHANGE = 0x1405,     // Party leader change
    PARTY_CHAT = 0x1406,              // Party chat message
    PARTY_XP_SYNC = 0x1407,           // Cross-map XP distribution

    // Chat System (0x1500-0x15FF)
    CHAT_MESSAGE = 0x1500,            // Cross-map chat
    CHAT_WHISPER = 0x1501,            // Whisper across maps
    CHAT_ANNOUNCEMENT = 0x1502,       // Server announcement
    CHAT_BROADCAST = 0x1503,          // Global broadcast

    // Database Operations (0x1600-0x16FF)
    DB_QUERY_REQUEST = 0x1600,        // Database query request
    DB_QUERY_RESPONSE = 0x1601,       // Database query response
    DB_SAVE_REQUEST = 0x1602,         // Database save request
    DB_SAVE_RESPONSE = 0x1603,        // Database save response

    // Synchronization (0x1700-0x17FF)
    SYNC_TIME_REQUEST = 0x1700,       // Time synchronization request
    SYNC_TIME_RESPONSE = 0x1701,      // Time synchronization response
    SYNC_CONFIG_UPDATE = 0x1702,      // Configuration update broadcast
    SYNC_EVENT_BROADCAST = 0x1703,    // Game event broadcast

    // Load Balancing (0x1800-0x18FF)
    LOAD_REPORT = 0x1800,             // Server load report
    LOAD_BALANCE_REQUEST = 0x1801,    // Request player transfer
    LOAD_BALANCE_RESPONSE = 0x1802,   // Response to transfer request

    // Unknown/Error
    UNKNOWN = 0x0000,
    MSG_ERROR = 0xFFFF
};

/**
 * @brief Server identification
 */
struct ServerId {
    enum class Type : uint32_t {
        AGENT = 1,
        DISTRIBUTE = 2,
        MAP = 3,
        MURIMNET = 4
    };

    Type type;
    uint32_t id;                      // Server instance ID

    ServerId() : type(Type::MAP), id(0) {}
    ServerId(Type t, uint32_t i) : type(t), id(i) {}

    bool IsValid() const { return id > 0; }
};

/**
 * @brief Server-to-server message wrapper
 *
 * This class wraps all cross-server communication with routing info,
 * serialization, and message type handling.
 */
class ServerMessage {
public:
    using Ptr = std::shared_ptr<ServerMessage>;

    /**
     * @brief Constructor for sending messages
     * @param type Message type
     * @param source Source server
     * @param destination Destination server (0 = broadcast)
     * @param sequence Sequence number for request/response matching
     */
    ServerMessage(
        ServerMessageType type,
        const ServerId& source,
        const ServerId& destination,
        uint64_t sequence = 0);

    /**
     * @brief Constructor for receiving messages
     * @param buffer Received byte buffer
     */
    explicit ServerMessage(ByteBuffer::Ptr buffer);

    /**
     * @brief Destructor
     */
    ~ServerMessage();

    /**
     * @brief Get message type
     */
    ServerMessageType GetType() const { return type_; }

    /**
     * @brief Get source server
     */
    const ServerId& GetSource() const { return source_; }

    /**
     * @brief Get destination server
     */
    const ServerId& GetDestination() const { return destination_; }

    /**
     * @brief Get sequence number
     */
    uint64_t GetSequence() const { return sequence_; }

    /**
     * @brief Get timestamp
     */
    uint64_t GetTimestamp() const { return timestamp_; }

    /**
     * @brief Set sequence number (for ServerConnection)
     */
    void SetSequence(uint64_t sequence) { sequence_ = sequence; }

    /**
     * @brief Set timestamp (for ServerConnection)
     */
    void SetTimestamp(uint64_t timestamp) { timestamp_ = timestamp; }

    /**
     * @brief Serialize message to byte buffer
     */
    ByteBuffer::Ptr Serialize() const;

    /**
     * @brief Deserialize message payload
     */
    bool Deserialize();

    // ============== Data Writers ==============

    void WriteUInt32(uint32_t value);
    void WriteUInt64(uint64_t value);
    void WriteString(const std::string& str);
    void WriteFloat(float value);
    void WriteDouble(double value);
    void WriteBool(bool value);
    void WriteBytes(const uint8_t* data, size_t size);

    // ============== Data Readers ==============

    uint32_t ReadUInt32();
    uint64_t ReadUInt64();
    std::string ReadString();
    float ReadFloat();
    double ReadDouble();
    bool ReadBool();
    std::vector<uint8_t> ReadBytes(size_t size);

    /**
     * @brief Get payload buffer for reading
     */
    ByteBuffer::Ptr GetPayload() const { return payload_; }

    /**
     * @brief Check if message has more data to read
     */
    bool HasMoreData() const;

    /**
     * @brief Reset read position to beginning of payload
     */
    void ResetReadPosition();

    /**
     * @brief Get message size
     */
    size_t GetSize() const;

private:
    ServerMessageType type_;
    ServerId source_;
    ServerId destination_;
    uint64_t sequence_;               // For request/response matching
    uint64_t timestamp_;              // Message creation time
    ByteBuffer::Ptr payload_;         // Message payload

    // Deserialization state
    std::vector<uint8_t> raw_data_;
    size_t read_pos_;
};

/**
 * @brief Message builder helper for creating common messages
 */
class ServerMessageBuilder {
public:
    /**
     * @brief Create player login message
     */
    static ServerMessage::Ptr CreatePlayerLogin(
        const ServerId& source,
        const ServerId& destination,
        uint64_t account_id,
        uint64_t character_id,
        const std::string& character_name);

    /**
     * @brief Create player logout message
     */
    static ServerMessage::Ptr CreatePlayerLogout(
        const ServerId& source,
        const ServerId& destination,
        uint64_t character_id,
        uint32_t reason);

    /**
     * @brief Create player teleport message
     */
    static ServerMessage::Ptr CreatePlayerTeleport(
        const ServerId& source,
        const ServerId& destination,
        uint64_t character_id,
        const std::string& from_map,
        const std::string& to_map,
        float x, float y, float z);

    /**
     * @brief Create server status update message
     */
    static ServerMessage::Ptr CreateServerStatus(
        const ServerId& source,
        const ServerId& destination,
        uint32_t player_count,
        uint32_t max_players,
        float cpu_usage,
        float memory_usage);

    /**
     * @brief Create guild update message
     */
    static ServerMessage::Ptr CreateGuildUpdate(
        const ServerId& source,
        const ServerId& destination,
        uint64_t guild_id,
        uint32_t update_type,
        const std::string& guild_name);

    /**
     * @brief Create party update message
     */
    static ServerMessage::Ptr CreatePartyUpdate(
        const ServerId& source,
        const ServerId& destination,
        uint64_t party_id,
        uint32_t update_type);

    /**
     * @brief Create chat message
     */
    static ServerMessage::Ptr CreateChatMessage(
        const ServerId& source,
        const ServerId& destination,
        uint64_t sender_id,
        const std::string& sender_name,
        uint32_t channel_type,
        const std::string& message);

    /**
     * @brief Create heartbeat message
     */
    static ServerMessage::Ptr CreateHeartbeat(
        const ServerId& source,
        const ServerId& destination);

    /**
     * @brief Create server registration message
     */
    static ServerMessage::Ptr CreateServerRegister(
        const ServerId& source,
        const ServerId& destination,
        const std::string& server_address,
        uint16_t server_port);

    /**
     * @brief Parse player login from message
     */
    static bool ParsePlayerLogin(
        ServerMessage::Ptr msg,
        uint64_t& account_id,
        uint64_t& character_id,
        std::string& character_name);

    /**
     * @brief Parse player logout from message
     */
    static bool ParsePlayerLogout(
        ServerMessage::Ptr msg,
        uint64_t& character_id,
        uint32_t& reason);

    /**
     * @brief Parse player teleport from message
     */
    static bool ParsePlayerTeleport(
        ServerMessage::Ptr msg,
        uint64_t& character_id,
        std::string& from_map,
        std::string& to_map,
        float& x, float& y, float& z);

    /**
     * @brief Parse server status from message
     */
    static bool ParseServerStatus(
        ServerMessage::Ptr msg,
        uint32_t& player_count,
        uint32_t& max_players,
        float& cpu_usage,
        float& memory_usage);
};

/**
 * @brief Convert server type to string
 */
std::string ServerTypeToString(ServerId::Type type);

/**
 * @brief Convert message type to string
 */
std::string MessageTypeToString(ServerMessageType type);

} // namespace Network
} // namespace Core
} // namespace Murim
