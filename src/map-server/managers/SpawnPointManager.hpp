#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include "../entities/Monster.hpp"
#include "../../core/spatial/QuadTree.hpp"

namespace Murim {
namespace MapServer {

/**
 * @brief 刷怪点配置
 */
struct SpawnPointConfig {
    uint64_t spawn_id;          // 刷怪点唯一ID
    uint32_t monster_id;        // 怪物模板ID
    Vector3 position;           // 刷怪位置
    uint16_t map_id;            // 地图ID
    uint32_t max_count;         // 最大同时存在数量
    uint32_t respawn_delay;     // 复活延迟（毫秒）
    uint32_t initial_spawn;     // 初始生成数量
    uint32_t group_id;          // 怪物组ID（用于协同AI）

    SpawnPointConfig()
        : spawn_id(0)
        , monster_id(0)
        , position()
        , map_id(1)
        , max_count(5)
        , respawn_delay(30000)  // 默认30秒
        , initial_spawn(5)
        , group_id(0)
    {}
};

/**
 * @brief 刷怪点运行时状态
 */
class SpawnPoint {
public:
    SpawnPoint(uint64_t spawn_id, const SpawnPointConfig& config);
    ~SpawnPoint();

    /**
     * @brief 更新刷怪点（每帧调用）
     * @param current_time 当前时间（毫秒）
     */
    void Update(uint32_t delta_time);

    /**
     * @brief 初始化（生成初始怪物）
     */
    void Initialize();

    /**
     * @brief 清理（移除所有怪物）
     */
    void Clear();

    /**
     * @brief 获取当前存活的怪物数量
     */
    uint32_t GetAliveCount() const { return alive_count_; }

    /**
     * @brief 获取刷怪点ID
     */
    uint64_t GetSpawnID() const { return spawn_id_; }

    /**
     * @brief 获取配置
     */
    const SpawnPointConfig& GetConfig() const { return config_; }

    /**
     * @brief 通知怪物死亡
     */
    void OnMonsterDeath(uint64_t entity_id);

private:
    /**
     * @brief 生成一个怪物
     */
    void SpawnMonster();

    /**
     * @brief 计算下次复活时间
     */
    void ScheduleRespawn();

    uint64_t spawn_id_;
    SpawnPointConfig config_;
    std::vector<std::unique_ptr<Monster>> monsters_;
    uint32_t alive_count_;
    uint64_t last_respawn_time_;
};

/**
 * @brief 刷怪点管理器
 *
 * 负责：
 * - 加载刷怪点配置
 * - 管理刷怪点生命周期
 * - 处理怪物刷新和复活
 * - 与EntityManager集成
 */
class SpawnPointManager {
public:
    SpawnPointManager();
    ~SpawnPointManager();

    /**
     * @brief 初始化管理器
     */
    bool Initialize(uint16_t map_id, const BoundingBox& bounds);

    /**
     * @brief 清理管理器
     */
    void Clear();

    /**
     * @brief 加载刷怪点配置（从数据库）
     */
    bool LoadSpawnPoints();

    /**
     * @brief 添加刷怪点
     */
    void AddSpawnPoint(const SpawnPointConfig& config);

    /**
     * @brief 移除刷怪点
     */
    void RemoveSpawnPoint(uint64_t spawn_id);

    /**
     * @brief 更新所有刷怪点（每帧调用）
     */
    void Update(uint32_t delta_time);

    /**
     * @brief 获取刷怪点
     */
    SpawnPoint* GetSpawnPoint(uint64_t spawn_id);

    /**
     * @brief 获取所有刷怪点
     */
    const std::unordered_map<uint64_t, std::unique_ptr<SpawnPoint>>& GetSpawnPoints() const {
        return spawn_points_;
    }

    /**
     * @brief 获取统计信息
     */
    uint32_t GetTotalSpawnPoints() const { return spawn_points_.size(); }
    uint32_t GetTotalAliveMonsters() const;

private:
    uint16_t map_id_;
    std::unordered_map<uint64_t, std::unique_ptr<SpawnPoint>> spawn_points_;
    bool initialized_;
};

} // namespace MapServer
} // namespace Murim
