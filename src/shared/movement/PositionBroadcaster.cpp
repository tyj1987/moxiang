#include "PositionBroadcaster.hpp"
#include "core/spdlog_wrapper.hpp"
#include <cmath>
#include <algorithm>

namespace Murim {
namespace Shared {
namespace Movement {

// ==================== AOIManager 实现 ====================

AOIManager::AOIManager(float grid_size)
    : grid_size_(grid_size) {

    if (grid_size_ <= 0.0f) {
        grid_size_ = 50.0f;  // 默认 50 米
        spdlog::warn("[AOIManager] Invalid grid_size {}, using default 50.0m", grid_size);
    }

    spdlog::info("[AOIManager] Initialized with grid size: {}m", grid_size_);
}

AOIManager::GridCoord AOIManager::WorldToGrid(float x, float y) const {
    GridCoord coord;
    coord.x = static_cast<int>(std::floor(x / grid_size_));
    coord.y = static_cast<int>(std::floor(y / grid_size_));
    return coord;
}

AOIManager::GridCell* AOIManager::GetGrid(const GridCoord& coord) {
    auto it = grids_.find(coord);
    if (it != grids_.end()) {
        return &it->second;
    }
    return nullptr;
}

void AOIManager::UpdatePosition(uint64_t character_id, float x, float y) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // 计算新网格坐标
    GridCoord new_grid = WorldToGrid(x, y);

    // 查找玩家当前所在的网格
    auto it = character_grids_.find(character_id);
    if (it != character_grids_.end()) {
        GridCoord old_grid = it->second;

        // 如果网格没有变化，直接返回
        if (old_grid == new_grid) {
            return;
        }

        // 从旧网格移除
        auto old_cell = GetGrid(old_grid);
        if (old_cell) {
            old_cell->RemoveCharacter(character_id);
            if (old_cell->IsEmpty()) {
                grids_.erase(old_grid);
            }
        }
    }

    // 添加到新网格
    auto& new_cell = grids_[new_grid];
    new_cell.AddCharacter(character_id);
    character_grids_[character_id] = new_grid;
}

void AOIManager::RemoveCharacter(uint64_t character_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = character_grids_.find(character_id);
    if (it == character_grids_.end()) {
        return;  // 玩家不存在
    }

    GridCoord grid = it->second;
    auto cell = GetGrid(grid);
    if (cell) {
        cell->RemoveCharacter(character_id);
        if (cell->IsEmpty()) {
            grids_.erase(grid);
        }
    }

    character_grids_.erase(it);
}

std::vector<AOIManager::GridCoord> AOIManager::GetNearbyGrids(
    float x, float y, float aoi_radius) const {

    std::shared_lock<std::shared_mutex> lock(mutex_);

    // 计算中心网格
    GridCoord center = WorldToGrid(x, y);

    // 计算需要检查的网格半径（格子数）
    int grid_radius = static_cast<int>(std::ceil(aoi_radius / grid_size_));

    // 收集周围 9 格（或更多，取决于 radius）
    std::vector<GridCoord> nearby_grids;
    for (int dx = -grid_radius; dx <= grid_radius; ++dx) {
        for (int dy = -grid_radius; dy <= grid_radius; ++dy) {
            GridCoord coord(center.x + dx, center.y + dy);
            nearby_grids.push_back(coord);
        }
    }

    return nearby_grids;
}

std::vector<uint64_t> AOIManager::GetAOICharacters(
    float x, float y, float aoi_radius) const {

    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<uint64_t> aoi_characters;
    GridCoord center = WorldToGrid(x, y);

    // 计算需要检查的网格半径
    int grid_radius = static_cast<int>(std::ceil(aoi_radius / grid_size_));

    // 遍历周围网格
    for (int dx = -grid_radius; dx <= grid_radius; ++dx) {
        for (int dy = -grid_radius; dy <= grid_radius; ++dy) {
            GridCoord coord(center.x + dx, center.y + dy);
            auto it = grids_.find(coord);
            if (it != grids_.end()) {
                const auto& cell = it->second;
                for (uint64_t char_id : cell.character_ids) {
                    aoi_characters.push_back(char_id);
                }
            }
        }
    }

    return aoi_characters;
}

void AOIManager::Clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    grids_.clear();
    character_grids_.clear();
}

// ==================== PositionBroadcaster 实现 ====================

PositionBroadcaster::PositionBroadcaster(
    const PositionBroadcastConfig& config,
    SendCallback send_callback)
    : config_(config),
      aoi_manager_(config.grid_size),
      send_callback_(send_callback) {

    if (!send_callback_) {
        spdlog::error("[PositionBroadcaster] Send callback is null!");
        throw std::invalid_argument("send_callback cannot be null");
    }

    stats_ = Statistics{0, 0, 0, 0};

    spdlog::info("[PositionBroadcaster] Initialized with config:");
    spdlog::info("  - AOI radius: {}m", config_.aoi_radius);
    spdlog::info("  - Grid size: {}m", config_.grid_size);
    spdlog::info("  - Position threshold: {}m", config_.position_threshold);
    spdlog::info("  - Batch size: {}", config_.batch_size);
    spdlog::info("  - Broadcast interval: {}ms", config_.broadcast_interval);
}

PositionBroadcaster::~PositionBroadcaster() {
    spdlog::info("[PositionBroadcaster] Destroyed. Statistics:");
    spdlog::info("  - Total updates: {}", stats_.total_updates);
    spdlog::info("  - Broadcast count: {}", stats_.broadcast_count);
    spdlog::info("  - Skipped updates: {}", stats_.skipped_updates);
}

bool PositionBroadcaster::UpdatePosition(uint64_t character_id,
                                       float x, float y, float z,
                                       uint16_t direction,
                                       uint8_t movement_type) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // 检查位置变化是否超过阈值
    auto it = character_positions_.find(character_id);
    if (it != character_positions_.end()) {
        const auto& pos = it->second;

        // 计算距离平方
        float dx = x - pos.x;
        float dy = y - pos.y;
        float dz = z - pos.z;
        float dist_sq = dx * dx + dy * dy + dz * dz;

        // 如果距离小于阈值，跳过此次更新
        if (dist_sq < config_.position_threshold * config_.position_threshold) {
            ++stats_.skipped_updates;
            return false;
        }
    }

    // 更新 AOI（只更新 x, y）
    aoi_manager_.UpdatePosition(character_id, x, y);

    // 更新位置缓存
    auto& pos = character_positions_[character_id];
    pos.x = x;
    pos.y = y;
    pos.z = z;
    pos.direction = direction;
    pos.movement_type = movement_type;
    pos.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // 添加到待发送队列
    pending_updates_[character_id] = PositionUpdate(
        character_id, x, y, z, direction, movement_type, pos.timestamp);

    ++stats_.total_updates;
    return true;
}

void PositionBroadcaster::RemoveCharacter(uint64_t character_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    aoi_manager_.RemoveCharacter(character_id);
    character_positions_.erase(character_id);
    pending_updates_.erase(character_id);
}

void PositionBroadcaster::Broadcast() {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    if (pending_updates_.empty()) {
        return;
    }

    // 批量处理
    std::vector<PositionUpdate> batch;
    batch.reserve(config_.batch_size);

    for (const auto& entry : pending_updates_) {
        batch.push_back(entry.second);

        if (batch.size() >= config_.batch_size) {
            // 发送这一批
            BroadcastBatch(batch, {});

            // 更新统计
            stats_.broadcast_count += batch.size();

            batch.clear();
        }
    }

    // 发送剩余的
    if (!batch.empty()) {
        BroadcastBatch(batch, {});
        stats_.broadcast_count += batch.size();
    }

    // 清空待发送队列
    pending_updates_.clear();
}

void PositionBroadcaster::BroadcastBatch(
    const std::vector<PositionUpdate>& updates,
    const std::vector<uint64_t>& target_characters) {

    // 如果没有指定目标玩家，则向每个玩家的 AOI 内的玩家广播
    if (target_characters.empty()) {
        for (const auto& update : updates) {
            // 获取该玩家 AOI 内的其他玩家
            auto aoi_chars = aoi_manager_.GetAOICharacters(
                update.x, update.y, config_.aoi_radius);

            // 过滤掉自己
            aoi_chars.erase(
                std::remove(aoi_chars.begin(), aoi_chars.end(), update.character_id),
                aoi_chars.end());

            if (aoi_chars.empty()) {
                continue;
            }

            // 序列化消息
            auto data = SerializePositionUpdate(update);

            // 发送给 AOI 内的每个玩家
            for (uint64_t target_id : aoi_chars) {
                send_callback_(target_id, data);
            }
        }
    } else {
        // 发送给指定目标
        for (const auto& update : updates) {
            auto data = SerializePositionUpdate(update);

            for (uint64_t target_id : target_characters) {
                send_callback_(target_id, data);
            }
        }
    }
}

std::vector<uint8_t> PositionBroadcaster::SerializePositionUpdate(
    const PositionUpdate& update) const {

    // 简单的二进制序列化格式
    // [2B message_id][8B character_id][4F x][4F y][4F z][2B direction][1B move_type][8B timestamp]
    // message_id = 0x0301 (PositionUpdate)

    constexpr uint16_t MESSAGE_ID = 0x0301;
    constexpr size_t BUFFER_SIZE = 2 + 8 + 4 + 4 + 4 + 2 + 1 + 8;

    std::vector<uint8_t> buffer(BUFFER_SIZE);
    size_t offset = 0;

    // Message ID
    std::memcpy(buffer.data() + offset, &MESSAGE_ID, 2);
    offset += 2;

    // Character ID
    std::memcpy(buffer.data() + offset, &update.character_id, 8);
    offset += 8;

    // Position
    std::memcpy(buffer.data() + offset, &update.x, 4);
    offset += 4;
    std::memcpy(buffer.data() + offset, &update.y, 4);
    offset += 4;
    std::memcpy(buffer.data() + offset, &update.z, 4);
    offset += 4;

    // Direction
    std::memcpy(buffer.data() + offset, &update.direction, 2);
    offset += 2;

    // Movement Type
    buffer[offset++] = update.movement_type;

    // Timestamp
    std::memcpy(buffer.data() + offset, &update.timestamp, 8);
    offset += 8;

    return buffer;
}

void PositionBroadcaster::Clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    aoi_manager_.Clear();
    character_positions_.clear();
    pending_updates_.clear();
    stats_ = Statistics{0, 0, 0, 0};
}

PositionBroadcaster::Statistics PositionBroadcaster::GetStatistics() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return stats_;
}

} // namespace Movement
} // namespace Shared
} // namespace Murim
