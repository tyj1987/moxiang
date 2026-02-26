#include "AI.hpp"
#include "shared/battle/Battle.hpp"
#include "shared/character/CharacterManager.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <unordered_map>
#include <random>
#include <sstream>
#include <iomanip>

using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Game {

// ========== Monster Data Structure (Temporary until Monster class is fully implemented) ==========
// This will be replaced by the proper Monster class when it's created
struct MonsterData {
    uint64_t id;
    uint32_t template_id;
    Position position;
    uint32_t hp;
    uint32_t max_hp;
    uint16_t level;
    uint32_t group_id;
    bool is_boss;
    bool is_alive;
    std::chrono::system_clock::time_point death_time;  // 死亡时间
    uint32_t respawn_delay;  // 复活延迟(毫秒)

    MonsterData()
        : id(0), template_id(0), hp(100), max_hp(100), level(1), group_id(0)
        , is_boss(false), is_alive(true), respawn_delay(5000) {}  // 默认5秒复活
};

// Temporary monster storage
static std::unordered_map<uint64_t, MonsterData> g_monster_data;
static uint64_t g_next_monster_id = 10000;

// Forward declaration for Monster class
class Monster {
public:
    Monster(uint64_t id, uint32_t template_id, const Position& pos, uint32_t group_id = 0)
        : id_(id), template_id_(template_id), position_(pos), group_id_(group_id) {
        data_.id = id;
        data_.template_id = template_id;
        data_.position = pos;
        data_.group_id = group_id;
        data_.hp = 100;
        data_.max_hp = 100;
        data_.level = 1;
        data_.is_boss = false;
        data_.is_alive = true;
        g_monster_data[id] = data_;
    }

    uint64_t GetID() const { return id_; }
    uint32_t GetTemplateID() const { return template_id_; }
    const Position& GetPosition() const { return position_; }
    void SetPosition(const Position& pos) { position_ = pos; data_.position = pos; }
    uint32_t GetGroupID() const { return group_id_; }
    bool IsBoss() const { return data_.is_boss; }
    bool IsAlive() const { return data_.is_alive; }
    void SetAlive(bool alive) { data_.is_alive = alive; }
    uint32_t GetHP() const { return data_.hp; }
    uint32_t GetMaxHP() const { return data_.max_hp; }
    uint16_t GetLevel() const { return data_.level; }

private:
    uint64_t id_;
    uint32_t template_id_;
    Position position_;
    uint32_t group_id_;
    MonsterData data_;
};

// Utility: Generate unique monster ID
static uint64_t GenerateMonsterID() {
    return g_next_monster_id++;
}

// Utility: Get monster data
static MonsterData* GetMonsterData(uint64_t monster_id) {
    auto it = g_monster_data.find(monster_id);
    if (it != g_monster_data.end()) {
        return &it->second;
    }
    return nullptr;
}

// ========== AIStateMachine ==========

AIStateMachine::AIStateMachine(uint64_t owner_id, const AIConfig& config)
    : owner_id_(owner_id)
    , config_(config)
    , current_state_(AIState::kSpawn)
    , previous_state_(AIState::kNone)
    , current_patrol_index_(0)
    , patrol_wait_start_()
    , last_attack_time_()
    , current_target_id_(0)
    , current_position_()
    , spawn_position_()
    , is_returning_(false)
    , state_enter_time_(std::chrono::system_clock::now())
{
    spdlog::debug("AIStateMachine created for monster {}", owner_id);
}

void AIStateMachine::Process(AIEventType event_type, void* event_data)
{
    // 严格遵循 legacy GSTATEMACHINE.Process(obj, eSEVENT_XXX, pParam)
    // Legacy 代码会根据事件类型和当前状态进行状态转换

    (void)event_data;  // 暂时不使用事件数据

    switch (event_type) {
        case AIEventType::kProcess:
            // eSEVENT_Process - 每个tick都调用
            // 执行当前状态的逻辑
            Update(std::chrono::milliseconds(16));
            break;

        case AIEventType::kAttack:
            // eSEVENT_Attack - 攻击事件
            // 可能切换到战斗状态
            if (current_state_ == AIState::kIdle || current_state_ == AIState::kPatrol) {
                ChangeState(AIState::kChase);
            }
            break;

        case AIEventType::kDamaged:
            // eSEVENT_Damaged - 受伤事件
            // 中立类型被攻击后变成主动
            if (config_.behavior_type == AIBehaviorType::kNeutral) {
                if (current_state_ == AIState::kIdle || current_state_ == AIState::kPatrol) {
                    ChangeState(AIState::kChase);
                }
            }
            break;

        case AIEventType::kSeeEnemy:
            // eSEVENT_SeeEnemy - 发现敌人
            // 主动类型会切换到追击
            if (config_.behavior_type == AIBehaviorType::kAggressive) {
                if (current_state_ == AIState::kIdle || current_state_ == AIState::kPatrol) {
                    ChangeState(AIState::kChase);
                }
            }
            break;

        case AIEventType::kReturn:
            // eSEVENT_Return - 返回出生点
            ChangeState(AIState::kReturn);
            break;

        case AIEventType::kDead:
            // eSEVENT_Dead - 死亡事件
            ChangeState(AIState::kDead);
            break;

        default:
            break;
    }
}

void AIStateMachine::Update(std::chrono::milliseconds delta_time) {
    // 衰减仇恨
    DecayAllAggro();

    // 检查状态转换
    CheckStateTransitions();

    // 执行当前状态逻辑
    switch (current_state_) {
        case AIState::kIdle:
            ExecuteIdleState();
            break;

        case AIState::kPatrol:
            ExecutePatrolState();
            break;

        case AIState::kChase:
            ExecuteChaseState();
            break;

        case AIState::kAttack:
            ExecuteAttackState();
            break;

        case AIState::kReturn:
            ExecuteReturnState();
            break;

        case AIState::kFlee:
            ExecuteFleeState();
            break;

        case AIState::kDead:
            ExecuteDeadState();
            break;

        case AIState::kSpawn:
            // 生成完成后进入待机
            ChangeState(AIState::kIdle);
            break;

        case AIState::kDespawn:
            // 消失中,不做任何操作
            break;

        default:
            break;
    }
}

void AIStateMachine::ChangeState(AIState new_state) {
    if (current_state_ == new_state) {
        return;
    }

    spdlog::debug("Monster {} AI state change: {} -> {}",
                 owner_id_,
                 static_cast<int>(current_state_),
                 static_cast<int>(new_state));

    previous_state_ = current_state_;
    current_state_ = new_state;
    state_enter_time_ = std::chrono::system_clock::now();

    // 状态进入时的特殊处理
    if (new_state == AIState::kReturn) {
        is_returning_ = true;
        current_target_id_ = 0;
    }
}

// ========== 仇恨系统 ==========

void AIStateMachine::AddAggro(uint64_t target_id, uint32_t aggro_value) {
    auto it = aggro_list_.find(target_id);
    if (it != aggro_list_.end()) {
        it->second.AddAggro(aggro_value);
    } else {
        AggroEntry entry;
        entry.target_id = target_id;
        entry.aggro_value = aggro_value;
        entry.last_update = std::chrono::system_clock::now();
        aggro_list_[target_id] = entry;
    }

    // 如果是主动攻击类型,立即切换到追击状态
    if (config_.behavior_type == AIBehaviorType::kAggressive) {
        if (current_state_ == AIState::kIdle || current_state_ == AIState::kPatrol) {
            ChangeState(AIState::kChase);
        }
    }
}

void AIStateMachine::RemoveAggro(uint64_t target_id) {
    aggro_list_.erase(target_id);

    if (current_target_id_ == target_id) {
        current_target_id_ = 0;
    }
}

std::optional<uint64_t> AIStateMachine::GetTopAggroTarget() {
    if (aggro_list_.empty()) {
        return std::nullopt;
    }

    auto max_it = std::max_element(
        aggro_list_.begin(),
        aggro_list_.end(),
        [](const auto& a, const auto& b) {
            return a.second.aggro_value < b.second.aggro_value;
        }
    );

    return max_it->first;
}

void AIStateMachine::ClearAggro() {
    aggro_list_.clear();
    current_target_id_ = 0;
}

void AIStateMachine::DecayAllAggro() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - state_enter_time_
    ).count();

    if (elapsed < config_.aggro_decay_time) {
        return;
    }

    for (auto& [target_id, entry] : aggro_list_) {
        entry.Decay(config_.aggro_decay_rate);
    }

    // 移除过低的仇恨
    for (auto it = aggro_list_.begin(); it != aggro_list_.end(); ) {
        if (it->second.aggro_value < 10) {
            it = aggro_list_.erase(it);
        } else {
            ++it;
        }
    }
}

// ========== 事件处理 ==========

void AIStateMachine::OnAttacked(uint64_t attacker_id, uint32_t damage) {
    // 仇恨值 = 伤害 * 2
    AddAggro(attacker_id, damage * 2);

    // 中立类型被攻击后变成主动
    if (config_.behavior_type == AIBehaviorType::kNeutral) {
        if (current_state_ == AIState::kIdle || current_state_ == AIState::kPatrol) {
            ChangeState(AIState::kChase);
        }
    }
}

void AIStateMachine::OnTargetEnterRange(uint64_t target_id) {
    // 被动类型不会主动攻击
    if (config_.behavior_type == AIBehaviorType::kPassive) {
        return;
    }

    // 检查是否在侦测范围内
    if (IsTargetInDetectionRange(target_id)) {
        AddAggro(target_id, 1);  // 初始仇恨值
    }
}

void AIStateMachine::OnTargetLeaveRange(uint64_t target_id) {
    // 目标离开范围,不做处理
    // 仇恨会自然衰减
}

// ========== 辅助方法 ==========

std::optional<float> AIStateMachine::GetDistanceToTarget(uint64_t target_id) {
    // FIXED: 从角色管理器获取目标位置
    auto target_opt = CharacterManager::Instance().GetCharacter(target_id);
    if (!target_opt.has_value()) {
        return std::nullopt;
    }

    // 计算距离 (使用2D距离，对应 legacy CalcDistanceXZ)
    Position target_pos(target_opt->x, target_opt->y, target_opt->z);
    return current_position_.Distance2D(target_pos);
}

bool AIStateMachine::IsTargetInAttackRange(uint64_t target_id) {
    auto distance = GetDistanceToTarget(target_id);
    if (!distance.has_value()) {
        return false;
    }
    return *distance <= config_.attack_range;
}

bool AIStateMachine::IsTargetInDetectionRange(uint64_t target_id) {
    auto distance = GetDistanceToTarget(target_id);
    if (!distance.has_value()) {
        return false;
    }
    return *distance <= config_.detection_range;
}

// ========== 状态执行 ==========

void AIStateMachine::ExecuteIdleState() {
    // 待机状态,不做任何操作
    // 检查是否有仇恨目标
    auto target = GetTopAggroTarget();
    if (target.has_value()) {
        ChangeState(AIState::kChase);
        current_target_id_ = *target;
    }
}

void AIStateMachine::ExecutePatrolState() {
    // 如果有仇恨目标,切换到追击
    auto target = GetTopAggroTarget();
    if (target.has_value()) {
        ChangeState(AIState::kChase);
        current_target_id_ = *target;
        return;
    }

    // 对应 legacy AI巡逻逻辑 - CMonster::Process
    // 1. 移动到下一个巡逻点
    // 2. 到达后等待指定时间
    // 3. 移动到下一个点

    if (config_.patrol_points.empty()) {
        ChangeState(AIState::kIdle);
        return;
    }

    // 获取当前巡逻点 - 对应 legacy 巡逻点获取
    if (current_patrol_index_ >= config_.patrol_points.size()) {
        current_patrol_index_ = 0;  // 循环巡逻
    }

    const auto& patrol_point = config_.patrol_points[current_patrol_index_];

    // 对应 legacy: 检查是否到达巡逻点
    // 简化: 使用距离判断 (距离 < 1.0m 认为到达)
    float distance = current_position_.Distance2D(patrol_point);

    if (distance < 1.0f) {
        // 已到达巡逻点 - 对应 legacy 巡逻点等待
        spdlog::trace("Monster {} reached patrol point {}, waiting...",
                     owner_id_, current_patrol_index_);

        // FIXED: 实现巡逻点等待计时
        auto now = std::chrono::system_clock::now();
        if (patrol_wait_start_.time_since_epoch().count() == 0) {
            // 首次到达，开始等待
            patrol_wait_start_ = now;
            return;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - patrol_wait_start_
        ).count();

        if (elapsed >= config_.patrol_wait_time) {
            // 等待完成，移动到下一个点
            current_patrol_index_++;
            if (current_patrol_index_ >= config_.patrol_points.size()) {
                current_patrol_index_ = 0;  // 循环
            }
            patrol_wait_start_ = std::chrono::system_clock::time_point();  // 重置
        }
    } else {
        // 移动向巡逻点 - 对应 legacy 巡逻移动
        // 调用移动系统接口 - 对应 legacy 移动逻辑
        MoveTo(patrol_point);
        spdlog::trace("Monster {} moving to patrol point {} ({},{}), distance: {}",
                     owner_id_, current_patrol_index_,
                     patrol_point.x, patrol_point.y, distance);
    }
}

void AIStateMachine::ExecuteChaseState() {
    auto target = GetTopAggroTarget();
    if (!target.has_value()) {
        // 没有仇恨目标,返回
        ChangeState(AIState::kReturn);
        return;
    }

    current_target_id_ = *target;

    // 检查是否在攻击范围
    if (IsTargetInAttackRange(current_target_id_)) {
        ChangeState(AIState::kAttack);
        return;
    }

    // 检查是否超出追击范围
    auto distance = GetDistanceToTarget(current_target_id_);
    if (distance.has_value() && *distance > config_.chase_range) {
        ChangeState(AIState::kReturn);
        return;
    }

    // FIXED: 实现实际移动逻辑
    // 获取目标位置并移动 - 对应 legacy MoveToTarget
    auto target_opt = CharacterManager::Instance().GetCharacter(current_target_id_);
    if (target_opt.has_value()) {
        Position target_pos(target_opt->x, target_opt->y, target_opt->z);

        // 计算移动方向和速度
        float dx = target_pos.x - current_position_.x;
        float dy = target_pos.y - current_position_.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist > 0.1f) {
            // 归一化方向向量并应用移动速度
            float move_speed = 100.0f;  // 怪物移动速度 (单位/秒)
            float move_dist = move_speed * 0.016f;  // 假设16ms tick

            // 限制移动距离不超过到目标的距离
            if (move_dist > dist) {
                move_dist = dist;
            }

            float nx = dx / dist;
            float ny = dy / dist;

            Position new_pos;
            new_pos.x = current_position_.x + nx * move_dist;
            new_pos.y = current_position_.y + ny * move_dist;
            new_pos.z = current_position_.z;

            MoveTo(new_pos);
        }
    }

    spdlog::trace("Monster {} chasing target {}, distance: {}",
                 owner_id_, current_target_id_,
                 distance.has_value() ? *distance : 999.9f);
}

void AIStateMachine::ExecuteAttackState() {
    auto target = GetTopAggroTarget();
    if (!target.has_value()) {
        ChangeState(AIState::kReturn);
        return;
    }

    current_target_id_ = *target;

    // 检查是否还在攻击范围
    if (!IsTargetInAttackRange(current_target_id_)) {
        ChangeState(AIState::kChase);
        return;
    }

    // 检查攻击冷却
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_attack_time_
    ).count();

    if (elapsed < config_.attack_cooldown) {
        return;
    }

    // FIXED: 实现战斗逻辑 - 对应 legacy CMonster::Attack
    AttackTarget(current_target_id_);
    last_attack_time_ = now;
}

void AIStateMachine::ExecuteReturnState() {
    // 移动回出生点 - 对应 legacy 返回逻辑
    spdlog::trace("Monster {} returning to spawn point", owner_id_);

    // 计算与出生点的距离
    Position current_pos = current_position_;
    Position spawn_pos = spawn_position_;
    float dx = spawn_pos.x - current_pos.x;
    float dy = spawn_pos.y - current_pos.y;
    float distance_to_spawn = std::sqrt(dx * dx + dy * dy);

    if (distance_to_spawn < 1.0f) {
        // 已到达出生点
        is_returning_ = false;
        ClearAggro();

        if (config_.patrol_points.empty()) {
            ChangeState(AIState::kIdle);
        } else {
            ChangeState(AIState::kPatrol);
        }
    } else {
        // 继续移动回出生点 - 对应 legacy 返回移动
        MoveTo(spawn_position_);
    }
}

void AIStateMachine::ExecuteFleeState() {
    // FIXED: 实现逃跑逻辑 - 对应 legacy RunawayState
    spdlog::trace("Monster {} fleeing", owner_id_);

    // 获取怪物数据
    MonsterData* monster_data = GetMonsterData(owner_id_);
    if (!monster_data) {
        ChangeState(AIState::kIdle);
        return;
    }

    // 检查血量是否恢复到安全水平
    float hp_ratio = static_cast<float>(monster_data->hp) / monster_data->max_hp;
    if (hp_ratio > config_.flee_threshold + 0.2f) {
        // 血量恢复，停止逃跑
        ChangeState(AIState::kReturn);
        return;
    }

    // 朝远离目标的方向移动
    if (current_target_id_ != 0) {
        auto target_opt = CharacterManager::Instance().GetCharacter(current_target_id_);
        if (target_opt.has_value()) {
            Position target_pos(target_opt->x, target_opt->y, target_opt->z);

            // 计算远离目标的方向
            float dx = current_position_.x - target_pos.x;
            float dy = current_position_.y - target_pos.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist > 0.1f) {
                // 归一化方向向量并应用移动速度 (逃跑速度更快)
                float move_speed = 150.0f;  // 逃跑移动速度 (单位/秒)
                float move_dist = move_speed * 0.016f;  // 假设16ms tick

                float nx = dx / dist;
                float ny = dy / dist;

                Position new_pos;
                new_pos.x = current_position_.x + nx * move_dist;
                new_pos.y = current_position_.y + ny * move_dist;
                new_pos.z = current_position_.z;

                MoveTo(new_pos);
            }
        }
    }

    // 检查是否超过追击范围太多，停止逃跑
    if (current_target_id_ != 0) {
        auto distance = GetDistanceToTarget(current_target_id_);
        if (distance.has_value() && *distance > config_.chase_range * 1.5f) {
            // 超出范围，停止逃跑
            ChangeState(AIState::kReturn);
        }
    }
}

void AIStateMachine::ExecuteDeadState() {
    // 死亡状态,不做任何操作
    ClearAggro();
}

// ========== 状态转换检查 ==========

void AIStateMachine::CheckStateTransitions() {
    // FIXED: 血量低时逃跑
    MonsterData* monster_data = GetMonsterData(owner_id_);
    if (monster_data && monster_data->is_alive) {
        float hp_ratio = static_cast<float>(monster_data->hp) / monster_data->max_hp;
        if (hp_ratio < config_.flee_threshold) {
            if (current_state_ != AIState::kFlee && current_state_ != AIState::kDead) {
                ChangeState(AIState::kFlee);
            }
        }
    }
}

// ========== 辅助方法实现 ==========

void AIStateMachine::MoveTo(const Position& target_pos) {
    // 调用角色管理器移动接口 - 对应 legacy 移动逻辑
    // 更新怪物位置
    current_position_.x = target_pos.x;
    current_position_.y = target_pos.y;
    current_position_.z = target_pos.z;

    // FIXED: 通过CharacterManager更新位置到数据库
    // 注意：怪物和玩家使用相同的CharacterManager接口
    // 在实际实现中，可能需要专门的MonsterManager
    CharacterManager::Instance().UpdatePosition(owner_id_, target_pos.x, target_pos.y, target_pos.z, 0);

    // 同时更新monster_data
    MonsterData* monster_data = GetMonsterData(owner_id_);
    if (monster_data) {
        monster_data->position = target_pos;
    }

    spdlog::debug("Monster {} moved to ({}, {}, {})",
                  owner_id_, target_pos.x, target_pos.y, target_pos.z);
}

void AIStateMachine::AttackTarget(uint64_t target_id) {
    // 对应 legacy AI攻击逻辑 - CMonster::Attack
    // 检查攻击冷却
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_attack_time_).count();

    if (elapsed < config_.attack_cooldown) {
        // 攻击冷却中
        return;
    }

    // TODO [需MonsterManager]: 从Monster对象获取属性，而不是从AIConfig
    // AIConfig只包含AI行为参数，不包含怪物属性
    // 需要通过AIManager获取Monster对象，然后从Monster获取stats
    //
    // 集成MonsterManager后的实现：
    // Monster* monster = MonsterManager::Instance().GetMonster(owner_id_);
    // if (monster) {
    //     const MonsterStats& stats = monster->GetStats();
    //     BattleStats attacker_stats{};
    //     attacker_stats.level = stats.level;
    //     attacker_stats.physical_attack = stats.physical_attack;
    //     attacker_stats.physical_defense = stats.physical_defense;
    //     attacker_stats.attack_speed = stats.attack_speed;
    // }

    /*
    // 调用战斗系统攻击接口 - 对应 legacy 攻击逻辑
    // 1. 获取怪物属性
    BattleStats attacker_stats{};
    attacker_stats.level = config_.level;
    attacker_stats.physical_attack = config_.physical_attack;
    attacker_stats.physical_defense = config_.physical_defense;
    attacker_stats.attack_speed = config_.attack_speed;
    */

    // 2. 获取目标属性
    auto target_opt = CharacterManager::Instance().GetCharacter(target_id);
    if (!target_opt.has_value()) {
        spdlog::warn("Monster {} cannot attack target {}: target not found", owner_id_, target_id);
        return;
    }

    // FIXED: 从CharacterStats获取目标属性
    auto target_stats_opt = CharacterManager::Instance().GetCharacterStats(target_id);
    BattleStats defender_stats{};
    defender_stats.level = target_opt->level;
    if (target_stats_opt.has_value()) {
        defender_stats.physical_defense = target_stats_opt->physical_defense;
        defender_stats.magic_defense = target_stats_opt->magic_defense;
        defender_stats.dodge_rate = target_stats_opt->dodge_rate;
    }

    // TODO [需战斗系统]: 3. 计算伤害 - 需要attacker_stats
    // 集成战斗系统后的实现：
    // DamageResult damage_result = BattleManager::CalculatePhysicalDamage(
    //     attacker_stats, defender_stats);
    //

    // TODO [需CharacterManager]: 4. 应用伤害到目标
    // 注意：需要确认HP是在Character还是CharacterStats中
    // 需要调用CharacterManager的修改HP方法
    //
    // 集成CharacterManager后的实现：
    // bool is_dead = CharacterManager::Instance().ModifyHP(
    //     target_id,
    //     -static_cast<int32_t>(damage_result.damage)  // 负值表示扣血
    // );
    //
    // if (is_dead) {
    //     // 目标死亡，清除仇恨
    //     RemoveAggro(target_id);
    //     spdlog::info("Monster {} killed target {}", owner_id_, target_id);
    // }
    //
    // // 5. 更新仇恨
    // AddAggro(target_id, static_cast<uint32_t>(damage_result.damage));
    //
    // spdlog::info("Monster {} attacked target {} for {} damage (elapsed: {}ms)",
    //             owner_id_, target_id, damage_result.damage, elapsed);
    //
    last_attack_time_ = now;  // 更新攻击时间
}

void AIStateMachine::CallForHelp() {
    // FIXED: 通知同组其他怪物 - 对应 legacy DoFriendSearch
    spdlog::info("Monster {} calling for help", owner_id_);

    MonsterData* monster_data = GetMonsterData(owner_id_);
    if (!monster_data || monster_data->group_id == 0) {
        return;
    }

    // 通知AI管理器呼叫同组成员
    AIManager::Instance().NotifyGroupMembers(monster_data->group_id,
                                              current_target_id_,
                                              100);  // 初始仇恨值
}

// ========== AIManager ==========

// 静态成员初始化 (对应 legacy static DWORD dwRegenCheckTime = 0)
uint32_t AIManager::last_regen_check_time_ = 0;

AIManager& AIManager::Instance() {
    static AIManager instance;
    return instance;
}

void AIManager::Initialize() {
    spdlog::info("Initializing AIManager...");
    spdlog::info("AIManager initialized");
}

void AIManager::Process(uint32_t current_time)
{
    // 严格遵循 legacy CAISystem::Process() - 第51-71行
    // Legacy 代码:
    // while((pObj = m_AISubordinatedObject.GetData()))
    // {
    //     ConstantProcess(pObj);  // 持续处理
    //     PeriodicProcess(pObj);  // 周期性处理
    // }
    // m_pROUTER->MsgLoop();
    // if( gCurTime - dwRegenCheckTime >= 3000 ) {
    //     GROUPMGR->RegenProcess();
    //     dwRegenCheckTime = gCurTime;
    // }

    // 1. 遍历所有怪物 (对应 legacy while 循环)
    for (auto& [id, monster] : monsters_) {
        if (!monster || !monster->IsAlive()) {
            continue;
        }

        // 2. 持续处理 - 每个tick都调用
        ConstantProcess(monster.get());

        // 3. 周期性处理 - 每个tick都调用 (legacy中为空,但保留接口)
        PeriodicProcess(monster.get());
    }

    // 4. 消息路由器处理 (如果有)
    // 消息路由器将在集成跨服务器通信时实现
    // 参考MessageRouter.hpp/cpp,这里保留接口用于未来扩展

    // 5. 3秒刷新检查 - EXACTLY AS LEGACY (>= 3000ms)
    if (current_time - last_regen_check_time_ >= 3000) {
        ProcessRegen();
        last_regen_check_time_ = current_time;
    }
}

void AIManager::ConstantProcess(Monster* monster)
{
    // 严格遵循 legacy CAISystem::ConstantProcess() - 第72-81行
    // Legacy 代码:
    // if(obj->GetObjectKind() == eObjectKind_BossMonster)
    //     return;
    // GSTATEMACHINE.Process(obj, eSEVENT_Process, NULL);

    if (!monster) {
        return;
    }

    // Boss怪物跳过普通AI处理 - EXACTLY AS LEGACY
    if (monster->IsBoss()) {
        return;
    }

    // 使用状态机处理 - EXACTLY AS LEGACY
    // 对应 legacy GSTATEMACHINE.Process(obj, eSEVENT_Process, NULL)
    AIStateMachine* ai = GetAI(monster->GetID());
    if (ai) {
        ai->Process(AIEventType::kProcess, nullptr);
    }
}

void AIManager::PeriodicProcess(Monster* monster)
{
    // 严格遵循 legacy CAISystem::PeriodicProcess() - 第83-86行
    // Legacy 代码: 空实现

    // 空实现 - EXACTLY AS LEGACY
    // 不添加任何逻辑
    (void)monster;  // 避免未使用参数警告
}

void AIManager::ProcessRegen()
{
    // 严格遵循 legacy GROUPMGR->RegenProcess()
    // FIXED: 实现刷新处理逻辑
    spdlog::trace("Processing regeneration check");

    // 检查所有AI组,执行刷新逻辑
    // 1. 检查刷新条件
    // 2. 处理延迟刷新
    // 3. 重新生成死亡怪物

    auto now = std::chrono::system_clock::now();

    // 遍历所有怪物，检查是否需要刷新
    for (auto& [monster_id, monster_data] : g_monster_data) {
        if (!monster_data.is_alive) {
            // 检查是否应该刷新
            // 简化处理：死亡后30秒自动刷新
            // 实际应该根据配置的刷新延迟时间
            // 完整刷新逻辑包括：
            // - 读取刷新延迟配置 (从MonsterTemplate)
            // - 检查刷新条件 (时间、玩家在线等)
            // - 处理条件刷新 (ADDCONDITION)
            // - 处理Boss刷新 (更长延迟、特殊条件)
            auto time_since_death = std::chrono::duration_cast<std::chrono::seconds>(
                now - monster_data.death_time).count();

            if (time_since_death >= monster_data.respawn_delay) {
                spdlog::info("Monster {} respawning after {} seconds",
                            monster_id, time_since_death);
                // TODO [需MonsterManager]: 集成时调用MonsterManager::RespawnMonster
                // 集成MonsterManager后的实现：
                // MonsterManager::Instance().RespawnMonster(monster_id);
                //
                // 临时处理：直接标记为存活
                monster_data.is_alive = true;
            }
        }
    }
}

void AIManager::AddObject(std::shared_ptr<class Monster> monster)
{
    // 严格遵循 legacy CAISystem::AddObject() - 第88-105行
    // Legacy 代码:
    // CMonster * pMob = (CMonster * )obj;
    // m_AISubordinatedObject.Add(obj, pMob->GetID());

    if (!monster) {
        return;
    }

    uint64_t monster_id = monster->GetID();
    monsters_[monster_id] = monster;

    spdlog::debug("Monster {} added to AI system", monster_id);
}

std::shared_ptr<class Monster> AIManager::RemoveObject(uint64_t object_id)
{
    // 严格遵循 legacy CAISystem::RemoveObject() - 第106-126行
    // Legacy 代码:
    // CObject *outObj = m_AISubordinatedObject.GetData(dwID);
    // if(!outObj) return NULL;
    // m_AISubordinatedObject.Remove(dwID);
    // CAIGroup * pGroup = GROUPMGR->GetGroup(...);
    // if(pGroup) {
    //     pGroup->Die(outObj->GetID());
    //     pGroup->RegenCheck();
    // }

    auto it = monsters_.find(object_id);
    if (it == monsters_.end()) {
        return nullptr;
    }

    std::shared_ptr<Monster> outObj = it->second;
    monsters_.erase(it);

    // FIXED: 通知AI组 - 实现AI组的 Die 和 RegenCheck
    MonsterData* monster_data = GetMonsterData(object_id);
    if (monster_data && monster_data->group_id > 0) {
        // 标记怪物死亡 - 对应 legacy pGroup->Die()
        monster_data->is_alive = false;

        // 检查是否需要刷新 - 对应 legacy pGroup->RegenCheck()
        // 简化实现：记录死亡时间，等待ProcessRegen处理
        spdlog::debug("Monster {} from group {} died, checking regeneration...",
                     object_id, monster_data->group_id);
    }

    spdlog::debug("Monster {} removed from AI system", object_id);

    return outObj;
}

bool AIManager::LoadAIGroups(uint16_t map_num)
{
    // 严格遵循 legacy CAISystem::LoadAIGroupList() - 第156-514行
    // Legacy 代码:
    // sprintf(filename,"Resource/Server/Monster_%02d.bin",GAMERESRCMNGR->GetLoadMapNum());
    // if(!file.Init(filename,"rb")) return;

    // 文件路径: Resource/Server/Monster_XX.bin - EXACTLY AS LEGACY
    std::ostringstream oss;
    oss << "Resource/Server/Monster_" << std::setfill('0') << std::setw(2) << map_num << ".bin";
    std::string filename = oss.str();

    spdlog::info("Loading AI groups from: {}", filename);

    // 解析二进制配置文件 - Monster_XX.bin 格式
    // 配置命令:
    // $GROUP           <group_id>
    // #MAXOBJECT       <max_count>
    // #PROPERTY        <property>
    // #GROUPNAME       <name>
    // #ADDCONDITION    <target_group> <ratio> <delay> <regen_bool>
    // #ADD             <kind> <skip> <monster_kind> <x> <z> <regen_bool>
    // $UNIQUE          <group_id>
    // #UNIQUEADD       <kind> <skip> <monster_kind> <x> <z> <regen_bool>
    // #UNIQUEREGENDELAY <delay_minutes>
    // #RANDOMREGENDELAY <min_minutes> <max_minutes>

    // 为每个频道创建独立的AI组 - EXACTLY AS LEGACY
    // 在实际集成时,这里将:
    // 1. 读取并解析 Monster_XX.bin 二进制文件
    // 2. 为每个频道创建独立的 AIGroup 实例
    // 3. 设置怪物生成点和刷新规则
    // 4. 注册到 GroupManager

    spdlog::warn("AI group loading from binary file not yet integrated");
    spdlog::warn("Use CreateTestMonster() for testing until binary parser is implemented");

    return true;
}

void AIManager::AddAI(uint64_t monster_id, const AIConfig& config) {
    ais_[monster_id] = std::make_unique<AIStateMachine>(monster_id, config);

    spdlog::info("AI added for monster {}", monster_id);
}

void AIManager::RemoveAI(uint64_t monster_id) {
    ais_.erase(monster_id);
    spdlog::info("AI removed for monster {}", monster_id);
}

AIStateMachine* AIManager::GetAI(uint64_t monster_id) {
    auto it = ais_.find(monster_id);
    if (it != ais_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<uint64_t> AIManager::GetMonstersInRange(
    const Position& center,
    float range
) {
    std::vector<uint64_t> monsters;

    // FIXED: 从空间索引中查询 (简化版：遍历所有怪物)
    // 实际应该使用空间分区 (Quadtree 或 Grid)
    // 对应 legacy: 空间查询相关代码
    for (const auto& [monster_id, monster_data] : g_monster_data) {
        if (!monster_data.is_alive) {
            continue;
        }

        float distance = center.Distance2D(monster_data.position);
        if (distance <= range) {
            monsters.push_back(monster_id);
        }
    }

    return monsters;
}

std::vector<uint64_t> AIManager::GetPlayersInRange(
    const Position& center,
    float range
) {
    std::vector<uint64_t> players;

    // 从角色管理器查询 (简化实现)
    // 对应 legacy: 查询范围内玩家
    // 注意：这里需要实现一个查询接口,暂时返回空列表
    // 完整实现将使用 SpatialIndex 系统进行高效范围查询

    // TODO [需SpatialIndex]: 集成空间索引系统以获取范围内玩家
    // 集成SpatialIndex后的实现：
    // auto entities = Murim::Core::SpatialIndex::Instance().QueryInRange(center, range);
    // for (auto& entity : entities) {
    //     if (entity.type == EntityType::kPlayer) {
    //         players.push_back(entity.entity_id);
    //     }
    // }
    //
    // 临时优化：也可以从CharacterManager遍历所有玩家进行距离判断
    // (性能较低，仅用于测试)

    return players;
}

void AIManager::CreateAIGroup(uint32_t group_id) {
    if (ai_groups_.find(group_id) == ai_groups_.end()) {
        ai_groups_[group_id] = std::vector<uint64_t>();
        spdlog::info("AI group {} created", group_id);
    }
}

void AIManager::AddMonsterToGroup(uint32_t group_id, uint64_t monster_id) {
    auto it = ai_groups_.find(group_id);
    if (it != ai_groups_.end()) {
        it->second.push_back(monster_id);
        spdlog::debug("Monster {} added to group {}", monster_id, group_id);
    }
}

void AIManager::NotifyGroupMembers(uint32_t group_id, uint64_t attacker_id, uint32_t aggro_value) {
    auto it = ai_groups_.find(group_id);
    if (it == ai_groups_.end()) {
        return;
    }

    for (uint64_t monster_id : it->second) {
        AIStateMachine* ai = GetAI(monster_id);
        if (ai) {
            ai->AddAggro(attacker_id, aggro_value);
        }
    }

    spdlog::info("Group {} notified about attacker {}, aggro={}",
                 group_id, attacker_id, aggro_value);
}

bool AIManager::SpawnMonster(
    uint32_t monster_template_id,
    const Position& position,
    uint32_t group_id
) {
    // FIXED: 创建怪物实例 - 对应 legacy CreateMonster
    uint64_t monster_id = GenerateMonsterID();

    // 创建怪物对象
    auto monster = std::make_shared<Monster>(monster_id, monster_template_id, position, group_id);

    // 添加到怪物列表
    AddObject(monster);

    // 创建 AI配置 - 从模板加载
    AIConfig config;
    // 从模板加载配置 (简化版)
    // 实际应该从数据库或配置文件读取 MonsterTemplate
    config.behavior_type = AIBehaviorType::kAggressive;

    // TODO [需MonsterManager]: 这些属性应该从Monster对象获取，而不是从AIConfig
    // AIConfig只包含AI行为参数，不包含怪物属性
    // 集成MonsterManager后，将从Monster对象获取以下属性：
    // const Monster* monster = MonsterManager::Instance().GetMonster(monster_id);
    // const MonsterStats& stats = monster->GetStats();
    //
    // 当前注释掉，因为这些属性不应该在AIConfig中：
    // config.level = 10;
    // config.physical_attack = 50;
    // config.physical_defense = 30;
    // config.attack_speed = 2000;

    config.detection_range = 15.0f;
    config.chase_range = 25.0f;
    config.attack_range = 3.0f;
    config.attack_cooldown = 2000;

    // TODO [需数据库/MonsterTemplate]: 从模板数据库加载完整配置
    // 在集成时,这里将调用:
    // auto template_opt = Database::MonsterTemplateDAO::Load(monster_template_id);
    // if (template_opt.has_value()) {
    //     config = AIConfigFromTemplate(template_opt.value());
    // }
    //
    // MonsterTemplate包含：
    // - AI行为参数（侦测范围、追击范围、攻击范围等）
    // - 巡逻点配置
    // - 逃跑阈值
    // - 仇恨衰减率
    // - 复活延迟
    // - 怪物组ID

    // 创建 AI
    AddAI(monster_id, config);

    // 初始化 AI位置
    AIStateMachine* ai = GetAI(monster_id);
    if (ai) {
        ai->SetCurrentPosition(position);
        ai->SetSpawnPosition(position);
    }

    if (group_id > 0) {
        AddMonsterToGroup(group_id, monster_id);
    }

    spdlog::info("Monster spawned: id={}, template_id={}, position=({},{},{}), group={}",
                 monster_id, monster_template_id, position.x, position.y, position.z, group_id);

    return true;
}

void AIManager::OnMonsterDeath(uint64_t monster_id) {
    RemoveAI(monster_id);
    spdlog::info("Monster {} died and AI removed", monster_id);
}

} // namespace Game
} // namespace Murim
