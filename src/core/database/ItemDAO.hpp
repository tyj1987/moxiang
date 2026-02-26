#pragma once

#include <vector>
#include <optional>
#include <unordered_map>
#include "pqxx_compat.hpp"  // For Result type
#include "shared/item/Item.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"

namespace Murim {
namespace Core {
namespace Database {

// Use Result from compatibility layer
using Result = Murim::Core::Database::Result;

/**
 * @brief 物品数据访问对象 (DAO)
 *
 * 负责物品模板、物品实例、背包、装备的数据库操作
 */
class ItemDAO {
public:
    // ========== 物品模板操作 ==========

    /**
     * @brief 加载物品模板
     * @param item_id 物品ID
     * @return 物品模板
     */
    static std::optional<Game::ItemTemplate> LoadItemTemplate(uint32_t item_id);

    /**
     * @brief 加载所有物品模板
     * @return 物品模板列表
     */
    static std::vector<Game::ItemTemplate> LoadAllItemTemplates();

    /**
     * @brief 保存物品模板
     * @param template 物品模板
     * @return 是否成功
     */
    static bool SaveItemTemplate(const Game::ItemTemplate& item_template);

    // ========== 物品实例操作 ==========

    /**
     * @brief 创建物品实例
     * @param item_id 物品模板ID
     * @param quantity 数量
     * @return 物品实例ID
     */
    static uint64_t CreateItemInstance(uint32_t item_id, uint16_t quantity = 1);

    /**
     * @brief 加载物品实例
     * @param instance_id 实例ID
     * @return 物品实例
     */
    static std::optional<Game::ItemInstance> LoadItemInstance(uint64_t instance_id);

    /**
     * @brief 保存物品实例
     * @param item_instance 物品实例
     * @return 是否成功
     */
    static bool SaveItemInstance(const Game::ItemInstance& item_instance);

    /**
     * @brief 删除物品实例
     * @param instance_id 实例ID
     * @return 是否成功
     */
    static bool DeleteItemInstance(uint64_t instance_id);

    // ========== 背包操作 ==========

    /**
     * @brief 加载角色背包
     * @param character_id 角色ID
     * @return 背包栏位列表
     */
    static std::vector<Game::InventorySlot> LoadInventory(uint64_t character_id);

    /**
     * @brief 添加物品到背包
     * @param character_id 角色ID
     * @param item_instance_id 物品实例ID
     * @param item_id 物品模板ID
     * @param slot_index 栏位索引
     * @param quantity 数量
     * @return 是否成功
     */
    static bool AddToInventory(uint64_t character_id, uint64_t item_instance_id,
                               uint32_t item_id, uint16_t slot_index, uint16_t quantity);

    /**
     * @brief 从背包移除物品
     * @param character_id 角色ID
     * @param slot_index 栏位索引
     * @param quantity 移除数量
     * @return 是否成功
     */
    static bool RemoveFromInventory(uint64_t character_id, uint16_t slot_index, uint16_t quantity);

    /**
     * @brief 移动物品 (栏位交换)
     * @param character_id 角色ID
     * @param from_slot 源栏位
     * @param to_slot 目标栏位
     * @return 是否成功
     */
    static bool MoveItemInInventory(uint64_t character_id, uint16_t from_slot, uint16_t to_slot);

    /**
     * @brief 更新背包栏位
     * @param slot 背包栏位
     * @return 是否成功
     */
    static bool UpdateInventorySlot(const Game::InventorySlot& slot);

    /**
     * @brief 清空背包
     * @param character_id 角色ID
     * @return 是否成功
     */
    static bool ClearInventory(uint64_t character_id);

    // ========== 装备操作 ==========

    /**
     * @brief 加载角色装备
     * @param character_id 角色ID
     * @return 装备槽数据
     */
    static std::optional<Game::EquipmentSlot> LoadEquipment(uint64_t character_id);

    /**
     * @brief 装备物品
     * @param character_id 角色ID
     * @param slot 装备部位
     * @param item_instance_id 物品实例ID
     * @return 是否成功
     */
    static bool EquipItem(uint64_t character_id, Game::EquipSlot slot, uint64_t item_instance_id);

    /**
     * @brief 卸下装备
     * @param character_id 角色ID
     * @param slot 装备部位
     * @return 是否成功
     */
    static bool UnequipItem(uint64_t character_id, Game::EquipSlot slot);

    /**
     * @brief 更新装备槽
     * @param equipment 装备槽数据
     * @return 是否成功
     */
    static bool SaveEquipment(const Game::EquipmentSlot& equipment);

private:
    /**
     * @brief 从数据库行构建物品模板
     */
    static std::optional<Game::ItemTemplate> RowToItemTemplate(Result& result, int row);

    /**
     * @brief 从数据库行构建物品实例
     */
    static std::optional<Game::ItemInstance> RowToItemInstance(Result& result, int row);

    /**
     * @brief 从数据库行构建背包栏位
     */
    static std::optional<Game::InventorySlot> RowToInventorySlot(Result& result, int row);

    /**
     * @brief 从数据库行构建装备槽
     */
    static std::optional<Game::EquipmentSlot> RowToEquipmentSlot(Result& result, int row);

    // 物品模板缓存
    static std::unordered_map<uint32_t, Game::ItemTemplate> item_template_cache_;
    static bool cache_loaded_;
};

} // namespace Database
} // namespace Core
} // namespace Murim
