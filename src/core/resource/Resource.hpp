#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace Murim {
namespace Core {
namespace Resource {

/**
 * @brief 怪物数据结构
 */
struct MonsterData {
    uint32_t monster_id;
    uint16_t level;
    uint32_t hp;
    uint32_t mp;
    uint32_t attack_power_min;
    uint32_t attack_power_max;
    uint32_t defence;
    uint32_t magic_defence;
    uint16_t aggro_range;
    uint16_t activity_range;
    float move_speed;
    float attack_speed;
    uint32_t exp_reward;
    uint32_t drop_id;
};

/**
 * @brief Boss 怪物数据
 */
struct BossMonsterData {
    uint32_t boss_id;
    std::string boss_name;
    uint16_t level;
    uint32_t hp;
    uint32_t attack_power;
    uint32_t respawn_time;
    float drop_rate_multiplier;
};

/**
 * @brief 物品兑换数据
 */
struct ItemChangeData {
    uint32_t change_id;
    uint32_t source_item_id;
    uint16_t source_quantity;
    uint32_t target_item_id;
    uint16_t target_quantity;
    uint16_t required_level;
};

/**
 * @brief 资源管理器
 *
 * 负责加载和管理游戏资源文件 (.bin 格式)
 */
class ResourceManager {
public:
    /**
     * @brief 获取单例实例
     */
    static ResourceManager& Instance();

    // ========== 初始化 ==========

    /**
     * @brief 初始化资源管理器
     * @param base_path 资源文件基础路径 (默认 "Resource")
     * @return 是否初始化成功
     */
    bool Initialize(const std::string& base_path = "Resource");

    /**
     * @brief 关闭资源管理器
     */
    void Shutdown();

    // ========== 路径配置 ==========

    /**
     * @brief 设置资源路径
     */
    void SetResourcePath(const std::string& path);

    /**
     * @brief 设置服务器资源路径
     */
    void SetServerResourcePath(const std::string& path);

    // ========== 怪物数据加载 ==========

    /**
     * @brief 加载指定地图的怪物数据
     * @param map_id 地图ID
     * @return 是否加载成功
     */
    bool LoadMonsterData(uint16_t map_id);

    /**
     * @brief 加载所有怪物数据
     * @return 加载的怪物数据数量
     */
    size_t LoadAllMonsterData();

    /**
     * @brief 获取怪物数据
     * @param monster_id 怪物ID
     * @return 怪物数据（如果存在）
     */
    const MonsterData* GetMonsterData(uint32_t monster_id) const;

    /**
     * @brief 获取地图的所有怪物数据
     * @param map_id 地图ID
     * @return 怪物数据列表
     */
    std::vector<MonsterData> GetMapMonsters(uint16_t map_id) const;

    // ========== Boss 怪物加载 ==========

    /**
     * @brief 加载 Boss 怪物数据
     * @return 是否加载成功
     */
    bool LoadBossData();

    /**
     * @brief 获取 Boss 数据
     * @param boss_id Boss ID
     * @return Boss 数据（如果存在）
     */
    const BossMonsterData* GetBossData(uint32_t boss_id) const;

    /**
     * @brief 获取所有 Boss 数据
     * @return Boss 数据列表
     */
    std::vector<BossMonsterData> GetAllBossData() const;

    // ========== 物品兑换加载 ==========

    /**
     * @brief 加载物品兑换数据
     * @return 是否加载成功
     */
    bool LoadItemChangeData();

    /**
     * @brief 获取物品兑换数据
     * @param item_id 物品ID
     * @return 兑换数据列表
     */
    std::vector<ItemChangeData> GetItemChanges(uint32_t item_id) const;

    // ========== 文件验证 ==========

    /**
     * @brief 验证资源文件是否存在
     * @param filename 文件名
     * @return 文件是否存在
     */
    bool FileExists(const std::string& filename) const;

    /**
     * @brief 获取完整文件路径
     * @param filename 文件名
     * @return 完整路径
     */
    std::string GetFullPath(const std::string& filename) const;

    // ========== 状态查询 ==========

    /**
     * @brief 获取已加载的怪物数量
     */
    size_t GetLoadedMonsterCount() const { return monster_data_map_.size(); }

    /**
     * @brief 获取已加载的 Boss 数量
     */
    size_t GetLoadedBossCount() const { return boss_data_map_.size(); }

    /**
     * @brief 获取已加载的物品兑换数量
     */
    size_t GetLoadedItemChangeCount() const { return item_change_map_.size(); }

    /**
     * @brief 是否已初始化
     */
    bool IsInitialized() const { return initialized_; }

private:
    ResourceManager();
    ~ResourceManager() = default;
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    /**
     * @brief 读取二进制文件
     * @param filename 文件名
     * @param buffer 输出缓冲区
     * @return 是否读取成功
     */
    bool ReadBinaryFile(const std::string& filename, std::vector<uint8_t>& buffer) const;

    // ========== 成员变量 ==========

    bool initialized_ = false;
    std::string base_path_;
    std::string server_path_;

    // 怪物数据映射 (monster_id -> MonsterData)
    std::unordered_map<uint32_t, MonsterData> monster_data_map_;

    // Boss 数据映射 (boss_id -> BossMonsterData)
    std::unordered_map<uint32_t, BossMonsterData> boss_data_map_;

    // 物品兑换映射 (source_item_id -> vector<ItemChangeData>)
    std::unordered_map<uint32_t, std::vector<ItemChangeData>> item_change_map_;
};

} // namespace Resource
} // namespace Core
} // namespace Murim
