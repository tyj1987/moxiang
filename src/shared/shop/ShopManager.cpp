#include "ShopManager.hpp"
#include "../item/ItemManager.hpp"
#include "../character/CharacterManager.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>

namespace Murim {
namespace Game {

// ========== ShopInfo 实现 ==========

void ShopInfo::AddItem(uint32_t item_id, uint32_t price, uint32_t stock) {
    ShopItem item;
    item.item_id = item_id;
    item.price = price;
    item.stock = stock;
    item.is_available = true;
    items[item_id] = item;
}

void ShopInfo::RemoveItem(uint32_t item_id) {
    items.erase(item_id);
}

const ShopItem* ShopInfo::GetItem(uint32_t item_id) const {
    auto it = items.find(item_id);
    if (it != items.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ShopInfo::HasStock(uint32_t item_id) const {
    auto it = items.find(item_id);
    if (it == items.end()) {
        return false;
    }
    // stock=0 表示无限库存
    return it->second.stock == 0 || it->second.stock > 0;
}

void ShopInfo::DecreaseStock(uint32_t item_id, uint32_t count) {
    auto it = items.find(item_id);
    if (it != items.end() && it->second.stock > 0) {
        it->second.stock = std::max(0, static_cast<int>(it->second.stock) - static_cast<int>(count));
    }
}

// ========== ShopManager 实现 ==========

ShopManager& ShopManager::Instance() {
    static ShopManager instance;
    return instance;
}

ShopManager::ShopManager() {
    spdlog::info("ShopManager created");
}

bool ShopManager::Initialize() {
    spdlog::info("Initializing ShopManager...");

    if (!LoadShops()) {
        spdlog::error("Failed to load shops");
        return false;
    }

    spdlog::info("ShopManager initialized: {} shops", shops_.size());
    return true;
}

bool ShopManager::LoadShops() {
    // TODO: 从数据库加载商店数据
    // 目前使用测试数据

    // 商店1：新手村武器店
    auto shop1 = std::make_shared<ShopInfo>();
    shop1->shop_id = 1001;
    shop1->shop_name = "新手村武器店";
    shop1->npc_id = 1001;  // 铁匠张三
    shop1->shop_type = 1;   // 武器店
    shop1->buy_discount = 1.0f;
    shop1->sell_discount = 0.5f;

    // 添加测试商品（物品ID需要与ItemManager中的数据匹配）
    shop1->AddItem(1001, 100, 0);  // 新手剑，100铜币，无限库存
    shop1->AddItem(1002, 150, 0);  // 新手斧，150铜币，无限库存
    shop1->AddItem(1003, 200, 0);  // 新手弓，200铜币，无限库存

    shops_[shop1->shop_id] = shop1;
    npc_to_shop_[shop1->npc_id] = shop1->shop_id;

    // 商店2：新手村防具店
    auto shop2 = std::make_shared<ShopInfo>();
    shop2->shop_id = 1002;
    shop2->shop_name = "新手村防具店";
    shop2->npc_id = 1002;  // 裁缝李四
    shop2->shop_type = 2;   // 防具店
    shop2->buy_discount = 1.0f;
    shop2->sell_discount = 0.5f;

    shop2->AddItem(2001, 80, 0);   // 新手布衣，80铜币
    shop2->AddItem(2002, 120, 0);  // 新手皮甲，120铜币
    shop2->AddItem(2003, 200, 0);  // 新手铁甲，200铜币

    shops_[shop2->shop_id] = shop2;
    npc_to_shop_[shop2->npc_id] = shop2->shop_id;

    // 商店3：杂货铺
    auto shop3 = std::make_shared<ShopInfo>();
    shop3->shop_id = 1003;
    shop3->shop_name = "王掌柜杂货铺";
    shop3->npc_id = 1003;  // 杂货商王掌柜
    shop3->shop_type = 3;   // 消耗品
    shop3->buy_discount = 1.0f;
    shop3->sell_discount = 0.3f;

    shop3->AddItem(3001, 10, 100);   // 生命药水，10铜币，库存100
    shop3->AddItem(3002, 20, 50);    // 魔法药水，20铜币，库存50
    shop3->AddItem(3003, 5, 0);      // 面包，5铜币，无限库存

    shops_[shop3->shop_id] = shop3;
    npc_to_shop_[shop3->npc_id] = shop3->shop_id;

    spdlog::info("Loaded {} shops (test data)", shops_.size());
    return true;
}

std::shared_ptr<ShopInfo> ShopManager::GetShop(uint32_t shop_id) {
    auto it = shops_.find(shop_id);
    if (it != shops_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<ShopInfo> ShopManager::GetShopByNPC(uint32_t npc_id) {
    auto it = npc_to_shop_.find(npc_id);
    if (it != npc_to_shop_.end()) {
        return GetShop(it->second);
    }
    return nullptr;
}

bool ShopManager::HasShop(uint32_t shop_id) const {
    return shops_.find(shop_id) != shops_.end();
}

// ========== 交易处理 ==========

TradeResult ShopManager::HandleBuyRequest(uint64_t character_id, uint32_t shop_id, uint32_t item_id, uint32_t count) {
    TradeResult result;
    result.success = false;

    // 获取商店
    auto shop = GetShop(shop_id);
    if (!shop) {
        result.error_code = 1;  // 商店不存在
        result.error_message = "商店不存在";
        return result;
    }

    // 获取商品
    auto item = shop->GetItem(item_id);
    if (!item) {
        result.error_code = 2;  // 商品不存在
        result.error_message = "商店没有此商品";
        return result;
    }

    // 验证请求
    if (!ValidateBuyRequest(character_id, shop.get(), item, count)) {
        result.error_code = 3;  // 请求无效
        result.error_message = "购买请求无效";
        return result;
    }

    // 执行购买
    return ExecuteBuy(character_id, shop.get(), item, count);
}

TradeResult ShopManager::HandleSellRequest(uint64_t character_id, uint32_t shop_id, uint32_t item_id, uint32_t count) {
    TradeResult result;
    result.success = false;

    // 获取商店
    auto shop = GetShop(shop_id);
    if (!shop) {
        result.error_code = 1;  // 商店不存在
        result.error_message = "商店不存在";
        return result;
    }

    // 验证请求
    if (!ValidateSellRequest(character_id, shop.get(), item_id, count)) {
        result.error_code = 3;  // 请求无效
        result.error_message = "出售请求无效";
        return result;
    }

    // 执行出售
    return ExecuteSell(character_id, shop.get(), item_id, count);
}

// ========== 价格计算 ==========

uint32_t ShopManager::CalculateBuyPrice(uint32_t shop_id, uint32_t item_id, uint32_t count) const {
    auto it = shops_.find(shop_id);
    if (it == shops_.end()) {
        return 0;
    }

    const ShopItem* item = it->second->GetItem(item_id);
    if (!item) {
        return 0;
    }

    // 应用购买折扣
    float discount = it->second->buy_discount;
    uint32_t total_price = static_cast<uint32_t>(item->price * count * discount);
    return total_price;
}

uint32_t ShopManager::CalculateSellPrice(uint32_t shop_id, uint32_t item_id, uint32_t count) const {
    auto it = shops_.find(shop_id);
    if (it == shops_.end()) {
        return 0;
    }

    // 获取物品基础价格（从ItemManager）
    // TODO: 需要ItemManager提供物品基础价格查询
    uint32_t base_price = 10;  // 临时默认值

    // 应用出售折扣
    float discount = it->second->sell_discount;
    uint32_t total_price = static_cast<uint32_t>(base_price * count * discount);
    return total_price;
}

// ========== 辅助方法 ==========

bool ShopManager::ValidateBuyRequest(uint64_t character_id, const ShopInfo* shop, const ShopItem* item, uint32_t count) {
    if (count == 0 || count > 999) {
        spdlog::warn("Invalid buy count: {}", count);
        return false;
    }

    if (!item->is_available) {
        spdlog::warn("Item not available: {}", item->item_id);
        return false;
    }

    if (!shop->HasStock(item->item_id)) {
        spdlog::warn("Item out of stock: {}", item->item_id);
        return false;
    }

    // TODO: 检查玩家金钱是否足够
    // TODO: 检查玩家背包是否有空位

    return true;
}

TradeResult ShopManager::ExecuteBuy(uint64_t character_id, const ShopInfo* shop, const ShopItem* item, uint32_t count) {
    TradeResult result;
    result.success = true;
    result.error_code = 0;
    result.item_id = item->item_id;
    result.item_count = count;

    // 计算价格
    uint32_t total_price = CalculateBuyPrice(shop->shop_id, item->item_id, count);

    // TODO: 扣除玩家金钱
    // TODO: 添加物品到玩家背包
    // TODO: 减少商店库存（如果不是无限库存）

    result.new_money = 0;  // TODO: 获取玩家新金钱数量

    spdlog::info("Character {} bought {}x item {} from shop {} for {} copper",
                 character_id, count, item->item_id, shop->shop_id, total_price);

    return result;
}

bool ShopManager::ValidateSellRequest(uint64_t character_id, const ShopInfo* shop, uint32_t item_id, uint32_t count) {
    if (count == 0 || count > 999) {
        spdlog::warn("Invalid sell count: {}", count);
        return false;
    }

    // TODO: 检查玩家是否拥有该物品
    // TODO: 检查玩家拥有足够的物品数量

    return true;
}

TradeResult ShopManager::ExecuteSell(uint64_t character_id, const ShopInfo* shop, uint32_t item_id, uint32_t count) {
    TradeResult result;
    result.success = true;
    result.error_code = 0;
    result.item_id = item_id;
    result.item_count = count;

    // 计算价格
    uint32_t total_price = CalculateSellPrice(shop->shop_id, item_id, count);

    // TODO: 从玩家背包移除物品
    // TODO: 增加玩家金钱

    result.new_money = 0;  // TODO: 获取玩家新金钱数量

    spdlog::info("Character {} sold {}x item {} to shop {} for {} copper",
                 character_id, count, item_id, shop->shop_id, total_price);

    return result;
}

} // namespace Game
} // namespace Murim
