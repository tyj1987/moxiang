#include "Party.hpp"
#include "core/database/PartyDAO.hpp"
#include "core/database/CharacterDAO.hpp"
#include "core/network/NotificationService.hpp"
#include "shared/character/CharacterManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>

// using Database = Murim::Core::Database;  // Removed - causes parsing errors
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Game {

// ========== PartyManager Singleton ==========

PartyManager& PartyManager::Instance() {
    static PartyManager instance;
    return instance;
}

bool PartyManager::Initialize() {
    spdlog::info("PartyManager initialized");
    return true;
}

void PartyManager::Process() {
    // 处理队伍逻辑 (暂时为空)
}

// ========== Party Creation/Disbandment ==========

uint64_t PartyManager::CreateParty(uint64_t leader_id, uint8_t max_members) {
    // Validate parameters
    if (leader_id == 0) {
        spdlog::error("CreateParty failed: Invalid leader_id");
        return 0;
    }

    if (max_members == 0 || max_members > 8) {
        spdlog::warn("CreateParty: Invalid max_members {}, using default 6", max_members);
        max_members = 6;
    }

    // Check if leader is already in a party
    auto existing_party_id = GetPartyID(leader_id);
    if (existing_party_id.has_value()) {
        spdlog::warn("CreateParty failed: Leader {} already in party {}", leader_id, existing_party_id.value());
        return 0;
    }

    // Get leader information from database
    Character leader_info;
    bool leader_loaded = false;

    try {
        auto character_opt = Murim::Core::Database::CharacterDAO::Load(leader_id);
        if (character_opt.has_value()) {
            leader_info.name = character_opt->name;
            leader_info.level = character_opt->level;
            leader_info.initial_weapon = character_opt->initial_weapon;
            leader_loaded = true;
        }
    } catch (const std::exception& e) {
        spdlog::error("CreateParty: Failed to load leader info: {}", e.what());
    }

    if (!leader_loaded) {
        spdlog::error("CreateParty failed: Cannot load leader info for {}", leader_id);
        return 0;
    }

    // Create party in database
    uint64_t party_id = 0;
    try {
        party_id = Murim::Core::Database::PartyDAO::CreateParty(leader_id, max_members);
    } catch (const std::exception& e) {
        spdlog::error("CreateParty: Database error: {}", e.what());
        return 0;
    }

    if (party_id == 0) {
        spdlog::error("CreateParty failed: Database returned party_id=0");
        return 0;
    }

    // Create party in memory
    Party party;
    party.party_id = party_id;
    party.leader_id = leader_id;
    party.create_time = std::chrono::system_clock::now();

    // Set default party settings
    party.settings.max_members = max_members;
    party.settings.exp_type = PartyExpDistributionType::kLevelBased;
    party.settings.auto_loot = 0;  // Off
    party.settings.level_diff_limit = 10;
    party.settings.share_range = 5000.0f;  // 50 meters

    // Add leader as first member
    PartyMember leader_member;
    leader_member.character_id = leader_id;
    leader_member.character_name = leader_info.name;
    leader_member.level = static_cast<uint8_t>(leader_info.level);
    leader_member.job = static_cast<uint8_t>(leader_info.initial_weapon);
    leader_member.hp = leader_info.hp;
    leader_member.max_hp = leader_info.max_hp;
    leader_member.mp = leader_info.mp;
    leader_member.max_mp = leader_info.max_mp;
    leader_member.status = PartyMemberStatus::kOnline;
    leader_member.x = leader_info.x;
    leader_member.y = leader_info.y;
    leader_member.z = leader_info.z;
    leader_member.join_time = std::chrono::system_clock::now();

    party.members.push_back(leader_member);

    // Store in memory
    parties_[party_id] = party;
    character_to_party_[leader_id] = party_id;

    spdlog::info("Party created: party_id={}, leader_id={}, name='{}', max_members={}",
                 party_id, leader_id, leader_info.name, max_members);

    // Notify leader
    SendPartyNotification(party_id, "Party created successfully");

    return party_id;
}

bool PartyManager::DisbandParty(uint64_t party_id) {
    if (party_id == 0) {
        spdlog::error("DisbandParty failed: Invalid party_id");
        return false;
    }

    auto it = parties_.find(party_id);
    if (it == parties_.end()) {
        spdlog::warn("DisbandParty failed: Party {} not found in memory", party_id);
        return false;
    }

    Party& party = it->second;

    // Notify all members
    SendPartyNotification(party_id, "Party is being disbanded");

    // Remove character_to_party_ mappings
    for (const auto& member : party.members) {
        character_to_party_.erase(member.character_id);

        // Notify each member
        Core::Network::NotificationService::Instance().NotifyPartyLeft(member.character_id);
    }

    // Delete from database
    try {
        Murim::Core::Database::PartyDAO::DisbandParty(party_id);
    } catch (const std::exception& e) {
        spdlog::error("DisbandParty: Database error: {}", e.what());
        // Continue with memory cleanup
    }

    // Remove from memory
    parties_.erase(it);

    spdlog::info("Party disbanded: party_id={}", party_id);

    return true;
}

// ========== Member Operations ==========

bool PartyManager::InviteToParty(
    uint64_t party_id,
    uint64_t inviter_id,
    uint64_t target_id
) {
    // Validate party
    auto party_opt = GetParty(party_id);
    if (!party_opt.has_value()) {
        spdlog::warn("InviteToParty failed: Party {} not found", party_id);
        return false;
    }

    Party* party = party_opt.value();

    // Validate inviter is in party
    if (!party->IsMember(inviter_id)) {
        spdlog::warn("InviteToParty failed: Inviter {} not in party {}", inviter_id, party_id);
        return false;
    }

    // Only leader can invite (legacy behavior)
    if (!party->IsLeader(inviter_id)) {
        spdlog::warn("InviteToParty failed: Inviter {} is not leader of party {}", inviter_id, party_id);
        return false;
    }

    // Check if party is full
    if (party->IsFull()) {
        spdlog::warn("InviteToParty failed: Party {} is full", party_id);
        return false;
    }

    // Check if target is already in a party
    auto target_party_id = GetPartyID(target_id);
    if (target_party_id.has_value()) {
        spdlog::warn("InviteToParty failed: Target {} already in party {}", target_id, target_party_id.value());
        return false;
    }

    // Check if target already has a pending invite from this party
    auto invite_it = party_invites_.find(target_id);
    if (invite_it != party_invites_.end()) {
        if (invite_it->second.find(inviter_id) != invite_it->second.end()) {
            spdlog::warn("InviteToParty failed: Target {} already has pending invite from {}", target_id, inviter_id);
            return false;
        }
    }

    // Get inviter name
    std::string inviter_name = "Unknown";
    for (const auto& member : party->members) {
        if (member.character_id == inviter_id) {
            inviter_name = member.character_name;
            break;
        }
    }

    // Create invite in memory
    party_invites_[target_id][inviter_id] = party_id;

    // Create invite in database
    try {
        Murim::Core::Database::PartyDAO::CreatePartyInvite(party_id, inviter_id, target_id);
    } catch (const std::exception& e) {
        spdlog::error("InviteToParty: Database error: {}", e.what());
        // Continue with memory invite
    }

    // Send invite to target
    SendPartyInvite(target_id, inviter_id, party_id);

    spdlog::info("Party invite sent: party_id={}, inviter_id={}, target_id={}",
                 party_id, inviter_id, target_id);

    return true;
}

bool PartyManager::AcceptInvite(uint64_t character_id, uint64_t party_id) {
    // Validate party
    auto party_opt = GetParty(party_id);
    if (!party_opt.has_value()) {
        spdlog::warn("AcceptInvite failed: Party {} not found", party_id);
        return false;
    }

    Party* party = party_opt.value();

    // Check if character already in a party
    auto existing_party_id = GetPartyID(character_id);
    if (existing_party_id.has_value()) {
        spdlog::warn("AcceptInvite failed: Character {} already in party {}", character_id, existing_party_id.value());
        return false;
    }

    // Check if party is full
    if (party->IsFull()) {
        spdlog::warn("AcceptInvite failed: Party {} is full", party_id);
        return false;
    }

    // Check if there's a pending invite
    auto invite_it = party_invites_.find(character_id);
    bool has_invite = false;
    if (invite_it != party_invites_.end()) {
        for (const auto& [inviter_id, invite_party_id] : invite_it->second) {
            if (invite_party_id == party_id) {
                has_invite = true;
                break;
            }
        }
    }

    if (!has_invite) {
        spdlog::warn("AcceptInvite failed: No pending invite for character {} to party {}", character_id, party_id);
        return false;
    }

    // Get character information
    std::string character_name;
    uint16_t character_level = 1;
    uint8_t character_job = 0;
    uint32_t character_hp = 100;
    uint32_t character_max_hp = 100;
    uint32_t character_mp = 50;
    uint32_t character_max_mp = 50;
    float character_x = 0.0f;
    float character_y = 0.0f;
    float character_z = 0.0f;

    try {
        auto character_opt = Murim::Core::Database::CharacterDAO::Load(character_id);
        if (character_opt.has_value()) {
            character_name = character_opt->name;
            character_level = character_opt->level;
            character_job = static_cast<uint8_t>(character_opt->initial_weapon);
            character_hp = character_opt->hp;
            character_max_hp = character_opt->max_hp;
            character_mp = character_opt->mp;
            character_max_mp = character_opt->max_mp;
            character_x = character_opt->x;
            character_y = character_opt->y;
            character_z = character_opt->z;
        } else {
            spdlog::error("AcceptInvite: Character {} not found in database", character_id);
            return false;
        }
    } catch (const std::exception& e) {
        spdlog::error("AcceptInvite: Failed to load character info: {}", e.what());
        return false;
    }

    // Add member to party in database
    try {
        if (!Murim::Core::Database::PartyDAO::AddPartyMember(party_id, character_id, character_name,
                                                 character_level, character_job)) {
            spdlog::error("AcceptInvite: Failed to add member to database");
            return false;
        }
    } catch (const std::exception& e) {
        spdlog::error("AcceptInvite: Database error: {}", e.what());
        return false;
    }

    // Add member to party in memory
    PartyMember new_member;
    new_member.character_id = character_id;
    new_member.character_name = character_name;
    new_member.level = character_level;
    new_member.job = character_job;
    new_member.hp = character_hp;
    new_member.max_hp = character_max_hp;
    new_member.mp = character_mp;
    new_member.max_mp = character_max_mp;
    new_member.status = PartyMemberStatus::kOnline;
    new_member.x = character_x;
    new_member.y = character_y;
    new_member.z = character_z;
    new_member.join_time = std::chrono::system_clock::now();

    party->members.push_back(new_member);
    character_to_party_[character_id] = party_id;

    // Remove invite
    if (invite_it != party_invites_.end()) {
        invite_it->second.clear();
        party_invites_.erase(invite_it);
    }

    // Notify all party members
    SendPartyNotification(party_id, character_name + " joined the party");

    // Notify the new member
    Core::Network::NotificationService::Instance().NotifyPartyJoined(character_id, party_id);

    spdlog::info("Party member added: party_id={}, character_id={}, name='{}', member_count={}",
                 party_id, character_id, character_name, party->GetMemberCount());

    return true;
}

bool PartyManager::RejectInvite(uint64_t character_id, uint64_t party_id) {
    // Remove the invite
    auto invite_it = party_invites_.find(character_id);
    if (invite_it == party_invites_.end()) {
        spdlog::warn("RejectInvite: No pending invite for character {} to party {}", character_id, party_id);
        return false;
    }

    bool found = false;
    for (auto it = invite_it->second.begin(); it != invite_it->second.end(); ) {
        if (it->second == party_id) {
            it = invite_it->second.erase(it);
            found = true;
            break;
        } else {
            ++it;
        }
    }

    if (invite_it->second.empty()) {
        party_invites_.erase(invite_it);
    }

    if (found) {
        spdlog::info("Party invite rejected: character_id={}, party_id={}", character_id, party_id);
    }

    return found;
}

bool PartyManager::LeaveParty(uint64_t character_id) {
    // Get party_id
    auto party_id_opt = GetPartyID(character_id);
    if (!party_id_opt.has_value()) {
        spdlog::warn("LeaveParty failed: Character {} not in a party", character_id);
        return false;
    }

    uint64_t party_id = party_id_opt.value();

    // Get party
    auto party_opt = GetParty(party_id);
    if (!party_opt.has_value()) {
        spdlog::warn("LeaveParty failed: Party {} not found", party_id);
        return false;
    }

    Party* party = party_opt.value();

    // Check if character is a member
    if (!party->IsMember(character_id)) {
        spdlog::warn("LeaveParty failed: Character {} not a member of party {}", character_id, party_id);
        return false;
    }

    // Get member name for notification
    std::string member_name;
    for (const auto& member : party->members) {
        if (member.character_id == character_id) {
            member_name = member.character_name;
            break;
        }
    }

    // If leader is leaving, transfer leadership or disband
    if (party->IsLeader(character_id)) {
        if (party->GetMemberCount() > 1) {
            // Transfer leadership to first non-leader member
            for (const auto& member : party->members) {
                if (member.character_id != character_id) {
                    spdlog::info("Leader {} leaving party {}, transferring leadership to {}",
                                character_id, party_id, member.character_id);
                    party->leader_id = member.character_id;
                    break;
                }
            }
        } else {
            // Only member, disband party
            spdlog::info("Last member {} leaving party {}, disbanding", character_id, party_id);
            return DisbandParty(party_id);
        }
    }

    // Remove from database
    try {
        Murim::Core::Database::PartyDAO::RemovePartyMember(party_id, character_id);
    } catch (const std::exception& e) {
        spdlog::error("LeaveParty: Database error: {}", e.what());
        // Continue with memory cleanup
    }

    // Remove from memory
    party->RemoveMember(character_id);
    character_to_party_.erase(character_id);

    // Notify
    SendPartyNotification(party_id, member_name + " left the party");
    Core::Network::NotificationService::Instance().NotifyPartyLeft(character_id);

    spdlog::info("Party member left: party_id={}, character_id={}, name='{}', remaining_members={}",
                 party_id, character_id, member_name, party->GetMemberCount());

    return true;
}

bool PartyManager::KickFromParty(
    uint64_t party_id,
    uint64_t leader_id,
    uint64_t target_id
) {
    // Validate party
    auto party_opt = GetParty(party_id);
    if (!party_opt.has_value()) {
        spdlog::warn("KickFromParty failed: Party {} not found", party_id);
        return false;
    }

    Party* party = party_opt.value();

    // Validate leader
    if (!party->IsLeader(leader_id)) {
        spdlog::warn("KickFromParty failed: {} is not leader of party {}", leader_id, party_id);
        return false;
    }

    // Cannot kick yourself
    if (leader_id == target_id) {
        spdlog::warn("KickFromParty failed: Cannot kick yourself");
        return false;
    }

    // Validate target is member
    if (!party->IsMember(target_id)) {
        spdlog::warn("KickFromParty failed: Target {} not in party {}", target_id, party_id);
        return false;
    }

    // Get target name
    std::string target_name;
    for (const auto& member : party->members) {
        if (member.character_id == target_id) {
            target_name = member.character_name;
            break;
        }
    }

    // Remove from database
    try {
        Murim::Core::Database::PartyDAO::RemovePartyMember(party_id, target_id);
    } catch (const std::exception& e) {
        spdlog::error("KickFromParty: Database error: {}", e.what());
        // Continue with memory cleanup
    }

    // Remove from memory
    party->RemoveMember(target_id);
    character_to_party_.erase(target_id);

    // Notify
    SendPartyNotification(party_id, target_name + " was kicked from the party");
    Core::Network::NotificationService::Instance().NotifyPartyLeft(target_id);

    spdlog::info("Party member kicked: party_id={}, leader_id={}, target_id={}, name='{}'",
                 party_id, leader_id, target_id, target_name);

    return true;
}

bool PartyManager::TransferLeadership(
    uint64_t party_id,
    uint64_t leader_id,
    uint64_t new_leader_id
) {
    // Validate party
    auto party_opt = GetParty(party_id);
    if (!party_opt.has_value()) {
        spdlog::warn("TransferLeadership failed: Party {} not found", party_id);
        return false;
    }

    Party* party = party_opt.value();

    // Validate current leader
    if (!party->IsLeader(leader_id)) {
        spdlog::warn("TransferLeadership failed: {} is not leader of party {}", leader_id, party_id);
        return false;
    }

    // Cannot transfer to yourself
    if (leader_id == new_leader_id) {
        spdlog::warn("TransferLeadership failed: Cannot transfer to yourself");
        return false;
    }

    // Validate new leader is member
    if (!party->IsMember(new_leader_id)) {
        spdlog::warn("TransferLeadership failed: New leader {} not in party {}", new_leader_id, party_id);
        return false;
    }

    // Get new leader name
    std::string new_leader_name;
    for (const auto& member : party->members) {
        if (member.character_id == new_leader_id) {
            new_leader_name = member.character_name;
            break;
        }
    }

    // Update in database
    try {
        Murim::Core::Database::PartyDAO::TransferLeadership(party_id, new_leader_id);
    } catch (const std::exception& e) {
        spdlog::error("TransferLeadership: Database error: {}", e.what());
        // Continue with memory update
    }

    // Update in memory
    party->SetLeader(new_leader_id);

    // Notify
    SendPartyNotification(party_id, new_leader_name + " is now the party leader");

    spdlog::info("Party leadership transferred: party_id={}, old_leader={}, new_leader={}, name='{}'",
                 party_id, leader_id, new_leader_id, new_leader_name);

    return true;
}

// ========== Party Query ==========

std::optional<Party*> PartyManager::GetParty(uint64_t party_id) {
    auto it = parties_.find(party_id);
    if (it != parties_.end()) {
        return &(it->second);
    }

    // Try to load from database
    try {
        auto party_opt = Murim::Core::Database::PartyDAO::GetParty(party_id);
        if (party_opt.has_value()) {
            // Load into memory
            parties_[party_id] = party_opt.value();

            // Update character_to_party_ mapping
            for (const auto& member : party_opt.value().members) {
                character_to_party_[member.character_id] = party_id;
            }

            return &(parties_[party_id]);
        }
    } catch (const std::exception& e) {
        spdlog::error("GetParty: Database error: {}", e.what());
    }

    return std::nullopt;
}

std::optional<uint64_t> PartyManager::GetPartyID(uint64_t character_id) {
    auto it = character_to_party_.find(character_id);
    if (it != character_to_party_.end()) {
        return it->second;
    }

    // Check database
    try {
        uint64_t party_id = Murim::Core::Database::PartyDAO::GetPartyID(character_id);
        if (party_id != 0) {
            // Load party into memory
            GetParty(party_id);
            return party_id;
        }
    } catch (const std::exception& e) {
        spdlog::error("GetPartyID: Database error: {}", e.what());
    }

    return std::nullopt;
}

Party* PartyManager::GetCharacterParty(uint64_t character_id) {
    auto party_id_opt = GetPartyID(character_id);
    if (party_id_opt.has_value()) {
        return GetParty(party_id_opt.value()).value_or(nullptr);
    }
    return nullptr;
}

std::vector<PartyMember> PartyManager::GetPartyMembersInRange(
    uint64_t character_id,
    float range
) {
    std::vector<PartyMember> members_in_range;

    Party* party = GetCharacterParty(character_id);
    if (party == nullptr) {
        return members_in_range;
    }

    // Get character position
    float char_x = 0.0f, char_y = 0.0f, char_z = 0.0f;
    for (const auto& member : party->members) {
        if (member.character_id == character_id) {
            char_x = member.x;
            char_y = member.y;
            char_z = member.z;
            break;
        }
    }

    // Find members in range
    float range_squared = range * range;
    for (const auto& member : party->members) {
        if (member.character_id == character_id) {
            continue;  // Skip self
        }

        if (!member.IsOnline()) {
            continue;  // Skip offline members
        }

        // Calculate distance squared
        float dx = member.x - char_x;
        float dy = member.y - char_y;
        float dz = member.z - char_z;
        float dist_squared = dx*dx + dy*dy + dz*dz;

        if (dist_squared <= range_squared) {
            members_in_range.push_back(member);
        }
    }

    spdlog::debug("GetPartyMembersInRange: character_id={}, range={}, members_found={}",
                  character_id, range, members_in_range.size());

    return members_in_range;
}

// ========== Experience Distribution ==========

std::unordered_map<uint64_t, uint32_t> PartyManager::DistributeExp(
    uint64_t party_id,
    uint32_t base_exp,
    const std::vector<uint64_t>& contributor_ids
) {
    std::unordered_map<uint64_t, uint32_t> exp_distribution;

    auto party_opt = GetParty(party_id);
    if (!party_opt.has_value()) {
        spdlog::warn("DistributeExp failed: Party {} not found", party_id);
        return exp_distribution;
    }

    Party* party = party_opt.value();

    if (party->members.empty()) {
        spdlog::warn("DistributeExp failed: Party {} has no members", party_id);
        return exp_distribution;
    }

    // Find max level in party
    uint16_t max_level = 0;
    for (const auto& member : party->members) {
        if (member.IsOnline() && member.IsAlive() && member.level > max_level) {
            max_level = member.level;
        }
    }

    // Get eligible members (online, alive, in range)
    std::vector<PartyMember*> eligible_members;
    for (auto& member : party->members) {
        if (!member.IsOnline() || !member.IsAlive()) {
            continue;
        }

        // Check if in range (using party settings share range)
        // For now, assume all online members are in range
        eligible_members.push_back(&member);
    }

    if (eligible_members.empty()) {
        spdlog::debug("DistributeExp: No eligible members in party {}", party_id);
        return exp_distribution;
    }

    // Distribute based on type
    switch (party->settings.exp_type) {
        case PartyExpDistributionType::kLevelBased:
            for (auto* member : eligible_members) {
                uint32_t member_exp = CalculateLevelBasedExp(base_exp, member->level, max_level);
                exp_distribution[member->character_id] = member_exp;
            }
            break;

        case PartyExpDistributionType::kEqual:
            {
                uint32_t exp_per_member = base_exp / static_cast<uint32_t>(eligible_members.size());
                for (auto* member : eligible_members) {
                    exp_distribution[member->character_id] = exp_per_member;
                }
            }
            break;

        case PartyExpDistributionType::kContribution:
            {
                // Calculate total contribution
                std::unordered_map<uint64_t, uint32_t> contribution_map;
                uint32_t total_contribution = 0;

                for (uint64_t contributor_id : contributor_ids) {
                    // Find contributor in party
                    bool is_member = false;
                    for (auto* member : eligible_members) {
                        if (member->character_id == contributor_id) {
                            is_member = true;
                            // Use level as contribution weight (simplified)
                            uint32_t contribution = member->level;
                            contribution_map[contributor_id] = contribution;
                            total_contribution += contribution;
                            break;
                        }
                    }
                }

                // Distribute based on contribution
                if (total_contribution > 0) {
                    for (auto* member : eligible_members) {
                        auto it = contribution_map.find(member->character_id);
                        if (it != contribution_map.end()) {
                            float ratio = static_cast<float>(it->second) / total_contribution;
                            exp_distribution[member->character_id] = static_cast<uint32_t>(base_exp * ratio);
                        } else {
                            // Non-contributors get reduced exp
                            exp_distribution[member->character_id] = base_exp / (eligible_members.size() * 2);
                        }
                    }
                } else {
                    // No contributors, distribute equally
                    uint32_t exp_per_member = base_exp / eligible_members.size();
                    for (auto* member : eligible_members) {
                        exp_distribution[member->character_id] = exp_per_member;
                    }
                }
            }
            break;

        default:
            spdlog::warn("DistributeExp: Unknown exp distribution type {}",
                        static_cast<int>(party->settings.exp_type));
            break;
    }

    spdlog::debug("DistributeExp: party_id={}, base_exp={}, members={}, distribution_type={}",
                  party_id, base_exp, exp_distribution.size(),
                  static_cast<int>(party->settings.exp_type));

    return exp_distribution;
}

// ========== Private Methods ==========

uint64_t PartyManager::GeneratePartyId() {
    return next_party_id_++;
}

void PartyManager::SendPartyNotification(
    uint64_t party_id,
    const std::string& message
) {
    auto party_opt = GetParty(party_id);
    if (!party_opt.has_value()) {
        return;
    }

    Party* party = party_opt.value();

    for (const auto& member : party->members) {
        if (member.IsOnline()) {
            Core::Network::NotificationService::Instance().SendSystemMessage(member.character_id, message);
        }
    }

    spdlog::debug("Party notification sent: party_id={}, message='{}'", party_id, message);
}

void PartyManager::SendPartyInvite(
    uint64_t target_id,
    uint64_t inviter_id,
    uint64_t party_id
) {
    // Get inviter name
    std::string inviter_name = "Unknown";

    auto party_opt = GetParty(party_id);
    if (party_opt.has_value()) {
        Party* party = party_opt.value();
        for (const auto& member : party->members) {
            if (member.character_id == inviter_id) {
                inviter_name = member.character_name;
                break;
            }
        }
    }

    // Also try to load from database if not found in party
    if (inviter_name == "Unknown") {
        try {
            auto character_opt = Murim::Core::Database::CharacterDAO::Load(inviter_id);
            if (character_opt.has_value()) {
                inviter_name = character_opt->name;
            }
        } catch (const std::exception& e) {
            spdlog::error("SendPartyInvite: Failed to load inviter name: {}", e.what());
        }
    }

    Core::Network::NotificationService::Instance().NotifyPartyInvite(
        target_id, inviter_id, inviter_name, party_id
    );

    spdlog::debug("Party invite sent: target_id={}, inviter_id={}, inviter_name='{}', party_id={}",
                  target_id, inviter_id, inviter_name, party_id);
}

uint32_t PartyManager::CalculateLevelBasedExp(
    uint32_t base_exp,
    uint16_t character_level,
    uint16_t max_level
) {
    // Legacy formula from Distribute_Random.cpp and Distribute_Damage.cpp
    // Higher level members get more exp, but it's normalized

    if (max_level == 0) {
        return base_exp;
    }

    // Calculate level ratio (0.5 to 1.0)
    float level_ratio = 0.5f + 0.5f * (static_cast<float>(character_level) / max_level);

    // Apply ratio to base exp
    uint32_t calculated_exp = static_cast<uint32_t>(base_exp * level_ratio);

    // Ensure minimum exp (at least 10% of base)
    calculated_exp = std::max(calculated_exp, base_exp / 10);

    return calculated_exp;
}

} // namespace Game
} // namespace Murim
