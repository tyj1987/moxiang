#include "MarriageManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include "core/database/DatabasePool.hpp"  // File is DatabasePool.hpp, class is ConnectionPool
#include <chrono>
#include <ctime>
#include <libpq-fe.h>

namespace Murim {
namespace Game {

// ========== 单例实现 ==========

MarriageManager& MarriageManager::Instance() {
    static MarriageManager instance;
    return instance;
}

// ========== 初始化 ==========

bool MarriageManager::Initialize(std::shared_ptr<Core::Database::ConnectionPool> db_pool) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_pool) {
        db_pool_ = db_pool;
        spdlog::info("[MarriageManager] Database pool set");
    }

    next_proposal_id_ = 1;
    next_marriage_id_ = 1;
    tick_counter_ = 0;

    spdlog::info("[MarriageManager] Initialized");
    return true;
}

void MarriageManager::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    spdlog::info("[MarriageManager] Shutdown complete");
}

// ========== 求婚系统 ==========

bool MarriageManager::SendProposal(uint64_t proposer_id, uint64_t target_id, uint32_t ring_item_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否已结婚
    if (IsMarried(proposer_id) || IsMarried(target_id)) {
        spdlog::warn("[MarriageManager] Character {} or {} already married", proposer_id, target_id);
        return false;
    }

    // 检查戒指道具（TODO: 需要ItemManager集成）

    // 创建求婚
    MarriageProposal proposal;
    proposal.proposal_id = next_proposal_id_++;
    proposal.proposer_id = proposer_id;
    proposal.target_id = target_id;
    proposal.proposal_time = static_cast<uint32_t>(std::time(nullptr));
    proposal.ring_item_id = ring_item_id;
    proposal.message = message;
    proposal.accepted = false;

    proposals_[proposal.proposal_id] = proposal;

    spdlog::info("[MarriageManager] Character {} proposed to character {} with ring {}",
        proposer_id, target_id, ring_item_id);

    return true;
}

bool MarriageManager::RespondProposal(uint64_t proposal_id, bool accept) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = proposals_.find(proposal_id);
    if (it == proposals_.end()) {
        spdlog::warn("[MarriageManager] Proposal {} not found", proposal_id);
        return false;
    }

    MarriageProposal& proposal = it->second;

    if (accept) {
        // 接受求婚，创建婚姻
        MarriageInfo marriage;
        marriage.marriage_id = next_marriage_id_++;
        marriage.husband_id = proposal.proposer_id;
        marriage.wife_id = proposal.target_id;
        marriage.marriage_date = static_cast<uint32_t>(std::time(nullptr));
        marriage.wedding_type = murim::WeddingType::SIMPLE;
        marriage.intimacy = 0;
        marriage.days_married = 0;
        marriage.has_honeymoon = false;

        marriages_[proposal.proposer_id] = marriage;
        marriages_[proposal.target_id] = marriage;

        spdlog::info("[MarriageManager] Marriage {} created between {} and {}",
            marriage.marriage_id, proposal.proposer_id, proposal.target_id);
    } else {
        spdlog::info("[MarriageManager] Proposal {} rejected", proposal_id);
    }

    proposals_.erase(it);
    return true;
}

const MarriageProposal* MarriageManager::GetProposal(uint64_t proposal_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = proposals_.find(proposal_id);
    if (it != proposals_.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<const MarriageProposal*> MarriageManager::GetProposalsForCharacter(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<const MarriageProposal*> result;
    for (const auto& pair : proposals_) {
        if (pair.second.target_id == character_id) {
            result.push_back(&(pair.second));
        }
    }
    return result;
}

// ========== 婚姻信息 ==========

const MarriageInfo* MarriageManager::GetMarriageInfo(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = marriages_.find(character_id);
    if (it != marriages_.end()) {
        return &(it->second);
    }
    return nullptr;
}

bool MarriageManager::LoadMarriageInfo(uint64_t character_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!db_pool_) {
        return false;
    }

    auto connection = db_pool_->GetConnection();
    if (!connection) {
        spdlog::error("[MarriageManager] Failed to get database connection");
        return false;
    }

    PGconn* conn = connection->GetPGConn();

    // 构建查询
    char query[512];
    snprintf(query, sizeof(query),
        "SELECT marriage_id, husband_id, wife_id, marriage_date, wedding_type, "
        "intimacy, has_honeymoon FROM marriages WHERE husband_id = %lu OR wife_id = %lu",
        (unsigned long)character_id, (unsigned long)character_id
    );

    PGresult* result = PQexec(conn, query);
    bool success = false;

    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
        MarriageInfo info;
        info.marriage_id = strtoull(PQgetvalue(result, 0, 0), nullptr, 10);
        info.husband_id = strtoull(PQgetvalue(result, 0, 1), nullptr, 10);
        info.wife_id = strtoull(PQgetvalue(result, 0, 2), nullptr, 10);
        info.marriage_date = strtoul(PQgetvalue(result, 0, 3), nullptr, 10);
        info.wedding_type = static_cast<murim::WeddingType>(strtol(PQgetvalue(result, 0, 4), nullptr, 10));
        info.intimacy = strtoul(PQgetvalue(result, 0, 5), nullptr, 10);
        info.has_honeymoon = (strcmp(PQgetvalue(result, 0, 6), "t") == 0);

        // 计算结婚天数
        uint32_t now = static_cast<uint32_t>(std::time(nullptr));
        info.days_married = (now - info.marriage_date) / 86400;

        marriages_[character_id] = info;
        success = true;
        spdlog::debug("[MarriageManager] Loaded marriage info for character {}", character_id);
    }

    PQclear(result);
    return success;
}

bool MarriageManager::SaveMarriageInfo(const MarriageInfo& info) {
    if (!db_pool_) {
        return false;
    }

    auto connection = db_pool_->GetConnection();
    if (!connection) {
        spdlog::error("[MarriageManager] Failed to get database connection");
        return false;
    }

    PGconn* conn = connection->GetPGConn();

    // 构建INSERT查询（使用ON CONFLICT）
    char query[1024];
    snprintf(query, sizeof(query),
        "INSERT INTO marriages (marriage_id, husband_id, wife_id, marriage_date, "
        "wedding_type, intimacy, has_honeymoon) VALUES (%lu, %lu, %lu, %u, %d, %u, %s) "
        "ON CONFLICT (marriage_id) DO UPDATE SET intimacy = %u, has_honeymoon = %s",
        (unsigned long)info.marriage_id,
        (unsigned long)info.husband_id,
        (unsigned long)info.wife_id,
        info.marriage_date,
        static_cast<int>(info.wedding_type),
        info.intimacy,
        info.has_honeymoon ? "true" : "false",
        info.intimacy,
        info.has_honeymoon ? "true" : "false"
    );

    PGresult* pg_result = PQexec(conn, query);
    bool success = (PQresultStatus(pg_result) == PGRES_COMMAND_OK);

    if (!success) {
        spdlog::error("[MarriageManager] Failed to save marriage info: {}", PQerrorMessage(conn));
    }

    PQclear(pg_result);
    return success;
}

// ========== 婚礼系统 ==========

bool MarriageManager::HoldWedding(uint64_t partner_id, murim::WeddingType wedding_type, uint32_t venue_id, uint64_t& out_marriage_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否已结婚
    if (!IsMarried(partner_id)) {
        spdlog::warn("[MarriageManager] Partner {} not married", partner_id);
        return false;
    }

    // TODO: 实现婚礼逻辑

    spdlog::info("[MarriageManager] Wedding held for marriage type {}", static_cast<int>(wedding_type));
    return true;
}

// ========== 夫妻技能 ==========

bool MarriageManager::CanUseCoupleSkill(uint64_t character_id, murim::CoupleSkillType skill_type) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否已结婚
    if (!IsMarried(character_id)) {
        return false;
    }

    // TODO: 检查技能冷却和解锁条件

    return true;
}

bool MarriageManager::UseCoupleSkill(uint64_t character_id, murim::CoupleSkillType skill_type, uint64_t partner_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // TODO: 实现技能逻辑

    spdlog::info("[MarriageManager] Character {} used couple skill {} with partner {}",
        character_id, static_cast<int>(skill_type), partner_id);

    return true;
}

// ========== 亲密度系统 ==========

bool MarriageManager::AddIntimacy(uint64_t character_id, uint64_t partner_id, uint32_t amount) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = marriages_.find(character_id);
    if (it == marriages_.end()) {
        return false;
    }

    MarriageInfo& marriage = it->second;
    marriage.intimacy += amount;

    // 限制亲密度最大值
    if (marriage.intimacy > 10000) {
        marriage.intimacy = 10000;
    }

    SaveMarriageInfo(marriage);

    spdlog::debug("[MarriageManager] Added {} intimacy for character {} and {}, new intimacy: {}",
        amount, character_id, partner_id, marriage.intimacy);

    return true;
}

uint32_t MarriageManager::GetIntimacy(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = marriages_.find(character_id);
    if (it != marriages_.end()) {
        return it->second.intimacy;
    }
    return 0;
}

// ========== 离婚系统 ==========

bool MarriageManager::Divorce(uint64_t character_id, uint64_t partner_id, bool mutual, const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto husband_it = marriages_.find(character_id);
    auto wife_it = marriages_.find(partner_id);

    if (husband_it == marriages_.end() || wife_it == marriages_.end()) {
        spdlog::warn("[MarriageManager] Marriage not found between {} and {}", character_id, partner_id);
        return false;
    }

    // 删除婚姻记录
    uint64_t marriage_id = husband_it->second.marriage_id;
    marriages_.erase(husband_it);
    marriages_.erase(wife_it);

    // TODO: 从数据库删除

    spdlog::info("[MarriageManager] Divorce: marriage {} between {} and {}, reason: {}",
        marriage_id, character_id, partner_id, reason);

    return true;
}

// ========== 统计信息 ==========

uint32_t MarriageManager::GetDaysMarried(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = marriages_.find(character_id);
    if (it != marriages_.end()) {
        return it->second.days_married;
    }
    return 0;
}

uint64_t MarriageManager::GetPartnerId(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = marriages_.find(character_id);
    if (it != marriages_.end()) {
        if (it->second.husband_id == character_id) {
            return it->second.wife_id;
        } else {
            return it->second.husband_id;
        }
    }
    return 0;
}

bool MarriageManager::IsMarried(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return marriages_.find(character_id) != marriages_.end();
}

// ========== 定时器 ==========

void MarriageManager::Update(uint32_t delta_time) {
    std::lock_guard<std::mutex> lock(mutex_);

    tick_counter_ += delta_time;

    // 每秒更新一次
    if (tick_counter_ >= 1000) {
        tick_counter_ = 0;

        // 更新结婚天数
        for (auto& pair : marriages_) {
            MarriageInfo& marriage = pair.second;
            uint32_t now = static_cast<uint32_t>(std::time(nullptr));
            marriage.days_married = (now - marriage.marriage_date) / 86400;
        }
    }
}

void MarriageManager::DailyReset() {
    spdlog::info("[MarriageManager] Performing daily reset");

    // 每日重置操作
    // TODO: 重置夫妻技能冷却
}

} // namespace Game
} // namespace Murim
