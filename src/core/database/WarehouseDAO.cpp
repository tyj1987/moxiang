// Murim MMORPG - Warehouse DAO Implementation

#include "WarehouseDAO.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Database {

std::optional<WarehouseInfo> WarehouseDAO::LoadByCharacter(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return std::nullopt;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "SELECT warehouse_id, character_id, slots "
                         "FROM warehouses "
                         "WHERE character_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return std::nullopt;
        }

        WarehouseInfo info;
        info.character_id = result->Get<uint64_t>(0, "character_id").value_or(0);
        info.max_slots = result->Get<uint32_t>(0, "slots").value_or(50);
        info.used_slots = 0;
        info.money = 0;

        // Load items
        auto items_result = executor.QueryParams(
            "SELECT warehouse_id, slot_id, item_id, quantity "
            "FROM warehouse_items "
            "WHERE warehouse_id = $1 "
            "ORDER BY slot_id",
            {std::to_string(info.character_id)}
        );

        if (items_result && items_result->RowCount() > 0) {
            for (int i = 0; i < items_result->RowCount(); ++i) {
                WarehouseSlotInfo slot_info;
                slot_info.character_id = info.character_id;
                slot_info.slot_id = items_result->Get<uint32_t>(i, "slot_id").value_or(0);
                slot_info.item_template_id = items_result->Get<uint32_t>(i, "item_id").value_or(0);
                slot_info.quantity = items_result->Get<int16_t>(i, "quantity").value_or(0);
                slot_info.item_instance_id = 0;

                // ITEMBASE 字段 (默认值)
                slot_info.durability = 0;
                slot_info.rare_idx = 0;
                slot_info.item_param = 0;

                info.slots.push_back(slot_info);
                info.used_slots++;
            }
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        return info;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to load warehouse for character {}: {}", character_id, e.what());
        return std::nullopt;
    }
}

std::vector<WarehouseSlotInfo> WarehouseDAO::LoadSlots(uint64_t character_id) {
    std::vector<WarehouseSlotInfo> slots;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return slots;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "SELECT wi.warehouse_id, wi.slot_id, wi.item_id, wi.quantity "
                         "FROM warehouse_items wi "
                         "INNER JOIN warehouses w ON wi.warehouse_id = w.warehouse_id "
                         "WHERE w.character_id = $1 "
                         "ORDER BY wi.slot_id";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});

        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return slots;
        }

        for (int i = 0; i < result->RowCount(); ++i) {
            WarehouseSlotInfo slot_info;
            slot_info.character_id = character_id;
            slot_info.slot_id = result->Get<uint32_t>(i, "slot_id").value_or(0);
            slot_info.item_template_id = result->Get<uint32_t>(i, "item_id").value_or(0);
            slot_info.quantity = result->Get<int16_t>(i, "quantity").value_or(0);
            slot_info.item_instance_id = 0;

            // ITEMBASE 字段 (默认值)
            slot_info.durability = 0;
            slot_info.rare_idx = 0;
            slot_info.item_param = 0;

            slots.push_back(slot_info);
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::debug("Loaded {} warehouse slots for character {}", slots.size(), character_id);

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to load warehouse slots for character {}: {}", character_id, e.what());
    }

    return slots;
}

bool WarehouseDAO::Create(const WarehouseInfo& warehouse_info) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "INSERT INTO warehouses (character_id, slots) "
                         "VALUES ($1, $2)";

        bool success = executor.ExecuteParams(sql, {
            std::to_string(warehouse_info.character_id),
            std::to_string(warehouse_info.max_slots)
        });

        if (!success) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return false;
        }

        // Add items
        for (const auto& slot : warehouse_info.slots) {
            AddItem(slot);
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Created warehouse for character {}", warehouse_info.character_id);
        return true;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to create warehouse: {}", e.what());
        return false;
    }
}

bool WarehouseDAO::Update(const WarehouseInfo& warehouse_info) {
    // TODO: Implement if needed
    (void)warehouse_info;  // 消除未使用参数警告
    return true;
}

bool WarehouseDAO::Delete(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "DELETE FROM warehouses WHERE character_id = $1";

        bool success = executor.ExecuteParams(sql, {std::to_string(character_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        return success;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to delete warehouse: {}", e.what());
        return false;
    }
}

bool WarehouseDAO::AddItem(const WarehouseSlotInfo& slot_info) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);

        // Get warehouse_id from character_id
        auto warehouse_result = executor.QueryParams(
            "SELECT warehouse_id FROM warehouses WHERE character_id = $1",
            {std::to_string(slot_info.character_id)}
        );

        if (!warehouse_result || warehouse_result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            spdlog::error("Warehouse not found for character {}", slot_info.character_id);
            return false;
        }

        uint32_t warehouse_id = warehouse_result->Get<uint32_t>(0, "warehouse_id").value_or(0);

        std::string sql = "INSERT INTO warehouse_items (warehouse_id, slot_id, item_id, quantity) "
                         "VALUES ($1, $2, $3, $4)";

        bool success = executor.ExecuteParams(sql, {
            std::to_string(warehouse_id),
            std::to_string(slot_info.slot_id),
            std::to_string(slot_info.item_template_id),
            std::to_string(slot_info.quantity)
        });

        ConnectionPool::Instance().ReturnConnection(conn);
        return success;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to add item to warehouse: {}", e.what());
        return false;
    }
}

bool WarehouseDAO::RemoveItem(uint64_t character_id, uint32_t slot_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);

        // Get warehouse_id from character_id
        auto warehouse_result = executor.QueryParams(
            "SELECT warehouse_id FROM warehouses WHERE character_id = $1",
            {std::to_string(character_id)}
        );

        if (!warehouse_result || warehouse_result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return false;
        }

        uint32_t warehouse_id = warehouse_result->Get<uint32_t>(0, "warehouse_id").value_or(0);

        std::string sql = "DELETE FROM warehouse_items "
                         "WHERE warehouse_id = $1 AND slot_id = $2";

        bool success = executor.ExecuteParams(sql, {
            std::to_string(warehouse_id),
            std::to_string(slot_id)
        });

        ConnectionPool::Instance().ReturnConnection(conn);
        return success;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to remove item from warehouse: {}", e.what());
        return false;
    }
}

bool WarehouseDAO::UpdateItem(const WarehouseSlotInfo& slot_info) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);

        // Get warehouse_id from character_id
        auto warehouse_result = executor.QueryParams(
            "SELECT warehouse_id FROM warehouses WHERE character_id = $1",
            {std::to_string(slot_info.character_id)}
        );

        if (!warehouse_result || warehouse_result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return false;
        }

        uint32_t warehouse_id = warehouse_result->Get<uint32_t>(0, "warehouse_id").value_or(0);

        std::string sql = "UPDATE warehouse_items SET quantity = $1 "
                         "WHERE warehouse_id = $2 AND slot_id = $3";

        bool success = executor.ExecuteParams(sql, {
            std::to_string(slot_info.quantity),
            std::to_string(warehouse_id),
            std::to_string(slot_info.slot_id)
        });

        ConnectionPool::Instance().ReturnConnection(conn);
        return success;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to update warehouse item: {}", e.what());
        return false;
    }
}

bool WarehouseDAO::UpdateMoney(uint64_t character_id, uint64_t money) {
    // TODO: Implement if money column is added to warehouses table
    (void)character_id;  // 消除未使用参数警告
    (void)money;         // 消除未使用参数警告
    return true;
}

bool WarehouseDAO::ClearSlots(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);

        // Get warehouse_id from character_id
        auto warehouse_result = executor.QueryParams(
            "SELECT warehouse_id FROM warehouses WHERE character_id = $1",
            {std::to_string(character_id)}
        );

        if (!warehouse_result || warehouse_result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return false;
        }

        uint32_t warehouse_id = warehouse_result->Get<uint32_t>(0, "warehouse_id").value_or(0);

        std::string sql = "DELETE FROM warehouse_items WHERE warehouse_id = $1";

        bool success = executor.ExecuteParams(sql, {std::to_string(warehouse_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        return success;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to clear warehouse slots: {}", e.what());
        return false;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
