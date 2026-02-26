#pragma once

#include <cstdint>
#include <cmath>

namespace Murim {
namespace Game {

/**
 * @brief 攻击计算器 - 严格对应 legacy CAttackCalc
 *
 * ⚠️ 重要: 所有公式必须与 legacy AttackCalc.cpp 100% 一致
 *
 * 源文件: legacy-source-code/[Server]Map/AttackCalc.cpp
 */
class AttackCalculator {
public:
    // ========== 经验值计算 ==========

    /**
     * @brief 计算玩家获得的经验值
     *
     * 对应: GetPlayerExpPoint(int level_gap, DWORD MonsterExp)
     *
     * @param level_gap 玩家等级 - 怪物等级
     * @param monster_exp 怪物基础经验值
     * @return 玩家获得的经验值
     */
    static uint32_t GetPlayerExpPoint(int level_gap, uint32_t monster_exp);

    /**
     * @brief 计算玩家点数（任务奖励等）
     *
     * 对应: GetPlayerPoint(LEVELTYPE level1, int leve_gab)
     *
     * @param level 玩家等级 (1-120)
     * @param level_gap 等级差 (玩家等级 - 怪物等级，-6 到 +9)
     * @return 玩家点数
     */
    static uint32_t GetPlayerPoint(uint16_t level, int level_gap);

    // ========== 暴击/必杀判定 ==========

    /**
     * @brief 判定是否暴击
     *
     * 对应: getCritical(CObject* pAttacker, CObject* pTarget, float fCriticalRate)
     *
     * 公式: (attackercritical + 20) / (targetlevel * 20 + 300)
     * 上限: 20%
     *
     * @param attacker_critical 攻击者暴击值
     * @param attacker_level 攻击者等级
     * @param target_level 目标等级
     * @param critical_rate_bonus 额外暴击率 (技能/装备)
     * @param unique_item_cri_rate 独特物品暴击率加成 (可为负)
     * @return 是否暴击
     */
    static bool IsCritical(
        uint32_t attacker_critical,
        uint16_t attacker_level,
        uint16_t target_level,
        float critical_rate_bonus = 0.0f,
        int16_t unique_item_cri_rate = 0
    );

    /**
     * @brief 判定是否必杀
     *
     * 对应: getDecisive(CObject* pAttacker, CObject* pTarget, float fCriticalRate)
     *
     * 公式与IsCritical完全相同,但使用decisive值
     *
     * @param attacker_decisive 攻击者必杀值
     * @param attacker_level 攻击者等级
     * @param target_level 目标等级
     * @param decisive_rate_bonus 额外必杀率
     * @param unique_item_dec_rate 独特物品必杀率加成
     * @return 是否必杀
     */
    static bool IsDecisive(
        uint32_t attacker_decisive,
        uint16_t attacker_level,
        uint16_t target_level,
        float decisive_rate_bonus = 0.0f,
        int16_t unique_item_dec_rate = 0
    );

    // ========== 物理攻击力计算 ==========

    /**
     * @brief 计算玩家物理攻击力
     *
     * 对应: getPlayerPhysicalAttackPower(CPlayer* pPlayer, float PhyAttackRate, BOOL bCritical)
     *
     * @param phy_attack_min 最小物理攻击
     * @param phy_attack_max 最大物理攻击
     * @param skill_base_atk 技能基础攻击加成 (GetSkillStatsOption()->BaseAtk)
     * @param phy_attack_rate 攻击力倍率
     * @param is_critical 是否暴击
     * @return 物理攻击力
     */
    static double GetPlayerPhysicalAttackPower(
        uint32_t phy_attack_min,
        uint32_t phy_attack_max,
        float skill_base_atk,
        float phy_attack_rate,
        bool is_critical
    );

    /**
     * @brief 计算怪物物理攻击力
     *
     * 对应: getMonsterPhysicalAttackPower(CMonster* pMonster, float PhyAttackRate, BOOL bCritical)
     *
     * 注意: 怪物物理攻击不应用倍率和暴击
     *
     * @param phy_attack_min 最小物理攻击
     * @param phy_attack_max 最大物理攻击
     * @return 物理攻击力
     */
    static double GetMonsterPhysicalAttackPower(
        uint32_t phy_attack_min,
        uint32_t phy_attack_max
    );

    // ========== 属性攻击力计算 ==========

    /**
     * @brief 计算玩家属性攻击力
     *
     * 对应: getPlayerAttributeAttackPower(CPlayer* pPlayer, WORD Attrib, DWORD AttAttackMin, DWORD AttAttackMax, float AttAttackRate)
     *
     * @param level 玩家等级
     * @param simmek 内功值 (GetSimMek)
     * @param attrib_attack_rate 属性攻击倍率
     * @param attrib_attack_min 最小属性攻击
     * @param attrib_attack_max 最大属性攻击
     * @param attrib_plus_percent 属性加成百分比 (GetAttribPlusPercent)
     * @return 属性攻击力
     */
    static double GetPlayerAttributeAttackPower(
        uint16_t level,
        uint16_t simmek,
        float attrib_attack_rate,
        uint32_t attrib_attack_min,
        uint32_t attrib_attack_max,
        float attrib_plus_percent
    );

    /**
     * @brief 计算怪物属性攻击力
     *
     * 对应: getMonsterAttributeAttackPower(CMonster* pMonster, WORD Attrib, DWORD AttAttackMin, DWORD AttAttackMax)
     *
     * @param attrib_attack_min 最小属性攻击
     * @param attrib_attack_max 最大属性攻击
     * @return 属性攻击力
     */
    static double GetMonsterAttributeAttackPower(
        uint32_t attrib_attack_min,
        uint32_t attrib_attack_max
    );

    // ========== 防御等级计算 ==========

    /**
     * @brief 计算物理防御等级
     *
     * 对应: getPhyDefenceLevel(CObject* pObject, CObject* pAttacker)
     *
     * 公式 (非日本版本): (phyDefence * 2.0 + 50) / (AttackerLevel * 20 + 150)
     * 上限: 90%
     *
     * @param phy_defense 基础物理防御力
     * @param defender_level 防御者等级
     * @param attacker_level 攻击者等级
     * @param skill_phy_def 技能防御加成 (GetSkillStatsOption()->PhyDef)
     * @param regist_phys 商店物品物理防御 (GetShopItemStats()->RegistPhys)
     * @param enemy_defen 独特物品降低防御 (GetUniqueItemStats()->nEnemyDefen)
     * @param party_defence_rate 组队防御加成 (可选)
     * @return 防御等级 (0.0-0.9)
     */
    static double GetPhyDefenceLevel(
        uint32_t phy_defense,
        uint16_t defender_level,
        uint16_t attacker_level,
        float skill_phy_def = 0.0f,
        float regist_phys = 0.0f,
        float enemy_defen = 0.0f,
        float party_defence_rate = 1.0f
    );

    // ========== 泰坦攻击力计算 (SW070127) ==========

    /**
     * @brief 计算泰坦物理攻击力
     *
     * 对应: getTitanPhysicalAttackPower(CTitan* pTitan, CPlayer* pPlayer, float PhyAttackRate, BOOL bCritical)
     * 注意：这个函数只计算泰坦自己的物理攻击力，不包括玩家攻击加成
     *
     * @param titan_phy_attack_min 泰坦最小物理攻击
     * @param titan_phy_attack_max 泰坦最大物理攻击
     * @param player_phy_attack_min 玩家最小物理攻击
     * @param player_phy_attack_max 玩家最大物理攻击
     * @param skill_base_atk 技能基础攻击加成
     * @param phy_attack_rate 攻击力倍率
     * @param is_critical 是否暴击
     * @return 泰坦物理攻击力
     */
    static double GetTitanPhysicalAttackPower(
        uint32_t titan_phy_attack_min,
        uint32_t titan_phy_attack_max,
        uint32_t player_phy_attack_min,
        uint32_t player_phy_attack_max,
        float skill_base_atk,
        float phy_attack_rate,
        bool is_critical
    );

    /**
     * @brief 计算泰坦属性攻击力
     *
     * 对应: getTitanAttributeAttackPower(CTitan* pTitan, CPlayer* pPlayer, WORD Attrib, float AttAttackRate)
     * Legacy 公式: (泰坦基础属性攻击 * (玩家内功 + 100) / 400 + 玩家内功 / 5) * 0.74
     *
     * @param level 玩家等级
     * @param simmek 玩家内功值
     * @param attrib_attack_rate 属性攻击倍率
     * @param titan_attrib_min 泰坦最小属性攻击
     * @param titan_attrib_max 泰坦最大属性攻击
     * @param player_attrib_min 玩家最小属性攻击
     * @param player_attrib_max 玩家最大属性攻击
     * @param attrib_plus_percent 属性加成百分比
     * @return 泰坦属性攻击力
     */
    static double GetTitanAttributeAttackPower(
        uint16_t level,
        uint16_t simmek,
        float attrib_attack_rate,
        uint32_t titan_attrib_min,
        uint32_t titan_attrib_max,
        uint32_t player_attrib_min,
        uint32_t player_attrib_max,
        float attrib_plus_percent
    );

    /**
     * @brief 合并泰坦和玩家的物理攻击力
     *
     * 对应: Legacy 代码 getPhysicalAttackPower 中的合并逻辑
     * 公式: 泰坦攻击力 + 玩家攻击力 * 0.6
     *
     * @param titan_attack_power 泰坦物理攻击力
     * @param player_attack_power 玩家物理攻击力
     * @return 合并后的物理攻击力
     */
    static double GetMergedTitanPhysicalAttack(
        double titan_attack_power,
        double player_attack_power
    );

    /**
     * @brief 合并泰坦和玩家的属性攻击力
     *
     * 对应: Legacy 代码 getAttributeAttackPower 中的合并逻辑
     * 公式: 泰坦攻击力 + 玩家攻击力 * 0.6
     *
     * @param titan_attack_power 泰坦属性攻击力
     * @param player_attack_power 玩家属性攻击力
     * @return 合并后的属性攻击力
     */
    static double GetMergedTitanAttributeAttack(
        double titan_attack_power,
        double player_attack_power
    );

    // ========== 辅助函数 ==========

    /**
     * @brief 计算百分比加成 (对应 GetPercent)
     *
     * @param rate 基础百分比
     * @param attacker_level 攻击者等级
     * @param target_level 目标等级
     * @return 百分比加成值
     */
    static uint16_t GetPercent(float rate, uint16_t attacker_level, uint16_t target_level);

private:
    // 随机数生成 (使用C标准库rand以保持一致)
    static uint32_t Random(uint32_t min_val, uint32_t max_val);
};

} // namespace Game
} // namespace Murim
