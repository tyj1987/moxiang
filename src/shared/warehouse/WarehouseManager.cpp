#include "WarehouseManager.hpp"
#include "../item/ItemManager.hpp"
#include "../character/CharacterManager.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>

namespace Murim {
namespace Game {

// ========== WarehouseInfo 实现 ==========

bool WarehouseInfo::AddItem(const WarehouseItem& item) {
    // 检查容量
    if (used_slots >= capacity) {
        return false;
    }

    items.push_back(item);
    UpdateUsedSlots();
    return true;
}

bool WarehouseInfo::RemoveItem(uint64_t item_instance_id) {
    auto it = std::remove_if(items.begin(), items.end(),
        [item_instance_id](const WarehouseItem& item) {
            return item.item_instance_id == item_instance_id;
        });

    if (it != items.end()) {
        items.erase(it, items.end());
        UpdateUsedSlots();
        return true;
    }
    return false;
}

WarehouseItem* WarehouseInfo::GetItem(uint64_t item_instance_id) {
    auto it = std::find_if(items.begin(), items.end(),
        [item_instance_id](const WarehouseItem& item) {
            return item.item_instance_id == item_instance_id;
        });

    if (it != items.end()) {
        return &(*it);
    }
    return nullptr;
}

uint32_t WarehouseInfo::FindEmptySlot() const {
    // 查找第一个空格子（未被占用的位置）
    for (uint32_t pos = 1; pos <= capacity; ++pos) {
        bool occupied = std::any_of(items.begin(), items.end(),
            [pos](const WarehouseItem& item) {
                return item.position == pos;
            });
        if (!occupied) {
            return pos;
        }
    }
    return 0;  // 没有空格子
}

void WarehouseInfo::UpdateUsedSlots() {
    used_slots = static_cast<uint32_t>(items.size());
}

// ========== WarehouseManager 实现 ==========

WarehouseManager& WarehouseManager::Instance() {
    static WarehouseManager instance;
    return instance;
}

WarehouseManager::WarehouseManager() {
    spdlog::info("WarehouseManager created");
}

bool WarehouseManager::Initialize() {
    spdlog::info("Initializing WarehouseManager...");

    // TODO: 从数据库加载所有仓库数据
    // 目前使用空状态，玩家第一次使用时自动创建

    spdlog::info("WarehouseManager initialized: {} warehouses", warehouses_.size());
    return true;
}

std::shared_ptr<WarehouseInfo> WarehouseManager::GetWarehouse(uint64_t character_id) {
    auto it = warehouses_.find(character_id);
    if (it != warehouses_.end()) {
        return it->second;
    }

    // 尝试从数据库加载
    if (LoadWarehouseFromDatabase(character_id)) {
        return warehouses_[character_id];
    }

    return nullptr;
}

bool WarehouseManager::CreateWarehouse(uint64_t character_id, uint32_t capacity) {
    if (HasWarehouse(character_id)) {
        spdlog::warn("Warehouse already exists for character {}", character_id);
        return false;
    }

    auto warehouse = std::make_shared<WarehouseInfo>();
    warehouse->character_id = character_id;
    warehouse->capacity = capacity;
    warehouse->used_slots = 0;
    warehouse->gold = 0;

    warehouses_[character_id] = warehouse;

    spdlog::info("Created warehouse for character {} with capacity {}",
                 character_id, capacity);

    // TODO: 保存到数据库
    return true;
}

bool WarehouseManager::HasWarehouse(uint64_t character_id) const {
    return warehouses_.find(character_id) != warehouses_.end();
}

// ========== 物品操作 ==========

WarehouseResult WarehouseManager::HandleDepositItem(
    uint64_t character_id,
    uint64_t item_instance_id,
    uint32_t item_id,
    uint32_t count) {

    WarehouseResult result;
    result.success = false;

    // 获取或创建仓库
    auto warehouse = GetWarehouse(character_id);
    if (!warehouse) {
        // 自动创建仓库
        if (!CreateWarehouse(character_id, 50)) {
            result.error_code = 1;
            result.error_message = "无法创建仓库";
            return result;
        }
        warehouse = warehouses_[character_id];
    }

    // 验证操作
    if (!ValidateWarehouseOperation(character_id)) {
        result.error_code = 2;
        result.error_message = "仓库验证失败";
        return result;
    }

    // 检查容量
    if (warehouse->used_slots >= warehouse->capacity) {
        result.error_code = 3;
        result.error_message = "仓库已满";
        return result;
    }

    // 执行存入
    return ExecuteDeposit(character_id, item_instance_id, item_id, count);
}

WarehouseResult WarehouseManager::HandleWithdrawItem(
    uint64_t character_id,
    uint32_t slot_id) {

    WarehouseResult result;
    result.success = false;

    // 获取仓库
    auto warehouse = GetWarehouse(character_id);
    if (!warehouse) {
        result.error_code = 1;
        result.error_message = "仓库不存在";
        return result;
    }

    // 验证操作
    if (!ValidateWarehouseOperation(character_id)) {
        result.error_code = 2;
        result.error_message = "仓库验证失败";
        return result;
    }

    // 通过 slot_id 查找物品
    uint64_t item_instance_id = 0;
    for (const auto& item : warehouse->items) {
        if (item.position == slot_id) {
            item_instance_id = item.item_instance_id;
            break;
        }
    }

    if (item_instance_id == 0) {
        result.error_code = 4;
        result.error_message = "栏位中没有物品";
        return result;
    }

    // 执行取出
    return ExecuteWithdraw(character_id, item_instance_id);
}

bool WarehouseManager::HandleSortWarehouse(uint64_t character_id) {
    auto warehouse = GetWarehouse(character_id);
    if (!warehouse) {
        return false;
    }

    // 按位置排序
    std::sort(warehouse->items.begin(), warehouse->items.end(),
        [](const WarehouseItem& a, const WarehouseItem& b) {
            return a.position < b.position;
        });

    // 重新分配位置
    uint32_t pos = 1;
    for (auto& item : warehouse->items) {
        if (!item.is_locked) {
            item.position = pos++;
        }
    }

    spdlog::info("Sorted warehouse for character {}", character_id);

    // TODO: 保存到数据库
    return true;
}

// ========== 金币操作 ==========

WarehouseResult WarehouseManager::HandleDepositGold(uint64_t character_id, uint64_t amount) {
    WarehouseResult result;
    result.success = false;

    auto warehouse = GetWarehouse(character_id);
    if (!warehouse) {
        result.error_code = 1;
        result.error_message = "仓库不存在";
        return result;
    }

    // TODO: 从玩家扣除金币
    warehouse->gold += amount;

    result.success = true;
    result.error_code = 0;
    result.new_gold = warehouse->gold;

    spdlog::info("Character {} deposited {} gold to warehouse (total: {})",
                 character_id, amount, warehouse->gold);

    // TODO: 保存到数据库
    return result;
}

WarehouseResult WarehouseManager::HandleWithdrawGold(uint64_t character_id, uint64_t amount) {
    WarehouseResult result;
    result.success = false;

    auto warehouse = GetWarehouse(character_id);
    if (!warehouse) {
        result.error_code = 1;
        result.error_message = "仓库不存在";
        return result;
    }

    // 检查余额
    if (warehouse->gold < amount) {
        result.error_code = 2;
        result.error_message = "仓库金币不足";
        return result;
    }

    warehouse->gold -= amount;

    result.success = true;
    result.error_code = 0;
    result.new_gold = warehouse->gold;

    spdlog::info("Character {} withdrew {} gold from warehouse (remaining: {})",
                 character_id, amount, warehouse->gold);

    // TODO: 保存到数据库并添加金币到玩家
    return result;
}

// ========== 辅助方法 ==========

bool WarehouseManager::LoadWarehouseFromDatabase(uint64_t character_id) {
    // TODO: 从数据库加载仓库数据
    // 使用 CharacterDAO 或专门的 WarehouseDAO
    spdlog::debug("Loading warehouse from database for character {}", character_id);
    return false;
}

bool WarehouseManager::SaveWarehouseToDatabase(uint64_t character_id) {
    // TODO: 保存仓库数据到数据库
    spdlog::debug("Saving warehouse to database for character {}", character_id);
    return true;
}

bool WarehouseManager::ValidateWarehouseOperation(uint64_t character_id) {
    // TODO: 验证角色状态、仓库锁定等
    return true;
}

WarehouseResult WarehouseManager::ExecuteDeposit(
    uint64_t character_id,
    uint64_t item_instance_id,
    uint32_t item_id,
    uint32_t count) {

    WarehouseResult result;
    result.success = false;

    auto warehouse = warehouses_[character_id];

    // 查找空格子
    uint32_t position = warehouse->FindEmptySlot();
    if (position == 0) {
        result.error_code = 5;
        result.error_message = "没有空格子";
        return result;
    }

    // TODO: 从玩家背包移除物品

    // 添加到仓库
    WarehouseItem item;
    item.item_instance_id = item_instance_id;
    item.item_id = item_id;
    item.count = count;
    item.position = position;
    item.is_locked = false;

    if (!warehouse->AddItem(item)) {
        result.error_code = 6;
        result.error_message = "添加物品失败";
        return result;
    }

    result.success = true;
    result.error_code = 0;
    result.item_instance_id = item_instance_id;
    result.position = position;

    spdlog::info("Character {} deposited item {} to warehouse position {}",
                 character_id, item_instance_id, position);

    // TODO: 保存到数据库
    return result;
}

WarehouseResult WarehouseManager::ExecuteWithdraw(
    uint64_t character_id,
    uint64_t item_instance_id) {

    WarehouseResult result;
    result.success = false;

    auto warehouse = warehouses_[character_id];

    // 获取物品信息
    WarehouseItem* item = warehouse->GetItem(item_instance_id);
    if (!item) {
        result.error_code = 5;
        result.error_message = "物品不存在";
        return result;
    }

    // TODO: 检查玩家背包是否有空位

    // 从仓库移除
    if (!warehouse->RemoveItem(item_instance_id)) {
        result.error_code = 6;
        result.error_message = "移除物品失败";
        return result;
    }

    // TODO: 添加物品到玩家背包

    result.success = true;
    result.error_code = 0;
    result.item_instance_id = item_instance_id;
    result.position = item->position;

    spdlog::info("Character {} withdrew item {} from warehouse position {}",
                 character_id, item_instance_id, item->position);

    // TODO: 保存到数据库
    return result;
}

} // namespace Game
} // namespace Murim
