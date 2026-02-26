#include "TradeManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>

namespace Murim {
namespace Game {

TradeManager& TradeManager::Instance() {
    static TradeManager instance;
    return instance;
}

TradeManager::TradeManager() : next_trade_id_(1) {}

bool TradeManager::Initialize() {
    spdlog::info("Initializing TradeManager...");
    spdlog::info("TradeManager initialized successfully");
    return true;
}

// ========== 交易请求 ==========

uint64_t TradeManager::RequestTrade(uint64_t initiator_id, uint64_t target_id) {
    // 不能与自己交易
    if (initiator_id == target_id) {
        spdlog::warn("Cannot trade with self: initiator_id={}", initiator_id);
        return 0;
    }

    // 检查目标是否已经在交易中
    if (player_trades_.find(target_id) != player_trades_.end()) {
        spdlog::warn("Target already in trade: target_id={}", target_id);
        return 0;
    }

    // 检查距离
    if (!ValidateDistance(initiator_id, target_id)) {
        spdlog::warn("Trade distance too far: initiator_id={}, target_id={}", initiator_id, target_id);
        return 0;
    }

    // 创建交易会话
    TradeSession trade;
    trade.trade_id = GenerateTradeId();
    trade.initiator_id = initiator_id;
    trade.initiator_name = ""; // TODO: 从 CharacterManager 获取
    trade.target_id = target_id;
    trade.target_name = ""; // TODO: 从 CharacterManager 获取
    trade.status = static_cast<uint32_t>(murim::TradeStatus::TRADE_REQUESTED);
    trade.start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    trade.timeout_time = trade.start_time + 120; // 2分钟超时
    trade.initiator_gold = 0;
    trade.target_gold = 0;
    trade.initiator_confirmed = false;
    trade.target_confirmed = false;

    // 保存交易会话
    trade_sessions_[trade.trade_id] = trade;
    player_trades_[initiator_id] = trade.trade_id;

    spdlog::info("Trade requested: trade_id={}, initiator_id={}, target_id={}",
                 trade.trade_id, initiator_id, target_id);

    // TODO: 发送交易请求通知给目标

    return trade.trade_id;
}

bool TradeManager::RespondTrade(uint64_t character_id, uint64_t trade_id, bool accept) {
    auto it = trade_sessions_.find(trade_id);
    if (it == trade_sessions_.end()) {
        spdlog::warn("Trade not found: trade_id={}", trade_id);
        return false;
    }

    TradeSession& trade = it->second;

    // 验证响应者
    if (trade.target_id != character_id) {
        spdlog::warn("Not the target of this trade: character_id={}, trade_id={}", character_id, trade_id);
        return false;
    }

    if (trade.status != static_cast<uint32_t>(murim::TradeStatus::TRADE_REQUESTED)) {
        spdlog::warn("Trade not in requested status: trade_id={}, status={}", trade_id, trade.status);
        return false;
    }

    if (!accept) {
        // 拒绝交易
        trade.status = static_cast<uint32_t>(murim::TradeStatus::TRADE_CANCELLED);
        trade_sessions_.erase(it);
        player_trades_.erase(trade.initiator_id);
        spdlog::info("Trade rejected: trade_id={}", trade_id);
        return false;
    }

    // 接受交易
    trade.status = static_cast<uint32_t>(murim::TradeStatus::TRADE_PENDING);
    player_trades_[trade.target_id] = trade.trade_id;

    spdlog::info("Trade accepted: trade_id={}", trade_id);

    // TODO: 发送交易开始通知给双方

    return true;
}

// ========== 交易操作 ==========

bool TradeManager::AddItem(uint64_t character_id, uint64_t trade_id, const TradeItem& item) {
    auto it = trade_sessions_.find(trade_id);
    if (it == trade_sessions_.end()) {
        return false;
    }

    TradeSession& trade = it->second;

    // 验证交易状态
    if (trade.status != static_cast<uint32_t>(murim::TradeStatus::TRADE_PENDING)) {
        return false;
    }

    // 验证参与者
    bool is_initiator = (trade.initiator_id == character_id);
    bool is_target = (trade.target_id == character_id);

    if (!is_initiator && !is_target) {
        return false;
    }

    // 验证物品
    if (!ValidateTradeItem(item)) {
        return false;
    }

    // 添加到相应的物品列表
    if (is_initiator) {
        trade.initiator_items.push_back(item);
    } else {
        trade.target_items.push_back(item);
    }

    // 重置确认状态
    trade.initiator_confirmed = false;
    trade.target_confirmed = false;

    spdlog::info("Item added to trade: trade_id={}, character_id={}, item_id={}",
                 trade_id, character_id, item.item_id);

    // TODO: 发送物品添加通知给对方
    NotifyTradeUpdate(trade_id);

    return true;
}

bool TradeManager::RemoveItem(uint64_t character_id, uint64_t trade_id, uint64_t item_instance_id) {
    auto it = trade_sessions_.find(trade_id);
    if (it == trade_sessions_.end()) {
        return false;
    }

    TradeSession& trade = it->second;

    // 验证交易状态
    if (trade.status != static_cast<uint32_t>(murim::TradeStatus::TRADE_PENDING)) {
        return false;
    }

    // 确定是发起者还是目标
    bool is_initiator = (trade.initiator_id == character_id);
    auto& items = is_initiator ? trade.initiator_items : trade.target_items;

    // 查找并移除物品
    auto item_it = std::find_if(items.begin(), items.end(),
        [item_instance_id](const TradeItem& item) {
            return item.item_instance_id == item_instance_id;
        });

    if (item_it == items.end()) {
        return false;
    }

    items.erase(item_it);

    // 重置确认状态
    trade.initiator_confirmed = false;
    trade.target_confirmed = false;

    spdlog::info("Item removed from trade: trade_id={}, character_id={}, item_instance_id={}",
                 trade_id, character_id, item_instance_id);

    // TODO: 发送物品移除通知给对方
    NotifyTradeUpdate(trade_id);

    return true;
}

bool TradeManager::AddGold(uint64_t character_id, uint64_t trade_id, uint64_t gold) {
    auto it = trade_sessions_.find(trade_id);
    if (it == trade_sessions_.end()) {
        return false;
    }

    TradeSession& trade = it->second;

    // 验证交易状态
    if (trade.status != static_cast<uint32_t>(murim::TradeStatus::TRADE_PENDING)) {
        return false;
    }

    // 验证参与者
    bool is_initiator = (trade.initiator_id == character_id);

    if (!is_initiator && trade.target_id != character_id) {
        return false;
    }

    // TODO: 验证玩家有足够金币

    // 添加金币
    if (is_initiator) {
        trade.initiator_gold = gold;
    } else {
        trade.target_gold = gold;
    }

    // 重置确认状态
    trade.initiator_confirmed = false;
    trade.target_confirmed = false;

    spdlog::info("Gold added to trade: trade_id={}, character_id={}, gold={}",
                 trade_id, character_id, gold);

    // TODO: 发送金币添加通知给对方
    NotifyTradeUpdate(trade_id);

    return true;
}

bool TradeManager::RemoveGold(uint64_t character_id, uint64_t trade_id) {
    auto it = trade_sessions_.find(trade_id);
    if (it == trade_sessions_.end()) {
        return false;
    }

    TradeSession& trade = it->second;

    // 验证交易状态
    if (trade.status != static_cast<uint32_t>(murim::TradeStatus::TRADE_PENDING)) {
        return false;
    }

    // 确定是发起者还是目标
    bool is_initiator = (trade.initiator_id == character_id);

    if (!is_initiator && trade.target_id != character_id) {
        return false;
    }

    // 移除金币
    if (is_initiator) {
        trade.initiator_gold = 0;
    } else {
        trade.target_gold = 0;
    }

    // 重置确认状态
    trade.initiator_confirmed = false;
    trade.target_confirmed = false;

    spdlog::info("Gold removed from trade: trade_id={}, character_id={}", trade_id, character_id);

    // TODO: 发送金币移除通知给对方
    NotifyTradeUpdate(trade_id);

    return true;
}

bool TradeManager::ConfirmTrade(uint64_t character_id, uint64_t trade_id) {
    auto it = trade_sessions_.find(trade_id);
    if (it == trade_sessions_.end()) {
        return false;
    }

    TradeSession& trade = it->second;

    // 验证交易状态
    if (trade.status != static_cast<uint32_t>(murim::TradeStatus::TRADE_PENDING)) {
        return false;
    }

    // 设置确认状态
    if (trade.initiator_id == character_id) {
        trade.initiator_confirmed = true;
    } else if (trade.target_id == character_id) {
        trade.target_confirmed = true;
    } else {
        return false;
    }

    spdlog::info("Trade confirmed: trade_id={}, character_id={}, both_confirmed={}",
                 trade_id, character_id, trade.BothConfirmed());

    // TODO: 发送确认通知给对方
    NotifyTradeUpdate(trade_id);

    // 如果双方都已确认，执行交易
    if (trade.BothConfirmed()) {
        ExecuteTrade(trade);
    }

    return true;
}

bool TradeManager::CancelTrade(uint64_t character_id, uint64_t trade_id, uint32_t reason) {
    auto it = trade_sessions_.find(trade_id);
    if (it == trade_sessions_.end()) {
        return false;
    }

    TradeSession& trade = it->second;

    // 验证参与者
    if (trade.initiator_id != character_id && trade.target_id != character_id) {
        return false;
    }

    // 检查是否已完成
    if (trade.status == static_cast<uint32_t>(murim::TradeStatus::TRADE_COMPLETED)) {
        return false;
    }

    // 返还物品和金币
    // TODO: 发送物品给发起者
    // TODO: 发送物品给目标
    // TODO: 返还金币

    // 更新状态
    trade.status = static_cast<uint32_t>(murim::TradeStatus::TRADE_CANCELLED);

    // 清理
    player_trades_.erase(trade.initiator_id);
    player_trades_.erase(trade.target_id);
    trade_sessions_.erase(it);

    spdlog::info("Trade cancelled: trade_id={}, character_id={}, reason={}",
                 trade_id, character_id, reason);

    // TODO: 发送取消通知给双方
    NotifyTradeUpdate(trade_id);

    return true;
}

// ========== 查询操作 ==========

TradeSession* TradeManager::GetTradeSession(uint64_t trade_id) {
    auto it = trade_sessions_.find(trade_id);
    if (it != trade_sessions_.end()) {
        return &it->second;
    }
    return nullptr;
}

TradeSession* TradeManager::GetPlayerTradeSession(uint64_t character_id) {
    auto it = player_trades_.find(character_id);
    if (it != player_trades_.end()) {
        return GetTradeSession(it->second);
    }
    return nullptr;
}

// ========== 维护操作 ==========

void TradeManager::Update() {
    // 定期检查超时交易（每10秒）
    static auto last_check = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_check).count() >= 10) {
        CancelTimeoutTrades();
        last_check = now;
    }
}

void TradeManager::CancelTimeoutTrades() {
    spdlog::debug("Checking for timeout trades...");

    std::vector<uint64_t> timeout_trades;

    for (const auto& [trade_id, trade] : trade_sessions_) {
        if (trade.IsExpired() && trade.status == static_cast<uint32_t>(murim::TradeStatus::TRADE_PENDING)) {
            timeout_trades.push_back(trade_id);
        }
    }

    for (uint64_t trade_id : timeout_trades) {
        uint32_t reason = static_cast<uint32_t>(murim::TradeCancelReason::TRADE_TIME_EXPIRED);
        CancelTrade(0, trade_id, reason);  // 0 表示系统取消
    }

    if (!timeout_trades.empty()) {
        spdlog::info("Cancelled {} timeout trades", timeout_trades.size());
    }
}

// ========== 辅助方法 ==========

uint64_t TradeManager::GenerateTradeId() {
    return next_trade_id_++;
}

bool TradeManager::ValidateTradeItem(const TradeItem& item) {
    // 验证数量
    if (item.quantity == 0 || item.quantity > 999) {
        return false;
    }

    // TODO: 更多验证
    return true;
}

bool TradeManager::ValidateDistance(uint64_t char1_id, uint64_t char2_id) {
    // TODO: 检查两个角色之间的距离
    // 交易必须在一定范围内（例如5米）
    return true;  // 暂时返回true
}

void TradeManager::ExecuteTrade(TradeSession& trade) {
    spdlog::info("Executing trade: trade_id={}", trade.trade_id);

    // TODO: 从发起者移除物品和金币
    // TODO: 从目标移除物品和金币

    // TODO: 发送物品给目标
    // TODO: 发送金币给发起者

    // TODO: 发送物品给发起者
    // TODO: 发送金币给目标

    trade.status = static_cast<uint32_t>(murim::TradeStatus::TRADE_COMPLETED);

    // 清理
    player_trades_.erase(trade.initiator_id);
    player_trades_.erase(trade.target_id);

    // TODO: 发送交易完成通知给双方
    NotifyTradeUpdate(trade.trade_id);

    spdlog::info("Trade completed: trade_id={}", trade.trade_id);
}

void TradeManager::NotifyTradeUpdate(uint64_t trade_id) {
    // TODO: 发送交易更新通知给双方玩家
    spdlog::debug("Notifying trade update: trade_id={}", trade_id);
}

void TradeManager::NotifyPlayer(uint64_t character_id, const std::string& message) {
    // TODO: 发送通知给玩家
    spdlog::info("Notifying player: character_id={}, message={}", character_id, message);
}

} // namespace Game
} // namespace Murim
