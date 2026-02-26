#include "MarketManager.hpp"
#include "shared/mail/MailManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>
#include <regex>

namespace Murim {
namespace Game {

MarketManager& MarketManager::Instance() {
    static MarketManager instance;
    return instance;
}

bool MarketManager::Initialize() {
    spdlog::info("Initializing MarketManager...");
    // TODO: 从数据库加载活跃挂单
    spdlog::info("MarketManager initialized successfully");
    return true;
}

// ========== 挂单操作 ==========

ListResult MarketManager::ListItem(
    uint64_t seller_id,
    const std::string& seller_name,
    uint64_t item_instance_id,
    uint32_t item_id,
    const std::string& item_name,
    uint32_t quantity,
    uint64_t price,
    uint64_t buyout_price,
    uint32_t duration_hours) {

    ListResult result;

    // 验证价格
    if (!ValidatePrice(price, buyout_price)) {
        result.success = false;
        result.error_message = "Invalid price";
        return result;
    }

    // 验证持续时间
    if (!ValidateDuration(duration_hours)) {
        result.success = false;
        result.error_message = "Invalid duration";
        return result;
    }

    // 验证数量
    if (quantity == 0 || quantity > 999) {
        result.success = false;
        result.error_message = "Invalid quantity";
        return result;
    }

    // 计算手续费
    result.fee = CalculateListingFee(price);

    // 创建挂单
    MarketListing listing;
    listing.listing_id = GenerateListingId();
    listing.seller_id = seller_id;
    listing.seller_name = seller_name;
    listing.item_instance_id = item_instance_id;
    listing.item_id = item_id;
    listing.item_name = item_name;
    listing.item_type = static_cast<uint32_t>(murim::MarketItemType::MARKET_ITEM);  // TODO: 从物品模板获取
    listing.quantity = quantity;
    listing.price = price;
    listing.buyout_price = buyout_price;
    listing.current_bid = 0;
    listing.bidder_id = 0;
    listing.bidder_name = "";
    listing.status = static_cast<uint32_t>(murim::MarketListingStatus::MARKET_ACTIVE);
    listing.list_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    listing.duration_hours = duration_hours;
    listing.end_time = listing.list_time + (duration_hours * 3600);
    listing.item_level = 1;  // TODO: 从物品模板获取
    listing.quality = 1;     // TODO: 从物品模板获取

    // 保存挂单
    listings_[listing.listing_id] = listing;
    seller_listings_[seller_id].push_back(listing.listing_id);
    SaveListing(listing);

    result.success = true;
    result.listing_id = listing.listing_id;

    spdlog::info("Item listed: seller_id={}, listing_id={}, item_id={}, price={}, buyout={}",
                 seller_id, listing.listing_id, item_id, price, buyout_price);

    return result;
}

bool MarketManager::DelistItem(uint64_t seller_id, uint64_t listing_id) {
    auto it = listings_.find(listing_id);
    if (it == listings_.end()) {
        spdlog::warn("Delist failed: listing not found, listing_id={}", listing_id);
        return false;
    }

    MarketListing& listing = it->second;

    // 验证所有权
    if (listing.seller_id != seller_id) {
        spdlog::warn("Delist failed: not the owner, listing_id={}, seller_id={}", listing_id, seller_id);
        return false;
    }

    // 验证状态
    if (listing.status != static_cast<uint32_t>(murim::MarketListingStatus::MARKET_ACTIVE)) {
        spdlog::warn("Delist failed: listing not active, listing_id={}, status={}", listing_id, listing.status);
        return false;
    }

    // 检查是否已有出价
    if (listing.current_bid > 0) {
        spdlog::warn("Delist failed: listing has bids, listing_id={}, current_bid={}", listing_id, listing.current_bid);
        return false;
    }

    // 返还物品
    if (!SendItemToPlayer(seller_id, listing.item_instance_id, listing.item_id, listing.quantity)) {
        spdlog::error("Delist failed: cannot return item, listing_id={}", listing_id);
        return false;
    }

    // 更新状态
    listing.status = static_cast<uint32_t>(murim::MarketListingStatus::MARKET_CANCELLED);
    DeleteListing(listing_id);

    // 从索引中移除
    auto& seller_list = seller_listings_[seller_id];
    seller_list.erase(std::remove(seller_list.begin(), seller_list.end(), listing_id), seller_list.end());

    spdlog::info("Item delisted: listing_id={}, seller_id={}", listing_id, seller_id);

    return true;
}

BuyResult MarketManager::BuyItem(uint64_t buyer_id, uint64_t listing_id, uint64_t buyout_price) {
    BuyResult result;

    auto it = listings_.find(listing_id);
    if (it == listings_.end()) {
        result.error_message = "Listing not found";
        return result;
    }

    MarketListing& listing = it->second;

    // 验证状态
    if (listing.status != static_cast<uint32_t>(murim::MarketListingStatus::MARKET_ACTIVE)) {
        result.error_message = "Listing not active";
        return result;
    }

    // 验证一口价
    if (listing.buyout_price == 0) {
        result.error_message = "No buyout price";
        return result;
    }

    if (buyout_price != listing.buyout_price) {
        result.error_message = "Invalid buyout price";
        return result;
    }

    // 不能购买自己的物品
    if (listing.seller_id == buyer_id) {
        result.error_message = "Cannot buy own item";
        return result;
    }

    // 执行购买
    result.spent_gold = listing.buyout_price;

    // TODO: 从买家扣除金币
    // TODO: 验证买家有足够金币

    // 计算卖家实际收到的金币（扣除手续费）
    uint64_t seller_gold = CalculateTransactionFee(listing.buyout_price);

    // 发送金币给卖家
    SendGoldToPlayer(listing.seller_id, seller_gold);

    // 发送物品给买家
    SendItemToPlayer(buyer_id, listing.item_instance_id, listing.item_id, listing.quantity);

    // 更新状态
    listing.status = static_cast<uint32_t>(murim::MarketListingStatus::MARKET_SOLD);
    DeleteListing(listing_id);

    // 发送售出通知给卖家
    SendMarketNotification(listing.seller_id, "ITEM_SOLD", std::to_string(listing_id));

    result.success = true;

    spdlog::info("Item bought: listing_id={}, buyer_id={}, seller_id={}, price={}",
                 listing_id, buyer_id, listing.seller_id, listing.buyout_price);

    return result;
}

BidResult MarketManager::BidItem(uint64_t bidder_id, const std::string& bidder_name, uint64_t listing_id, uint64_t bid_price) {
    BidResult result;

    auto it = listings_.find(listing_id);
    if (it == listings_.end()) {
        result.error_message = "Listing not found";
        return result;
    }

    MarketListing& listing = it->second;

    // 验证状态
    if (listing.status != static_cast<uint32_t>(murim::MarketListingStatus::MARKET_ACTIVE)) {
        result.error_message = "Listing not active";
        return result;
    }

    // 不能出价自己的物品
    if (listing.seller_id == bidder_id) {
        result.error_message = "Cannot bid on own item";
        return result;
    }

    // 验证出价（必须高于当前出价）
    if (bid_price <= listing.current_bid) {
        result.error_message = "Bid too low";
        return result;
    }

    // 验证出价（必须高于起拍价）
    if (bid_price < listing.price) {
        result.error_message = "Bid below starting price";
        return result;
    }

    // 最低加价幅度（5%）
    uint64_t min_increment = std::max<uint64_t>(listing.price * 0.05, 1);
    if (listing.current_bid > 0 && bid_price < listing.current_bid + min_increment) {
        result.error_message = "Bid increment too small";
        return result;
    }

    // 如果有之前的出价者，通知他被超越了
    result.outbid = (listing.current_bid > 0);

    // TODO: 从新出价者扣除金币（作为保证金）
    // TODO: 退还之前出价者的金币

    // 更新出价信息
    listing.current_bid = bid_price;
    listing.bidder_id = bidder_id;
    listing.bidder_name = bidder_name;

    result.current_bid = bid_price;
    result.success = true;

    // 如果有之前的出价者，发送被超越通知
    if (result.outbid) {
        // TODO: 找到之前的出价者ID
        // SendMarketNotification(previous_bidder_id, "OUTBID", ...);
    }

    spdlog::info("Bid placed: listing_id={}, bidder_id={}, bid_price={}, previous_bid={}",
                 listing_id, bidder_id, bid_price, listing.current_bid - bid_price);

    return result;
}

// ========== 查询操作 ==========

MarketListing* MarketManager::GetListing(uint64_t listing_id) {
    auto it = listings_.find(listing_id);
    if (it != listings_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<MarketListing> MarketManager::SearchListings(
    const std::string& search_text,
    uint32_t item_type,
    uint32_t min_level,
    uint32_t max_level,
    uint64_t min_price,
    uint64_t max_price,
    uint32_t quality,
    uint32_t sort_type,
    uint32_t page,
    uint32_t page_size) {

    std::vector<MarketListing> results;

    // 收集符合条件的挂单
    for (const auto& [listing_id, listing] : listings_) {
        // 只显示在售中的
        if (listing.status != static_cast<uint32_t>(murim::MarketListingStatus::MARKET_ACTIVE)) {
            continue;
        }

        // 过滤过期的
        if (listing.IsExpired()) {
            continue;
        }

        // 搜索文本过滤
        if (!search_text.empty()) {
            std::string search_lower = search_text;
            std::string name_lower = listing.item_name;
            std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

            if (name_lower.find(search_lower) == std::string::npos) {
                continue;
            }
        }

        // 物品类型过滤
        if (item_type != 0 && listing.item_type != item_type) {
            continue;
        }

        // 等级过滤
        if (listing.item_level < min_level || listing.item_level > max_level) {
            continue;
        }

        // 价格过滤
        if (listing.price < min_price || listing.price > max_price) {
            continue;
        }

        // 品质过滤
        if (quality != 0 && listing.quality != quality) {
            continue;
        }

        results.push_back(listing);
    }

    // 排序
    switch (sort_type) {
        case 0: // PRICE_ASC
            std::sort(results.begin(), results.end(), [](const MarketListing& a, const MarketListing& b) {
                return a.price < b.price;
            });
            break;
        case 1: // PRICE_DESC
            std::sort(results.begin(), results.end(), [](const MarketListing& a, const MarketListing& b) {
                return a.price > b.price;
            });
            break;
        case 2: // TIME_ASC
            std::sort(results.begin(), results.end(), [](const MarketListing& a, const MarketListing& b) {
                return a.end_time < b.end_time;
            });
            break;
        case 3: // TIME_DESC
            std::sort(results.begin(), results.end(), [](const MarketListing& a, const MarketListing& b) {
                return a.end_time > b.end_time;
            });
            break;
        case 4: // LEVEL_ASC
            std::sort(results.begin(), results.end(), [](const MarketListing& a, const MarketListing& b) {
                return a.item_level < b.item_level;
            });
            break;
        case 5: // LEVEL_DESC
            std::sort(results.begin(), results.end(), [](const MarketListing& a, const MarketListing& b) {
                return a.item_level > b.item_level;
            });
            break;
    }

    // 分页
    uint32_t start_index = page * page_size;
    if (start_index >= results.size()) {
        return {};
    }

    uint32_t end_index = std::min(start_index + page_size, static_cast<uint32_t>(results.size()));

    return std::vector<MarketListing>(results.begin() + start_index, results.begin() + end_index);
}

std::vector<MarketListing> MarketManager::GetMyListings(uint64_t seller_id, uint32_t page, uint32_t page_size) {
    std::vector<MarketListing> results;

    auto it = seller_listings_.find(seller_id);
    if (it == seller_listings_.end()) {
        return results;
    }

    for (uint64_t listing_id : it->second) {
        auto lit = listings_.find(listing_id);
        if (lit != listings_.end()) {
            results.push_back(lit->second);
        }
    }

    // 分页
    uint32_t start_index = page * page_size;
    if (start_index >= results.size()) {
        return {};
    }

    uint32_t end_index = std::min(start_index + page_size, static_cast<uint32_t>(results.size()));

    return std::vector<MarketListing>(results.begin() + start_index, results.begin() + end_index);
}

uint32_t MarketManager::GetListingCount(uint64_t seller_id) {
    if (seller_id == 0) {
        return static_cast<uint32_t>(listings_.size());
    }

    auto it = seller_listings_.find(seller_id);
    if (it == seller_listings_.end()) {
        return 0;
    }

    return static_cast<uint32_t>(it->second.size());
}

// ========== 维护操作 ==========

void MarketManager::CleanExpiredListings() {
    spdlog::info("Cleaning expired market listings...");

    uint32_t cleaned_count = 0;

    for (auto it = listings_.begin(); it != listings_.end(); ) {
        MarketListing& listing = it->second;

        if (listing.IsExpired() && listing.status == static_cast<uint32_t>(murim::MarketListingStatus::MARKET_ACTIVE)) {
            // 检查是否有出价
            if (listing.current_bid > 0) {
                // 有出价，通知最高出价者中标
                // TODO: 发送物品给出价者
                // TODO: 发送金币给卖家
                SendItemToPlayer(listing.bidder_id, listing.item_instance_id, listing.item_id, listing.quantity);
                SendGoldToPlayer(listing.seller_id, listing.current_bid);

                listing.status = static_cast<uint32_t>(murim::MarketListingStatus::MARKET_SOLD);
            } else {
                // 无出价，返还物品给卖家
                SendItemToPlayer(listing.seller_id, listing.item_instance_id, listing.item_id, listing.quantity);

                listing.status = static_cast<uint32_t>(murim::MarketListingStatus::MARKET_EXPIRED);
            }

            DeleteListing(listing.listing_id);
            cleaned_count++;
        }

        ++it;
    }

    spdlog::info("Cleaned {} expired listings", cleaned_count);
}

void MarketManager::Update() {
    // 定期清理过期挂单（每小时执行一次）
    static auto last_cleanup = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::minutes>(now - last_cleanup).count() >= 60) {
        CleanExpiredListings();
        last_cleanup = now;
    }
}

// ========== 辅助方法 ==========

uint64_t MarketManager::GenerateListingId() {
    return next_listing_id_++;
}

uint64_t MarketManager::CalculateListingFee(uint64_t price) {
    // 挂单手续费为价格的 1%
    return price / 100;
}

uint64_t MarketManager::CalculateTransactionFee(uint64_t price) {
    // 交易手续费为价格的 5%
    return price * 95 / 100;
}

bool MarketManager::ValidatePrice(uint64_t price, uint64_t buyout_price) {
    // 价格必须大于0
    if (price == 0) {
        return false;
    }

    // 一口价必须大于等于起拍价
    if (buyout_price > 0 && buyout_price < price) {
        return false;
    }

    // 价格上限
    if (price > 1000000000ULL || buyout_price > 1000000000ULL) {
        return false;
    }

    return true;
}

bool MarketManager::ValidateDuration(uint32_t duration_hours) {
    // 只允许 12/24/48/72 小时
    return duration_hours == 12 || duration_hours == 24 || duration_hours == 48 || duration_hours == 72;
}

bool MarketManager::SaveListing(const MarketListing& listing) {
    // TODO: 保存到数据库
    spdlog::debug("Saving listing to database: listing_id={}", listing.listing_id);
    return true;
}

bool MarketManager::DeleteListing(uint64_t listing_id) {
    // TODO: 从数据库删除
    spdlog::debug("Deleting listing from database: listing_id={}", listing_id);
    return true;
}

bool MarketManager::SendItemToPlayer(uint64_t character_id, uint64_t item_instance_id, uint32_t item_id, uint32_t quantity) {
    // 通过邮件系统发送物品
    // TODO: 调用 MailManager 发送系统邮件
    spdlog::info("Sending item to player via mail: character_id={}, item_id={}, quantity={}",
                 character_id, item_id, quantity);
    return true;
}

bool MarketManager::SendGoldToPlayer(uint64_t character_id, uint64_t gold) {
    // 通过邮件系统发送金币
    // TODO: 调用 MailManager 发送系统邮件
    spdlog::info("Sending gold to player via mail: character_id={}, gold={}", character_id, gold);
    return true;
}

void MarketManager::SendMarketNotification(uint64_t character_id, const std::string& notification_type, const std::string& data) {
    // TODO: 发送市场通知给玩家
    spdlog::info("Sending market notification: character_id={}, type={}, data={}",
                 character_id, notification_type, data);
}

// ========== Singleton 构造函数 ==========

MarketManager::MarketManager()
    : next_listing_id_(1) {
}

} // namespace Game
} // namespace Murim
