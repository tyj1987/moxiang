// MurimServer - Item Operation System
// 物品使用逻辑系统 - 处理消耗品、装备等物品的使用
// 对应legacy: ItemManager, Player中的物品使用相关函数

#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <functional>

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class GameObject;
class Player;
class Item;

// ========== Item Kind ==========
// 物品类型枚举
enum class ItemKind : uint16_t {
    None = 0,
    Weapon = 1,           // 武器
    Armor = 2,            // 防具
    Helmet = 3,           // 头盔
    Gloves = 4,           // 手套
    Shoes = 5,            // 鞋子
    Necklace = 6,         // 项链
    Ring = 7,             // 戒指
    HP_Potion = 100,      // HP药水
    MP_Potion = 101,      // MP药水
    BuffScroll = 102,     // Buff卷轴
    TeleportScroll = 103, // 传送卷轴
    Food = 104,           // 食物
    Material = 200,       // 材料
    QuestItem = 300,      // 任务物品
    ShopItem = 400,       // 商城物品
    PetItem = 500,        // 宠物物品
    TitanItem = 600       // 泰坦物品
};

// ========== Item Use Result ==========
// 物品使用结果
enum class ItemUseResult : uint8_t {
    Success = 0,           // 成功
    Failed = 1,            // 失败
    Cooldown = 2,          // 冷却中
    OutOfRange = 3,        // 超出范围
    InvalidTarget = 4,     // 无效目标
    NotEnoughLevel = 5,    // 等级不足
    NotEnoughMP = 6,       // 蓝量不足
    InvalidPosition = 7,   // 位置无效
    AlreadyActive = 8,     // 效果已激活
    FullHP = 9,            // HP已满
    FullMP = 10,           // MP已满
    InventoryFull = 11,    // 背包满
    Dead = 12              // 死亡状态
};

// ========== Item Use Context ==========
// 物品使用上下文 - 包含使用时的所有信息
struct ItemUseContext {
    Player* user;              // 使用者
    GameObject* target;        // 目标（可能为null）
    float target_x;            // 目标位置X
    float target_y;            // 目标位置Y
    float target_z;            // 目标位置Z
    uint32_t current_time;     // 当前时间

    ItemUseContext()
        : user(nullptr), target(nullptr),
          target_x(0), target_y(0), target_z(0),
          current_time(0) {}
};

// ========== Item Effect ==========
// 物品效果结构
struct ItemEffect {
    enum class Type : uint8_t {
        RestoreHP = 0,          // 恢复HP
        RestoreMP = 1,          // 恢复MP
        Buff = 2,               // Buff效果
        Teleport = 3,           // 传送
        LearnSkill = 4,         // 学习技能
        AddExp = 5,             // 增加经验
        RestoreState = 6,       // 恢复状态
        None = 255
    };

    Type type;
    float value1;              // 效果值1
    float value2;              // 效果值2
    float value3;              // 效果值3
    uint32_t duration;         // 持续时间(ms)
    uint32_t buff_id;          // Buff ID

    ItemEffect()
        : type(Type::None), value1(0), value2(0), value3(0),
          duration(0), buff_id(0) {}

    ItemEffect(Type t, float v1 = 0, float v2 = 0, float v3 = 0,
               uint32_t dur = 0, uint32_t bid = 0)
        : type(t), value1(v1), value2(v2), value3(v3),
          duration(dur), buff_id(bid) {}
};

// ========== Item Use Handler ==========
// 物品使用处理器基类
class ItemUseHandler {
public:
    ItemUseHandler(ItemKind kind);
    virtual ~ItemUseHandler() = default;

    // 检查是否可以使用
    virtual bool CanUse(const Item* item, const ItemUseContext& context) = 0;

    // 使用物品
    virtual ItemUseResult Use(const Item* item, const ItemUseContext& context) = 0;

    // 获取物品类型
    ItemKind GetItemKind() const { return item_kind_; }

protected:
    ItemKind item_kind_;
};

// ========== HP Potion Handler ==========
// HP药水使用处理器
class ItemUseHandler_HPPotion : public ItemUseHandler {
public:
    ItemUseHandler_HPPotion();
    virtual ~ItemUseHandler_HPPotion() = default;

    bool CanUse(const Item* item, const ItemUseContext& context) override;
    ItemUseResult Use(const Item* item, const ItemUseContext& context) override;
};

// ========== MP Potion Handler ==========
// MP药水使用处理器
class ItemUseHandler_MPPotion : public ItemUseHandler {
public:
    ItemUseHandler_MPPotion();
    virtual ~ItemUseHandler_MPPotion() = default;

    bool CanUse(const Item* item, const ItemUseContext& context) override;
    ItemUseResult Use(const Item* item, const ItemUseContext& context) override;
};

// ========== Buff Scroll Handler ==========
// Buff卷轴使用处理器
class ItemUseHandler_BuffScroll : public ItemUseHandler {
public:
    ItemUseHandler_BuffScroll();
    virtual ~ItemUseHandler_BuffScroll() = default;

    bool CanUse(const Item* item, const ItemUseContext& context) override;
    ItemUseResult Use(const Item* item, const ItemUseContext& context) override;
};

// ========== Teleport Scroll Handler ==========
// 传送卷轴使用处理器
class ItemUseHandler_TeleportScroll : public ItemUseHandler {
public:
    ItemUseHandler_TeleportScroll();
    virtual ~ItemUseHandler_TeleportScroll() = default;

    bool CanUse(const Item* item, const ItemUseContext& context) override;
    ItemUseResult Use(const Item* item, const ItemUseContext& context) override;
};

// ========== Food Handler ==========
// 食物使用处理器
class ItemUseHandler_Food : public ItemUseHandler {
public:
    ItemUseHandler_Food();
    virtual ~ItemUseHandler_Food() = default;

    bool CanUse(const Item* item, const ItemUseContext& context) override;
    ItemUseResult Use(const Item* item, const ItemUseContext& context) override;
};

// ========== Equipment Equip Handler ==========
// 装备穿戴处理器
class ItemUseHandler_Equipment : public ItemUseHandler {
public:
    ItemUseHandler_Equipment();
    virtual ~ItemUseHandler_Equipment() = default;

    bool CanUse(const Item* item, const ItemUseContext& context) override;
    ItemUseResult Use(const Item* item, const ItemUseContext& context) override;
};

// ========== Item Operation Manager ==========
// 物品操作管理器 - 管理所有物品使用处理器
class ItemOperationManager {
private:
    std::map<ItemKind, std::unique_ptr<ItemUseHandler>> handlers_;

public:
    ItemOperationManager();
    ~ItemOperationManager();

    // 初始化
    void Init();
    void Release();

    // 注册处理器
    void RegisterHandler(ItemKind kind, std::unique_ptr<ItemUseHandler> handler);

    // 检查物品是否可用
    bool CanUseItem(const Item* item, const ItemUseContext& context);

    // 使用物品
    ItemUseResult UseItem(const Item* item, const ItemUseContext& context);

    // 快速使用（自动创建上下文）
    ItemUseResult UseItemQuick(Player* player, uint32_t item_id, GameObject* target = nullptr);

    // 获取处理器
    ItemUseHandler* GetHandler(ItemKind kind);
    const ItemUseHandler* GetHandler(ItemKind kind) const;

private:
    // 创建默认处理器
    void CreateDefaultHandlers();
};

// ========== Item Cooldown Manager ==========
// 物品冷却管理器
class ItemCooldownManager {
private:
    struct CooldownInfo {
        uint32_t item_id;
        uint32_t category_id;     // 类别ID（同类物品共享冷却）
        uint32_t cooldown_time;   // 冷却时间(ms)
        uint32_t last_use_time;   // 上次使用时间

        CooldownInfo()
            : item_id(0), category_id(0), cooldown_time(0), last_use_time(0) {}
    };

    std::map<uint32_t, CooldownInfo> cooldowns_;  // item_id -> cooldown
    uint64_t owner_id_;

public:
    ItemCooldownManager();
    explicit ItemCooldownManager(uint64_t owner_id);
    ~ItemCooldownManager();

    void Init(uint64_t owner_id);

    // 设置冷却
    void SetCooldown(uint32_t item_id, uint32_t category_id, uint32_t cooldown_ms);

    // 检查冷却
    bool IsOnCooldown(uint32_t item_id) const;
    bool IsOnCooldownByCategory(uint32_t category_id) const;

    // 获取剩余冷却时间(ms)
    uint32_t GetRemainingCooldown(uint32_t item_id) const;

    // 使用物品（更新冷却）
    void UseItem(uint32_t item_id);

    // 清除冷却
    void ClearCooldown(uint32_t item_id);
    void ClearAllCooldowns();

private:
    uint32_t GetCurrentTime() const;
};

// ========== Helper Functions ==========

// 获取物品类型
ItemKind GetItemKind(uint32_t item_id);

// 是否为消耗品
bool IsConsumableItem(ItemKind kind);

// 是否为装备
bool IsEquipmentItem(ItemKind kind);

// 计算恢复量
float CalculateRestoreAmount(float base_value, uint16_t player_level);

// 获取物品效果列表
std::vector<ItemEffect> GetItemEffects(uint32_t item_id);

// 根据物品类型获取装备槽
EquipSlot GetEquipSlotForItemType(ItemType item_type);

// 根据Buff ID获取状态类型
SpecialStateType GetStateTypeFromBuffID(uint32_t buff_id);

// 检查位置是否有效
bool IsValidPosition(float x, float y, float z);

} // namespace Game
} // namespace Murim
