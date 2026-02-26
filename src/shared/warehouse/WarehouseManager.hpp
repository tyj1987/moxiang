#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>

namespace Murim {
namespace Game {

/**
 * @brief 仓库物品信息
 */
struct WarehouseItem {
    uint64_t item_instance_id;   // 物品实例ID
    uint32_t item_id;            // 物品模板ID
    uint32_t count;              // 数量
    uint32_t position;           // 仓库格子位置
    bool is_locked;             // 是否锁定
};

/**
 * @brief 仓库信息
 */
struct WarehouseInfo {
    uint64_t character_id;       // 角色ID
    uint32_t capacity;           // 仓库容量（格子数）
    uint32_t used_slots;         // 已使用格子数
    uint64_t gold;               // 开通仓库花费的金币
    std::vector<WarehouseItem> items;  // 仓库物品列表

    /**
     * @brief 添加物品
     */
    bool AddItem(const WarehouseItem& item);

    /**
     * @brief 移除物品
     */
    bool RemoveItem(uint64_t item_instance_id);

    /**
     * @brief 获取物品
     */
    WarehouseItem* GetItem(uint64_t item_instance_id);

    /**
     * @brief 查找空格子
     */
    uint32_t FindEmptySlot() const;

    /**
     * @brief 更新已使用格子数
     */
    void UpdateUsedSlots();
};

/**
 * @brief 仓库操作结果
 */
struct WarehouseResult {
    bool success;                  // 是否成功
    uint8_t error_code;            // 错误码
    std::string error_message;     // 错误消息

    // 成功时的返回数据
    uint32_t new_gold;             // 新的金币数量
    uint64_t item_instance_id;     // 物品实例ID
    uint32_t position;             // 物品位置
};

/**
 * @brief 仓库管理器
 *
 * 负责管理玩家仓库和物品存取操作
 */
class WarehouseManager {
public:
    /**
     * @brief 获取单例实例
     */
    static WarehouseManager& Instance();

    // ========== 初始化 ==========

    /**
     * @brief 初始化仓库管理器
     * @return 是否初始化成功
     */
    bool Initialize();

    // ========== 仓库查询 ==========

    /**
     * @brief 获取仓库信息
     * @param character_id 角色ID
     * @return 仓库信息（如果存在）
     */
    std::shared_ptr<WarehouseInfo> GetWarehouse(uint64_t character_id);

    /**
     * @brief 创建仓库
     * @param character_id 角色ID
     * @param capacity 仓库容量
     * @return 是否创建成功
     */
    bool CreateWarehouse(uint64_t character_id, uint32_t capacity = 50);

    /**
     * @brief 检查仓库是否存在
     */
    bool HasWarehouse(uint64_t character_id) const;

    /**
     * @brief 获取仓库数量
     */
    size_t GetWarehouseCount() const { return warehouses_.size(); }

    // ========== 物品操作 ==========

    /**
     * @brief 存入物品
     * @param character_id 角色ID
     * @param item_instance_id 物品实例ID
     * @param item_id 物品模板ID
     * @param count 数量
     * @return 操作结果
     */
    WarehouseResult HandleDepositItem(
        uint64_t character_id,
        uint64_t item_instance_id,
        uint32_t item_id,
        uint32_t count
    );

    /**
     * @brief 取出物品
     * @param character_id 角色ID
     * @param slot_id 栏位ID
     * @return 操作结果
     */
    WarehouseResult HandleWithdrawItem(
        uint64_t character_id,
        uint32_t slot_id
    );

    /**
     * @brief 整理仓库
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool HandleSortWarehouse(uint64_t character_id);

    // ========== 金币操作 ==========

    /**
     * @brief 存入金币
     * @param character_id 角色ID
     * @param amount 金币数量
     * @return 操作结果
     */
    WarehouseResult HandleDepositGold(uint64_t character_id, uint64_t amount);

    /**
     * @brief 取出金币
     * @param character_id 角色ID
     * @param amount 金币数量
     * @return 操作结果
     */
    WarehouseResult HandleWithdrawGold(uint64_t character_id, uint64_t amount);

private:
    WarehouseManager();
    ~WarehouseManager() = default;
    WarehouseManager(const WarehouseManager&) = delete;
    WarehouseManager& operator=(const WarehouseManager&) = delete;

    // ========== 数据 ==========

    std::unordered_map<uint64_t, std::shared_ptr<WarehouseInfo>> warehouses_;  // <character_id, WarehouseInfo>

    // ========== 辅助方法 ==========

    /**
     * @brief 从数据库加载仓库数据
     */
    bool LoadWarehouseFromDatabase(uint64_t character_id);

    /**
     * @brief 保存仓库数据到数据库
     */
    bool SaveWarehouseToDatabase(uint64_t character_id);

    /**
     * @brief 验证仓库操作
     */
    bool ValidateWarehouseOperation(uint64_t character_id);

    /**
     * @brief 执行存入物品
     */
    WarehouseResult ExecuteDeposit(
        uint64_t character_id,
        uint64_t item_instance_id,
        uint32_t item_id,
        uint32_t count
    );

    /**
     * @brief 执行取出物品
     */
    WarehouseResult ExecuteWithdraw(
        uint64_t character_id,
        uint64_t item_instance_id
    );
};

} // namespace Game
} // namespace Murim
