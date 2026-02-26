#pragma once

#include <vector>
#include <optional>
#include <unordered_map>
#include <memory>
#include "Item.hpp"
#include "core/database/ItemDAO.hpp"
#include "../battle/Buff.hpp"  // For Buff type

namespace Murim {
namespace Game {

/**
 * @brief 物品管理器
 *
 * 负责物品的业务逻辑:
 * - 物品获取和分配
 * - 物品使用
 * - 背包管理
 * - 装备穿戴
 * - 物品验证
 *
 * 对应 legacy: CItemManager
 */
class ItemManager {
public:
    /**
     * @brief 获取单例实例
     */
    static ItemManager& Instance();

    /**
     * @brief 更新物品系统
     * @param delta_time 时间增量(毫秒)
     */
    void Update(uint32_t delta_time);

    // ========== 物品模板管理 ==========

    /**
     * @brief 初始化物品模板 (从数据库加载所有模板)
     */
    void Initialize();

    /**
     * @brief 获取物品模板
     * @param item_id 物品ID
     * @return 物品模板 (如果不存在则返回 nullopt)
     */
    std::optional<ItemTemplate> GetItemTemplate(uint32_t item_id);

    /**
     * @brief 重新加载物品模板
     */
    void ReloadTemplates();

    // ========== 物品获取 ==========

    /**
     * @brief 给角色添加物品
     * @param character_id 角色ID
     * @param item_id 物品模板ID
     * @param quantity 数量
     * @return 物品实例ID (失败返回 0)
     */
    uint64_t ObtainItem(uint64_t character_id, uint32_t item_id, uint16_t quantity = 1);

    /**
     * @brief 给角色添加物品 (指定栏位)
     * @param character_id 角色ID
     * @param item_id 物品模板ID
     * @param quantity 数量
     * @param slot_index 栏位索引
     * @return 是否成功
     */
    bool ObtainItemToSlot(uint64_t character_id, uint32_t item_id,
                          uint16_t quantity, uint16_t slot_index);

    /**
     * @brief 移除角色物品
     * @param character_id 角色ID
     * @param item_id 物品模板ID
     * @param quantity 数量
     * @return 实际移除的数量
     */
    uint16_t RemoveItem(uint64_t character_id, uint32_t item_id, uint16_t quantity);

    /**
     * @brief 移除指定栏位的物品
     * @param character_id 角色ID
     * @param slot_index 栏位索引
     * @param quantity 数量
     * @return 是否成功
     */
    bool RemoveItemFromSlot(uint64_t character_id, uint16_t slot_index, uint16_t quantity);

    // ========== 物品查询 ==========

    /**
     * @brief 获取角色物品数量
     * @param character_id 角色ID
     * @param item_id 物品模板ID
     * @return 物品数量
     */
    uint16_t GetItemCount(uint64_t character_id, uint32_t item_id);

    /**
     * @brief 检查角色是否有足够的物品
     * @param character_id 角色ID
     * @param item_id 物品模板ID
     * @param quantity 需要的数量
     * @return 是否足够
     */
    bool HasEnoughItem(uint64_t character_id, uint32_t item_id, uint16_t quantity);

    /**
     * @brief 获取角色背包
     * @param character_id 角色ID
     * @return 背包栏位列表
     */
    std::vector<InventorySlot> GetInventory(uint64_t character_id);

    /**
     * @brief 获取指定栏位的物品
     * @param character_id 角色ID
     * @param slot_index 栏位索引
     * @return 背包栏位 (如果为空则返回 nullopt)
     */
    std::optional<InventorySlot> GetInventorySlot(uint64_t character_id, uint16_t slot_index);

    // ========== 背包管理 ==========

    /**
     * @brief 查找空栏位
     * @param character_id 角色ID
     * @return 空栏位索引 (如果没有则返回 nullopt)
     */
    std::optional<uint16_t> FindEmptySlot(uint64_t character_id);

    /**
     * @brief 查找多个空栏位
     * @param character_id 角色ID
     * @param count 需要的空栏位数量
     * @return 空栏位索引列表
     */
    std::vector<uint16_t> FindEmptySlots(uint64_t character_id, uint16_t count);

    /**
     * @brief 移动物品 (栏位交换)
     * @param character_id 角色ID
     * @param from_slot 源栏位
     * @param to_slot 目标栏位
     * @return 是否成功
     */
    bool MoveItem(uint64_t character_id, uint16_t from_slot, uint16_t to_slot);

    /**
     * @brief 合并物品 (用于可堆叠物品)
     * @param character_id 角色ID
     * @param from_slot 源栏位
     * @param to_slot 目标栏位
     * @return 是否成功
     */
    bool MergeItem(uint64_t character_id, uint16_t from_slot, uint16_t to_slot);

    /**
     * @brief 分割物品
     * @param character_id 角色ID
     * @param from_slot 源栏位
     * @param to_slot 目标栏位
     * @param quantity 分割数量
     * @return 是否成功
     */
    bool SplitItem(uint64_t character_id, uint16_t from_slot, uint16_t to_slot, uint16_t quantity);

    /**
     * @brief 获取背包空栏位数量
     * @param character_id 角色ID
     * @return 空栏位数量
     */
    uint16_t GetEmptySlotCount(uint64_t character_id);

    // ========== 装备管理 ==========

    /**
     * @brief 获取角色装备
     * @param character_id 角色ID
     * @return 装备槽数据
     */
    std::optional<EquipmentSlot> GetEquipment(uint64_t character_id);

    /**
     * @brief 装备物品
     * @param character_id 角色ID
     * @param slot_index 背包栏位索引
     * @return 物品使用结果
     */
    ItemUseResult EquipItem(uint64_t character_id, uint16_t slot_index);

    /**
     * @brief 卸下装备
     * @param character_id 角色ID
     * @param equip_slot 装备部位
     * @return 是否成功
     */
    bool UnequipItem(uint64_t character_id, EquipSlot equip_slot);

    /**
     * @brief 检查物品是否可装备
     * @param character_id 角色ID
     * @param item_template 物品模板
     * @return 是否可装备
     */
    bool CanEquip(uint64_t character_id, const ItemTemplate& item_template);

    // ========== 物品使用 ==========

    /**
     * @brief 使用物品
     * @param character_id 角色ID
     * @param slot_index 栏位索引
     * @return 物品使用结果
     */
    ItemUseResult UseItem(uint64_t character_id, uint16_t slot_index);

    /**
     * @brief 使用消耗品
     * @param character_id 角色ID
     * @param item_template 物品模板
     * @param quantity 使用数量
     * @return 物品使用结果
     */
    ItemUseResult UseConsumable(uint64_t character_id, const ItemTemplate& item_template, uint16_t quantity);

    // ========== 物品验证 ==========

    /**
     * @brief 检查物品ID是否有效
     * @param item_id 物品ID
     * @return 是否有效
     */
    bool IsValidItem(uint32_t item_id);

    /**
     * @brief 检查栏位索引是否有效
     * @param slot_index 栏位索引
     * @return 是否有效
     */
    bool IsValidSlot(uint16_t slot_index);

    /**
     * @brief 检查装备部位是否有效
     * @param equip_slot 装备部位
     * @return 是否有效
     */
    bool IsValidEquipSlot(EquipSlot equip_slot);

    // ========== 背包容量 ==========

    /**
     * @brief 获取背包容量
     * @return 背包容量 (默认40)
     */
    uint16_t GetInventoryCapacity();

    /**
     * @brief 设置背包容量
     * @param capacity 容量
     */
    void SetInventoryCapacity(uint16_t capacity);

    // ========== 物品丢弃 ==========

    /**
     * @brief 丢弃物品
     * @param character_id 角色ID
     * @param slot_index 栏位索引
     * @param quantity 数量
     * @return 是否成功
     */
    bool DropItem(uint64_t character_id, uint16_t slot_index, uint16_t quantity);

private:
    ItemManager() = default;
    ~ItemManager() = default;
    ItemManager(const ItemManager&) = delete;
    ItemManager& operator=(const ItemManager&) = delete;

    // 物品模板缓存
    std::unordered_map<uint32_t, ItemTemplate> item_templates_;
    bool templates_loaded_ = false;

    // 背包容量
    uint16_t inventory_capacity_ = 40;

    // 辅助方法

    /**
     * @brief 加载角色数据 (用于验证装备条件)
     */
    struct CharacterInfo {
        uint16_t level;
        uint8_t initial_weapon;
        uint8_t gender;
        uint16_t strength;
        uint16_t intelligence;
        uint16_t dexterity;
        uint16_t vitality;
    };

    /**
     * @brief 获取角色信息 (用于装备验证)
     */
    std::optional<CharacterInfo> GetCharacterInfo(uint64_t character_id);

    /**
     * @brief 装备属性加成 - 对应 legacy 装备属性累加
     */
    struct EquipmentBonus {
        // 基础属性加成
        uint16_t strength = 0;
        uint16_t intelligence = 0;
        uint16_t dexterity = 0;
        uint16_t vitality = 0;
        uint16_t wisdom = 0;

        // 战斗属性加成
        uint32_t physical_attack = 0;
        uint32_t physical_defense = 0;
        uint32_t magic_attack = 0;
        uint32_t magic_defense = 0;
        uint32_t attack_speed = 0;
        uint32_t move_speed = 0;
        uint16_t critical_rate = 0;
        uint16_t hit_rate = 0;
        uint16_t dodge_rate = 0;
    };

    /**
     * @brief 更新角色属性 (装备/卸下时) - 对应 legacy 装备属性更新
     */
    void UpdateCharacterStats(uint64_t character_id);

    /**
     * @brief 计算装备提供的属性加成 - 对应 legacy 装备属性计算
     * @return 装备属性加成
     */
    EquipmentBonus CalculateEquipmentBonus(uint64_t character_id);

    /**
     * @brief 从消耗品创建 Buff - 对应 legacy 消耗品 Buff 创建
     * @param item_template 物品模板
     * @return Buff 数据 (如果没有 Buff 效果则返回 nullopt)
     */
    std::optional<Buff> CreateBuffFromConsumable(const ItemTemplate& item_template);

    /**
     * @brief 获取消耗品 Buff 持续时间 - 对应 legacy Buff 持续时间计算
     * @param item_template 物品模板
     * @return 持续时间(秒)
     */
    uint32_t GetConsumableBuffDuration(const ItemTemplate& item_template);
};

} // namespace Game
} // namespace Murim
