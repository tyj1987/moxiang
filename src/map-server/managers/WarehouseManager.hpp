// Murim MMORPG - 仓库系统
// 负责玩家仓库存储功能

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace Murim {
namespace MapServer {

// 仓库栏位
struct WarehouseSlot {
    uint32_t slot_id;          // 栏位ID (1-100)
    uint64_t item_instance_id;  // 物品实例ID
    uint32_t item_template_id;  // 物品模板ID
    std::string item_name;
    int16_t quantity;          // 数量

    // ITEMBASE 字段 (匹配 ItemInstance)
    uint32_t durability = 0;     // 耐久度
    uint32_t rare_idx = 0;       // 稀有索引
    uint32_t item_param = 0;     // 物品参数
};

// 仓库数据
struct WarehouseData {
    uint64_t character_id;
    std::string character_name;
    uint32_t max_slots;         // 最大栏位数
    uint32_t used_slots;        // 已使用栏位数
    std::vector<WarehouseSlot> slots;
    uint64_t money;             // 金币（仓库内独立金币）
};

// 仓库管理器
class WarehouseManager {
public:
    static WarehouseManager& Instance();

    bool Initialize();
    void Shutdown();

    // 仓库操作
    bool OpenWarehouse(uint64_t player_id, uint32_t npc_id);
    bool CloseWarehouse(uint64_t player_id);

    // 存取物品
    struct DepositResult {
        bool success;
        std::string message;
        uint32_t new_slot_id;
    };
    DepositResult DepositItem(uint64_t player_id, uint64_t item_instance_id);

    struct WithdrawResult {
        bool success;
        std::string message;
        uint64_t item_instance_id;
    };
    WithdrawResult WithdrawItem(uint64_t player_id, uint32_t slot_id);

    // 整理仓库
    bool SortWarehouse(uint64_t player_id);
    bool ExpandWarehouse(uint64_t player_id, uint32_t additional_slots);

    // 查询
    const WarehouseData* GetWarehouse(uint64_t player_id) const;

    // 金币操作
    bool DepositMoney(uint64_t player_id, uint64_t amount);
    bool WithdrawMoney(uint64_t player_id, uint64_t amount);
    uint64_t GetWarehouseMoney(uint64_t player_id) const;

private:
    WarehouseManager() = default;
    ~WarehouseManager() = default;

    std::unordered_map<uint64_t, WarehouseData> warehouses_;

    // 玩家当前打开的仓库
    std::unordered_map<uint64_t, uint32_t> player_warehouses_;  // player_id -> npc_id

    // 从数据库加载仓库数据
    void LoadWarehouseFromDatabase(uint64_t player_id);
    bool SaveWarehouseToDatabase(uint64_t player_id);
};

} // namespace MapServer
} // namespace Murim
