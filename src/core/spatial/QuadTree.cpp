/**
 * QuadTree.cpp
 *
 * QuadTree spatial partitioning implementation
 */

#include "QuadTree.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace Murim {

//==============================================================================
// QuadTreeNode Implementation
//==============================================================================

QuadTreeNode::QuadTreeNode(const BoundingBox& bounds, int capacity, int max_depth, int depth)
    : bounds_(bounds)
    , capacity_(capacity)
    , max_depth_(max_depth)
    , depth_(depth)
    , is_leaf_(true) {
}

bool QuadTreeNode::Insert(const SpatialEntity& entity) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if entity is within bounds
    if (!bounds_.Contains(entity.position)) {
        return false;
    }

    // If we're a leaf node
    if (is_leaf_) {
        // If we have capacity, add the entity
        if (entities_.size() < static_cast<size_t>(capacity_) || depth_ >= max_depth_) {
            entities_.push_back(entity);
            return true;
        }

        // Otherwise, split and redistribute
        Split();

        // Redistribute existing entities
        for (const auto& ent : entities_) {
            Quadrant quad = GetQuadrant(ent.position);
            if (children_[quad]) {
                children_[quad]->Insert(ent);
            }
        }
        entities_.clear();

        // Insert the new entity
        Quadrant quad = GetQuadrant(entity.position);
        if (children_[quad]) {
            return children_[quad]->Insert(entity);
        }
        return false;
    }

    // If we're an internal node, delegate to appropriate child
    Quadrant quad = GetQuadrant(entity.position);
    if (children_[quad]) {
        return children_[quad]->Insert(entity);
    }

    return false;
}

bool QuadTreeNode::Remove(uint64_t entity_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // If we're a leaf node, search in our entities
    if (is_leaf_) {
        auto it = std::find_if(entities_.begin(), entities_.end(),
            [entity_id](const SpatialEntity& e) { return e.entity_id == entity_id; });

        if (it != entities_.end()) {
            entities_.erase(it);
            return true;
        }
        return false;
    }

    // If we're an internal node, delegate to children
    for (auto& child : children_) {
        if (child && child->Remove(entity_id)) {
            // Check if we should merge
            TryMerge();
            return true;
        }
    }

    return false;
}

bool QuadTreeNode::Update(uint64_t entity_id, const Position& new_position) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Step 1: Find and remove the entity (inline implementation to avoid deadlock)
    // Note: We skip TryMerge() during update to avoid tree restructuring

    // Helper lambda to find and remove entity
    std::function<bool(QuadTreeNode*)> find_and_remove = [&](QuadTreeNode* node) -> bool {
        if (!node) return false;

        if (node->is_leaf_) {
            auto it = std::find_if(node->entities_.begin(), node->entities_.end(),
                [entity_id](const SpatialEntity& e) { return e.entity_id == entity_id; });

            if (it != node->entities_.end()) {
                node->entities_.erase(it);
                return true;
            }
            return false;
        }

        // Internal node - search children (don't call TryMerge during update!)
        for (auto& child : node->children_) {
            if (child && find_and_remove(child.get())) {
                // Skip TryMerge() during update to prevent tree restructuring
                return true;
            }
        }
        return false;
    };

    bool found = find_and_remove(this);
    if (!found) {
        return false;  // Entity not found
    }

    // Step 2: Insert entity at new position (inline implementation to avoid deadlock)
    SpatialEntity new_entity(entity_id, new_position);

    // Helper lambda to insert entity (recursive, no locking)
    std::function<bool(QuadTreeNode*)> insert_entity = [&](QuadTreeNode* node) -> bool {
        // Check if position is within this node's bounds
        if (!node->bounds_.Contains(new_position)) {
            return false;  // Position is outside this node's bounds
        }

        // If we're a leaf node with space, add here
        if (node->is_leaf_) {
            if (static_cast<int>(node->entities_.size()) < node->capacity_) {
                node->entities_.push_back(new_entity);
                return true;
            }
            // Need to split
            node->Split();
        }

        // If we're an internal node (or just split), insert into appropriate child
        for (auto& child : node->children_) {
            if (child && child->bounds_.Contains(new_position)) {
                return insert_entity(child.get());
            }
        }

        // If we get here and are still an internal node, something went wrong
        // (position is within bounds but no child contains it)
        // This shouldn't happen after split, but handle it gracefully
        if (node->is_leaf_) {
            node->entities_.push_back(new_entity);
            return true;
        }

        return false;  // Failed to insert
    };

    return insert_entity(this);
}

void QuadTreeNode::QueryInRange(const Position& center, float radius, std::vector<SpatialEntity>& results) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if this node's bounds intersects with the search circle
    if (!bounds_.IntersectsCircle(center, radius)) {
        return;
    }

    // If we're a leaf node, check all entities
    if (is_leaf_) {
        float radius_sq = radius * radius;
        for (const auto& entity : entities_) {
            if (entity.position.DistanceSquared2D(center) <= radius_sq) {
                results.push_back(entity);
            }
        }
        return;
    }

    // If we're an internal node, query children
    for (const auto& child : children_) {
        if (child) {
            child->QueryInRange(center, radius, results);
        }
    }
}

void QuadTreeNode::QueryInArea(const BoundingBox& area, std::vector<SpatialEntity>& results) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if this node's bounds intersects with the search area
    if (!bounds_.Intersects(area)) {
        return;
    }

    // If we're a leaf node, check all entities
    if (is_leaf_) {
        for (const auto& entity : entities_) {
            if (area.Contains(entity.position)) {
                results.push_back(entity);
            }
        }
        return;
    }

    // If we're an internal node, query children
    for (const auto& child : children_) {
        if (child) {
            child->QueryInArea(area, results);
        }
    }
}

const SpatialEntity* QuadTreeNode::FindNearest(const Position& center, float max_radius, uint64_t entity_id_to_ignore) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if this node's bounds intersects with the search circle
    if (!bounds_.IntersectsCircle(center, max_radius)) {
        return nullptr;
    }

    const SpatialEntity* nearest = nullptr;
    float nearest_distance_sq = max_radius * max_radius;

    // If we're a leaf node, check all entities
    if (is_leaf_) {
        for (const auto& entity : entities_) {
            // Skip ignored entity
            if (entity.entity_id == entity_id_to_ignore) {
                continue;
            }

            float dist_sq = entity.position.DistanceSquared2D(center);
            if (dist_sq < nearest_distance_sq) {
                nearest_distance_sq = dist_sq;
                nearest = &entity;
            }
        }
        return nearest;
    }

    // If we're an internal node, query children
    for (const auto& child : children_) {
        if (child) {
            const SpatialEntity* child_nearest = child->FindNearest(center, max_radius, entity_id_to_ignore);
            if (child_nearest) {
                float dist_sq = child_nearest->position.DistanceSquared2D(center);
                if (dist_sq < nearest_distance_sq) {
                    nearest_distance_sq = dist_sq;
                    nearest = child_nearest;
                }
            }
        }
    }

    return nearest;
}

int QuadTreeNode::CountEntities() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_leaf_) {
        return static_cast<int>(entities_.size());
    }

    int count = 0;
    for (const auto& child : children_) {
        if (child) {
            count += child->CountEntities();
        }
    }
    return count;
}

int QuadTreeNode::CountLeafNodes() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_leaf_) {
        return 1;
    }

    int count = 0;
    for (const auto& child : children_) {
        if (child) {
            count += child->CountLeafNodes();
        }
    }
    return count;
}

void QuadTreeNode::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    entities_.clear();
    for (auto& child : children_) {
        child.reset();
    }
    is_leaf_ = true;
}

void QuadTreeNode::Split() {
    if (!is_leaf_) {
        return; // Already split
    }

    // Create 4 children
    for (int i = 0; i < 4; ++i) {
        BoundingBox child_bounds = CreateQuadrantBounds(static_cast<Quadrant>(i));
        children_[i] = std::make_unique<QuadTreeNode>(child_bounds, capacity_, max_depth_, depth_ + 1);
    }

    // Redistribute existing entities to children
    std::vector<SpatialEntity> entities_to_move = std::move(entities_);
    entities_.clear();  // Clear entities from parent

    for (const auto& entity : entities_to_move) {
        // Find the appropriate child for this entity
        for (auto& child : children_) {
            if (child && child->bounds_.Contains(entity.position)) {
                child->entities_.push_back(entity);
                break;
            }
        }
    }

    is_leaf_ = false;
}

void QuadTreeNode::Merge() {
    if (is_leaf_) {
        return; // Already a leaf
    }

    // Collect all entities from children
    std::vector<SpatialEntity> all_entities;
    for (auto& child : children_) {
        if (child) {
            // Recursively collect from children
            std::vector<SpatialEntity> child_entities;
            child->QueryInArea(child->GetBounds(), child_entities);
            all_entities.insert(all_entities.end(), child_entities.begin(), child_entities.end());
            child.reset();
        }
    }

    // Store entities in this node
    entities_ = std::move(all_entities);
    is_leaf_ = true;
}

QuadTreeNode::Quadrant QuadTreeNode::GetQuadrant(const Position& pos) const {
    float mid_x = bounds_.min_x + bounds_.Width() * 0.5f;
    float mid_y = bounds_.min_y + bounds_.Height() * 0.5f;

    if (pos.x < mid_x) {
        return (pos.y < mid_y) ? SW : NW;
    } else {
        return (pos.y < mid_y) ? SE : NE;
    }
}

BoundingBox QuadTreeNode::CreateQuadrantBounds(Quadrant quadrant) const {
    float mid_x = bounds_.min_x + bounds_.Width() * 0.5f;
    float mid_y = bounds_.min_y + bounds_.Height() * 0.5f;

    switch (quadrant) {
        case NW: // North-West (left, top)
            return BoundingBox(bounds_.min_x, mid_y, mid_x, bounds_.max_y);
        case NE: // North-East (right, top)
            return BoundingBox(mid_x, mid_y, bounds_.max_x, bounds_.max_y);
        case SW: // South-West (left, bottom)
            return BoundingBox(bounds_.min_x, bounds_.min_y, mid_x, mid_y);
        case SE: // South-East (right, bottom)
            return BoundingBox(mid_x, bounds_.min_y, bounds_.max_x, mid_y);
        default:
            return bounds_;
    }
}

void QuadTreeNode::TryMerge() {
    if (is_leaf_) {
        return;
    }

    // Count total entities in all children
    int total_entities = 0;
    for (const auto& child : children_) {
        if (child) {
            total_entities += child->CountEntities();
        }
    }

    // Merge if total entities is below capacity
    if (total_entities < capacity_) {
        Merge();
    }
}

//==============================================================================
// QuadTree Implementation
//==============================================================================

QuadTree::QuadTree(const BoundingBox& world_bounds, int node_capacity, int max_depth)
    : world_bounds_(world_bounds)
    , node_capacity_(node_capacity)
    , max_depth_(max_depth) {
    root_ = std::make_unique<QuadTreeNode>(world_bounds_, node_capacity_, max_depth_, 0);
}

bool QuadTree::Insert(const SpatialEntity& entity) {
    std::lock_guard<std::mutex> lock(mutex_);
    return root_->Insert(entity);
}

bool QuadTree::Remove(uint64_t entity_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return root_->Remove(entity_id);
}

bool QuadTree::Update(uint64_t entity_id, const Position& new_position) {
    std::lock_guard<std::mutex> lock(mutex_);
    return root_->Update(entity_id, new_position);
}

std::vector<SpatialEntity> QuadTree::QueryInRange(const Position& center, float radius) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<SpatialEntity> results;
    root_->QueryInRange(center, radius, results);
    return results;
}

std::vector<SpatialEntity> QuadTree::QueryInArea(const BoundingBox& area) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<SpatialEntity> results;
    root_->QueryInArea(area, results);
    return results;
}

const SpatialEntity* QuadTree::FindNearest(const Position& center, float max_radius, uint64_t entity_id_to_ignore) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return root_->FindNearest(center, max_radius, entity_id_to_ignore);
}

int QuadTree::CountEntities() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return root_->CountEntities();
}

int QuadTree::CountLeafNodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return root_->CountLeafNodes();
}

void QuadTree::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    root_->Clear();
}

QuadTree::Statistics QuadTree::GetStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Statistics stats;
    stats.total_entities = root_->CountEntities();
    stats.leaf_nodes = root_->CountLeafNodes();
    stats.max_depth_reached = root_->GetDepth();
    stats.avg_entities_per_leaf = stats.leaf_nodes > 0 ?
        static_cast<float>(stats.total_entities) / static_cast<float>(stats.leaf_nodes) : 0.0f;

    return stats;
}

} // namespace Murim
