// Murim MMORPG - 商店系统实现

#include "ShopManager.hpp"
#include "shared/character/CharacterManager.hpp"
#include "shared/item/ItemManager.hpp"
#include "core/database/ShopDAO.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace MapServer {

using ConnectionPool = Murim::Core::Database::ConnectionPool;

ShopManager& ShopManager::Instance() {
    static ShopManager instance;
    return instance;
}

bool ShopManager::Initialize() {
    spdlog::info("Initializing ShopManager...");

    LoadShops();

    spdlog::info("ShopManager initialized: {} shops loaded", shops_.size());
    return true;
}

void ShopManager::Shutdown() {
    spdlog::info("Shutting down ShopManager...");
    shops_.clear();
    npc_shops_.clear();
    player_shops_.clear();
}

void ShopManager::LoadShops() {
    spdlog::info("Loading shops from database...");

    try {
        // 从数据库加载商店数据
        auto db_shops = Murim::Core::Database::ShopDAO::LoadAllShops();

        for (const auto& db_shop : db_shops) {
            // 转换 Database::ShopInfo → MapServer::ShopInfo
            ShopInfo shop_info;
            shop_info.shop_id = db_shop.shop_id;
            shop_info.shop_name = db_shop.shop_name;
            shop_info.npc_id = db_shop.npc_id;
            shop_info.shop_type = db_shop.shop_type;
            shop_info.buy_rate = db_shop.buy_rate;
            shop_info.sell_rate = db_shop.sell_rate;

            // 按NPC索引
            npc_shops_[shop_info.npc_id].push_back(shop_info.shop_id);

            // 加载商店物品
            auto db_items = Murim::Core::Database::ShopDAO::LoadShopItems(shop_info.shop_id);
            for (const auto& db_item : db_items) {
                ShopItem item;
                item.item_id = db_item.item_id;
                item.stock = db_item.stock;
                item.price_multiplier = db_item.price_multiplier;
                item.restock_time = db_item.restock_time;
                item.base_price = 0;  // TODO: 从 item_templates 表获取
                item.sell_price = 0;

                shop_info.AddItem(item);
            }

            shops_[shop_info.shop_id] = shop_info;
        }

        spdlog::info("Loaded {} shops from database", shops_.size());

    } catch (const std::exception& e) {
        spdlog::error("Failed to load shops: {}", e.what());
    }
}

bool ShopManager::OpenShop(uint64_t player_id, uint32_t shop_id) {
    // 检查商店是否存在
    auto shop_it = shops_.find(shop_id);
    if (shop_it == shops_.end()) {
        spdlog::error("Shop {} not found", shop_id);
        return false;
    }

    // 关闭之前的商店
    CloseShop(player_id);

    // 记录玩家打开的商店
    player_shops_[player_id] = shop_id;

    spdlog::info("Player {} opened shop {}", player_id, shop_it->second.shop_name);

    // TODO: 发送网络消息给客户端，打开商店UI
    // TODO: 发送商店商品列表

    return true;
}

bool ShopManager::CloseShop(uint64_t player_id) {
    auto it = player_shops_.find(player_id);
    if (it != player_shops_.end()) {
        spdlog::info("Player {} closing shop {}", player_id, it->second);
        player_shops_.erase(it);
        return true;
    }
    return false;
}

ShopManager::BuyResult ShopManager::BuyItem(
    uint64_t player_id,
    uint32_t shop_id,
    uint32_t item_id,
    int16_t quantity
) {
    BuyResult result;
    result.success = false;

    // 检查商店是否打开
    auto shop_it = player_shops_.find(player_id);
    if (shop_it == player_shops_.end() || shop_it->second != shop_id) {
        result.message = "商店未打开";
        return result;
    }

    // 获取商店信息
    auto shop = GetShop(shop_id);
    if (!shop) {
        result.message = "商店不存在";
        return result;
    }

    // 获取商品信息
    const ShopItem* shop_item = shop->GetItem(item_id);
    if (!shop_item) {
        result.message = "商店不售卖此物品";
        return result;
    }

    // 检查库存
    if (!shop_item->CanBuy(quantity)) {
        result.message = "库存不足";
        return result;
    }

    // 计算价格
    uint32_t total_cost = static_cast<uint32_t>(shop_item->base_price * shop_item->price_multiplier * quantity);

    // TODO: 检查玩家金钱
    // TODO: 扣除玩家金钱
    // TODO: 给予物品
    // TODO: 减少商店库存

    result.success = true;
    result.message = "购买成功";
    // result.new_item_id = ...;

    spdlog::info("Player {} bought item {} (x{}) from shop {} for {} money",
                 player_id, item_id, quantity, shop_id, total_cost);

    return result;
}

ShopManager::SellResult ShopManager::SellItem(
    uint64_t player_id,
    uint32_t shop_id,
    uint64_t item_instance_id
) {
    SellResult result;
    result.success = false;

    // 检查商店是否存在
    auto shop = GetShop(shop_id);
    if (!shop) {
        result.message = "商店不存在";
        return result;
    }

    // TODO: 检查物品是否存在
    // TODO: 检查物品是否可出售
    // TODO: 计算售价
    // TODO: 移除物品
    // TODO: 增加金钱

    result.success = true;
    result.message = "出售成功";
    result.money_gained = 100;

    spdlog::info("Player {} sold item {} to shop {}", player_id, item_instance_id, shop_id);

    return result;
}

const ShopInfo* ShopManager::GetShop(uint32_t shop_id) const {
    auto it = shops_.find(shop_id);
    if (it != shops_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<uint32_t> ShopManager::GetShopIdsByNPC(uint32_t npc_id) const {
    auto it = npc_shops_.find(npc_id);
    if (it != npc_shops_.end()) {
        return it->second;
    }
    return {};
}

} // namespace MapServer
} // namespace Murim
