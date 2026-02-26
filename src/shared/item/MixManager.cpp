#include "MixManager.hpp"
#include "../item/ItemManager.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <random>
#include <algorithm>

namespace Murim {
namespace Game {
namespace ItemMix {

// 常量定义 (对应 legacy)
constexpr uint32_t MAX_MIX_PERCENT = 10000;  // 100% (10000 = 100.00%)
constexpr float DEFAULT_BIGFAIL_PROB = 0.2f;  // 普通物品完全失败概率 20%
constexpr float RARE_BIGFAIL_PROB = 0.3f;     // 稀有物品完全失败概率 30%
constexpr float SHOPITEM_BONUS_RATIO = 0.1f;  // 保护道具成功率加成 10%

MixManager& MixManager::Instance() {
    static MixManager instance;
    return instance;
}

void MixManager::LoadRecipes(const std::vector<MixRecipe>& recipes) {
    for (const auto& recipe : recipes) {
        recipes_[recipe.base_item_idx] = recipe;
    }
    spdlog::info("Loaded {} mix recipes", recipes_.size());
}

const MixRecipe* MixManager::GetRecipe(uint32_t base_item_idx) const {
    auto it = recipes_.find(base_item_idx);
    if (it != recipes_.end()) {
        return &it->second;
    }
    return nullptr;
}

uint32_t MixManager::CalculateSuccessRate(
    const MixResult& result,
    uint16_t player_mix_ability_level,
    bool has_shop_item_bonus
) {
    // 基础成功率
    uint32_t ratio = result.success_ratio;

    // 技能等级加成 (每个等级增加 MAX_MIX_PERCENT/100 = 1%)
    uint32_t ability_bonus = player_mix_ability_level * (MAX_MIX_PERCENT / 100);
    ratio += ability_bonus;

    // 保护道具加成 (+10%)
    if (has_shop_item_bonus) {
        ratio += static_cast<uint32_t>(MAX_MIX_PERCENT * SHOPITEM_BONUS_RATIO);
    }

    // 限制范围 [0, 10000]
    ratio = std::min(ratio, MAX_MIX_PERCENT);

    spdlog::debug("Calculated mix success rate: {} (base: {}, ability: {}, bonus: {})",
                  ratio, result.success_ratio, ability_bonus, has_shop_item_bonus);

    return ratio;
}

bool MixManager::VerifyMaterials(
    const std::vector<MixMaterial>& required_materials,
    const std::vector<MixMaterial>& player_materials,
    bool is_dup_item,
    uint32_t durability
) {
    // TODO: 实现材料验证逻辑
    // 1. 遍历所需材料
    // 2. 检查玩家提供的材料是否满足数量要求
    // 3. 对于可堆叠物品，需要考虑耐久度

    return true;
}

MixResultCode MixManager::MixItem(
    uint64_t character_id,
    uint32_t base_item_idx,
    uint16_t base_item_pos,
    uint16_t result_index,
    const std::vector<MixMaterial>& materials,
    uint32_t money_available,
    uint16_t shop_item_idx,
    uint16_t shop_item_pos
) {
    // ========== 第1步: 验证组合技能 ==========
    // TODO: 检查角色是否有组合技能 (eAUKJOB_Mix)
    // if (!CharacterManager::Instance().HasAbility(character_id, eAUKJOB_Mix)) {
    //     return MixResultCode::kNoAbility;
    // }

    // ========== 第2步: 验证基础物品 ==========
    auto base_item_instance = ItemManager::Instance().GetItemInstance(character_id, base_item_pos);
    if (!base_item_instance || base_item_instance->item_id != base_item_idx) {
        spdlog::warn("Mix failed: base item not found (idx: {}, pos: {})", base_item_idx, base_item_pos);
        return MixResultCode::kInvalidItem;
    }

    // ========== 第3步: 验证保护道具 ==========
    bool has_shop_item = false;
    if (shop_item_idx > 0) {
        auto shop_item_instance = ItemManager::Instance().GetItemInstance(character_id, shop_item_pos);
        if (!shop_item_instance || shop_item_instance->item_id != shop_item_idx) {
            spdlog::warn("Mix failed: shop item not found (idx: {}, pos: {})", shop_item_idx, shop_item_pos);
            return MixResultCode::kShopItemNotFound;
        }
        has_shop_item = true;
    }

    // ========== 第4步: 验证材料物品 ==========
    for (const auto& material : materials) {
        if (material.position == base_item_pos) {
            spdlog::warn("Mix failed: material position equals base item position");
            return MixResultCode::kInvalidMaterial;
        }

        auto material_item = ItemManager::Instance().GetItemInstance(character_id, material.position);
        if (!material_item || material_item->item_id != material.item_idx) {
            spdlog::warn("Mix failed: material not found (idx: {}, pos: {})", material.item_idx, material.position);
            return MixResultCode::kInvalidMaterial;
        }
    }

    // ========== 第5步: 获取配方信息 ==========
    const MixRecipe* recipe = GetRecipe(base_item_idx);
    if (!recipe) {
        spdlog::warn("Mix failed: no recipe found for base item idx {}", base_item_idx);
        return MixResultCode::kNoRecipe;
    }

    if (result_index >= recipe->results.size()) {
        spdlog::warn("Mix failed: result index {} out of range (max: {})",
                     result_index, recipe->results.size());
        return MixResultCode::kNoRecipe;
    }

    const MixResult& result = recipe->results[result_index];

    // ========== 第6步: 验证耐久度 ==========
    // 非药水和非宝石物品的耐久度必须为1
    // TODO: 检查物品类型
    if (base_item_instance->durability > 1) {
        spdlog::warn("Mix failed: base item durability too high (durability: {})",
                     base_item_instance->durability);
        return MixResultCode::kDurabilityTooHigh;
    }

    // ========== 第7步: 计算所需金币 ==========
    uint32_t required_money = result.money;
    // TODO: 检查是否为可堆叠物品 (IsDupItem)
    // if (IsDupItem(base_item_idx)) {
    //     required_money *= base_item_instance->durability;
    // }

    if (money_available < required_money) {
        spdlog::warn("Mix failed: insufficient money (required: {}, available: {})",
                     required_money, money_available);
        return MixResultCode::kInsufficientMoney;
    }

    // ========== 第8步: 验证材料数量 ==========
    if (!VerifyMaterials(result.materials, materials, false, base_item_instance->durability)) {
        spdlog::warn("Mix failed: insufficient materials");
        return MixResultCode::kInsufficientMaterial;
    }

    // ========== 第9步: 计算成功率 ==========
    // TODO: 获取玩家的组合技能等级
    uint16_t mix_ability_level = 0;  // ABILITYMGR->GetAbilityLevel(ABILITYINDEX_ITEMMIX, ...)
    uint32_t success_rate = CalculateSuccessRate(result, mix_ability_level, has_shop_item);

    // ========== 第10步: 随机判定 ==========
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, MAX_MIX_PERCENT - 1);
    uint32_t seed = dis(gen);

    bool is_success = (seed < success_rate);

    spdlog::info("Mix roll: seed={}, success_rate={}, success={}", seed, success_rate, is_success);

    // ========== 第11步: 消耗材料 (失败时) ==========
    if (!is_success) {
        // 消耗所有材料
        for (const auto& material : materials) {
            // TODO: 消耗材料物品
            // ItemManager::Instance().DiscardItem(character_id, material.position, material.item_idx, material.durability);
        }
        // 扣除金币
        // TODO: CharacterManager::Instance().SetMoney(character_id, money_available - required_money);
    }

    // ========== 第12步: 消耗保护道具 ==========
    if (has_shop_item) {
        // TODO: 消耗保护道具
        // ItemManager::Instance().DiscardItem(character_id, shop_item_pos, shop_item_idx, 1);
    }

    // ========== 第13步: 处理结果 ==========
    if (is_success) {
        // ========== 成功 ==========
        // 消耗基础物品和材料，给予新物品

        // TODO: 消耗基础物品
        // ItemManager::Instance().DiscardItem(character_id, base_item_pos, base_item_idx, 1);

        // 给予新物品
        uint64_t new_item_instance_id = ItemManager::Instance().CreateItem(
            character_id, result.result_item_idx, 1);

        if (new_item_instance_id == 0) {
            spdlog::error("Mix failed: could not create result item idx {}", result.result_item_idx);
            return MixResultCode::kUpdateFailed;
        }

        spdlog::info("Mix success: created item instance {}", new_item_instance_id);
        return MixResultCode::kSuccess;

    } else {
        // ========== 失败 ==========

        // 计算完全失败概率
        float big_fail_prob = DEFAULT_BIGFAIL_PROB;  // 默认20%
        if (base_item_instance->rare_idx > 0) {
            big_fail_prob = RARE_BIGFAIL_PROB;      // 稀有物品30%
        }

        // 如果使用了保护道具，则不会完全失败
        if (has_shop_item) {
            spdlog::info("Mix failed with protection: base item preserved");
            return MixResultCode::kSmallFailWithProtection;
        }

        // 随机判定是完全失败还是部分失败
        uint32_t fail_seed = dis(gen);
        bool is_big_fail = (fail_seed <= MAX_MIX_PERCENT * big_fail_prob);

        if (is_big_fail) {
            // ========== 完全失败: 销毁基础物品 ==========
            // TODO: 消耗基础物品
            // ItemManager::Instance().DiscardItem(character_id, base_item_pos, base_item_idx, 9999);

            spdlog::info("Mix big fail: base item destroyed (prob: {})", big_fail_prob);
            return MixResultCode::kBigFail;

        } else {
            // ========== 部分失败: 保留基础物品 ==========
            spdlog::info("Mix small fail: base item preserved (prob: {})", 1.0f - big_fail_prob);
            return MixResultCode::kSmallFail;
        }
    }

    return MixResultCode::kUnknownError;
}

} // namespace ItemMix
} // namespace Game
} // namespace Murim
