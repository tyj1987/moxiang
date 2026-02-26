// MurimServer - Equipment Enchant Implementation
// 装备强化系统实现

#include "EquipmentEnchant.hpp"
#include "../game/Player.hpp"
#include "../game/Item.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Murim {
namespace Game {

// ========== Helper Functions ==========

std::string JewelInfo::GetEffectDescription() const {
    switch (type) {
        case 1: return "攻击力+" + std::to_string(value1);
        case 2: return "防御力+" + std::to_string(value1);
        case 3: return "HP+" + std::to_string(value1);
        case 4: return "MP+" + std::to_string(value1);
        case 5: return "暴击+" + std::to_string(value1) + "%";
        case 6: return "闪避+" + std::to_string(value1) + "%";
        case 7: return "命中+" + std::to_string(value1) + "%";
        default: return "未知效果";
    }
}

float EnchantRecipe::CalculateActualSuccessRate(uint16_t ability_value,
                                                 uint16_t material_num,
                                                 uint16_t item_level) const {
    float base_rate = success_rate;

    // 应用各种加成
    float ability_bonus = EquipmentEnchantCalculator::GetAbilityBonus(ability_value);
    float material_bonus = EquipmentEnchantCalculator::GetMaterialBonus(material_num);
    float item_level_bonus = EquipmentEnchantCalculator::GetItemLevelBonus(item_level);
    float adjust = EquipmentEnchantCalculator::GetAdjustValue();

    float final_rate = base_rate + ability_bonus + material_bonus + item_level_bonus + adjust;

    // 限制在5%-95%之间
    final_rate = std::max(MIN_SUCCESS_RATE, std::min(MAX_SUCCESS_RATE, final_rate));

    return final_rate;
}

// ========== EquipmentEnchantManager ==========

EquipmentEnchantManager::EquipmentEnchantManager() {
}

EquipmentEnchantManager::~EquipmentEnchantManager() {
    Release();
}

bool EquipmentEnchantManager::Init() {
    LoadNormalRecipes();
    LoadRareRecipes();
    LoadJewelDatabase();
    LoadProtectCards();

    spdlog::info("EquipmentEnchantManager: Initialized");
    return true;
}

void EquipmentEnchantManager::Release() {
    normal_recipes_.clear();
    rare_recipes_.clear();
    jewel_db_.clear();
    protect_cards_.clear();
}

void EquipmentEnchantManager::LoadNormalRecipes() {
    // 加载普通强化配方（1-50级）
    for (uint8_t level = 1; level <= MAX_REINFORCE_LEVEL; ++level) {
        EnchantRecipe recipe;
        recipe.target_level = level;

        // 基础成功率：随着等级递减
        // 1-10级: 100%
        // 11-20级: 90% -> 50%
        // 21-30级: 45% -> 20%
        // 31-40级: 18% -> 8%
        // 41-50级: 7% -> 5%
        if (level <= 10) {
            recipe.success_rate = 100.0f;
        } else if (level <= 20) {
            recipe.success_rate = 90.0f - (level - 10) * 4.0f;  // 90% -> 50%
        } else if (level <= 30) {
            recipe.success_rate = 45.0f - (level - 20) * 2.5f;  // 45% -> 20%
        } else if (level <= 40) {
            recipe.success_rate = 18.0f - (level - 30) * 1.0f;  // 18% -> 8%
        } else {
            recipe.success_rate = 7.0f - (level - 40) * 0.2f;   // 7% -> 5%
        }

        // 降级率：20级以上开始有降级风险
        if (level > 20) {
            recipe.downgrade_rate = (level - 20) * 1.5f;  // 最高45%
            recipe.downgrade_rate = std::min(recipe.downgrade_rate, 45.0f);
        }

        // 损毁率：30级以上开始有损毁风险
        if (level > 30) {
            recipe.destroy_rate = (level - 30) * 2.0f;  // 最高40%
            recipe.destroy_rate = std::min(recipe.destroy_rate, 40.0f);
        }

        // 金币消耗
        recipe.gold_cost = 1000 * level * level;  // 等级平方 × 1000

        // 材料需求
        uint16_t material_count = (level + 4) / 5;  // 每5级增加1个材料
        recipe.materials.push_back(EnchantMaterial(50000, material_count));  // 示例材料ID

        normal_recipes_[level] = recipe;
    }

    spdlog::info("EquipmentEnchantManager: Loaded {} normal reinforcement recipes",
                 normal_recipes_.size());
}

void EquipmentEnchantManager::LoadRareRecipes() {
    // 加载稀有强化配方（1-30级）
    for (uint8_t level = 1; level <= MAX_RARE_REINFORCE_LEVEL; ++level) {
        EnchantRecipe recipe;
        recipe.target_level = level;

        // 稀有强化成功率更高
        if (level <= 10) {
            recipe.success_rate = 100.0f;
        } else if (level <= 20) {
            recipe.success_rate = 95.0f - (level - 10) * 3.0f;  // 95% -> 65%
        } else {
            recipe.success_rate = 65.0f - (level - 20) * 2.0f;  // 65% -> 45%
        }

        // 稀有强化不会降级或损毁
        recipe.downgrade_rate = 0.0f;
        recipe.destroy_rate = 0.0f;

        // 金币消耗更高
        recipe.gold_cost = 2000 * level * level;

        // 需要稀有材料
        recipe.materials.push_back(EnchantMaterial(50001, 1));  // 稀有材料

        rare_recipes_[level] = recipe;
    }

    spdlog::info("EquipmentEnchantManager: Loaded {} rare reinforcement recipes",
                 rare_recipes_.size());
}

const EnchantRecipe* EquipmentEnchantManager::GetNormalRecipe(uint8_t level) const {
    auto it = normal_recipes_.find(level);
    return (it != normal_recipes_.end()) ? &it->second : nullptr;
}

const EnchantRecipe* EquipmentEnchantManager::GetRareRecipe(uint8_t level) const {
    auto it = rare_recipes_.find(level);
    return (it != rare_recipes_.end()) ? &it->second : nullptr;
}

bool EquipmentEnchantManager::CanEnchant(const Item* item, uint8_t target_level, bool is_rare) const {
    if (!item) {
        return false;
    }

    // 检查是否可强化物品
    if (!IsEnchantableItem(item)) {
        return false;
    }

    // 获取当前强化等级
    uint8_t current_level = item->GetEnchantLevel();

    // 检查目标等级
    if (target_level != current_level + 1) {
        return false;
    }

    // 检查最大等级
    uint8_t max_level = GetMaxEnchantLevel(item->GetItemId(), is_rare);
    if (target_level > max_level) {
        return false;
    }

    return true;
}

bool EquipmentEnchantManager::HasEnoughMaterials(const Player* player, const EnchantRecipe& recipe) const {
    if (!player) {
        return false;
    }

    for (const auto& material : recipe.materials) {
        uint32_t count = player->GetItemCount(material.item_id);
        if (count < material.count) {
            return false;
        }
    }

    return true;
}

bool EquipmentEnchantManager::HasEnoughGold(const Player* player, uint32_t gold_cost) const {
    if (!player) {
        return false;
    }

    return player->GetGold() >= gold_cost;
}

EnchantResult EquipmentEnchantManager::EnchantItem(Player* player, Item* item, bool is_rare) {
    if (!player || !item) {
        return EnchantResult::Invalid;
    }

    uint8_t current_level = item->GetEnchantLevel();
    uint8_t target_level = current_level + 1;

    // 获取配方
    const EnchantRecipe* recipe = is_rare ? GetRareRecipe(target_level) : GetNormalRecipe(target_level);
    if (!recipe) {
        return EnchantResult::Invalid;
    }

    // 检查材料
    if (!HasEnoughMaterials(player, *recipe)) {
        return EnchantResult::MaterialLost;
    }

    // 检查金币
    if (!HasEnoughGold(player, recipe->gold_cost)) {
        return EnchantResult::Failed;
    }

    // 消耗材料
    for (const auto& material : recipe->materials) {
        player->RemoveItem(material.item_id, material.count);
    }

    // 消耗金币
    player->RemoveGold(recipe->gold_cost);

    // 计算成功率
    // 获取能力值（影响成功率的玩家属性，如智力、运气等）
    // 默认使用等级相关的值，后续可以从Player属性系统获取
    uint16_t ability_value = player->GetLevel() * 10;  // 简化公式：等级×10
    uint16_t material_num = static_cast<uint16_t>(recipe->materials.size());
    uint16_t item_level = item->GetItemLevel();

    float success_rate = CalculateSuccessRate(*recipe, ability_value, material_num, item_level);

    // 掷骰子
    EnchantResult result = RollEnchantResult(
        success_rate,
        recipe->downgrade_rate,
        recipe->destroy_rate
    );

    // 应用结果
    switch (result) {
        case EnchantResult::Success:
            ApplyEnchantSuccess(item, target_level);
            spdlog::info("[Enchant-{}] Success: Item {} -> level {}",
                        player->GetObjectId(), item->GetItemId(), target_level);
            break;

        case EnchantResult::Failed:
            ApplyEnchantFailed(item);
            spdlog::info("[Enchant-{}] Failed: Item {} at level {}",
                        player->GetObjectId(), item->GetItemId(), current_level);
            break;

        case EnchantResult::Downgrade:
            ApplyEnchantDowngrade(item);
            spdlog::warn("[Enchant-{}] Downgrade: Item {} -> level {}",
                         player->GetObjectId(), item->GetItemId(), current_level - 1);
            break;

        case EnchantResult::Destroyed:
            ApplyEnchantDestroyed(item);
            spdlog::warn("[Enchant-{}] Destroyed: Item {}",
                         player->GetObjectId(), item->GetItemId());
            break;

        default:
            break;
    }

    return result;
}

EnchantResult EquipmentEnchantManager::EnchantItemWithMaterials(
    Player* player, Item* item,
    const std::vector<EnchantMaterial>& materials,
    bool use_protect_card) {

    if (!player || !item || materials.empty()) {
        return EnchantResult::Invalid;
    }

    // TODO [需配置系统]: 实现使用指定材料的强化
    // 需要支持：
    // 1. 自定义材料列表（不同材料提供不同加成）
    // 2. 材料加成配置表（material_id -> bonus_rate）
    // 3. 保护卡系统集成
    // 4. 特殊材料效果（如必成功材料）
    //
    // 示例配置格式：
    // {
    //   "custom_materials": {
    //     "50001": { "bonus_rate": 5.0, "protect": false },
    //     "50002": { "bonus_rate": 10.0, "protect": true }
    //   }
    // }
    //
    // 这里简化处理，调用普通强化
    return EnchantItem(player, item, false);
}

void EquipmentEnchantManager::ApplyEnchantSuccess(Item* item, uint8_t new_level) {
    if (!item) {
        return;
    }

    item->SetEnchantLevel(new_level);

    // 计算并应用属性加成
    EnchantInfo bonus = CalculateEnchantBonus(new_level, item->GetItemId());
    item->AddEnchantBonus(bonus);
}

void EquipmentEnchantManager::ApplyEnchantFailed(Item* item) {
    if (!item) {
        return;
    }

    // 失败不做任何改变，材料和金币已消耗
}

void EquipmentEnchantManager::ApplyEnchantDowngrade(Item* item) {
    if (!item) {
        return;
    }

    uint8_t current_level = item->GetEnchantLevel();
    if (current_level > 0) {
        uint8_t new_level = current_level - 1;
        item->SetEnchantLevel(new_level);

        // 移除属性加成
        EnchantInfo bonus = CalculateEnchantBonus(new_level, item->GetItemId());
        item->SetEnchantBonus(bonus);
    }
}

void EquipmentEnchantManager::ApplyEnchantDestroyed(Item* item) {
    if (!item) {
        return;
    }

    // TODO [需其他系统]: 从玩家背包移除装备
    // 需要背包系统提供：
    // 1. Player::RemoveItem(Item* item) - 从背包删除物品
    // 2. InventorySystem::DestroyItem(uint64_t item_id) - 销毁物品
    // 3. 客户端通知（装备销毁消息）
    // 4. 数据库更新（删除物品记录）
    spdlog::warn("Equipment destroyed: item_id={}, owner_id={}",
                item->GetItemId(), item->GetOwnerId());

    // 临时标记为已销毁
    // item->SetDestroyed(true);
}

bool EquipmentEnchantManager::LoadJewelDatabase() {
    // 加载宝石数据
    // 示例：1级攻击宝石
    jewel_db_[10001] = JewelInfo(10001, 1, 1, 10);  // 攻击力+10
    jewel_db_[10002] = JewelInfo(10002, 1, 2, 10);  // 防御力+10
    jewel_db_[10003] = JewelInfo(10003, 1, 3, 100); // HP+100
    jewel_db_[10004] = JewelInfo(10004, 1, 4, 50);  // MP+50

    // 2级攻击宝石
    jewel_db_[10005] = JewelInfo(10005, 2, 1, 25);  // 攻击力+25

    spdlog::info("EquipmentEnchantManager: Loaded {} jewel types", jewel_db_.size());
    return true;
}

const JewelInfo* EquipmentEnchantManager::GetJewelInfo(uint32_t jewel_id) const {
    auto it = jewel_db_.find(jewel_id);
    return (it != jewel_db_.end()) ? &it->second : nullptr;
}

bool EquipmentEnchantManager::CanSocketJewel(const Item* item, uint32_t jewel_id) const {
    if (!item) {
        return false;
    }

    // 检查是否有空插槽
    if (item->GetEmptySocketCount() == 0) {
        return false;
    }

    // 检查宝石是否存在
    const JewelInfo* jewel = GetJewelInfo(jewel_id);
    if (!jewel) {
        return false;
    }

    return true;
}

bool EquipmentEnchantManager::SocketJewel(Player* player, Item* item, uint32_t jewel_id) {
    if (!player || !item) {
        return false;
    }

    if (!CanSocketJewel(item, jewel_id)) {
        return false;
    }

    // 消耗宝石
    player->RemoveItem(jewel_id, 1);

    // 镶嵌宝石
    const JewelInfo* jewel = GetJewelInfo(jewel_id);
    item->SocketJewel(*jewel);

    spdlog::info("[Enchant-{}] Socketed jewel {} into item {}",
                player->GetObjectId(), jewel_id, item->GetItemId());

    return true;
}

bool EquipmentEnchantManager::RemoveJewel(Item* item, uint8_t socket_index) {
    if (!item) {
        return false;
    }

    // 移除宝石
    const JewelInfo* jewel = item->GetJewel(socket_index);
    if (!jewel) {
        return false;
    }

    item->RemoveJewel(socket_index);

    spdlog::debug("Removed jewel from socket {}", socket_index);
    return true;
}

EnchantInfo EquipmentEnchantManager::CalculateEnchantBonus(uint8_t level, uint32_t item_id) const {
    EnchantInfo info;
    info.level = level;

    // 基础成长：每级+5%基础属性
    float growth_rate = 1.0f + (level * 0.05f);

    // TODO [需其他系统]: 从ItemDatabase获取基础属性
    // 需要ItemDatabase系统提供：
    // 1. GetBaseItemInfo(uint32_t item_id) -> ItemInfo*
    // 2. ItemInfo包含：base_attack, base_defence, base_hp, base_mp等
    // 3. 根据装备类型（武器、防具、饰品）返回不同属性
    //
    // 示例：
    // const ItemInfo* item_info = ItemDatabase::GetInstance().GetItemInfo(item_id);
    // if (item_info) {
    //     base_attack = item_info->GetBaseAttack();
    //     base_defence = item_info->GetBaseDefence();
    //     ...
    // }
    //
    // 临时使用硬编码默认值
    uint16_t base_attack = 100;
    uint16_t base_defence = 50;
    uint16_t base_hp = 1000;
    uint16_t base_mp = 500;

    info.attack_power = static_cast<uint16_t>(base_attack * growth_rate * 0.1f);
    info.defence = static_cast<uint16_t>(base_defence * growth_rate * 0.1f);
    info.max_hp = static_cast<uint16_t>(base_hp * growth_rate * 0.05f);
    info.max_mp = static_cast<uint16_t>(base_mp * growth_rate * 0.05f);

    return info;
}

uint16_t EquipmentEnchantManager::CalculateBonusStat(uint8_t level, uint16_t base_stat, float rate) const {
    return static_cast<uint16_t>(base_stat * rate * level);
}

float EquipmentEnchantManager::CalculateSuccessRate(const EnchantRecipe& recipe,
                                                    uint16_t ability_value,
                                                    uint16_t material_num,
                                                    uint16_t item_level) const {
    return recipe.CalculateActualSuccessRate(ability_value, material_num, item_level);
}

void EquipmentEnchantManager::LoadProtectCards() {
    // 加载保护卡数据
    protect_cards_[60001] = 50.0f;  // 50%保护率
    protect_cards_[60002] = 70.0f;  // 70%保护率
    protect_cards_[60003] = 100.0f; // 100%保护率

    spdlog::debug("EquipmentEnchantManager: Loaded {} protect cards", protect_cards_.size());
}

float EquipmentEnchantManager::GetProtectRate(uint32_t card_id) const {
    auto it = protect_cards_.find(card_id);
    return (it != protect_cards_.end()) ? it->second : 0.0f;
}

bool EquipmentEnchantManager::UseProtectCard(Player* player, uint32_t card_id) {
    if (!player) {
        return false;
    }

    // 检查是否有保护卡
    if (player->GetItemCount(card_id) == 0) {
        return false;
    }

    // 消耗保护卡
    player->RemoveItem(card_id, 1);

    spdlog::debug("[Enchant-{}] Used protect card {}", player->GetObjectId(), card_id);
    return true;
}

EnchantResult EquipmentEnchantManager::RollEnchantResult(float success_rate,
                                                         float downgrade_rate,
                                                         float destroy_rate,
                                                         float protect_rate) {
    // 生成0-100的随机数
    float roll = RandomRange(0.0f, 100.0f);

    // 检查保护
    float actual_downgrade = downgrade_rate * (1.0f - protect_rate);
    float actual_destroy = destroy_rate * (1.0f - protect_rate);

    // 判定结果
    if (roll <= success_rate) {
        return EnchantResult::Success;
    } else if (roll <= success_rate + actual_downgrade) {
        return EnchantResult::Downgrade;
    } else if (roll <= success_rate + actual_downgrade + actual_destroy) {
        return EnchantResult::Destroyed;
    } else {
        return EnchantResult::Failed;
    }
}

const char* EquipmentEnchantManager::GetEnchantResultString(EnchantResult result) {
    switch (result) {
        case EnchantResult::Success: return "Success";
        case EnchantResult::Failed: return "Failed";
        case EnchantResult::Downgrade: return "Downgrade";
        case EnchantResult::Destroyed: return "Destroyed";
        case EnchantResult::MaterialLost: return "MaterialLost";
        case EnchantResult::Invalid: return "Invalid";
        default: return "Unknown";
    }
}

// ========== EquipmentEnchantCalculator ==========

float EquipmentEnchantCalculator::GetAbilityBonus(uint16_t ability_value) {
    // 能力值加成表
    // 每10点能力值增加1%成功率，最多+20%
    float bonus = ability_value / 10.0f;
    return std::min(bonus, 20.0f);
}

float EquipmentEnchantCalculator::GetMaterialBonus(uint16_t material_num) {
    // 材料数量加成表
    // 每个额外材料+0.5%，最多+10%
    if (material_num <= 1) {
        return 0.0f;
    }
    float bonus = (material_num - 1) * 0.5f;
    return std::min(bonus, 10.0f);
}

float EquipmentEnchantCalculator::GetItemLevelBonus(uint16_t item_level) {
    // 物品等级加成表
    // 每10级+1%，最多+5%
    float bonus = item_level / 10.0f;
    return std::min(bonus, 5.0f);
}

float EquipmentEnchantCalculator::GetAdjustValue() {
    // 调整值（固定+2%）
    return 2.0f;
}

float EquipmentEnchantCalculator::CalculateTotalBonus(uint16_t ability_value,
                                                      uint16_t material_num,
                                                      uint16_t item_level) {
    return GetAbilityBonus(ability_value) +
           GetMaterialBonus(material_num) +
           GetItemLevelBonus(item_level) +
           GetAdjustValue();
}

// ========== Helper Functions ==========

bool IsEnchantableItem(const Item* item) {
    if (!item) {
        return false;
    }

    ItemKind kind = GetItemKind(item->GetItemId());
    return IsEquipmentItem(kind);
}

uint8_t GetMaxEnchantLevel(uint32_t item_id, bool is_rare) {
    if (is_rare) {
        return MAX_RARE_REINFORCE_LEVEL;
    } else {
        return MAX_REINFORCE_LEVEL;
    }
}

uint32_t CalculateEnchantGoldCost(uint8_t current_level, uint8_t target_level) {
    // 金币消耗 = 目标等级² × 1000
    return static_cast<uint32_t>(target_level * target_level * 1000);
}

float CalculateBaseSuccessRate(uint8_t level) {
    // 基础成功率公式
    if (level <= 10) {
        return 100.0f;
    } else if (level <= 20) {
        return 90.0f - (level - 10) * 4.0f;
    } else if (level <= 30) {
        return 45.0f - (level - 20) * 2.5f;
    } else {
        return 18.0f - (level - 30) * 1.0f;
    }
}

std::vector<EnchantMaterial> GetRequiredMaterials(uint8_t target_level) {
    std::vector<EnchantMaterial> materials;

    // 每5级需要1个材料
    uint16_t count = (target_level + 4) / 5;
    materials.push_back(EnchantMaterial(50000, count));

    return materials;
}

} // namespace Game
} // namespace Murim
