#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include "protocol/pet.pb.h"

namespace Murim {
namespace Game {

/**
 * @brief 宠物定义（C++版本）
 */
struct PetDefinition {
    uint32_t pet_id;
    std::string name;
    std::string description;
    murim::PetQuality quality;
    murim::PetGrowthType growth_type;

    // 基础属性
    uint32_t base_hp;
    uint32_t base_mp;
    uint32_t base_attack;
    uint32_t base_defense;
    uint32_t base_magic_attack;
    uint32_t base_magic_defense;
    uint32_t base_speed;

    // 成长率
    float hp_growth;
    float mp_growth;
    float attack_growth;
    float defense_growth;
    float magic_attack_growth;
    float magic_defense_growth;
    float speed_growth;

    // 其他属性
    uint32_t capture_rate;
    uint32_t max_level;
    uint32_t evolve_level;
    uint32_t evolve_to;

    // 战斗属性
    uint32_t attack_range;
    uint32_t skill_id;
    std::vector<uint32_t> skill_ids;

    // 外观
    uint32_t model_id;
    uint32_t icon_id;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::PetDefinition ToProto() const;
};

/**
 * @brief 宠物数据
 */
struct PetData {
    uint64_t pet_unique_id;          // 唯一ID
    uint32_t pet_id;                 // 定义ID
    std::string name;                // 自定义名称
    uint32_t level;                  // 等级
    uint64_t exp;                    // 经验值
    murim::PetState state;           // 状态

    // 当前属性
    uint32_t hp;
    uint32_t mp;
    uint32_t max_hp;
    uint32_t max_mp;
    uint32_t attack;
    uint32_t defense;
    uint32_t magic_attack;
    uint32_t magic_defense;
    uint32_t speed;

    // 成长属性
    uint32_t potential;              // 潜能点
    uint32_t loyalty;                // 忠诚度
    uint32_t intimacy;               // 亲密度

    // 装备
    uint32_t equipment_id;

    // 技能
    std::vector<uint32_t> learned_skills;

    // 时间戳
    uint64_t birth_time;
    uint64_t last_feed_time;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::PetData ToProto() const;

    /**
     * @brief 计算升级所需经验
     */
    uint64_t GetExpForNextLevel() const;

    /**
     * @brief 检查是否可以进化
     */
    bool CanEvolve() const;
};

/**
 * @brief 宠物系统管理器
 *
 * 负责宠物的捕捉、养成、进化、战斗
 */
class PetManager {
public:
    static PetManager& Instance();

    bool Initialize();

    // ========== 宠物定义管理 ==========

    /**
     * @brief 注册宠物定义
     */
    bool RegisterPet(const PetDefinition& definition);

    /**
     * @brief 获取宠物定义
     */
    PetDefinition* GetPetDefinition(uint32_t pet_id);

    /**
     * @brief 获取所有宠物定义
     */
    std::vector<PetDefinition> GetAllPetDefinitions();

    // ========== 玩家宠物管理 ==========

    /**
     * @brief 获取玩家宠物列表
     */
    std::vector<PetData>* GetPlayerPets(uint64_t character_id);

    /**
     * @brief 获取玩家宠物
     */
    PetData* GetPet(uint64_t character_id, uint64_t pet_unique_id);

    /**
     * @brief 捕捉宠物
     */
    PetData* CapturePet(uint64_t character_id, uint32_t pet_id, uint32_t monster_level);

    /**
     * @brief 放生宠物
     */
    bool ReleasePet(uint64_t character_id, uint64_t pet_unique_id);

    /**
     * @brief 重命名宠物
     */
    bool RenamePet(uint64_t character_id, uint64_t pet_unique_id, const std::string& new_name);

    // ========== 宠物召唤 ==========

    /**
     * @brief 召唤宠物
     */
    bool SummonPet(uint64_t character_id, uint64_t pet_unique_id);

    /**
     * @brief 召回宠物
     */
    bool UnsummonPet(uint64_t character_id);

    /**
     * @brief 获取当前召唤的宠物
     */
    PetData* GetSummonedPet(uint64_t character_id);

    // ========== 宠物养成 ==========

    /**
     * @brief 增加宠物经验
     */
    bool AddPetExp(uint64_t character_id, uint64_t pet_unique_id, uint64_t exp_amount);

    /**
     * @brief 升级宠物
     */
    bool LevelUpPet(uint64_t character_id, uint64_t pet_unique_id, uint64_t exp_amount);

    /**
     * @brief 培养宠物（使用潜能点）
     */
    bool TrainPet(uint64_t character_id, uint64_t pet_unique_id, uint32_t stat_type, uint32_t points);

    /**
     * @brief 进化宠物
     */
    bool EvolvePet(uint64_t character_id, uint64_t pet_unique_id);

    /**
     * @brief 喂食宠物
     */
    bool FeedPet(uint64_t character_id, uint64_t pet_unique_id, uint32_t food_id, uint32_t count);

    // ========== 宠物技能 ==========

    /**
     * @brief 学习技能
     */
    bool LearnSkill(uint64_t character_id, uint64_t pet_unique_id, uint32_t skill_id);

    /**
     * @brief 检查是否已学习技能
     */
    bool HasLearnedSkill(uint64_t character_id, uint64_t pet_unique_id, uint32_t skill_id);

    // ========== 宠物装备 ==========

    /**
     * @brief 装备宠物装备
     */
    bool EquipItem(uint64_t character_id, uint64_t pet_unique_id, uint32_t equipment_id);

    /**
     * @brief 卸下宠物装备
     */
    bool UnequipItem(uint64_t character_id, uint64_t pet_unique_id);

    // ========== 宠物战斗 ==========

    /**
     * @brief 宠物受到伤害
     */
    bool TakeDamage(uint64_t character_id, uint64_t pet_unique_id, uint32_t damage);

    /**
     * @brief 宠物死亡
     */
    bool OnPetDead(uint64_t character_id, uint64_t pet_unique_id);

    /**
     * @brief 复活宠物
     */
    bool RevivePet(uint64_t character_id, uint64_t pet_unique_id);

    // ========== 辅助方法 ==========

    /**
     * @brief 生成唯一宠物ID
     */
    uint64_t GenerateUniquePetId();

    /**
     * @brief 计算宠物属性
     */
    void CalculatePetStats(PetData& pet, const PetDefinition& definition);

    /**
     * @brief 检查宠物是否已满
     */
    bool IsPetSlotFull(uint64_t character_id);

    /**
     * @brief 发送宠物获得通知
     */
    void NotifyPetObtained(uint64_t character_id, const PetData& pet);

    /**
     * @brief 发送宠物升级通知
     */
    void NotifyPetLevelUp(uint64_t character_id, const PetData& pet, uint32_t old_level);

private:
    PetManager();
    ~PetManager() = default;
    PetManager(const PetManager&) = delete;
    PetManager& operator=(const PetManager&) = delete;

    // ========== 宠物定义存储 ==========
    // pet_id -> PetDefinition
    std::unordered_map<uint32_t, PetDefinition> pet_definitions_;

    // ========== 玩家宠物数据 ==========
    // character_id -> PetData list
    std::unordered_map<uint64_t, std::vector<PetData>> player_pets_;

    // ========== 召唤的宠物 ==========
    // character_id -> pet_unique_id
    std::unordered_map<uint64_t, uint64_t> summoned_pets_;

    // ========== 唯一ID生成器 ==========
    uint64_t next_unique_pet_id_;

    // ========== 配置参数 ==========
    uint32_t max_pet_slots_;          // 最大宠物栏位数
    uint32_t base_exp_gain_;          // 基础经验获取
    uint32_t intimacy_decay_rate_;    // 亲密度衰减率（每小时）
};

} // namespace Game
} // namespace Murim
