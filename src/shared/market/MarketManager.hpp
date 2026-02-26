#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <chrono>
#include "protocol/market.pb.h"

namespace Murim {
namespace Game {

/**
 * @brief 市场挂单信息
 */
struct MarketListing {
    uint64_t listing_id;              // 挂单ID
    uint64_t seller_id;               // 卖家ID
    std::string seller_name;          // 卖家名称
    uint64_t item_instance_id;        // 物品实例ID
    uint32_t item_id;                 // 物品模板ID
    std::string item_name;            // 物品名称
    uint32_t item_type;               // 物品类型
    uint32_t quantity;                // 数量
    uint64_t price;                   // 起拍价
    uint64_t buyout_price;            // 一口价（0表示无）
    uint64_t current_bid;             // 当前最高出价
    uint64_t bidder_id;               // 当前最高出价者ID
    std::string bidder_name;          // 当前最高出价者名称
    uint32_t status;                  // 状态（MarketListingStatus）
    uint64_t list_time;               // 上架时间
    uint32_t duration_hours;          // 持续时间（小时）
    uint64_t end_time;                // 结束时间
    uint32_t item_level;              // 物品等级
    uint32_t quality;                 // 品质

    /**
     * @brief 检查是否已过期
     */
    bool IsExpired() const {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        return now >= static_cast<time_t>(end_time);
    }

    /**
     * @brief 检查是否可购买（一口价）
     */
    bool CanBuyout() const {
        return buyout_price > 0 && status == static_cast<uint32_t>(murim::MarketListingStatus::MARKET_ACTIVE);
    }

    /**
     * @brief 检查是否可出价（拍卖）
     */
    bool CanBid() const {
        return status == static_cast<uint32_t>(murim::MarketListingStatus::MARKET_ACTIVE) && IsExpired() == false;
    }
};

/**
 * @brief 上架结果
 */
struct ListResult {
    bool success = false;
    uint64_t listing_id = 0;
    uint64_t fee = 0;
    std::string error_message;
};

/**
 * @brief 购买结果
 */
struct BuyResult {
    bool success = false;
    uint64_t spent_gold = 0;
    std::string error_message;
};

/**
 * @brief 出价结果
 */
struct BidResult {
    bool success = false;
    uint64_t current_bid = 0;
    bool outbid = false;  // 是否被超越
    std::string error_message;
};

/**
 * @brief 市场管理器
 *
 * 负责拍卖行/市场系统的核心逻辑
 */
class MarketManager {
public:
    /**
     * @brief 获取单例实例
     */
    static MarketManager& Instance();

    /**
     * @brief 初始化市场系统
     * @return 是否成功
     */
    bool Initialize();

    // ========== 挂单操作 ==========

    /**
     * @brief 上架物品
     * @param seller_id 卖家ID
     * @param seller_name 卖家名称
     * @param item_instance_id 物品实例ID
     * @param item_id 物品模板ID
     * @param item_name 物品名称
     * @param quantity 数量
     * @param price 起拍价
     * @param buyout_price 一口价
     * @param duration_hours 持续时间
     * @return 上架结果
     */
    ListResult ListItem(
        uint64_t seller_id,
        const std::string& seller_name,
        uint64_t item_instance_id,
        uint32_t item_id,
        const std::string& item_name,
        uint32_t quantity,
        uint64_t price,
        uint64_t buyout_price,
        uint32_t duration_hours
    );

    /**
     * @brief 下架物品
     * @param seller_id 卖家ID
     * @param listing_id 挂单ID
     * @return 是否成功
     */
    bool DelistItem(uint64_t seller_id, uint64_t listing_id);

    /**
     * @brief 购买物品（一口价）
     * @param buyer_id 买家ID
     * @param listing_id 挂单ID
     * @param buyout_price 一口价
     * @return 购买结果
     */
    BuyResult BuyItem(uint64_t buyer_id, uint64_t listing_id, uint64_t buyout_price);

    /**
     * @brief 出价（拍卖）
     * @param bidder_id 出价者ID
     * @param bidder_name 出价者名称
     * @param listing_id 挂单ID
     * @param bid_price 出价
     * @return 出价结果
     */
    BidResult BidItem(uint64_t bidder_id, const std::string& bidder_name, uint64_t listing_id, uint64_t bid_price);

    // ========== 查询操作 ==========

    /**
     * @brief 获取挂单详情
     * @param listing_id 挂单ID
     * @return 挂单信息（如果存在）
     */
    MarketListing* GetListing(uint64_t listing_id);

    /**
     * @brief 搜索市场物品
     * @param search_text 搜索关键词
     * @param item_type 物品类型过滤
     * @param min_level 最低等级
     * @param max_level 最高等级
     * @param min_price 最低价格
     * @param max_price 最高价格
     * @param quality 品质过滤
     * @param sort_type 排序方式
     * @param page 页码
     * @param page_size 每页数量
     * @return 挂单列表
     */
    std::vector<MarketListing> SearchListings(
        const std::string& search_text,
        uint32_t item_type,
        uint32_t min_level,
        uint32_t max_level,
        uint64_t min_price,
        uint64_t max_price,
        uint32_t quality,
        uint32_t sort_type,
        uint32_t page,
        uint32_t page_size
    );

    /**
     * @brief 获取我的挂单
     * @param seller_id 卖家ID
     * @param page 页码
     * @param page_size 每页数量
     * @return 挂单列表
     */
    std::vector<MarketListing> GetMyListings(uint64_t seller_id, uint32_t page, uint32_t page_size);

    /**
     * @brief 获取挂单数量
     * @param seller_id 卖家ID（0表示全部）
     * @return 挂单数量
     */
    uint32_t GetListingCount(uint64_t seller_id = 0);

    // ========== 维护操作 ==========

    /**
     * @brief 清理过期挂单
     */
    void CleanExpiredListings();

    /**
     * @brief 更新市场状态（定期调用）
     */
    void Update();

private:
    MarketManager();
    ~MarketManager() = default;
    MarketManager(const MarketManager&) = delete;
    MarketManager& operator=(const MarketManager&) = delete;

    // ========== 数据存储 ==========

    // 挂单映射 (listing_id -> MarketListing)
    std::unordered_map<uint64_t, MarketListing> listings_;

    // 卖家挂单索引 (seller_id -> vector<listing_id>)
    std::unordered_map<uint64_t, std::vector<uint64_t>> seller_listings_;

    // 挂单ID生成器
    uint64_t next_listing_id_ = 1;

    // ========== 辅助方法 ==========

    /**
     * @brief 生成挂单ID
     */
    uint64_t GenerateListingId();

    /**
     * @brief 计算挂单手续费
     * @param price 价格
     * @return 手续费
     */
    uint64_t CalculateListingFee(uint64_t price);

    /**
     * @brief 计算交易手续费
     * @param price 价格
     * @return 手续费
     */
    uint64_t CalculateTransactionFee(uint64_t price);

    /**
     * @brief 验证挂单价格
     * @param price 价格
     * @param buyout_price 一口价
     * @return 是否有效
     */
    bool ValidatePrice(uint64_t price, uint64_t buyout_price);

    /**
     * @brief 验证持续时间
     * @param duration_hours 持续时间
     * @return 是否有效
     */
    bool ValidateDuration(uint32_t duration_hours);

    /**
     * @brief 保存挂单到数据库
     */
    bool SaveListing(const MarketListing& listing);

    /**
     * @brief 从数据库删除挂单
     */
    bool DeleteListing(uint64_t listing_id);

    /**
     * @brief 发送物品到玩家（通过邮件）
     */
    bool SendItemToPlayer(uint64_t character_id, uint64_t item_instance_id, uint32_t item_id, uint32_t quantity);

    /**
     * @brief 发送金币到玩家（通过邮件）
     */
    bool SendGoldToPlayer(uint64_t character_id, uint64_t gold);

    /**
     * @brief 发送市场通知
     */
    void SendMarketNotification(uint64_t character_id, const std::string& notification_type, const std::string& data);
};

} // namespace Game
} // namespace Murim
