#pragma once

#include "../item/Item.hpp"
#include <vector>
#include <optional>

namespace Murim {
namespace Game {
namespace ItemMix {

/**
 * @brief 物品组合材料
 *
 * 对应 legacy: MATERIAL_ARRAY
 */
struct MixMaterial {
    uint32_t item_idx;      // 材料物品ID (wItemIdx)
    uint16_t position;      // 背包位置 (ItemPos)
    uint32_t durability;    // 耐久度 (Dur)

    MixMaterial(uint32_t idx = 0, uint16_t pos = 0, uint32_t dura = 0)
        : item_idx(idx), position(pos), durability(dura) {}
};

/**
 * @brief 物品组合结果信息
 *
 * 对应 legacy: ITEM_MIX_RESULT_INFO
 */
struct MixResult {
    uint32_t result_item_idx;   // 结果物品ID (wResItemIdx)
    uint32_t money;             // 所需金币 (Money)
    uint16_t success_ratio;     // 基础成功率 (SuccessRatio) (0-10000, 10000=100%)
    std::vector<MixMaterial> materials;  // 所需材料列表

    MixResult() : result_item_idx(0), money(0), success_ratio(0) {}
};

/**
 * @brief 物品组合配方
 *
 * 对应 legacy: ITEM_MIX_INFO
 */
struct MixRecipe {
    uint32_t base_item_idx;         // 基础物品ID (wItemIdx)
    std::vector<MixResult> results; // 可能的结果列表

    MixRecipe() : base_item_idx(0) {}
};

/**
 * @brief 物品组合返回码
 *
 * 对应 legacy MixItem 函数返回值
 */
enum class MixResultCode : int {
    kSuccess = 0,               // 成功

    // 错误码
    kNoAbility = 1,             // 无组合技能 (eAUKJOB_Mix)
    kInvalidItem = 2,           // 无效物品
    kInvalidMaterial = 3,       // 无效材料
    kNoRecipe = 4,              // 无配方信息
    kInsufficientMaterial = 5,  // 材料不足
    kInsufficientMoney = 6,     // 金币不足
    kDurabilityTooHigh = 7,     // 耐久度过高
    kDiscardFailed = 8,         // 丢弃物品失败
    kUpdateFailed = 9,          // 更新物品失败
    kUnknownError = 10,         // 未知错误

    // 失败相关
    kShopItemNotFound = 20,     // 保护道具不存在
    kLevelMismatch = 23,        // 等级不匹配

    // 特殊返回码
    kBigFailWithProtection = 21,    // 完全失败但已保护
    kSmallFailWithProtection = 22,  // 部分失败但已保护
    kBigFail = 1000,                 // 完全失败 (基础物品销毁)
    kSmallFail = 1001                // 部分失败 (保留基础物品)
};

/**
 * @brief 物品组合管理器
 *
 * 对应 legacy: CItemManager::MixItem (Lines 2376-2773)
 *
 * 功能:
 * 1. 验证玩家是否有组合技能 (eAUKJOB_Mix)
 * 2. 验证基础物品和材料物品
 * 3. 计算成功率 (基础 + 技能等级 + 保护道具加成)
 * 4. 随机判定成功/失败
 * 5. 处理结果:
 *    - 成功: 消耗基础物品和材料，给予新物品
 *    - 完全失败 (20%): 销毁基础物品和材料
 *    - 部分失败 (80%): 保留基础物品，消耗材料
 *    - 有保护道具: 防止完全失败
 * 6. 稀有物品 (RareIdx > 0) 30%完全失败
 */
class MixManager {
public:
    static MixManager& Instance();

    /**
     * @brief 执行物品组合
     *
     * @param character_id 角色ID
     * @param base_item_idx 基础物品ID
     * @param base_item_pos 基础物品位置
     * @param result_index 结果索引 (psResultItemInfo中的索引)
     * @param materials 材料列表
     * @param money_available 当前金币
     * @param shop_item_idx 保护道具ID (可选)
     * @param shop_item_pos 保护道具位置 (可选)
     * @return MixResultCode 组合结果码
     */
    MixResultCode MixItem(
        uint64_t character_id,
        uint32_t base_item_idx,
        uint16_t base_item_pos,
        uint16_t result_index,
        const std::vector<MixMaterial>& materials,
        uint32_t money_available,
        uint16_t shop_item_idx = 0,
        uint16_t shop_item_pos = 0
    );

    /**
     * @brief 加载组合配方数据
     * @param recipes 配方列表
     */
    void LoadRecipes(const std::vector<MixRecipe>& recipes);

    /**
     * @brief 获取指定物品的配方
     * @param base_item_idx 基础物品ID
     * @return 配方指针，如不存在则返回nullptr
     */
    const MixRecipe* GetRecipe(uint32_t base_item_idx) const;

private:
    MixManager() = default;
    ~MixManager() = default;
    MixManager(const MixManager&) = delete;
    MixManager& operator=(const MixManager&) = delete;

    /**
     * @brief 计算成功率
     *
     * @param result 组合结果信息
     * @param player_mix_ability_level 玩家组合技能等级
     * @param has_shop_item_bonus 是否有保护道具加成
     * @return 成功率 (0-10000, 10000=100%)
     */
    uint32_t CalculateSuccessRate(
        const MixResult& result,
        uint16_t player_mix_ability_level,
        bool has_shop_item_bonus
    );

    /**
     * @brief 验证材料是否足够
     *
     * @param required_materials 所需材料
     * @param player_materials 玩家提供的材料
     * @param is_dup_item 是否为可堆叠物品
     * @param durability 耐久度 (用于可堆叠物品的数量计算)
     * @return true if materials are sufficient
     */
    bool VerifyMaterials(
        const std::vector<MixMaterial>& required_materials,
        const std::vector<MixMaterial>& player_materials,
        bool is_dup_item,
        uint32_t durability
    );

    std::unordered_map<uint32_t, MixRecipe> recipes_;  // 配方列表
};

} // namespace ItemMix
} // namespace Game
} // namespace Murim
