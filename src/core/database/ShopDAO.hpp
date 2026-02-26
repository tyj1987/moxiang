// Murim MMORPG - Shop DAO
// 负责商店数据的数据库访问

#pragma once

#include <string>
#include <vector>
#include <optional>

namespace Murim {
namespace Core {
namespace Database {

// 商店信息结构
struct ShopInfo {
    uint32_t shop_id;
    std::string shop_name;
    uint32_t npc_id;
    uint16_t shop_type;     // 0=普通, 1=武器, 2=防具, 3=药品
    float buy_rate;
    float sell_rate;
};

// 商店物品信息
struct ShopItemInfo {
    uint32_t shop_id;
    uint32_t item_id;
    int16_t stock;
    float price_multiplier;
    uint32_t restock_time;
};

// 商店DAO类
class ShopDAO {
public:
    // 加载所有商店
    static std::vector<ShopInfo> LoadAllShops();

    // 根据NPC加载商店
    static std::vector<ShopInfo> LoadByNPC(uint32_t npc_id);

    // 加载商店物品
    static std::vector<ShopItemInfo> LoadShopItems(uint32_t shop_id);

    // 创建商店
    static bool CreateShop(const ShopInfo& shop_info);

    // 添加商店物品
    static bool AddShopItem(const ShopItemInfo& item_info);

    // 更新库存
    static bool UpdateStock(uint32_t shop_id, uint32_t item_id, int16_t new_stock);
};

} // namespace Database
} // namespace Core
} // namespace Murim
