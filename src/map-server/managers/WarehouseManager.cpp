// Murim MMORPG - 仓库系统实现

#include "WarehouseManager.hpp"
#include "shared/item/ItemManager.hpp"
#include "core/database/WarehouseDAO.hpp"
#include "../../core/spdlog_wrapper.hpp"

namespace Murim {
namespace MapServer {

WarehouseManager& WarehouseManager::Instance() {
    static WarehouseManager instance;
    return instance;
}

bool WarehouseManager::Initialize() {
    spdlog::info("Initializing WarehouseManager...");
    spdlog::info("WarehouseManager initialized");
    return true;
}

void WarehouseManager::Shutdown() {
    spdlog::info("Shutting down WarehouseManager...");
    warehouses_.clear();
    player_warehouses_.clear();
}

bool WarehouseManager::OpenWarehouse(uint64_t player_id, uint32_t npc_id) {
    spdlog::info("Player {} opening warehouse at NPC {}", player_id, npc_id);

    // 加载仓库数据
    LoadWarehouseFromDatabase(player_id);

    // 记录打开的仓库
    player_warehouses_[player_id] = npc_id;

    // TODO: 发送网络消息给客户端，打开仓库UI
    return true;
}

bool WarehouseManager::CloseWarehouse(uint64_t player_id) {
    spdlog::info("Player {} closing warehouse", player_id);

    // 保存仓库数据
    SaveWarehouseToDatabase(player_id);

    // 关闭仓库
    player_warehouses_.erase(player_id);

    // TODO: 发送网络消息给客户端，关闭仓库UI
    return true;
}

WarehouseManager::DepositResult WarehouseManager::DepositItem(
    uint64_t player_id,
    uint64_t item_instance_id
) {
    DepositResult result;
    result.success = false;

    // TODO: 检查物品是否存在
    // TODO: 检查是否已绑定
    // TODO: 从背包移除物品
    // TODO: 添加到仓库

    result.success = true;
    result.message = "物品已存入仓库";

    spdlog::info("Player {} deposited item {} to warehouse", player_id, item_instance_id);
    return result;
}

WarehouseManager::WithdrawResult WarehouseManager::WithdrawItem(
    uint64_t player_id,
    uint32_t slot_id
) {
    WithdrawResult result;
    result.success = false;

    // 获取仓库数据
    auto warehouse = GetWarehouse(player_id);
    if (!warehouse) {
        result.message = "仓库未打开";
        return result;
    }

    // 查找栏位
    if (slot_id >= warehouse->slots.size()) {
        result.message = "无效的栏位";
        return result;
    }

    const WarehouseSlot& slot = warehouse->slots[slot_id];
    if (slot.item_instance_id == 0) {
        result.message = "栏位为空";
        return result;
    }

    // TODO: 检查背包空间
    // TODO: 从仓库移除物品
    // TODO: 添加到背包

    result.success = true;
    result.message = "物品已取出";
    result.item_instance_id = slot.item_instance_id;

    spdlog::info("Player {} withdrew item {} from slot {}", player_id, slot.item_instance_id, slot_id);
    return result;
}

bool WarehouseManager::SortWarehouse(uint64_t player_id) {
    spdlog::info("Player {} sorting warehouse", player_id);

    // TODO: 实现排序逻辑
    // 按物品类型、品质、名称排序

    return true;
}

bool WarehouseManager::ExpandWarehouse(uint64_t player_id, uint32_t additional_slots) {
    spdlog::info("Player {} expanding warehouse by {} slots", player_id, additional_slots);

    // TODO: 检查玩家金钱
    // TODO: 扣除金钱
    // TODO: 增加仓库栏位上限

    return true;
}

const WarehouseData* WarehouseManager::GetWarehouse(uint64_t player_id) const {
    auto it = warehouses_.find(player_id);
    if (it != warehouses_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool WarehouseManager::DepositMoney(uint64_t player_id, uint64_t amount) {
    // TODO: 从玩家背包扣除金币
    // TODO: 添加到仓库金币
    (void)player_id;  // 消除未使用参数警告
    (void)amount;     // 消除未使用参数警告
    return true;
}

bool WarehouseManager::WithdrawMoney(uint64_t player_id, uint64_t amount) {
    // TODO: 从仓库金币扣除
    // TODO: 添加到玩家背包
    (void)player_id;  // 消除未使用参数警告
    (void)amount;     // 消除未使用参数警告
    return true;
}

uint64_t WarehouseManager::GetWarehouseMoney(uint64_t player_id) const {
    auto warehouse = GetWarehouse(player_id);
    if (warehouse) {
        return warehouse->money;
    }
    return 0;
}

void WarehouseManager::LoadWarehouseFromDatabase(uint64_t player_id) {
    spdlog::info("Loading warehouse for player {}", player_id);

    try {
        // Try to load from database
        auto warehouse_info = Murim::Core::Database::WarehouseDAO::LoadByCharacter(player_id);

        if (warehouse_info) {
            // Convert to WarehouseData
            WarehouseData data;
            data.character_id = warehouse_info->character_id;
            data.character_name = warehouse_info->character_name;
            data.max_slots = warehouse_info->max_slots;
            data.used_slots = warehouse_info->used_slots;
            data.money = warehouse_info->money;

            // Convert slots
            for (const auto& slot_info : warehouse_info->slots) {
                WarehouseSlot slot;
                slot.slot_id = slot_info.slot_id;
                slot.item_instance_id = slot_info.item_instance_id;
                slot.item_template_id = slot_info.item_template_id;
                slot.item_name = slot_info.item_name;
                slot.quantity = slot_info.quantity;

                // ITEMBASE 字段
                slot.durability = slot_info.durability;
                slot.rare_idx = slot_info.rare_idx;
                slot.item_param = slot_info.item_param;

                data.slots.push_back(slot);
            }

            warehouses_[player_id] = data;
            spdlog::info("Warehouse loaded for player {}: {} items", player_id, data.used_slots);

        } else {
            // Create new warehouse
            spdlog::info("Creating new warehouse for player {}", player_id);

            WarehouseData data;
            data.character_id = player_id;
            data.character_name = "";
            data.max_slots = 100;
            data.used_slots = 0;
            data.money = 0;
            data.slots.resize(100);  // Initialize 100 slots

            warehouses_[player_id] = data;

            // Save to database
            SaveWarehouseToDatabase(player_id);
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to load warehouse for player {}: {}", player_id, e.what());

        // Create empty warehouse in memory
        WarehouseData data;
        data.character_id = player_id;
        data.character_name = "";
        data.max_slots = 100;
        data.used_slots = 0;
        data.money = 0;
        data.slots.resize(100);

        warehouses_[player_id] = data;
    }
}

bool WarehouseManager::SaveWarehouseToDatabase(uint64_t player_id) {
    auto warehouse = GetWarehouse(player_id);
    if (!warehouse) {
        spdlog::error("Cannot save warehouse: player {} not found", player_id);
        return false;
    }

    try {
        // Convert to WarehouseInfo
        Murim::Core::Database::WarehouseInfo info;
        info.character_id = warehouse->character_id;
        info.character_name = warehouse->character_name;
        info.max_slots = warehouse->max_slots;
        info.used_slots = warehouse->used_slots;
        info.money = warehouse->money;

        // Convert slots
        for (const auto& slot : warehouse->slots) {
            if (slot.item_instance_id != 0) {  // Only save non-empty slots
                Murim::Core::Database::WarehouseSlotInfo slot_info;
                slot_info.character_id = player_id;
                slot_info.slot_id = slot.slot_id;
                slot_info.item_instance_id = slot.item_instance_id;
                slot_info.item_template_id = slot.item_template_id;
                slot_info.item_name = slot.item_name;
                slot_info.quantity = slot.quantity;

                // ITEMBASE 字段
                slot_info.durability = slot.durability;
                slot_info.rare_idx = slot.rare_idx;
                slot_info.item_param = slot.item_param;

                info.slots.push_back(slot_info);
            }
        }

        // Save to database
        bool success = Murim::Core::Database::WarehouseDAO::Update(info);

        if (success) {
            spdlog::info("Warehouse saved for player {}", player_id);
        } else {
            spdlog::error("Failed to save warehouse for player {}", player_id);
        }

        return success;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save warehouse for player {}: {}", player_id, e.what());
        return false;
    }
}

} // namespace MapServer
} // namespace Murim
