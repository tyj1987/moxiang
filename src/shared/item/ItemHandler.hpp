#pragma once

#include "ItemManager.hpp"
#include "../character/CharacterManager.hpp"
#include "../battle/DamageCalculator.hpp"
#include "../battle/SpecialStateManager.hpp"
#include "../game/GameObject.hpp"
#include <unordered_map>
#include <memory>
#include "core/spdlog_wrapper.hpp"

namespace Murim {

// 前向声明
namespace Core {
namespace Network {
class Connection;
}
}

namespace Game {

// Using statements for simpler type names
using GameObjectPtr = std::shared_ptr<Murim::Game::GameObject>;
using ConnectionPtr = std::shared_ptr<Murim::Core::Network::Connection>;


/**
 * @brief 物品装备请求
 */
struct EquipItemRequest {
    uint64_t character_id;      // 角色ID
    uint32_t item_id;         // 物品ID
    uint16_t slot_index;       // 背包槽位索引
};

/**
 * @brief 物品装备响应
 */
struct EquipItemResponse {
    uint32_t result_code;       // 结果码 (0=成功, 1=物品不存在, 2=槽位无效, 3=不满足装备条件, 4=装备部位无效)
    uint32_t item_id;         // 物品ID
    uint16_t equip_slot;       // 装备部位
    uint32_t strength;         // 力量加成
    uint32_t intelligence;       // 智力加成
    uint32_t dexterity;        // 敏捷加成
    uint32_t vitality;         // 体力加成
    uint32_t physical_attack;   // 物理攻击加成
    uint32_t physical_defense;  // 物理防御加成
    uint32_t magic_attack;      // 魔法攻击加成
    uint32_t magic_defense;    // 魔法防御加成
    uint32_t attack_speed;      // 攻击速度加成
    uint32_t move_speed;        // 移动速度加成
};

/**
 * @brief 卸下装备请求
 */
struct UnequipItemRequest {
    uint64_t character_id;      // 角色ID
    uint16_t equip_slot;       // 装备部位
};

/**
 * @brief 卸下装备响应
 */
struct UnequipItemResponse {
    uint32_t result_code;       // 结果码 (0=成功, 1=装备部位无效, 2=该部位无装备)
    uint16_t equip_slot;       // 装备部位
    uint32_t item_id;         // 物品ID
    uint16_t target_slot;      // 目标背包槽位
};

/**
 * @brief 使用消耗品请求
 */
struct UseConsumableRequest {
    uint64_t character_id;      // 角色ID
    uint32_t item_id;         // 物品ID
    uint16_t slot_index;       // 背包槽位索引
    uint16_t quantity;         // 使用数量
};

/**
 * @brief 使用消耗品响应
 */
struct UseConsumableResponse {
    uint32_t result_code;       // 结果码 (0=成功, 1=物品不存在, 2=物品不可使用, 3=数量不足, 4=效果应用失败)
    uint32_t item_id;         // 物品ID
    uint32_t remaining_quantity; // 剩余数量
    uint32_t hp_restored;      // 恢复的HP
    uint32_t mp_restored;      // 恢复的MP
    uint32_t buff_id;         // 应用的Buff ID
    uint32_t buff_duration;   // Buff持续时间（秒）
};

/**
 * @brief 丢弃物品请求
 */
struct DropItemRequest {
    uint64_t character_id;      // 角色ID
    uint32_t item_id;         // 物品ID
    uint16_t slot_index;       // 背包槽位索引
    uint16_t quantity;         // 丢弃数量
    float x;                   // 丢弃位置X
    float y;                   // 丢弃位置Y
    float z;                   // 丢弃位置Z
};

/**
 * @brief 丢弃物品响应
 */
struct DropItemResponse {
    uint32_t result_code;       // 结果码 (0=成功, 1=物品不存在, 2=槽位无效, 3=数量不足)
    uint32_t item_id;         // 物品ID
    uint16_t quantity;         // 丢弃数量
};

/**
 * @brief 整理物品请求
 */
struct MergeItemRequest {
    uint64_t character_id;      // 角色ID
    uint16_t from_slot;        // 源槽位
    uint16_t to_slot;          // 目标槽位
};

/**
 * @brief 整理物品响应
 */
struct MergeItemResponse {
    uint32_t result_code;       // 结果码 (0=成功, 1=物品不可叠加, 2=槽位无效, 3=数量不足)
    uint32_t item_id;         // 物品ID
    uint16_t from_slot;        // 源槽位
    uint16_t to_slot;          // 目标槽位
    uint16_t total_quantity;    // 整理后总数量
};

/**
 * @brief 分割物品请求
 */
struct SplitItemRequest {
    uint64_t character_id;      // 角色ID
    uint16_t from_slot;        // 源槽位
    uint16_t to_slot;          // 目标槽位（空槽位）
    uint16_t quantity;         // 分割数量
};

/**
 * @brief 分割物品响应
 */
struct SplitItemResponse {
    uint32_t result_code;       // 结果码 (0=成功, 1=物品不可分割, 2=源槽位无效, 3=目标槽位非空, 4=数量不足)
    uint32_t item_id;         // 物品ID
    uint16_t from_slot;        // 源槽位
    uint16_t to_slot;          // 目标槽位
    uint16_t from_quantity;    // 源槽位剩余数量
    uint16_t to_quantity;      // 目标槽位数量
};

/**
 * @brief 物品处理器
 *
 * 负责处理客户端的物品操作请求：
 * - 验证物品是否存在
 * - 验证槽位是否有效
 * - 验证操作条件（装备条件、使用条件）
 * - 执行物品操作（装备、卸下、使用、丢弃、整理、分割）
 * - 应用物品效果（属性加成、Buff效果、HP/MP恢复）
 * - 广播角色状态变化
 * - 保存角色数据到数据库
 *
 * 对应 legacy: 物品操作处理（多个文件散布在 ServerSystem.cpp）
 */
class ItemHandler {
public:
    /**
     * @brief 构造函数
     */
    ItemHandler(
        Murim::Game::CharacterManager& char_manager,
        Murim::Game::ItemManager& item_manager,
        std::shared_ptr<Murim::Game::DamageCalculator> damage_calculator,
        std::shared_ptr<Murim::Game::SpecialStateManager> buff_manager);

    /**
     * @brief 处理装备物品请求
     */
    EquipItemResponse HandleEquipItem(const EquipItemRequest& request);

    /**
     * @brief 处理卸下装备请求
     */
    UnequipItemResponse HandleUnequipItem(const UnequipItemRequest& request);

    /**
     * @brief 处理使用消耗品请求
     */
    UseConsumableResponse HandleUseConsumable(const UseConsumableRequest& request);

    /**
     * @brief 处理丢弃物品请求
     */
    DropItemResponse HandleDropItem(const DropItemRequest& request);

    /**
     * @brief 处理整理物品请求
     */
    MergeItemResponse HandleMergeItem(const MergeItemRequest& request);

    /**
     * @brief 处理分割物品请求
     */
    SplitItemResponse HandleSplitItem(const SplitItemRequest& request);

private:
    /**
     * @brief 验证装备条件
     * @return 是否满足装备条件
     */
    bool ValidateEquipConditions(GameObjectPtr character,
                                const ItemTemplate& item);

    /**
     * @brief 应用装备属性加成
     */
    void ApplyEquipmentBonus(GameObjectPtr character,
                          const ItemTemplate& item);

    /**
     * @brief 移除装备属性加成
     */
    void RemoveEquipmentBonus(GameObjectPtr character,
                          const ItemTemplate& item);

    /**
     * @brief 应用消耗品效果
     */
    void ApplyConsumableEffect(GameObjectPtr character,
                            const ItemTemplate& item,
                            uint16_t quantity);

    /**
     * @brief 广播角色属性变化
     */
    void BroadcastCharacterStats(uint64_t character_id,
                              GameObjectPtr character,
                              ConnectionPtr exclude_conn = nullptr);

    Murim::Game::CharacterManager& character_manager_;
    Murim::Game::ItemManager& item_manager_;
    std::shared_ptr<Murim::Game::DamageCalculator> damage_calculator_;
    std::shared_ptr<Murim::Game::SpecialStateManager> buff_manager_;
};

} // namespace Game
} // namespace Murim
