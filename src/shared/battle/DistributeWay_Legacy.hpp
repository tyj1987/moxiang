// MurimServer - Distribute System (Legacy Compatible)
// 物品/经验分配系统 - 完全匹配Legacy实现
// 对应 legacy: legacy-source-code\[Server]Map\DistributeWay.h/cpp

#pragma once

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <vector>
#include "../../core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

// ========== 常量定义（匹配Legacy） ==========

#define MAX_NAME_LENGTH 16
#define ITEM_LEVEL_CHECK_RANGE 10      // 物品等级检查范围
#define PARTY_MEMBER_BONUS 0.1f        // 每个成员加成10%
#define LEVEL_DIFF_PENALTY 10          // 等级差惩罚阈值
#define LEVEL_DIFF_REDUCTION 0.5f      // 等级差惩罚减半

// ========== 前向声明 ==========

class CPlayer;

// ========== 数据结构（匹配Legacy） ==========

/**
 * @brief 队伍接收成员信息（用于分配）
 */
struct PARTY_RECEIVE_MEMBER {
    uint64_t characterID;      // 角色ID
    char name[MAX_NAME_LENGTH + 1];
    uint16_t level;            // 等级
    uint32_t damage;           // 造成伤害（伤害分配用）
    float exp;                 // 获得经验
    uint8_t lifePercent;       // 生命值百分比
    uint8_t naeRyukPercent;    // 内力百分比
    bool bLogged;             // 是否登录

    PARTY_RECEIVE_MEMBER() {
        memset(this, 0, sizeof(PARTY_RECEIVE_MEMBER));
    }
};

/**
 * @brief 大随机数生成器（匹配Legacy的cRand_Big）
 * 对应 legacy: legacy-source-code\[Server]Map\DistributeWay.h:16-26
 */
class cRand_Big {
private:
    int mRangeVal;

public:
    cRand_Big() : mRangeVal(0) {
        srand(static_cast<uint32_t>(time(nullptr)));
    }

    ~cRand_Big() = default;

    /**
     * @brief 生成随机值
     */
    uint32_t RandomVal() {
        return static_cast<uint32_t>(rand());
    }
};

// ========== CDistributeWay基类（完全匹配Legacy） ==========

/**
 * @brief 分配方式基类（完全匹配Legacy的CDistributeWay）
 * 对应 legacy: legacy-source-code\[Server]Map\DistributeWay.h:29-54
 */
class CDistributeWay {
protected:
    cRand_Big mBigRandObj;

    /**
     * @brief 检查是否可以获得物品（等级检查）
     */
    bool ChkGetItemLevel(uint16_t playerLvl, uint16_t monsterLvl);

public:
    CDistributeWay() = default;
    virtual ~CDistributeWay() = default;

    /**
     * @brief 计算并分配能力值/经验（匹配Legacy的CalcAbilandSend）
     */
    virtual void CalcAbilandSend(uint16_t monsterLevel, PARTY_RECEIVE_MEMBER* pMemberInfo, uint16_t maxLevel);

    /**
     * @brief 发送物品（匹配Legacy的SendItem）
     */
    virtual void SendItem(PARTY_RECEIVE_MEMBER* pRealMember, uint16_t dropItemID, uint32_t dropItemRatio,
                        void* pMonInfo, uint16_t monsterKind, uint16_t maxLevel);

    /**
     * @brief 发送个人经验
     */
    void SendToPersonalExp(CPlayer* pReceivePlayer, uint32_t exp);

    /**
     * @brief 发送个人能力值
     */
    void SendToPersonalAbil(CPlayer* pReceivePlayer, uint16_t monsterLevel);

    /**
     * @brief 发送个人金钱
     */
    void SendToPersonalMoney(CPlayer* pPlayer, uint32_t money, uint16_t monsterKind);

    /**
     * @brief 发送个人物品
     */
    void SendToPersonalItem(CPlayer* pPlayer, uint16_t dropItemID, uint32_t dropItemRatio,
                           void* pMonInfo, uint16_t monsterKind);

    /**
     * @brief 计算获得能力值经验（匹配Legacy的CalcObtainAbilityExp）
     */
    uint32_t CalcObtainAbilityExp(uint16_t killerLevel, uint16_t monsterLevel);

    /**
     * @brief 物品等级变化
     */
    void ItemChangeAtLv(uint16_t& dropItemID);
};

// ========== CDistribute_Random（随机分配） ==========

/**
 * @brief 随机分配类（完全匹配Legacy的CDistribute_Random）
 * 对应 legacy: legacy-source-code\[Server]Map\Distribute_Random.h:16-26
 */
class CDistribute_Random : public CDistributeWay {
private:
    CDistribute_Random() = default;

public:
    /**
     * @brief 获取单例实例
     */
    static CDistribute_Random& GetInstance() {
        static CDistribute_Random instance;
        return instance;
    }

    /**
     * @brief 发送物品（随机分配实现）
     */
    virtual void SendItem(PARTY_RECEIVE_MEMBER* pRealMember, uint16_t dropItemID, uint32_t dropItemRatio,
                        void* pMonInfo, uint16_t monsterKind, uint16_t maxLevel) override;
};

// ========== CDistribute_Damage（伤害分配） ==========

/**
 * @brief 伤害分配类（完全匹配Legacy的CDistribute_Damage）
 * 对应 legacy: legacy-source-code\[Server]Map\Distribute_Damage.h:16-26
 */
class CDistribute_Damage : public CDistributeWay {
private:
    CDistribute_Damage() = default;

public:
    /**
     * @brief 获取单例实例
     */
    static CDistribute_Damage& GetInstance() {
        static CDistribute_Damage instance;
        return instance;
    }

    /**
     * @brief 发送物品（伤害分配实现）
     */
    virtual void SendItem(PARTY_RECEIVE_MEMBER* pRealMember, uint16_t dropItemID, uint32_t dropItemRatio,
                        void* pMonInfo, uint16_t monsterKind, uint16_t maxLevel) override;
};

} // namespace Game
} // namespace Murim
