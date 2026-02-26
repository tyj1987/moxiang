// Murim MMORPG - Shop DAO实现

#include "ShopDAO.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Database {

std::vector<ShopInfo> ShopDAO::LoadAllShops() {
    std::vector<ShopInfo> shops;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return shops;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "SELECT shop_id, shop_name, npc_id, shop_type, buy_rate, sell_rate "
                         "FROM shops "
                         "ORDER BY shop_id";

        auto result = executor.Query(sql);

        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            spdlog::debug("No shops found in database");
            return shops;
        }

        for (int i = 0; i < result->RowCount(); ++i) {
            ShopInfo info;
            info.shop_id = result->Get<uint32_t>(i, "shop_id").value_or(0);
            info.shop_name = result->Get<std::string>(i, "shop_name").value_or("");
            info.npc_id = result->Get<uint32_t>(i, "npc_id").value_or(0);
            info.shop_type = result->Get<uint16_t>(i, "shop_type").value_or(0);
            info.buy_rate = result->Get<float>(i, "buy_rate").value_or(1.0f);
            info.sell_rate = result->Get<float>(i, "sell_rate").value_or(1.0f);

            shops.push_back(info);
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Loaded {} shops from database", shops.size());

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to load shops: {}", e.what());
    }

    return shops;
}

std::vector<ShopInfo> ShopDAO::LoadByNPC(uint32_t npc_id) {
    std::vector<ShopInfo> shops;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return shops;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "SELECT shop_id, shop_name, npc_id, shop_type, buy_rate, sell_rate "
                         "FROM shops "
                         "WHERE npc_id = $1 "
                         "ORDER BY shop_id";

        auto result = executor.QueryParams(sql, {std::to_string(npc_id)});

        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            spdlog::debug("No shops found for NPC {}", npc_id);
            return shops;
        }

        for (int i = 0; i < result->RowCount(); ++i) {
            ShopInfo info;
            info.shop_id = result->Get<uint32_t>(i, "shop_id").value_or(0);
            info.shop_name = result->Get<std::string>(i, "shop_name").value_or("");
            info.npc_id = result->Get<uint32_t>(i, "npc_id").value_or(0);
            info.shop_type = result->Get<uint16_t>(i, "shop_type").value_or(0);
            info.buy_rate = result->Get<float>(i, "buy_rate").value_or(1.0f);
            info.sell_rate = result->Get<float>(i, "sell_rate").value_or(1.0f);

            shops.push_back(info);
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::debug("Loaded {} shops for NPC {}", shops.size(), npc_id);

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to load shops for NPC {}: {}", npc_id, e.what());
    }

    return shops;
}

std::vector<ShopItemInfo> ShopDAO::LoadShopItems(uint32_t shop_id) {
    std::vector<ShopItemInfo> items;

    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return items;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "SELECT shop_id, item_id, stock, price_multiplier, restock_time "
                         "FROM shop_items "
                         "WHERE shop_id = $1 "
                         "ORDER BY item_id";

        auto result = executor.QueryParams(sql, {std::to_string(shop_id)});

        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            spdlog::debug("No items found for shop {}", shop_id);
            return items;
        }

        for (int i = 0; i < result->RowCount(); ++i) {
            ShopItemInfo info;
            info.shop_id = result->Get<uint32_t>(i, "shop_id").value_or(0);
            info.item_id = result->Get<uint32_t>(i, "item_id").value_or(0);
            info.stock = result->Get<int16_t>(i, "stock").value_or(0);
            info.price_multiplier = result->Get<float>(i, "price_multiplier").value_or(1.0f);
            info.restock_time = result->Get<uint32_t>(i, "restock_time").value_or(0);

            items.push_back(info);
        }

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::debug("Loaded {} items for shop {}", items.size(), shop_id);

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to load items for shop {}: {}", shop_id, e.what());
    }

    return items;
}

bool ShopDAO::CreateShop(const ShopInfo& shop_info) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "INSERT INTO shops (shop_name, npc_id, shop_type, buy_rate, sell_rate) "
                         "VALUES ($1, $2, $3, $4, $5)";

        bool success = executor.ExecuteParams(sql, {
            shop_info.shop_name,
            std::to_string(shop_info.npc_id),
            std::to_string(shop_info.shop_type),
            std::to_string(shop_info.buy_rate),
            std::to_string(shop_info.sell_rate)
        });

        ConnectionPool::Instance().ReturnConnection(conn);

        if (success) {
            spdlog::info("Created shop: {}", shop_info.shop_name);
            return true;
        } else {
            spdlog::error("Failed to create shop: {}", shop_info.shop_name);
            return false;
        }

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to create shop: {}", e.what());
        return false;
    }
}

bool ShopDAO::AddShopItem(const ShopItemInfo& item_info) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "INSERT INTO shop_items (shop_id, item_id, stock, price_multiplier, restock_time) "
                         "VALUES ($1, $2, $3, $4, $5)";

        bool success = executor.ExecuteParams(sql, {
            std::to_string(item_info.shop_id),
            std::to_string(item_info.item_id),
            std::to_string(item_info.stock),
            std::to_string(item_info.price_multiplier),
            std::to_string(item_info.restock_time)
        });

        ConnectionPool::Instance().ReturnConnection(conn);

        if (success) {
            spdlog::debug("Added item {} to shop {}", item_info.item_id, item_info.shop_id);
            return true;
        } else {
            spdlog::error("Failed to add item {} to shop {}", item_info.item_id, item_info.shop_id);
            return false;
        }

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to add item to shop: {}", e.what());
        return false;
    }
}

bool ShopDAO::UpdateStock(uint32_t shop_id, uint32_t item_id, int16_t new_stock) {
    auto conn = ConnectionPool::Instance().GetConnection();
    if (!conn) {
        spdlog::error("Failed to get database connection");
        return false;
    }

    try {
        QueryExecutor executor(conn);
        std::string sql = "UPDATE shop_items SET stock = $1 "
                         "WHERE shop_id = $2 AND item_id = $3";

        bool success = executor.ExecuteParams(sql, {
            std::to_string(new_stock),
            std::to_string(shop_id),
            std::to_string(item_id)
        });

        ConnectionPool::Instance().ReturnConnection(conn);
        return success;

    } catch (const std::exception& e) {
        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::error("Failed to update stock: {}", e.what());
        return false;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
