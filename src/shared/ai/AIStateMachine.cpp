// MurimServer - AI State Machine Implementation
// AI状态机系统实现

#include "AIStateMachine.hpp"
#include "../game/Monster.hpp"
#include "../game/GameObject.hpp"
#include "../utils/TimeUtils.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <cmath>
#include <cstdlib>
#include <ctime>

namespace Murim {
namespace Game {

// ========== Helper Functions ==========

uint32_t GetCurrentTimeMs() {
    // 使用TimeUtils获取当前时间
    return static_cast<uint32_t>(TimeUtils::GetCurrentTimeMs());
}

float CalculateDistance(float x1, float y1, float z1, float x2, float y2, float z2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float RandomRange(float min, float max) {
    float scale = max - min;
    return min + (static_cast<float>(std::rand()) / RAND_MAX) * scale;
}

int RandomRange(int min, int max) {
    return min + (std::rand() % (max - min + 1));
}

const char* GetStateName(AIState state) {
    switch (state) {
        case AIState::None: return "None";
        case AIState::Stand: return "Stand";
        case AIState::Rest: return "Rest";
        case AIState::WalkAround: return "WalkAround";
        case AIState::Pursuit: return "Pursuit";
        case AIState::Attack: return "Attack";
        case AIState::Search: return "Search";
        case AIState::Runaway: return "Runaway";
        case AIState::Die: return "Die";
        case AIState::Return: return "Return";
        default: return "Unknown";
    }
}

// ========== AIStateBase ==========

AIStateBase::AIStateBase(AIState state, Monster* monster)
    : state_(state), monster_(monster),
      ai_params_(monster->GetAIParameters()),
      state_info_(monster->GetAIStateInfo()) {
}

AIState AIStateBase::Update(float delta_time) {
    // 默认实现：什么都不做，保持当前状态
    return state_;
}

// ========== Stand State ==========

AIState_Stand::AIState_Stand(Monster* monster)
    : AIStateBase(AIState::Stand, monster) {
}

void AIState_Stand::Enter() {
    uint32_t current_time = GetCurrentTimeMs();

    // 停止移动
    monster_->Stop();

    // 设置状态持续时间（随机休息时间）
    uint32_t duration = RandomRange(
        ai_params_.rest_time_min,
        ai_params_.rest_time_max
    );

    state_info_.state_start_time = current_time;
    state_info_.state_end_time = current_time + duration;

    spdlog::debug("[AI-{}] Stand: Started, duration={}ms",
                 monster_->GetObjectId(), duration);
}

AIState AIState_Stand::Update(float delta_time) {
    uint32_t current_time = GetCurrentTimeMs();

    // 检查状态是否结束
    if (state_info_.IsStateExpired(current_time)) {
        // 站立结束后，决定下一个状态
        if (HasTarget()) {
            return AIState::Pursuit;
        } else {
            // 30%概率搜索，70%概率巡逻
            if (RandomRange(0, 100) < 30) {
                return AIState::Search;
            } else {
                return AIState::WalkAround;
            }
        }
    }

    // 检查是否有新目标进入范围
    if (!HasTarget()) {
        GameObject* target = monster_->SearchForTarget(
            ai_params_.search_range
        );
        if (target) {
            monster_->SetTarget(target);
            return AIState::Pursuit;
        }
    }

    return AIState::Stand;
}

// ========== Rest State ==========

AIState_Rest::AIState_Rest(Monster* monster)
    : AIStateBase(AIState::Rest, monster) {
}

void AIState_Rest::Enter() {
    uint32_t current_time = GetCurrentTimeMs();

    // 停止移动
    monster_->Stop();

    // 设置休息持续时间
    uint32_t duration = RandomRange(
        ai_params_.rest_time_min,
        ai_params_.rest_time_max
    );

    state_info_.state_start_time = current_time;
    state_info_.state_end_time = current_time + duration;

    spdlog::debug("[AI-{}] Rest: Started, duration={}ms",
                 monster_->GetObjectId(), duration);
}

AIState AIState_Rest::Update(float delta_time) {
    uint32_t current_time = GetCurrentTimeMs();

    // 休息期间缓慢回血
    monster_->RestoreHealth(delta_time * 0.01f);  // 每秒回1%最大生命

    // 检查休息是否结束
    if (state_info_.IsStateExpired(current_time)) {
        if (HasTarget()) {
            return AIState::Pursuit;
        } else {
            return AIState::Stand;
        }
    }

    // 如果受到攻击，立即醒来
    if (monster_->WasRecentlyAttacked()) {
        return AIState::Stand;
    }

    return AIState::Rest;
}

void AIState_Rest::Exit() {
    spdlog::debug("[AI-{}] Rest: Exited", monster_->GetObjectId());
}

// ========== Walk Around State ==========

AIState_WalkAround::AIState_WalkAround(Monster* monster)
    : AIStateBase(AIState::WalkAround, monster),
      has_target_(false) {
}

void AIState_WalkAround::Enter() {
    uint32_t current_time = GetCurrentTimeMs();

    // 生成随机游荡目标
    GenerateRandomTarget();

    // 设置游荡持续时间
    uint32_t duration = RandomRange(
        ai_params_.walk_around_time_min,
        ai_params_.walk_around_time_max
    );

    state_info_.state_start_time = current_time;
    state_info_.state_end_time = current_time + duration;

    spdlog::debug("[AI-{}] WalkAround: Started, duration={}ms",
                 monster_->GetObjectId(), duration);
}

AIState AIState_WalkAround::Update(float delta_time) {
    // 检查是否到达目标点
    float current_x, current_y, current_z;
    monster_->GetPosition(&current_x, &current_y, &current_z);

    float distance = CalculateDistance(
        current_x, current_y, current_z,
        target_x_, target_y_, target_z_
    );

    if (distance < 10.0f) {
        // 到达目标点，生成新目标
        GenerateRandomTarget();
    }

    // 检查状态是否结束
    uint32_t current_time = GetCurrentTimeMs();
    if (state_info_.IsStateExpired(current_time)) {
        return AIState::Stand;
    }

    // 检查是否有目标
    if (HasTarget()) {
        return AIState::Pursuit;
    }

    // 检查是否离原点太远
    if (GetDistanceToOrigin() > ai_params_.return_range) {
        return AIState::Return;
    }

    return AIState::WalkAround;
}

void AIState_WalkAround::GenerateRandomTarget() {
    // 获取当前位置
    float current_x, current_y, current_z;
    monster_->GetPosition(&current_x, &current_y, &current_z);

    // 在游荡范围内生成随机点
    float angle = RandomRange(0.0f, 6.28318f);  // 0-2π
    float distance = RandomRange(50.0f, ai_params_.walk_around_range);

    target_x_ = current_x + std::cos(angle) * distance;
    target_y_ = current_y;
    target_z_ = current_z + std::sin(angle) * distance;

    // 移动到目标点
    monster_->MoveTo(target_x_, target_y_, target_z_);

    spdlog::debug("[AI-{}] WalkAround: New target ({:.1f}, {:.1f}, {:.1f})",
                 monster_->GetObjectId(), target_x_, target_y_, target_z_);
}

// ========== Search State ==========

AIState_Search::AIState_Search(Monster* monster)
    : AIStateBase(AIState::Search, monster),
      last_search_time_(0),
      found_target_(nullptr) {
}

void AIState_Search::Enter() {
    uint32_t current_time = GetCurrentTimeMs();

    // 停止移动
    monster_->Stop();

    state_info_.state_start_time = current_time;
    state_info_.state_end_time = current_time + 3000;  // 最多搜索3秒

    last_search_time_ = 0;
    found_target_ = nullptr;

    spdlog::debug("[AI-{}] Search: Started", monster_->GetObjectId());
}

AIState AIState_Search::Update(float delta_time) {
    uint32_t current_time = GetCurrentTimeMs();

    // 定期搜索
    if (current_time - last_search_time_ >= ai_params_.search_interval) {
        last_search_time_ = current_time;

        found_target_ = SearchForTarget();
        if (found_target_) {
            monster_->SetTarget(found_target_);
            spdlog::debug("[AI-{}] Search: Found target {}",
                         monster_->GetObjectId(), found_target_->GetObjectId());
            return AIState::Pursuit;
        }
    }

    // 检查搜索超时
    if (state_info_.IsStateExpired(current_time)) {
        // 没找到目标，改为巡逻
        return AIState::WalkAround;
    }

    return AIState::Search;
}

GameObject* AIState_Search::SearchForTarget() {
    return monster_->SearchForTarget(ai_params_.search_range);
}

// ========== Pursuit State ==========

AIState_Pursuit::AIState_Pursuit(Monster* monster)
    : AIStateBase(AIState::Pursuit, monster) {
}

void AIState_Pursuit::Enter() {
    uint32_t current_time = GetCurrentTimeMs();

    state_info_.state_start_time = current_time;
    state_info_.state_end_time = current_time + ai_params_.pursuit_forgive_time;

    spdlog::debug("[AI-{}] Pursuit: Started, target={}",
                 monster_->GetObjectId(), state_info_.target_id);
}

AIState AIState_Pursuit::Update(float delta_time) {
    // 检查目标是否有效
    if (!HasTarget() || !IsTargetAlive()) {
        ClearTarget();
        return AIState::Stand;
    }

    // 移动向目标
    GameObject* target = GetTarget();
    float target_x, target_y, target_z;
    target->GetPosition(&target_x, &target_y, &target_z);

    monster_->MoveTo(target_x, target_y, target_z);

    // 检查距离
    float distance = GetDistanceToTarget();

    if (distance <= ai_params_.attack_range) {
        // 进入攻击范围
        return AIState::Attack;
    }

    // 检查追击超时
    uint32_t current_time = GetCurrentTimeMs();
    if (state_info_.IsStateExpired(current_time)) {
        spdlog::debug("[AI-{}] Pursuit: Forgived target {}",
                     monster_->GetObjectId(), state_info_.target_id);
        ClearTarget();
        return AIState::Return;
    }

    // 检查是否离原点太远
    if (GetDistanceToOrigin() > ai_params_.return_range * 1.5f) {
        ClearTarget();
        return AIState::Return;
    }

    return AIState::Pursuit;
}

void AIState_Pursuit::Exit() {
    spdlog::debug("[AI-{}] Pursuit: Exited", monster_->GetObjectId());
}

// ========== Attack State ==========

AIState_Attack::AIState_Attack(Monster* monster)
    : AIStateBase(AIState::Attack, monster),
      last_attack_time_(0),
      attack_count_(0) {
}

void AIState_Attack::Enter() {
    uint32_t current_time = GetCurrentTimeMs();

    // 停止移动
    monster_->Stop();

    // 转向目标
    if (HasTarget()) {
        GameObject* target = GetTarget();
        float target_x, target_y, target_z;
        target->GetPosition(&target_x, &target_y, &target_z);
        monster_->FaceTowards(target_x, target_y, target_z);
    }

    state_info_.state_start_time = current_time;
    state_info_.state_end_time = current_time + 5000;  // 最多攻击5秒

    last_attack_time_ = 0;
    attack_count_ = 0;

    spdlog::debug("[AI-{}] Attack: Started", monster_->GetObjectId());
}

AIState AIState_Attack::Update(float delta_time) {
    // 检查目标是否有效
    if (!HasTarget() || !IsTargetAlive()) {
        ClearTarget();
        return AIState::Stand;
    }

    // 检查目标是否还在攻击范围内
    if (!IsTargetInRange(ai_params_.attack_range)) {
        return AIState::Pursuit;
    }

    uint32_t current_time = GetCurrentTimeMs();

    // 检查攻击冷却
    if (current_time - last_attack_time_ >= ai_params_.attack_delay) {
        if (PerformAttack()) {
            last_attack_time_ = current_time;
            attack_count_++;
        }
    }

    // 检查攻击超时
    if (state_info_.IsStateExpired(current_time)) {
        return AIState::Stand;
    }

    return AIState::Attack;
}

void AIState_Attack::Exit() {
    spdlog::debug("[AI-{}] Attack: Exited, total attacks={}",
                 monster_->GetObjectId(), attack_count_);
}

bool AIState_Attack::PerformAttack() {
    GameObject* target = GetTarget();
    if (!target) {
        return false;
    }

    // 执行攻击
    bool success = monster_->Attack(target);

    if (success) {
        spdlog::debug("[AI-{}] Attack: Performed attack on target {}",
                     monster_->GetObjectId(), target->GetObjectId());
    }

    return success;
}

void AIState_Attack::SelectNextAttack() {
    // TODO [需技能系统]: 选择下一个攻击技能
    // 需要集成技能系统后实现：
    // 1. 从怪物配置获取可用技能列表
    // 2. 根据冷却时间、MP消耗、距离等条件选择技能
    // 3. 返回选中的技能ID
    //
    // 示例实现框架：
    // auto available_skills = monster_->GetAvailableSkills();
    // for (auto& skill : available_skills) {
    //     if (!skill->IsOnCooldown() && monster_->HasEnoughMP(skill->GetMPCost())) {
    //         return skill;
    //     }
    // }
}

// ========== Runaway State ==========

AIState_Runaway::AIState_Runaway(Monster* monster)
    : AIStateBase(AIState::Runaway, monster),
      runaway_dir_x_(0), runaway_dir_z_(0) {
}

void AIState_Runaway::Enter() {
    uint32_t current_time = GetCurrentTimeMs();

    // 计算逃跑方向
    CalculateRunawayDirection();

    state_info_.state_start_time = current_time;
    state_info_.state_end_time = current_time + 3000;  // 逃跑3秒

    spdlog::debug("[AI-{}] Runaway: Started, HP < {:.0f}%",
                 monster_->GetObjectId(), ai_params_.runaway_hp_percent * 100);
}

AIState AIState_Runaway::Update(float delta_time) {
    // 继续逃跑
    float current_x, current_y, current_z;
    monster_->GetPosition(&current_x, &current_y, &current_z);

    float new_x = current_x + runaway_dir_x_ * ai_params_.return_speed;
    float new_z = current_z + runaway_dir_z_ * ai_params_.return_speed;

    monster_->MoveTo(new_x, current_y, new_z);

    // 检查逃跑是否结束
    uint32_t current_time = GetCurrentTimeMs();
    if (state_info_.IsStateExpired(current_time)) {
        // 如果血量恢复，可以继续战斗
        if (monster_->GetHealthPercent() > ai_params_.runaway_hp_percent * 2) {
            return AIState::Stand;
        } else {
            return AIState::Return;
        }
    }

    return AIState::Runaway;
}

void AIState_Runaway::CalculateRunawayDirection() {
    if (!HasTarget()) {
        runaway_dir_x_ = 1.0f;
        runaway_dir_z_ = 0.0f;
        return;
    }

    GameObject* target = GetTarget();
    float target_x, target_y, target_z;
    target->GetPosition(&target_x, &target_y, &target_z);

    float current_x, current_y, current_z;
    monster_->GetPosition(&current_x, &current_y, &current_z);

    // 计算远离目标的方向
    float dx = current_x - target_x;
    float dz = current_z - target_z;
    float length = std::sqrt(dx * dx + dz * dz);

    if (length > 0) {
        runaway_dir_x_ = dx / length;
        runaway_dir_z_ = dz / length;
    } else {
        runaway_dir_x_ = 1.0f;
        runaway_dir_z_ = 0.0f;
    }
}

// ========== Return State ==========

AIState_Return::AIState_Return(Monster* monster)
    : AIStateBase(AIState::Return, monster) {
}

void AIState_Return::Enter() {
    uint32_t current_time = GetCurrentTimeMs();

    spdlog::debug("[AI-{}] Return: Started, returning to origin ({:.1f}, {:.1f}, {:.1f})",
                 monster_->GetObjectId(),
                 state_info_.origin_x, state_info_.origin_y, state_info_.origin_z);

    // 移动回原点
    monster_->MoveTo(
        state_info_.origin_x,
        state_info_.origin_y,
        state_info_.origin_z
    );

    state_info_.state_start_time = current_time;
    state_info_.state_end_time = current_time + 10000;  // 最多10秒
}

AIState AIState_Return::Update(float delta_time) {
    // 检查是否到达原点
    float distance = GetDistanceToOrigin();

    if (distance < 20.0f) {
        // 到达原点
        return AIState::Stand;
    }

    // 继续移动
    monster_->MoveTo(
        state_info_.origin_x,
        state_info_.origin_y,
        state_info_.origin_z
    );

    // 检查是否有新目标
    if (HasTarget()) {
        return AIState::Pursuit;
    }

    // 检查超时
    uint32_t current_time = GetCurrentTimeMs();
    if (state_info_.IsStateExpired(current_time)) {
        // 超时，传送回原点
        monster_->TeleportTo(
            state_info_.origin_x,
            state_info_.origin_y,
            state_info_.origin_z
        );
        return AIState::Stand;
    }

    return AIState::Return;
}

void AIState_Return::Exit() {
    spdlog::debug("[AI-{}] Return: Exited", monster_->GetObjectId());
}

// ========== Die State ==========

AIState_Die::AIState_Die(Monster* monster)
    : AIStateBase(AIState::Die, monster) {
}

void AIState_Die::Enter() {
    uint32_t current_time = GetCurrentTimeMs();

    // 停止所有行动
    monster_->Stop();

    // 播放死亡动画
    monster_->PlayDeathAnimation();

    state_info_.state_start_time = current_time;
    state_info_.state_end_time = current_time + 5000;  // 5秒后消失

    spdlog::debug("[AI-{}] Die: Started", monster_->GetObjectId());
}

AIState AIState_Die::Update(float delta_time) {
    // 死亡状态不切换，等待对象被销毁
    return AIState::Die;
}

// ========== AIStateMachine ==========

AIStateMachine::AIStateMachine(Monster* monster)
    : monster_(monster),
      current_state_ptr_(nullptr) {

    // 初始化随机数种子
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

AIStateMachine::~AIStateMachine() {
    states_.clear();
}

void AIStateMachine::Init() {
    CreateStates();
    ChangeState(AIState::Stand);

    spdlog::debug("[AI-{}] StateMachine: Initialized",
                 monster_->GetObjectId());
}

void AIStateMachine::CreateStates() {
    CreateState<AIState_Stand>(AIState::Stand);
    CreateState<AIState_Rest>(AIState::Rest);
    CreateState<AIState_WalkAround>(AIState::WalkAround);
    CreateState<AIState_Search>(AIState::Search);
    CreateState<AIState_Pursuit>(AIState::Pursuit);
    CreateState<AIState_Attack>(AIState::Attack);
    CreateState<AIState_Runaway>(AIState::Runaway);
    CreateState<AIState_Return>(AIState::Return);
    CreateState<AIState_Die>(AIState::Die);
}

template<typename StateType>
void AIStateMachine::CreateState(AIState state) {
    states_[state] = std::make_unique<StateType>(monster_);
}

void AIStateMachine::Update(float delta_time) {
    if (!current_state_ptr_) {
        return;
    }

    // 更新当前状态
    AIState next_state = current_state_ptr_->Update(delta_time);

    // 检查是否需要切换状态
    if (next_state != state_info_.current_state) {
        ChangeState(next_state);
    }

    // 更新状态首次标志
    if (state_info_.is_state_first) {
        state_info_.is_state_first = false;
    }
}

void AIStateMachine::ChangeState(AIState new_state) {
    if (state_info_.current_state == new_state) {
        return;
    }

    // 退出当前状态
    if (current_state_ptr_) {
        current_state_ptr_->Exit();
    }

    // 切换到新状态
    AIState old_state = state_info_.current_state;
    state_info_.SetState(new_state);

    // 查找新状态
    auto it = states_.find(new_state);
    if (it != states_.end()) {
        current_state_ptr_ = it->second.get();
        current_state_ptr_->Enter();

        spdlog::debug("[AI-{}] StateMachine: {} -> {}",
                     monster_->GetObjectId(),
                     GetStateName(old_state),
                     GetStateName(new_state));
    } else {
        spdlog::error("[AI-{}] StateMachine: State {} not found!",
                     monster_->GetObjectId(),
                     static_cast<int>(new_state));
    }
}

void AIStateMachine::SetTarget(GameObject* target) {
    if (target) {
        state_info_.target_id = target->GetObjectId();
        target->GetPosition(&state_info_.target_x, &state_info_.target_y, &state_info_.target_z);
    } else {
        ClearTarget();
    }
}

void AIStateMachine::ClearTarget() {
    state_info_.target_id = 0;
    state_info_.target_x = 0;
    state_info_.target_y = 0;
    state_info_.target_z = 0;
}

GameObject* AIStateMachine::GetTarget() const {
    if (state_info_.target_id == 0) {
        return nullptr;
    }
    // TODO [需GameObjectManager]: 从GameObjectManager获取目标对象
    // 需要集成GameObjectManager后实现：
    // return GameObjectManager::Instance().GetObject(state_info_.target_id);
    //
    // 注意：需要处理对象已被销毁的情况
    return nullptr;
}

bool AIStateMachine::HasTarget() const {
    return state_info_.target_id != 0;
}

bool AIStateMachine::IsTargetInRange(float range) const {
    return GetDistanceToTarget() <= range;
}

bool AIStateMachine::IsTargetAlive() const {
    GameObject* target = GetTarget();
    return target && target->IsAlive();
}

float AIStateMachine::GetDistanceToTarget() const {
    if (!HasTarget()) {
        return FLT_MAX;
    }

    float current_x, current_y, current_z;
    monster_->GetPosition(&current_x, &current_y, &current_z);

    return CalculateDistance(
        current_x, current_y, current_z,
        state_info_.target_x,
        state_info_.target_y,
        state_info_.target_z
    );
}

float AIStateMachine::GetDistanceToOrigin() const {
    float current_x, current_y, current_z;
    monster_->GetPosition(&current_x, &current_y, &current_z);

    return CalculateDistance(
        current_x, current_y, current_z,
        state_info_.origin_x,
        state_info_.origin_y,
        state_info_.origin_z
    );
}

} // namespace Game
} // namespace Murim
