#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include "protocol/title.pb.h"

namespace Murim {
namespace Game {

/**
 * @brief 称号定义（C++版本）
 */
struct TitleDefinition {
    uint32_t title_id;
    std::string name;
    std::string description;
    murim::TitleCategory category;
    murim::TitleDisplayLocation display_location;
    bool permanent;
    uint32_t duration_days;

    // 属性加成
    int32_t hp_bonus;
    int32_t mp_bonus;
    int32_t attack_bonus;
    int32_t defense_bonus;
    int32_t magic_attack_bonus;
    int32_t magic_defense_bonus;
    int32_t speed_bonus;
    int32_t critical_bonus;
    int32_t all_stats_bonus;

    // 获取条件类型
    enum class RequirementType {
        NONE,
        ACHIEVEMENT,
        LEVEL,
        WEALTH,
        KILL,
        QUEST,
        SPECIAL
    };
    RequirementType requirement_type;

    // 条件数据
    struct RequirementData {
        uint32_t achievement_id;
        uint32_t target_level;
        uint64_t target_gold;
        uint32_t monster_type_id;
        uint32_t kill_count;
        uint32_t quest_id;
        std::string condition;
    } requirement;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::TitleDefinition ToProto() const;

    /**
     * @brief 检查是否满足获取条件
     */
    bool CheckRequirement(uint64_t character_id) const;
};

/**
 * @brief 玩家称号数据
 */
struct PlayerTitle {
    uint32_t title_id;
    bool owned;
    uint64_t obtain_time;
    uint64_t expire_time;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::PlayerTitle ToProto() const;
};

/**
 * @brief 称号系统管理器
 *
 * 负责称号的定义、获取、装备、属性加成
 */
class TitleManager {
public:
    static TitleManager& Instance();

    bool Initialize();

    // ========== 称号定义管理 ==========

    /**
     * @brief 注册称号定义
     */
    bool RegisterTitle(const TitleDefinition& definition);

    /**
     * @brief 获取称号定义
     */
    TitleDefinition* GetTitleDefinition(uint32_t title_id);

    /**
     * @brief 获取所有称号定义
     */
    std::vector<TitleDefinition> GetAllTitles();

    /**
     * @brief 按类别获取称号
     */
    std::vector<TitleDefinition> GetTitlesByCategory(murim::TitleCategory category);

    // ========== 玩家称号管理 ==========

    /**
     * @brief 获取玩家拥有的称号列表
     */
    std::vector<PlayerTitle>* GetPlayerTitles(uint64_t character_id);

    /**
     * @brief 检查玩家是否拥有称号
     */
    bool HasTitle(uint64_t character_id, uint32_t title_id);

    /**
     * @brief 给予称号
     */
    bool GrantTitle(uint64_t character_id, uint32_t title_id, uint32_t duration_days = 0);

    /**
     * @brief 移除称号
     */
    bool RemoveTitle(uint64_t character_id, uint32_t title_id);

    // ========== 称号装备 ==========

    /**
     * @brief 装备称号
     */
    bool EquipTitle(uint64_t character_id, uint32_t title_id);

    /**
     * @brief 卸载称号
     */
    bool UnequipTitle(uint64_t character_id);

    /**
     * @brief 获取当前装备的称号
     */
    uint32_t GetEquippedTitle(uint64_t character_id);

    /**
     * @brief 获取带称号的显示名称
     */
    std::string GetDisplayName(uint64_t character_id, const std::string& character_name);

    // ========== 称号属性加成 ==========

    /**
     * @brief 计算称号属性加成
     */
    void CalculateTitleBonus(uint64_t character_id,
                             int32_t& hp_bonus,
                             int32_t& mp_bonus,
                             int32_t& attack_bonus,
                             int32_t& defense_bonus);

    /**
     * @brief 应用称号属性加成
     */
    void ApplyTitleBonus(uint64_t character_id);

    /**
     * @brief 移除称号属性加成
     */
    void RemoveTitleBonus(uint64_t character_id);

    // ========== 维护操作 ==========

    /**
     * @brief 检查并移除过期称号
     */
    void CheckExpiredTitles();

    /**
     * @brief 保存玩家称号数据
     */
    bool SavePlayerTitles(uint64_t character_id);

    /**
     * @brief 加载玩家称号数据
     */
    bool LoadPlayerTitles(uint64_t character_id);

private:
    TitleManager();
    ~TitleManager() = default;
    TitleManager(const TitleManager&) = delete;
    TitleManager& operator=(const TitleManager&) = delete;

    // ========== 称号定义存储 ==========
    // title_id -> TitleDefinition
    std::unordered_map<uint32_t, TitleDefinition> title_definitions_;

    // ========== 玩家称号数据 ==========
    // character_id -> PlayerTitle list
    std::unordered_map<uint64_t, std::vector<PlayerTitle>> player_titles_;

    // ========== 装备称号 ==========
    // character_id -> equipped title_id
    std::unordered_map<uint64_t, uint32_t> equipped_titles_;

    // ========== 辅助方法 ==========

    /**
     * @brief 检查称号是否可装备
     */
    bool CanEquip(uint64_t character_id, uint32_t title_id);

    /**
     * @brief 发送称号获取通知
     */
    void NotifyTitleObtained(uint64_t character_id, const TitleDefinition& title);

    /**
     * @brief 发送称号过期通知
     */
    void NotifyTitleExpired(uint64_t character_id, const TitleDefinition& title);
};

} // namespace Game
} // namespace Murim
