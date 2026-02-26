#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include "protocol/mount.pb.h"

namespace Murim {
namespace Game {

/**
 * @brief 坐骑定义（C++版本）
 */
struct MountDefinition {
    uint32_t mount_id;
    std::string name;
    std::string description;
    murim::MountQuality quality;
    murim::MountType type;

    // 基础属性
    uint32_t base_speed_bonus;          // 基础速度加成（%）
    uint32_t max_star_level;            // 最大星级
    uint32_t evolve_mount_id;           // 进化后的坐骑ID

    // 星级属性加成
    std::vector<uint32_t> speed_bonus_per_star;  // 每星速度加成

    // 技能
    uint32_t skill_id;
    uint32_t skill_unlock_star;

    // 外观
    uint32_t model_id;
    uint32_t icon_id;
    std::string mesh_name;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::MountDefinition ToProto() const;

    /**
     * @brief 获取指定星级速度加成
     */
    uint32_t GetSpeedBonus(uint32_t star_level) const;
};

/**
 * @brief 玩家坐骑数据
 */
struct PlayerMount {
    uint32_t mount_id;                  // 坐骑ID
    uint32_t star_level;                // 星级（1-5）
    murim::MountState state;           // 状态
    uint64_t exp;                       // 进化经验

    // 装备
    uint32_t saddle_id;                 // 鞍具ID
    uint32_t decoration_id;             // 装饰ID

    // 技能
    bool skill_unlocked;               // 技能是否解锁
    uint32_t skill_id;                  // 已解锁的技能ID

    // 获得时间
    uint64_t obtain_time;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::MountData ToProto() const;

    /**
     * @brief 计算进化所需经验
     */
    uint64_t GetExpForNextStar() const;

    /**
     * @brief 检查是否可以进化
     */
    bool CanEvolve() const;
};

/**
 * @brief 坐骑系统管理器
 *
 * 负责坐骑的获取、骑乘、进化、装备管理
 */
class MountManager {
public:
    static MountManager& Instance();

    bool Initialize();

    // ========== 坐骑定义管理 ==========

    /**
     * @brief 注册坐骑定义
     */
    bool RegisterMount(const MountDefinition& definition);

    /**
     * @brief 获取坐骑定义
     */
    MountDefinition* GetMountDefinition(uint32_t mount_id);

    /**
     * @brief 获取所有坐骑定义
     */
    std::vector<MountDefinition> GetAllMountDefinitions();

    // ========== 玩家坐骑管理 ==========

    /**
     * @brief 获取玩家坐骑列表
     */
    std::vector<PlayerMount>* GetPlayerMounts(uint64_t character_id);

    /**
     * @brief 获取玩家坐骑
     */
    PlayerMount* GetMount(uint64_t character_id, uint32_t mount_id);

    /**
     * @brief 检查玩家是否拥有坐骑
     */
    bool HasMount(uint64_t character_id, uint32_t mount_id);

    /**
     * @brief 给予坐骑
     */
    bool GrantMount(uint64_t character_id, uint32_t mount_id);

    // ========== 坐骑骑乘 ==========

    /**
     * @brief 骑乘坐骑
     */
    bool RideMount(uint64_t character_id, uint32_t mount_id);

    /**
     * @brief 下马
     */
    bool UnrideMount(uint64_t character_id);

    /**
     * @brief 获取当前骑乘的坐骑
     */
    PlayerMount* GetRiddenMount(uint64_t character_id);

    /**
     * @brief 获取当前速度加成
     */
    uint32_t GetCurrentSpeedBonus(uint64_t character_id);

    // ========== 坐骑进化 ==========

    /**
     * @brief 增加进化经验
     */
    bool AddExp(uint64_t character_id, uint32_t mount_id, uint64_t exp_amount);

    /**
     * @brief 进化坐骑（升星）
     */
    bool EvolveMount(uint64_t character_id, uint32_t mount_id, uint32_t material_count);

    // ========== 坐骑技能 ==========

    /**
     * @brief 解锁技能
     */
    bool UnlockSkill(uint64_t character_id, uint32_t mount_id);

    /**
     * @brief 检查技能是否已解锁
     */
    bool IsSkillUnlocked(uint64_t character_id, uint32_t mount_id);

    // ========== 坐骑装备 ==========

    /**
     * @brief 装备鞍具
     */
    bool EquipSaddle(uint64_t character_id, uint32_t mount_id, uint32_t saddle_id);

    /**
     * @brief 卸下鞍具
     */
    bool UnequipSaddle(uint64_t character_id, uint32_t mount_id);

    /**
     * @brief 装备装饰
     */
    bool EquipDecoration(uint64_t character_id, uint32_t mount_id, uint32_t decoration_id);

    /**
     * @brief 卸下装饰
     */
    bool UnequipDecoration(uint64_t character_id, uint32_t mount_id);

    // ========== 辅助方法 ==========

    /**
     * @brief 计算坐骑总速度加成
     */
    uint32_t CalculateTotalSpeedBonus(uint64_t character_id, const PlayerMount& mount);

    /**
     * @brief 发送坐骑获得通知
     */
    void NotifyMountObtained(uint64_t character_id, const PlayerMount& mount);

    /**
     * @brief 发送坐骑进化通知
     */
    void NotifyMountEvolved(uint64_t character_id, const PlayerMount& mount, uint32_t old_star);

    /**
     * @brief 发送技能解锁通知
     */
    void NotifySkillUnlocked(uint64_t character_id, const PlayerMount& mount);

private:
    MountManager();
    ~MountManager() = default;
    MountManager(const MountManager&) = delete;
    MountManager& operator=(const MountManager&) = delete;

    // ========== 坐骑定义存储 ==========
    // mount_id -> MountDefinition
    std::unordered_map<uint32_t, MountDefinition> mount_definitions_;

    // ========== 玩家坐骑数据 ==========
    // character_id -> PlayerMount list
    std::unordered_map<uint64_t, std::vector<PlayerMount>> player_mounts_;

    // ========== 骑乘的坐骑 ==========
    // character_id -> mount_id
    std::unordered_map<uint64_t, uint32_t> ridden_mounts_;

    // ========== 配置参数 ==========
    uint32_t max_mount_slots_;           // 最大坐骑栏位数
    uint32_t base_evolve_exp_;           // 基础进化经验
    uint32_t evolve_material_per_star_;  // 每星所需材料
};

} // namespace Game
} // namespace Murim
