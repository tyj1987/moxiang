#include "Party.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>

namespace Murim {
namespace Game {

// ========== PartyMember Methods ==========

float PartyMember::DistanceTo(const PartyMember& other) const {
    float dx = x - other.x;
    float dy = y - other.y;
    float dz = z - other.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

uint16_t PartyMember::GetLevelDiff(const PartyMember& other) const {
    if (level >= other.level) {
        return level - other.level;
    }
    return other.level - level;
}

// ========== Party Methods ==========

bool Party::IsMember(uint64_t character_id) const {
    for (const auto& member : members) {
        if (member.character_id == character_id) {
            return true;
        }
    }
    return false;
}

std::optional<PartyMember*> Party::GetMember(uint64_t character_id) {
    for (auto& member : members) {
        if (member.character_id == character_id) {
            return &member;
        }
    }
    return std::nullopt;
}

bool Party::AddMember(const PartyMember& member) {
    // Check if already a member
    if (IsMember(member.character_id)) {
        spdlog::warn("Party::AddMember failed: Character {} already in party {}", member.character_id, party_id);
        return false;
    }

    // Check if party is full
    if (IsFull()) {
        spdlog::warn("Party::AddMember failed: Party {} is full", party_id);
        return false;
    }

    members.push_back(member);
    spdlog::debug("Party::AddMember: party_id={}, character_id={}, name='{}', member_count={}",
                  party_id, member.character_id, member.character_name, members.size());

    return true;
}

bool Party::RemoveMember(uint64_t character_id) {
    for (auto it = members.begin(); it != members.end(); ++it) {
        if (it->character_id == character_id) {
            spdlog::debug("Party::RemoveMember: party_id={}, character_id={}, name='{}', remaining={}",
                          party_id, character_id, it->character_name, members.size() - 1);
            members.erase(it);
            return true;
        }
    }

    spdlog::warn("Party::RemoveMember failed: Character {} not found in party {}", character_id, party_id);
    return false;
}

bool Party::SetLeader(uint64_t character_id) {
    // Verify new leader is a member
    if (!IsMember(character_id)) {
        spdlog::warn("Party::SetLeader failed: Character {} is not a member of party {}", character_id, party_id);
        return false;
    }

    uint64_t old_leader = leader_id;
    leader_id = character_id;

    spdlog::info("Party::SetLeader: party_id={}, old_leader={}, new_leader={}", party_id, old_leader, leader_id);
    return true;
}

bool Party::UpdateMemberStatus(uint64_t character_id, PartyMemberStatus status) {
    for (auto& member : members) {
        if (member.character_id == character_id) {
            member.status = status;
            spdlog::debug("Party::UpdateMemberStatus: party_id={}, character_id={}, status={}",
                          party_id, character_id, static_cast<int>(status));
            return true;
        }
    }

    spdlog::warn("Party::UpdateMemberStatus failed: Character {} not found in party {}", character_id, party_id);
    return false;
}

bool Party::UpdateMemberPosition(
    uint64_t character_id,
    float x, float y, float z
) {
    for (auto& member : members) {
        if (member.character_id == character_id) {
            member.x = x;
            member.y = y;
            member.z = z;
            return true;
        }
    }

    spdlog::warn("Party::UpdateMemberPosition failed: Character {} not found in party {}", character_id, party_id);
    return false;
}

} // namespace Game
} // namespace Murim
