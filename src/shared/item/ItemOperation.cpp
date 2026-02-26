// MurimServer - Item Operation Implementation
// 物品使用逻辑系统实现

#include "ItemOperation.hpp"
#include "../game/Player.hpp"
#include "../game/GameObject.hpp"
#include "../game/Item.hpp"
#include "../battle/SpecialStates.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>

namespace Murim {
namespace Game {

// ========== Helper Functions ==========

ItemKind GetItemKind(uint32_t item_id) {
    // TODO [需其他系统]: 从ItemDatabase获取物品类型
    // 需要实现物品数据库查询功能
    // return ItemDatabase::GetItemKind(item_id);

    // 临时实现：根据物品ID范围判断类型
    if (item_id >= 1000 && item_id < 2000) {
        return ItemKind::HP_Potion;
    } else if (item_id >= 2000 && item_id < 3000) {
        return ItemKind::MP_Potion;
    } else if (item_id >= 3000 && item_id < 4000) {
        return ItemKind::Weapon;
    } else if (item_id >= 4000 && item_id < 5000) {
        return ItemKind::Armor;
    }
    return ItemKind::None;
}

bool IsConsumableItem(ItemKind kind) {
    return kind == ItemKind::HP_Potion ||
           kind == ItemKind::MP_Potion ||
           kind == ItemKind::BuffScroll ||
           kind == ItemKind::TeleportScroll ||
           kind == ItemKind::Food;
}

bool IsEquipmentItem(ItemKind kind) {
    return kind == ItemKind::Weapon ||
           kind == ItemKind::Armor ||
           kind == ItemKind::Helmet ||
           kind == ItemKind::Gloves ||
           kind == ItemKind::Shoes ||
           kind == ItemKind::Necklace ||
           kind == ItemKind::Ring;
}

float CalculateRestoreAmount(float base_value, uint16_t player_level) {
    // 恢复量 = 基础值 * (1 + 等级 * 0.1)
    return base_value * (1.0f + player_level * 0.1f);
}

std::vector<ItemEffect> GetItemEffects(uint32_t item_id) {
    std::vector<ItemEffect> effects;

    // TODO [需其他系统]: 从ItemDatabase获取物品效果
    // 需要实现物品数据库查询功能
    // return ItemDatabase::GetItemEffects(item_id);

    // 临时实现：根据物品ID范围提供示例效果
    if (item_id >= 1000 && item_id < 2000) {
        // HP药水
        ItemEffect effect(ItemEffect::Type::RestoreHP, 100.0f);
        effects.push_back(effect);
    } else if (item_id >= 2000 && item_id < 3000) {
        // MP药水
        ItemEffect effect(ItemEffect::Type::RestoreMP, 50.0f);
        effects.push_back(effect);
    }

    return effects;
}

// ========== ItemUseHandler ==========

ItemUseHandler::ItemUseHandler(ItemKind kind)
    : item_kind_(kind) {
}

// ========== HP Potion Handler ==========

ItemUseHandler_HPPotion::ItemUseHandler_HPPotion()
    : ItemUseHandler(ItemKind::HP_Potion) {
}

bool ItemUseHandler_HPPotion::CanUse(const Item* item, const ItemUseContext& context) {
    if (!context.user || !item) {
        return false;
    }

    // 检查HP是否已满
    if (context.user->GetHealthPercent() >= 100.0f) {
        return false;
    }

    // 检查是否死亡
    if (!context.user->IsAlive()) {
        return false;
    }

    return true;
}

ItemUseResult ItemUseHandler_HPPotion::Use(const Item* item, const ItemUseContext& context) {
    if (!context.user || !item) {
        return ItemUseResult::Failed;
    }

    // 获取恢复效果
    std::vector<ItemEffect> effects = GetItemEffects(item->GetItemId());
    float restore_amount = 0.0f;

    for (const auto& effect : effects) {
        if (effect.type == ItemEffect::Type::RestoreHP) {
            uint16_t player_level = context.user->GetLevel();
            restore_amount = CalculateRestoreAmount(effect.value1, player_level);
        }
    }

    if (restore_amount <= 0.0f) {
        return ItemUseResult::Failed;
    }

    // 检查HP是否已满
    if (context.user->GetHealthPercent() >= 100.0f) {
        return ItemUseResult::FullHP;
    }

    // 恢复HP
    uint32_t restore_int = static_cast<uint32_t>(restore_amount);
    context.user->RestoreHP(restore_int);

    spdlog::debug("[Item-{}] HP Potion: Restored {} HP",
                 context.user->GetObjectId(), restore_int);

    return ItemUseResult::Success;
}

// ========== MP Potion Handler ==========

ItemUseHandler_MPPotion::ItemUseHandler_MPPotion()
    : ItemUseHandler(ItemKind::MP_Potion) {
}

bool ItemUseHandler_MPPotion::CanUse(const Item* item, const ItemUseContext& context) {
    if (!context.user || !item) {
        return false;
    }

    // 检查MP是否已满
    if (context.user->GetManaPercent() >= 100.0f) {
        return false;
    }

    // 检查是否死亡
    if (!context.user->IsAlive()) {
        return false;
    }

    return true;
}

ItemUseResult ItemUseHandler_MPPotion::Use(const Item* item, const ItemUseContext& context) {
    if (!context.user || !item) {
        return ItemUseResult::Failed;
    }

    // 获取恢复效果
    std::vector<ItemEffect> effects = GetItemEffects(item->GetItemId());
    float restore_amount = 0.0f;

    for (const auto& effect : effects) {
        if (effect.type == ItemEffect::Type::RestoreMP) {
            uint16_t player_level = context.user->GetLevel();
            restore_amount = CalculateRestoreAmount(effect.value1, player_level);
        }
    }

    if (restore_amount <= 0.0f) {
        return ItemUseResult::Failed;
    }

    // 检查MP是否已满
    if (context.user->GetManaPercent() >= 100.0f) {
        return ItemUseResult::FullMP;
    }

    // 恢复MP
    uint32_t restore_int = static_cast<uint32_t>(restore_amount);
    context.user->RestoreMP(restore_int);

    spdlog::debug("[Item-{}] MP Potion: Restored {} MP",
                 context.user->GetObjectId(), restore_int);

    return ItemUseResult::Success;
}

// ========== Buff Scroll Handler ==========

ItemUseHandler_BuffScroll::ItemUseHandler_BuffScroll()
    : ItemUseHandler(ItemKind::BuffScroll) {
}

bool ItemUseHandler_BuffScroll::CanUse(const Item* item, const ItemUseContext& context) {
    if (!context.user || !item) {
        return false;
    }

    // 检查是否死亡
    if (!context.user->IsAlive()) {
        return false;
    }

    // 检查Buff是否已存在
    std::vector<ItemEffect> effects = GetItemEffects(item->GetItemId());
    for (const auto& effect : effects) {
        if (effect.type == ItemEffect::Type::Buff) {
            // 检查玩家是否已有此Buff
            if (SpecialStateManager::HasState(context.user->GetObjectId(), effect.buff_id)) {
                spdlog::debug("[Item-{}] Buff Scroll: Buff {} already exists",
                            context.user->GetObjectId(), effect.buff_id);
                return false;
            }
        }
    }

    return true;
}

ItemUseResult ItemUseHandler_BuffScroll::Use(const Item* item, const ItemUseContext& context) {
    if (!context.user || !item) {
        return ItemUseResult::Failed;
    }

    // 获取Buff效果
    std::vector<ItemEffect> effects = GetItemEffects(item->GetItemId());

    for (const auto& effect : effects) {
        if (effect.type == ItemEffect::Type::Buff) {
            // 应用Buff
            SpecialStateInfo state;
            state.state_id = effect.buff_id;
            state.state_type = GetStateTypeFromBuffID(effect.buff_id);  // 根据buff_id确定状态类型
            state.duration = effect.duration / 1000.0f;  // ms转秒
            state.time_remaining = state.duration;
            state.level = 1;

            // 应用状态到玩家
            SpecialStateManager::ApplyState(
                context.user->GetObjectId(),
                state.state_type,
                context.user->GetObjectId(),  // caster_id
                0,  // skill_id (物品使用)
                1,  // skill_level
                effect.duration
            );

            spdlog::debug("[Item-{}] Buff Scroll: Applied buff {} for {:.1f}s",
                         context.user->GetObjectId(), effect.buff_id, state.duration);
        }
    }

    return ItemUseResult::Success;
}

// ========== Teleport Scroll Handler ==========

ItemUseHandler_TeleportScroll::ItemUseHandler_TeleportScroll()
    : ItemUseHandler(ItemKind::TeleportScroll) {
}

bool ItemUseHandler_TeleportScroll::CanUse(const Item* item, const ItemUseContext& context) {
    if (!context.user || !item) {
        return false;
    }

    // 检查是否死亡
    if (!context.user->IsAlive()) {
        return false;
    }

    // 检查是否在战斗中
    if (SpecialStateManager::HasState(context.user->GetObjectId(), STATE_BATTLE)) {
        spdlog::debug("[Item-{}] Teleport Scroll: Cannot use in battle",
                    context.user->GetObjectId());
        return false;
    }

    // 检查目标位置是否有效
    if (!IsValidPosition(context.target_x, context.target_y, context.target_z)) {
        spdlog::debug("[Item-{}] Teleport Scroll: Invalid position ({:.1f}, {:.1f}, {:.1f})",
                    context.user->GetObjectId(),
                    context.target_x, context.target_y, context.target_z);
        return false;
    }

    return true;
}

ItemUseResult ItemUseHandler_TeleportScroll::Use(const Item* item, const ItemUseContext& context) {
    if (!context.user || !item) {
        return ItemUseResult::Failed;
    }

    // 传送到目标位置
    context.user->TeleportTo(context.target_x, context.target_y, context.target_z);

    spdlog::debug("[Item-{}] Teleport Scroll: Teleported to ({:.1f}, {:.1f}, {:.1f})",
                 context.user->GetObjectId(),
                 context.target_x, context.target_y, context.target_z);

    return ItemUseResult::Success;
}

// ========== Food Handler ==========

ItemUseHandler_Food::ItemUseHandler_Food()
    : ItemUseHandler(ItemKind::Food) {
}

bool ItemUseHandler_Food::CanUse(const Item* item, const ItemUseContext& context) {
    if (!context.user || !item) {
        return false;
    }

    // 检查是否死亡
    if (!context.user->IsAlive()) {
        return false;
    }

    // 检查HP和MP是否都已满
    if (context.user->GetHealthPercent() >= 100.0f &&
        context.user->GetManaPercent() >= 100.0f) {
        return false;
    }

    return true;
}

ItemUseResult ItemUseHandler_Food::Use(const Item* item, const ItemUseContext& context) {
    if (!context.user || !item) {
        return ItemUseResult::Failed;
    }

    // 获取效果
    std::vector<ItemEffect> effects = GetItemEffects(item->GetItemId());
    uint16_t player_level = context.user->GetLevel();

    bool restored = false;

    for (const auto& effect : effects) {
        if (effect.type == ItemEffect::Type::RestoreHP) {
            float restore_amount = CalculateRestoreAmount(effect.value1, player_level);
            uint32_t restore_int = static_cast<uint32_t>(restore_amount);
            context.user->RestoreHP(restore_int);
            restored = true;

            spdlog::debug("[Item-{}] Food: Restored {} HP",
                         context.user->GetObjectId(), restore_int);
        }

        if (effect.type == ItemEffect::Type::RestoreMP) {
            float restore_amount = CalculateRestoreAmount(effect.value1, player_level);
            uint32_t restore_int = static_cast<uint32_t>(restore_amount);
            context.user->RestoreMP(restore_int);
            restored = true;

            spdlog::debug("[Item-{}] Food: Restored {} MP",
                         context.user->GetObjectId(), restore_int);
        }
    }

    if (!restored) {
        return ItemUseResult::Failed;
    }

    return ItemUseResult::Success;
}

// ========== Equipment Equip Handler ==========

ItemUseHandler_Equipment::ItemUseHandler_Equipment()
    : ItemUseHandler(ItemKind::Weapon) {  // 使用Weapon作为默认类型
}

bool ItemUseHandler_Equipment::CanUse(const Item* item, const ItemUseContext& context) {
    if (!context.user || !item) {
        return false;
    }

    // 获取物品模板
    const ItemTemplate* item_template = item->GetTemplate();
    if (!item_template) {
        spdlog::warn("[Item-Equipment] Item template is null");
        return false;
    }

    // 检查等级需求
    if (item_template->limit_level.has_value()) {
        if (context.user->GetLevel() < item_template->limit_level.value()) {
            spdlog::debug("[Item-Equipment] Level requirement not met: player_lvl={}, item_lvl={}",
                        context.user->GetLevel(), item_template->limit_level.value());
            return false;
        }
    }

    // 检查职业需求
    if (item_template->limit_job.has_value() && item_template->limit_job.value() != 0) {
        // limit_job为0表示所有职业都可以使用
        if (context.user->GetClass() != item_template->limit_job.value()) {
            spdlog::debug("[Item-Equipment] Class requirement not met: player_class={}, item_class={}",
                        static_cast<int>(context.user->GetClass()),
                        static_cast<int>(item_template->limit_job.value()));
            return false;
        }
    }

    // 检查装备槽是否可用
    // 获取装备类型对应的装备槽
    EquipSlot equip_slot = GetEquipSlotForItemType(item_template->item_type);
    if (!context.user->IsEquipSlotAvailable(static_cast<uint8_t>(equip_slot))) {
        spdlog::debug("[Item-Equipment] Equip slot not available: slot={}, item={}",
                    static_cast<uint8_t>(equip_slot), item_template->item_id);
        return false;
    }

    return true;
}

ItemUseResult ItemUseHandler_Equipment::Use(const Item* item, const ItemUseContext& context) {
    if (!context.user || !item) {
        return ItemUseResult::Failed;
    }

    // 装备物品
    bool success = context.user->EquipItem(item);

    if (success) {
        spdlog::debug("[Item-{}] Equipment: Equipped item {}",
                     context.user->GetObjectId(), item->GetItemId());
        return ItemUseResult::Success;
    } else {
        return ItemUseResult::Failed;
    }
}

// ========== ItemOperationManager ==========

ItemOperationManager::ItemOperationManager() {
}

ItemOperationManager::~ItemOperationManager() {
    Release();
}

void ItemOperationManager::Init() {
    CreateDefaultHandlers();
    spdlog::debug("ItemOperationManager: Initialized");
}

void ItemOperationManager::Release() {
    handlers_.clear();
}

void ItemOperationManager::CreateDefaultHandlers() {
    // 创建各种物品类型的处理器
    RegisterHandler(ItemKind::HP_Potion, std::make_unique<ItemUseHandler_HPPotion>());
    RegisterHandler(ItemKind::MP_Potion, std::make_unique<ItemUseHandler_MPPotion>());
    RegisterHandler(ItemKind::BuffScroll, std::make_unique<ItemUseHandler_BuffScroll>());
    RegisterHandler(ItemKind::TeleportScroll, std::make_unique<ItemUseHandler_TeleportScroll>());
    RegisterHandler(ItemKind::Food, std::make_unique<ItemUseHandler_Food>());

    // 装备类（使用同一个处理器）
    auto equipment_handler = std::make_unique<ItemUseHandler_Equipment>();
    RegisterHandler(ItemKind::Weapon, std::move(equipment_handler));
    RegisterHandler(ItemKind::Armor, std::make_unique<ItemUseHandler_Equipment>());
    RegisterHandler(ItemKind::Helmet, std::make_unique<ItemUseHandler_Equipment>());
    RegisterHandler(ItemKind::Gloves, std::make_unique<ItemUseHandler_Equipment>());
    RegisterHandler(ItemKind::Shoes, std::make_unique<ItemUseHandler_Equipment>());
    RegisterHandler(ItemKind::Necklace, std::make_unique<ItemUseHandler_Equipment>());
    RegisterHandler(ItemKind::Ring, std::make_unique<ItemUseHandler_Equipment>());

    spdlog::debug("ItemOperationManager: Created {} handlers", handlers_.size());
}

void ItemOperationManager::RegisterHandler(ItemKind kind, std::unique_ptr<ItemUseHandler> handler) {
    if (handler) {
        handlers_[kind] = std::move(handler);
    }
}

bool ItemOperationManager::CanUseItem(const Item* item, const ItemUseContext& context) {
    if (!item) {
        return false;
    }

    ItemKind kind = GetItemKind(item->GetItemId());
    auto it = handlers_.find(kind);

    if (it == handlers_.end()) {
        spdlog::warn("ItemOperationManager: No handler for item kind {}", static_cast<int>(kind));
        return false;
    }

    return it->second->CanUse(item, context);
}

ItemUseResult ItemOperationManager::UseItem(const Item* item, const ItemUseContext& context) {
    if (!item) {
        return ItemUseResult::Failed;
    }

    ItemKind kind = GetItemKind(item->GetItemId());
    auto it = handlers_.find(kind);

    if (it == handlers_.end()) {
        spdlog::warn("ItemOperationManager: No handler for item kind {}", static_cast<int>(kind));
        return ItemUseResult::Failed;
    }

    return it->second->Use(item, context);
}

ItemUseResult ItemOperationManager::UseItemQuick(Player* player, uint32_t item_id, GameObject* target) {
    if (!player) {
        return ItemUseResult::Failed;
    }

    // TODO [需其他系统]: 从玩家背包获取物品并使用
    // 需要通过Player对象获取背包系统中的物品
    // Item* item = player->GetInventory().GetItem(item_id);
    // if (!item) {
    //     spdlog::warn("[ItemUse] Item {} not found in player inventory", item_id);
    //     return ItemUseResult::Failed;
    // }

    // 创建上下文
    ItemUseContext context;
    context.user = player;
    context.target = target;
    context.current_time = GetCurrentTimeMs();

    // 使用物品
    // ItemUseResult result = UseItem(item, context);

    // 消耗物品（如果是消耗品）
    // if (result == ItemUseResult::Success && IsConsumableItem(GetItemKind(item_id))) {
    //     player->GetInventory().ConsumeItem(item_id, 1);
    // }

    // return result;

    return ItemUseResult::Success;  // 临时返回
}

// ========== 辅助函数 ==========

/**
 * @brief 根据物品类型获取装备槽
 */
EquipSlot GetEquipSlotForItemType(ItemType item_type) {
    switch (item_type) {
        case ItemType::kWeapon:
            return EquipSlot::kWeapon;
        case ItemType::kArmor:
            return EquipSlot::kArmor;
        case ItemType::kHelmet:
            return EquipSlot::kHelmet;
        case ItemType::kGloves:
            return EquipSlot::kGloves;
        case ItemType::kShoes:
            return EquipSlot::kShoes;
        case ItemType::kNecklace:
            return EquipSlot::kNecklace;
        case ItemType::kRing:
            return EquipSlot::kRing;
        case ItemType::kCostume:
            return EquipSlot::kCostume;
        default:
            return EquipSlot::kWeapon;  // 默认武器槽
    }
}

/**
 * @brief 根据Buff ID获取状态类型
 */
SpecialStateType GetStateTypeFromBuffID(uint32_t buff_id) {
    // 根据buff_id范围判断状态类型
    // 这里简化处理,实际应根据配置文件查询
    if (buff_id >= 1000 && buff_id < 2000) {
        return SpecialStateType::kBuff;
    } else if (buff_id >= 2000 && buff_id < 3000) {
        return SpecialStateType::kDebuff;
    } else if (buff_id >= 3000 && buff_id < 4000) {
        return SpecialStateType::kDot;
    } else if (buff_id >= 4000 && buff_id < 5000) {
        return SpecialStateType::kHot;
    }
    return SpecialStateType::kBuff;  // 默认为Buff
}

/**
 * @brief 检查位置是否有效
 */
bool IsValidPosition(float x, float y, float z) {
    // 基本边界检查
    const float MAX_COORD = 100000.0f;  // 最大坐标值
    const float MIN_COORD = -100000.0f;  // 最小坐标值

    if (x < MIN_COORD || x > MAX_COORD ||
        y < MIN_COORD || y > MAX_COORD ||
        z < MIN_COORD || z > MAX_COORD) {
        return false;
    }

    // TODO [需其他系统]: 更详细的位置验证
    // 需要地图系统和导航网格系统支持
    // - 检查是否在地图范围内
    // - 检查是否有障碍物
    // - 检查是否可以进行传送
    // - 检查目标位置是否安全
    // if (!MapSystem::IsInBounds(x, y, z)) {
    //     return false;
    // }
    // if (NavigationSystem::HasObstacle(x, y, z)) {
    //     return false;
    // }

    return true;
}

ItemUseHandler* ItemOperationManager::GetHandler(ItemKind kind) {
    auto it = handlers_.find(kind);
    return (it != handlers_.end()) ? it->second.get() : nullptr;
}

const ItemUseHandler* ItemOperationManager::GetHandler(ItemKind kind) const {
    auto it = handlers_.find(kind);
    return (it != handlers_.end()) ? it->second.get() : nullptr;
}

// ========== ItemCooldownManager ==========

ItemCooldownManager::ItemCooldownManager()
    : owner_id_(0) {
}

ItemCooldownManager::ItemCooldownManager(uint64_t owner_id)
    : owner_id_(owner_id) {
}

ItemCooldownManager::~ItemCooldownManager() {
}

void ItemCooldownManager::Init(uint64_t owner_id) {
    owner_id_ = owner_id;
    spdlog::debug("ItemCooldownManager: Initialized for owner {}", owner_id);
}

void ItemCooldownManager::SetCooldown(uint32_t item_id, uint32_t category_id, uint32_t cooldown_ms) {
    CooldownInfo info;
    info.item_id = item_id;
    info.category_id = category_id;
    info.cooldown_time = cooldown_ms;
    info.last_use_time = GetCurrentTime();

    cooldowns_[item_id] = info;

    spdlog::debug("[ItemCD-{}] Set: item={}, category={}, cooldown={}ms",
                 owner_id_, item_id, category_id, cooldown_ms);
}

bool ItemCooldownManager::IsOnCooldown(uint32_t item_id) const {
    auto it = cooldowns_.find(item_id);
    if (it == cooldowns_.end()) {
        return false;
    }

    uint32_t current_time = GetCurrentTime();
    uint32_t elapsed = current_time - it->second.last_use_time;

    return elapsed < it->second.cooldown_time;
}

bool ItemCooldownManager::IsOnCooldownByCategory(uint32_t category_id) const {
    uint32_t current_time = GetCurrentTime();

    for (const auto& pair : cooldowns_) {
        if (pair.second.category_id == category_id) {
            uint32_t elapsed = current_time - pair.second.last_use_time;
            if (elapsed < pair.second.cooldown_time) {
                return true;
            }
        }
    }

    return false;
}

uint32_t ItemCooldownManager::GetRemainingCooldown(uint32_t item_id) const {
    auto it = cooldowns_.find(item_id);
    if (it == cooldowns_.end()) {
        return 0;
    }

    uint32_t current_time = GetCurrentTime();
    uint32_t elapsed = current_time - it->second.last_use_time;

    if (elapsed >= it->second.cooldown_time) {
        return 0;
    }

    return it->second.cooldown_time - elapsed;
}

void ItemCooldownManager::UseItem(uint32_t item_id) {
    auto it = cooldowns_.find(item_id);
    if (it != cooldowns_.end()) {
        it->second.last_use_time = GetCurrentTime();
    }
}

void ItemCooldownManager::ClearCooldown(uint32_t item_id) {
    cooldowns_.erase(item_id);
    spdlog::debug("[ItemCD-{}] Cleared cooldown for item {}", owner_id_, item_id);
}

void ItemCooldownManager::ClearAllCooldowns() {
    cooldowns_.clear();
    spdlog::debug("[ItemCD-{}] Cleared all cooldowns", owner_id_);
}

uint32_t ItemCooldownManager::GetCurrentTime() const {
    // TODO [需其他系统]: 实现获取当前时间的函数
    // 需要使用服务器时间系统
    // return GameTimer::GetCurrentTimeMs() / 1000;

    // 临时实现：使用静态计数器（仅用于测试）
    static uint32_t time = 0;
    return time++;
}

} // namespace Game
} // namespace Murim
