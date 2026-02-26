#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <mutex>
#include "protocol/marriage.pb.h"
#include "core/database/DatabasePool.hpp"  // File is DatabasePool.hpp, class is ConnectionPool

namespace Murim {
namespace Game {

/**
 * @brief 求婚信息
 */
struct MarriageProposal {
    uint64_t proposal_id = 0;
    uint64_t proposer_id = 0;
    std::string proposer_name;
    uint64_t target_id = 0;
    std::string target_name;
    uint32_t proposal_time = 0;
    uint32_t ring_item_id = 0;
    std::string message;
    bool accepted = false;
};

/**
 * @brief 婚姻信息
 */
struct MarriageInfo {
    uint64_t marriage_id = 0;
    uint64_t husband_id = 0;
    std::string husband_name;
    uint64_t wife_id = 0;
    std::string wife_name;
    uint32_t marriage_date = 0;
    murim::WeddingType wedding_type;
    uint32_t intimacy = 0;
    uint32_t days_married = 0;
    bool has_honeymoon = false;
};

/**
 * @brief 夫妻技能
 */
struct CoupleSkill {
    murim::CoupleSkillType skill_type;
    uint32_t skill_level = 1;
    uint32_t cooldown = 0;
    bool unlocked = false;
    uint32_t unlock_intimacy = 0;
};

/**
 * @brief 结婚管理器
 */
class MarriageManager {
public:
    static MarriageManager& Instance();

    bool Initialize(std::shared_ptr<Core::Database::ConnectionPool> db_pool);
    void Shutdown();

    // ========== 求婚系统 ==========

    bool SendProposal(uint64_t proposer_id, uint64_t target_id, uint32_t ring_item_id, const std::string& message);
    bool RespondProposal(uint64_t proposal_id, bool accept);
    const MarriageProposal* GetProposal(uint64_t proposal_id) const;
    std::vector<const MarriageProposal*> GetProposalsForCharacter(uint64_t character_id) const;

    // ========== 婚姻信息 ==========

    const MarriageInfo* GetMarriageInfo(uint64_t character_id) const;
    bool LoadMarriageInfo(uint64_t character_id);
    bool SaveMarriageInfo(const MarriageInfo& info);

    // ========== 婚礼系统 ==========

    bool HoldWedding(uint64_t partner_id, murim::WeddingType wedding_type, uint32_t venue_id, uint64_t& out_marriage_id);

    // ========== 夫妻技能 ==========

    bool CanUseCoupleSkill(uint64_t character_id, murim::CoupleSkillType skill_type) const;
    bool UseCoupleSkill(uint64_t character_id, murim::CoupleSkillType skill_type, uint64_t partner_id);

    // ========== 亲密度系统 ==========

    bool AddIntimacy(uint64_t character_id, uint64_t partner_id, uint32_t amount);
    uint32_t GetIntimacy(uint64_t character_id) const;

    // ========== 离婚系统 ==========

    bool Divorce(uint64_t character_id, uint64_t partner_id, bool mutual, const std::string& reason);

    // ========== 统计信息 ==========

    uint32_t GetDaysMarried(uint64_t character_id) const;
    uint64_t GetPartnerId(uint64_t character_id) const;
    bool IsMarried(uint64_t character_id) const;

    // ========== 定时器 ==========

    void Update(uint32_t delta_time);
    void DailyReset();

private:
    MarriageManager() = default;
    ~MarriageManager() = default;
    MarriageManager(const MarriageManager&) = delete;
    MarriageManager& operator=(const MarriageManager&) = delete;

    mutable std::mutex mutex_;
    std::shared_ptr<Core::Database::ConnectionPool> db_pool_;

    std::unordered_map<uint64_t, MarriageInfo> marriages_;        // character_id -> marriage
    std::unordered_map<uint64_t, MarriageProposal> proposals_;    // proposal_id -> proposal
    std::unordered_map<uint64_t, CoupleSkill> skills_;            // character_id -> skills

    uint64_t next_proposal_id_ = 1;
    uint64_t next_marriage_id_ = 1;
    uint32_t tick_counter_ = 0;
};

} // namespace Game
} // namespace Murim
