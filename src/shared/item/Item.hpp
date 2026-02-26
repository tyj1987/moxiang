#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <optional>

namespace Murim {
namespace Game {

/**
 * @brief 物品类型枚举
 *
 * 对应 legacy: ItemKind
 */
enum class ItemType : uint16_t {
    kWeapon = 1,        // 武器
    kArmor = 2,         // 防具(胸甲)
    kHelmet = 3,        // 头盔
    kPotion = 4,        // 消耗品 (药水等)
    kMaterial = 5,      // 材料
    kQuest = 6,         // 任务物品
    kCash = 7,          // 道具商城物品
    kPet = 8,           // 宠物物品
    kCostume = 9,       // 时装
    kGloves = 10,       // 手套
    kShoes = 11,        // 鞋子
    kNecklace = 12,     // 项链
    kRing = 13,         // 戒指
    kOther = 99         // 其他
};

/**
 * @brief 装备部位枚举
 */
enum class EquipSlot : uint16_t {
    kWeapon = 0,        // 武器
    kHelmet = 1,        // 头盔
    kArmor = 2,         // 胸甲
    kPants = 3,         // 护腿
    kShoes = 4,         // 鞋子
    kGloves = 5,        // 手套
    kAccessory1 = 6,    // 饰品1 (项链)
    kAccessory2 = 7,    // 饰品2 (戒指)
    kAccessory3 = 8,    // 饰品3 (耳环)
    kCape = 9,          // 披肩
    kMax = 10
};

/**
 * @brief 装备品质枚举
 */
enum class ItemQuality : uint16_t {
    kCommon = 1,        // 普通 (白色)
    kUncommon = 2,      // 优秀 (绿色)
    kRare = 3,          // 稀有 (蓝色)
    kEpic = 4,          // 史诗 (紫色)
    kLegendary = 5,     // 传说 (橙色)
    kUnique = 6         // 独一无二 (红色)
};

/**
 * @brief 物品参数位标志 (对应 legacy ITEMPARAM)
 *
 * 用于 ItemParam 字段，表示物品的各种状态标志
 */
enum class ItemParamFlag : uint32_t {
    kNone = 0x00000000,
    kBound = 0x00000001,        // 绑定物品 (不可交易)
    kLocked = 0x00000002,       // 锁定 (不可使用/交易/丢弃)
    kQuestItem = 0x00000004,    // 任务物品标志
    kTimed = 0x00000008,        // 时限物品
    kProtected = 0x00000010,    // 组合失败保护 (防止完全失败)
    kReserved1 = 0x00000020,
    kReserved2 = 0x00000040,
    kReserved3 = 0x00000080
};

/**
 * @brief 位标志运算符 (支持组合)
 */
inline ItemParamFlag operator|(ItemParamFlag a, ItemParamFlag b) {
    return static_cast<ItemParamFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline ItemParamFlag operator&(ItemParamFlag a, ItemParamFlag b) {
    return static_cast<ItemParamFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline ItemParamFlag& operator|=(ItemParamFlag& a, ItemParamFlag b) {
    a = a | b;
    return a;
}

/**
 * @brief 属性抗性
 */
struct AttributeResist {
    uint16_t fire = 0;      // 火抗
    uint16_t ice = 0;       // 冰抗
    uint16_t lightning = 0; // 雷抗
    uint16_t poison = 0;    // 毒抗
    uint16_t holy = 0;      // 圣抗
};

/**
 * @brief 物品模板 (静态数据)
 *
 * 对应 legacy: ITEM_INFO
 * 存储在 item_templates 表中
 */
struct ItemTemplate {
    // 基本信息
    uint32_t item_id;               // 物品ID (唯一)
    std::string name;               // 物品名称
    ItemType item_type;             // 物品类型
    ItemQuality quality;            // 物品品质
    uint16_t item_grade;            // 物品等级/阶数

    // 价格信息
    uint32_t buy_price;             // 购买价格
    uint32_t sell_price;            // 出售价格

    // 使用限制
    std::optional<uint16_t> limit_job;       // 限制职业 (0=无限制, 1=战士, 2=法师, 3=刺客)
    std::optional<uint16_t> limit_gender;    // 限制性别 (0=无限制, 1=男, 2=女)
    std::optional<uint16_t> limit_level;     // 限制等级
    std::optional<uint16_t> limit_strength;  // 限制力量
    std::optional<uint16_t> limit_int;      // 限制智力
    std::optional<uint16_t> limit_dex;      // 限制敏捷
    std::optional<uint16_t> limit_vit;      // 限制体力

    // 装备信息
    std::optional<EquipSlot> equip_slot;     // 装备部位
    std::optional<uint16_t> weapon_type;     // 武器类型

    // 基础属性加成
    uint16_t strength = 0;          // 力量+
    uint16_t intelligence = 0;      // 智力+
    uint16_t dexterity = 0;         // 敏捷+
    uint16_t vitality = 0;          // 体力+
    uint16_t wisdom = 0;            // 智慧+

    // 战斗属性加成
    uint32_t hp = 0;                // 生命值+
    uint32_t mp = 0;                // 内力值+
    uint32_t physical_attack = 0;   // 物理攻击+
    uint32_t physical_defense = 0;  // 物理防御+
    uint32_t magic_attack = 0;      // 魔法攻击+
    uint32_t magic_defense = 0;     // 魔法防御+
    uint32_t attack_speed = 0;      // 攻击速度+
    uint32_t move_speed = 0;        // 移动速度+
    uint16_t critical_rate = 0;     // 暴击率+ (%)
    uint16_t hit_rate = 0;          // 命中率+ (%)
    uint16_t dodge_rate = 0;        // 闪避率+ (%)

    // 属性攻击
    uint16_t fire_attack = 0;       // 火属性攻击
    uint16_t ice_attack = 0;        // 冰属性攻击
    uint16_t lightning_attack = 0;  // 雷属性攻击
    uint16_t poison_attack = 0;     // 毒属性攻击
    uint16_t holy_attack = 0;       // 圣属性攻击

    // 属性抗性
    AttributeResist attr_resist;

    // 特殊效果
    uint16_t hp_recover = 0;        // HP恢复
    float hp_recover_rate = 0.0f;   // HP恢复率 (%)
    uint16_t mp_recover = 0;        // MP恢复
    float mp_recover_rate = 0.0f;   // MP恢复率 (%)

    // 其他属性
    bool is_tradeable = true;       // 是否可交易
    bool is_dropable = true;        // 是否可丢弃
    bool is_destroyable = true;     // 是否可销毁
    uint16_t stack_limit = 1;       // 可堆叠数量 (1=不可堆叠)

    // 消耗品属性
    uint16_t cooldown = 0;          // 冷却时间 (秒)
    std::string use_script;         // 使用时执行的脚本

    // 任务物品属性
    std::optional<uint32_t> quest_id; // 关联任务ID

    /**
     * @brief 检查物品是否可装备
     */
    bool IsEquipable() const {
        return item_type == ItemType::kWeapon ||
               item_type == ItemType::kArmor ||
               item_type == ItemType::kHelmet ||
               item_type == ItemType::kGloves ||
               item_type == ItemType::kShoes ||
               item_type == ItemType::kNecklace ||
               item_type == ItemType::kRing ||
               item_type == ItemType::kCostume;
    }

    /**
     * @brief 检查物品是否可堆叠
     */
    bool IsStackable() const {
        return stack_limit > 1;
    }

    /**
     * @brief 获取物品显示品质颜色
     */
    const char* GetQualityColor() const {
        switch (quality) {
            case ItemQuality::kCommon: return "#FFFFFF";    // 白色
            case ItemQuality::kUncommon: return "#00FF00";  // 绿色
            case ItemQuality::kRare: return "#0080FF";      // 蓝色
            case ItemQuality::kEpic: return "#8000FF";      // 紫色
            case ItemQuality::kLegendary: return "#FF8000"; // 橙色
            case ItemQuality::kUnique: return "#FF0000";    // 红色
            default: return "#FFFFFF";
        }
    }
};

/**
 * @brief 物品实例 (动态数据)
 *
 * 对应 legacy: ITEMBASE (CommonStruct.h Lines 572-582)
 *
 * Legacy结构定义:
 * struct ICONBASE {
 *     DWORD dwDBIdx;      // 数据库索引 (instance_id)
 *     WORD wIconIdx;      // ItemID (item_id)
 *     POSTYPE Position;   // 背包位置 (slot_index)
 * };
 *
 * struct ITEMBASE : public ICONBASE {
 *     DURTYPE Durability;        // DWORD - 耐久度 (uint32_t)
 *     DWORD RareIdx;             // 稀有物品索引 (单值DWORD，非数组)
 *     POSTYPE QuickPosition;     // 快捷栏位置 (WORD/uint16_t)
 *     ITEMPARAM ItemParam;       // 物品参数 (DWORD位标志，uint32_t)
 * };
 *
 * 存储在 item_instances 表中
 */
struct ItemInstance {
    // ========== ICONBASE 基础字段 ==========
    uint64_t instance_id;        // dwDBIdx - 数据库主键
    uint32_t item_id;            // wIconIdx - 物品模板ID
    uint16_t slot_index = 0;     // Position - 背包位置 (0-127)

    // ========== ITEMBASE 字段 (必须与Legacy完全一致) ==========
    uint32_t durability = 0;     // DURTYPE - 当前耐久度 (DWORD)
    uint32_t rare_idx = 0;       // RareIdx - 稀有物品索引 (DWORD，单值)
    uint16_t quick_position = 0; // QuickPosition - 快捷栏位置 (POSTYPE)
    uint32_t item_param = 0;     // ITEMPARAM - 物品参数位标志 (DWORD)

    // ========== 运行时字段 (不存储在数据库) ==========
    uint16_t quantity = 1;       // 可堆叠数量
    uint64_t create_time = 0;    // 创建时间戳

    /**
     * @brief 检查是否已损坏
     * @return true if durability is 0
     */
    bool IsBroken() const {
        return durability == 0;
    }

    /**
     * @brief 检查是否可使用
     * @return true if not locked and not broken
     */
    bool IsUsable() const {
        return !IsBroken() && !HasFlag(ItemParamFlag::kLocked);
    }

    /**
     * @brief 检查物品参数位标志
     * @param flag 要检查的标志
     * @return true if flag is set
     */
    bool HasFlag(ItemParamFlag flag) const {
        return (item_param & static_cast<uint32_t>(flag)) != 0;
    }

    /**
     * @brief 设置物品参数位标志
     * @param flag 要设置的标志
     * @param set true=设置标志, false=清除标志
     */
    void SetFlag(ItemParamFlag flag, bool set = true) {
        if (set) {
            item_param |= static_cast<uint32_t>(flag);
        } else {
            item_param &= ~static_cast<uint32_t>(flag);
        }
    }

    /**
     * @brief 检查是否绑定物品
     */
    bool IsBound() const {
        return HasFlag(ItemParamFlag::kBound);
    }

    /**
     * @brief 检查是否锁定
     */
    bool IsLocked() const {
        return HasFlag(ItemParamFlag::kLocked);
    }

    /**
     * @brief 检查是否有组合保护
     */
    bool HasMixProtection() const {
        return HasFlag(ItemParamFlag::kProtected);
    }

    /**
     * @brief 检查是否为稀有物品 (RareIdx > 0)
     */
    bool IsRare() const {
        return rare_idx > 0;
    }
};

/**
 * @brief 背包栏位数据
 *
 * 对应 legacy: Inventory
 * 存储在 inventory 表中
 */
struct InventorySlot {
    uint64_t character_id;          // 角色ID
    uint64_t item_instance_id;      // 物品实例ID
    uint32_t item_id;               // 物品模板ID
    uint16_t slot_index;            // 栏位索引 (0-127)
    uint16_t quantity;              // 数量

    /**
     * @brief 检查栏位是否为空
     */
    bool IsEmpty() const {
        return item_instance_id == 0;
    }
};

/**
 * @brief 装备槽数据
 *
 * 对应 legacy: WearSlot
 * 存储在 equipment 表中
 */
struct EquipmentSlot {
    uint64_t character_id;          // 角色ID
    std::optional<uint64_t> equip_slots[10]; // 10个装备槽位

    /**
     * @brief 获取指定部位的装备
     */
    std::optional<uint64_t> GetEquip(EquipSlot slot) const {
        size_t index = static_cast<size_t>(slot);
        if (index >= 10) {
            return std::nullopt;
        }
        return equip_slots[index];
    }

    /**
     * @brief 设置指定部位的装备
     */
    void SetEquip(EquipSlot slot, uint64_t instance_id) {
        size_t index = static_cast<size_t>(slot);
        if (index < 10) {
            equip_slots[index] = instance_id;
        }
    }

    /**
     * @brief 卸下指定部位的装备
     */
    void Unequip(EquipSlot slot) {
        size_t index = static_cast<size_t>(slot);
        if (index < 10) {
            equip_slots[index] = std::nullopt;
        }
    }

    /**
     * @brief 获取所有已装备的物品实例ID - 对应 legacy 装备遍历
     * @return 已装备物品的实例ID列表
     */
    std::vector<uint64_t> GetAllEquipped() const {
        std::vector<uint64_t> equipped_items;
        for (size_t i = 0; i < 10; ++i) {
            if (equip_slots[i].has_value()) {
                equipped_items.push_back(equip_slots[i].value());
            }
        }
        return equipped_items;
    }
};

/**
 * @brief 物品使用结果
 */
enum class ItemUseResult : uint8_t {
    kSuccess = 0,           // 成功
    kFailed = 1,            // 失败
    kCooldown = 2,          // 冷却中
    kInvalidItem = 3,       // 无效物品
    kInvalidTarget = 4,     // 无效目标
    kNotEnoughMP = 5,       // 内力不足
    kNotEnoughHP = 6,       // 生命不足
    kDisabled = 7,          // 物品被禁用
    kUnknown = 99           // 未知错误
};

/**
 * @brief 物品类 - 物品实例的运行时包装
 *
 * 对应 legacy: ITEMBASE
 * 封装ItemTemplate和ItemInstance数据
 */
class Item {
private:
    const ItemTemplate* template_;  // 物品模板(静态数据)
    ItemInstance instance_;         // 物品实例(动态数据)

public:
    Item(const ItemTemplate* tmpl, const ItemInstance& inst)
        : template_(tmpl), instance_(inst) {}

    // ========== 访问模板数据 ==========
    const ItemTemplate* GetTemplate() const { return template_; }
    uint32_t GetItemId() const { return template_->item_id; }
    const std::string& GetName() const { return template_->name; }
    ItemType GetType() const { return template_->item_type; }
    ItemQuality GetQuality() const { return template_->quality; }

    // ========== 访问实例数据 ==========
    uint64_t GetInstanceId() const { return instance_.instance_id; }
    uint16_t GetQuantity() const { return instance_.quantity; }
    uint16_t GetSlotIndex() const { return instance_.slot_index; }
    uint32_t GetDurability() const { return instance_.durability; }
    uint32_t GetRareIdx() const { return instance_.rare_idx; }
    uint16_t GetQuickPosition() const { return instance_.quick_position; }
    uint32_t GetItemParam() const { return instance_.item_param; }

    // ========== ITEMBASE字段访问方法 ==========

    /**
     * @brief 检查物品参数位标志
     */
    bool HasFlag(ItemParamFlag flag) const {
        return instance_.HasFlag(flag);
    }

    /**
     * @brief 检查是否绑定
     */
    bool IsBound() const {
        return instance_.IsBound();
    }

    /**
     * @brief 检查是否锁定
     */
    bool IsLocked() const {
        return instance_.IsLocked();
    }

    /**
     * @brief 检查是否有组合保护
     */
    bool HasMixProtection() const {
        return instance_.HasMixProtection();
    }

    /**
     * @brief 检查是否为稀有物品
     */
    bool IsRare() const {
        return instance_.IsRare();
    }

    // ========== 便捷访问方法 ==========

    /**
     * @brief 获取物品显示品质颜色
     */
    const char* GetQualityColor() const {
        return template_->GetQualityColor();
    }

    /**
     * @brief 检查是否可装备
     */
    bool IsEquipable() const {
        return template_->IsEquipable();
    }

    /**
     * @brief 检查是否可堆叠
     */
    bool IsStackable() const {
        return template_->IsStackable();
    }

    /**
     * @brief 检查是否已损坏
     */
    bool IsBroken() const {
        return instance_.IsBroken();
    }

    /**
     * @brief 检查是否可使用
     */
    bool IsUsable() const {
        return instance_.IsUsable();
    }

    /**
     * @brief 获取耐久度 (直接返回值，移除了百分比计算)
     */
    uint32_t GetDurabilityValue() const {
        return instance_.durability;
    }
};

} // namespace Game
} // namespace Murim
