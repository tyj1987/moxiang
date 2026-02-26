#include "ServerMessage.hpp"
#include "spdlog_wrapper.hpp"
#include <chrono>
#include <cstring>

namespace Murim {
namespace Core {
namespace Network {

// ============== ServerMessage Implementation ==============

ServerMessage::ServerMessage(
    ServerMessageType type,
    const ServerId& source,
    const ServerId& destination,
    uint64_t sequence)
    : type_(type)
    , source_(source)
    , destination_(destination)
    , sequence_(sequence)
    , timestamp_(std::chrono::system_clock::now().time_since_epoch().count())
    , payload_(std::make_shared<ByteBuffer>(1024))
    , read_pos_(0) {
    spdlog::trace("ServerMessage created: type={}, source={}:{}, dest={}:{}",
                  static_cast<uint32_t>(type),
                  static_cast<uint32_t>(source.type), source.id,
                  static_cast<uint32_t>(destination.type), destination.id);
}

ServerMessage::ServerMessage(ByteBuffer::Ptr buffer)
    : type_(ServerMessageType::UNKNOWN)
    , source_()
    , destination_()
    , sequence_(0)
    , timestamp_(0)
    , payload_(std::make_shared<ByteBuffer>(1024))
    , read_pos_(0) {
    // Deserialize header from buffer
    if (!Deserialize()) {
        spdlog::error("Failed to deserialize ServerMessage");
    }
}

ServerMessage::~ServerMessage() {
    spdlog::trace("ServerMessage destroyed: type={}", static_cast<uint32_t>(type_));
}

ByteBuffer::Ptr ServerMessage::Serialize() const {
    auto buffer = std::make_shared<ByteBuffer>(4096);

    // Write message header
    buffer->WriteUInt32(static_cast<uint32_t>(type_));
    buffer->WriteUInt32(static_cast<uint32_t>(source_.type));
    buffer->WriteUInt32(source_.id);
    buffer->WriteUInt32(static_cast<uint32_t>(destination_.type));
    buffer->WriteUInt32(destination_.id);
    buffer->WriteUInt64(sequence_);
    buffer->WriteUInt64(timestamp_);

    // Write payload size
    buffer->WriteUInt32(static_cast<uint32_t>(payload_->GetSize()));

    // Write payload data
    if (payload_->GetSize() > 0) {
        buffer->Write(payload_->GetData(), payload_->GetSize());
    }

    return buffer;
}

bool ServerMessage::Deserialize() {
    // Header is already parsed before this is called
    // This method parses the payload portion
    return true;
}

// ============== Data Writers ==============

void ServerMessage::WriteUInt32(uint32_t value) {
    payload_->WriteUInt32(value);
}

void ServerMessage::WriteUInt64(uint64_t value) {
    payload_->WriteUInt64(value);
}

void ServerMessage::WriteString(const std::string& str) {
    payload_->WriteUInt32(static_cast<uint32_t>(str.size()));
    if (!str.empty()) {
        payload_->Write(reinterpret_cast<const uint8_t*>(str.c_str()), str.size());
    }
}

void ServerMessage::WriteFloat(float value) {
    static_assert(sizeof(float) == sizeof(uint32_t), "Float must be 32 bits");
    uint32_t int_value;
    std::memcpy(&int_value, &value, sizeof(float));
    payload_->WriteUInt32(int_value);
}

void ServerMessage::WriteDouble(double value) {
    static_assert(sizeof(double) == sizeof(uint64_t), "Double must be 64 bits");
    uint64_t int_value;
    std::memcpy(&int_value, &value, sizeof(double));
    payload_->WriteUInt64(int_value);
}

void ServerMessage::WriteBool(bool value) {
    payload_->WriteUInt32(value ? 1 : 0);
}

void ServerMessage::WriteBytes(const uint8_t* data, size_t size) {
    payload_->WriteUInt32(static_cast<uint32_t>(size));
    payload_->Write(data, size);
}

// ============== Data Readers ==============

uint32_t ServerMessage::ReadUInt32() {
    return payload_->ReadUInt32();
}

uint64_t ServerMessage::ReadUInt64() {
    return payload_->ReadUInt64();
}

std::string ServerMessage::ReadString() {
    uint32_t length = payload_->ReadUInt32();
    if (length == 0) {
        return "";
    }
    return payload_->ReadString(length);
}

float ServerMessage::ReadFloat() {
    uint32_t int_value = payload_->ReadUInt32();
    float value;
    std::memcpy(&value, &int_value, sizeof(float));
    return value;
}

double ServerMessage::ReadDouble() {
    uint64_t int_value = payload_->ReadUInt64();
    double value;
    std::memcpy(&value, &int_value, sizeof(double));
    return value;
}

bool ServerMessage::ReadBool() {
    return payload_->ReadUInt32() != 0;
}

std::vector<uint8_t> ServerMessage::ReadBytes(size_t size) {
    std::vector<uint8_t> data(size);
    if (size > 0) {
        payload_->Read(data.data(), size);
    }
    return data;
}

bool ServerMessage::HasMoreData() const {
    return payload_->GetReadableSize() > 0;
}

void ServerMessage::ResetReadPosition() {
    // ByteBuffer doesn't expose reset, so we'd need to add it
    // For now, recreate the buffer
    spdlog::warn("ResetReadPosition not fully implemented");
}

size_t ServerMessage::GetSize() const {
    // Header size: 4*5 + 8*2 = 36 bytes
    // Payload size field: 4 bytes
    // Payload: variable
    return 40 + payload_->GetSize();
}

// ============== ServerMessageBuilder Implementation ==============

ServerMessage::Ptr ServerMessageBuilder::CreatePlayerLogin(
    const ServerId& source,
    const ServerId& destination,
    uint64_t account_id,
    uint64_t character_id,
    const std::string& character_name) {

    auto msg = std::make_shared<ServerMessage>(
        ServerMessageType::PLAYER_LOGIN, source, destination);

    msg->WriteUInt64(account_id);
    msg->WriteUInt64(character_id);
    msg->WriteString(character_name);

    return msg;
}

ServerMessage::Ptr ServerMessageBuilder::CreatePlayerLogout(
    const ServerId& source,
    const ServerId& destination,
    uint64_t character_id,
    uint32_t reason) {

    auto msg = std::make_shared<ServerMessage>(
        ServerMessageType::PLAYER_LOGOUT, source, destination);

    msg->WriteUInt64(character_id);
    msg->WriteUInt32(reason);

    return msg;
}

ServerMessage::Ptr ServerMessageBuilder::CreatePlayerTeleport(
    const ServerId& source,
    const ServerId& destination,
    uint64_t character_id,
    const std::string& from_map,
    const std::string& to_map,
    float x, float y, float z) {

    auto msg = std::make_shared<ServerMessage>(
        ServerMessageType::PLAYER_TELEPORT, source, destination);

    msg->WriteUInt64(character_id);
    msg->WriteString(from_map);
    msg->WriteString(to_map);
    msg->WriteFloat(x);
    msg->WriteFloat(y);
    msg->WriteFloat(z);

    return msg;
}

ServerMessage::Ptr ServerMessageBuilder::CreateServerStatus(
    const ServerId& source,
    const ServerId& destination,
    uint32_t player_count,
    uint32_t max_players,
    float cpu_usage,
    float memory_usage) {

    auto msg = std::make_shared<ServerMessage>(
        ServerMessageType::SERVER_STATUS_UPDATE, source, destination);

    msg->WriteUInt32(player_count);
    msg->WriteUInt32(max_players);
    msg->WriteFloat(cpu_usage);
    msg->WriteFloat(memory_usage);

    return msg;
}

ServerMessage::Ptr ServerMessageBuilder::CreateGuildUpdate(
    const ServerId& source,
    const ServerId& destination,
    uint64_t guild_id,
    uint32_t update_type,
    const std::string& guild_name) {

    auto msg = std::make_shared<ServerMessage>(
        ServerMessageType::GUILD_UPDATE, source, destination);

    msg->WriteUInt64(guild_id);
    msg->WriteUInt32(update_type);
    msg->WriteString(guild_name);

    return msg;
}

ServerMessage::Ptr ServerMessageBuilder::CreatePartyUpdate(
    const ServerId& source,
    const ServerId& destination,
    uint64_t party_id,
    uint32_t update_type) {

    auto msg = std::make_shared<ServerMessage>(
        ServerMessageType::PARTY_UPDATE, source, destination);

    msg->WriteUInt64(party_id);
    msg->WriteUInt32(update_type);

    return msg;
}

ServerMessage::Ptr ServerMessageBuilder::CreateChatMessage(
    const ServerId& source,
    const ServerId& destination,
    uint64_t sender_id,
    const std::string& sender_name,
    uint32_t channel_type,
    const std::string& message) {

    auto msg = std::make_shared<ServerMessage>(
        ServerMessageType::CHAT_MESSAGE, source, destination);

    msg->WriteUInt64(sender_id);
    msg->WriteString(sender_name);
    msg->WriteUInt32(channel_type);
    msg->WriteString(message);

    return msg;
}

ServerMessage::Ptr ServerMessageBuilder::CreateHeartbeat(
    const ServerId& source,
    const ServerId& destination) {

    auto msg = std::make_shared<ServerMessage>(
        ServerMessageType::SERVER_HEARTBEAT, source, destination);

    // Heartbeat has no payload
    return msg;
}

ServerMessage::Ptr ServerMessageBuilder::CreateServerRegister(
    const ServerId& source,
    const ServerId& destination,
    const std::string& server_address,
    uint16_t server_port) {

    auto msg = std::make_shared<ServerMessage>(
        ServerMessageType::SERVER_REGISTER, source, destination);

    msg->WriteString(server_address);
    msg->WriteUInt32(server_port);

    return msg;
}

// ============== Parser Functions ==============

bool ServerMessageBuilder::ParsePlayerLogin(
    ServerMessage::Ptr msg,
    uint64_t& account_id,
    uint64_t& character_id,
    std::string& character_name) {

    if (!msg || msg->GetType() != ServerMessageType::PLAYER_LOGIN) {
        return false;
    }

    try {
        account_id = msg->ReadUInt64();
        character_id = msg->ReadUInt64();
        character_name = msg->ReadString();
        return true;
    } catch (...) {
        spdlog::error("Failed to parse PLAYER_LOGIN message");
        return false;
    }
}

bool ServerMessageBuilder::ParsePlayerLogout(
    ServerMessage::Ptr msg,
    uint64_t& character_id,
    uint32_t& reason) {

    if (!msg || msg->GetType() != ServerMessageType::PLAYER_LOGOUT) {
        return false;
    }

    try {
        character_id = msg->ReadUInt64();
        reason = msg->ReadUInt32();
        return true;
    } catch (...) {
        spdlog::error("Failed to parse PLAYER_LOGOUT message");
        return false;
    }
}

bool ServerMessageBuilder::ParsePlayerTeleport(
    ServerMessage::Ptr msg,
    uint64_t& character_id,
    std::string& from_map,
    std::string& to_map,
    float& x, float& y, float& z) {

    if (!msg || msg->GetType() != ServerMessageType::PLAYER_TELEPORT) {
        return false;
    }

    try {
        character_id = msg->ReadUInt64();
        from_map = msg->ReadString();
        to_map = msg->ReadString();
        x = msg->ReadFloat();
        y = msg->ReadFloat();
        z = msg->ReadFloat();
        return true;
    } catch (...) {
        spdlog::error("Failed to parse PLAYER_TELEPORT message");
        return false;
    }
}

bool ServerMessageBuilder::ParseServerStatus(
    ServerMessage::Ptr msg,
    uint32_t& player_count,
    uint32_t& max_players,
    float& cpu_usage,
    float& memory_usage) {

    if (!msg || msg->GetType() != ServerMessageType::SERVER_STATUS_UPDATE) {
        return false;
    }

    try {
        player_count = msg->ReadUInt32();
        max_players = msg->ReadUInt32();
        cpu_usage = msg->ReadFloat();
        memory_usage = msg->ReadFloat();
        return true;
    } catch (...) {
        spdlog::error("Failed to parse SERVER_STATUS_UPDATE message");
        return false;
    }
}

// ============== Utility Functions ==============

std::string ServerTypeToString(ServerId::Type type) {
    switch (type) {
        case ServerId::Type::AGENT:     return "AgentServer";
        case ServerId::Type::DISTRIBUTE: return "DistributeServer";
        case ServerId::Type::MAP:       return "MapServer";
        case ServerId::Type::MURIMNET:  return "MurimNetServer";
        default:                        return "Unknown";
    }
}

std::string MessageTypeToString(ServerMessageType type) {
    // Connection Management
    switch (type) {
        case ServerMessageType::SERVER_REGISTER:      return "SERVER_REGISTER";
        case ServerMessageType::SERVER_DEREGISTER:    return "SERVER_DEREGISTER";
        case ServerMessageType::SERVER_HEARTBEAT:     return "SERVER_HEARTBEAT";
        case ServerMessageType::SERVER_STATUS_UPDATE: return "SERVER_STATUS_UPDATE";
        case ServerMessageType::SERVER_SHUTDOWN:      return "SERVER_SHUTDOWN";

        // Player Management
        case ServerMessageType::PLAYER_LOGIN:         return "PLAYER_LOGIN";
        case ServerMessageType::PLAYER_LOGOUT:        return "PLAYER_LOGOUT";
        case ServerMessageType::PLAYER_DATA_REQUEST:  return "PLAYER_DATA_REQUEST";
        case ServerMessageType::PLAYER_DATA_RESPONSE: return "PLAYER_DATA_RESPONSE";
        case ServerMessageType::PLAYER_KICK:          return "PLAYER_KICK";
        case ServerMessageType::PLAYER_SAVE:          return "PLAYER_SAVE";

        // Movement & Teleportation
        case ServerMessageType::PLAYER_TELEPORT:      return "PLAYER_TELEPORT";
        case ServerMessageType::PLAYER_TELEPORT_ACK:  return "PLAYER_TELEPORT_ACK";
        case ServerMessageType::PLAYER_POSITION_SYNC: return "PLAYER_POSITION_SYNC";
        case ServerMessageType::PLAYER_ENTER_MAP:     return "PLAYER_ENTER_MAP";
        case ServerMessageType::PLAYER_LEAVE_MAP:     return "PLAYER_LEAVE_MAP";

        // Guild System
        case ServerMessageType::GUILD_CREATE:         return "GUILD_CREATE";
        case ServerMessageType::GUILD_DISBAND:        return "GUILD_DISBAND";
        case ServerMessageType::GUILD_JOIN:           return "GUILD_JOIN";
        case ServerMessageType::GUILD_LEAVE:          return "GUILD_LEAVE";
        case ServerMessageType::GUILD_UPDATE:         return "GUILD_UPDATE";
        case ServerMessageType::GUILD_MEMBER_UPDATE:  return "GUILD_MEMBER_UPDATE";
        case ServerMessageType::GUILD_CHAT:           return "GUILD_CHAT";
        case ServerMessageType::GUILD_NOTICE:         return "GUILD_NOTICE";
        case ServerMessageType::GUILD_WAR_START:      return "GUILD_WAR_START";
        case ServerMessageType::GUILD_WAR_END:        return "GUILD_WAR_END";

        // Party System
        case ServerMessageType::PARTY_CREATE:         return "PARTY_CREATE";
        case ServerMessageType::PARTY_DISBAND:        return "PARTY_DISBAND";
        case ServerMessageType::PARTY_JOIN:           return "PARTY_JOIN";
        case ServerMessageType::PARTY_LEAVE:          return "PARTY_LEAVE";
        case ServerMessageType::PARTY_UPDATE:         return "PARTY_UPDATE";
        case ServerMessageType::PARTY_LEADER_CHANGE:  return "PARTY_LEADER_CHANGE";
        case ServerMessageType::PARTY_CHAT:           return "PARTY_CHAT";
        case ServerMessageType::PARTY_XP_SYNC:        return "PARTY_XP_SYNC";

        // Chat System
        case ServerMessageType::CHAT_MESSAGE:         return "CHAT_MESSAGE";
        case ServerMessageType::CHAT_WHISPER:         return "CHAT_WHISPER";
        case ServerMessageType::CHAT_ANNOUNCEMENT:    return "CHAT_ANNOUNCEMENT";
        case ServerMessageType::CHAT_BROADCAST:       return "CHAT_BROADCAST";

        // Database Operations
        case ServerMessageType::DB_QUERY_REQUEST:     return "DB_QUERY_REQUEST";
        case ServerMessageType::DB_QUERY_RESPONSE:    return "DB_QUERY_RESPONSE";
        case ServerMessageType::DB_SAVE_REQUEST:      return "DB_SAVE_REQUEST";
        case ServerMessageType::DB_SAVE_RESPONSE:     return "DB_SAVE_RESPONSE";

        // Synchronization
        case ServerMessageType::SYNC_TIME_REQUEST:    return "SYNC_TIME_REQUEST";
        case ServerMessageType::SYNC_TIME_RESPONSE:   return "SYNC_TIME_RESPONSE";
        case ServerMessageType::SYNC_CONFIG_UPDATE:   return "SYNC_CONFIG_UPDATE";
        case ServerMessageType::SYNC_EVENT_BROADCAST: return "SYNC_EVENT_BROADCAST";

        // Load Balancing
        case ServerMessageType::LOAD_REPORT:          return "LOAD_REPORT";
        case ServerMessageType::LOAD_BALANCE_REQUEST: return "LOAD_BALANCE_REQUEST";
        case ServerMessageType::LOAD_BALANCE_RESPONSE:return "LOAD_BALANCE_RESPONSE";

        default:                                      return "UNKNOWN";
    }
}

} // namespace Network
} // namespace Core
} // namespace Murim
