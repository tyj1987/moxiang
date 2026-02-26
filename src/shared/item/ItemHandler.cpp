/**
 * @file ItemHandler.cpp
 * @brief 物品处理器实现
 */

#include "ItemHandler.hpp"
#include "core/network/PacketSerializer.hpp"
#include "character/CharacterManager.hpp"
#include "item/ItemManager.hpp"
#include "battle/DamageCalculator.hpp"
#include "battle/SpecialStateManager.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

ItemHandler::ItemHandler(
    Murim::Game::CharacterManager& char_manager,
    Murim::Game::ItemManager& item_manager,
    std::shared_ptr<Murim::Game::DamageCalculator> damage_calculator,
    std::shared_ptr<Murim::Game::SpecialStateManager> buff_manager)
    : character_manager_(char_manager),
      item_manager_(item_manager),
      damage_calculator_(damage_calculator),
      buff_manager_(buff_manager) {

    spdlog::info("[ItemHandler] Initialized");
}

ItemHandler::~ItemHandler() {
    spdlog::info("[ItemHandler] Destroyed");
}

EquipItemResponse ItemHandler::HandleEquipItem(const EquipItemRequest& request) {
    spdlog::info("[ItemHandler] Processing equip item request:");
    spdlog::info("  Character ID: {}", request.character_id);
    spdlog::info("  Item ID: {}", request.item_id);
    spdlog::info("  Slot Index: {}", request.slot_index);

    EquipItemResponse response;
    response.result_code = 0;  // 默认成功

    // 1. 验证角色
    auto character_opt = character_manager_->GetCharacter(request.character_id);
    if (!character_opt.has_value()) {
        spdlog::error("[ItemHandler] Character not found: {}", request.character_id);
        response.result_code = 1;  // 角色不存在
        return response;
    }

    auto& character = character_opt.value();

    // 2. 验证物品模板
    auto item_template_opt = item_manager_->GetItemTemplate(request.item_id);
    if (!item_template_opt.has_value()) {
        spdlog::error("[ItemHandler] Item template not found: {}", request.item_id);
        response.result_code = 1;  // 物品不存在
        return response;
    }

    const auto& item_template = item_template_opt.value();

    // 3. 验证槽位是否有效
    if (!item_manager_->IsValidSlot(request.slot_index)) {
        spdlog::error("[ItemHandler] Invalid slot index: {}", request.slot_index);
        response.result_code = 2;  // 槽位无效
        return response;
    }

    // 4. 验证装备条件
    if (!ValidateEquipConditions(character, item_template)) {
        spdlog::warn("[ItemHandler] Character does not meet equip conditions");
        response.result_code = 3;  // 不满足装备条件
        return response;
    }

    // 5. 验证装备部位是否有效
    if (!item_manager_->IsValidEquipSlot(item_template.equip_slot)) {
        spdlog::error("[ItemHandler] Invalid equip slot: {}", item_template.equip_slot);
        response.result_code = 4;  // 装备部位无效
        return response;
    }

    // 6. 执行装备
    auto equip_result = item_manager_->EquipItem(request.character_id, request.slot_index);

    if (equip_result == ItemUseResult::kSuccess) {
        spdlog::info("[ItemHandler] Item equipped successfully");

        // 7. 应用装备属性加成
        ApplyEquipmentBonus(character, item_template);

        // 8. 更新角色属性
        item_manager_->UpdateCharacterStats(request.character_id);

        // 9. 保存到数据库
        character_manager_->SaveCharacter(character);

        // 10. 填充响应
        response.result_code = 0;
        response.item_id = request.item_id;
        response.equip_slot = static_cast<uint16_t>(item_template.equip_slot);

        // 计算属性加成
        auto bonus = item_manager_->CalculateEquipmentBonus(request.character_id);
        response.strength = bonus.strength;
        response.intelligence = bonus.intelligence;
        response.dexterity = bonus.dexterity;
        response.vitality = bonus.vitality;
        response.physical_attack = bonus.physical_attack;
        response.physical_defense = bonus.physical_defense;
        response.magic_attack = bonus.magic_attack;
        response.magic_defense = bonus.magic_defense;
        response.attack_speed = bonus.attack_speed;
        response.move_speed = bonus.move_speed;

        // 11. 广播角色状态变化
        BroadcastCharacterStats(request.character_id, character);

    } else {
        spdlog::error("[ItemHandler] Failed to equip item");
        response.result_code = 3;  // 装备失败
    }

    spdlog::info("[ItemHandler] Equip item result: code={}, item={}, slot={}",
                 response.result_code, response.item_id, response.equip_slot);

    return response;
}

UnequipItemResponse ItemHandler::HandleUnequipItem(const UnequipItemRequest& request) {
    spdlog::info("[ItemHandler] Processing unequip item request:");
    spdlog::info("  Character ID: {}", request.character_id);
    spdlog::info("  Equip Slot: {}", request.equip_slot);

    UnequipItemResponse response;
    response.result_code = 0;  // 默认成功

    // 1. 验证角色
    auto character_opt = character_manager_->GetCharacter(request.character_id);
    if (!character_opt.has_value()) {
        spdlog::error("[ItemHandler] Character not found: {}", request.character_id);
        response.result_code = 1;  // 角色不存在
        return response;
    }

    auto& character = character_opt.value();

    // 2. 验证装备部位是否有效
    if (!item_manager_->IsValidEquipSlot(request.equip_slot)) {
        spdlog::error("[ItemHandler] Invalid equip slot: {}", request.equip_slot);
        response.result_code = 1;  // 装备部位无效
        return response;
    }

    // 3. 检查该部位是否有装备
    auto equipment_opt = item_manager_->GetEquipment(request.character_id);
    if (!equipment_opt.has_value()) {
        spdlog::error("[ItemHandler] Failed to get equipment");
        response.result_code = 2;  // 获取装备失败
        return response;
    }

    const auto& equipment = equipment_opt.value();

    // 检查该部位是否有装备
    if (!equipment.slots[request.equip_slot].has_value()) {
        spdlog::warn("[ItemHandler] No equipment in slot: {}", request.equip_slot);
        response.result_code = 2;  // 该部位无装备
        return response;
    }

    // 4. 获取装备的物品模板
    const auto& equip_slot = equipment.slots[request.equip_slot].value();
    auto item_template_opt = item_manager_->GetItemTemplate(equip_slot.item_id);
    if (!item_template_opt.has_value()) {
        spdlog::error("[ItemHandler] Equipment item template not found: {}", equip_slot.item_id);
        response.result_code = 1;
        return response;
    }

    const auto& item_template = item_template_opt.value();

    // 5. 执行卸下
    bool success = item_manager_->UnequipItem(request.character_id, request.equip_slot);

    if (success) {
        spdlog::info("[ItemHandler] Item unequipped successfully");

        // 6. 移除装备属性加成
        RemoveEquipmentBonus(character, item_template);

        // 7. 更新角色属性
        item_manager_->UpdateCharacterStats(request.character_id);

        // 8. 保存到数据库
        character_manager_->SaveCharacter(character);

        // 9. 填充响应
        response.result_code = 0;
        response.equip_slot = request.equip_slot;
        response.item_id = item_template.id_;
        response.target_slot = equip_slot.slot_index;

        // 10. 广播角色状态变化
        BroadcastCharacterStats(request.character_id, character);

    } else {
        spdlog::error("[ItemHandler] Failed to unequip item");
        response.result_code = 1;
    }

    spdlog::info("[ItemHandler] Unequip item result: code={}, slot={}, item={}, target_slot={}",
                 response.result_code, response.equip_slot, response.item_id, response.target_slot);

    return response;
}

UseConsumableResponse ItemHandler::HandleUseConsumable(const UseConsumableRequest& request) {
    spdlog::info("[ItemHandler] Processing use consumable request:");
    spdlog::info("  Character ID: {}", request.character_id);
    spdlog::info("  Item ID: {}", request.item_id);
    spdlog::info("  Slot Index: {}", request.slot_index);
    spdlog::info("  Quantity: {}", request.quantity);

    UseConsumableResponse response;
    response.result_code = 0;  // 默认成功

    // 1. 验证角色
    auto character_opt = character_manager_->GetCharacter(request.character_id);
    if (!character_opt.has_value()) {
        spdlog::error("[ItemHandler] Character not found: {}", request.character_id);
        response.result_code = 1;  // 角色不存在
        return response;
    }

    auto& character = character_opt.value();

    // 2. 验证物品模板
    auto item_template_opt = item_manager_->GetItemTemplate(request.item_id);
    if (!item_template_opt.has_value()) {
        spdlog::error("[ItemHandler] Item template not found: {}", request.item_id);
        response.result_code = 1;  // 物品不存在
        return response;
    }

    const auto& item_template = item_template_opt.value();

    // 3. 验证物品类型
    if (item_template.type != ItemType::kConsumable) {
        spdlog::warn("[ItemHandler] Item is not consumable: {}", request.item_id);
        response.result_code = 2;  // 物品不可使用
        return response;
    }

    // 4. 验证数量
    auto slot_opt = item_manager_->GetInventorySlot(request.character_id, request.slot_index);
    if (!slot_opt.has_value()) {
        spdlog::error("[ItemHandler] Invalid slot index: {}", request.slot_index);
        response.result_code = 2;
        return response;
    }

    const auto& slot = slot_opt.value();
    if (slot.quantity < request.quantity) {
        spdlog::warn("[ItemHandler] Not enough quantity: have={}, need={}",
                     slot.quantity, request.quantity);
        response.result_code = 3;  // 数量不足
        response.remaining_quantity = slot.quantity;
        return response;
    }

    // 5. 使用物品
    auto use_result = item_manager_->UseConsumable(request.character_id, item_template, request.quantity);

    if (use_result == ItemUseResult::kSuccess) {
        spdlog::info("[ItemHandler] Consumable used successfully");

        // 6. 应用消耗品效果
        ApplyConsumableEffect(character, item_template, request.quantity);

        // 7. 填充响应
        response.result_code = 0;
        response.item_id = request.item_id;
        response.remaining_quantity = slot.quantity - request.quantity;
        response.hp_restored = 0;
        response.mp_restored = 0;
        response.buff_id = 0;
        response.buff_duration = 0;

        // 根据消耗品类型填充效果
        if (item_template.consumable_type == ConsumableType::kHpPotion) {
            uint32_t hp_restore = item_template.hp_restore * request.quantity;
            response.hp_restored = hp_restore;
            spdlog::info("[ItemHandler] HP restored: {}", hp_restore);
        } else if (item_template.consumable_type == ConsumableType::kMpPotion) {
            uint32_t mp_restore = item_template.mp_restore * request.quantity;
            response.mp_restored = mp_restore;
            spdlog::info("[ItemHandler] MP restored: {}", mp_restore);
        } else if (item_template.consumable_type == ConsumableType::kBuffScroll) {
            response.buff_id = item_template.buff_id;
            response.buff_duration = item_template.buff_duration;
            spdlog::info("[ItemHandler] Buff applied: id={}, duration={}s",
                         response.buff_id, response.buff_duration);
        }

        // 8. 保存到数据库
        character_manager_->SaveCharacter(character);

        // 9. 广播角色状态变化
        BroadcastCharacterStats(request.character_id, character);

    } else {
        spdlog::error("[ItemHandler] Failed to use consumable");
        response.result_code = 4;  // 效果应用失败
    }

    spdlog::info("[ItemHandler] Use consumable result: code={}, item={}, remaining={}, hp={}, mp={}",
                 response.result_code, response.item_id, response.remaining_quantity,
                 response.hp_restored, response.mp_restored);

    return response;
}

DropItemResponse ItemHandler::HandleDropItem(const DropItemRequest& request) {
    spdlog::info("[ItemHandler] Processing drop item request:");
    spdlog::info("  Character ID: {}", request.character_id);
    spdlog::info("  Item ID: {}", request.item_id);
    spdlog::info("  Slot Index: {}", request.slot_index);
    spdlog::info("  Quantity: {}", request.quantity);
    spdlog::info("  Position: ({:.1f}, {:.1f}, {:.1f})", request.x, request.y, request.z);

    DropItemResponse response;
    response.result_code = 0;  // 默认成功

    // 1. 验证角色
    auto character_opt = character_manager_->GetCharacter(request.character_id);
    if (!character_opt.has_value()) {
        spdlog::error("[ItemHandler] Character not found: {}", request.character_id);
        response.result_code = 1;  // 角色不存在
        return response;
    }

    // 2. 验证槽位
    auto slot_opt = item_manager_->GetInventorySlot(request.character_id, request.slot_index);
    if (!slot_opt.has_value()) {
        spdlog::error("[ItemHandler] Invalid slot index: {}", request.slot_index);
        response.result_code = 2;  // 槽位无效
        return response;
    }

    const auto& slot = slot_opt.value();

    // 3. 验证数量
    if (slot.quantity < request.quantity) {
        spdlog::warn("[ItemHandler] Not enough quantity: have={}, need={}",
                     slot.quantity, request.quantity);
        response.result_code = 4;  // 数量不足
        return response;
    }

    // 4. 丢弃物品
    uint16_t actual_dropped = item_manager_->RemoveItem(request.character_id, slot.item_id, request.quantity);

    if (actual_dropped > 0) {
        spdlog::info("[ItemHandler] Item dropped successfully: quantity={}", actual_dropped);

        // 5. 保存到数据库
        auto character_opt2 = character_manager_->GetCharacter(request.character_id);
        if (character_opt2.has_value()) {
            character_manager_->SaveCharacter(character_opt2.value());
        }

        // 6. 填充响应
        response.result_code = 0;
        response.item_id = slot.item_id;
        response.quantity = actual_dropped;

        // 7. TODO: 创建地面掉落物
        // 暂时直接删除物品
        spdlog::info("[ItemHandler] Drop position: ({:.1f}, {:.1f}, {:.1f})",
                     request.x, request.y, request.z);

    } else {
        spdlog::error("[ItemHandler] Failed to drop item");
        response.result_code = 1;
    }

    spdlog::info("[ItemHandler] Drop item result: code={}, item={}, quantity={}",
                 response.result_code, response.item_id, response.quantity);

    return response;
}

MergeItemResponse ItemHandler::HandleMergeItem(const MergeItemRequest& request) {
    spdlog::info("[ItemHandler] Processing merge item request:");
    spdlog::info("  Character ID: {}", request.character_id);
    spdlog::info("  From Slot: {}", request.from_slot);
    spdlog::info("  To Slot: {}", request.to_slot);

    MergeItemResponse response;
    response.result_code = 0;  // 默认成功

    // 1. 验证角色
    auto character_opt = character_manager_->GetCharacter(request.character_id);
    if (!character_opt.has_value()) {
        spdlog::error("[ItemHandler] Character not found: {}", request.character_id);
        response.result_code = 2;  // 槽位无效
        return response;
    }

    // 2. 验证槽位
    auto from_slot_opt = item_manager_->GetInventorySlot(request.character_id, request.from_slot);
    auto to_slot_opt = item_manager_->GetInventorySlot(request.character_id, request.to_slot);

    if (!from_slot_opt.has_value() || !to_slot_opt.has_value()) {
        spdlog::error("[ItemHandler] Invalid slot indices");
        response.result_code = 2;
        return response;
    }

    const auto& from_slot = from_slot_opt.value();
    const auto& to_slot = to_slot_opt.value();

    // 3. 验证物品可堆叠
    if (from_slot.item_id != to_slot.item_id) {
        spdlog::warn("[ItemHandler] Items are not the same: from_id={}, to_id={}",
                     from_slot.item_id, to_slot.item_id);
        response.result_code = 1;  // 物品不可叠加
        return response;
    }

    auto item_template_opt = item_manager_->GetItemTemplate(from_slot.item_id);
    if (!item_template_opt.has_value()) {
        spdlog::error("[ItemHandler] Item template not found: {}", from_slot.item_id);
        response.result_code = 1;
        return response;
    }

    const auto& item_template = item_template_opt.value();

    if (!item_template.stackable) {
        spdlog::warn("[ItemHandler] Item is not stackable: {}", from_slot.item_id);
        response.result_code = 1;  // 物品不可叠加
        return response;
    }

    // 4. 执行整理
    bool success = item_manager_->MergeItem(request.character_id, request.from_slot, request.to_slot);

    if (success) {
        spdlog::info("[ItemHandler] Items merged successfully");

        // 5. 填充响应
        response.result_code = 0;
        response.item_id = from_slot.item_id;
        response.from_slot = request.from_slot;
        response.to_slot = request.to_slot;
        response.total_quantity = from_slot.quantity + to_slot.quantity;

        // 6. 保存到数据库
        auto character_opt2 = character_manager_->GetCharacter(request.character_id);
        if (character_opt2.has_value()) {
            character_manager_->SaveCharacter(character_opt2.value());
        }

    } else {
        spdlog::error("[ItemHandler] Failed to merge items");
        response.result_code = 2;
    }

    spdlog::info("[ItemHandler] Merge item result: code={}, item={}, from_slot={}, to_slot={}, total={}",
                 response.result_code, response.item_id, response.from_slot,
                 response.to_slot, response.total_quantity);

    return response;
}

SplitItemResponse ItemHandler::HandleSplitItem(const SplitItemRequest& request) {
    spdlog::info("[ItemHandler] Processing split item request:");
    spdlog::info("  Character ID: {}", request.character_id);
    spdlog::info("  From Slot: {}", request.from_slot);
    spdlog::info("  To Slot: {}", request.to_slot);
    spdlog::info("  Quantity: {}", request.quantity);

    SplitItemResponse response;
    response.result_code = 0;  // 默认成功

    // 1. 验证角色
    auto character_opt = character_manager_->GetCharacter(request.character_id);
    if (!character_opt.has_value()) {
        spdlog::error("[ItemHandler] Character not found: {}", request.character_id);
        response.result_code = 2;  // 槽位无效
        return response;
    }

    // 2. 验证源槽位
    auto from_slot_opt = item_manager_->GetInventorySlot(request.character_id, request.from_slot);
    if (!from_slot_opt.has_value()) {
        spdlog::error("[ItemHandler] Invalid from slot index: {}", request.from_slot);
        response.result_code = 2;  // 源槽位无效
        return response;
    }

    const auto& from_slot = from_slot_opt.value();

    // 3. 验证数量
    if (from_slot.quantity < request.quantity) {
        spdlog::warn("[ItemHandler] Not enough quantity: have={}, need={}",
                     from_slot.quantity, request.quantity);
        response.result_code = 4;  // 数量不足
        return response;
    }

    // 4. 验证目标槽位为空
    auto to_slot_opt = item_manager_->GetInventorySlot(request.character_id, request.to_slot);
    if (!to_slot_opt.has_value()) {
        spdlog::error("[ItemHandler] Target slot is not empty: {}", request.to_slot);
        response.result_code = 3;  // 目标槽位非空
        return response;
    }

    // 5. 执行分割
    bool success = item_manager_->SplitItem(request.character_id, request.from_slot, request.to_slot, request.quantity);

    if (success) {
        spdlog::info("[ItemHandler] Item split successfully");

        // 6. 填充响应
        response.result_code = 0;
        response.item_id = from_slot.item_id;
        response.from_slot = request.from_slot;
        response.to_slot = request.to_slot;
        response.from_quantity = from_slot.quantity - request.quantity;
        response.to_quantity = request.quantity;

        // 7. 保存到数据库
        auto character_opt2 = character_manager_->GetCharacter(request.character_id);
        if (character_opt2.has_value()) {
            character_manager_->SaveCharacter(character_opt2.value());
        }

    } else {
        spdlog::error("[ItemHandler] Failed to split item");
        response.result_code = 1;
    }

    spdlog::info("[ItemHandler] Split item result: code={}, item={}, from_slot={}, to_slot={}, from_qty={}, to_qty={}",
                 response.result_code, response.item_id, response.from_slot,
                 response.to_slot, response.from_quantity, response.to_quantity);

    return response;
}

// ========== 私有方法 ==========

bool ItemHandler::ValidateEquipConditions(const Murim::Game::GameObject& character,
                                      const ItemTemplate& item) {
    // 检查等级要求
    if (item.required_level > 0 && character.level_ < item.required_level) {
        spdlog::debug("[ItemHandler] Level requirement not met: char_level={}, required={}",
                     character.level_, item.required_level);
        return false;
    }

    // 检查职业要求
    if (item.required_job != Game::JobClass::kNone &&
        character.job_ != item.required_job) {
        spdlog::debug("[ItemHandler] Job requirement not met: char_job={}, required={}",
                     static_cast<int>(character.job_), static_cast<int>(item.required_job));
        return false;
    }

    // 检查基础属性要求
    if (item.required_strength > 0 && character.strength_ < item.required_strength) {
        spdlog::debug("[ItemHandler] Strength requirement not met");
        return false;
    }

    if (item.required_intelligence > 0 && character.intelligence_ < item.required_intelligence) {
        spdlog::debug("[ItemHandler] Intelligence requirement not met");
        return false;
    }

    if (item.required_dexterity > 0 && character.dexterity_ < item.required_dexterity) {
        spdlog::debug("[ItemHandler] Dexterity requirement not met");
        return false;
    }

    if (item.required_vitality > 0 && character.vitality_ < item.required_vitality) {
        spdlog::debug("[ItemHandler] Vitality requirement not met");
        return false;
    }

    spdlog::debug("[ItemHandler] All equip conditions met");
    return true;
}

void ItemHandler::ApplyEquipmentBonus(const Murim::Game::GameObject& character,
                                      const ItemTemplate& item) {
    spdlog::info("[ItemHandler] Applying equipment bonus: item_id={}", item.id_);

    // 应用装备属性加成到角色
    // 注意：这里只是临时应用，实际属性更新由 UpdateCharacterStats 完成
    // 对应 legacy: 装备属性加成

    spdlog::info("[ItemHandler] Equipment bonus applied: strength={}, intelligence={}, ...",
                 item.bonus_strength, item.bonus_intelligence);
}

void ItemHandler::RemoveEquipmentBonus(const Murim::Game::GameObject& character,
                                      const ItemTemplate& item) {
    spdlog::info("[ItemHandler] Removing equipment bonus: item_id={}", item.id_);

    // 移除装备属性加成
    // 对应 legacy: 装备属性移除

    spdlog::info("[ItemHandler] Equipment bonus removed");
}

void ItemHandler::ApplyConsumableEffect(const Murim::Game::GameObject& character,
                                       const ItemTemplate& item,
                                       uint16_t quantity) {
    spdlog::info("[ItemHandler] Applying consumable effect: item_id={}, quantity={}",
                 item.id_, quantity);

    // 根据消耗品类型应用效果
    if (item.consumable_type == ConsumableType::kHpPotion) {
        // 恢复HP
        uint32_t hp_restore = item.hp_restore * quantity;
        uint32_t new_hp = std::min(character.current_hp_ + hp_restore, character.max_hp_);
        character.current_hp_ = new_hp;

        spdlog::info("[ItemHandler] HP restored: have={}, restore={}, new_hp={}/{}/{}",
                     character.current_hp_ - hp_restore, hp_restore,
                     new_hp, character.max_hp_, character.max_hp_);
    }
    else if (item.consumable_type == ConsumableType::kMpPotion) {
        // 恢复MP
        uint32_t mp_restore = item.mp_restore * quantity;
        uint32_t new_mp = std::min(character.current_mp_ + mp_restore, character.max_mp_);
        character.current_mp_ = new_mp;

        spdlog::info("[ItemHandler] MP restored: have={}, restore={}, new_mp={}/{}/{}",
                     character.current_mp_ - mp_restore, mp_restore,
                     new_mp, character.max_mp_, character.max_mp_);
    }
    else if (item.consumable_type == ConsumableType::kBuffScroll) {
        // 应用Buff
        if (item.buff_id > 0) {
            // 创建Buff数据
            Buff buff;
            buff.id_ = item.buff_id;
            buff.buff_id_ = item.buff_id;  // 与技能ID关联
            buff.caster_id_ = character.id_;
            buff.target_id_ = character.id_;
            buff.duration_ = item.buff_duration * 1000;  // 转换为毫秒
            buff.strength_ = item.buff_strength;
            buff.intelligence_ = item.buff_intelligence;
            buff.dexterity_ = item.buff_dexterity;
            buff.vitality_ = item.buff_vitality;
            buff.physical_attack_ = item.buff_physical_attack;
            buff.physical_defense_ = item.buff_physical_defense;
            buff.magic_attack_ = item.buff_magic_attack;
            buff.magic_defense_ = item.buff_magic_defense;

            // 应用Buff
            buff_manager_->ApplyBuff(character.id_, buff);

            spdlog::info("[ItemHandler] Buff applied: id={}, duration={}s",
                         item.buff_id, item.buff_duration);
        }
    }

    spdlog::info("[ItemHandler] Consumable effect applied");
}

void ItemHandler::BroadcastCharacterStats(uint64_t character_id,
                                         const Murim::Game::GameObject& character,
                                         std::shared_ptr<Core::Network::Connection> exclude_conn) {
    spdlog::info("[ItemHandler] Broadcasting character stats: character_id={}", character_id);

    // 构造角色属性更新消息
    std::vector<uint8_t> data;

    // CharacterID: 8 bytes
    for (int i = 0; i < 8; i++) {
        data.push_back((character_id >> (i * 8)) & 0xFF);
    }

    // MaxHP: 4 bytes
    data.push_back((character.max_hp_ >> 0) & 0xFF);
    data.push_back((character.max_hp_ >> 8) & 0xFF);
    data.push_back((character.max_hp_ >> 16) & 0xFF);
    data.push_back((character.max_hp_ >> 24) & 0xFF);

    // CurrentHP: 4 bytes
    data.push_back((character.current_hp_ >> 0) & 0xFF);
    data.push_back((character.current_hp_ >> 8) & 0xFF);
    data.push_back((character.current_hp_ >> 16) & 0xFF);
    data.push_back((character.current_hp_ >> 24) & 0xFF);

    // MaxMP: 4 bytes
    data.push_back((character.max_mp_ >> 0) & 0xFF);
    data.push_back((character.max_mp_ >> 8) & 0xFF);
    data.push_back((character.max_mp_ >> 16) & 0xFF);
    data.push_back((character.max_mp_ >> 24) & 0xFF);

    // CurrentMP: 4 bytes
    data.push_back((character.current_mp_ >> 0) & 0xFF);
    data.push_back((character.current_mp_ >> 8) & 0xFF);
    data.push_back((character.current_mp_ >> 16) & 0xFF);
    data.push_back((character.current_mp_ >> 24) & 0xFF);

    // 获取周围玩家
    // TODO: 从 AOIManager 获取150米内的玩家
    std::vector<uint64_t> nearby_characters;
    // TODO: nearby_characters = aoi_manager_->GetAOICharacters(character.x, character.y, 150.0f);

    // 广播给周围玩家
    for (uint64_t nearby_char_id : nearby_characters) {
        // 跳过发送给自己
        if (nearby_char_id == character_id) {
            continue;
        }

        // 获取连接
        // TODO: auto conn = character_manager_->GetConnectionByCharacterId(nearby_char_id);
        // if (conn && conn != exclude_conn) {
        //     auto buffer = std::make_shared<Core::Network::ByteBuffer>(data.data(), data.size());
        //     conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t) {
        //         if (ec) {
        //             spdlog::error("[ItemHandler] Failed to broadcast stats: {}", ec.message());
        //         }
        //     });
        // }
    }

    spdlog::info("[ItemHandler] Stats broadcasted to {} characters", nearby_characters.size());
}

} // namespace Game
} // namespace Murim
