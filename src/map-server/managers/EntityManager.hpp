/**
 * EntityManager.hpp
 *
 * 实体管理器 - 管理游戏世界中的所有实体
 * 集成 QuadTree 空间索引,提供高效的附近实体查询
 *
 * 对应 legacy: CGridSystem / CObjectManager
 */

#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "core/spatial/QuadTree.hpp"
#include "shared/character/Character.hpp"

namespace Murim {
namespace MapServer {

/**
 * 实体类型
 */
enum class EntityType {
    kPlayer = 0,     // 玩家
    kNPC = 1,        // NPC
    kMonster = 2,    // 怪物
    kPet = 3,        // 宠物
    kItem = 4,       // 地面物品
    kObject = 5      // 游戏对象(陷阱、传送门等)
};

/**
 * 游戏实体基类
 */
struct GameEntity {
    uint64_t entity_id;          // 实体唯一ID
    EntityType type;             // 实体类型
    Position position;           // 位置
    float rotation;              // 旋转角度
    void* user_data;             // 用户数据指针(指向 Character/NPC/Monster 等)

    GameEntity()
        : entity_id(0)
        , type(EntityType::kPlayer)
        , rotation(0.0f)
        , user_data(nullptr)
    {}

    GameEntity(uint64_t id, EntityType type_, const Position& pos, void* data = nullptr)
        : entity_id(id)
        , type(type_)
        , position(pos)
        , rotation(0.0f)
        , user_data(data)
    {}

    /**
     * 获取实体类型名称
     */
    const char* GetTypeName() const {
        switch (type) {
            case EntityType::kPlayer: return "Player";
            case EntityType::kNPC: return "NPC";
            case EntityType::kMonster: return "Monster";
            case EntityType::kPet: return "Pet";
            case EntityType::kItem: return "Item";
            case EntityType::kObject: return "Object";
            default: return "Unknown";
        }
    }
};

/**
 * 实体管理器
 */
class EntityManager {
public:
    /**
     * @brief 获取单例实例
     */
    static EntityManager& Instance();

    /**
     * @brief 初始化实体管理器
     * @param world_bounds 游戏世界边界
     * @param success 是否初始化成功
     */
    bool Initialize(const BoundingBox& world_bounds);

    /**
     * @brief 添加实体
     * @param entity 实体信息
     * @return 是否添加成功
     */
    bool AddEntity(const GameEntity& entity);

    /**
     * @brief 移除实体
     * @param entity_id 实体ID
     * @return 是否移除成功
     */
    bool RemoveEntity(uint64_t entity_id);

    /**
     * @brief 更新实体位置
     * @param entity_id 实体ID
     * @param new_position 新位置
     * @return 是否更新成功
     */
    bool UpdateEntityPosition(uint64_t entity_id, const Position& new_position);

    /**
     * @brief 获取实体
     * @param entity_id 实体ID
     * @return 实体指针,如果不存在返回 nullptr
     */
    GameEntity* GetEntity(uint64_t entity_id);

    /**
     * @brief 查询附近的实体
     * @param center 中心位置
     * @param radius 查询半径
     * @param type_filter 实体类型过滤器(EntityType::kPlayer 等),-1 表示所有类型
     * @return 附近的实体列表
     */
    std::vector<GameEntity*> QueryNearbyEntities(
        const Position& center,
        float radius,
        EntityType type_filter = static_cast<EntityType>(-1)
    );

    /**
     * @brief 查询矩形区域内的实体
     * @param area 查询区域
     * @param type_filter 实体类型过滤器,-1 表示所有类型
     * @return 区域内的实体列表
     */
    std::vector<GameEntity*> QueryEntitiesInArea(
        const BoundingBox& area,
        EntityType type_filter = static_cast<EntityType>(-1)
    );

    /**
     * @brief 查找最近的实体
     * @param center 中心位置
     * @param max_radius 最大查询半径
     * @param type_filter 实体类型过滤器
     * @param entity_id_to_ignore 要忽略的实体ID(例如自己)
     * @return 最近的实体指针,如果没有找到返回 nullptr
     */
    GameEntity* FindNearestEntity(
        const Position& center,
        float max_radius,
        EntityType type_filter,
        uint64_t entity_id_to_ignore = 0
    );

    /**
     * @brief 获取所有在线玩家
     * @return 玩家实体列表
     */
    std::vector<GameEntity*> GetAllPlayers();

    /**
     * @brief 获取所有怪物
     * @return 怪物实体列表
     */
    std::vector<GameEntity*> GetAllMonsters();

    /**
     * @brief 获取实体数量
     * @return 实体总数
     */
    size_t GetEntityCount() const { return entities_.size(); }

    /**
     * @brief 获取指定类型的实体数量
     * @param type 实体类型
     * @return 实体数量
     */
    size_t GetEntityCountByType(EntityType type) const;

    /**
     * @brief 清空所有实体
     */
    void Clear();

    /**
     * @brief 获取空间索引统计信息
     * @return QuadTree 统计信息
     */
    QuadTree::Statistics GetSpatialStatistics() const;

    /**
     * @brief 打印调试信息
     */
    void PrintDebugInfo();

private:
    EntityManager() = default;
    ~EntityManager() = default;
    EntityManager(const EntityManager&) = delete;
    EntityManager& operator=(const EntityManager&) = delete;

    // ========== 成员变量 ==========

    std::unique_ptr<QuadTree> spatial_index_;                    // 空间索引
    std::unordered_map<uint64_t, GameEntity> entities_;          // 实体存储(entity_id -> entity)
    std::unordered_map<uint64_t, SpatialEntity> spatial_entities_; // 空间实体存储(entity_id -> spatial_entity)

    mutable std::mutex mutex_;                                   // 线程安全锁

    // ========== 辅助方法 ==========

    /**
     * @brief 创建空间实体
     * @param entity 游戏实体
     * @return 空间实体
     */
    SpatialEntity CreateSpatialEntity(const GameEntity& entity);
};

} // namespace MapServer
} // namespace Murim
