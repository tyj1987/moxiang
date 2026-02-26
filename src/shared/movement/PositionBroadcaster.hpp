#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include "../game/GameObject.hpp"
#include "../../core/network/ConnectionManager.hpp"

namespace Murim {
namespace Shared {
namespace Movement {

// 前向声明
class AOIManager;

/**
 * @brief 位置更新消息结构
 *
 * 用于封装角色位置信息并广播给周围玩家
 */
struct PositionUpdate {
    uint64_t character_id;      // 角色ID
    float x;                     // X 坐标
    float y;                     // Y 坐标
    float z;                     // Z 坐标
    uint16_t direction;          // 朝向 (0-360)
    uint8_t movement_type;       // 移动类型 (0=站立, 1=走路, 2=跑步, 3=跳跃)
    uint64_t timestamp;          // 时间戳

    PositionUpdate()
        : character_id(0), x(0.0f), y(0.0f), z(0.0f),
          direction(0), movement_type(0), timestamp(0) {}

    PositionUpdate(uint64_t id, float px, float py, float pz,
                 uint16_t dir, uint8_t move_type, uint64_t ts)
        : character_id(id), x(px), y(py), z(pz),
          direction(dir), movement_type(move_type), timestamp(ts) {}
};

/**
 * @brief 位置广播配置
 *
 * 配置广播系统的各项参数
 */
struct PositionBroadcastConfig {
    float aoi_radius;              // AOI 半径（格子数）
    float grid_size;               // 网格大小（米）
    float position_threshold;      // 位置变化阈值（米，小于此值不广播）
    uint32_t batch_size;          // 批量发送大小
    uint32_t broadcast_interval;   // 广播间隔（毫秒）

    PositionBroadcastConfig()
        : aoi_radius(150.0f),      // 150米 AOI 半径
          grid_size(50.0f),         // 50米网格
          position_threshold(0.5f), // 0.5米阈值
          batch_size(20),           // 每批20个玩家
          broadcast_interval(100) {} // 100ms 广播间隔
};

/**
 * @brief AOI (Area of Interest) 管理器
 *
 * 管理玩家的兴趣区域，使用网格分区进行空间索引
 */
class AOIManager {
public:
    /**
     * @brief 网格坐标
     */
    struct GridCoord {
        int x;
        int y;

        GridCoord() : x(0), y(0) {}
        GridCoord(int gx, int gy) : x(gx), y(gy) {}

        bool operator==(const GridCoord& other) const {
            return x == other.x && y == other.y;
        }
    };

    /**
     * @brief 网格坐标哈希函数
     */
    struct GridCoordHash {
        size_t operator()(const GridCoord& coord) const {
            return std::hash<int>()(coord.x) ^ (std::hash<int>()(coord.y) << 1);
        }
    };

    /**
     * @brief 网格单元
     */
    struct GridCell {
        std::unordered_set<uint64_t> character_ids;  // 该网格内的玩家ID集合

        void AddCharacter(uint64_t character_id) {
            character_ids.insert(character_id);
        }

        void RemoveCharacter(uint64_t character_id) {
            character_ids.erase(character_id);
        }

        bool IsEmpty() const {
            return character_ids.empty();
        }

        size_t GetCount() const {
            return character_ids.size();
        }
    };

    AOIManager(float grid_size);

    /**
     * @brief 更新玩家位置（线程安全）
     */
    void UpdatePosition(uint64_t character_id, float x, float y);

    /**
     * @brief 移除玩家
     */
    void RemoveCharacter(uint64_t character_id);

    /**
     * @brief 获取 AOI 内的玩家ID（9格系统）
     */
    std::vector<uint64_t> GetAOICharacters(float x, float y, float aoi_radius) const;

    /**
     * @brief 获取玩家周围的网格坐标（9格）
     */
    std::vector<GridCoord> GetNearbyGrids(float x, float y, float aoi_radius) const;

    /**
     * @brief 清空所有数据
     */
    void Clear();

private:
    float grid_size_;                                       // 网格大小
    std::unordered_map<GridCoord, GridCell, GridCoordHash> grids_;  // 所有网格
    std::unordered_map<uint64_t, GridCoord> character_grids_; // 玩家ID -> 所在网格
    mutable std::shared_mutex mutex_;                        // 读写锁

    /**
     * @brief 将世界坐标转换为网格坐标
     */
    GridCoord WorldToGrid(float x, float y) const;

    /**
     * @brief 获取网格（线程安全）
     */
    GridCell* GetGrid(const GridCoord& coord);
};

/**
 * @brief 位置广播管理器
 *
 * 负责收集位置更新并广播给周围的玩家
 */
class PositionBroadcaster {
public:
    /**
     * @brief 回调函数类型：发送消息给指定连接
     */
    using SendCallback = std::function<void(uint64_t, const std::vector<uint8_t>&)>;

    PositionBroadcaster(const PositionBroadcastConfig& config,
                     SendCallback send_callback);

    ~PositionBroadcaster();

    /**
     * @brief 更新玩家位置并添加到广播队列
     *
     * @param character_id 玩家ID
     * @param x, y, z 位置坐标
     * @param direction 朝向
     * @param movement_type 移动类型
     * @return 是否添加到队列（如果位置变化小于阈值则不添加）
     */
    bool UpdatePosition(uint64_t character_id, float x, float y, float z,
                     uint16_t direction, uint8_t movement_type);

    /**
     * @brief 移除玩家
     */
    void RemoveCharacter(uint64_t character_id);

    /**
     * @brief 广播所有待发送的位置更新
     *
     * 此方法应定期调用（如每100ms）
     */
    void Broadcast();

    /**
     * @brief 清空所有数据
     */
    void Clear();

    /**
     * @brief 获取统计信息
     */
    struct Statistics {
        size_t total_updates;        // 总更新次数
        size_t broadcast_count;      // 广播次数
        size_t skipped_updates;     // 跳过的更新（位置变化小）
        size_t aoi_characters;      // 当前 AOI 内玩家数
    };

    Statistics GetStatistics() const;

private:
    PositionBroadcastConfig config_;           // 配置
    AOIManager aoi_manager_;                 // AOI 管理器
    SendCallback send_callback_;              // 发送回调

    // 玩家位置缓存（用于检查位置变化）
    struct CharacterPosition {
        float x, y, z;
        uint16_t direction;
        uint8_t movement_type;
        uint64_t timestamp;
    };

    std::unordered_map<uint64_t, CharacterPosition> character_positions_;
    std::unordered_map<uint64_t, PositionUpdate> pending_updates_;

    mutable std::shared_mutex mutex_;          // 读写锁
    Statistics stats_;                        // 统计信息

    /**
     * @brief 序列化位置更新消息
     */
    std::vector<uint8_t> SerializePositionUpdate(const PositionUpdate& update) const;

    /**
     * @brief 批量广播位置更新
     */
    void BroadcastBatch(const std::vector<PositionUpdate>& updates,
                     const std::vector<uint64_t>& target_characters);
};

} // namespace Movement
} // namespace Shared
} // namespace Murim
