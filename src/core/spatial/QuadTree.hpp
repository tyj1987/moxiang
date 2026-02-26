/**
 * QuadTree.hpp
 *
 * QuadTree spatial partitioning data structure for efficient range queries.
 * Provides O(log n) insertion, deletion, and range query operations.
 *
 * Thread-safe implementation using mutex locks.
 */

#pragma once

#include <memory>
#include <vector>
#include <array>
#include <mutex>
#include <functional>
#include <cmath>
#include <limits>

namespace Murim {

/**
 * 3D position structure
 */
struct Position {
    float x;
    float y;
    float z;

    Position() : x(0.0f), y(0.0f), z(0.0f) {}
    Position(float x_, float y_, float z_ = 0.0f) : x(x_), y(y_), z(z_) {}

    // Calculate squared distance to another position
    float DistanceSquared(const Position& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return dx * dx + dy * dy + dz * dz;
    }

    // Calculate distance to another position
    float Distance(const Position& other) const {
        return std::sqrt(DistanceSquared(other));
    }

    // Calculate squared 2D distance (ignoring z-axis)
    float DistanceSquared2D(const Position& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        return dx * dx + dy * dy;
    }

    // Calculate 2D distance (ignoring z-axis)
    float Distance2D(const Position& other) const {
        return std::sqrt(DistanceSquared2D(other));
    }
};

/**
 * Spatial entity stored in the QuadTree
 */
struct SpatialEntity {
    uint64_t entity_id;      // Unique identifier (character_id, npc_id, etc.)
    Position position;       // Current position
    void* user_data;         // Optional pointer to user data

    SpatialEntity() : entity_id(0), user_data(nullptr) {}
    SpatialEntity(uint64_t id, const Position& pos, void* data = nullptr)
        : entity_id(id), position(pos), user_data(data) {}
};

/**
 * Axis-aligned bounding box
 */
struct BoundingBox {
    float min_x;
    float min_y;
    float max_x;
    float max_y;

    BoundingBox() : min_x(0), min_y(0), max_x(0), max_y(0) {}

    BoundingBox(float min_x_, float min_y_, float max_x_, float max_y_)
        : min_x(min_x_), min_y(min_y_), max_x(max_x_), max_y(max_y_) {}

    // Check if a point is inside this bounding box
    bool Contains(const Position& pos) const {
        return pos.x >= min_x && pos.x <= max_x &&
               pos.y >= min_y && pos.y <= max_y;
    }

    // Check if another bounding box intersects with this one
    bool Intersects(const BoundingBox& other) const {
        return !(other.max_x < min_x || other.min_x > max_x ||
                 other.max_y < min_y || other.min_y > max_y);
    }

    // Check if a circle (position + radius) intersects this bounding box
    bool IntersectsCircle(const Position& center, float radius) const {
        // Find the closest point on the rectangle to the circle center
        float closest_x = std::max(min_x, std::min(center.x, max_x));
        float closest_y = std::max(min_y, std::min(center.y, max_y));

        // Calculate distance from closest point to circle center
        float dx = center.x - closest_x;
        float dy = center.y - closest_y;

        return (dx * dx + dy * dy) <= (radius * radius);
    }

    // Get width of bounding box
    float Width() const { return max_x - min_x; }

    // Get height of bounding box
    float Height() const { return max_y - min_y; }

    // Get area of bounding box
    float Area() const { return Width() * Height(); }
};

/**
 * QuadTree node class
 */
class QuadTreeNode {
public:
    // Quadrant enumeration
    enum Quadrant {
        NW = 0, // North-West
        NE = 1, // North-East
        SW = 2, // South-West
        SE = 3  // South-East
    };

private:
    BoundingBox bounds_;
    int capacity_;
    int max_depth_;
    int depth_;

    std::vector<SpatialEntity> entities_;
    std::array<std::unique_ptr<QuadTreeNode>, 4> children_;
    bool is_leaf_;

    mutable std::mutex mutex_;

public:
    /**
     * Constructor
     * @param bounds Bounding box of this node
     * @param capacity Maximum entities before splitting
     * @param max_depth Maximum depth of tree
     * @param depth Current depth of this node
     */
    QuadTreeNode(const BoundingBox& bounds, int capacity, int max_depth, int depth = 0);

    /**
     * Insert an entity into the tree
     * @param entity Entity to insert
     * @return true if inserted successfully
     */
    bool Insert(const SpatialEntity& entity);

    /**
     * Remove an entity from the tree
     * @param entity_id ID of entity to remove
     * @return true if removed successfully
     */
    bool Remove(uint64_t entity_id);

    /**
     * Update an entity's position
     * @param entity_id ID of entity to update
     * @param new_position New position
     * @return true if updated successfully
     */
    bool Update(uint64_t entity_id, const Position& new_position);

    /**
     * Query entities within a radius of a position
     * @param center Center position
     * @param radius Search radius
     * @param results Output vector for results
     */
    void QueryInRange(const Position& center, float radius, std::vector<SpatialEntity>& results) const;

    /**
     * Query entities in a rectangular area
     * @param area Bounding box to search
     * @param results Output vector for results
     */
    void QueryInArea(const BoundingBox& area, std::vector<SpatialEntity>& results) const;

    /**
     * Find the nearest entity to a position
     * @param center Center position
     * @param max_radius Maximum search radius
     * @param entity_id_to_ignore Entity ID to ignore (e.g., self)
     * @return Pointer to nearest entity, or nullptr if none found
     */
    const SpatialEntity* FindNearest(const Position& center, float max_radius, uint64_t entity_id_to_ignore = 0) const;

    /**
     * Get total number of entities in this node and all children
     */
    int CountEntities() const;

    /**
     * Get number of leaf nodes
     */
    int CountLeafNodes() const;

    /**
     * Get maximum depth of tree
     */
    int GetMaxDepth() const { return max_depth_; }

    /**
     * Get current depth of this node
     */
    int GetDepth() const { return depth_; }

    /**
     * Check if node is a leaf
     */
    bool IsLeaf() const { return is_leaf_; }

    /**
     * Get bounding box
     */
    const BoundingBox& GetBounds() const { return bounds_; }

    /**
     * Clear all entities and reset tree
     */
    void Clear();

private:
    /**
     * Split node into 4 children
     */
    void Split();

    /**
     * Merge children back into parent
     */
    void Merge();

    /**
     * Get which quadrant a position belongs to
     */
    Quadrant GetQuadrant(const Position& pos) const;

    /**
     * Create bounding box for a quadrant
     */
    BoundingBox CreateQuadrantBounds(Quadrant quadrant) const;

    /**
     * Try to merge all entities into this node
     */
    void TryMerge();
};

/**
 * QuadTree class - Main interface for spatial indexing
 */
class QuadTree {
private:
    std::unique_ptr<QuadTreeNode> root_;
    BoundingBox world_bounds_;
    int node_capacity_;
    int max_depth_;
    mutable std::mutex mutex_;

public:
    /**
     * Constructor
     * @param world_bounds Bounding box of the entire game world
     * @param node_capacity Maximum entities per node before splitting
     * @param max_depth Maximum depth of quadtree
     */
    QuadTree(const BoundingBox& world_bounds, int node_capacity = 8, int max_depth = 10);

    /**
     * Insert an entity into the tree
     * @param entity Entity to insert
     * @return true if inserted successfully
     */
    bool Insert(const SpatialEntity& entity);

    /**
     * Remove an entity from the tree
     * @param entity_id ID of entity to remove
     * @return true if removed successfully
     */
    bool Remove(uint64_t entity_id);

    /**
     * Update an entity's position
     * @param entity_id ID of entity to update
     * @param new_position New position
     * @return true if updated successfully
     */
    bool Update(uint64_t entity_id, const Position& new_position);

    /**
     * Query entities within a radius of a position
     * @param center Center position
     * @param radius Search radius
     * @return Vector of entities within radius
     */
    std::vector<SpatialEntity> QueryInRange(const Position& center, float radius) const;

    /**
     * Query entities in a rectangular area
     * @param area Bounding box to search
     * @return Vector of entities in area
     */
    std::vector<SpatialEntity> QueryInArea(const BoundingBox& area) const;

    /**
     * Find the nearest entity to a position
     * @param center Center position
     * @param max_radius Maximum search radius
     * @param entity_id_to_ignore Entity ID to ignore (e.g., self)
     * @return Pointer to nearest entity, or nullptr if none found
     */
    const SpatialEntity* FindNearest(const Position& center, float max_radius, uint64_t entity_id_to_ignore = 0) const;

    /**
     * Get total number of entities in the tree
     */
    int CountEntities() const;

    /**
     * Get number of leaf nodes
     */
    int CountLeafNodes() const;

    /**
     * Get world bounds
     */
    const BoundingBox& GetWorldBounds() const { return world_bounds_; }

    /**
     * Clear all entities
     */
    void Clear();

    /**
     * Get statistics about the tree
     */
    struct Statistics {
        int total_entities;
        int leaf_nodes;
        int max_depth_reached;
        float avg_entities_per_leaf;
    };

    Statistics GetStatistics() const;
};

} // namespace Murim
