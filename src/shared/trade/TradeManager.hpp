#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <chrono>
#include "protocol/trade.pb.h"

namespace Murim {
namespace Game {

/**
 * @brief 交易物品信息
 */
struct TradeItem {
    uint64_t item_instance_id;
    uint32_t item_id;
    std::string item_name;
    uint32_t quantity;
    uint32_t quality;
    uint32_t item_level;
};

/**
 * @brief 交易会话
 */
struct TradeSession {
    uint64_t trade_id;
    uint64_t initiator_id;
    std::string initiator_name;
    uint64_t target_id;
    std::string target_name;
    uint32_t status;
    uint64_t start_time;
    uint64_t timeout_time;

    // 发起者的交易物品
    std::vector<TradeItem> initiator_items;
    uint64_t initiator_gold;

    // 目标的交易物品
    std::vector<TradeItem> target_items;
    uint64_t target_gold;

    // 确认状态
    bool initiator_confirmed;
    bool target_confirmed;

    /**
     * @brief 检查是否已过期
     */
    bool IsExpired() const {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        return now >= static_cast<time_t>(timeout_time);
    }

    /**
     * @brief 检查双方是否都已确认
     */
    bool BothConfirmed() const {
        return initiator_confirmed && target_confirmed;
    }
};

/**
 * @brief 交易管理器
 *
 * 负责玩家间直接交易的核心逻辑
 */
class TradeManager {
public:
    static TradeManager& Instance();

    bool Initialize();

    // ========== 交易请求 ==========

    /**
     * @brief 发起交易请求
     */
    uint64_t RequestTrade(uint64_t initiator_id, uint64_t target_id);

    /**
     * @brief 响应交易请求
     */
    bool RespondTrade(uint64_t character_id, uint64_t trade_id, bool accept);

    // ========== 交易操作 ==========

    /**
     * @brief 添加物品到交易
     */
    bool AddItem(uint64_t character_id, uint64_t trade_id, const TradeItem& item);

    /**
     * @brief 移除交易物品
     */
    bool RemoveItem(uint64_t character_id, uint64_t trade_id, uint64_t item_instance_id);

    /**
     * @brief 添加金币到交易
     */
    bool AddGold(uint64_t character_id, uint64_t trade_id, uint64_t gold);

    /**
     * @brief 移除交易金币
     */
    bool RemoveGold(uint64_t character_id, uint64_t trade_id);

    /**
     * @brief 确认交易
     */
    bool ConfirmTrade(uint64_t character_id, uint64_t trade_id);

    /**
     * @brief 取消交易
     */
    bool CancelTrade(uint64_t character_id, uint64_t trade_id, uint32_t reason);

    // ========== 查询操作 ==========

    /**
     * @brief 获取交易会话
     */
    TradeSession* GetTradeSession(uint64_t trade_id);

    /**
     * @brief 获取玩家的交易会话
     */
    TradeSession* GetPlayerTradeSession(uint64_t character_id);

    // ========== 维护操作 ==========

    /**
     * @brief 更新交易状态（定期调用）
     */
    void Update();

    /**
     * @brief 取消所有超时的交易
     */
    void CancelTimeoutTrades();

private:
    TradeManager();
    ~TradeManager() = default;
    TradeManager(const TradeManager&) = delete;
    TradeManager& operator=(const TradeManager&) = delete;

    // 交易会话 (trade_id -> TradeSession)
    std::unordered_map<uint64_t, TradeSession> trade_sessions_;

    // 玩家到交易的映射 (character_id -> trade_id)
    std::unordered_map<uint64_t, uint64_t> player_trades_;

    // 交易ID生成器
    uint64_t next_trade_id_ = 1;

    // ========== 辅助方法 ==========

    uint64_t GenerateTradeId();
    bool ValidateTradeItem(const TradeItem& item);
    bool ValidateDistance(uint64_t char1_id, uint64_t char2_id);
    void ExecuteTrade(TradeSession& trade);
    void NotifyTradeUpdate(uint64_t trade_id);
    void NotifyPlayer(uint64_t character_id, const std::string& message);
};

} // namespace Game
} // namespace Murim
