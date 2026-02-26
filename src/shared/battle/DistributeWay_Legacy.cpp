// MurimServer - Distribute System Implementation (Legacy Compatible)
// 物品/经验分配系统实现 - 完全匹配Legacy实现
// 对应 legacy: legacy-source-code\[Server]Map\DistributeWay.cpp

#include "DistributeWay_Legacy.hpp"
#include <algorithm>

namespace Murim {
namespace Game {

// ========== CDistributeWay基类实现 ==========

bool CDistributeWay::ChkGetItemLevel(uint16_t playerLvl, uint16_t monsterLvl)
{
    // 如果玩家等级比怪物等级高10级以上，无法获得物品
    // 对应 legacy的物品等级检查逻辑
    if (playerLvl > monsterLvl + ITEM_LEVEL_CHECK_RANGE) {
        return false;
    }
    return true;
}

void CDistributeWay::CalcAbilandSend(uint16_t monsterLevel, PARTY_RECEIVE_MEMBER* pMemberInfo,
                                     uint16_t maxLevel)
{
    if (!pMemberInfo) {
        spdlog::error("[DistributeWay] CalcAbilandSend: pMemberInfo is null!");
        return;
    }

    // 获取成员数量（假设最多7个）
    uint8_t memberCount = 0;
    for (int i = 0; i < 7; ++i) {
        if (pMemberInfo[i].bLogged) {
            memberCount++;
        }
    }

    if (memberCount == 0) {
        spdlog::warn("[DistributeWay] CalcAbilandSend: No logged members!");
        return;
    }

    // 队伍加成：1.0 + 0.1 * (成员数 - 1)
    // 对应 legacy的队伍加成公式
    float fPartyBonus = 1.0f + PARTY_MEMBER_BONUS * (memberCount - 1);

    // 分配经验给每个成员
    for (int i = 0; i < 7; ++i) {
        if (!pMemberInfo[i].bLogged) {
            continue;
        }

        // 每个成员获得的队伍经验（平均分配）
        float fMemberExp = 100.0f * fPartyBonus / memberCount;

        // 等级惩罚
        int nLevelDiff = static_cast<int>(pMemberInfo[i].level) - static_cast<int>(monsterLevel);
        if (abs(nLevelDiff) > LEVEL_DIFF_PENALTY) {
            fMemberExp *= LEVEL_DIFF_REDUCTION;  // 经验减半
        }

        // 设置经验
        pMemberInfo[i].exp = fMemberExp;

        spdlog::debug("[DistributeWay] Member {} (Lv{}) gets {} exp from monster (Lv{})",
                    pMemberInfo[i].name, pMemberInfo[i].level, fMemberExp, monsterLevel);
    }
}

void CDistributeWay::SendItem(PARTY_RECEIVE_MEMBER* pRealMember, uint16_t dropItemID,
                             uint32_t dropItemRatio, void* pMonInfo, uint16_t monsterKind,
                             uint16_t maxLevel)
{
    // 基类默认实现：随机分配
    spdlog::debug("[DistributeWay] SendItem: Base implementation (random distribution)");
}

void CDistributeWay::SendToPersonalExp(CPlayer* pReceivePlayer, uint32_t exp)
{
    // TODO: 实现发送经验给玩家
    spdlog::debug("[DistributeWay] SendToPersonalExp: player gets {} exp", exp);
}

void CDistributeWay::SendToPersonalAbil(CPlayer* pReceivePlayer, uint16_t monsterLevel)
{
    // TODO: 实现发送能力值给玩家
    spdlog::debug("[DistributeWay] SendToPersonalAbil: monster level {}", monsterLevel);
}

void CDistributeWay::SendToPersonalMoney(CPlayer* pPlayer, uint32_t money, uint16_t monsterKind)
{
    // TODO: 实现发送金钱给玩家
    spdlog::debug("[DistributeWay] SendToPersonalMoney: {} money", money);
}

void CDistributeWay::SendToPersonalItem(CPlayer* pPlayer, uint16_t dropItemID,
                                      uint32_t dropItemRatio, void* pMonInfo, uint16_t monsterKind)
{
    // TODO: 实现发送物品给玩家
    spdlog::debug("[DistributeWay] SendToPersonalItem: item {}", dropItemID);
}

uint32_t CDistributeWay::CalcObtainAbilityExp(uint16_t killerLevel, uint16_t monsterLevel)
{
    // 基础经验：怪物等级 * 100
    uint32_t baseExp = monsterLevel * 100;

    // 等级调整
    int nLevelDiff = static_cast<int>(killerLevel) - static_cast<int>(monsterLevel);

    if (nLevelDiff > 0) {
        // 玩家等级高，经验减少
        baseExp = static_cast<uint32_t>(baseExp / (1.0f + nLevelDiff * 0.1f));
    } else if (nLevelDiff < 0) {
        // 玩家等级低，经验增加
        baseExp = static_cast<uint32_t>(baseExp * (1.0f - nLevelDiff * 0.1f));
    }

    spdlog::debug("[DistributeWay] CalcObtainAbilityExp: killer Lv={} vs monster Lv={} => {} exp",
                killerLevel, monsterLevel, baseExp);

    return baseExp;
}

void CDistributeWay::ItemChangeAtLv(uint16_t& dropItemID)
{
    // TODO: 根据等级改变掉落物品
    spdlog::debug("[DistributeWay] ItemChangeAtLv: item {}", dropItemID);
}

// ========== CDistribute_Random实现 ==========

void CDistribute_Random::SendItem(PARTY_RECEIVE_MEMBER* pRealMember, uint16_t dropItemID,
                                  uint32_t dropItemRatio, void* pMonInfo, uint16_t monsterKind,
                                  uint16_t maxLevel)
{
    if (!pRealMember) {
        spdlog::error("[DistributeRandom] SendItem: pRealMember is null!");
        return;
    }

    // 检查哪些成员可以获得物品（等级检查+登录检查）
    std::vector<int> validMembers;
    for (int i = 0; i < 7; ++i) {
        if (pRealMember[i].bLogged && ChkGetItemLevel(pRealMember[i].level, maxLevel)) {
            validMembers.push_back(i);
        }
    }

    if (validMembers.empty()) {
        spdlog::warn("[DistributeRandom] No valid members for item {}", dropItemID);
        return;
    }

    // 随机选择一个幸运成员
    int nRandomIndex = mBigRandObj.RandomVal() % validMembers.size();
    int nLuckyMemberIndex = validMembers[nRandomIndex];

    // 发送物品给幸运成员
    // TODO: 实际发送物品
    spdlog::info("[DistributeRandom] Item {} goes to member {} (Lv{})",
               dropItemID, pRealMember[nLuckyMemberIndex].name, pRealMember[nLuckyMemberIndex].level);
}

// ========== CDistribute_Damage实现 ==========

void CDistribute_Damage::SendItem(PARTY_RECEIVE_MEMBER* pRealMember, uint16_t dropItemID,
                                   uint32_t dropItemRatio, void* pMonInfo, uint16_t monsterKind,
                                   uint16_t maxLevel)
{
    if (!pRealMember) {
        spdlog::error("[DistributeDamage] SendItem: pRealMember is null!");
        return;
    }

    // 计算总伤害
    uint32_t totalDamage = 0;
    for (int i = 0; i < 7; ++i) {
        if (pRealMember[i].bLogged) {
            totalDamage += pRealMember[i].damage;
        }
    }

    // 如果总伤害为0，回退到随机分配
    if (totalDamage == 0) {
        spdlog::debug("[DistributeDamage] Total damage is 0, falling back to random distribution");
        CDistribute_Random::GetInstance().SendItem(pRealMember, dropItemID, dropItemRatio,
                                                     pMonInfo, monsterKind, maxLevel);
        return;
    }

    // 计算每个成员的伤害占比
    std::vector<float> damageRatio(7, 0.0f);
    std::vector<int> loggedIndices;

    for (int i = 0; i < 7; ++i) {
        if (pRealMember[i].bLogged) {
            damageRatio[i] = static_cast<float>(pRealMember[i].damage) / totalDamage;
            loggedIndices.push_back(i);
        }
    }

    // 随机数（0-1）
    float fRandom = static_cast<float>(mBigRandObj.RandomVal()) / static_cast<float>(RAND_MAX);

    // 按伤害占比选择成员
    float fAccumulated = 0.0f;
    int nSelectedIndex = -1;

    for (int i = 0; i < 7; ++i) {
        if (!pRealMember[i].bLogged) {
            continue;
        }

        fAccumulated += damageRatio[i];
        if (fRandom <= fAccumulated) {
            nSelectedIndex = i;
            break;
        }
    }

    // 理论上不会到这里，但以防万一
    if (nSelectedIndex == -1 && !loggedIndices.empty()) {
        nSelectedIndex = loggedIndices.back();
    }

    // 发送物品给选中的成员
    if (nSelectedIndex >= 0 && nSelectedIndex < 7) {
        // TODO: 实际发送物品
        spdlog::info("[DistributeDamage] Item {} goes to member {} (Lv{}, damage ratio={:.2f})",
                   dropItemID, pRealMember[nSelectedIndex].name,
                   pRealMember[nSelectedIndex].level, damageRatio[nSelectedIndex]);
    }
}

} // namespace Game
} // namespace Murim
