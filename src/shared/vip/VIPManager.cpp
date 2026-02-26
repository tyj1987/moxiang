#include "VIPManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <chrono>
#include <algorithm>

namespace Murim {
namespace Game {

// ========== VIPLevelInfo ==========

murim::VIPLevelInfo VIPLevelInfo::ToProto() const {
    murim::VIPLevelInfo proto;
    proto.set_vip_level(vip_level);
    proto.set_required_exp(required_exp);
    proto.set_description(description);

    for (uint32_t privilege : privileges) {
        proto.add_privileges(privilege);
    }

    for (uint32_t item_id : gift_item_ids) {
        proto.add_gift_item_ids(item_id);
    }

    for (uint32_t count : gift_item_counts) {
        proto.add_gift_item_counts(count);
    }

    return proto;
}

// ========== VIPPlayerData ==========

murim::VIPPlayerData VIPPlayerData::ToProto() const {
    murim::VIPPlayerData proto;
    proto.set_vip_level(vip_level);
    proto.set_current_exp(current_exp);
    proto.set_total_exp(total_exp);
    proto.set_monthly_recharge(monthly_recharge);
    proto.set_total_recharge(total_recharge);

    for (bool claimed : level_gift_claimed) {
        proto.add_level_gift_claimed(claimed);
    }

    proto.set_last_daily_gift_day(last_daily_gift_day);
    proto.set_last_monthly_gift_month(last_monthly_gift_month);

    return proto;
}

// ========== VIPManager ==========

VIPManager& VIPManager::Instance() {
    static VIPManager instance;
    return instance;
}

VIPManager::VIPManager()
    : max_vip_level_(10)
    , exp_per_yuan_(100)
    , daily_gift_item_id_(0)
    , daily_gift_item_count_(0) {
}

bool VIPManager::Initialize() {
    spdlog::info("Initializing VIPManager...");

    // TODO: 从数据库或配置文件加载VIP等级定义

    // 示例：注册VIP等级
    for (uint32_t level = 0; level <= max_vip_level_; level++) {
        VIPLevelInfo level_info;
        level_info.vip_level = level;
        level_info.required_exp = static_cast<uint64_t>(1000 * std::pow(level + 1, 2));

        // VIP 0 特权
        if (level >= 0) {
            // 基础玩家，无特殊特权
        }

        // VIP 1 特权
        if (level >= 1) {
            level_info.privileges.push_back(murim::VIPPrivilegeType::EXTRA_INVENTORY);
            level_info.gift_item_ids.push_back(1001);
            level_info.gift_item_counts.push_back(10);
        }

        // VIP 2 特权
        if (level >= 2) {
            level_info.privileges.push_back(murim::VIPPrivilegeType::EXTRA_WAREHOUSE);
            level_info.gift_item_ids.push_back(1002);
            level_info.gift_item_counts.push_back(5);
        }

        // VIP 3 特权
        if (level >= 3) {
            level_info.privileges.push_back(murim::VIPPrivilegeType::FAST_TRAVEL);
            level_info.privileges.push_back(murim::VIPPrivilegeType::EXP_BONUS);
            level_info.gift_item_ids.push_back(1003);
            level_info.gift_item_counts.push_back(3);
        }

        // VIP 4 特权
        if (level >= 4) {
            level_info.privileges.push_back(murim::VIPPrivilegeType::AUTO_COMBAT);
            level_info.privileges.push_back(murim::VIPPrivilegeType::GOLD_BONUS);
        }

        // VIP 5 特权
        if (level >= 5) {
            level_info.privileges.push_back(murim::VIPPrivilegeType::BAZAAR_DISCOUNT);
            level_info.gift_item_ids.push_back(2001);
            level_info.gift_item_counts.push_back(1);
        }

        // VIP 6 特权
        if (level >= 6) {
            level_info.privileges.push_back(murim::VIPPrivilegeType::PRIORITY_QUEUE);
            level_info.privileges.push_back(murim::VIPPrivilegeType::DAILY_GIFT);
        }

        // VIP 7 特权
        if (level >= 7) {
            level_info.privileges.push_back(murim::VIPPrivilegeType::REVIVE_FREE);
            level_info.privileges.push_back(murim::VIPPrivilegeType::DUNGEON_ENTRIES);
        }

        // VIP 8 特权
        if (level >= 8) {
            level_info.privileges.push_back(murim::VIPPrivilegeType::SPECIAL_MOUNT);
            level_info.gift_item_ids.push_back(4001);
            level_info.gift_item_counts.push_back(1);
        }

        // VIP 9 特权
        if (level >= 9) {
            level_info.privileges.push_back(murim::VIPPrivilegeType::SPECIAL_TITLE);
            level_info.gift_item_ids.push_back(3001);
            level_info.gift_item_counts.push_back(1);
        }

        // VIP 10 特权
        if (level >= 10) {
            level_info.privileges.push_back(murim::VIPPrivilegeType::MONTHLY_GIFT);
            level_info.gift_item_ids.push_back(9001);
            level_info.gift_item_counts.push_back(1);
        }

        level_info.description = "VIP等级 " + std::to_string(level);

        RegisterVIPLevel(level_info);
    }

    spdlog::info("VIPManager initialized successfully");
    return true;
}

// ========== VIP等级管理 ==========

bool VIPManager::RegisterVIPLevel(const VIPLevelInfo& level_info) {
    if (vip_levels_.find(level_info.vip_level) != vip_levels_.end()) {
        spdlog::warn("VIP level already registered: {}", level_info.vip_level);
        return false;
    }

    vip_levels_[level_info.vip_level] = level_info;
    spdlog::debug("Registered VIP level: {}", level_info.vip_level);
    return true;
}

VIPLevelInfo* VIPManager::GetVIPLevelInfo(uint32_t vip_level) {
    auto it = vip_levels_.find(vip_level);
    if (it != vip_levels_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<VIPLevelInfo> VIPManager::GetAllVIPLevels() {
    std::vector<VIPLevelInfo> result;
    for (const auto& [level, info] : vip_levels_) {
        result.push_back(info);
    }

    // 按 vip_level 排序
    std::sort(result.begin(), result.end(),
        [](const VIPLevelInfo& a, const VIPLevelInfo& b) {
            return a.vip_level < b.vip_level;
        });

    return result;
}

// ========== 玩家VIP数据管理 ==========

VIPPlayerData* VIPManager::GetPlayerVIPData(uint64_t character_id) {
    auto it = player_vip_data_.find(character_id);
    if (it != player_vip_data_.end()) {
        return &it->second;
    }

    // 创建新数据
    VIPPlayerData new_data;
    new_data.vip_level = 0;
    new_data.current_exp = 0;
    new_data.total_exp = 0;
    new_data.monthly_recharge = 0;
    new_data.total_recharge = 0;

    // 初始化礼包领取状态
    new_data.level_gift_claimed.resize(max_vip_level_ + 1, false);
    new_data.last_daily_gift_day = 0;
    new_data.last_monthly_gift_month = 0;

    player_vip_data_[character_id] = new_data;
    return &player_vip_data_[character_id];
}

bool VIPManager::AddExp(uint64_t character_id, uint64_t exp_amount) {
    auto* data = GetPlayerVIPData(character_id);
    if (!data) {
        return false;
    }

    uint32_t old_level = data->vip_level;
    data->current_exp += exp_amount;
    data->total_exp += exp_amount;

    // 检查是否升级
    while (data->vip_level < max_vip_level_) {
        auto* next_level_info = GetVIPLevelInfo(data->vip_level + 1);
        if (!next_level_info || data->current_exp < next_level_info->required_exp) {
            break;
        }

        data->vip_level++;
        spdlog::info("VIP leveled up: character_id={}, level={}", character_id, data->vip_level);
    }

    if (data->vip_level > old_level) {
        // 发送升级通知
        NotifyVIPLevelUp(character_id, old_level, data->vip_level);
    }

    return true;
}

bool VIPManager::Recharge(uint64_t character_id, uint32_t product_id, uint32_t amount) {
    auto* data = GetPlayerVIPData(character_id);
    if (!data) {
        return false;
    }

    // 增加充值金额
    data->monthly_recharge += amount;
    data->total_recharge += amount;

    // 增加VIP经验（1元 = exp_per_yuan_经验）
    uint64_t exp_gain = amount * exp_per_yuan_;
    bool success = AddExp(character_id, exp_gain);

    if (success) {
        spdlog::info("Recharge successful: character_id={}, amount={}, exp_gain={}",
                     character_id, amount, exp_gain);
    }

    return success;
}

// ========== VIP特权 ==========

bool VIPManager::HasPrivilege(uint64_t character_id, murim::VIPPrivilegeType privilege) {
    auto* data = GetPlayerVIPData(character_id);
    if (!data) {
        return false;
    }

    auto* level_info = GetVIPLevelInfo(data->vip_level);
    if (!level_info) {
        return false;
    }

    // 检查特权列表
    for (uint32_t priv : level_info->privileges) {
        if (priv == static_cast<uint32_t>(privilege)) {
            return true;
        }
    }

    return false;
}

std::vector<uint32_t> VIPManager::GetPrivileges(uint64_t character_id) {
    auto* data = GetPlayerVIPData(character_id);
    if (!data) {
        return {};
    }

    auto* level_info = GetVIPLevelInfo(data->vip_level);
    if (!level_info) {
        return {};
    }

    return level_info->privileges;
}

// ========== VIP礼包 ==========

bool VIPManager::ClaimLevelGift(uint64_t character_id, uint32_t vip_level, std::vector<uint32_t>& item_ids, std::vector<uint32_t>& item_counts) {
    auto* data = GetPlayerVIPData(character_id);
    if (!data) {
        return false;
    }

    // 检查是否可以领取
    if (!CanClaimLevelGift(character_id, vip_level)) {
        spdlog::warn("Cannot claim level gift: character_id={}, vip_level={}", character_id, vip_level);
        return false;
    }

    auto* level_info = GetVIPLevelInfo(vip_level);
    if (!level_info) {
        return false;
    }

    // 标记为已领取
    data->level_gift_claimed[vip_level] = true;

    // 返回物品列表
    item_ids = level_info->gift_item_ids;
    item_counts = level_info->gift_item_counts;

    spdlog::info("VIP level gift claimed: character_id={}, vip_level={}", character_id, vip_level);

    // TODO: 将物品添加到玩家背包
    // ItemManager::Instance().AddItems(character_id, item_ids, item_counts);

    return true;
}

bool VIPManager::ClaimDailyGift(uint64_t character_id, std::vector<uint32_t>& item_ids, std::vector<uint32_t>& item_counts) {
    auto* data = GetPlayerVIPData(character_id);
    if (!data) {
        return false;
    }

    // 检查是否可以领取
    if (!CanClaimDailyGift(character_id)) {
        spdlog::warn("Cannot claim daily gift: character_id={}", character_id);
        return false;
    }

    // 获取当前日期
    auto now = std::chrono::system_clock::now();
    auto time_now = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&time_now);
    uint32_t current_day = tm_now->tm_yday;

    // 更新领取状态
    data->last_daily_gift_day = current_day;

    // 每日礼包物品（根据VIP等级）
    item_ids.clear();
    item_counts.clear();

    if (daily_gift_item_id_ != 0) {
        item_ids.push_back(daily_gift_item_id_);
        item_counts.push_back(daily_gift_item_count_);
    }

    // VIP等级加成
    uint32_t bonus_item_count = data->vip_level;
    item_counts[0] += bonus_item_count;

    spdlog::info("VIP daily gift claimed: character_id={}, day={}", character_id, current_day);

    // TODO: 将物品添加到玩家背包
    return true;
}

bool VIPManager::ClaimMonthlyGift(uint64_t character_id, std::vector<uint32_t>& item_ids, std::vector<uint32_t>& item_counts) {
    auto* data = GetPlayerVIPData(character_id);
    if (!data) {
        return false;
    }

    // 检查是否可以领取
    if (!CanClaimMonthlyGift(character_id)) {
        spdlog::warn("Cannot claim monthly gift: character_id={}", character_id);
        return false;
    }

    // 获取当前月份
    auto now = std::chrono::system_clock::now();
    auto time_now = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&time_now);
    uint32_t current_month = tm_now->tm_mon;

    // 更新领取状态
    data->last_monthly_gift_month = current_month;

    // 每月礼包物品（固定）
    item_ids = {9001, 9002, 9003};
    item_counts = {1, 1, 1};

    spdlog::info("VIP monthly gift claimed: character_id={}, month={}", character_id, current_month);

    // TODO: 将物品添加到玩家背包
    return true;
}

// ========== 辅助方法 ==========

bool VIPManager::CanClaimLevelGift(uint64_t character_id, uint32_t vip_level) {
    auto* data = GetPlayerVIPData(character_id);
    if (!data) {
        return false;
    }

    // 检查VIP等级是否足够
    if (data->vip_level < vip_level) {
        return false;
    }

    // 检查是否已领取
    if (vip_level >= data->level_gift_claimed.size()) {
        return false;
    }

    if (data->level_gift_claimed[vip_level]) {
        return false;  // 已领取
    }

    return true;
}

bool VIPManager::CanClaimDailyGift(uint64_t character_id) {
    auto* data = GetPlayerVIPData(character_id);
    if (!data) {
        return false;
    }

    // 获取当前日期
    auto now = std::chrono::system_clock::now();
    auto time_now = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&time_now);
    uint32_t current_day = tm_now->tm_yday;

    // 检查是否已领取
    return data->last_daily_gift_day != current_day;
}

bool VIPManager::CanClaimMonthlyGift(uint64_t character_id) {
    auto* data = GetPlayerVIPData(character_id);
    if (!data) {
        return false;
    }

    // 获取当前月份
    auto now = std::chrono::system_clock::now();
    auto time_now = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&time_now);
    uint32_t current_month = tm_now->tm_mon;

    // 检查是否已领取
    return data->last_monthly_gift_month != current_month;
}

void VIPManager::NotifyVIPLevelUp(uint64_t character_id, uint32_t old_level, uint32_t new_level) {
    // TODO: 发送VIP升级通知给客户端

    // 获取新解锁的特权
    auto* new_level_info = GetVIPLevelInfo(new_level);
    if (!new_level_info) {
        return;
    }

    auto* old_level_info = GetVIPLevelInfo(old_level);
    std::vector<uint32_t> old_privileges;
    if (old_level_info) {
        old_privileges = old_level_info->privileges;
    }

    // 找出新增的特权
    std::vector<uint32_t> new_privileges;
    for (uint32_t priv : new_level_info->privileges) {
        bool found = false;
        for (uint32_t old_priv : old_privileges) {
            if (priv == old_priv) {
                found = true;
                break;
            }
        }
        if (!found) {
            new_privileges.push_back(priv);
        }
    }

    spdlog::info("Notification: VIP leveled up - character_id={}, {} -> {}, new_privileges={}",
                character_id, old_level, new_level, new_privileges.size());
}

} // namespace Game
} // namespace Murim
