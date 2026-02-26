#include "ConnectionManager.hpp"
#include "spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Network {

void ConnectionManager::AddConnection(Connection::Ptr connection) {
    if (!connection) {
        spdlog::warn("Attempted to add null connection");
        return;
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);
    uint64_t conn_id = connection->GetConnectionId();
    connections_[conn_id] = connection;

    spdlog::info("Connection added: {} (total: {})", conn_id, connections_.size());
}

void ConnectionManager::RemoveConnection(uint64_t connection_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        spdlog::info("Connection removed: {} (total: {})", connection_id, connections_.size() - 1);
        connections_.erase(it);
    } else {
        spdlog::warn("Attempted to remove non-existent connection: {}", connection_id);
    }
}

Connection::Ptr ConnectionManager::GetConnection(uint64_t connection_id) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        return it->second;
    }

    return nullptr;
}

std::vector<Connection::Ptr> ConnectionManager::GetAllConnections() {
    std::vector<Connection::Ptr> connections;
    std::shared_lock<std::shared_mutex> lock(mutex_);

    connections.reserve(connections_.size());
    for (const auto& pair : connections_) {
        connections.push_back(pair.second);
    }

    return connections;
}

size_t ConnectionManager::GetConnectionCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return connections_.size();
}

void ConnectionManager::BroadcastMessage(std::shared_ptr<ByteBuffer> buffer, uint64_t exclude_connection_id) {
    if (!buffer) {
        spdlog::warn("Attempted to broadcast null buffer");
        return;
    }

    std::shared_lock<std::shared_mutex> lock(mutex_);

    for (const auto& pair : connections_) {
        uint64_t conn_id = pair.first;
        Connection::Ptr connection = pair.second;

        // 跳过排除的连接
        if (exclude_connection_id != 0 && conn_id == exclude_connection_id) {
            continue;
        }

        // 检查连接是否打开
        if (connection && connection->IsOpen()) {
            connection->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t bytes_sent) {
                if (ec) {
                    spdlog::error("Failed to send broadcast message: {}", ec.message());
                }
            });
        }
    }

    spdlog::debug("Broadcast message to {} connections", connections_.size());
}

void ConnectionManager::CleanupClosedConnections() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    size_t initial_count = connections_.size();
    auto it = connections_.begin();
    while (it != connections_.end()) {
        if (!it->second || !it->second->IsOpen()) {
            spdlog::info("Cleaning up closed connection: {}", it->first);
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }

    size_t cleaned_count = initial_count - connections_.size();
    if (cleaned_count > 0) {
        spdlog::info("Cleaned up {} closed connections (remaining: {})", cleaned_count, connections_.size());
    }
}

bool ConnectionManager::Initialize() {
    spdlog::info("ConnectionManager initialized");
    return true;
}

bool ConnectionManager::Start() {
    spdlog::info("ConnectionManager started");
    return true;
}

void ConnectionManager::Stop() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // 关闭所有连接
    for (auto& pair : connections_) {
        if (pair.second) {
            pair.second->Close();
        }
    }

    connections_.clear();
    spdlog::info("ConnectionManager stopped");
}

void ConnectionManager::Update(uint32_t delta_time) {
    // 定期清理关闭的连接 (每 30 秒)
    static uint32_t cleanup_timer = 0;
    cleanup_timer += delta_time;

    if (cleanup_timer >= 30000) {
        cleanup_timer = 0;
        CleanupClosedConnections();
    }
}

} // namespace Network
} // namespace Core
} // namespace Murim
