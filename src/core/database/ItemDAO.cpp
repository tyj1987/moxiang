#include "ItemDAO.hpp"
#include "core/spdlog_wrapper.hpp"
#include <sstream>

namespace Murim {
namespace Core {
namespace Database {

// 引用 Core::Database 中的类型
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

// 静态成员初始化
std::unordered_map<uint32_t, Game::ItemTemplate> ItemDAO::item_template_cache_;
bool ItemDAO::cache_loaded_ = false;

// ========== 物品模板操作 ==========

std::optional<Game::ItemTemplate> ItemDAO::LoadItemTemplate(uint32_t item_id) {
    // 先从缓存查找
    if (cache_loaded_) {
        auto it = item_template_cache_.find(item_id);
        if (it != item_template_cache_.end()) {
            return it->second;
        }
    }

    // 从数据库加载
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT item_id, item_name, item_type, item_subtype, quality, "
                          "required_level, required_job_class, "
                          "physical_attack, physical_defense, magic_attack, magic_defense, "
                          "hp_bonus, mp_bonus, price, "
                          "is_tradeable, is_dropable, stack_limit, description "
                          "FROM item_templates WHERE item_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(item_id)});

        if (result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return std::nullopt;
        }

        auto item_template = RowToItemTemplate(*result, 0);

        // 添加到缓存
        if (item_template.has_value()) {
            item_template_cache_[item_id] = item_template.value();
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        return item_template;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load item template {}: {}", item_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

std::vector<Game::ItemTemplate> ItemDAO::LoadAllItemTemplates() {
    std::vector<Game::ItemTemplate> templates;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return templates;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT item_id, item_name, item_type, item_subtype, quality, "
                          "required_level, required_job_class, "
                          "physical_attack, physical_defense, magic_attack, magic_defense, "
                          "hp_bonus, mp_bonus, price, "
                          "is_tradeable, is_dropable, stack_limit, description "
                          "FROM item_templates ORDER BY item_id";

        auto result = executor.Query(sql);

        for (int i = 0; i < result->RowCount(); ++i) {
            if (auto item_template = RowToItemTemplate(*result, i)) {
                templates.push_back(*item_template);
                item_template_cache_[item_template->item_id] = *item_template;
            }
        }

        cache_loaded_ = true;
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Loaded {} item templates", templates.size());

    } catch (const std::exception& e) {
        spdlog::error("Failed to load item templates: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
    }

    return templates;
}

bool ItemDAO::SaveItemTemplate(const Game::ItemTemplate& item_template) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "INSERT INTO item_templates (item_id, item_name, item_type, item_subtype, quality, "
                         "required_level, required_job_class, "
                         "physical_attack, physical_defense, magic_attack, magic_defense, "
                         "hp_bonus, mp_bonus, price, "
                         "is_tradeable, is_dropable, stack_limit, description) "
                         "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17) "
                         "ON CONFLICT (item_id) DO UPDATE SET "
                         "item_name = EXCLUDED.item_name, "
                         "item_type = EXCLUDED.item_type, "
                         "item_subtype = EXCLUDED.item_subtype, "
                         "quality = EXCLUDED.quality, "
                         "required_level = EXCLUDED.required_level, "
                         "required_job_class = EXCLUDED.required_job_class, "
                         "physical_attack = EXCLUDED.physical_attack, "
                         "physical_defense = EXCLUDED.physical_defense, "
                         "magic_attack = EXCLUDED.magic_attack, "
                         "magic_defense = EXCLUDED.magic_defense, "
                         "hp_bonus = EXCLUDED.hp_bonus, "
                         "mp_bonus = EXCLUDED.mp_bonus, "
                         "price = EXCLUDED.price, "
                         "is_tradeable = EXCLUDED.is_tradeable, "
                         "is_dropable = EXCLUDED.is_dropable, "
                         "stack_limit = EXCLUDED.stack_limit, "
                         "description = EXCLUDED.description";

        size_t affected = executor.ExecuteParams(sql,
            {std::to_string(item_template.item_id),
             item_template.name,
             std::to_string(static_cast<uint16_t>(item_template.item_type)),
             "0", // item_subtype - placeholder
             std::to_string(static_cast<uint16_t>(item_template.quality)),
             std::to_string(item_template.limit_level.value_or(0)),
             std::to_string(item_template.limit_job.value_or(0)),
             "0", // physical_attack - placeholder
             "0", // physical_defense - placeholder
             "0", // magic_attack - placeholder
             "0", // magic_defense - placeholder
             "0", // hp_bonus - placeholder
             "0", // mp_bonus - placeholder
             std::to_string(item_template.buy_price),
             "true", // is_tradeable - placeholder
             "true", // is_dropable - placeholder
             "0" // stack_limit - placeholder
            }
        );

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Saved item template: {}", item_template.item_id);
        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save item template {}: {}", item_template.item_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

// ========== 物品实例操作 ==========

uint64_t ItemDAO::CreateItemInstance(uint32_t item_id, uint16_t quantity) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return 0;
    }

    QueryExecutor executor(conn);

    try {
        // 创建物品实例，初始化所有 ITEMBASE 字段为默认值
        std::string sql = "INSERT INTO item_instances "
                          "(item_id, quantity, durability, rare_idx, quick_position, item_param) "
                          "VALUES ($1, $2, 0, 0, 0, 0) "
                          "RETURNING instance_id";

        auto result = executor.QueryParams(sql, {std::to_string(item_id), std::to_string(quantity)});
        uint64_t instance_id = result->Get<uint64_t>(0, "instance_id").value_or(0);

        spdlog::info("Created item instance: id={}, item_id={}, quantity={}",
                     instance_id, item_id, quantity);

        ConnectionPool::Instance().ReturnConnection(conn);
        return instance_id;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create item instance: {}", e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return 0;
    }
}

std::optional<Game::ItemInstance> ItemDAO::LoadItemInstance(uint64_t instance_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        // 查询所有 ITEMBASE 字段
        std::string sql = "SELECT instance_id, item_id, quantity, create_time, "
                          "durability, rare_idx, quick_position, item_param "
                          "FROM item_instances WHERE instance_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(instance_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return RowToItemInstance(*result, 0);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load item instance {}: {}", instance_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

bool ItemDAO::SaveItemInstance(const Game::ItemInstance& item_instance) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 保存所有 ITEMBASE 字段
        std::string sql = "UPDATE item_instances SET "
                         "item_id = $1, "
                         "quantity = $2, "
                         "durability = $3, "
                         "rare_idx = $4, "
                         "quick_position = $5, "
                         "item_param = $6 "
                         "WHERE instance_id = $7";

        size_t affected = executor.ExecuteParams(sql,
            {std::to_string(item_instance.item_id),
             std::to_string(item_instance.quantity),
             std::to_string(item_instance.durability),
             std::to_string(item_instance.rare_idx),
             std::to_string(item_instance.quick_position),
             std::to_string(item_instance.item_param),
             std::to_string(item_instance.instance_id)}
        );

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::debug("Saved item instance: {}", item_instance.instance_id);
        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save item instance {}: {}", item_instance.instance_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool ItemDAO::DeleteItemInstance(uint64_t instance_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "DELETE FROM item_instances WHERE item_instance_id = $1";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(instance_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Deleted item instance: {}", instance_id);
        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to delete item instance {}: {}", instance_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

// ========== 背包操作 ==========

std::vector<Game::InventorySlot> ItemDAO::LoadInventory(uint64_t character_id) {
    std::vector<Game::InventorySlot> inventory;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return inventory;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT inventory_id, character_id, item_instance_id, item_id, "
                          "slot_index, quantity "
                          "FROM inventory "
                          "WHERE character_id = $1 "
                          "ORDER BY slot_index";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        for (int i = 0; i < result->RowCount(); ++i) {
            if (auto slot = RowToInventorySlot(*result, i)) {
                inventory.push_back(*slot);
            }
        }

        spdlog::info("Loaded {} inventory slots for character {}", inventory.size(), character_id);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load inventory for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
    }

    return inventory;
}

bool ItemDAO::AddToInventory(uint64_t character_id, uint64_t item_instance_id,
                             uint32_t item_id, uint16_t slot_index, uint16_t quantity) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "INSERT INTO inventory (character_id, item_instance_id, item_id, slot_index, quantity) "
                          "VALUES ($1, $2, $3, $4, $5) "
                          "ON CONFLICT (character_id, slot_index) DO UPDATE SET "
                          "item_instance_id = EXCLUDED.item_instance_id, "
                          "item_id = EXCLUDED.item_id, "
                          "quantity = EXCLUDED.quantity";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(character_id), std::to_string(item_instance_id), std::to_string(item_id), std::to_string(slot_index), std::to_string(quantity)});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Added item to inventory: character_id={}, slot_index={}, item_id={}",
                     character_id, slot_index, item_id);
        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to add item to inventory for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool ItemDAO::RemoveFromInventory(uint64_t character_id, uint16_t slot_index, uint16_t quantity) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "UPDATE inventory SET quantity = quantity - $1 "
                          "WHERE character_id = $2 AND slot_index = $3 AND quantity >= $1";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(quantity), std::to_string(character_id), std::to_string(slot_index)});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Removed {} items from inventory: character_id={}, slot_index={}",
                     quantity, character_id, slot_index);
        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to remove item from inventory for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool ItemDAO::MoveItemInInventory(uint64_t character_id, uint16_t from_slot, uint16_t to_slot) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        // 使用事务确保原子性
        executor.BeginTransaction();

        // 获取源槽位数据
        std::string select_sql = "SELECT item_instance_id, item_id, quantity "
                                  "FROM inventory "
                                  "WHERE character_id = $1 AND slot_index = $2";

        auto from_result = executor.QueryParams(select_sql, {std::to_string(character_id), std::to_string(from_slot)});
        uint64_t item_instance_id = from_result->Get<uint64_t>(0, "item_instance_id").value_or(0);
        uint32_t item_id = from_result->Get<uint32_t>(0, "item_id").value_or(0);
        uint16_t quantity = from_result->Get<uint16_t>(0, "quantity").value_or(0);

        // 获取目标槽位数据
        std::shared_ptr<Result> to_result;
        bool has_target = false;
        try {
            to_result = executor.QueryParams(select_sql, {std::to_string(character_id), std::to_string(to_slot)});
            has_target = to_result->RowCount() > 0;
        } catch (...) {
            // 目标槽位为空
        }

        if (has_target) {
            // 交换两个槽位
            uint64_t to_item_instance_id = to_result->Get<uint64_t>(0, "item_instance_id").value_or(0);
            uint32_t to_item_id = to_result->Get<uint32_t>(0, "item_id").value_or(0);
            uint16_t to_quantity = to_result->Get<uint16_t>(0, "quantity").value_or(0);

            // 更新源槽位为目标槽位的数据
            std::string update_from_sql = "UPDATE inventory SET "
                                           "item_instance_id = $1, item_id = $2, quantity = $3 "
                                           "WHERE character_id = $4 AND slot_index = $5";
            executor.ExecuteParams(update_from_sql, {std::to_string(to_item_instance_id), std::to_string(to_item_id), std::to_string(to_quantity), std::to_string(character_id), std::to_string(from_slot)});

            // 更新目标槽位为源槽位的数据
            std::string update_to_sql = "UPDATE inventory SET "
                                         "item_instance_id = $1, item_id = $2, quantity = $3 "
                                         "WHERE character_id = $4 AND slot_index = $5";
            executor.ExecuteParams(update_to_sql, {std::to_string(item_instance_id), std::to_string(item_id), std::to_string(quantity), std::to_string(character_id), std::to_string(to_slot)});
        } else {
            // 移动到空槽位
            std::string update_from_sql = "DELETE FROM inventory "
                                           "WHERE character_id = $1 AND slot_index = $2";
            executor.ExecuteParams(update_from_sql, {std::to_string(character_id), std::to_string(from_slot)});

            std::string insert_to_sql = "INSERT INTO inventory (character_id, item_instance_id, item_id, slot_index, quantity) "
                                         "VALUES ($1, $2, $3, $4, $5)";
            executor.ExecuteParams(insert_to_sql, {std::to_string(character_id), std::to_string(item_instance_id), std::to_string(item_id), std::to_string(to_slot), std::to_string(quantity)});
        }

        executor.Commit();
        ConnectionPool::Instance().ReturnConnection(conn);

        spdlog::info("Moved item in inventory: character_id={}, from_slot={}, to_slot={}",
                     character_id, from_slot, to_slot);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to move item in inventory for character {}: {}", character_id, e.what());
        executor.Rollback();
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool ItemDAO::UpdateInventorySlot(const Game::InventorySlot& slot) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "UPDATE inventory SET "
                          "item_instance_id = $1, "
                          "item_id = $2, "
                          "quantity = $3 "
                          "WHERE character_id = $4 AND slot_index = $5";

        size_t affected = executor.ExecuteParams(sql,
            {std::to_string(slot.item_instance_id),
             std::to_string(slot.item_id),
             std::to_string(slot.quantity),
             std::to_string(slot.character_id),
             std::to_string(slot.slot_index)}
        );

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Updated inventory slot: character_id={}, slot={}", slot.character_id, slot.slot_index);
        return affected > 0;

    } catch (const std::exception& e) {
        // 消除格式化字符串警告，使用显式拼接
        std::string error_msg = "Failed to update inventory slot [character_id=" +
                                  std::to_string(slot.character_id) +
                                  ", slot=" + std::to_string(slot.slot_index) +
                                  "]: " + e.what();
        spdlog::error("{}", error_msg);
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool ItemDAO::ClearInventory(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "DELETE FROM inventory WHERE character_id = $1";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(character_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Cleared inventory for character {}, deleted {} slots", character_id, affected);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to clear inventory for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

// ========== 装备操作 ==========

std::optional<Game::EquipmentSlot> ItemDAO::LoadEquipment(uint64_t character_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return std::nullopt;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "SELECT character_id, "
                          "equip_slot_0, equip_slot_1, equip_slot_2, equip_slot_3, equip_slot_4, "
                          "equip_slot_5, equip_slot_6, equip_slot_7, equip_slot_8, equip_slot_9 "
                          "FROM equipment "
                          "WHERE character_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(character_id)});
        ConnectionPool::Instance().ReturnConnection(conn);

        return RowToEquipmentSlot(*result, 0);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load equipment for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return std::nullopt;
    }
}

bool ItemDAO::EquipItem(uint64_t character_id, Game::EquipSlot slot, uint64_t item_instance_id) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        size_t slot_index = static_cast<size_t>(slot);
        std::string column_name = "equip_slot_" + std::to_string(slot_index);

        std::string sql = "INSERT INTO equipment (character_id, " + column_name + ") "
                          "VALUES ($1, $2) "
                          "ON CONFLICT (character_id) DO UPDATE SET "
                          + column_name + " = EXCLUDED." + column_name;

        size_t affected = executor.ExecuteParams(sql, {std::to_string(character_id), std::to_string(item_instance_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Equipped item: character_id={}, slot={}, item_instance_id={}",
                     character_id, slot_index, item_instance_id);
        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to equip item for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}
bool ItemDAO::UnequipItem(uint64_t character_id, Game::EquipSlot slot) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        size_t slot_index = static_cast<size_t>(slot);
        std::string column_name = "equip_slot_" + std::to_string(slot_index);

        std::string sql = "UPDATE equipment SET " + column_name + " = NULL "
                          "WHERE character_id = $1";

        size_t affected = executor.ExecuteParams(sql, {std::to_string(character_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Unequipped item: character_id={}, slot={}", character_id, slot_index);
        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to unequip item for character {}: {}", character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

bool ItemDAO::SaveEquipment(const Game::EquipmentSlot& equipment) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        return false;
    }

    QueryExecutor executor(conn);

    try {
        std::string sql = "INSERT INTO equipment (character_id, "
                          "equip_slot_0, equip_slot_1, equip_slot_2, equip_slot_3, equip_slot_4, "
                          "equip_slot_5, equip_slot_6, equip_slot_7, equip_slot_8, equip_slot_9) "
                          "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11) "
                          "ON CONFLICT (character_id) DO UPDATE SET "
                          "equip_slot_0 = EXCLUDED.equip_slot_0, "
                          "equip_slot_1 = EXCLUDED.equip_slot_1, "
                          "equip_slot_2 = EXCLUDED.equip_slot_2, "
                          "equip_slot_3 = EXCLUDED.equip_slot_3, "
                          "equip_slot_4 = EXCLUDED.equip_slot_4, "
                          "equip_slot_5 = EXCLUDED.equip_slot_5, "
                          "equip_slot_6 = EXCLUDED.equip_slot_6, "
                          "equip_slot_7 = EXCLUDED.equip_slot_7, "
                          "equip_slot_8 = EXCLUDED.equip_slot_8, "
                          "equip_slot_9 = EXCLUDED.equip_slot_9";

        size_t affected = executor.ExecuteParams(sql,
            {std::to_string(equipment.character_id),
             std::to_string(equipment.equip_slots[0].value_or(0)),
             std::to_string(equipment.equip_slots[1].value_or(0)),
             std::to_string(equipment.equip_slots[2].value_or(0)),
             std::to_string(equipment.equip_slots[3].value_or(0)),
             std::to_string(equipment.equip_slots[4].value_or(0)),
             std::to_string(equipment.equip_slots[5].value_or(0)),
             std::to_string(equipment.equip_slots[6].value_or(0)),
             std::to_string(equipment.equip_slots[7].value_or(0)),
             std::to_string(equipment.equip_slots[8].value_or(0)),
             std::to_string(equipment.equip_slots[9].value_or(0))}
        );

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Saved equipment for character {}", equipment.character_id);
        return affected > 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save equipment for character {}: {}", equipment.character_id, e.what());
        ConnectionPool::Instance().ReturnConnection(conn);
        return false;
    }
}

// ========== 辅助方法 ==========

std::optional<Game::ItemTemplate> ItemDAO::RowToItemTemplate(Result& result, int row) {
    try {
        Game::ItemTemplate item_template;

        item_template.item_id = result.Get<uint32_t>(row, "item_id").value_or(0);
        item_template.name = result.GetValue(row, "item_name");
        item_template.item_type = static_cast<Game::ItemType>(result.Get<uint16_t>(row, "item_type").value_or(0));
        item_template.quality = static_cast<Game::ItemQuality>(result.Get<uint16_t>(row, "quality").value_or(0));
        item_template.buy_price = result.Get<uint32_t>(row, "price").value_or(0);

        // 简化实现,实际需要解析所有字段
        // ...

        return item_template;

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse item template row: {}", e.what());
        return std::nullopt;
    }
}

std::optional<Game::ItemInstance> ItemDAO::RowToItemInstance(Result& result, int row) {
    try {
        Game::ItemInstance item_instance;

        // ICONBASE 字段
        item_instance.instance_id = result.Get<uint64_t>(row, "instance_id").value_or(0);
        item_instance.item_id = result.Get<uint32_t>(row, "item_id").value_or(0);
        // slot_index 不存储在 item_instances 表中，而是从 inventory 表获取

        // ITEMBASE 字段 (完全匹配Legacy结构)
        item_instance.durability = result.Get<uint32_t>(row, "durability").value_or(0);
        item_instance.rare_idx = result.Get<uint32_t>(row, "rare_idx").value_or(0);
        item_instance.quick_position = result.Get<uint16_t>(row, "quick_position").value_or(0);
        item_instance.item_param = result.Get<uint32_t>(row, "item_param").value_or(0);

        // 运行时字段
        item_instance.quantity = result.Get<uint16_t>(row, "quantity").value_or(1);
        // create_time 暂时跳过 (需要时间戳转换)

        return item_instance;

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse item instance row: {}", e.what());
        return std::nullopt;
    }
}

std::optional<Game::InventorySlot> ItemDAO::RowToInventorySlot(Result& result, int row) {
    try {
        Game::InventorySlot slot;

        slot.character_id = result.Get<uint64_t>(row, "character_id").value_or(0);
        slot.item_instance_id = result.Get<uint64_t>(row, "item_instance_id").value_or(0);
        slot.item_id = result.Get<uint32_t>(row, "item_id").value_or(0);
        slot.slot_index = result.Get<uint16_t>(row, "slot_index").value_or(0);
        slot.quantity = result.Get<uint16_t>(row, "quantity").value_or(0);

        return slot;

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse inventory slot row: {}", e.what());
        return std::nullopt;
    }
}

std::optional<Game::EquipmentSlot> ItemDAO::RowToEquipmentSlot(Result& result, int row) {
    try {
        Game::EquipmentSlot equipment;

        equipment.character_id = result.Get<uint64_t>(row, "character_id").value_or(0);

        for (size_t i = 0; i < 10; ++i) {
            std::string col_name = "equip_slot_" + std::to_string(i);
            auto value = result.Get<uint64_t>(row, col_name);
            if (value.has_value()) {
                equipment.equip_slots[i] = value.value();
            }
        }

        return equipment;

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse equipment slot row: {}", e.what());
        return std::nullopt;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
