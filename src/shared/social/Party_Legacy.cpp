// MurimServer - Party System Implementation (Legacy Compatible)
// 组队系统实现 - 完全匹配Legacy实现
// 对应 legacy: legacy-source-code\[Server]Map\Party.cpp

#include "Party_Legacy.hpp"
#include "core/network/NotificationService.hpp"
#include <algorithm>
#include <chrono>

namespace Murim {
namespace Game {

// ========== Party类实现 ==========

Party::Party(uint32_t id, uint64_t masterID, const char* strMasterName,
             uint16_t masterLevel, const PartyAddOption* pAddOption)
    : m_PartyIDx(id)
    , m_TacticObjectID(0)
    , m_Option(0)
    , m_OldSendtime(0)
    , m_dwRequestPlayerID(0)
    , m_dwRequestProcessTime(0)
    , m_MasterChanging(false)
{
    // 初始化成员数组
    memset(m_Member, 0, sizeof(m_Member));

    // 设置队长（索引0）
    m_Member[0].MemberID = masterID;
    strncpy(m_Member[0].Name, strMasterName, MAX_NAME_LENGTH);
    m_Member[0].Name[MAX_NAME_LENGTH] = '\0';
    m_Member[0].Level = masterLevel;
    m_Member[0].bLogged = true;
    m_Member[0].LifePercent = 100;
    m_Member[0].ShieldPercent = 0;
    m_Member[0].NaeRyukPercent = 0;

    // 复制附加选项
    if (pAddOption) {
        memcpy(&m_AddPotion, pAddOption, sizeof(PartyAddOption));
        m_Option = pAddOption->bOption;
    } else {
        memset(&m_AddPotion, 0, sizeof(PartyAddOption));
        m_AddPotion.bOption = ePartyOpt_Random;
        m_Option = ePartyOpt_Random;
    }

    spdlog::debug("[Party] Created party ID={}, master={}", m_PartyIDx, masterID);
}

Party::~Party() {
    spdlog::debug("[Party] Destroyed party ID={}", m_PartyIDx);
}

bool Party::AddPartyMember(uint64_t addMemberID, const char* name, uint16_t lvl,
                           uint8_t lifePercent, uint8_t shieldPercent, uint8_t naeRyukPercent)
{
    // 检查队长是否存在
    if (m_Member[0].MemberID == 0) {
        spdlog::error("[Party] AddPartyMember: No master in party (ID={})", m_PartyIDx);
        return false;
    }

    // 查找空位（索引1-6）
    for (int n = 1; n < MAX_PARTY_LISTNUM; ++n) {
        if (m_Member[n].MemberID == 0) {
            m_Member[n].SetMember(addMemberID, name, lvl, lifePercent, shieldPercent, naeRyukPercent);
            spdlog::debug("[Party] Added member ID={} to party ID={}", addMemberID, m_PartyIDx);
            return true;
        }
    }

    spdlog::error("[Party] AddPartyMember: Party is full (ID={})", m_PartyIDx);
    return false;
}

bool Party::RemovePartyMember(uint64_t memberID)
{
    for (int n = 1; n < MAX_PARTY_LISTNUM; ++n) {
        if (m_Member[n].MemberID == memberID) {
            memset(&m_Member[n], 0, sizeof(PartyMember));
            spdlog::debug("[Party] Removed member ID={} from party ID={}", memberID, m_PartyIDx);
            return true;
        }
    }

    spdlog::warn("[Party] RemovePartyMember: Member ID={} not found in party ID={}", memberID, m_PartyIDx);
    return false;
}

bool Party::ChangeMaster(uint64_t fromID, uint64_t toID)
{
    // 验证
    if (m_Member[0].MemberID == toID) {
        spdlog::error("[Party] ChangeMaster: Cannot transfer to yourself! (ID={})", m_PartyIDx);
        return false;
    }

    if (m_Member[0].MemberID != fromID) {
        spdlog::error("[Party] ChangeMaster: Not the party master! (ID={})", m_PartyIDx);
        return false;
    }

    // 查找新队长并交换
    for (int n = 1; n < MAX_PARTY_LISTNUM; ++n) {
        if (m_Member[n].MemberID == toID) {
            std::swap(m_Member[0], m_Member[n]);
            spdlog::info("[Party] Transferred leadership: {} -> {} (ID={})", fromID, toID, m_PartyIDx);
            return true;
        }
    }

    spdlog::error("[Party] ChangeMaster: Target not found in party! (ID={})", m_PartyIDx);
    return false;
}

bool Party::IsPartyMember(uint64_t playerID) const
{
    for (int n = 0; n < MAX_PARTY_LISTNUM; ++n) {
        if (m_Member[n].MemberID == playerID) {
            return true;
        }
    }
    return false;
}

bool Party::IsMemberLogIn(int n) const
{
    if (n < 0 || n >= MAX_PARTY_LISTNUM) {
        return false;
    }
    return m_Member[n].bLogged;
}

void Party::UserLogIn(uint64_t memberID, const char* name, uint16_t lvl, bool bNotifyUserLogin)
{
    int n = SetMemberInfo(memberID, name, lvl, true);
    if (n == -1) {
        spdlog::error("[Party] UserLogIn: Failed to set member info! ID={}, MemberID={}", m_PartyIDx, memberID);
        return;
    }

    // 发送队伍信息给登录的成员
    SendPartyInfo(memberID);

    if (bNotifyUserLogin) {
        // 发送登录信息给其他成员
        // TODO: 实现SendPlayerInfoToOtherMembers
        spdlog::debug("[Party] Member login notified: ID={} (party ID={})", memberID, m_PartyIDx);
    }
}

void Party::UserLogOut(uint64_t playerID)
{
    for (int n = 0; n < MAX_PARTY_LISTNUM; ++n) {
        if (m_Member[n].MemberID == playerID) {
            m_Member[n].bLogged = false;
            m_Member[n].LifePercent = 0;
            m_Member[n].ShieldPercent = 0;
            m_Member[n].NaeRyukPercent = 0;
            spdlog::debug("[Party] Member logout: ID={} (party ID={})", playerID, m_PartyIDx);
            return;
        }
    }
}

void Party::SendMsgUserLogOut(uint64_t playerID)
{
    // 构造登出消息
    struct MSG_DWORD {
        uint32_t Category;
        uint32_t Protocol;
        uint64_t dwObjectID;
        uint64_t dwData;
    };

    MSG_DWORD msg;
    msg.Category = 0;  // MP_PARTY
    msg.Protocol = 0;  // MP_PARTY_MEMBER_LOGOUT
    msg.dwData = playerID;

    SendMsgToAll(&msg, sizeof(msg));
}

void Party::SendPartyInfo(uint64_t toMemberID)
{
    // TODO: 实现网络消息发送
    spdlog::debug("[Party] Sending party info to member ID={} (party ID={})", toMemberID, m_PartyIDx);
}

void Party::SendMsgToAll(const void* msg, int size)
{
    // TODO: 实现网络消息发送
    spdlog::debug("[Party] Sending message to all members (party ID={})", m_PartyIDx);
}

void Party::SendMsgExceptOne(const void* msg, int size, uint64_t playerID)
{
    // TODO: 实现网络消息发送
    spdlog::debug("[Party] Sending message to all except ID={} (party ID={})", playerID, m_PartyIDx);
}

char* Party::GetMemberName(uint64_t memberID)
{
    for (int n = 0; n < MAX_PARTY_LISTNUM; ++n) {
        if (m_Member[n].MemberID == memberID) {
            return m_Member[n].Name;
        }
    }
    return nullptr;
}

uint64_t Party::GetFirstOnlineMemberID()
{
    for (int n = 0; n < MAX_PARTY_LISTNUM; ++n) {
        if (m_Member[n].MemberID != 0 && m_Member[n].bLogged) {
            return m_Member[n].MemberID;
        }
    }
    return 0;
}

uint8_t Party::GetMemberNum()
{
    uint8_t count = 0;
    for (int n = 0; n < MAX_PARTY_LISTNUM; ++n) {
        if (m_Member[n].MemberID != 0) {
            count++;
        }
    }
    return count;
}

bool Party::IsMasterChanging(uint64_t playerID, uint8_t protocol)
{
    // TODO: 实现队长变更检查逻辑
    return m_MasterChanging;
}

void Party::SetAddOption(const PartyAddOption* pAddOption)
{
    if (pAddOption) {
        memcpy(&m_AddPotion, pAddOption, sizeof(PartyAddOption));
        m_Option = pAddOption->bOption;
    }
}

void Party::StartRequestProcessTime()
{
    m_dwRequestProcessTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

void Party::InitRequestTime()
{
    m_dwRequestProcessTime = 0;
}

int Party::SetMemberInfo(uint64_t memberID, const char* strName, uint16_t lvl, bool bLog)
{
    for (int n = 0; n < MAX_PARTY_LISTNUM; ++n) {
        if (m_Member[n].MemberID == memberID) {
            if (strName) {
                strncpy(m_Member[n].Name, strName, MAX_NAME_LENGTH);
                m_Member[n].Name[MAX_NAME_LENGTH] = '\0';
            }
            m_Member[n].Level = lvl;
            m_Member[n].bLogged = bLog;
            return n;
        }
    }
    return -1;
}

void Party::SetMemberLevel(uint64_t playerID, uint16_t lvl)
{
    for (int n = 0; n < MAX_PARTY_LISTNUM; ++n) {
        if (m_Member[n].MemberID == playerID) {
            m_Member[n].Level = lvl;
            spdlog::debug("[Party] Set member ID={} level to {}", playerID, lvl);
            return;
        }
    }
}

void Party::BreakUp()
{
    spdlog::info("[Party] Breaking up party ID={}", m_PartyIDx);
    memset(m_Member, 0, sizeof(m_Member));
}

void Party::GetMemberInfo(int n, PARTY_MEMBER* pRtInfo)
{
    if (n < 0 || n >= MAX_PARTY_LISTNUM || !pRtInfo) {
        return;
    }

    pRtInfo->dwMemberID = m_Member[n].MemberID;
    strncpy(pRtInfo->Name, m_Member[n].Name, MAX_NAME_LENGTH);
    pRtInfo->Name[MAX_NAME_LENGTH] = '\0';
    pRtInfo->Level = m_Member[n].Level;
    pRtInfo->LifePercent = m_Member[n].LifePercent;
    pRtInfo->ShieldPercent = m_Member[n].ShieldPercent;
    pRtInfo->NaeRyukPercent = m_Member[n].NaeRyukPercent;
    pRtInfo->bLogged = m_Member[n].bLogged;
}

void Party::Process()
{
    // 检查请求超时（10秒）
    if (m_dwRequestProcessTime != 0) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        if (now - m_dwRequestProcessTime > eDICISION_TIME) {
            spdlog::debug("[Party] Request timeout (party ID={})", m_PartyIDx);
            InitRequestTime();
        }
    }
}

// ========== PartyManager类实现 ==========

PartyManager::PartyManager()
    : m_nextPartyID(1)
{
    spdlog::info("[PartyManager] Initialized");
}

PartyManager::~PartyManager()
{
    // 清理所有队伍
    for (auto& pair : m_partyRoomHashTable) {
        delete pair.second;
    }
    m_partyRoomHashTable.clear();
    m_character_to_party_.clear();

    spdlog::info("[PartyManager] Destroyed");
}

PartyManager& PartyManager::Instance()
{
    static PartyManager instance;
    return instance;
}

void PartyManager::Initialize()
{
    spdlog::info("[PartyManager] Initialize called");
}

Party* PartyManager::RegistParty(uint32_t partyID)
{
    Party* pParty = m_partyRoomHashTable[partyID];

    if (pParty == nullptr) {
        PartyAddOption defaultOption;
        pParty = new Party(partyID, 0, "", 1, &defaultOption);
        m_partyRoomHashTable[partyID] = pParty;
        spdlog::info("[PartyManager] Registered new party ID={}", partyID);
    }

    return pParty;
}

void PartyManager::CreatePartyQuery(uint64_t masterID, const PartyAddOption* pAddOption)
{
    // TODO: 实现数据库查询
    uint32_t partyID = m_nextPartyID++;

    Party* pParty = new Party(partyID, masterID, "Master", 50, pAddOption);
    m_partyRoomHashTable[partyID] = pParty;
    m_character_to_party_[masterID] = partyID;

    spdlog::info("[PartyManager] Created party ID={} for master={}", partyID, masterID);
}

void PartyManager::AddPartyInvite(uint64_t masterID, uint64_t targetID)
{
    // TODO: 实现邀请逻辑
    spdlog::info("[PartyManager] Party invite: master={} -> target={}", masterID, targetID);
}

void PartyManager::DelMember(uint64_t memberID, uint32_t partyID)
{
    Party* pParty = GetParty(partyID);
    if (pParty) {
        pParty->RemovePartyMember(memberID);
        m_character_to_party_.erase(memberID);
        spdlog::info("[PartyManager] Removed member {} from party {}", memberID, partyID);
    }
}

void PartyManager::BreakupParty(uint32_t partyID, uint64_t masterID)
{
    Party* pParty = GetParty(partyID);
    if (pParty && pParty->GetMasterID() == masterID) {
        pParty->BreakUp();

        // 清理角色映射
        for (int i = 0; i < MAX_PARTY_LISTNUM; ++i) {
            uint64_t memberID = pParty->GetMemberID(i);
            if (memberID != 0) {
                m_character_to_party_.erase(memberID);
            }
        }

        // 删除队伍
        delete pParty;
        m_partyRoomHashTable.erase(partyID);

        spdlog::info("[PartyManager] Broke up party {}", partyID);
    }
}

Party* PartyManager::GetParty(uint32_t partyID)
{
    auto it = m_partyRoomHashTable.find(partyID);
    if (it != m_partyRoomHashTable.end()) {
        return it->second;
    }
    return nullptr;
}

const Party* PartyManager::GetParty(uint32_t partyID) const
{
    auto it = m_partyRoomHashTable.find(partyID);
    if (it != m_partyRoomHashTable.end()) {
        return it->second;
    }
    return nullptr;
}

Party* PartyManager::GetPartyByMember(uint64_t characterID)
{
    auto it = m_character_to_party_.find(characterID);
    if (it != m_character_to_party_.end()) {
        return GetParty(it->second);
    }
    return nullptr;
}

} // namespace Game
} // namespace Murim
