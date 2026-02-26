// MurimServer - Guild System Implementation (Legacy Compatible)
// 公会系统实现 - 完全匹配Legacy实现
// 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp

#include "Guild_Legacy.hpp"
#include "core/network/NotificationService.hpp"
#include <algorithm>

namespace Murim {
namespace Game {

// ========== Guild类实现 ==========

Guild::Guild(GUILDINFO* pInfo, uint32_t guildMoney)
    : m_MarkName(0)
    , m_nMemberOnConnectingThisMap(0)
    , m_GTBattleID(0)
    , m_bItemInfoInited(false)
    , m_bWaitingItemInfoFromDB(false)
    , m_nStudentCount(0)
{
    if (pInfo) {
        memcpy(&m_GuildInfo, pInfo, sizeof(GUILDINFO));
    }

    memset(m_RankMemberIdx, 0, sizeof(m_RankMemberIdx));

    spdlog::info("[Guild] Created guild ID={}, name={}", m_GuildInfo.GuildIdx, m_GuildInfo.GuildName);
}

Guild::~Guild()
{
    // 清理成员列表
    for (auto& pair : m_MemberList) {
        delete pair.second;
    }
    m_MemberList.clear();

    spdlog::info("[Guild] Destroyed guild ID={}", m_GuildInfo.GuildIdx);
}

// ========== 权限检查 ==========

bool Guild::IsMaster(uint32_t playerIDX) const
{
    return m_GuildInfo.MasterIdx == playerIDX;
}

bool Guild::IsViceMaster(uint32_t playerIDX) const
{
    return m_RankMemberIdx[0] == playerIDX;
}

bool Guild::IsMember(uint32_t memberIDX) const
{
    // 检查是否在成员列表中
    auto it = m_MemberList.find(memberIDX);
    if (it != m_MemberList.end()) {
        return true;
    }
    return false;
}

// ========== 成员管理 ==========

bool Guild::AddMember(GUILDMEMBERINFO* pMemberInfo)
{
    if (!pMemberInfo) {
        spdlog::error("[Guild] AddMember: pMemberInfo is null!");
        return false;
    }

    // 检查是否已存在
    if (IsMember(pMemberInfo->MemberIdx)) {
        spdlog::warn("[Guild] AddMember: Member {} already exists", pMemberInfo->MemberIdx);
        return false;
    }

    // 检查是否已满
    // MAX_GUILDMEMBER_NUM包括会长，m_MemberList不包括会长，所以限制是MAX_GUILDMEMBER_NUM-1
    if (m_MemberList.size() >= (MAX_GUILDMEMBER_NUM - 1)) {
        spdlog::error("[Guild] AddMember: Guild is full! (ID={})", m_GuildInfo.GuildIdx);
        return false;
    }

    // 创建新成员信息
    GUILDMEMBERINFO* pInfo = new GUILDMEMBERINFO;
    memcpy(pInfo, pMemberInfo, sizeof(GUILDMEMBERINFO));

    // 添加到成员列表
    m_MemberList[pMemberInfo->MemberIdx] = pInfo;

    // 设置职位成员索引（特殊职位：副会长、长老）
    // GUILD_MASTER(0)是会长，不需要在这里处理
    // GUILD_MEMBER(3)和GUILD_STUDENT(4)是普通职位，不需要特殊处理
    if (pInfo->Rank <= GUILD_SENIOR) {  // 0, 1, 2需要处理
        if (pInfo->Rank == GUILD_VICEMASTER) {
            m_RankMemberIdx[0] = pInfo->MemberIdx;
        } else if (pInfo->Rank == GUILD_SENIOR) {
            int pos = IsVacancy(GUILD_SENIOR);
            if ((pos < 0) || (pos >= eRankPos_Max)) {
                spdlog::error("[Guild] AddMember: Invalid senior position! (pos={})", pos);
                return true;
            }
            m_RankMemberIdx[pos] = pInfo->MemberIdx;
        }
    }

    // 学徒计数
    if (pInfo->Rank == GUILD_STUDENT) {
        m_nStudentCount++;
    }

    spdlog::info("[Guild] Added member: {} (ID={}, rank={})",
                pInfo->MemberName, pInfo->MemberIdx, pInfo->Rank);

    return true;
}

bool Guild::DeleteMember(uint32_t playerIDX, uint8_t bType)
{
    auto it = m_MemberList.find(playerIDX);
    if (it == m_MemberList.end()) {
        spdlog::warn("[Guild] DeleteMember: Member {} not found", playerIDX);
        return false;
    }

    GUILDMEMBERINFO* pInfo = it->second;

    // 学徒计数
    if (pInfo->Rank == GUILD_STUDENT) {
        m_nStudentCount--;
    }

    // 重置职位成员信息
    ResetRankMemberInfo(playerIDX, pInfo->Rank);

    // 删除
    delete pInfo;
    m_MemberList.erase(it);

    spdlog::info("[Guild] Deleted member: {} (ID={})", playerIDX, bType);

    return true;
}

bool Guild::ChangeMemberRank(uint32_t memberIdx, uint8_t toRank)
{
    auto it = m_MemberList.find(memberIdx);
    if (it == m_MemberList.end()) {
        spdlog::error("[Guild] ChangeMemberRank: Member {} not found", memberIdx);
        return false;
    }

    GUILDMEMBERINFO* pInfo = it->second;
    uint8_t fromRank = pInfo->Rank;

    // 重置旧职位
    ResetRankMemberInfo(memberIdx, fromRank);

    // 设置新职位
    pInfo->Rank = toRank;

    // 设置新职位的索引（特殊职位：副会长、长老）
    if (toRank <= GUILD_SENIOR) {  // 0, 1, 2需要处理
        if (toRank == GUILD_VICEMASTER) {
            m_RankMemberIdx[0] = memberIdx;
        } else if (toRank == GUILD_SENIOR) {
            int pos = IsVacancy(GUILD_SENIOR);
            if ((pos < 0) || (pos >= eRankPos_Max)) {
                spdlog::error("[Guild] ChangeMemberRank: No vacancy for senior!");
                return false;
            }
            m_RankMemberIdx[pos] = memberIdx;
        }
    }

    // 学徒计数
    if (toRank == GUILD_STUDENT) {
        m_nStudentCount++;
    } else if (fromRank == GUILD_STUDENT) {
        m_nStudentCount--;
    }

    spdlog::info("[Guild] Changed member {} rank: {} -> {}", memberIdx, fromRank, toRank);

    return true;
}

bool Guild::DoChangeMemberRank(uint32_t memberIdx, int pos, uint8_t rank)
{
    auto it = m_MemberList.find(memberIdx);
    if (it == m_MemberList.end()) {
        return false;
    }

    GUILDMEMBERINFO* pInfo = it->second;
    pInfo->Rank = rank;

    if (rank == GUILD_VICEMASTER) {
        m_RankMemberIdx[0] = memberIdx;
    } else if (rank == GUILD_SENIOR) {
        m_RankMemberIdx[pos] = memberIdx;
    }

    return true;
}

void Guild::ResetRankMemberInfo(uint32_t memberIdx, uint8_t rank)
{
    if (rank == GUILD_VICEMASTER) {
        if (m_RankMemberIdx[0] == memberIdx) {
            m_RankMemberIdx[0] = 0;
        }
    } else if (rank == GUILD_SENIOR) {
        for (int i = 1; i < eRankPos_Max; ++i) {  // 从1开始，跳过副会长位置
            if (m_RankMemberIdx[i] == memberIdx) {
                m_RankMemberIdx[i] = 0;
                break;
            }
        }
    }
}

int Guild::IsVacancy(uint8_t toRank)
{
    if (toRank == GUILD_VICEMASTER) {
        return (m_RankMemberIdx[0] == 0) ? eRankPos_1 : eRankPos_Err;
    } else if (toRank == GUILD_SENIOR) {
        for (int i = 1; i < eRankPos_Max; ++i) {
            if (m_RankMemberIdx[i] == 0) {
                return i;
            }
        }
        return eRankPos_Err;
    }
    return eRankPos_Err;
}

bool Guild::CanAddMember()
{
    // MAX_GUILDMEMBER_NUM包括会长，m_MemberList不包括会长，所以限制是MAX_GUILDMEMBER_NUM-1
    return m_MemberList.size() < (MAX_GUILDMEMBER_NUM - 1);
}

// ========== 成员列表 ==========

void Guild::GetTotalMember(GUILDMEMBERINFO* pRtInfo)
{
    if (!pRtInfo) {
        return;
    }

    int i = 0;
    for (auto& pair : m_MemberList) {
        if (i >= MAX_GUILDMEMBER_NUM) {
            break;
        }
        memcpy(&pRtInfo[i], pair.second, sizeof(GUILDMEMBERINFO));
        i++;
    }

    // 清空剩余位置
    for (; i < MAX_GUILDMEMBER_NUM; ++i) {
        memset(&pRtInfo[i], 0, sizeof(GUILDMEMBERINFO));
    }
}

GUILDMEMBERINFO* Guild::GetMemberInfo(uint32_t memberIdx)
{
    auto it = m_MemberList.find(memberIdx);
    if (it != m_MemberList.end()) {
        return it->second;
    }
    return nullptr;
}

// ========== 消息发送 ==========

void Guild::SendMsgToAll(const void* msg, int size)
{
    // TODO: 实现网络消息发送
    spdlog::debug("[Guild] Sending message to all members (ID={})", m_GuildInfo.GuildIdx);
}

void Guild::SendMsgToAllExceptOne(const void* msg, int size, uint32_t characterIdx)
{
    // TODO: 实现网络消息发送
    spdlog::debug("[Guild] Sending message to all except {} (ID={})", characterIdx, m_GuildInfo.GuildIdx);
}

void Guild::SendDeleteMsg(uint32_t playerIDX, uint8_t bType)
{
    // TODO: 构造并发送删除成员消息
    spdlog::info("[Guild] Send delete msg: member={}, type={}", playerIDX, bType);
}

void Guild::SendAddMsg(GUILDMEMBERINFO* pInfo, uint32_t characterIDX)
{
    // TODO: 构造并发送添加成员消息
    if (pInfo) {
        spdlog::info("[Guild] Send add msg: member={}", pInfo->MemberName);
    }
}

// ========== 公会操作 ==========

void Guild::BreakUp()
{
    spdlog::info("[Guild] Breaking up guild ID={}", m_GuildInfo.GuildIdx);

    // 清理所有成员
    for (auto& pair : m_MemberList) {
        delete pair.second;
    }
    m_MemberList.clear();
}

void Guild::LevelUp()
{
    if (m_GuildInfo.GuildLevel < 255) {
        m_GuildInfo.GuildLevel++;
        spdlog::info("[Guild] Guild leveled up: {} (ID={})",
                    m_GuildInfo.GuildLevel, m_GuildInfo.GuildIdx);
    }
}

void Guild::LevelDown()
{
    if (m_GuildInfo.GuildLevel > 1) {
        m_GuildInfo.GuildLevel--;
        spdlog::info("[Guild] Guild leveled down: {} (ID={})",
                    m_GuildInfo.GuildLevel, m_GuildInfo.GuildIdx);
    }
}

// ========== 公会公告 ==========

void Guild::SetGuildNotice(const char* pNotice)
{
    if (pNotice) {
        strncpy(m_GuildInfo.GuildNotice, pNotice, MAX_GUILD_NOTICE);
        m_GuildInfo.GuildNotice[MAX_GUILD_NOTICE] = '\0';
        spdlog::debug("[Guild] Set notice: {}", m_GuildInfo.GuildNotice);
    }
}

// ========== 学徒系统 ==========

bool Guild::CanAddStudent()
{
    return m_nStudentCount < GUILD_STUDENT_NUM_MAX;
}

// ========== 公会积分系统 ==========

void Guild::AddHuntedMonsterCount(int addCount)
{
    m_GuildPoint.GuildHuntedMonsterCount += addCount;
    spdlog::debug("[Guild] Hunted monster count: {}", m_GuildPoint.GuildHuntedMonsterCount);
}

void Guild::SetHuntedMonsterTotalCountInfo(int setTotal, uint32_t dBProcessTime)
{
    m_GuildPoint.GuildHuntedMonsterTotalCount = setTotal;
    m_GuildPoint.DBProcessTime = dBProcessTime;
    spdlog::debug("[Guild] Total hunted count: {}", setTotal);
}

void Guild::InitGuildPointInfo(GUILDPOINT_INFO* pGuildPointInfo)
{
    if (pGuildPointInfo) {
        memcpy(&m_GuildPoint, pGuildPointInfo, sizeof(GUILDPOINT_INFO));
        spdlog::info("[Guild] Initialized guild point: {}", m_GuildPoint.GuildPoint);
    }
}

void Guild::SetGuildPoint(int totalPoint)
{
    m_GuildPoint.GuildPoint = totalPoint;
    spdlog::debug("[Guild] Set guild point: {}", totalPoint);
}

// ========== 其他方法 ==========

void Guild::SetMemberLevel(uint32_t playerIdx, uint16_t playerLvl)
{
    auto it = m_MemberList.find(playerIdx);
    if (it != m_MemberList.end()) {
        it->second->Memberlvl = playerLvl;
        spdlog::debug("[Guild] Set member {} level to {}", playerIdx, playerLvl);
    }
}

void Guild::SetLogInfo(uint32_t playerIdx, bool vals)
{
    auto it = m_MemberList.find(playerIdx);
    if (it != m_MemberList.end()) {
        it->second->bLogged = vals;
        spdlog::debug("[Guild] Set member {} logged: {}", playerIdx, vals);
    }
}

} // namespace Game
} // namespace Murim
