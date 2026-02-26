/**
 * SpatialIndex.cpp
 *
 * High-level spatial index implementation
 */

#include "SpatialIndex.hpp"
#include <algorithm>
#include <stdexcept>

namespace Murim {

//==============================================================================
// SpatialIndex Implementation
//==============================================================================

SpatialIndex::SpatialIndex(
    const BoundingBox& world_bounds,
    int node_capacity,
    int max_depth)
    : quadtree_(std::make_unique<QuadTree>(world_bounds, node_capacity, max_depth)) {
}

bool SpatialIndex::RegisterEntity(
    uint64_t entity_id,
    EntityType type,
    const Position& position,
    void* user_data) {

    std::lock_guard<std::mutex> lock(mutex_);

    // Check if entity already exists
    if (entity_metadata_.find(entity_id) != entity_metadata_.end()) {
        return false;
    }

    // Create spatial entity
    SpatialEntity spatial_entity(entity_id, position, user_data);

    // Insert into quadtree
    if (!quadtree_->Insert(spatial_entity)) {
        return false;
    }

    // Store metadata
    entity_metadata_[entity_id] = EntityMetadata(entity_id, type, position, user_data);

    return true;
}

bool SpatialIndex::UnregisterEntity(uint64_t entity_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if entity exists
    auto it = entity_metadata_.find(entity_id);
    if (it == entity_metadata_.end()) {
        return false;
    }

    // Remove from quadtree
    if (!quadtree_->Remove(entity_id)) {
        return false;
    }

    // Remove metadata
    entity_metadata_.erase(it);

    return true;
}

bool SpatialIndex::UpdateEntityPosition(uint64_t entity_id, const Position& new_position) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if entity exists
    auto it = entity_metadata_.find(entity_id);
    if (it == entity_metadata_.end()) {
        return false;
    }

    // Update in quadtree
    if (!quadtree_->Update(entity_id, new_position)) {
        return false;
    }

    // Update metadata
    it->second.position = new_position;

    return true;
}

bool SpatialIndex::GetEntityPosition(uint64_t entity_id, Position& out_position) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entity_metadata_.find(entity_id);
    if (it == entity_metadata_.end()) {
        return false;
    }

    out_position = it->second.position;
    return true;
}

std::vector<SpatialEntity> SpatialIndex::QueryInRange(
    const Position& center,
    float radius,
    EntityType include_type) const {

    std::lock_guard<std::mutex> lock(mutex_);

    auto results = quadtree_->QueryInRange(center, radius);

    // Filter by type if specified
    if (include_type != EntityType::OBJECT) {
        return FilterByType(results, include_type);
    }

    return results;
}

std::vector<SpatialEntity> SpatialIndex::QueryInArea(
    float min_x, float min_y, float max_x, float max_y,
    EntityType include_type) const {

    BoundingBox area(min_x, min_y, max_x, max_y);
    return QueryInArea(area, include_type);
}

std::vector<SpatialEntity> SpatialIndex::QueryInArea(
    const BoundingBox& area,
    EntityType include_type) const {

    std::lock_guard<std::mutex> lock(mutex_);

    auto results = quadtree_->QueryInArea(area);

    // Filter by type if specified
    if (include_type != EntityType::OBJECT) {
        return FilterByType(results, include_type);
    }

    return results;
}

const SpatialEntity* SpatialIndex::GetNearestEntity(
    const Position& center,
    float max_radius,
    uint64_t entity_id_to_ignore,
    EntityType include_type) const {

    std::lock_guard<std::mutex> lock(mutex_);

    // If no type filter, use direct quadtree query
    if (include_type == EntityType::OBJECT) {
        return quadtree_->FindNearest(center, max_radius, entity_id_to_ignore);
    }

    // Otherwise, manually search entity_metadata_ for nearest entity of correct type
    const EntityMetadata* nearest_metadata = nullptr;
    float nearest_distance_sq = max_radius * max_radius;

    for (const auto& pair : entity_metadata_) {
        const EntityMetadata& metadata = pair.second;

        // Check type filter
        if (metadata.type != include_type) {
            continue;
        }

        // Skip ignored entity
        if (metadata.entity_id == entity_id_to_ignore) {
            continue;
        }

        // Check distance
        float dist_sq = metadata.position.DistanceSquared2D(center);
        if (dist_sq <= nearest_distance_sq) {
            nearest_distance_sq = dist_sq;
            nearest_metadata = &metadata;
        }
    }

    if (!nearest_metadata) {
        return nullptr;
    }

    // Return pointer to member variable cache (valid as long as mutex is held)
    nearest_entity_cache_ = SpatialEntity(
        nearest_metadata->entity_id,
        nearest_metadata->position,
        nearest_metadata->user_data
    );
    return &nearest_entity_cache_;
}

std::vector<SpatialEntity> SpatialIndex::QueryByTypeInRange(
    const Position& center,
    float radius,
    EntityType type) const {

    return QueryInRange(center, radius, type);
}

std::vector<SpatialEntity> SpatialIndex::QueryByTypeInArea(
    const BoundingBox& area,
    EntityType type) const {

    return QueryInArea(area, type);
}

std::vector<SpatialEntity> SpatialIndex::QueryNearbyPlayers(
    const Position& center,
    float radius,
    uint64_t exclude_player_id) const {

    std::lock_guard<std::mutex> lock(mutex_);

    auto results = quadtree_->QueryInRange(center, radius);
    auto filtered = FilterByType(results, EntityType::PLAYER);

    // Remove excluded player
    if (exclude_player_id != 0) {
        filtered.erase(
            std::remove_if(filtered.begin(), filtered.end(),
                [exclude_player_id](const SpatialEntity& e) {
                    return e.entity_id == exclude_player_id;
                }),
            filtered.end()
        );
    }

    return filtered;
}

std::vector<SpatialEntity> SpatialIndex::QueryNearbyMonsters(
    const Position& center,
    float radius) const {

    return QueryInRange(center, radius, EntityType::MONSTER);
}

std::vector<SpatialEntity> SpatialIndex::QueryNearbyNPCs(
    const Position& center,
    float radius) const {

    return QueryInRange(center, radius, EntityType::NPC);
}

std::vector<SpatialEntity> SpatialIndex::QueryPartyMembersInRange(
    const Position& center,
    float radius,
    const std::vector<uint64_t>& party_member_ids) const {

    std::lock_guard<std::mutex> lock(mutex_);

    auto results = quadtree_->QueryInRange(center, radius);
    return FilterByEntityIds(results, party_member_ids);
}

int SpatialIndex::GetEntityCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(entity_metadata_.size());
}

int SpatialIndex::GetEntityCountByType(EntityType type) const {
    std::lock_guard<std::mutex> lock(mutex_);

    int count = 0;
    for (const auto& pair : entity_metadata_) {
        if (pair.second.type == type) {
            ++count;
        }
    }

    return count;
}

void SpatialIndex::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    quadtree_->Clear();
    entity_metadata_.clear();
}

SpatialIndex::Statistics SpatialIndex::GetStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Statistics stats;
    stats.total_entities = 0;
    stats.players = 0;
    stats.npcs = 0;
    stats.monsters = 0;
    stats.pets = 0;
    stats.items = 0;
    stats.objects = 0;

    // Count by type
    for (const auto& pair : entity_metadata_) {
        ++stats.total_entities;
        switch (pair.second.type) {
            case EntityType::PLAYER:
                ++stats.players;
                break;
            case EntityType::NPC:
                ++stats.npcs;
                break;
            case EntityType::MONSTER:
                ++stats.monsters;
                break;
            case EntityType::PET:
                ++stats.pets;
                break;
            case EntityType::ITEM:
                ++stats.items;
                break;
            case EntityType::OBJECT:
                ++stats.objects;
                break;
        }
    }

    // Get quadtree stats
    auto quadtree_stats = quadtree_->GetStatistics();
    stats.quadtree_leaf_nodes = quadtree_stats.leaf_nodes;
    stats.max_depth_reached = quadtree_stats.max_depth_reached;
    stats.avg_entities_per_leaf = quadtree_stats.avg_entities_per_leaf;

    return stats;
}

bool SpatialIndex::IsEntityRegistered(uint64_t entity_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entity_metadata_.find(entity_id) != entity_metadata_.end();
}

bool SpatialIndex::GetEntityType(uint64_t entity_id, EntityType& out_type) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entity_metadata_.find(entity_id);
    if (it == entity_metadata_.end()) {
        return false;
    }

    out_type = it->second.type;
    return true;
}

std::vector<SpatialEntity> SpatialIndex::FilterByType(
    const std::vector<SpatialEntity>& entities,
    EntityType type) const {

    std::vector<SpatialEntity> filtered;

    for (const auto& entity : entities) {
        auto it = entity_metadata_.find(entity.entity_id);
        if (it != entity_metadata_.end() && it->second.type == type) {
            filtered.push_back(entity);
        }
    }

    return filtered;
}

std::vector<SpatialEntity> SpatialIndex::FilterByEntityIds(
    const std::vector<SpatialEntity>& entities,
    const std::vector<uint64_t>& entity_ids) const {

    std::vector<SpatialEntity> filtered;

    for (const auto& entity : entities) {
        if (std::find(entity_ids.begin(), entity_ids.end(), entity.entity_id) != entity_ids.end()) {
            filtered.push_back(entity);
        }
    }

    return filtered;
}

} // namespace Murim
