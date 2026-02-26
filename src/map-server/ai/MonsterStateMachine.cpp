#include "MonsterStateMachine.hpp"
#include "MonsterAI.hpp"  // MonsterAI完整定义
#include "../entities/Monster.hpp"
#include "shared/character/Player.hpp"
#include "core/spdlog_wrapper.hpp"
#include <random>

namespace Murim {
namespace Game {

MonsterStateMachine::MonsterStateMachine(MapServer::Monster* monster)
    : monster_(monster)
    , chase_target_(nullptr)
    , current_state_(AIState::kSpawn)
    , state_enter_time_(std::chrono::system_clock::now())
    , state_duration_(0)
    , patrol_point_()
    , has_patrol_point_(false)
    , last_search_time_(0)
    , last_attack_time_(0)
{
    spdlog::debug("[StateMachine] Created for monster");
}

MonsterStateMachine::~MonsterStateMachine() {
    spdlog::debug("[StateMachine] Destroyed");
}

void MonsterStateMachine::Update(uint32_t delta_time) {
    if (!monster_) {
        return;
    }

    // 更新状态持续时间
    auto now = std::chrono::system_clock::now();
    state_duration_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - state_enter_time_
    ).count();

    // 根据当前状态执行相应逻辑
    switch (current_state_) {
        case AIState::kIdle:
            HandleIdle(delta_time);
            break;

        case AIState::kPatrol:
            HandlePatrol(delta_time);
            break;

        case AIState::kChase:
            HandleChase(delta_time);
            break;

        case AIState::kAttack:
            HandleAttack(delta_time);
            break;

        case AIState::kReturn:
            HandleReturn(delta_time);
            break;

        case AIState::kDead:
            // 死亡状态不处理
            break;

        case AIState::kSpawn:
            // 生成后直接切换到Idle
            ChangeState(AIState::kIdle);
            break;

        default:
            spdlog::warn("[StateMachine] Unknown state: {}", static_cast<int>(current_state_));
            break;
    }
}

void MonsterStateMachine::ChangeState(AIState new_state) {
    if (current_state_ == new_state) {
        return;  // 状态未改变
    }

    AIState old_state = current_state_;
    current_state_ = new_state;
    state_enter_time_ = std::chrono::system_clock::now();
    state_duration_ = 0;

    spdlog::debug("[StateMachine] State transition: {} -> {}",
                  static_cast<int>(old_state), static_cast<int>(new_state));

    // 状态进入时的初始化
    switch (new_state) {
        case AIState::kPatrol:
            SelectRandomPatrolPoint();
            break;

        case AIState::kReturn:
            // 清除追击目标
            ClearTarget();
            break;

        default:
            break;
    }
}

uint64_t MonsterStateMachine::GetStateDuration() const {
    return state_duration_;
}

bool MonsterStateMachine::CanSearchForTarget() const {
    return (state_duration_ - last_search_time_) >= SEARCH_INTERVAL_MS;
}

bool MonsterStateMachine::ShouldReturnToSpawn() const {
    // 如果没有目标或目标无效，且不在出生点附近
    if (!IsTargetValid()) {
        return !IsAtSpawnPoint();
    }
    return false;
}

// ========== 状态处理方法 ==========

void MonsterStateMachine::HandleIdle(uint32_t delta_time) {
    // 定时索敌
    if (CanSearchForTarget()) {
        if (TryFindTarget()) {
            // 找到目标，切换到追击
            ChangeState(AIState::kChase);
            return;
        }
        last_search_time_ = state_duration_;
    }

    // Idle 5秒后可能开始巡逻
    if (state_duration_ >= 5000) {
        ChangeState(AIState::kPatrol);
    }
}

void MonsterStateMachine::HandlePatrol(uint32_t delta_time) {
    // 定时索敌
    if (CanSearchForTarget()) {
        if (TryFindTarget()) {
            // 找到目标，切换到追击
            ChangeState(AIState::kChase);
            return;
        }
        last_search_time_ = state_duration_;
    }

    // 移动到巡逻点
    if (has_patrol_point_) {
        monster_->MoveTo(patrol_point_, delta_time);

        // 检查是否到达巡逻点
        float distance = monster_->GetDistanceTo(patrol_point_);
        if (distance < 50.0f) {
            // 到达巡逻点，返回Idle
            ChangeState(AIState::kIdle);
        }
    } else {
        // 没有巡逻点，返回Idle
        ChangeState(AIState::kIdle);
    }
}

void MonsterStateMachine::HandleChase(uint32_t delta_time) {
    // 检查目标是否有效
    if (!IsTargetValid()) {
        // 目标丢失，返回出生点
        ChangeState(AIState::kReturn);
        return;
    }

    // 检查是否在攻击范围
    if (IsTargetInAttackRange()) {
        // 进入攻击范围，切换到攻击状态
        ChangeState(AIState::kAttack);
        return;
    }

    // 检查是否超出活动范围
    if (IsOutsideDomain()) {
        // 超出活动范围，返回出生点
        ChangeState(AIState::kReturn);
        return;
    }

    // 追击目标
    monster_->MoveToEntity(chase_target_, delta_time);
}

void MonsterStateMachine::HandleAttack(uint32_t delta_time) {
    // 检查目标是否有效
    if (!IsTargetValid()) {
        // 目标丢失，返回出生点
        ChangeState(AIState::kReturn);
        return;
    }

    // 检查目标是否离开攻击范围
    if (!IsTargetInAttackRange()) {
        // 目标离开，切换回追击
        ChangeState(AIState::kChase);
        return;
    }

    // 检查攻击冷却
    if ((state_duration_ - last_attack_time_) >= ATTACK_INTERVAL_MS) {
        // 执行攻击
        monster_->Attack(chase_target_);
        last_attack_time_ = state_duration_;

        // 检查目标是否死亡
        if (chase_target_ && chase_target_->GetHP() == 0) {
            // 目标死亡，清除目标并返回Idle
            ClearTarget();
            ChangeState(AIState::kIdle);
        }
    }
}

void MonsterStateMachine::HandleReturn(uint32_t delta_time) {
    // 检查是否到达出生点
    if (IsAtSpawnPoint()) {
        // 到达出生点，切换到Idle
        ChangeState(AIState::kIdle);
        return;
    }

    // 返回出生点
    monster_->ReturnToSpawnPoint();
}

// ========== 辅助方法 ==========

bool MonsterStateMachine::TryFindTarget() {
    if (!monster_) {
        return false;
    }

    // 使用MonsterAI索敌
    auto& ai = monster_->GetAI();
    auto target = ai.FindTarget();

    if (target && target->IsValid()) {
        chase_target_ = target;
        spdlog::debug("[StateMachine] Found target: {}", target->GetName());
        return true;
    }

    return false;
}

void MonsterStateMachine::SelectRandomPatrolPoint() {
    if (!monster_) {
        return;
    }

    // 获取怪物当前位置
    const auto& current_pos = monster_->GetPosition();

    // 在活动范围内随机选择一个点
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-100.0f, 100.0f);

    patrol_point_.x = current_pos.x + dis(gen);
    patrol_point_.y = current_pos.y;
    patrol_point_.z = current_pos.z + dis(gen);
    has_patrol_point_ = true;

    spdlog::debug("[StateMachine] Selected patrol point: ({}, {}, {})",
                  patrol_point_.x, patrol_point_.y, patrol_point_.z);
}

bool MonsterStateMachine::IsTargetValid() const {
    if (!chase_target_) {
        return false;
    }

    // 检查目标是否存活
    if (!chase_target_->IsAlive()) {
        return false;
    }

    return true;
}

bool MonsterStateMachine::IsTargetInAttackRange() const {
    if (!chase_target_ || !monster_) {
        return false;
    }

    auto& ai = monster_->GetAI();
    return ai.IsInAttackRange(chase_target_);
}

bool MonsterStateMachine::IsOutsideDomain() const {
    if (!monster_) {
        return false;
    }

    auto& ai = monster_->GetAI();
    const auto& current_pos = monster_->GetPosition();
    return ai.IsOutsideDomain(current_pos);
}

bool MonsterStateMachine::IsAtSpawnPoint() const {
    if (!monster_) {
        return true;
    }

    float distance = monster_->GetDistanceToSpawnPoint();
    return distance < 50.0f;  // 50像素内算作到达
}

void MonsterStateMachine::ClearTarget() {
    if (chase_target_) {
        spdlog::debug("[StateMachine] Clearing target: {}",
                      chase_target_->GetName());
        chase_target_.reset();
    }

    // 清除AI中的目标
    if (monster_) {
        auto& ai = monster_->GetAI();
        ai.ClearTarget();
    }
}

} // namespace Game
} // namespace Murim
