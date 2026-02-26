// MurimServer - Guild System (Legacy Compatible)
// 公会系统 - 完全匹配Legacy实现
// 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp, Guild.h

#pragma once

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

// ========== 常量定义（匹配Legacy） ==========

#define MAX_NAME_LENGTH 16
#define MAX_GUILD_NAME 20
#define MAX_GUILD_NOTICE 150
#define MAX_GUILDMEMBER_NUM 50    // 基础版：30正式 + 20学徒
#define GUILD_STUDENT_NUM_MAX 25

// ========== 枚举定义（匹配Legacy） ==========

/**
 * @brief 公会职位（完全匹配Legacy）
 * 对应 legacy: legacy-source-code\[CC]Header\CommonStruct.h
 */
enum GuildPosition : uint8_t {
    GUILD_MASTER = 0,      // 会长
    GUILD_VICEMASTER = 1,  // 副会长（1个）
    GUILD_SENIOR = 2,      // 长老（2个）
    GUILD_MEMBER = 3,      // 成员
    GUILD_STUDENT = 4,     // 学徒（最多25个）
};

/**
 * @brief 职位位置（匹配Legacy的RTChangeRank）
 */
enum RTChangeRank {
    eRankPos_1 = 0,      // 副会长
    eRankPos_2 = 1,      // 长老1
    eRankPos_3 = 2,      // 长老2
    eRankPos_Max = 3,
    eRankPos_Err = 4,
};

/**
 * @brief 公会加成时间类型
 */
enum eGuildPlusTimeKind {
    eGPT_Kind_Max = 10,
};

// ========== 结构体定义（完全匹配Legacy） ==========

/**
 * @brief 公会信息（完全匹配Legacy的GUILDINFO）
 * 对应 legacy: legacy-source-code\[CC]Header\CommonStruct.h:3916-3931
 */
struct GUILDINFO {
    uint32_t GuildIdx;                        // 公会ID
    uint32_t MasterIdx;                       // 会长ID
    char MasterName[MAX_NAME_LENGTH + 1];     // 会长名称
    char GuildName[MAX_GUILD_NAME + 1];       // 公会名称
    char GuildNotice[MAX_GUILD_NOTICE + 1];   // 公会公告
    uint32_t UnionIdx;                        // 联盟ID
    uint32_t MapNum;                          // 地图编号
    uint8_t GuildLevel;                       // 公会等级
    uint32_t MarkName;                        // 公会标记
    uint8_t LvUpCounter;                      // 升级计数器
    bool bNeedMasterChecking;                 // 是否需要检查会长

    GUILDINFO() {
        memset(this, 0, sizeof(GUILDINFO));
    }
};

/**
 * @brief 公会成员信息（完全匹配Legacy的GUILDMEMBERINFO）
 * 对应 legacy: legacy-source-code\[CC]Header\CommonStruct.h:3933-3951
 */
struct GUILDMEMBERINFO {
    uint32_t MemberIdx;                       // 成员ID
    char MemberName[MAX_NAME_LENGTH + 1];    // 成员名称
    uint16_t Memberlvl;                       // 成员等级
    uint8_t Rank;                             // 职位
    bool bLogged;                            // 是否登录

    GUILDMEMBERINFO() {
        memset(this, 0, sizeof(GUILDMEMBERINFO));
    }

    /**
     * @brief 初始化信息（匹配Legacy的InitInfo）
     */
    void InitInfo(uint32_t playerIDX, const char* playerName, uint16_t playerLvl,
                 uint8_t playerRank = GUILD_MEMBER, bool bLogin = false) {
        MemberIdx = playerIDX;
        strncpy(MemberName, playerName, MAX_NAME_LENGTH);
        MemberName[MAX_NAME_LENGTH] = '\0';
        Memberlvl = playerLvl;
        Rank = playerRank;
        bLogged = bLogin;
    }
};

/**
 * @brief 公会使用中的加成时间信息
 */
struct GUILDUSING_PLUSTIME_INFO {
    uint32_t PlusTimeEndTime;  // 加成结束时间

    GUILDUSING_PLUSTIME_INFO() : PlusTimeEndTime(0) {}
};

/**
 * @brief 公会积分信息（完全匹配Legacy的GUILDPOINT_INFO）
 * 对应 legacy: legacy-source-code\[CC]Header\CommonGameDefine.h:3373-3388
 */
struct GUILDPOINT_INFO {
    GUILDPOINT_INFO() {
        memset(this, 0, sizeof(GUILDPOINT_INFO));
    }

    int GuildPoint;                          // 公会积分
    int GuildHuntedMonsterCount;              // 狩猎怪物计数
    int GuildHuntedMonsterTotalCount;         // 总狩猎怪物计数
    uint32_t DBProcessTime;                   // DB处理时间
    uint32_t GuildPlusTimeflg;                // 公会加成时间标志
    GUILDUSING_PLUSTIME_INFO GuildUsingPlusTimeInfo[eGPT_Kind_Max];
};

// ========== Guild类（完全匹配Legacy的CGuild） ==========

/**
 * @brief 公会类（完全匹配Legacy的CGuild）
 * 对应 legacy: legacy-source-code\[Server]Map\Guild.h:42-191
 */

class Guild {
private:
    // ========== 核心属性（完全匹配Legacy） ==========

    GUILDINFO m_GuildInfo;                                            // 公会信息

    // 成员存储（使用unordered_map替代Legacy的cPtrList）
    std::unordered_map<uint32_t, GUILDMEMBERINFO*> m_MemberList;  // MemberIdx -> MemberInfo

    uint32_t m_MarkName;                                   // 公会标记
    uint32_t m_RankMemberIdx[eRankPos_Max];                 // [0]=副会长ID, [1][2]=长老ID
    GUILDPOINT_INFO m_GuildPoint;                           // 公会积分系统
    uint32_t m_nMemberOnConnectingThisMap;                  // 当前地图在线成员数

    // 锦标赛
    uint32_t m_GTBattleID;                                  // 锦标赛战斗ID

    // 物品信息
    bool m_bItemInfoInited;                                 // 物品信息是否初始化
    bool m_bWaitingItemInfoFromDB;                          // 是否正在等待DB的物品信息

    // 学徒系统
    uint32_t m_nStudentCount;                               // 学徒数量

public:
    /**
     * @brief 构造函数（匹配Legacy）
     * 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp:17-49
     */
    Guild(GUILDINFO* pInfo, uint32_t guildMoney);

    /**
     * @brief 析构函数
     */
    virtual ~Guild();

    // ========== 权限检查（完全匹配Legacy） ==========

    /**
     * @brief 是否为会长（匹配Legacy的IsMaster）
     * 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp:66-72
     */
    bool IsMaster(uint32_t playerIDX) const;

    /**
     * @brief 是否为副会长（匹配Legacy的IsViceMaster）
     * 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp:74-80
     */
    bool IsViceMaster(uint32_t playerIDX) const;

    /**
     * @brief 是否为成员（匹配Legacy的IsMember）
     * 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp:257-278
     */
    bool IsMember(uint32_t memberIDX) const;

    // ========== 成员管理（完全匹配Legacy） ==========

    /**
     * @brief 添加成员（匹配Legacy的AddMember）
     * 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp:117-160
     */
    bool AddMember(GUILDMEMBERINFO* pMemberInfo);

    /**
     * @brief 删除成员（匹配Legacy的DeleteMember）
     * 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp:162-191
     */
    bool DeleteMember(uint32_t playerIDX, uint8_t bType);

    /**
     * @brief 修改成员职位（匹配Legacy的ChangeMemberRank）
     */
    bool ChangeMemberRank(uint32_t memberIdx, uint8_t toRank);

    /**
     * @brief 修改成员职位（具体实现）
     */
    bool DoChangeMemberRank(uint32_t memberIdx, int pos, uint8_t rank);

    /**
     * @brief 重置职位成员信息
     */
    void ResetRankMemberInfo(uint32_t memberIdx, uint8_t rank);

    /**
     * @brief 检查职位是否有空缺（匹配Legacy的IsVacancy）
     */
    int IsVacancy(uint8_t toRank);

    /**
     * @brief 是否可以添加成员（匹配Legacy的CanAddMember）
     */
    bool CanAddMember();

    // ========== 公会信息 ==========

    /**
     * @brief 获取公会ID
     */
    uint32_t GetIdx() const { return m_GuildInfo.GuildIdx; }

    /**
     * @brief 获取公会名称
     */
    char* GetGuildName() { return m_GuildInfo.GuildName; }

    /**
     * @brief 获取公会信息
     */
    GUILDINFO* GetGuildInfo() { return &m_GuildInfo; }

    /**
     * @brief 获取会长ID
     */
    uint32_t GetMasterIdx() const { return m_GuildInfo.MasterIdx; }

    // ========== 成员列表 ==========

    /**
     * @brief 获取成员数量
     */
    int GetMemberNum() const { return static_cast<int>(m_MemberList.size()); }

    /**
     * @brief 获取成员列表
     */
    std::unordered_map<uint32_t, GUILDMEMBERINFO*>* GetMemberList() {
        return &m_MemberList;
    }

    /**
     * @brief 获取所有成员（匹配Legacy的GetTotalMember）
     * 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp:285-301
     */
    void GetTotalMember(GUILDMEMBERINFO* pRtInfo);

    /**
     * @brief 获取成员信息
     */
    GUILDMEMBERINFO* GetMemberInfo(uint32_t memberIdx);

    // ========== 消息发送 ==========

    /**
     * @brief 发送消息给所有成员（匹配Legacy的SendMsgToAll）
     * 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp:214-233
     */
    void SendMsgToAll(const void* msg, int size);

    /**
     * @brief 发送消息给除一人外的所有成员（匹配Legacy的SendMsgToAllExceptOne）
     * 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp:235-255
     */
    void SendMsgToAllExceptOne(const void* msg, int size, uint32_t characterIdx);

    /**
     * @brief 发送删除成员消息（匹配Legacy的SendDeleteMsg）
     * 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp:193-201
     */
    void SendDeleteMsg(uint32_t playerIDX, uint8_t bType);

    /**
     * @brief 发送添加成员消息（匹配Legacy的SendAddMsg）
     * 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp:203-212
     */
    void SendAddMsg(GUILDMEMBERINFO* pInfo, uint32_t characterIDX);

    // ========== 公会操作 ==========

    /**
     * @brief 解散公会（匹配Legacy的BreakUp）
     * 对应 legacy: legacy-source-code\[Server]Map\Guild.cpp:82-114
     */
    void BreakUp();

    /**
     * @brief 公会升级（匹配Legacy的LevelUp）
     */
    void LevelUp();

    /**
     * @brief 公会降级（匹配Legacy的LevelDown）
     */
    void LevelDown();

    // ========== 公会公告 ==========

    /**
     * @brief 设置公会公告
     */
    void SetGuildNotice(const char* pNotice);

    // ========== 公会标记 ==========

    /**
     * @brief 获取公会标记
     */
    uint32_t GetMarkName() const { return m_MarkName; }

    /**
     * @brief 设置公会标记
     */
    void SetMarkName(uint32_t name) { m_MarkName = name; }

    // ========== 联盟系统 ==========

    /**
     * @brief 设置联盟ID
     */
    void SetGuildUnionIdx(uint32_t dwGuildUnionIdx) { m_GuildInfo.UnionIdx = dwGuildUnionIdx; }

    /**
     * @brief 获取联盟ID
     */
    uint32_t GetGuildUnionIdx() const { return m_GuildInfo.UnionIdx; }

    // ========== 地图编号 ==========

    /**
     * @brief 获取位置
     */
    uint32_t GetLocation() const { return m_GuildInfo.MapNum; }

    /**
     * @brief 设置位置
     */
    void SetLocation(uint32_t mapNum) { m_GuildInfo.MapNum = mapNum; }

    // ========== 学徒系统 ==========

    /**
     * @brief 获取学徒数量
     */
    uint32_t GetStudentNum() const { return m_nStudentCount; }

    /**
     * @brief 是否可以添加学徒
     */
    bool CanAddStudent();

    // ========== 公会积分系统 ==========

    /**
     * @brief 添加狩猎怪物计数
     */
    void AddHuntedMonsterCount(int addCount);

    /**
     * @brief 设置狩猎怪物计数
     */
    void SetHuntedMonsterCount(int setCount) { m_GuildPoint.GuildHuntedMonsterCount = setCount; }

    /**
     * @brief 获取狩猎怪物计数
     */
    int GetHuntedMonsterCount() const { return m_GuildPoint.GuildHuntedMonsterCount; }

    /**
     * @brief 设置狩猎怪物总计数信息
     */
    void SetHuntedMonsterTotalCountInfo(int setTotal, uint32_t dBProcessTime);

    /**
     * @brief 获取总狩猎计数
     */
    int GetHuntedMonsterTotalCount() const { return m_GuildPoint.GuildHuntedMonsterTotalCount; }

    /**
     * @brief 初始化公会积分信息
     */
    void InitGuildPointInfo(GUILDPOINT_INFO* pGuildPointInfo);

    /**
     * @brief 获取公会积分信息
     */
    GUILDPOINT_INFO* GetGuildPointInfo() { return &m_GuildPoint; }

    /**
     * @brief 设置公会积分
     */
    void SetGuildPoint(int totalPoint);

    /**
     * @brief 获取公会积分
     */
    int GetGuildPoint() const { return m_GuildPoint.GuildPoint; }

    // ========== 在线成员 ==========

    /**
     * @brief 添加在线成员计数
     */
    void AddConnectingMapMemberCount(int val) { m_nMemberOnConnectingThisMap += val; }

    /**
     * @brief 获取在线成员计数
     */
    int GetConnectingMapMemberCount() const { return m_nMemberOnConnectingThisMap; }

    // ========== 其他方法 ==========

    /**
     * @brief 设置成员等级
     */
    void SetMemberLevel(uint32_t playerIdx, uint16_t playerLvl);

    /**
     * @brief 设置登录信息
     */
    void SetLogInfo(uint32_t playerIdx, bool vals);

    // ========== 锦标赛 ==========

    /**
     * @brief 设置锦标赛战斗ID
     */
    void SetGTBattleID(uint32_t battleID) { m_GTBattleID = battleID; }

    /**
     * @brief 获取锦标赛战斗ID
     */
    uint32_t GetGTBattleID() const { return m_GTBattleID; }

    // ========== 物品信息 ==========

    /**
     * @brief 设置物品信息初始化
     */
    void SetItemInfoInited(bool bInit) { m_bItemInfoInited = bInit; }

    /**
     * @brief 物品信息是否初始化
     */
    bool IsItemInfoInited() const { return m_bItemInfoInited; }

    /**
     * @brief 设置等待物品信息
     */
    void SetWaitingItemInfoFromDB(bool bVal) { m_bWaitingItemInfoFromDB = bVal; }

    /**
     * @brief 是否等待物品信息
     */
    bool IsWaitingItemInfoFromDB() const { return m_bWaitingItemInfoFromDB; }
};

} // namespace Game
} // namespace Murim
