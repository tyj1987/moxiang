// MurimServer - Equipment Enchant System
// 装备强化系统 - 处理装备强化、宝石镶嵌、属性提升
// 对应legacy: ReinforceManager

#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <string>

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class Player;
class Item;

// ========== Constants ==========
constexpr uint8_t MAX_REINFORCE_LEVEL = 50;
constexpr uint8_t MAX_RARE_REINFORCE_LEVEL = 30;
constexpr float MAX_SUCCESS_RATE = 95.0f;
constexpr float MIN_SUCCESS_RATE = 5.0f;

// ========== Enchant Result ==========
// 强化结果枚举
enum class EnchantResult : uint8_t {
    Success = 0,           // 成功（等级+1）
    Failed = 1,            // 失败（等级不变，材料消失）
    Downgrade = 2,          // 降级（等级-1）
    Destroyed = 3,          // 损毁（装备消失）
    MaterialLost = 4,       // 材料丢失
    Invalid = 5             // 无效操作
};

// ========== Enchant Material Info ==========
// 强化材料信息
struct EnchantMaterial {
    uint32_t item_id;           // 材料物品ID
    uint16_t count;             // 需要数量
    bool is_consumed;           // 是否消耗

    EnchantMaterial()
        : item_id(0), count(0), is_consumed(true) {}

    EnchantMaterial(uint32_t id, uint16_t cnt, bool consumed = true)
        : item_id(id), count(cnt), is_consumed(consumed) {}
};

// ========== Enchant Recipe ==========
// 强化配方
struct EnchantRecipe {
    uint8_t target_level;       // 目标强化等级
    float success_rate;          // 成功率（0-100）
    float downgrade_rate;        // 降级率
    float destroy_rate;          // 损毁率
    uint32_t gold_cost;          // 金币消耗
    std::vector<EnchantMaterial> materials;  // 需要的材料

    EnchantRecipe()
        : target_level(0), success_rate(50.0f),
          downgrade_rate(0.0f), destroy_rate(0.0f),
          gold_cost(0) {}

    // 计算实际成功率（考虑各种加成）
    float CalculateActualSuccessRate(uint16_t ability_value,
                                     uint16_t material_num,
                                     uint16_t item_level) const;
};

// ========== Enchant Info ==========
// 强化信息
struct EnchantInfo {
    uint8_t level;               // 当前强化等级
    uint32_t exp;                // 强化经验（用于稀有强化）
    uint16_t attack_power;       // 攻击力加成
    uint16_t defence;            // 防御力加成
    uint16_t max_hp;             // 最大生命加成
    uint16_t max_mp;             // 最大蓝量加成

    EnchantInfo()
        : level(0), exp(0), attack_power(0),
          defence(0), max_hp(0), max_mp(0) {}

    // 清空
    void Clear() {
        level = 0;
        exp = 0;
        attack_power = 0;
        defence = 0;
        max_hp = 0;
        max_mp = 0;
    }

    // 是否已强化
    bool IsEnchanted() const { return level > 0; }
};

// ========== Jewel Info ==========
// 宝石信息
struct JewelInfo {
    uint32_t item_id;            // 宝石物品ID
    uint8_t grade;               // 宝石等级（1-10）
    uint8_t type;                // 宝石类型
    // 类型: 1=攻击力, 2=防御力, 3=HP, 4=MP, 5=暴击, 6=闪避, 7=命中

    uint16_t value1;             // 效果值1
    uint16_t value2;             // 效果值2

    JewelInfo()
        : item_id(0), grade(1), type(1), value1(0), value2(0) {}

    JewelInfo(uint32_t id, uint8_t gr, uint8_t tp, uint16_t v1, uint16_t v2 = 0)
        : item_id(id), grade(gr), type(tp), value1(v1), value2(v2) {}

    // 获取效果描述
    std::string GetEffectDescription() const;
};

// ========== Equipment Enchant Manager ==========
// 装备强化管理器
class EquipmentEnchantManager {
private:
    // 强化配方表
    std::map<uint8_t, EnchantRecipe> normal_recipes_;    // 普通强化配方
    std::map<uint8_t, EnchantRecipe> rare_recipes_;      // 稀有强化配方

    // 宝石数据
    std::map<uint32_t, JewelInfo> jewel_db_;             // 宝石数据库

    // 强化保护卡
    std::map<uint32_t, float> protect_cards_;            // 保护卡 -> 保护率

public:
    EquipmentEnchantManager();
    ~EquipmentEnchantManager();

    // 初始化
    bool Init();
    void Release();

    // ========== 强化配方 ==========
    void LoadNormalRecipes();
    void LoadRareRecipes();

    const EnchantRecipe* GetNormalRecipe(uint8_t level) const;
    const EnchantRecipe* GetRareRecipe(uint8_t level) const;

    // ========== 强化检查 ==========
    bool CanEnchant(const Item* item, uint8_t target_level, bool is_rare = false) const;
    bool HasEnoughMaterials(const Player* player, const EnchantRecipe& recipe) const;
    bool HasEnoughGold(const Player* player, uint32_t gold_cost) const;

    // ========== 强化执行 ==========
    EnchantResult EnchantItem(Player* player, Item* item, bool is_rare = false);
    EnchantResult EnchantItemWithMaterials(Player* player, Item* item,
                                          const std::vector<EnchantMaterial>& materials,
                                          bool use_protect_card = false);

    // ========== 强化结果处理 ==========
    void ApplyEnchantSuccess(Item* item, uint8_t new_level);
    void ApplyEnchantFailed(Item* item);
    void ApplyEnchantDowngrade(Item* item);
    void ApplyEnchantDestroyed(Item* item);

    // ========== 宝石系统 ==========
    bool LoadJewelDatabase();
    const JewelInfo* GetJewelInfo(uint32_t jewel_id) const;

    bool CanSocketJewel(const Item* item, uint32_t jewel_id) const;
    bool SocketJewel(Player* player, Item* item, uint32_t jewel_id);
    bool RemoveJewel(Item* item, uint8_t socket_index);

    // ========== 属性计算 ==========
    EnchantInfo CalculateEnchantBonus(uint8_t level, uint32_t item_id) const;
    uint16_t CalculateBonusStat(uint8_t level, uint16_t base_stat, float rate) const;

    // ========== 成功率计算 ==========
    float CalculateSuccessRate(const EnchantRecipe& recipe,
                               uint16_t ability_value,
                               uint16_t material_num,
                               uint16_t item_level) const;

    // ========== 保护卡 ==========
    void LoadProtectCards();
    float GetProtectRate(uint32_t card_id) const;
    bool UseProtectCard(Player* player, uint32_t card_id);

    // ========== 辅助函数 ==========
    static EnchantResult RollEnchantResult(float success_rate,
                                          float downgrade_rate,
                                          float destroy_rate,
                                          float protect_rate = 0.0f);

    static const char* GetEnchantResultString(EnchantResult result);
};

// ========== Equipment Enchant Calculator ==========
// 强化计算器 - 计算强化相关的各种数值
class EquipmentEnchantCalculator {
public:
    // ========== 强化表查询 ==========
    // 能力值加成表
    static float GetAbilityBonus(uint16_t ability_value);

    // 材料数量加成表
    static float GetMaterialBonus(uint16_t material_num);

    // 物品等级加成表
    static float GetItemLevelBonus(uint16_t item_level);

    // 调整值表
    static float GetAdjustValue();

    // ========== 综合计算 ==========
    static float CalculateTotalBonus(uint16_t ability_value,
                                     uint16_t material_num,
                                     uint16_t item_level);
};

// ========== Helper Functions ==========

// 检查物品是否可强化
bool IsEnchantableItem(const Item* item);

// 获取装备最大强化等级
uint8_t GetMaxEnchantLevel(uint32_t item_id, bool is_rare);

// 计算强化需要的金币
uint32_t CalculateEnchantGoldCost(uint8_t current_level, uint8_t target_level);

// 计算强化成功率
float CalculateBaseSuccessRate(uint8_t level);

// 获取强化需要的材料
std::vector<EnchantMaterial> GetRequiredMaterials(uint8_t target_level);

} // namespace Game
} // namespace Murim
