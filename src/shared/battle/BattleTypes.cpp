// MurimServer - BattleTypes Implementation
// 战场类型实现 - 公会锦标赛、门派战场、攻城战等

#include "BattleTypes.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

// ========== Battle_GTournament (公会锦标赛) ==========

Battle_GTournament::Battle_GTournament(uint32_t battle_id, uint32_t guild_id_1, uint32_t guild_id_2)
    : Battle(battle_id) {
    participant_guilds_.push_back(guild_id_1);
    participant_guilds_.push_back(guild_id_2);

    guild_scores_[guild_id_1] = 0;
    guild_scores_[guild_id_2] = 0;

    spdlog::info("Created GTournament Battle {}: Guild {} vs Guild {}",
                 battle_id, guild_id_1, guild_id_2);
}

void Battle_GTournament::OnFightStart() {
    spdlog::info("GTournament Battle {}: Fight Started", battle_id_);

    // 广播战斗开始
    BroadcastBattleStart();

    // 初始化存活统计
    for (uint32_t guild_id : participant_guilds_) {
        alive_member_counts_[guild_id] = 0;
    }

    // 初始化战场状态
    battle_state_ = BattleState::kFighting;
}

void Battle_GTournament::OnDestroy() {
    spdlog::info("GTournament Battle {}: Destroyed", battle_id_);

    // 广播战斗结束
    BroadcastBattleEnd();

    // 计算并发放奖励
    DistributeRewards();

    // 清理战场数据
    CleanupBattleData();
}

bool Battle_GTournament::Judge() {
    // 判断条件:
    // 1. 一方分数达到max_score_
    // 2. 一方全部阵亡
    // 3. 时间到

    for (const auto& pair : guild_scores_) {
        if (pair.second >= max_score_) {
            spdlog::info("GTournament Battle {}: Guild {} reached max score {}",
                     battle_id_, pair.first, max_score_);
            return true;
        }
    }

    // 检查双方存活人数
    for (const auto& pair : alive_member_counts_) {
        if (pair.second == 0) {
            spdlog::info("GTournament Battle {}: Guild {} has no alive members",
                     battle_id_, pair.first);
            return true;
        }
    }

    return false;
}

void Battle_GTournament::Victory(int winner_team_num) {
    uint32_t winner_guild_id = participant_guilds_[winner_team_num];

    spdlog::info("GTournament Battle {}: Guild {} Wins!", battle_id_, winner_guild_id);

    // 1. 广播胜利消息
    BroadcastVictory(winner_guild_id);

    // 2. 发放胜利奖励
    GrantVictoryRewards(winner_guild_id);

    // 3. 更新公会战绩
    UpdateGuildStats(winner_guild_id, true);

    // 4. 传送玩家回出生点
    TeleportPlayersToSpawnPoint();
}

void Battle_GTournament::Draw() {
    spdlog::info("GTournament Battle {}: Draw", battle_id_);

    // 1. 广播平局消息
    BroadcastDraw();

    // 2. 发放平局奖励(减半)
    GrantDrawRewards();

    // 3. 传送玩家回出生点
    TeleportPlayersToSpawnPoint();
}

void Battle_GTournament::OnTeamMemberAdd(uint64_t member_id) {
    spdlog::debug("GTournament Battle {}: Member {} joined", battle_id_, member_id);

    // 更新队伍列表
    uint32_t guild_id = GetMemberGuild(member_id);
    team_members_[guild_id].insert(member_id);

    // 更新存活统计
    alive_member_counts_[guild_id]++;

    spdlog::debug("GTournament Battle {}: Guild {} now has {} alive members",
             battle_id_, guild_id, alive_member_counts_[guild_id]);
}

void Battle_GTournament::OnTeamMemberDie(uint64_t member_id) {
    spdlog::debug("GTournament Battle {}: Member {} died", battle_id_, member_id);

    // 更新存活统计
    uint32_t guild_id = GetMemberGuild(member_id);
    if (alive_member_counts_.find(guild_id) != alive_member_counts_.end()) {
        alive_member_counts_[guild_id]--;

        spdlog::debug("GTournament Battle {}: Guild {} now has {} alive members",
                 battle_id_, guild_id, alive_member_counts_[guild_id]);
    }
}

void Battle_GTournament::OnMonsterDistribute() {
    spdlog::debug("GTournament Battle {}: Distributing monster drops", battle_id_);

    // 分配怪物掉落 - 根据公会分数分配
    DistributeMonsterDropsByScore();
}

uint32_t Battle_GTournament::GetGuildScore(uint32_t guild_id) const {
    auto it = guild_scores_.find(guild_id);
    return (it != guild_scores_.end()) ? it->second : 0;
}

void Battle_GTournament::AddGuildScore(uint32_t guild_id, uint32_t score) {
    guild_scores_[guild_id] += score;

    spdlog::debug("GTournament Battle {}: Guild {} score increased to {}",
                 battle_id_, guild_id, guild_scores_[guild_id]);

    // 检查是否达到胜利条件
    if (guild_scores_[guild_id] >= max_score_) {
        Judge();
    }
}

// ========== Battle_MunpaField (门派战场) ==========

Battle_MunpaField::Battle_MunpaField(uint32_t battle_id, uint32_t munpa_id_1, uint32_t munpa_id_2)
    : Battle(battle_id) {
    participant_munpas_.push_back(munpa_id_1);
    participant_munpas_.push_back(munpa_id_2);

    munpa_scores_[munpa_id_1] = 0;
    munpa_scores_[munpa_id_2] = 0;

    spdlog::info("Created MunpaField Battle {}: Munpa {} vs Munpa {}",
                 battle_id, munpa_id_1, munpa_id_2);
}

void Battle_MunpaField::OnFightStart() {
    spdlog::info("MunpaField Battle {}: Fight Started", battle_id_);
}

void Battle_MunpaField::OnDestroy() {
    spdlog::info("MunpaField Battle {}: Destroyed", battle_id_);
}

bool Battle_MunpaField::Judge() {
    for (const auto& pair : munpa_scores_) {
        if (pair.second >= win_score_) {
            return true;
        }
    }
    return false;
}

void Battle_MunpaField::Victory(int winner_team_num) {
    uint32_t winner_munpa_id = participant_munpas_[winner_team_num];
    spdlog::info("MunpaField Battle {}: Munpa {} Wins!", battle_id_, winner_munpa_id);
}

void Battle_MunpaField::Draw() {
    spdlog::info("MunpaField Battle {}: Draw", battle_id_);
}

void Battle_MunpaField::OnTeamMemberAdd(uint64_t member_id) {
    spdlog::debug("MunpaField Battle {}: Member {} joined", battle_id_, member_id);
}

void Battle_MunpaField::OnTeamMemberDie(uint64_t member_id) {
    spdlog::debug("MunpaField Battle {}: Member {} died", battle_id_, member_id);
}

void Battle_MunpaField::OnMonsterDistribute() {
    spdlog::debug("MunpaField Battle {}: Distributing monster drops", battle_id_);
}

uint32_t Battle_MunpaField::GetMunpaScore(uint32_t munpa_id) const {
    auto it = munpa_scores_.find(munpa_id);
    return (it != munpa_scores_.end()) ? it->second : 0;
}

void Battle_MunpaField::AddMunpaScore(uint32_t munpa_id, uint32_t score) {
    munpa_scores_[munpa_id] += score;
    spdlog::debug("MunpaField Battle {}: Munpa {} score increased to {}",
                 battle_id_, munpa_id, munpa_scores_[munpa_id]);
}

// ========== Battle_MurimField (武林战场) ==========

Battle_MurimField::Battle_MurimField(uint32_t battle_id)
    : Battle(battle_id) {
    spdlog::info("Created MurimField Battle {}", battle_id);
}

void Battle_MurimField::OnFightStart() {
    spdlog::info("MurimField Battle {}: Fight Started", battle_id_);
}

void Battle_MurimField::OnDestroy() {
    spdlog::info("MurimField Battle {}: Destroyed", battle_id_);
}

bool Battle_MurimField::Judge() {
    // 武林战场：积分制
    for (const auto& pair : player_scores_) {
        if (pair.second >= max_score_) {
            return true;
        }
    }
    return false;
}

void Battle_MurimField::Victory(int winner_team_num) {
    spdlog::info("MurimField Battle {}: Team {} Wins!", battle_id_, winner_team_num);
}

void Battle_MurimField::Draw() {
    spdlog::info("MurimField Battle {}: Draw", battle_id_);
}

void Battle_MurimField::OnTeamMemberAdd(uint64_t member_id) {
    spdlog::debug("MurimField Battle {}: Member {} joined", battle_id_, member_id);
}

void Battle_MurimField::OnTeamMemberDie(uint64_t member_id) {
    spdlog::debug("MurimField Battle {}: Member {} died", battle_id_, member_id);
}

void Battle_MurimField::OnMonsterDistribute() {
    spdlog::debug("MurimField Battle {}: Distributing monster drops", battle_id_);
}

uint32_t Battle_MurimField::GetPlayerScore(uint64_t player_id) const {
    auto it = player_scores_.find(player_id);
    return (it != player_scores_.end()) ? it->second : 0;
}

void Battle_MurimField::AddPlayerScore(uint64_t player_id, uint32_t score) {
    player_scores_[player_id] += score;
    spdlog::debug("MurimField Battle {}: Player {} score increased to {}",
                 battle_id_, player_id, player_scores_[player_id]);
}

// ========== Battle_SiegeWar (攻城战) ==========

Battle_SiegeWar::Battle_SiegeWar(uint32_t battle_id, uint32_t attacking_guild_id, uint32_t defending_guild_id)
    : Battle(battle_id)
    , attacking_guild_id_(attacking_guild_id)
    , defending_guild_id_(defending_guild_id) {

    // 初始化据点
    InitializeBases();

    spdlog::info("Created SiegeWar Battle {}: Attacking Guild {} vs Defending Guild {}",
                 battle_id, attacking_guild_id_, defending_guild_id_);
}

void Battle_SiegeWar::OnFightStart() {
    spdlog::info("SiegeWar Battle {}: Fight Started", battle_id_);
}

void Battle_SiegeWar::OnDestroy() {
    spdlog::info("SiegeWar Battle {}: Destroyed", battle_id_);
}

bool Battle_SiegeWar::Judge() {
    // 攻城胜利条件:
    // 1. 城门被摧毁
    // 2. 占领所有据点
    // 3. 防守方全部阵亡

    if (gate_destroyed_) {
        spdlog::info("SiegeWar Battle {}: Gate destroyed, attackers win", battle_id_);
        return true;
    }

    uint8_t captured_bases = GetCapturedBaseCount(attacking_guild_id_);
    if (captured_bases >= base_ids_.size()) {
        spdlog::info("SiegeWar Battle {}: All bases captured by attackers", battle_id_);
        return true;
    }

    // 检查防守方存活人数
    uint32_t defender_alive_count = GetAliveMemberCount(defending_guild_id_);
    if (defender_alive_count == 0) {
        spdlog::info("SiegeWar Battle {}: All defenders eliminated", battle_id_);
        return true;
    }

    return false;
}

void Battle_SiegeWar::Victory(int winner_team_num) {
    if (winner_team_num == 0) {
        spdlog::info("SiegeWar Battle {}: Attacking Guild {} Wins!", battle_id_, attacking_guild_id_);
    } else {
        spdlog::info("SiegeWar Battle {}: Defending Guild {} Wins!", battle_id_, defending_guild_id_);
    }
}

void Battle_SiegeWar::Draw() {
    spdlog::info("SiegeWar Battle {}: Draw", battle_id_);
}

void Battle_SiegeWar::OnTeamMemberAdd(uint64_t member_id) {
    spdlog::debug("SiegeWar Battle {}: Member {} joined", battle_id_, member_id);
}

void Battle_SiegeWar::OnTeamMemberDie(uint64_t member_id) {
    spdlog::debug("SiegeWar Battle {}: Member {} died", battle_id_, member_id);
}

void Battle_SiegeWar::OnMonsterDistribute() {
    spdlog::debug("SiegeWar Battle {}: Distributing monster drops", battle_id_);
}

bool Battle_SiegeWar::CaptureBase(uint32_t base_id, uint64_t capturer_id) {
    // 检查据点是否可以被占领
    if (!CanCaptureBase(base_id, capturer_id)) {
        spdlog::warn("SiegeWar Battle {}: Base {} cannot be captured by player {}",
                battle_id_, base_id, capturer_id);
        return false;
    }

    uint32_t guild_id = GetObjectGuild(capturer_id);
    base_owners_[base_id] = guild_id;

    spdlog::info("SiegeWar Battle {}: Base {} captured by Guild {}",
                 battle_id_, base_id, base_owners_[base_id]);

    return true;
}

uint8_t Battle_SiegeWar::GetCapturedBaseCount(uint32_t guild_id) const {
    uint8_t count = 0;
    for (const auto& pair : base_owners_) {
        if (pair.second == guild_id) {
            count++;
        }
    }
    return count;
}

// ========== Battle_Suryun (修炼) ==========

Battle_Suryun::Battle_Suryun(uint32_t battle_id, uint64_t player_id)
    : Battle(battle_id)
    , training_player_id_(player_id) {

    spdlog::info("Created Suryun Battle {}: Player {}", battle_id, player_id);
}

void Battle_Suryun::OnFightStart() {
    spdlog::info("Suryun Battle {}: Fight Started for Player {}", battle_id_, training_player_id_);
    progress_ = 0.0f;
}

void Battle_Suryun::OnDestroy() {
    spdlog::info("Suryun Battle {}: Destroyed. Progress: {}%", battle_id_, progress_);
}

bool Battle_Suryun::Judge() {
    return IsCompleted() || training_player_id_ == 0;
}

void Battle_Suryun::Victory(int winner_team_num) {
    spdlog::info("Suryun Battle {}: Player {} completed training!", battle_id_, training_player_id_);

    // 发放修炼奖励
    GrantTrainingRewards();
}

void Battle_Suryun::Draw() {
    spdlog::info("Suryun Battle {}: Player {} failed (Draw)", battle_id_, training_player_id_);
}

void Battle_Suryun::OnTeamMemberAdd(uint64_t member_id) {
    // 修炼通常是单人，但可能有协助
}

void Battle_Suryun::OnTeamMemberDie(uint64_t member_id) {
    spdlog::info("Suryun Battle {}: Player {} died during training", battle_id_, member_id_);
}

void Battle_Suryun::OnMonsterDistribute() {
    // 修炼的怪物不掉落，或给修炼者
}

// ========== Battle_VimuStreet (街战) ==========

Battle_VimuStreet::Battle_VimuStreet(uint32_t battle_id, uint64_t player_id, uint64_t target_id)
    : Battle(battle_id)
    , initiator_id_(player_id)
    , target_id_(target_id) {

    win_counts_[player_id] = 0;
    lose_counts_[player_id] = 0;
    win_counts_[target_id] = 0;
    lose_counts_[target_id] = 0;

    spdlog::info("Created VimuStreet Battle {}: Initator {} vs Target {}",
                 battle_id, player_id, target_id);
}

void Battle_VimuStreet::OnFightStart() {
    spdlog::info("VimuStreet Battle {}: Fight Started", battle_id_);
}

void Battle_VimuStreet::OnDestroy() {
    spdlog::info("VimuStreet Battle {}: Destroyed", battle_id_);

    // 计算PK惩罚
    CalculatePKPenalty();
}

bool Battle_VimuStreet::Judge() {
    // 街战判定:
    // 1. 一方死亡
    // 2. 一方逃跑
    // 3. 一方超时未行动

    // 检查双方存活状态
    bool initiator_alive = IsPlayerAlive(initiator_id_);
    bool target_alive = IsPlayerAlive(target_id_);

    if (!initiator_alive || !target_alive) {
        return true;
    }

    return false;
}

void Battle_VimuStreet::Victory(int winner_team_num) {
    uint64_t winner_id = (winner_team_num == 0) ? initiator_id_ : target_id_;
    uint64_t loser_id = (winner_id == initiator_id_) ? target_id_ : initiator_id_;

    spdlog::info("VimuStreet Battle {}: Winner is {}", battle_id_, winner_id);

    // 增加获胜次数
    AddWin(winner_id);
    AddLose(loser_id);

    // 计算PK奖励/惩罚
    CalculatePKRewardsAndPenalties(winner_id, loser_id);
}

void Battle_VimuStreet::Draw() {
    spdlog::info("VimuStreet Battle {}: Draw", battle_id_);
}

void Battle_VimuStreet::OnTeamMemberAdd(uint64_t member_id) {
    // 街战通常是1v1
}

void Battle_VimuStreet::OnTeamMemberDie(uint64_t member_id) {
    spdlog::info("VimuStreet Battle {}: Member {} died", battle_id_, member_id);
}

void Battle_VimuStreet::OnMonsterDistribute() {
    // 街战没有怪物掉落
}

uint16_t Battle_VimuStreet::GetWinCount(uint64_t player_id) const {
    auto it = win_counts_.find(player_id);
    return (it != win_counts_.end()) ? it->second : 0;
}

uint16_t Battle_VimuStreet::GetLoseCount(uint64_t player_id) const {
    auto it = lose_counts_.find(player_id);
    return (it != lose_counts_.end()) ? it->second : 0;
}

void Battle_VimuStreet::AddWin(uint64_t player_id) {
    win_counts_[player_id]++;
}

void Battle_VimuStreet::AddLose(uint64_t player_id) {
    lose_counts_[player_id]++;
}

// ========== BattleFactory ==========

std::unique_ptr<Battle> BattleFactory::CreateBattle(
    BattleType battle_type,
    uint32_t battle_id,
    const std::vector<uint64_t>& participants
) {
    switch (battle_type) {
    case BattleType::kGTournament:
        if (participants.size() >= 2) {
            // 从participants获取公会ID
            uint32_t guild_id_1 = GetPlayerGuild(participants[0]);
            uint32_t guild_id_2 = GetPlayerGuild(participants[1]);
            if (guild_id_1 != 0 && guild_id_2 != 0) {
                return CreateGTournament(battle_id, guild_id_1, guild_id_2);
            }
        }
        break;

    case BattleType::kMunpaField:
        if (participants.size() >= 2) {
            // 从participants获取门派ID
            uint32_t munpa_id_1 = GetPlayerMunpa(participants[0]);
            uint32_t munpa_id_2 = GetPlayerMunpa(participants[1]);
            if (munpa_id_1 != 0 && munpa_id_2 != 0) {
                return CreateMunpaField(battle_id, munpa_id_1, munpa_id_2);
            }
        }
        break;

    case BattleType::kMurimField:
        return CreateMurimField(battle_id);

    case BattleType::kSiegeWar:
        if (participants.size() >= 2) {
            // 从participants获取攻守方公会ID
            uint32_t attacking_guild_id = GetPlayerGuild(participants[0]);
            uint32_t defending_guild_id = GetPlayerGuild(participants[1]);
            if (attacking_guild_id != 0 && defending_guild_id != 0) {
                return CreateSiegeWar(battle_id, attacking_guild_id, defending_guild_id);
            }
        }
        break;

    case BattleType::kSuryun:
        if (!participants.empty()) {
            return CreateSuryun(battle_id, participants[0]);
        }
        break;

    case BattleType::kVimuStreet:
        if (participants.size() >= 2) {
            return CreateVimuStreet(battle_id, participants[0], participants[1]);
        }
        break;

    default:
        spdlog::warn("Unknown battle type: {}", static_cast<uint8_t>(battle_type));
        break;
    }

    return nullptr;
}

std::unique_ptr<Battle_GTournament> BattleFactory::CreateGTournament(
    uint32_t battle_id,
    uint32_t guild_id_1,
    uint32_t guild_id_2
) {
    return std::make_unique<Battle_GTournament>(battle_id, guild_id_1, guild_id_2);
}

std::unique_ptr<Battle_MunpaField> BattleFactory::CreateMunpaField(
    uint32_t battle_id,
    uint32_t munpa_id_1,
    uint32_t munpa_id_2
) {
    return std::make_unique<Battle_MunpaField>(battle_id, munpa_id_1, munpa_id_2);
}

std::unique_ptr<Battle_MurimField> BattleFactory::CreateMurimField(uint32_t battle_id) {
    return std::make_unique<Battle_MurimField>(battle_id);
}

std::unique_ptr<Battle_SiegeWar> BattleFactory::CreateSiegeWar(
    uint32_t battle_id,
    uint32_t attacking_guild_id,
    uint32_t defending_guild_id
) {
    return std::make_unique<Battle_SiegeWar>(battle_id, attacking_guild_id, defending_guild_id);
}

std::unique_ptr<Battle_Suryun> BattleFactory::CreateSuryun(
    uint32_t battle_id,
    uint64_t player_id
) {
    return std::make_unique<Battle_Suryun>(battle_id, player_id);
}

std::unique_ptr<Battle_VimuStreet> BattleFactory::CreateVimuStreet(
    uint32_t battle_id,
    uint64_t player_id,
    uint64_t target_id
) {
    return std::make_unique<Battle_VimuStreet>(battle_id, player_id, target_id);
}

// ========== Battle_GTournament 辅助函数 ==========

void Battle_GTournament::BroadcastBattleStart() {
    // 构建战斗开始消息
    // 通知所有公会成员战斗开始
    spdlog::info("GTournament Battle {}: Broadcasting battle start to guilds {} and {}",
             battle_id_, participant_guilds_[0], participant_guilds_[1]);

    // TODO [网络/数据库]: 发送网络消息给所有公会成员
}

void Battle_GTournament::BroadcastBattleEnd() {
    spdlog::info("GTournament Battle {}: Broadcasting battle end", battle_id_);
    // TODO [网络/数据库]: 发送战斗结束消息
}

void Battle_GTournament::BroadcastVictory(uint32_t winner_guild_id) {
    spdlog::info("GTournament Battle {}: Broadcasting victory for guild {}",
             battle_id_, winner_guild_id);
    // TODO [网络/数据库]: 发送胜利消息
}

void Battle_GTournament::BroadcastDraw() {
    spdlog::info("GTournament Battle {}: Broadcasting draw", battle_id_);
    // TODO [网络/数据库]: 发送平局消息
}

void Battle_GTournament::GrantVictoryRewards(uint32_t guild_id) {
    // 胜利奖励: 公会经验、金币、声望
    uint32_t guild_exp = 1000;
    uint32_t gold = 5000;
    uint32_t reputation = 100;

    spdlog::info("GTournament Battle {}: Granting victory rewards to guild {}: "
             "exp={}, gold={}, reputation={}",
             battle_id_, guild_id, guild_exp, gold, reputation);

    // TODO [网络/数据库]: 发放奖励给公会 (经验、金币、声望)
}

void Battle_GTournament::GrantDrawRewards() {
    // 平局奖励(减半)
    for (uint32_t guild_id : participant_guilds_) {
        uint32_t guild_exp = 500;
        uint32_t gold = 2500;
        uint32_t reputation = 50;

        spdlog::info("GTournament Battle {}: Granting draw rewards to guild {}: "
                 "exp={}, gold={}, reputation={}",
                 battle_id_, guild_id, guild_exp, gold, reputation);

        // TODO [网络/数据库]: 发放奖励 (经验、金币、声望)
    }
}

void Battle_GTournament::UpdateGuildStats(uint32_t guild_id, bool is_victory) {
    if (is_victory) {
        spdlog::info("GTournament Battle {}: Updating guild {} stats: victory",
                 battle_id_, guild_id);
        // TODO [网络/数据库]: 更新公会胜利统计
    } else {
        spdlog::info("GTournament Battle {}: Updating guild {} stats: draw",
                 battle_id_, guild_id);
        // TODO [网络/数据库]: 更新公会平局统计
    }
}

void Battle_GTournament::TeleportPlayersToSpawnPoint() {
    spdlog::info("GTournament Battle {}: Teleporting players to spawn points", battle_id_);
    // TODO [网络/数据库]: 传送所有玩家回出生点
}

void Battle_GTournament::DistributeRewards() {
    spdlog::info("GTournament Battle {}: Distributing battle rewards", battle_id_);
    // TODO [网络/数据库]: 根据战斗结果发放奖励
}

void Battle_GTournament::CleanupBattleData() {
    spdlog::info("GTournament Battle {}: Cleaning up battle data", battle_id_);
    // 清理所有战斗相关数据
    alive_member_counts_.clear();
    team_members_.clear();
    battle_state_ = BattleState::kEnded;
}

void Battle_GTournament::DistributeMonsterDropsByScore() {
    // 根据公会分数分配掉落
    uint32_t total_score = 0;
    for (const auto& pair : guild_scores_) {
        total_score += pair.second;
    }

    if (total_score == 0) {
        // 分数相等,平均分配
        spdlog::debug("GTournament Battle {}: Equal scores, distributing drops evenly",
                 battle_id_);
        return;
    }

    // 按分数比例分配
    for (const auto& pair : guild_scores_) {
        float ratio = static_cast<float>(pair.second) / total_score;
        spdlog::debug("GTournament Battle {}: Guild {} gets {:.1f}% of drops",
                 battle_id_, pair.first, ratio * 100.0f);
        // TODO [网络/数据库]: 按比例分配掉落物品给公会成员
    }
}

uint32_t Battle_GTournament::GetMemberGuild(uint64_t member_id) {
    // 从GameObjectManager获取对象的公会ID
    // 简化实现: 根据member_id返回公会ID
    for (uint32_t guild_id : participant_guilds_) {
        auto it = team_members_.find(guild_id);
        if (it != team_members_.end() && it->second.count(member_id)) {
            return guild_id;
        }
    }
    // TODO: 从GameObject获取所属公会 (需要GameObjectManager集成)
    return 0;  // 未找到
}

// ========== Battle_SiegeWar 辅助函数 ==========

void Battle_SiegeWar::InitializeBases() {
    // 初始化攻城战据点
    // 据点1: 城门
    // 据点2-5: 城墙塔楼
    base_ids_ = {1, 2, 3, 4, 5};

    // 初始所有据点归防守方
    for (uint32_t base_id : base_ids_) {
        base_owners_[base_id] = defending_guild_id_;
    }

    spdlog::info("SiegeWar Battle {}: Initialized {} bases, all owned by defending guild {}",
             battle_id_, base_ids_.size(), defending_guild_id_);
}

bool Battle_SiegeWar::CanCaptureBase(uint32_t base_id, uint64_t capturer_id) const {
    // 检查据点是否存在
    if (base_owners_.find(base_id) == base_owners_.end()) {
        return false;
    }

    uint32_t guild_id = GetObjectGuild(capturer_id);

    // 只有进攻方可以占领据点
    if (guild_id != attacking_guild_id_) {
        return false;
    }

    // 据点已经被进攻方占领
    if (base_owners_.at(base_id) == attacking_guild_id_) {
        return false;
    }

    return true;
}

uint32_t Battle_SiegeWar::GetObjectGuild(uint64_t object_id) const {
    // 简化实现: 根据ID判断
    for (const auto& pair : guild_members_) {
        if (pair.second.count(object_id)) {
            return pair.first;
        }
    }
    // TODO: 从GameObject获取所属公会 (需要GameObjectManager集成)
    return 0;
}

uint32_t Battle_SiegeWar::GetAliveMemberCount(uint32_t guild_id) const {
    auto it = alive_member_counts_.find(guild_id);
    return (it != alive_member_counts_.end()) ? it->second : 0;
}

// ========== Battle_VimuStreet 辅助函数 ==========

bool Battle_VimuStreet::IsPlayerAlive(uint64_t player_id) const {
    // 从GameObjectManager获取对象并检查IsAlive()
    GameObject* obj = GameObjectManager::GetObject(player_id);
    if (obj) {
        bool alive = obj->IsAlive();
        spdlog::trace("VimuStreet Battle {}: Player {} is alive = {}",
                     battle_id_, player_id, alive);
        return alive;
    }
    spdlog::warn("VimuStreet Battle {}: Player {} object not found", battle_id_, player_id);
    return false;
}

void Battle_VimuStreet::CalculatePKPenalty() {
    spdlog::info("VimuStreet Battle {}: Calculating PK penalty", battle_id_);

    // PK惩罚:
    // 1. 减少武林点数
    // 2. 增加PK计数
    // 3. 可能红名惩罚

    spdlog::debug("VimuStreet Battle {}: PK penalty calculation completed", battle_id_);
    // TODO [网络/数据库]: 实现PK惩罚逻辑 (武林点数扣除、PK计数、红名状态)
}

void Battle_VimuStreet::CalculatePKRewardsAndPenalties(uint64_t winner_id, uint64_t loser_id) {
    spdlog::info("VimuStreet Battle {}: Calculating PK rewards and penalties",
             battle_id_);

    // PK奖励:
    // 1. 获胜方获得武林点数
    // 2. 获胜方声望提升

    // PK惩罚:
    // 1. 失败方损失武林点数
    // 2. 失败方装备耐久度降低
    // 3. 失败方可能掉落物品(红名玩家)

    uint32_t murim_points_reward = 100;
    uint32_t murim_points_penalty = 50;

    spdlog::info("VimuStreet Battle {}: Winner {} gains {} Murim points",
             battle_id_, winner_id, murim_points_reward);
    spdlog::info("VimuStreet Battle {}: Loser {} loses {} Murim points",
             battle_id_, loser_id, murim_points_penalty);

    // TODO [网络/数据库]: 应用PK奖励和惩罚 (武林点数、声望、装备耐久、物品掉落)
}

// ========== Battle_Suryun 辅助函数 ==========

void Battle_Suryun::GrantTrainingRewards() {
    // 修炼奖励:
    // 1. 经验值
    // 2. 技能点
    // 3. 修炼声望

    uint32_t exp_reward = 10000;
    uint32_t skill_points_reward = 5;
    uint32_t reputation_reward = 50;

    spdlog::info("Suryun Battle {}: Granting training rewards to player {}: "
             "exp={}, skill_points={}, reputation={}",
             battle_id_, training_player_id_, exp_reward, skill_points_reward, reputation_reward);

    // TODO [网络/数据库]: 发放奖励给玩家 (经验值、技能点、修炼声望)
}

// ========== BattleFactory 辅助函数 ==========

uint32_t BattleFactory::GetPlayerGuild(uint64_t player_id) {
    // 简化实现: 返回0表示未找到
    spdlog::debug("BattleFactory: Getting guild for player {}", player_id);
    // TODO: 从GameObject获取玩家所属公会 (需要GameObjectManager集成)
    return 0;
}

uint32_t BattleFactory::GetPlayerMunpa(uint64_t player_id) {
    // 简化实现: 返回0表示未找到
    spdlog::debug("BattleFactory: Getting munpa for player {}", player_id);
    // TODO: 从GameObject获取玩家所属门派 (需要GameObjectManager集成)
    return 0;
}

} // namespace Game
} // namespace Murim
