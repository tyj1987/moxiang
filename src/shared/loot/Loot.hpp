#pragma once

#include <cstdint>
#include <vector>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <atomic>

namespace Murim {
namespace Game {

// 前向声明
enum class ItemType : uint16_t;
enum class ItemQuality : uint16_t;

/**
 * @brief 掉落物品（单个物品）
 */
struct LootItem {
    uint32_t item_id;      // 物品ID
    uint32_t quantity;     // 数量
    uint16_t quality;      // 品质（用于显示颜色）
    uint64_t owner_id;     // 拾取保护所有者ID（0=无保护）

    LootItem() : item_id(0), quantity(0), quality(1), owner_id(0) {}

    LootItem(uint32_t id, uint32_t qty, uint16_t qual, uint64_t owner = 0)
        : item_id(id), quantity(qty), quality(qual), owner_id(owner) {}
};

/**
 * @brief 地面掉落物
 */
struct LootDrop {
    uint64_t loot_id;              // 掉落物ID（唯一）
    uint64_t source_id;             // 来源ID（死亡的角色/怪物ID）
    float x, y, z;                  // 掉落位置
    uint32_t map_id;                // 地图ID

    std::vector<LootItem> items;    // 掉落物品列表

    std::chrono::system_clock::time_point drop_time;  // 掉落时间
    uint32_t duration;              // 存在时间（秒），0=永久

    // 拾取保护
    uint64_t owner_id;              // 所有者ID（0=无保护）
    uint32_t protect_duration;      // 保护时间（秒）

    /**
     * @brief 检查掉落物是否已过期
     */
    bool IsExpired() const {
        if (duration == 0) {
            return false;  // 永久掉落
        }

        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - drop_time).count();
        return elapsed >= duration;
    }

    /**
     * @brief 检查是否有拾取保护
     */
    bool IsProtected() const {
        if (owner_id == 0) {
            return false;  // 无保护
        }

        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - drop_time).count();
        return elapsed < protect_duration;
    }

    /**
     * @brief 检查玩家是否可以拾取
     */
    bool CanLoot(uint64_t character_id) const {
        if (!IsProtected()) {
            return true;  // 无保护，所有人可拾取
        }
        return character_id == owner_id;  // 有保护，仅所有者可拾取
    }
};

/**
 * @brief 掉落管理器
 */
class LootManager {
public:
    /**
     * @brief 获取单例实例
     */
    static LootManager& Instance();

    /**
     * @brief 生成掉落物
     * @param source_id 来源ID
     * @param x, y, z 掉落位置
     * @param map_id 地图ID
     * @param items 掉落物品列表
     * @param owner_id 所有者ID（0=无保护）
     * @param protect_duration 保护时间（秒）
     * @param duration 掉落存在时间（秒）
     * @return 掉落物ID
     */
    uint64_t CreateLootDrop(
        uint64_t source_id,
        float x, float y, float z,
        uint32_t map_id,
        const std::vector<LootItem>& items,
        uint64_t owner_id = 0,
        uint32_t protect_duration = 30,  // 默认30秒保护
        uint32_t duration = 300          // 默认5分钟存在
    );

    /**
     * @brief 移除掉落物
     */
    bool RemoveLootDrop(uint64_t loot_id);

    /**
     * @brief 获取掉落物
     */
    std::optional<LootDrop> GetLootDrop(uint64_t loot_id);

    /**
     * @brief 查询附近的掉落物
     * @param x, y, z 中心位置
     * @param radius 查询半径
     * @param map_id 地图ID
     */
    std::vector<LootDrop> QueryNearbyLoot(
        float x, float y, float z,
        float radius,
        uint32_t map_id
    );

    /**
     * @brief 清理过期的掉落物
     * @return 清理的数量
     */
    size_t CleanupExpiredLoot();

    /**
     * @brief 生成掉落物ID
     */
    uint64_t GenerateLootId();

private:
    LootManager() = default;
    ~LootManager() = default;

    // 掉落物存储：loot_id -> LootDrop
    std::unordered_map<uint64_t, LootDrop> loot_drops_;

    // 掉落物ID计数器
    std::atomic<uint64_t> next_loot_id_{1};
};

} // namespace Game
} // namespace Murim
