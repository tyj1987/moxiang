/**
 * EntityManager.cpp
 *
 * 实体管理器实现
 */

#include "EntityManager.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>

namespace Murim {
namespace MapServer {

EntityManager& EntityManager::Instance() {
    static EntityManager instance;
    return instance;
}

bool EntityManager::Initialize(const BoundingBox& world_bounds) {
    spdlog::info("Initializing EntityManager...");
    spdlog::info("World bounds: ({}, {}) -> ({}, {})",
                 world_bounds.min_x, world_bounds.min_y,
                 world_bounds.max_x, world_bounds.max_y);

    try {
        // 创建空间索引 (QuadTree)
        // 参数: 世界边界, 每节点最大实体数=8, 最大深度=10
        spatial_index_ = std::make_unique<QuadTree>(world_bounds, 8, 10);

        spdlog::info("EntityManager initialized successfully");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize EntityManager: {}", e.what());
        return false;
    }
}

bool EntityManager::AddEntity(const GameEntity& entity) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查实体是否已存在
    if (entities_.find(entity.entity_id) != entities_.end()) {
        spdlog::warn("Entity already exists: id={}, type={}",
                     entity.entity_id, entity.GetTypeName());
        return false;
    }

    // 添加到实体存储
    entities_[entity.entity_id] = entity;

    // 添加到空间索引
    SpatialEntity spatial_entity = CreateSpatialEntity(entity);
    if (!spatial_index_->Insert(spatial_entity)) {
        spdlog::error("Failed to insert entity into spatial index: id={}, type={}, pos=({},{},{})",
                      entity.entity_id, entity.GetTypeName(),
                      entity.position.x, entity.position.y, entity.position.z);
        entities_.erase(entity.entity_id);
        return false;
    }

    // 保存空间实体引用(用于后续的位置更新)
    spatial_entities_[entity.entity_id] = spatial_entity;

    spdlog::debug("Entity added: id={}, type={}, pos=({},{},{})",
                  entity.entity_id, entity.GetTypeName(),
                  entity.position.x, entity.position.y, entity.position.z);

    return true;
}

bool EntityManager::RemoveEntity(uint64_t entity_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 从空间索引中移除
    if (spatial_entities_.find(entity_id) != spatial_entities_.end()) {
        if (!spatial_index_->Remove(entity_id)) {
            spdlog::warn("Failed to remove entity from spatial index: id={}", entity_id);
        }
        spatial_entities_.erase(entity_id);
    }

    // 从实体存储中移除
    size_t erased = entities_.erase(entity_id);
    if (erased == 0) {
        spdlog::warn("Entity not found: id={}", entity_id);
        return false;
    }

    spdlog::debug("Entity removed: id={}", entity_id);
    return true;
}

bool EntityManager::UpdateEntityPosition(uint64_t entity_id, const Position& new_position) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查实体是否存在
    auto it = entities_.find(entity_id);
    if (it == entities_.end()) {
        spdlog::warn("Entity not found for position update: id={}", entity_id);
        return false;
    }

    // 更新实体位置
    Position old_position = it->second.position;
    it->second.position = new_position;

    // 更新空间索引中的位置
    if (spatial_entities_.find(entity_id) != spatial_entities_.end()) {
        if (!spatial_index_->Update(entity_id, new_position)) {
            spdlog::error("Failed to update entity position in spatial index: id={}, old_pos=({},{},{}), new_pos=({},{},{})",
                          entity_id, old_position.x, old_position.y, old_position.z,
                          new_position.x, new_position.y, new_position.z);
            // 回滚位置更新
            it->second.position = old_position;
            return false;
        }
    }

    spdlog::trace("Entity position updated: id={}, old_pos=({},{},{}), new_pos=({},{},{})",
                  entity_id, old_position.x, old_position.y, old_position.z,
                  new_position.x, new_position.y, new_position.z);

    return true;
}

GameEntity* EntityManager::GetEntity(uint64_t entity_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(entity_id);
    if (it != entities_.end()) {
        return &it->second;
    }

    return nullptr;
}

std::vector<GameEntity*> EntityManager::QueryNearbyEntities(
    const Position& center,
    float radius,
    EntityType type_filter
) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<GameEntity*> results;

    // 使用空间索引查询附近的实体
    std::vector<SpatialEntity> spatial_results = spatial_index_->QueryInRange(center, radius);

    // 过滤实体类型
    for (const auto& spatial_entity : spatial_results) {
        auto it = entities_.find(spatial_entity.entity_id);
        if (it != entities_.end()) {
            // 应用类型过滤器
            if (type_filter == static_cast<EntityType>(-1) || it->second.type == type_filter) {
                results.push_back(&it->second);
            }
        }
    }

    return results;
}

std::vector<GameEntity*> EntityManager::QueryEntitiesInArea(
    const BoundingBox& area,
    EntityType type_filter
) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<GameEntity*> results;

    // 使用空间索引查询区域内的实体
    std::vector<SpatialEntity> spatial_results = spatial_index_->QueryInArea(area);

    // 过滤实体类型
    for (const auto& spatial_entity : spatial_results) {
        auto it = entities_.find(spatial_entity.entity_id);
        if (it != entities_.end()) {
            if (type_filter == static_cast<EntityType>(-1) || it->second.type == type_filter) {
                results.push_back(&it->second);
            }
        }
    }

    return results;
}

GameEntity* EntityManager::FindNearestEntity(
    const Position& center,
    float max_radius,
    EntityType type_filter,
    uint64_t entity_id_to_ignore
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 使用空间索引查找最近的实体
    const SpatialEntity* spatial_result = spatial_index_->FindNearest(center, max_radius, entity_id_to_ignore);
    if (!spatial_result) {
        return nullptr;
    }

    // 检查实体类型
    auto it = entities_.find(spatial_result->entity_id);
    if (it != entities_.end()) {
        if (type_filter == static_cast<EntityType>(-1) || it->second.type == type_filter) {
            return &it->second;
        }
    }

    return nullptr;
}

std::vector<GameEntity*> EntityManager::GetAllPlayers() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<GameEntity*> players;
    players.reserve(entities_.size());

    for (auto& pair : entities_) {
        if (pair.second.type == EntityType::kPlayer) {
            players.push_back(&pair.second);
        }
    }

    return players;
}

std::vector<GameEntity*> EntityManager::GetAllMonsters() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<GameEntity*> monsters;
    monsters.reserve(entities_.size());

    for (auto& pair : entities_) {
        if (pair.second.type == EntityType::kMonster) {
            monsters.push_back(&pair.second);
        }
    }

    return monsters;
}

size_t EntityManager::GetEntityCountByType(EntityType type) const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = 0;
    for (const auto& pair : entities_) {
        if (pair.second.type == type) {
            ++count;
        }
    }

    return count;
}

void EntityManager::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    entities_.clear();
    spatial_entities_.clear();
    spatial_index_->Clear();

    spdlog::info("EntityManager cleared");
}

QuadTree::Statistics EntityManager::GetSpatialStatistics() const {
    return spatial_index_->GetStatistics();
}

void EntityManager::PrintDebugInfo() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto stats = GetSpatialStatistics();

    spdlog::info("========== EntityManager Debug Info ==========");
    spdlog::info("Total entities: {}", entities_.size());
    spdlog::info("  - Players: {}", GetEntityCountByType(EntityType::kPlayer));
    spdlog::info("  - NPCs: {}", GetEntityCountByType(EntityType::kNPC));
    spdlog::info("  - Monsters: {}", GetEntityCountByType(EntityType::kMonster));
    spdlog::info("  - Pets: {}", GetEntityCountByType(EntityType::kPet));
    spdlog::info("  - Items: {}", GetEntityCountByType(EntityType::kItem));
    spdlog::info("  - Objects: {}", GetEntityCountByType(EntityType::kObject));

    spdlog::info("Spatial Index Statistics:");
    spdlog::info("  - Total entities: {}", stats.total_entities);
    spdlog::info("  - Leaf nodes: {}", stats.leaf_nodes);
    spdlog::info("  - Max depth reached: {}", stats.max_depth_reached);
    spdlog::info("  - Avg entities per leaf: {:.2f}", stats.avg_entities_per_leaf);
    spdlog::info("============================================");
}

SpatialEntity EntityManager::CreateSpatialEntity(const GameEntity& entity) {
    return SpatialEntity(entity.entity_id, entity.position, entity.user_data);
}

} // namespace MapServer
} // namespace Murim
