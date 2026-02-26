// Murim MMORPG - 商店系统
// 负责NPC商店的交易功能

#pragma once

#include <unordered_map>
#include <vector>
#include "shared/item/Item.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace MapServer {

// 商店物品信息
struct ShopItem {
    uint32_t item_id;           // 物品ID
    std::string name;          // 物品名称
    int16_t stock;              // 库存 (-1=无限)
    float price_multiplier;    // 价格倍率
    uint32_t restock_time;      // 补货时间(秒)
    uint32_t base_price;        // 基础价格
    uint32_t sell_price;        // 售价

    // 是否可以购买
    bool CanBuy(int16_t quantity) const {
        return stock == -1 || stock >= quantity;
    }

    // 购买后减少库存
    void DecreaseStock(int16_t quantity) {
        if (stock != -1) {
            stock -= quantity;
        }
    }
};

// 商店信息
struct ShopInfo {
    uint32_t shop_id;
    std::string shop_name;
    uint32_t npc_id;
    uint16_t shop_type;         // 0=普通, 1=武器, 2=防具, 3=药品
    float buy_rate;             // 买入汇率
    float sell_rate;            // 卖出汇率
    std::unordered_map<uint32_t, ShopItem> items;

    // 添加商品
    void AddItem(const ShopItem& item) {
        items[item.item_id] = item;
    }

    // 获取商品
    const ShopItem* GetItem(uint32_t item_id) const {
        auto it = items.find(item_id);
        if (it != items.end()) {
            return &it->second;
        }
        return nullptr;
    }

    // 获取所有商品
    const std::unordered_map<uint32_t, ShopItem>& GetItems() const {
        return items;
    }
};

// 商店管理器
class ShopManager {
public:
    static ShopManager& Instance();

    bool Initialize();
    void Shutdown();

    // 加载商店数据
    void LoadShops();
    void LoadShopFromDatabase(uint32_t shop_id);

    // 商店操作
    bool OpenShop(uint64_t player_id, uint32_t shop_id);
    bool CloseShop(uint64_t player_id);

    // 购买物品
    struct BuyResult {
        bool success;
        std::string message;
        uint64_t new_item_id;
    };
    BuyResult BuyItem(uint64_t player_id, uint32_t shop_id, uint32_t item_id, int16_t quantity);

    // 出售物品
    struct SellResult {
        bool success;
        std::string message;
        uint32_t money_gained;
    };
    SellResult SellItem(uint64_t player_id, uint32_t shop_id, uint64_t item_instance_id);

    // 查询
    const ShopInfo* GetShop(uint32_t shop_id) const;
    std::vector<uint32_t> GetShopIdsByNPC(uint32_t npc_id) const;

private:
    ShopManager() = default;
    ~ShopManager() = default;

    std::unordered_map<uint32_t, ShopInfo> shops_;  // shop_id -> ShopInfo
    std::unordered_map<uint32_t, std::vector<uint32_t>> npc_shops_;  // npc_id -> shop_ids

    // 玩家当前打开的商店
    std::unordered_map<uint64_t, uint32_t> player_shops_;  // player_id -> shop_id
};

} // namespace MapServer
} // namespace Murim
