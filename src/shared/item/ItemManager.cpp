#include "ItemManager.hpp"
#include "core/network/NotificationService.hpp"
// #include "shared/battle/BuffManager.hpp"  // TODO: Create BuffManager
#include "shared/battle/Buff.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include "core/database/CharacterDAO.hpp"
#include "shared/character/CharacterManager.hpp"

using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;
// using Database = Murim::Core::Database;  // Removed - causes parsing errors

namespace Murim {
namespace Game {

ItemManager& ItemManager::Instance() {
    static ItemManager instance;
    return instance;
}

void ItemManager::Update(uint32_t delta_time) {
    // 处理物品掉落、过期检查等 (暂时为空)
    (void)delta_time;
}

// ========== 物品模板管理 ==========

void ItemManager::Initialize() {
    spdlog::info("Initializing ItemManager...");

    // 从数据库加载所有物品模板
    auto templates = Murim::Core::Database::ItemDAO::LoadAllItemTemplates();

    item_templates_.clear();
    for (const auto& item_template : templates) {
        item_templates_[item_template.item_id] = item_template;
    }

    templates_loaded_ = true;

    spdlog::info("ItemManager initialized: {} item templates loaded",
                 item_templates_.size());
}

std::optional<ItemTemplate> ItemManager::GetItemTemplate(uint32_t item_id) {
    // 确保模板已加载
    if (!templates_loaded_) {
        Initialize();
    }

    auto it = item_templates_.find(item_id);
    if (it != item_templates_.end()) {
        return it->second;
    }

    // 尝试从数据库加载
    auto item_template = Murim::Core::Database::ItemDAO::LoadItemTemplate(item_id);
    if (item_template.has_value()) {
        item_templates_[item_id] = item_template.value();
        return item_template;
    }

    return std::nullopt;
}

void ItemManager::ReloadTemplates() {
    spdlog::info("Reloading item templates...");
    templates_loaded_ = false;
    Initialize();
}

// ========== 物品获取 ==========

uint64_t ItemManager::ObtainItem(uint64_t character_id, uint32_t item_id, uint16_t quantity) {
    // 验证物品
    auto item_template = GetItemTemplate(item_id);
    if (!item_template.has_value()) {
        spdlog::error("Invalid item_id: {}", item_id);
        return 0;
    }

    // 检查数量
    if (quantity == 0 || quantity > item_template->stack_limit) {
        spdlog::error("Invalid quantity: {} (max: {})", quantity, item_template->stack_limit);
        return 0;
    }

    // 查找空栏位
    auto empty_slot = FindEmptySlot(character_id);
    if (!empty_slot.has_value()) {
        spdlog::warn("No empty slot for character {}", character_id);
        return 0;
    }

    return ObtainItemToSlot(character_id, item_id, quantity, empty_slot.value()) ? 1 : 0;
}

bool ItemManager::ObtainItemToSlot(uint64_t character_id, uint32_t item_id,
                                    uint16_t quantity, uint16_t slot_index) {
    // 验证栏位
    if (!IsValidSlot(slot_index)) {
        spdlog::error("Invalid slot_index: {}", slot_index);
        return false;
    }

    // 验证物品
    auto item_template = GetItemTemplate(item_id);
    if (!item_template.has_value()) {
        spdlog::error("Invalid item_id: {}", item_id);
        return false;
    }

    // 创建物品实例
    uint64_t instance_id = Murim::Core::Database::ItemDAO::CreateItemInstance(item_id, quantity);
    if (instance_id == 0) {
        spdlog::error("Failed to create item instance");
        return false;
    }

    // 添加到背包
    bool success = Murim::Core::Database::ItemDAO::AddToInventory(character_id, instance_id,
                                                      item_id, slot_index, quantity);

    if (success) {
        spdlog::info("Character {} obtained item {} x{} at slot {}",
                     character_id, item_id, quantity, slot_index);
    }

    return success;
}

uint16_t ItemManager::RemoveItem(uint64_t character_id, uint32_t item_id, uint16_t quantity) {
    auto inventory = GetInventory(character_id);
    uint16_t removed = 0;

    for (const auto& slot : inventory) {
        if (removed >= quantity) {
            break;
        }

        if (slot.item_id == item_id) {
            uint16_t to_remove = std::min<uint16_t>(slot.quantity, static_cast<uint16_t>(quantity - removed));
            if (RemoveItemFromSlot(character_id, slot.slot_index, to_remove)) {
                removed += to_remove;
            }
        }
    }

    if (removed > 0) {
        spdlog::info("Character {} removed item {} x{}",
                     character_id, item_id, removed);
    }

    return removed;
}

bool ItemManager::RemoveItemFromSlot(uint64_t character_id, uint16_t slot_index, uint16_t quantity) {
    if (!IsValidSlot(slot_index)) {
        return false;
    }

    bool success = Murim::Core::Database::ItemDAO::RemoveFromInventory(character_id, slot_index, quantity);

    if (success) {
        spdlog::debug("Character {} removed {} items from slot {}",
                      character_id, quantity, slot_index);
    }

    return success;
}

// ========== 物品查询 ==========

uint16_t ItemManager::GetItemCount(uint64_t character_id, uint32_t item_id) {
    auto inventory = GetInventory(character_id);
    uint16_t count = 0;

    for (const auto& slot : inventory) {
        if (slot.item_id == item_id) {
            count += slot.quantity;
        }
    }

    return count;
}

bool ItemManager::HasEnoughItem(uint64_t character_id, uint32_t item_id, uint16_t quantity) {
    return GetItemCount(character_id, item_id) >= quantity;
}

std::vector<InventorySlot> ItemManager::GetInventory(uint64_t character_id) {
    return Murim::Core::Database::ItemDAO::LoadInventory(character_id);
}

std::optional<InventorySlot> ItemManager::GetInventorySlot(uint64_t character_id, uint16_t slot_index) {
    if (!IsValidSlot(slot_index)) {
        return std::nullopt;
    }

    auto inventory = GetInventory(character_id);
    for (const auto& slot : inventory) {
        if (slot.slot_index == slot_index) {
            return slot;
        }
    }

    return std::nullopt;
}

// ========== 背包管理 ==========

std::optional<uint16_t> ItemManager::FindEmptySlot(uint64_t character_id) {
    auto inventory = GetInventory(character_id);

    // 标记已占用的栏位
    std::vector<bool> occupied(inventory_capacity_, false);

    for (const auto& slot : inventory) {
        if (slot.slot_index < inventory_capacity_) {
            occupied[slot.slot_index] = true;
        }
    }

    // 查找第一个空栏位
    for (uint16_t i = 0; i < inventory_capacity_; ++i) {
        if (!occupied[i]) {
            return i;
        }
    }

    return std::nullopt;
}

std::vector<uint16_t> ItemManager::FindEmptySlots(uint64_t character_id, uint16_t count) {
    std::vector<uint16_t> empty_slots;

    auto inventory = GetInventory(character_id);

    // 标记已占用的栏位
    std::vector<bool> occupied(inventory_capacity_, false);

    for (const auto& slot : inventory) {
        if (slot.slot_index < inventory_capacity_) {
            occupied[slot.slot_index] = true;
        }
    }

    // 查找空栏位
    for (uint16_t i = 0; i < inventory_capacity_; ++i) {
        if (!occupied[i]) {
            empty_slots.push_back(i);
        }
    }

    return empty_slots;
}

bool ItemManager::MoveItem(uint64_t character_id, uint16_t from_slot, uint16_t to_slot) {
    if (!IsValidSlot(from_slot) || !IsValidSlot(to_slot)) {
        return false;
    }

    if (from_slot == to_slot) {
        return true;  // 无操作
    }

    bool success = Murim::Core::Database::ItemDAO::MoveItemInInventory(character_id, from_slot, to_slot);

    if (success) {
        spdlog::debug("Character {} moved item from slot {} to slot {}",
                      character_id, from_slot, to_slot);
    }

    return success;
}

bool ItemManager::MergeItem(uint64_t character_id, uint16_t from_slot, uint16_t to_slot) {
    if (!IsValidSlot(from_slot) || !IsValidSlot(to_slot)) {
        return false;
    }

    auto from_slot_data = GetInventorySlot(character_id, from_slot);
    auto to_slot_data = GetInventorySlot(character_id, to_slot);

    if (!from_slot_data.has_value() || !to_slot_data.has_value()) {
        return false;
    }

    // 检查是否是同一种物品
    if (from_slot_data->item_id != to_slot_data->item_id) {
        return false;
    }

    // 检查是否可堆叠
    auto item_template = GetItemTemplate(from_slot_data->item_id);
    if (!item_template.has_value() || !item_template->IsStackable()) {
        return false;
    }

    // 计算可合并数量
    uint16_t space = item_template->stack_limit - to_slot_data->quantity;
    uint16_t to_merge = (std::min)(from_slot_data->quantity, space);

    if (to_merge == 0) {
        return false;
    }

    // 更新目标栏位
    to_slot_data->quantity += to_merge;
    Murim::Core::Database::ItemDAO::UpdateInventorySlot(*to_slot_data);

    // 更新源栏位
    if (to_merge == from_slot_data->quantity) {
        // 全部合并，删除源栏位
        Murim::Core::Database::ItemDAO::RemoveFromInventory(character_id, from_slot, from_slot_data->quantity);
    } else {
        // 部分合并
        from_slot_data->quantity -= to_merge;
        Murim::Core::Database::ItemDAO::UpdateInventorySlot(*from_slot_data);
    }

    spdlog::debug("Character {} merged {} items from slot {} to slot {}",
                  character_id, to_merge, from_slot, to_slot);

    return true;
}

bool ItemManager::SplitItem(uint64_t character_id, uint16_t from_slot, uint16_t to_slot, uint16_t quantity) {
    if (!IsValidSlot(from_slot) || !IsValidSlot(to_slot)) {
        return false;
    }

    auto from_slot_data = GetInventorySlot(character_id, from_slot);
    if (!from_slot_data.has_value()) {
        return false;
    }

    if (quantity >= from_slot_data->quantity) {
        return false;  // 应该使用 MoveItem
    }

    // 检查目标栏位是否为空
    auto to_slot_data = GetInventorySlot(character_id, to_slot);
    if (to_slot_data.has_value()) {
        return false;  // 目标栏位不为空
    }

    // 创建新的物品实例
    auto item_template = GetItemTemplate(from_slot_data->item_id);
    if (!item_template.has_value()) {
        return false;
    }

    // 检查是否可堆叠
    if (!item_template->IsStackable()) {
        return false;
    }

    // 创建新实例
    uint64_t new_instance_id = Murim::Core::Database::ItemDAO::CreateItemInstance(from_slot_data->item_id, quantity);
    if (new_instance_id == 0) {
        return false;
    }

    // 添加到目标栏位
    if (!Murim::Core::Database::ItemDAO::AddToInventory(character_id, new_instance_id,
                                            from_slot_data->item_id, to_slot, quantity)) {
        Murim::Core::Database::ItemDAO::DeleteItemInstance(new_instance_id);
        return false;
    }

    // 更新源栏位
    Murim::Core::Database::ItemDAO::UpdateInventorySlot(*from_slot_data);

    spdlog::debug("Character {} split {} items from slot {} to slot {}",
                  character_id, quantity, from_slot, to_slot);

    return true;
}

uint16_t ItemManager::GetEmptySlotCount(uint64_t character_id) {
    auto inventory = GetInventory(character_id);

    // 标记已占用的栏位
    std::vector<bool> occupied(inventory_capacity_, false);

    for (const auto& slot : inventory) {
        if (slot.slot_index < inventory_capacity_) {
            occupied[slot.slot_index] = true;
        }
    }

    // 计算空栏位数
    uint16_t count = 0;
    for (bool slot : occupied) {
        if (!slot) {
            ++count;
        }
    }

    return count;
}

// ========== 装备管理 ==========

std::optional<EquipmentSlot> ItemManager::GetEquipment(uint64_t character_id) {
    return Murim::Core::Database::ItemDAO::LoadEquipment(character_id);
}

ItemUseResult ItemManager::EquipItem(uint64_t character_id, uint16_t slot_index) {
    // 获取栏位物品
    auto slot_data = GetInventorySlot(character_id, slot_index);
    if (!slot_data.has_value()) {
        return ItemUseResult::kInvalidItem;
    }

    // 获取物品模板
    auto item_template = GetItemTemplate(slot_data->item_id);
    if (!item_template.has_value()) {
        return ItemUseResult::kInvalidItem;
    }

    // 检查是否可装备
    if (!item_template->IsEquipable()) {
        return ItemUseResult::kInvalidItem;
    }

    // 检查装备条件
    if (!CanEquip(character_id, *item_template)) {
        return ItemUseResult::kInvalidTarget;
    }

    // 获取装备部位
    if (!item_template->equip_slot.has_value()) {
        return ItemUseResult::kInvalidItem;
    }

    EquipSlot equip_slot = item_template->equip_slot.value();

    // 获取当前装备
    auto equipment = GetEquipment(character_id);
    if (!equipment.has_value()) {
        equipment = EquipmentSlot{};
        equipment->character_id = character_id;
    }

    // 获取当前装备的物品实例ID
    auto current_equip = equipment->GetEquip(equip_slot);

    // 装备新物品
    if (!Murim::Core::Database::ItemDAO::EquipItem(character_id, equip_slot, slot_data->item_instance_id)) {
        return ItemUseResult::kFailed;
    }

    // 从背包移除
    Murim::Core::Database::ItemDAO::RemoveFromInventory(character_id, slot_index, 1);

    // 如果有旧装备,放回背包
    if (current_equip.has_value()) {
        Murim::Core::Database::ItemDAO::AddToInventory(character_id, current_equip.value(),
                                           slot_data->item_id, slot_index, 1);
        Murim::Core::Database::ItemDAO::UnequipItem(character_id, equip_slot);
    }

    spdlog::info("Character {} equipped item {} at slot {}",
                 character_id, slot_data->item_id, static_cast<int>(equip_slot));

    // 更新角色属性

    return ItemUseResult::kSuccess;
}

bool ItemManager::UnequipItem(uint64_t character_id, EquipSlot equip_slot) {
    if (!IsValidEquipSlot(equip_slot)) {
        return false;
    }

    // 获取装备
    auto equipment = GetEquipment(character_id);
    if (!equipment.has_value()) {
        return false;
    }

    // 获取装备实例ID
    auto equip_instance_id = equipment->GetEquip(equip_slot);
    if (!equip_instance_id.has_value()) {
        return false;  // 该部位没有装备
    }

    // 加载物品实例
    auto item_instance = Murim::Core::Database::ItemDAO::LoadItemInstance(equip_instance_id.value());
    if (!item_instance.has_value()) {
        return false;
    }

    // 查找空栏位
    auto empty_slot = FindEmptySlot(character_id);
    if (!empty_slot.has_value()) {
        spdlog::warn("No empty slot to unequip item");
        return false;
    }

    // 添加到背包
    if (!Murim::Core::Database::ItemDAO::AddToInventory(
                                            character_id,
                                            item_instance->instance_id,
                                            item_instance->item_id, empty_slot.value(), 1)) {
        return false;
    }

    // 卸下装备
    if (!Murim::Core::Database::ItemDAO::UnequipItem(character_id, equip_slot)) {
        return false;
    }

    spdlog::info("Character {} unequipped item from slot {}",
                 character_id, static_cast<int>(equip_slot));

    // 更新角色属性

    return true;
}

bool ItemManager::CanEquip(uint64_t character_id, const ItemTemplate& item_template) {
    // 获取角色信息
    auto char_info = GetCharacterInfo(character_id);
    if (!char_info.has_value()) {
        return false;
    }

    // Check job class restriction
    if (item_template.limit_job.has_value() &&
        item_template.limit_job.value() != 0 &&
        item_template.limit_job.value() != char_info->initial_weapon) {
        return false;
    }

    // Check gender restriction
    if (item_template.limit_gender.has_value() &&
        item_template.limit_gender.value() != 0 &&
        item_template.limit_gender.value() != char_info->gender + 1) {
        return false;
    }

    // Check level restriction
    if (item_template.limit_level.has_value() &&
        char_info->level < item_template.limit_level.value()) {
        return false;
    }

    // Check strength requirement
    if (item_template.limit_strength.has_value() &&
        char_info->strength < item_template.limit_strength.value()) {
        return false;
    }

    if (item_template.limit_int.has_value() &&
        char_info->intelligence < item_template.limit_int.value()) {
        return false;
    }

    if (item_template.limit_dex.has_value() &&
        char_info->dexterity < item_template.limit_dex.value()) {
        return false;
    }

    if (item_template.limit_vit.has_value() &&
        char_info->vitality < item_template.limit_vit.value()) {
        return false;
    }

    return true;
}

// ========== 物品使用 ==========

ItemUseResult ItemManager::UseItem(uint64_t character_id, uint16_t slot_index) {
    // 获取栏位物品
    auto slot_data = GetInventorySlot(character_id, slot_index);
    if (!slot_data.has_value()) {
        return ItemUseResult::kInvalidItem;
    }

    // 获取物品模板
    auto item_template = GetItemTemplate(slot_data->item_id);
    if (!item_template.has_value()) {
        return ItemUseResult::kInvalidItem;
    }

    // 根据物品类型处理
    switch (item_template->item_type) {
        case ItemType::kPotion:
            return UseConsumable(character_id, *item_template, slot_data->quantity);

        case ItemType::kWeapon:
        case ItemType::kArmor:
        case ItemType::kCostume:
            return EquipItem(character_id, slot_index);

        default:
            return ItemUseResult::kInvalidItem;
    }
}

ItemUseResult ItemManager::UseConsumable(uint64_t character_id, const ItemTemplate& item_template, uint16_t quantity) {
    // 对应 legacy 消耗品使用逻辑
    try {
        // 获取角色信息
        auto character_opt = Murim::Core::Database::CharacterDAO::Load(character_id);
        if (!character_opt.has_value()) {
            spdlog::error("Character {} not found for using consumable", character_id);
            return ItemUseResult::kInvalidTarget;
        }

        auto& character = character_opt.value();

        // 检查HP/MP恢复
        bool has_effect = false;
        uint32_t old_hp = character.hp;
        uint32_t old_mp = character.mp;

        // 恢复 HP - 对应 legacy HP恢复逻辑
        if (item_template.hp_recover > 0) {
            uint32_t recover_amount = item_template.hp_recover * quantity;
            character.hp = (std::min)(character.max_hp, character.hp + recover_amount);
            has_effect = true;
            spdlog::debug("Recovered {} HP ({} -> {})", character.hp - old_hp, old_hp, character.hp);
        }

        // 恢复 HP百分比 - 对应 legacy HP百分比恢复
        if (item_template.hp_recover_rate > 0.0f) {
            uint32_t recover_amount = static_cast<uint32_t>(character.max_hp * item_template.hp_recover_rate * quantity);
            character.hp = (std::min)(character.max_hp, character.hp + recover_amount);
            has_effect = true;
            spdlog::debug("Recovered {} HP by rate ({} -> {})", character.hp - old_hp, old_hp, character.hp);
        }

        // 恢复 MP - 对应 legacy MP恢复逻辑
        if (item_template.mp_recover > 0) {
            uint32_t recover_amount = item_template.mp_recover * quantity;
            character.mp = (std::min)(character.max_mp, character.mp + recover_amount);
            has_effect = true;
            spdlog::debug("Recovered {} MP ({} -> {})", character.mp - old_mp, old_mp, character.mp);
        }

        // 恢复 MP百分比 - 对应 legacy MP百分比恢复
        if (item_template.mp_recover_rate > 0.0f) {
            uint32_t recover_amount = static_cast<uint32_t>(character.max_mp * item_template.mp_recover_rate * quantity);
            character.mp = (std::min)(character.max_mp, character.mp + recover_amount);
            has_effect = true;
            spdlog::debug("Recovered {} MP by rate ({} -> {})", character.mp - old_mp, old_mp, character.mp);
        }

        // Apply stat bonuses - legacy stat application
        if (item_template.strength > 0 || item_template.intelligence > 0 ||
            item_template.dexterity > 0 || item_template.vitality > 0 ||
            item_template.physical_attack > 0 || item_template.physical_defense > 0 ||
            item_template.magic_attack > 0 || item_template.magic_defense > 0 ||
            item_template.attack_speed > 0 || item_template.move_speed > 0 ||
            item_template.critical_rate > 0 || item_template.hit_rate > 0 ||
            item_template.dodge_rate > 0) {
            // Create and apply Buff - legacy Buff system
            auto buff_opt = CreateBuffFromConsumable(item_template);
            if (buff_opt.has_value()) {
                auto& buff = buff_opt.value();

                // Apply buff (player uses on self)
                uint64_t buff_id = BuffManager::Instance().ApplyBuff(
                    character_id,  // caster = self
                    character_id,  // target = self
                    buff
                );

                if (buff_id != 0) {
                    spdlog::info("Applied buff {} from consumable {} to character {}",
                                buff.buff_id, item_template.item_id, character_id);
                    has_effect = true;

                    // Update character stats (buff already applied via BuffManager)
                    // Stats will be automatically applied in next combat calculation
                }
            }
        }

        if (!has_effect) {
            spdlog::warn("Consumable {} has no effect", item_template.item_id);
            return ItemUseResult::kFailed;
        }

        // 保存角色数据
        bool success = Murim::Core::Database::CharacterDAO().Save(character);

        if (success) {
            spdlog::info("Character {} used consumable {} x{} - HP: {}->{}, MP: {}->{}",
                        character_id, item_template.item_id, quantity,
                        old_hp, character.hp, old_mp, character.mp);

            // 发送HP/MP更新消息到客户端 - 对应 legacy HP/MP通知
            Core::Network::NotificationService::Instance().NotifyHpMpUpdate(
                character_id, character.hp, character.max_hp, character.mp, character.max_mp);

        }

        return success ? ItemUseResult::kSuccess : ItemUseResult::kFailed;

    } catch (const std::exception& e) {
        spdlog::error("Failed to use consumable {} for character {}: {}",
                     item_template.item_id, character_id, e.what());
        return ItemUseResult::kFailed;
    }
}

// ========== 物品验证 ==========

bool ItemManager::IsValidItem(uint32_t item_id) {
    return GetItemTemplate(item_id).has_value();
}

bool ItemManager::IsValidSlot(uint16_t slot_index) {
    return slot_index < inventory_capacity_;
}

bool ItemManager::IsValidEquipSlot(EquipSlot equip_slot) {
    int slot_value = static_cast<int>(equip_slot);
    return slot_value >= 0 && slot_value < 10;
}

// ========== 背包容量 ==========

uint16_t ItemManager::GetInventoryCapacity() {
    return inventory_capacity_;
}

void ItemManager::SetInventoryCapacity(uint16_t capacity) {
    inventory_capacity_ = capacity;
    spdlog::info("Inventory capacity set to {}", capacity);
}

// ========== 物品丢弃 ==========

bool ItemManager::DropItem(uint64_t character_id, uint16_t slot_index, uint16_t quantity) {
    // 获取栏位物品
    auto slot_data = GetInventorySlot(character_id, slot_index);
    if (!slot_data.has_value()) {
        return false;
    }

    // 获取物品模板
    auto item_template = GetItemTemplate(slot_data->item_id);
    if (!item_template.has_value()) {
        return false;
    }

    // 检查是否可丢弃
    if (!item_template->is_dropable) {
        spdlog::warn("Item {} is not dropable", slot_data->item_id);
        return false;
    }

    // 移除物品
    return RemoveItemFromSlot(character_id, slot_index, quantity);
}

// ========== 辅助方法 ==========

std::optional<ItemManager::CharacterInfo> ItemManager::GetCharacterInfo(uint64_t character_id) {
    auto character = Murim::Core::Database::CharacterDAO::Load(character_id);
    if (!character.has_value()) {
        return std::nullopt;
    }

    auto stats = Murim::Core::Database::CharacterDAO::LoadStats(character_id);
    if (!stats.has_value()) {
        return std::nullopt;
    }

    CharacterInfo info;
    info.level = character->level;
    info.initial_weapon = character->initial_weapon;
    info.gender = character->gender;
    info.strength = stats->strength;
    info.intelligence = stats->intelligence;
    info.dexterity = stats->dexterity;
    info.vitality = stats->vitality;

    return info;
}

void ItemManager::UpdateCharacterStats(uint64_t character_id) {
    // 对应 legacy 装备属性更新逻辑
    try {
        // 1. 获取基础属性 - 对应 legacy 基础属性获取
        auto character_opt = Murim::Core::Database::CharacterDAO::Load(character_id);
        if (!character_opt.has_value()) {
            spdlog::error("Character {} not found for UpdateCharacterStats", character_id);
            return;
        }

        auto stats_opt = Murim::Core::Database::CharacterDAO::LoadStats(character_id);
        if (!stats_opt.has_value()) {
            spdlog::error("Character stats {} not found", character_id);
            return;
        }

        CharacterStats stats = stats_opt.value();

        // 2. 获取所有装备的加成 - 对应 legacy 装备属性累加
        EquipmentBonus bonus = CalculateEquipmentBonus(character_id);

        // 3. 计算最终属性 - 对应 legacy 属性计算
        // 基础属性 + 装备加成
        stats.strength += bonus.strength;
        stats.intelligence += bonus.intelligence;
        stats.dexterity += bonus.dexterity;
        stats.vitality += bonus.vitality;
        stats.wisdom += bonus.wisdom;

        // 战斗属+ 装备加成
        stats.physical_attack += bonus.physical_attack;
        stats.physical_defense += bonus.physical_defense;
        stats.magic_attack += bonus.magic_attack;
        stats.magic_defense += bonus.magic_defense;
        stats.attack_speed = static_cast<uint16_t>(stats.attack_speed + bonus.attack_speed);
        stats.move_speed = static_cast<uint16_t>(stats.move_speed + bonus.move_speed);
        stats.critical_rate += bonus.critical_rate;
        stats.hit_rate += bonus.hit_rate;
        stats.dodge_rate += bonus.dodge_rate;

        // 4. 更新角色 - 对应 legacy 属性保存
        Murim::Core::Database::CharacterDAO().SaveStats(character_id, stats);

        spdlog::debug("Updated stats for character {} - str:{} int:{} dex:{} vit:{}",
                     character_id, stats.strength, stats.intelligence,
                     stats.dexterity, stats.vitality);

    } catch (const std::exception& e) {
        spdlog::error("Failed to update stats for character {}: {}", character_id, e.what());
    }
}

ItemManager::EquipmentBonus ItemManager::CalculateEquipmentBonus(uint64_t character_id) {
    // 对应 legacy 装备属性计算逻辑
    EquipmentBonus bonus{};

    auto equipment = GetEquipment(character_id);
    if (!equipment.has_value()) {
        return bonus;
    }

    // 遍历所有装备槽 - 对应 legacy 装备遍历
    std::vector<uint64_t> equipped_items = equipment->GetAllEquipped();

    for (uint64_t item_instance_id : equipped_items) {
        // 获取物品实例
        auto item_instance_opt = Murim::Core::Database::ItemDAO::LoadItemInstance(item_instance_id);
        if (!item_instance_opt.has_value()) {
            continue;
        }

        // 获取物品模板
        auto item_template_opt = GetItemTemplate(item_instance_opt->item_id);
        if (!item_template_opt.has_value()) {
            continue;
        }

        const auto& item = item_template_opt.value();

        // 累加基础属�?- 对应 legacy 属性累�?        bonus.strength += item.strength;
        bonus.intelligence += item.intelligence;
        bonus.dexterity += item.dexterity;
        bonus.vitality += item.vitality;
        bonus.wisdom += item.wisdom;

        // 累加战斗属�?- 对应 legacy 战斗属性累�?        bonus.physical_attack += item.physical_attack;
        bonus.physical_defense += item.physical_defense;
        bonus.magic_attack += item.magic_attack;
        bonus.magic_defense += item.magic_defense;
        bonus.attack_speed += item.attack_speed;
        bonus.move_speed += item.move_speed;
        bonus.critical_rate += item.critical_rate;
        bonus.hit_rate += item.hit_rate;
        bonus.dodge_rate += item.dodge_rate;
    }

    spdlog::debug("Calculated equipment bonus for character {} - str:{} int:{} atk:{} def:{}",
                 character_id, bonus.strength, bonus.intelligence,
                 bonus.physical_attack, bonus.physical_defense);

    return bonus;
}

// ========== Buff-Item Integration ==========

std::optional<Buff> ItemManager::CreateBuffFromConsumable(const ItemTemplate& item_template) {
    // 对应 legacy 消耗品 Buff 创建逻辑
    Buff buff{};
    buff.skill_id = 0;  // 0 表示来自物品,不是技能
    buff.name = "Item: " + item_template.name;
    buff.description = "Buff from consumable item";
    buff.is_purgeable = false;  // 物品 Buff 不可净化
    buff.is_dispellable = true; // 可驱散
    buff.interval = 0;  // 非周期性
    buff.duration = GetConsumableBuffDuration(item_template);

    std::vector<BuffEffect> effects;

    // 基础属性增益
    if (item_template.strength > 0) {
        effects.push_back({BuffEffectType::kStrengthBoost, static_cast<int32_t>(item_template.strength), 0.0f});
    }
    if (item_template.intelligence > 0) {
        effects.push_back({BuffEffectType::kIntelligenceBoost, static_cast<int32_t>(item_template.intelligence), 0.0f});
    }
    if (item_template.dexterity > 0) {
        effects.push_back({BuffEffectType::kDexterityBoost, static_cast<int32_t>(item_template.dexterity), 0.0f});
    }
    if (item_template.vitality > 0) {
        effects.push_back({BuffEffectType::kVitalityBoost, static_cast<int32_t>(item_template.vitality), 0.0f});
    }
    if (item_template.wisdom > 0) {
        effects.push_back({BuffEffectType::kWisdomBoost, static_cast<int32_t>(item_template.wisdom), 0.0f});
    }

    // 战斗属性增益
    if (item_template.physical_attack > 0) {
        effects.push_back({BuffEffectType::kPhysicalAttackBoost, static_cast<int32_t>(item_template.physical_attack), 0.0f});
    }
    if (item_template.physical_defense > 0) {
        effects.push_back({BuffEffectType::kPhysicalDefenseBoost, static_cast<int32_t>(item_template.physical_defense), 0.0f});
    }
    if (item_template.magic_attack > 0) {
        effects.push_back({BuffEffectType::kMagicAttackBoost, static_cast<int32_t>(item_template.magic_attack), 0.0f});
    }
    if (item_template.magic_defense > 0) {
        effects.push_back({BuffEffectType::kMagicDefenseBoost, static_cast<int32_t>(item_template.magic_defense), 0.0f});
    }
    if (item_template.attack_speed > 0) {
        effects.push_back({BuffEffectType::kAttackSpeedBoost, static_cast<int32_t>(item_template.attack_speed), 0.0f});
    }
    if (item_template.move_speed > 0) {
        effects.push_back({BuffEffectType::kMoveSpeedBoost, static_cast<int32_t>(item_template.move_speed), 0.0f});
    }
    if (item_template.critical_rate > 0) {
        effects.push_back({BuffEffectType::kCriticalRateBoost, static_cast<int32_t>(item_template.critical_rate), 0.0f});
    }
    if (item_template.hit_rate > 0) {
        effects.push_back({BuffEffectType::kHitRateBoost, static_cast<int32_t>(item_template.hit_rate), 0.0f});
    }
    if (item_template.dodge_rate > 0) {
        effects.push_back({BuffEffectType::kDodgeRateBoost, static_cast<int32_t>(item_template.dodge_rate), 0.0f});
    }

    // HP/MP 恢复 Buff (持续恢复)
    if (item_template.hp_recover_rate > 0.0f) {
        effects.push_back({BuffEffectType::kRegeneration, 0, item_template.hp_recover_rate});
    }
    if (item_template.mp_recover_rate > 0.0f) {
        effects.push_back({BuffEffectType::kManaRegeneration, 0, item_template.mp_recover_rate});
    }

    // 如果没有任何效果,返回 nullopt
    if (effects.empty()) {
        return std::nullopt;
    }

    buff.effects = effects;
    return buff;
}

uint32_t ItemManager::GetConsumableBuffDuration(const ItemTemplate& item_template) {
    // 对应 legacy 消耗品 Buff 持续时间计算
    // 根据物品类型和等级决定持续时间

    // 如果物品有冷却时间,使用冷却时间作为 Buff 持续时间
    if (item_template.cooldown > 0) {
        // 持续时间通常是冷却时间的 2-3 倍
        return item_template.cooldown * 2;
    }

    // 根据物品等级决定默认持续时间
    uint32_t base_duration = 300;  // 默认 5 分钟

    if (item_template.item_grade <= 10) {
        base_duration = 300;  // 5 分钟 (初级物品)
    } else if (item_template.item_grade <= 30) {
        base_duration = 600;  // 10 分钟 (中级物品)
    } else if (item_template.item_grade <= 50) {
        base_duration = 900;  // 15 分钟 (高级物品)
    } else {
        base_duration = 1800; // 30 分钟 (顶级物品)
    }

    // 食物和药水通常持续时间较短
    if (item_template.item_type == ItemType::kPotion) {
        // 药水效果通常较短 (即时效果为主, Buff 较短)
        if (item_template.hp_recover > 0 || item_template.mp_recover > 0) {
            // 纯恢复药水没有持续 Buff
            return 0;
        }
        // 属性药水持续 5-10 分钟
        return 600;
    }

    // Buff 卷轴持续时间较长
    if (item_template.physical_attack > 0 || item_template.magic_attack > 0 ||
        item_template.physical_defense > 0 || item_template.magic_defense > 0) {
        // 攻击/防御 Buff 卷轴持续 15-30 分钟
        return base_duration * 2;
    }

    return base_duration;
}

} // namespace Game
} // namespace Murim
