#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>

namespace Murim {
namespace Game {

// 前置声明
class ItemManager;

/**
 * @brief 商品信息
 */
struct ShopItem {
    uint32_t item_id;        // 物品ID
    uint32_t price;          // 价格（铜币）
    uint32_t stock;          // 库存（0=无限）
    bool is_available;       // 是否可用
};

/**
 * @brief 商店信息
 */
struct ShopInfo {
    uint32_t shop_id;                // 商店ID
    std::string shop_name;           // 商店名称
    uint32_t npc_id;                 // 所属NPC ID
    uint8_t shop_type;               // 商店类型 (0=普通, 1=武器, 2=防具, 3=消耗品)
    float buy_discount;              // 购买折扣 (0.0-1.0)
    float sell_discount;             // 出售折扣 (0.0-1.0)

    std::unordered_map<uint32_t, ShopItem> items;  // 商品列表 <item_id, ShopItem>

    /**
     * @brief 添加商品
     */
    void AddItem(uint32_t item_id, uint32_t price, uint32_t stock = 0);

    /**
     * @brief 移除商品
     */
    void RemoveItem(uint32_t item_id);

    /**
     * @brief 获取商品
     */
    const ShopItem* GetItem(uint32_t item_id) const;

    /**
     * @brief 检查商品是否有库存
     */
    bool HasStock(uint32_t item_id) const;

    /**
     * @brief 减少库存
     */
    void DecreaseStock(uint32_t item_id, uint32_t count = 1);
};

/**
 * @brief 交易结果
 */
struct TradeResult {
    bool success;              // 是否成功
    uint8_t error_code;        // 错误码
    std::string error_message; // 错误消息

    // 成功时的返回数据
    uint64_t new_money;        // 新的金钱数量
    uint32_t item_id;          // 物品ID
    uint32_t item_count;       // 物品数量
};

/**
 * @brief 商店管理器
 *
 * 负责管理所有商店和商品交易
 */
class ShopManager {
public:
    /**
     * @brief 获取单例实例
     */
    static ShopManager& Instance();

    // ========== 初始化 ==========

    /**
     * @brief 初始化商店管理器
     * @return 是否初始化成功
     */
    bool Initialize();

    /**
     * @brief 加载商店数据
     * @return 是否加载成功
     */
    bool LoadShops();

    // ========== 商店查询 ==========

    /**
     * @brief 获取商店信息
     * @param shop_id 商店ID
     * @return 商店信息（如果存在）
     */
    std::shared_ptr<ShopInfo> GetShop(uint32_t shop_id);

    /**
     * @brief 根据NPC ID获取商店
     * @param npc_id NPC ID
     * @return 商店信息（如果存在）
     */
    std::shared_ptr<ShopInfo> GetShopByNPC(uint32_t npc_id);

    /**
     * @brief 检查商店是否存在
     */
    bool HasShop(uint32_t shop_id) const;

    /**
     * @brief 获取商店数量
     */
    size_t GetShopCount() const { return shops_.size(); }

    // ========== 交易处理 ==========

    /**
     * @brief 处理购买请求
     * @param character_id 角色ID
     * @param shop_id 商店ID
     * @param item_id 物品ID
     * @param count 购买数量
     * @return 交易结果
     */
    TradeResult HandleBuyRequest(uint64_t character_id, uint32_t shop_id, uint32_t item_id, uint32_t count);

    /**
     * @brief 处理出售请求
     * @param character_id 角色ID
     * @param shop_id 商店ID
     * @param item_id 物品ID
     * @param count 出售数量
     * @return 交易结果
     */
    TradeResult HandleSellRequest(uint64_t character_id, uint32_t shop_id, uint32_t item_id, uint32_t count);

    // ========== 价格计算 ==========

    /**
     * @brief 计算购买价格
     * @param shop_id 商店ID
     * @param item_id 物品ID
     * @param count 数量
     * @return 总价格
     */
    uint32_t CalculateBuyPrice(uint32_t shop_id, uint32_t item_id, uint32_t count) const;

    /**
     * @brief 计算出售价格
     * @param shop_id 商店ID
     * @param item_id 物品ID
     * @param count 数量
     * @return 总价格
     */
    uint32_t CalculateSellPrice(uint32_t shop_id, uint32_t item_id, uint32_t count) const;

private:
    ShopManager();
    ~ShopManager() = default;
    ShopManager(const ShopManager&) = delete;
    ShopManager& operator=(const ShopManager&) = delete;

    // ========== 数据 ==========

    std::unordered_map<uint32_t, std::shared_ptr<ShopInfo>> shops_;  // <shop_id, ShopInfo>
    std::unordered_map<uint32_t, uint32_t> npc_to_shop_;             // <npc_id, shop_id>

    // ========== 辅助方法 ==========

    /**
     * @brief 验证购买请求
     */
    bool ValidateBuyRequest(uint64_t character_id, const ShopInfo* shop, const ShopItem* item, uint32_t count);

    /**
     * @brief 执行购买交易
     */
    TradeResult ExecuteBuy(uint64_t character_id, const ShopInfo* shop, const ShopItem* item, uint32_t count);

    /**
     * @brief 验证出售请求
     */
    bool ValidateSellRequest(uint64_t character_id, const ShopInfo* shop, uint32_t item_id, uint32_t count);

    /**
     * @brief 执行出售交易
     */
    TradeResult ExecuteSell(uint64_t character_id, const ShopInfo* shop, uint32_t item_id, uint32_t count);
};

} // namespace Game
} // namespace Murim
