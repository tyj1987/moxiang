// MurimServer - Party System (Legacy Compatible)
// 组队系统 - 完全匹配Legacy实现
// 对应 legacy: legacy-source-code\[Server]Map\Party.cpp, Party.h

#pragma once

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include "../../core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

// ========== 常量定义（匹配Legacy） ==========

#define MAX_NAME_LENGTH 16
#define MAX_PARTY_LISTNUM 7
#define MAX_PARTY_NAME 28
#define MAX_CHAT_LENGTH 128

// ========== 枚举定义（匹配Legacy） ==========

/**
 * @brief 分配方式（对应Legacy的ePartyOpt）
 */
enum PartyOption : uint8_t {
    ePartyOpt_Random = 0,    // 随机分配
    ePartyOpt_Damage = 1,    // 伤害分配
    ePartyOpt_Sequence = 2,  // 顺序分配
};

// ========== 结构体定义（完全匹配Legacy） ==========

/**
 * @brief 队伍成员信息（完全匹配Legacy的PARTYMEMBER）
 * 对应 legacy: legacy-source-code\[Server]Map\Party.h:21-67
 */
struct PartyMember {
    char Name[MAX_NAME_LENGTH + 1];  // 角色名称
    uint64_t MemberID;                // 角色ID
    bool bLogged;                     // 是否登录
    uint8_t LifePercent;              // 生命值百分比 (0-100)
    uint8_t ShieldPercent;            // 护盾百分比 (0-100)
    uint8_t NaeRyukPercent;           // 内力百分比 (0-100)
    uint16_t Level;                   // 等级

    /**
     * @brief 构造函数
     */
    PartyMember() {
        memset(this, 0, sizeof(PartyMember));
    }

    /**
     * @brief 设置成员信息（匹配Legacy的SetMember）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.h:33-54
     */
    void SetMember(uint64_t addMemberID, const char* strName, uint16_t lvl,
                   uint8_t lifePercent = 100, uint8_t shieldPercent = 0,
                   uint8_t naeRyukPercent = 0) {
        MemberID = addMemberID;
        // SafeStrCpy(this->Name, strName, MAX_NAME_LENGTH + 1);
        strncpy(this->Name, strName, MAX_NAME_LENGTH);
        this->Name[MAX_NAME_LENGTH] = '\0';

        Level = lvl;
        bLogged = true;
        LifePercent = lifePercent;
        ShieldPercent = shieldPercent;
        NaeRyukPercent = naeRyukPercent;
    }

    /**
     * @brief 设置等级
     */
    void SetLevel(uint16_t lvl) {
        Level = lvl;
    }
};

/**
 * @brief 队伍附加选项（完全匹配Legacy的PARTY_ADDOPTION）
 * 对应 legacy: legacy-source-code\[CC]Header\CommonStruct.h:656-665
 */
struct PartyAddOption {
    uint32_t dwPartyIndex;                    // 队伍索引
    char szTheme[MAX_PARTY_NAME + 1];        // 队伍主题/名称
    uint16_t wMinLevel;                       // 最低等级
    uint16_t wMaxLevel;                       // 最高等级
    uint8_t bPublic;                          // 公开/非公开 (0=非公开, 1=公开)
    uint16_t wLimitCount;                     // 限制人数
    uint8_t bOption;                          // 分配方式

    PartyAddOption() {
        memset(this, 0, sizeof(PartyAddOption));
        bOption = ePartyOpt_Random;  // 默认随机分配
    }
};

/**
 * @brief 队伍信息（用于网络消息）
 * 对应 legacy: legacy-source-code\[CC]Header\CommonStruct.h:674-681
 */
struct PARTY_INFO {
    uint32_t PartyDBIdx;                      // 队伍数据库ID
    PartyMember Member[MAX_PARTY_LISTNUM];    // 成员列表
    uint32_t dwMemberID[MAX_PARTY_LISTNUM];   // 成员ID列表
    char szMasterName[MAX_NAME_LENGTH + 1];   // 队长名称
    PartyAddOption PartyAddOption;            // 队伍附加选项
};

/**
 * @brief 队伍成员信息（用于网络消息）
 */
struct PARTY_MEMBER {
    uint64_t dwMemberID;          // 成员ID
    char Name[MAX_NAME_LENGTH + 1];
    uint16_t Level;               // 等级
    uint8_t LifePercent;          // 生命值百分比
    uint8_t ShieldPercent;        // 护盾百分比
    uint8_t NaeRyukPercent;       // 内力百分比
    bool bLogged;                 // 是否登录
};

// ========== Party类（完全匹配Legacy的CParty） ==========

/**
 * @brief 队伍类（完全匹配Legacy的CParty）
 * 对应 legacy: legacy-source-code\[Server]Map\Party.h:69-174
 */
class Party {
public:
    // ========== 构造函数（为测试支持设为public） ==========

    /**
     * @brief 构造函数（匹配Legacy）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.cpp:46-77
     */
    Party(uint32_t id, uint64_t masterID, const char* strMasterName,
          uint16_t masterLevel, const PartyAddOption* pAddOption);

    /**
     * @brief 析构函数
     */
    virtual ~Party();

private:
    // 决策时间常量（10秒）
    enum {
        eDICISION_TIME = 10000,
    };

    // ========== 核心属性（完全匹配Legacy） ==========

    uint32_t m_PartyIDx;                               // 队伍ID
    PartyMember m_Member[MAX_PARTY_LISTNUM];           // 成员数组（固定7个）
    uint32_t m_TacticObjectID;                         // 战术对象ID
    uint8_t m_Option;                                  // 分配方式
    uint32_t m_OldSendtime;                            // 上次发送时间
    PartyAddOption m_AddPotion;                        // 附加选项
    uint64_t m_dwRequestPlayerID;                      // 请求玩家ID
    uint64_t m_dwRequestProcessTime;                   // 请求处理时间（10秒超时）
    bool m_MasterChanging;                             // 队长是否正在变更

public:

    // ========== 成员管理（完全匹配Legacy） ==========

    /**
     * @brief 添加成员（匹配Legacy的AddPartyMember）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.cpp:112-135
     */
    bool AddPartyMember(uint64_t addMemberID, const char* name, uint16_t lvl,
                       uint8_t lifePercent = 100, uint8_t shieldPercent = 0,
                       uint8_t naeRyukPercent = 0);

    /**
     * @brief 移除成员（匹配Legacy的RemovePartyMember）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.cpp:137-154
     */
    bool RemovePartyMember(uint64_t memberID);

    /**
     * @brief 转让队长（匹配Legacy的ChangeMaster）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.cpp:156-172
     */
    bool ChangeMaster(uint64_t fromID, uint64_t toID);

    /**
     * @brief 检查是否为成员（匹配Legacy的IsPartyMember）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.cpp:174-179
     */
    bool IsPartyMember(uint64_t playerID) const;

    /**
     * @brief 检查成员是否登录（匹配Legacy的IsMemberLogIn）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.cpp:181-184
     */
    bool IsMemberLogIn(int n) const;

    // ========== 登录/登出处理（完全匹配Legacy） ==========

    /**
     * @brief 成员登录（匹配Legacy的UserLogIn）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.cpp:186-215
     */
    void UserLogIn(uint64_t memberID, const char* name, uint16_t lvl,
                   bool bNotifyUserLogin = true);

    /**
     * @brief 成员登出（匹配Legacy的UserLogOut）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.cpp:223-235
     */
    void UserLogOut(uint64_t playerID);

    /**
     * @brief 发送登出消息（匹配Legacy的SendMsgUserLogOut）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.cpp:237-245
     */
    void SendMsgUserLogOut(uint64_t playerID);

    // ========== 消息发送（完全匹配Legacy） ==========

    /**
     * @brief 发送队伍信息（匹配Legacy的SendPartyInfo）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.cpp:89-110
     */
    void SendPartyInfo(uint64_t toMemberID);

    /**
     * @brief 发送消息给所有成员（匹配Legacy的SendMsgToAll）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.cpp:247-262
     */
    void SendMsgToAll(const void* msg, int size);

    /**
     * @brief 发送消息给除一人外的所有成员（匹配Legacy的SendMsgExceptOne）
     * 对应 legacy: legacy-source-code\[Server]Map\Party.cpp:264-281
     */
    void SendMsgExceptOne(const void* msg, int size, uint64_t playerID);

    // ========== 获取器（完全匹配Legacy） ==========

    /**
     * @brief 获取队伍ID
     */
    uint32_t GetPartyIdx() const { return m_PartyIDx; }

    /**
     * @brief 获取成员ID（匹配Legacy的GetMemberID）
     */
    uint64_t GetMemberID(int n) const { return m_Member[n].MemberID; }

    /**
     * @brief 获取队长ID（匹配Legacy的GetMasterID）
     */
    uint64_t GetMasterID() const { return m_Member[0].MemberID; }

    /**
     * @brief 获取成员名称（匹配Legacy的GetMemberName）
     */
    char* GetMemberName(uint64_t memberID);

    /**
     * @brief 获取第一个在线成员ID（匹配Legacy的GetFirstOnlineMemberID）
     */
    uint64_t GetFirstOnlineMemberID();

    /**
     * @brief 获取成员数量
     */
    uint8_t GetMemberNum();

    // ========== 战术对象（匹配Legacy） ==========

    /**
     * @brief 设置战术对象ID
     */
    void SetTacticObjectID(uint32_t tacticObjID) { m_TacticObjectID = tacticObjID; }

    /**
     * @brief 获取战术对象ID
     */
    uint32_t GetTacticObjectID() const { return m_TacticObjectID; }

    // ========== 队长变更（匹配Legacy） ==========

    /**
     * @brief 设置队长是否正在变更
     */
    void SetMasterChanging(bool val) { m_MasterChanging = val; }

    /**
     * @brief 检查队长是否正在变更（匹配Legacy的IsMasterChanging）
     */
    bool IsMasterChanging(uint64_t playerID, uint8_t protocol);

    // ========== 附加选项（匹配Legacy） ==========

    /**
     * @brief 设置附加选项（匹配Legacy的SetAddOption）
     */
    void SetAddOption(const PartyAddOption* pAddOption);

    /**
     * @brief 获取附加选项（匹配Legacy的GetAddOption）
     */
    const PartyAddOption* GetAddOption() const { return &m_AddPotion; }

    // ========== 分配方式（匹配Legacy） ==========

    /**
     * @brief 设置分配方式（匹配Legacy的SetOption）
     */
    void SetOption(uint8_t option) { m_Option = option; }

    /**
     * @brief 获取分配方式（匹配Legacy的GetOption）
     */
    uint8_t GetOption() const { return m_Option; }

    // ========== 请求处理（匹配Legacy） ==========

    /**
     * @brief 设置请求玩家ID
     */
    void SetRequestPlayerID(uint64_t playerID) { m_dwRequestPlayerID = playerID; }

    /**
     * @brief 获取请求玩家ID
     */
    uint64_t GetRequestPlayerID() const { return m_dwRequestPlayerID; }

    /**
     * @brief 开始请求处理时间（匹配Legacy的StartRequestProcessTime）
     */
    void StartRequestProcessTime();

    /**
     * @brief 初始化请求时间（匹配Legacy的InitRequestTime）
     */
    void InitRequestTime();

    // ========== 其他方法（匹配Legacy） ==========

    /**
     * @brief 设置成员信息（匹配Legacy的SetMemberInfo）
     */
    int SetMemberInfo(uint64_t memberID, const char* strName, uint16_t lvl, bool bLog);

    /**
     * @brief 设置成员等级（匹配Legacy的SetMemberLevel）
     */
    void SetMemberLevel(uint64_t playerID, uint16_t lvl);

    /**
     * @brief 解散队伍（匹配Legacy的BreakUp）
     */
    void BreakUp();

    /**
     * @brief 获取成员信息（匹配Legacy的GetMemberInfo）
     */
    void GetMemberInfo(int n, PARTY_MEMBER* pRtInfo);

    /**
     * @brief 获取成员数组
     */
    PartyMember* GetPartyMember() { return m_Member; }

    /**
     * @brief 处理（匹配Legacy的Process）
     */
    void Process();

    // 友元类
    friend class PartyManager;
};

// ========== PartyManager类（完全匹配Legacy的CPartyManager） ==========

/**
 * @brief 组队管理器（完全匹配Legacy的CPartyManager）
 * 对应 legacy: legacy-source-code\[Server]Map\PartyManager.h
 */
class PartyManager {
private:
    // 哈希表存储（匹配Legacy的CHashTable<CParty*>）
    std::unordered_map<uint32_t, Party*> m_partyRoomHashTable;  // party_id -> Party*

    // 角色到队伍的映射
    std::unordered_map<uint64_t, uint32_t> m_character_to_party_;  // character_id -> party_id

    // 队伍ID生成器
    uint32_t m_nextPartyID;

    /**
     * @brief 构造函数（私有，单例模式）
     */
    PartyManager();

    /**
     * @brief 析构函数
     */
    ~PartyManager();

public:
    /**
     * @brief 获取单例实例（匹配Legacy的GETINSTANCE）
     */
    static PartyManager& Instance();

    /**
     * @brief 初始化
     */
    void Initialize();

    // ========== 队伍注册（完全匹配Legacy） ==========

    /**
     * @brief 注册队伍（匹配Legacy的RegistParty）
     * 对应 legacy: legacy-source-code\[Server]Map\PartyManager.cpp:26-62
     */
    Party* RegistParty(uint32_t partyID);

    // ========== 队伍创建（完全匹配Legacy） ==========

    /**
     * @brief 创建队伍请求（匹配Legacy的CreatePartyQuery）
     * 对应 legacy: legacy-source-code\[Server]Map\PartyManager.cpp:120-158
     */
    void CreatePartyQuery(uint64_t masterID, const PartyAddOption* pAddOption);

    // ========== 队伍邀请（完全匹配Legacy） ==========

    /**
     * @brief 添加邀请（匹配Legacy的AddPartyInvite）
     * 对应 legacy: legacy-source-code\[Server]Map\PartyManager.cpp:160-200
     */
    void AddPartyInvite(uint64_t masterID, uint64_t targetID);

    // ========== 成员操作（完全匹配Legacy） ==========

    /**
     * @brief 删除成员（匹配Legacy的DelMember）
     */
    void DelMember(uint64_t memberID, uint32_t partyID);

    /**
     * @brief 解散队伍（匹配Legacy的BreakupParty）
     */
    void BreakupParty(uint32_t partyID, uint64_t masterID);

    // ========== 队伍查询（完全匹配Legacy） ==========

    /**
     * @brief 获取队伍
     */
    Party* GetParty(uint32_t partyID);

    /**
     * @brief 获取队伍（const版本）
     */
    const Party* GetParty(uint32_t partyID) const;

    /**
     * @brief 通过成员获取队伍
     */
    Party* GetPartyByMember(uint64_t characterID);

    // ========== 统计 ==========

    /**
     * @brief 获取队伍数量
     */
    size_t GetPartyCount() const { return m_partyRoomHashTable.size(); }
};

} // namespace Game
} // namespace Murim
