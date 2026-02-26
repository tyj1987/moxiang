#pragma once

#include "Battle.hpp"
#include <vector>
#include <memory>
#include <cstdint>
#include <map>
#include <set>

namespace Murim {
namespace Game {

/**
 * @brief 战场状态枚举
 */
enum class BattleState : uint8_t {
    kWaiting = 0,        // 等待开始
    kFighting = 1,       // 战斗中
    kEnded = 2           // 已结束
};

/**
 * @brief 战场类型枚举
 */
enum class BattleType : uint8_t {
    kNormal = 0,         // 普通战斗
    kGTournament,       // 公会锦标赛
    kSiegeWar,          // 攻城战
    kMunpaField,        // 门派战场
    kMurimField,        // 武林战场
    kSuryun,            // 修炼
    kVimuStreet,        // 武林街战
    kMax
};

/**
 * @brief 公会锦标赛战场
 */
class Battle_GTournament : public Battle {
public:
    Battle_GTournament(uint32_t battle_id, uint32_t guild_id_1, uint32_t guild_id_2);

    // ========== 公会锦标赛特有功能 ==========

    /**
     * @brief 战斗开始
     */
    void OnFightStart() override;

    /**
     * @brief 战斗结束
     */
    void OnDestroy() override;

    /**
     * @brief 判定胜负
     */
    bool Judge() override;

    /**
     * @brief 胜利处理
     */
    void Victory(int winner_team_num) override;

    /**
     * @brief 平局处理
     */
    void Draw() override;

    /**
     * @brief 队员加入
     */
    void OnTeamMemberAdd(uint64_t member_id) override;

    /**
     * @brief 队员死亡
     */
    void OnTeamMemberDie(uint64_t member_id) override;

    /**
     * @brief 怪物掉落分配
     */
    void OnMonsterDistribute() override;

    /**
     * @brief 获取参战公会ID
     */
    std::vector<uint32_t> GetParticipantGuilds() const { return participant_guilds_; }

    /**
     * @brief 获取锦标赛分数
     */
    uint32_t GetGuildScore(uint32_t guild_id) const;

    /**
     * @brief 增加公会分数
     */
    void AddGuildScore(uint32_t guild_id, uint32_t score);

private:
    // ========== 辅助函数 ==========

    /**
     * @brief 广播战斗开始
     */
    void BroadcastBattleStart();

    /**
     * @brief 广播战斗结束
     */
    void BroadcastBattleEnd();

    /**
     * @brief 广播胜利消息
     */
    void BroadcastVictory(uint32_t winner_guild_id);

    /**
     * @brief 广播平局消息
     */
    void BroadcastDraw();

    /**
     * @brief 发放胜利奖励
     */
    void GrantVictoryRewards(uint32_t guild_id);

    /**
     * @brief 发放平局奖励
     */
    void GrantDrawRewards();

    /**
     * @brief 更新公会战绩
     */
    void UpdateGuildStats(uint32_t guild_id, bool is_victory);

    /**
     * @brief 传送玩家回出生点
     */
    void TeleportPlayersToSpawnPoint();

    /**
     * @brief 分发战斗奖励
     */
    void DistributeRewards();

    /**
     * @brief 清理战斗数据
     */
    void CleanupBattleData();

    /**
     * @brief 根据分数分配怪物掉落
     */
    void DistributeMonsterDropsByScore();

    /**
     * @brief 获取成员所属公会
     */
    uint32_t GetMemberGuild(uint64_t member_id);

private:
    std::vector<uint32_t> participant_guilds_;
    std::map<uint32_t, uint32_t> guild_scores_;  // guild_id -> score
    std::map<uint32_t, std::set<uint64_t>> team_members_;  // guild_id -> member_ids
    std::map<uint32_t, uint32_t> alive_member_counts_;  // guild_id -> alive count
    BattleState battle_state_ = BattleState::kWaiting;
    uint16_t max_score_ = 1000;
};

/**
 * @brief 门派战场
 */
class Battle_MunpaField : public Battle {
public:
    Battle_MunpaField(uint32_t battle_id, uint32_t munpa_id_1, uint32_t munpa_id_2);

    void OnFightStart() override;
    void OnDestroy() override;
    bool Judge() override;
    void Victory(int winner_team_num) override;
    void Draw() override;
    void OnTeamMemberAdd(uint64_t member_id) override;
    void OnTeamMemberDie(uint64_t member_id) override;
    void OnMonsterDistribute() override;

    uint32_t GetMunpaScore(uint32_t munpa_id) const;
    void AddMunpaScore(uint32_t munpa_id, uint32_t score);

private:
    std::vector<uint32_t> participant_munpas_;
    std::map<uint32_t, uint32_t> munpa_scores_;
    uint32_t win_score_ = 500;
};

/**
 * @brief 武林战场
 */
class Battle_MurimField : public Battle {
public:
    Battle_MurimField(uint32_t battle_id);

    void OnFightStart() override;
    void OnDestroy() override;
    bool Judge() override;
    void Victory(int winner_team_num) override;
    void Draw() override;
    void OnTeamMemberAdd(uint64_t member_id) override;
    void OnTeamMemberDie(uint64_t member_id) override;
    void OnMonsterDistribute() override;

    // 武林战场特有的积分系统
    uint32_t GetPlayerScore(uint64_t player_id) const;
    void AddPlayerScore(uint64_t player_id, uint32_t score);

private:
    std::map<uint64_t, uint32_t> player_scores_;
    uint32_t max_score_ = 1000;
};

/**
 * @brief 攻城战
 */
class Battle_SiegeWar : public Battle {
public:
    Battle_SiegeWar(uint32_t battle_id, uint32_t attacking_guild_id, uint32_t defending_guild_id);

    void OnFightStart() override;
    void OnDestroy() override;
    bool Judge() override;
    void Victory(int winner_team_num) override;
    void Draw() override;
    void OnTeamMemberAdd(uint64_t member_id) override;
    void OnTeamMemberDie(uint64_t member_id) override;
    void OnMonsterDistribute() override;

    // 攻城战特有功能
    /**
     * @brief 占领据点
     */
    bool CaptureBase(uint32_t base_id, uint64_t capturer_id);

    /**
     * @brief 获取占领的据点数量
     */
    uint8_t GetCapturedBaseCount(uint32_t guild_id) const;

    /**
     * @brief 检查是否攻破城门
     */
    bool IsGateDestroyed() const { return gate_destroyed_; }

    /**
     * @brief 设置城门状态
     */
    void SetGateDestroyed(bool destroyed) { gate_destroyed_ = destroyed; }

private:
    // ========== 辅助函数 ==========

    /**
     * @brief 初始化据点
     */
    void InitializeBases();

    /**
     * @brief 检查据点是否可以被占领
     */
    bool CanCaptureBase(uint32_t base_id, uint64_t capturer_id) const;

    /**
     * @brief 获取对象所属公会
     */
    uint32_t GetObjectGuild(uint64_t object_id) const;

    /**
     * @brief 获取公会存活成员数
     */
    uint32_t GetAliveMemberCount(uint32_t guild_id) const;

private:
    uint32_t attacking_guild_id_;
    uint32_t defending_guild_id_;
    bool gate_destroyed_ = false;
    std::map<uint32_t, uint32_t> base_owners_;  // base_id -> guild_id
    std::vector<uint32_t> base_ids_;
    std::map<uint32_t, std::set<uint64_t>> guild_members_;  // guild_id -> member_ids
    std::map<uint32_t, uint32_t> alive_member_counts_;  // guild_id -> alive count
};

/**
 * @brief 修炼系统
 */
class Battle_Suryun : public Battle {
public:
    Battle_Suryun(uint32_t battle_id, uint64_t player_id);

    void OnFightStart() override;
    void OnDestroy() override;
    bool Judge() override;
    void Victory(int winner_team_num) override;
    void Draw() override;
    void OnTeamMemberAdd(uint64_t member_id) override;
    void OnTeamMemberDie(uint64_t member_id) override;
    OnMonsterDistribute() override;

    // 修炼特有功能
    /**
     * @brief 获取修炼进度
     */
    float GetProgress() const { return progress_; }

    /**
     * @brief 增加修炼进度
     */
    void AddProgress(float amount) { progress_ = FMath::Clamp(progress_ + amount, 0.0f, 100.0f); }

    /**
     * @brief 是否修炼完成
     */
    bool IsCompleted() const { return progress_ >= 100.0f; }

private:
    // ========== 辅助函数 ==========

    /**
     * @brief 发放修炼奖励
     */
    void GrantTrainingRewards();

private:
    uint64_t training_player_id_;
    float progress_ = 0.0f;
    uint32_t required_monster_kills_ = 0;
    uint32_t current_monster_kills_ = 0;
};

/**
 * @brief 武林街战
 */
class Battle_VimuStreet : public Battle {
public:
    Battle_VimuStreet(uint32_t battle_id, uint64_t player_id, uint64_t target_id);

    void OnFightStart() override;
    void OnDestroy() override;
    bool Judge() override;
    void Victory(int winner_team_num) override;
    void Draw() override;
    void OnTeamMemberAdd(uint64_t member_id) override;
    void OnTeamMemberDie(uint64_t member_id) override;
    void OnMonsterDistribute() override;

    // 街战特有功能
    /**
     * @brief 获取PK获胜次数
     */
    uint16_t GetWinCount(uint64_t player_id) const;

    /**
     * @brief 增加获胜次数
     */
    void AddWin(uint64_t player_id);

    /**
     * @brief 获取PK失败次数
     */
    uint16_t GetLoseCount(uint64_t player_id) const;

    /**
     * @brief 增加失败次数
     */
    void AddLose(uint64_t player_id);

private:
    // ========== 辅助函数 ==========

    /**
     * @brief 检查玩家是否存活
     */
    bool IsPlayerAlive(uint64_t player_id) const;

    /**
     * @brief 计算PK惩罚
     */
    void CalculatePKPenalty();

    /**
     * @brief 计算PK奖励和惩罚
     */
    void CalculatePKRewardsAndPenalties(uint64_t winner_id, uint64_t loser_id);

private:
    uint64_t initiator_id_;
    uint64_t target_id_;
    std::map<uint64_t, uint16_t> win_counts_;
    std::map<uint64_t, uint16_t> lose_counts_;
    bool pk_enabled_ = true;
};

/**
 * @brief 战场工厂 - 根据类型创建战场
 */
class BattleFactory {
public:
    /**
     * @brief 创建战场
     *
     * @param battle_type 战场类型
     * @param battle_id 战场ID
     * @param participants 参与者ID列表
     * @return 战场对象
     */
    static std::unique_ptr<Battle> CreateBattle(
        BattleType battle_type,
        uint32_t battle_id,
        const std::vector<uint64_t>& participants
    );

    /**
     * @brief 创建公会锦标赛战场
     */
    static std::unique_ptr<Battle_GTournament> CreateGTournament(
        uint32_t battle_id,
        uint32_t guild_id_1,
        uint32_t guild_id_2
    );

    /**
     * @brief 创建门派战场
     */
    static std::unique_ptr<Battle_MunpaField> CreateMunpaField(
        uint32_t battle_id,
        uint32_t munpa_id_1,
        uint32_t munpa_id_2
    );

    /**
     * @brief 创建武林战场
     */
    static std::unique_ptr<Battle_MurimField> CreateMurimField(uint32_t battle_id);

    /**
     * @brief 创建攻城战
     */
    static std::unique_ptr<Battle_SiegeWar> CreateSiegeWar(
        uint32_t battle_id,
        uint32_t attacking_guild_id,
        uint32_t defending_guild_id
    );

    /**
     * @brief 创建修炼战场
     */
    static std::unique_ptr<Battle_Suryun> CreateSuryun(
        uint32_t battle_id,
        uint64_t player_id
    );

    /**
     * @brief 创建街战
     */
    static std::unique_ptr<Battle_VimuStreet> CreateVimuStreet(
        uint32_t battle_id,
        uint64_t player_id,
        uint64_t target_id
    );

private:
    /**
     * @brief 获取玩家所属公会
     */
    static uint32_t GetPlayerGuild(uint64_t player_id);

    /**
     * @brief 获取玩家所属门派
     */
    static uint32_t GetPlayerMunpa(uint64_t player_id);
};

} // namespace Game
} // namespace Murim
