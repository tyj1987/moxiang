/**
 * SpatialIndex.hpp
 *
 * High-level interface for spatial indexing in the game world.
 * Provides a clean API for registering, updating, and querying entities.
 *
 * This is the main interface that game managers (ChatManager, AIManager, etc.)
 * should use for spatial queries.
 */

#pragma once

#include "QuadTree.hpp"
#include <unordered_map>
#include <mutex>
#include <memory>

namespace Murim {

/**
 * Entity type enumeration for type-based queries
 */
enum class EntityType {
    PLAYER,
    NPC,
    MONSTER,
    PET,
    ITEM,
    OBJECT
};

/**
 * Entity metadata for spatial index
 */
struct EntityMetadata {
    uint64_t entity_id;
    EntityType type;
    Position position;
    void* user_data;

    EntityMetadata()
        : entity_id(0)
        , type(EntityType::OBJECT)
        , user_data(nullptr) {}

    EntityMetadata(uint64_t id, EntityType t, const Position& pos, void* data = nullptr)
        : entity_id(id)
        , type(t)
        , position(pos)
        , user_data(data) {}
};

/**
 * Query result with entity and distance
 */
struct QueryResult {
    SpatialEntity entity;
    float distance;

    QueryResult(const SpatialEntity& e, float d)
        : entity(e), distance(d) {}
};

/**
 * Spatial index class - High-level interface
 *
 * Thread-safe singleton-like interface for spatial indexing.
 * Manages entity registration, position updates, and spatial queries.
 */
class SpatialIndex {
public:
    /**
     * Constructor
     * @param world_bounds Bounding box of the game world
     * @param node_capacity Maximum entities per quadtree node
     * @param max_depth Maximum depth of quadtree
     */
    SpatialIndex(
        const BoundingBox& world_bounds,
        int node_capacity = 8,
        int max_depth = 10
    );

    /**
     * Destructor
     */
    ~SpatialIndex() = default;

    // Prevent copying
    SpatialIndex(const SpatialIndex&) = delete;
    SpatialIndex& operator=(const SpatialIndex&) = delete;

    /**
     * Register an entity in the spatial index
     * @param entity_id Unique entity identifier
     * @param type Type of entity
     * @param position Initial position
     * @param user_data Optional user data pointer
     * @return true if registered successfully
     */
    bool RegisterEntity(uint64_t entity_id, EntityType type, const Position& position, void* user_data = nullptr);

    /**
     * Unregister an entity from the spatial index
     * @param entity_id Entity to unregister
     * @return true if unregistered successfully
     */
    bool UnregisterEntity(uint64_t entity_id);

    /**
     * Update an entity's position
     * @param entity_id Entity to update
     * @param new_position New position
     * @return true if updated successfully
     */
    bool UpdateEntityPosition(uint64_t entity_id, const Position& new_position);

    /**
     * Get an entity's current position
     * @param entity_id Entity to query
     * @param out_position Output parameter for position
     * @return true if entity found
     */
    bool GetEntityPosition(uint64_t entity_id, Position& out_position) const;

    /**
     * Query all entities within a radius of a position
     * @param center Center position
     * @param radius Search radius
     * @param include_type Optional filter for entity type
     * @return Vector of entities within radius
     */
    std::vector<SpatialEntity> QueryInRange(
        const Position& center,
        float radius,
        EntityType include_type = EntityType::OBJECT
    ) const;

    /**
     * Query entities in a rectangular area
     * @param min_x Minimum X coordinate
     * @param min_y Minimum Y coordinate
     * @param max_x Maximum X coordinate
     * @param max_y Maximum Y coordinate
     * @param include_type Optional filter for entity type
     * @return Vector of entities in area
     */
    std::vector<SpatialEntity> QueryInArea(
        float min_x, float min_y, float max_x, float max_y,
        EntityType include_type = EntityType::OBJECT
    ) const;

    /**
     * Query entities in a rectangular area using BoundingBox
     * @param area Bounding box to search
     * @param include_type Optional filter for entity type
     * @return Vector of entities in area
     */
    std::vector<SpatialEntity> QueryInArea(
        const BoundingBox& area,
        EntityType include_type = EntityType::OBJECT
    ) const;

    /**
     * Find the nearest entity to a position
     * @param center Center position
     * @param max_radius Maximum search radius
     * @param entity_id_to_ignore Entity ID to ignore (e.g., self)
     * @param include_type Optional filter for entity type
     * @return Pointer to nearest entity, or nullptr if none found
     */
    const SpatialEntity* GetNearestEntity(
        const Position& center,
        float max_radius,
        uint64_t entity_id_to_ignore = 0,
        EntityType include_type = EntityType::OBJECT
    ) const;

    /**
     * Find all entities of a specific type within radius
     * @param center Center position
     * @param radius Search radius
     * @param type Entity type to filter
     * @return Vector of entities of type within radius
     */
    std::vector<SpatialEntity> QueryByTypeInRange(
        const Position& center,
        float radius,
        EntityType type
    ) const;

    /**
     * Find all entities of a specific type in an area
     * @param area Bounding box to search
     * @param type Entity type to filter
     * @return Vector of entities of type in area
     */
    std::vector<SpatialEntity> QueryByTypeInArea(
        const BoundingBox& area,
        EntityType type
    ) const;

    /**
     * Query nearby players (convenience method for ChatManager)
     * @param center Center position
     * @param radius Search radius
     * @param exclude_player_id Player ID to exclude (e.g., sender)
     * @return Vector of nearby player entities
     */
    std::vector<SpatialEntity> QueryNearbyPlayers(
        const Position& center,
        float radius,
        uint64_t exclude_player_id = 0
    ) const;

    /**
     * Query nearby monsters (convenience method for AIManager)
     * @param center Center position
     * @param radius Search radius
     * @return Vector of nearby monster entities
     */
    std::vector<SpatialEntity> QueryNearbyMonsters(
        const Position& center,
        float radius
    ) const;

    /**
     * Query nearby NPCs (convenience method for AIManager)
     * @param center Center position
     * @param radius Search radius
     * @return Vector of nearby NPC entities
     */
    std::vector<SpatialEntity> QueryNearbyNPCs(
        const Position& center,
        float radius
    ) const;

    /**
     * Get all party members within range (convenience method for PartyManager)
     * @param center Center position
     * @param radius Search radius
     * @param party_member_ids List of party member IDs
     * @return Vector of party members within range
     */
    std::vector<SpatialEntity> QueryPartyMembersInRange(
        const Position& center,
        float radius,
        const std::vector<uint64_t>& party_member_ids
    ) const;

    /**
     * Get total number of entities in the index
     */
    int GetEntityCount() const;

    /**
     * Get count of entities by type
     */
    int GetEntityCountByType(EntityType type) const;

    /**
     * Clear all entities from the index
     */
    void Clear();

    /**
     * Get statistics about the spatial index
     */
    struct Statistics {
        int total_entities;
        int players;
        int npcs;
        int monsters;
        int pets;
        int items;
        int objects;
        int quadtree_leaf_nodes;
        int max_depth_reached;
        float avg_entities_per_leaf;
    };

    Statistics GetStatistics() const;

    /**
     * Check if an entity is registered
     */
    bool IsEntityRegistered(uint64_t entity_id) const;

    /**
     * Get entity type
     */
    bool GetEntityType(uint64_t entity_id, EntityType& out_type) const;

private:
    mutable std::mutex mutex_;
    std::unique_ptr<QuadTree> quadtree_;
    std::unordered_map<uint64_t, EntityMetadata> entity_metadata_;

    // Temporary entity for GetNearestEntity (to avoid returning pointer to local)
    mutable SpatialEntity nearest_entity_cache_;

    /**
     * Filter results by entity type
     */
    std::vector<SpatialEntity> FilterByType(
        const std::vector<SpatialEntity>& entities,
        EntityType type
    ) const;

    /**
     * Filter results by entity IDs
     */
    std::vector<SpatialEntity> FilterByEntityIds(
        const std::vector<SpatialEntity>& entities,
        const std::vector<uint64_t>& entity_ids
    ) const;
};

} // namespace Murim
