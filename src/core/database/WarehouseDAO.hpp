// Murim MMORPG - Warehouse DAO
// Responsible for warehouse data database access

#pragma once

#include <string>
#include <vector>
#include <optional>

namespace Murim {
namespace Core {
namespace Database {

// Warehouse slot structure (corresponds to warehouse_slots table)
struct WarehouseSlotInfo {
    uint64_t character_id;
    uint32_t slot_id;
    uint64_t item_instance_id;
    uint32_t item_template_id;
    std::string item_name;
    int16_t quantity;

    // ITEMBASE 字段 (匹配 ItemInstance)
    uint32_t durability = 0;     // 耐久度
    uint32_t rare_idx = 0;       // 稀有索引
    uint32_t item_param = 0;     // 物品参数
};

// Warehouse data structure (corresponds to warehouses table)
struct WarehouseInfo {
    uint64_t character_id;
    std::string character_name;
    uint32_t max_slots;
    uint32_t used_slots;
    uint64_t money;
    std::vector<WarehouseSlotInfo> slots;
};

// Warehouse DAO class
class WarehouseDAO {
public:
    // Load warehouse by character ID
    static std::optional<WarehouseInfo> LoadByCharacter(uint64_t character_id);

    // Load warehouse slots by character ID
    static std::vector<WarehouseSlotInfo> LoadSlots(uint64_t character_id);

    // Create warehouse
    static bool Create(const WarehouseInfo& warehouse_info);

    // Update warehouse
    static bool Update(const WarehouseInfo& warehouse_info);

    // Delete warehouse
    static bool Delete(uint64_t character_id);

    // Add item to warehouse slot
    static bool AddItem(const WarehouseSlotInfo& slot_info);

    // Remove item from warehouse slot
    static bool RemoveItem(uint64_t character_id, uint32_t slot_id);

    // Update item in warehouse slot
    static bool UpdateItem(const WarehouseSlotInfo& slot_info);

    // Update warehouse money
    static bool UpdateMoney(uint64_t character_id, uint64_t money);

    // Clear all warehouse slots for a character
    static bool ClearSlots(uint64_t character_id);
};

} // namespace Database
} // namespace Core
} // namespace Murim
