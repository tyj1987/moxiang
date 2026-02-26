#include "AttackCalculator.hpp"
#include "core/spdlog_wrapper.hpp"
#include "PlayerxMonsterPointTable.hpp"
#include <cstdlib>
#include <algorithm>
#include <cmath>

namespace Murim {
namespace Game {

// ========== 经验值计算 ==========

uint32_t AttackCalculator::GetPlayerPoint(uint16_t level, int level_gap) {
    // 对应: GetPlayerPoint(LEVELTYPE level1, int leve_gab)
    // Legacy 代码: return GAMERESRCMNGR->GetPLAYERxMONSTER_POINT(level1, leve_gab);

    // 常量定义（来自 Legacy CommonGameDefine.h）
    const int MAX_CHARACTER_LEVEL_NUM = 120;  // 最大等级
    const int MONSTERLEVELRESTRICT_LOWSTARTNUM = 6;
    const int MAX_MONSTERLEVELPOINTRESTRICT_NUM = 9;

    // 检查玩家是否满级
    if (level >= MAX_CHARACTER_LEVEL_NUM) {
        return 0;  // 满级不再获得点数
    }

    // 限制等级差范围
    if (level_gap < -MONSTERLEVELRESTRICT_LOWSTARTNUM) {
        level_gap = -MONSTERLEVELRESTRICT_LOWSTARTNUM;
    } else if (level_gap >= MAX_MONSTERLEVELPOINTRESTRICT_NUM) {
        level_gap = MAX_MONSTERLEVELPOINTRESTRICT_NUM;
    }

    // 使用静态数据表（仅加载一次）
    static PlayerxMonsterPointTable table;
    static bool loaded = false;

    if (!loaded) {
        // 尝试从配置文件加载数据表
        std::string config_path = "config/PlayerxMonsterPoint.json";

        // 尝试多个可能的路径
        std::vector<std::string> possible_paths = {
            config_path,
            "../config/PlayerxMonsterPoint.json",
            "../../config/PlayerxMonsterPoint.json"
        };

        bool load_success = false;
        for (const auto& path : possible_paths) {
            if (table.LoadFromFile(path)) {
                load_success = true;
                spdlog::info("[GetPlayerPoint] Loaded data table from: {}", path);
                break;
            }
        }

        if (!load_success) {
            spdlog::warn("[GetPlayerPoint] Failed to load data table, using fallback formula");

            // 备用方案：使用简化公式
            int base_point = 100;
            float gap_multiplier = 1.0f + (level_gap * 0.1f);
            if (gap_multiplier < 0.0f) {
                gap_multiplier = 0.0f;
            }

            loaded = true;  // 标记为已尝试加载，避免重复尝试
            return static_cast<uint32_t>(base_point * gap_multiplier);
        }

        loaded = true;
    }

    // 从数据表查询点数
    uint32_t point = table.GetPoint(level, level_gap);

    spdlog::debug("[GetPlayerPoint] Level={}, LevelGap={}, Point={}", level, level_gap, point);

    return point;
}

uint32_t AttackCalculator::GetPlayerExpPoint(int level_gap, uint32_t monster_exp) {
    // 对应: GetPlayerExpPoint(int level_gap, DWORD MonsterExp)
    // level_gap = 玩家等级 - 怪物等级

    float exp = 0.0f;

    if (level_gap < -8) {          // 玩家比怪物低9级或更多
        exp = static_cast<float>(monster_exp * 1.5);
    }
    else if (level_gap > -9 && level_gap < 1) {  // 玩家比怪物低0-8级
        exp = static_cast<float>(monster_exp + monster_exp * (-level_gap) * 0.05f);
    }
    else if (level_gap > 0 && level_gap < 5) {    // 玩家比怪物高1-4级
        exp = static_cast<float>(monster_exp * (5 - level_gap) * 0.2f);
    }
    else if (level_gap == 5) {       // 玩家比怪物高5级
        exp = static_cast<float>(monster_exp * 0.1f);
    }
    else if (level_gap > 5) {        // 玩家比怪物高6级或更多
        return 0;
    }
    else {
        return 0;
    }

    // 四舍五入 (对应: if( (DWORD)(Exp * 10) > (DWORD)Exp * 10 ))
    uint32_t exp_int = static_cast<uint32_t>(exp);
    uint32_t exp_times_10 = static_cast<uint32_t>(exp * 10.0f);
    if (exp_times_10 > exp_int * 10) {
        return exp_int + 1;
    } else {
        return exp_int;
    }
}

// ========== 暴击/必杀判定 ==========

bool AttackCalculator::IsCritical(
    uint32_t attacker_critical,
    uint16_t attacker_level,
    uint16_t target_level,
    float critical_rate_bonus,
    int16_t unique_item_cri_rate
) {
    // 对应: getCritical(CObject* pAttacker, CObject* pTarget, float fCriticalRate)
    // 使用非日本版本公式

    // 基础暴击率: (attackercritical + 20.f) / (targetlevel * 20.f + 300.f)
    float critical_rate = (static_cast<float>(attacker_critical) + 20.0f) /
                          (static_cast<float>(target_level) * 20.0f + 300.0f);

    // 最大暴击率上限20%
    if (critical_rate > 0.2f) {
        critical_rate = 0.2f;
    }

    uint16_t critical_percent = static_cast<uint16_t>(critical_rate * 100.0f);

    // 加上技能/装备提供的额外暴击率
    if (critical_rate_bonus > 0.0f) {
        critical_percent += GetPercent(critical_rate_bonus, attacker_level, target_level);
    }

    // 独特物品加成 (可为负)
    if ((static_cast<int>(critical_percent) + unique_item_cri_rate) < 0) {
        critical_percent = 0;
    } else {
        critical_percent += unique_item_cri_rate;
    }

    // 随机判定 (对应: rand()%100 < wCriticalPercent)
    return (rand() % 100) < critical_percent;
}

bool AttackCalculator::IsDecisive(
    uint32_t attacker_decisive,
    uint16_t attacker_level,
    uint16_t target_level,
    float decisive_rate_bonus,
    int16_t unique_item_dec_rate
) {
    // 对应: getDecisive(CObject* pAttacker, CObject* pTarget, float fCriticalRate)
    // 公式与IsCritical完全相同

    float decisive_rate = (static_cast<float>(attacker_decisive) + 20.0f) /
                          (static_cast<float>(target_level) * 20.0f + 300.0f);

    if (decisive_rate > 0.2f) {
        decisive_rate = 0.2f;
    }

    uint16_t decisive_percent = static_cast<uint16_t>(decisive_rate * 100.0f);

    if (decisive_rate_bonus > 0.0f) {
        decisive_percent += GetPercent(decisive_rate_bonus, attacker_level, target_level);
    }

    if ((static_cast<int>(decisive_percent) + unique_item_dec_rate) < 0) {
        decisive_percent = 0;
    } else {
        decisive_percent += unique_item_dec_rate;
    }

    return (rand() % 100) < decisive_percent;
}

// ========== 物理攻击力计算 ==========

double AttackCalculator::GetPlayerPhysicalAttackPower(
    uint32_t phy_attack_min,
    uint32_t phy_attack_max,
    float skill_base_atk,
    float phy_attack_rate,
    bool is_critical
) {
    // 对应: getPlayerPhysicalAttackPower(CPlayer* pPlayer, float PhyAttackRate, BOOL bCritical)

    double physical_attack_power = 0.0;
    uint32_t min_val = phy_attack_min;
    uint32_t max_val = phy_attack_max;

    // 1. 技能属性加成 (06.06.2勒涅 - 勒涅)
    // float val = 1 + pPlayer->GetSkillStatsOption()->BaseAtk;
    float val = 1.0f + skill_base_atk;

    if (val < 0.0f) {
        val = 0.0f;
    }

    // 四舍五入 (对应: (DWORD)((minVal * val) + 0.5))
    min_val = static_cast<uint32_t>((min_val * val) + 0.5);
    max_val = static_cast<uint32_t>((max_val * val) + 0.5);

    // 2. 在最小最大值之间随机
    if (max_val <= min_val) {
        physical_attack_power = static_cast<double>(min_val);
    } else {
        uint32_t gap = max_val - min_val + 1;
        physical_attack_power = static_cast<double>(min_val + rand() % gap);
    }

    // 3. 应用攻击力倍率
    physical_attack_power = physical_attack_power * phy_attack_rate;

    // 4. 暴击倍率 (非日本版本)
    // 对应: #ifndef _JAPAN_LOCAL_
    if (is_critical) {
        physical_attack_power = physical_attack_power * 1.5f;  // 暴击1.5倍
    }

    return physical_attack_power;
}

double AttackCalculator::GetMonsterPhysicalAttackPower(
    uint32_t phy_attack_min,
    uint32_t phy_attack_max
) {
    // 对应: getMonsterPhysicalAttackPower(CMonster* pMonster, float PhyAttackRate, BOOL bCritical)
    // 注意: 怪物物理攻击不应用倍率和暴击

    double physical_attack_power = 0.0;
    uint32_t min_val = phy_attack_min;
    uint32_t max_val = phy_attack_max;

    if (max_val <= min_val) {
        physical_attack_power = static_cast<double>(min_val);
    } else {
        uint32_t gap = max_val - min_val + 1;
        physical_attack_power = static_cast<double>(min_val + rand() % gap);
    }

    return physical_attack_power;
}

// ========== 属性攻击力计算 ==========

double AttackCalculator::GetPlayerAttributeAttackPower(
    uint16_t level,
    uint16_t simmek,
    float attrib_attack_rate,
    uint32_t attrib_attack_min,
    uint32_t attrib_attack_max,
    float attrib_plus_percent
) {
    // 对应: getPlayerAttributeAttackPower(CPlayer* pPlayer, WORD Attrib, DWORD AttAttackMin, DWORD AttAttackMax, float AttAttackRate)

    uint32_t min_v = 0;
    uint32_t max_v = 0;

    if (attrib_attack_rate > 0.0f) {
        // 属性百分比攻击力
        // WORD SimMek = pPlayer->GetSimMek();
        // double midtermVal = (double)(SimMek + 200)/(double)100;
        double midterm_val = (static_cast<double>(simmek + 200)) / 100.0;

        // DWORD MinLVV = (pPlayer->GetLevel()+5) - 5;  // = pPlayer->GetLevel()
        // DWORD MaxLVV = (pPlayer->GetLevel()+5) + 5;  // = pPlayer->GetLevel() + 10
        uint32_t min_lvv = level;           // (level + 5) - 5
        uint32_t max_lvv = level + 10;      // (level + 5) + 5

        // MinV = DWORD((MinLVV * AttAttackRate * midtermVal) + SimMek/5 + min(SimMek-12,25));
        // MaxV = DWORD((MaxLVV * AttAttackRate * midtermVal) + SimMek/5 + min(SimMek-12,25));
        uint32_t simmek_div_5 = simmek / 5;
        int32_t simmek_minus_12 = simmek - 12;
        uint32_t min_extra = (simmek_minus_12 < 25) ? simmek_minus_12 : 25;
        if (min_extra < 0) min_extra = 0;  // 防止负数

        min_v = static_cast<uint32_t>((min_lvv * attrib_attack_rate * midterm_val) + simmek_div_5 + min_extra);
        max_v = static_cast<uint32_t>((max_lvv * attrib_attack_rate * midterm_val) + simmek_div_5 + min_extra);
    }

    // 加上固定属性攻击
    min_v += attrib_attack_min;
    max_v += attrib_attack_max;

    // 属性加成 (装备/特殊物品)
    // float AttUp = 1 + pPlayer->GetAttribPlusPercent(Attrib);
    float att_up = 1.0f + attrib_plus_percent;
    min_v = static_cast<uint32_t>(att_up * min_v);
    max_v = static_cast<uint32_t>(att_up * max_v);

    // 随机取值
    return Random(min_v, max_v);
}

double AttackCalculator::GetMonsterAttributeAttackPower(
    uint32_t attrib_attack_min,
    uint32_t attrib_attack_max
) {
    // 对应: getMonsterAttributeAttackPower(CMonster* pMonster, WORD Attrib, DWORD AttAttackMin, DWORD AttAttackMax)
    // 非常简单的随机

    uint32_t gap = attrib_attack_max - attrib_attack_min + 1;
    return static_cast<double>(attrib_attack_min + rand() % gap);
}

// ========== 防御等级计算 ==========

double AttackCalculator::GetPhyDefenceLevel(
    uint32_t phy_defense,
    uint16_t defender_level,
    uint16_t attacker_level,
    float skill_phy_def,
    float regist_phys,
    float enemy_defen,
    float party_defence_rate
) {
    // 对应: getPhyDefenceLevel(CObject* pObject, CObject* pAttacker)
    // 使用非日本版本公式

    double phy_defence = static_cast<double>(phy_defense);

    // 1. 攻击者的独特物品: 降低敌方防御
    // if (pAttacker->GetObjectKind() == eObjectKind_Player) {
    //     phyDefence = phyDefence * (1.0f - (((CPlayer*)pAttacker)->GetUniqueItemStats()->nEnemyDefen * 0.01f));
    // }
    phy_defence = phy_defence * (1.0f - (enemy_defen * 0.01f));

    // 2. 防御者的商店物品防御加成
    // if (pObject->GetObjectKind() == eObjectKind_Player) {
    //     phyDefence += (phyDefence * ((CPlayer*)pObject)->GetShopItemStats()->RegistPhys) / 100;
    //
    //     // 3. 技能防御加成
    //     float val = 1.0f + ((CPlayer*)pObject)->GetSkillStatsOption()->PhyDef;
    //     if (val < 0.0f)
    //         val = 0.0f;
    //     phyDefence = phyDefence * val;
    //
    //     // 4. 组队防御加成
    //     if (party_defence_rate > 1.0f)
    //         phyDefence = phyDefence * party_defence_rate;
    // }

    phy_defence += (phy_defence * regist_phys) / 100.0;

    float val = 1.0f + skill_phy_def;
    if (val < 0.0f) {
        val = 0.0f;
    }
    phy_defence = phy_defence * val;

    if (party_defence_rate != 1.0f) {
        phy_defence = phy_defence * party_defence_rate;
    }

    // 5. 计算防御等级 (非日本版本)
    // double phyDefenceLevel = (phyDefence*2.0 + 50) / (AttackerLevel*20 + 150);
    double phy_defence_level = (phy_defence * 2.0 + 50.0) / (static_cast<double>(attacker_level) * 20.0 + 150.0);

    if (phy_defence_level < 0.0) {
        spdlog::warn("PhyDefenceLevel < 0: {}", phy_defence_level);
        phy_defence_level = 0.0;
    }

    // 最大防御等级90%
    if (phy_defence_level > 0.9) {
        phy_defence_level = 0.9;
    }

    // 注意: 泰坦防御修正未在此实现,需要在更高层处理

    return phy_defence_level;
}

// ========== 泰坦攻击力计算 (SW070127) ==========

double AttackCalculator::GetTitanPhysicalAttackPower(
    uint32_t titan_phy_attack_min,
    uint32_t titan_phy_attack_max,
    uint32_t player_phy_attack_min,
    uint32_t player_phy_attack_max,
    float skill_base_atk,
    float phy_attack_rate,
    bool is_critical
) {
    // 对应: getTitanPhysicalAttackPower(CTitan* pTitan, CPlayer* pPlayer, float PhyAttackRate, BOOL bCritical)
    // 注意：这个函数只计算泰坦自己的物理攻击力，不包括玩家攻击加成
    // 玩家攻击加成在调用此函数后，通过 GetMergedTitanAttackPower 函数合并

    double titan_attack_power = 0.0f;
    uint32_t min_val = titan_phy_attack_min;
    uint32_t max_val = titan_phy_attack_max;

    // 1. 泰坦基础攻击 (在泰坦最小最大值之间随机)
    if (max_val <= min_val) {
        titan_attack_power = static_cast<double>(min_val);
    } else {
        uint32_t gap = max_val - min_val + 1;
        titan_attack_power = static_cast<double>(min_val + rand() % gap);
    }

    // 2. 技能属性加成
    float val = 1.0f + skill_base_atk;
    if (val < 0.0f) {
        val = 0.0f;
    }
    titan_attack_power = titan_attack_power * val;

    // 3. 应用攻击力倍率
    titan_attack_power = titan_attack_power * phy_attack_rate;

    // 4. 暴击倍率
    if (is_critical) {
        titan_attack_power = titan_attack_power * 1.5f;  // 泰坦暴击也是1.5倍
    }

    spdlog::debug("Titan Physical Attack (Base): {} (Titan Min: {}, Max: {}, Rate: {}, Crit: {})",
                  titan_attack_power,
                  titan_phy_attack_min,
                  titan_phy_attack_max,
                  phy_attack_rate,
                  is_critical);

    return titan_attack_power;
}

double AttackCalculator::GetTitanAttributeAttackPower(
    uint16_t level,
    uint16_t simmek,
    float attrib_attack_rate,
    uint32_t titan_attrib_min,
    uint32_t titan_attrib_max,
    uint32_t player_attrib_min,
    uint32_t player_attrib_max,
    float attrib_plus_percent
) {
    // 对应: getTitanAttributeAttackPower(CTitan* pTitan, CPlayer* pPlayer, WORD Attrib, float AttAttackRate)
    // Legacy 公式: DWORD(( pStats->AttributeAttack.GetElement_Val(Attrib) * (ownerSimMek + 100)/400 + ownerSimMek/5 ) * 0.74f)

    // 注意：泰坦属性攻击力计算与玩家属性攻击力计算完全不同！
    // Legacy 使用简化的公式，不涉及 MinLVV/MaxLVV 计算

    // 使用泰坦的基础属性攻击（titan_attrib_min 和 titan_attrib_max 相同，因为是固定值）
    uint32_t titan_base_attrib = titan_attrib_min;

    // Legacy 公式: (泰坦基础属性攻击 * (玩家内功 + 100) / 400 + 玩家内功 / 5) * 0.74
    double calc_value = (static_cast<double>(titan_base_attrib) * (simmek + 100) / 400.0) + (simmek / 5.0);
    calc_value = calc_value * 0.74f;

    // 转换为整数
    uint32_t min_pwr = static_cast<uint32_t>(calc_value);
    uint32_t max_pwr = static_cast<uint32_t>(calc_value);

    // 应用属性攻击倍率
    double attack_power = static_cast<double>(Random(min_pwr, max_pwr)) * attrib_attack_rate;

    spdlog::debug("Titan Attribute Attack: {} (Titan Base: {}, SimMek: {}, Rate: {})",
                  attack_power,
                  titan_base_attrib,
                  simmek,
                  attrib_attack_rate);

    return attack_power;
}

// ========== 泰坦和玩家攻击力合并 ==========

double AttackCalculator::GetMergedTitanPhysicalAttack(
    double titan_attack_power,
    double player_attack_power
) {
    // 对应: Legacy 代码 getPhysicalAttackPower 中的合并逻辑
    // double finalPwr = titanPwr + masterPwr * 0.6f;

    double final_power = titan_attack_power + player_attack_power * 0.6f;

    spdlog::debug("Merged Titan Physical Attack: {} (Titan: {} + Player: {} * 0.6)",
                  final_power,
                  titan_attack_power,
                  player_attack_power);

    return final_power;
}

double AttackCalculator::GetMergedTitanAttributeAttack(
    double titan_attack_power,
    double player_attack_power
) {
    // 对应: Legacy 代码 getAttributeAttackPower 中的合并逻辑
    // double finalPwr = titanPwr + masterPwr * 0.6f;

    double final_power = titan_attack_power + player_attack_power * 0.6f;

    spdlog::debug("Merged Titan Attribute Attack: {} (Titan: {} + Player: {} * 0.6)",
                  final_power,
                  titan_attack_power,
                  player_attack_power);

    return final_power;
}

// ========== 辅助函数 ==========

uint16_t AttackCalculator::GetPercent(float rate, uint16_t attacker_level, uint16_t target_level) {
    // 对应: GetPercent(fCriticalRate, pAttacker->GetLevel(), pTarget->GetLevel())
    // Legacy 源文件: [CC]Header/CommonCalcFunc.cpp
    //
    // Legacy 公式: SeedVal + {(操作者等级 - 目标等级)/2.5*0.01}
    // 实际代码: SeedVal + LevelGap * 0.025f

    int level_gap = static_cast<int>(attacker_level) - static_cast<int>(target_level);

    float f_rate = rate + (level_gap * 0.025f);

    if (f_rate <= 0.0f) {
        return 0;
    }

    return static_cast<uint16_t>(f_rate * 100.0f);
}

uint32_t AttackCalculator::Random(uint32_t min_val, uint32_t max_val) {
    // 对应: random(MinV, MaxV)
    if (max_val <= min_val) {
        return min_val;
    }

    uint32_t gap = max_val - min_val + 1;
    return min_val + rand() % gap;
}

} // namespace Game
} // namespace Murim
