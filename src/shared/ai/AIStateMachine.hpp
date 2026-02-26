// MurimServer - AI State Machine System
// AI状态机系统 - 管理怪物的AI行为和状态转换
// 对应legacy: Monster.h/cpp中的StateProcess和相关函数

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <map>

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class GameObject;
class Monster;

// ========== AI States ==========
// AI状态枚举
enum class AIState : uint8_t {
    None = 0,
    Stand = 1,           // 站立/待机
    Rest = 2,            // 休息
    WalkAround = 3,      // 巡游
    Pursuit = 4,         // 追击
    Attack = 5,          // 攻击
    Search = 6,          // 搜索目标
    Runaway = 7,         // 逃跑
    Die = 8,             // 死亡
    Return = 9           // 返回原位
};

// ========== AI State Parameters ==========
// AI状态参数 - 控制AI行为的各种参数
struct AIParameters {
    // 巡逻参数
    float walk_around_range;        // 巡游范围
    uint32_t walk_around_time_min;  // 巡游时间最小值(ms)
    uint32_t walk_around_time_max;  // 巡游时间最大值(ms)

    // 搜索参数
    float search_range;             // 搜索范围
    uint32_t search_interval;       // 搜索间隔(ms)

    // 追击参数
    float pursuit_range;            // 追击范围
    uint32_t pursuit_forgive_time;  // 追击宽容时间(ms)

    // 攻击参数
    float attack_range;             // 攻击范围
    uint32_t attack_delay;          // 攻击延迟(ms)

    // 休息参数
    uint32_t rest_time_min;         // 休息时间最小值(ms)
    uint32_t rest_time_max;         // 休息时间最大值(ms)

    // 逃跑参数
    float runaway_hp_percent;       // 逃跑血量百分比
    float runaway_range;            // 逃跑范围

    // 返回参数
    float return_range;             // 返回触发范围
    float return_speed;             // 返回速度

    AIParameters()
        : walk_around_range(500.0f),
          walk_around_time_min(2000),
          walk_around_time_max(5000),
          search_range(1000.0f),
          search_interval(1000),
          pursuit_range(1500.0f),
          pursuit_forgive_time(5000),
          attack_range(100.0f),
          attack_delay(1000),
          rest_time_min(3000),
          rest_time_max(8000),
          runaway_hp_percent(0.2f),
          runaway_range(2000.0f),
          return_range(3000.0f),
          return_speed(5.0f) {}
};

// ========== AI State Info ==========
// AI状态信息 - 记录当前状态的详细信息
struct AIStateInfo {
    AIState current_state;           // 当前状态
    AIState previous_state;          // 上一个状态

    uint32_t state_start_time;       // 状态开始时间
    uint32_t state_end_time;         // 状态结束时间
    bool is_state_first;             // 是否是状态的第一帧

    // 目标对象
    uint64_t target_id;              // 目标ID
    float target_x, target_y, target_z;  // 目标位置

    // 原始位置（用于返回）
    float origin_x, origin_y, origin_z;

    AIStateInfo()
        : current_state(AIState::Stand),
          previous_state(AIState::None),
          state_start_time(0),
          state_end_time(0),
          is_state_first(false),
          target_id(0),
          target_x(0), target_y(0), target_z(0),
          origin_x(0), origin_y(0), origin_z(0) {}

    void SetState(AIState new_state) {
        previous_state = current_state;
        current_state = new_state;
        is_state_first = true;
    }

    bool IsStateExpired(uint32_t current_time) const {
        return current_time >= state_end_time;
    }

    float GetStateProgress(uint32_t current_time) const {
        if (state_end_time <= state_start_time) {
            return 0.0f;
        }
        uint32_t elapsed = current_time - state_start_time;
        uint32_t duration = state_end_time - state_start_time;
        return static_cast<float>(elapsed) / static_cast<float>(duration);
    }
};

// ========== AI State Base ==========
// AI状态基类 - 所有AI状态的父类
class AIStateBase {
public:
    AIStateBase(AIState state, Monster* monster);
    virtual ~AIStateBase() = default;

    // 状态进入
    virtual void Enter() {}

    // 状态更新 - 返回是否应该切换到新状态
    virtual AIState Update(float delta_time);

    // 状态退出
    virtual void Exit() {}

    // 获取状态
    AIState GetState() const { return state_; }

protected:
    AIState state_;
    Monster* monster_;
    AIParameters& ai_params_;
    AIStateInfo& state_info_;
};

// ========== Stand State ==========
// 站立状态 - 怪物原地站立待机
class AIState_Stand : public AIStateBase {
public:
    AIState_Stand(Monster* monster);
    virtual ~AIState_Stand() = default;

    void Enter() override;
    AIState Update(float delta_time) override;
};

// ========== Rest State ==========
// 休息状态 - 怪物休息回血
class AIState_Rest : public AIStateBase {
public:
    AIState_Rest(Monster* monster);
    virtual ~AIState_Rest() = default;

    void Enter() override;
    AIState Update(float delta_time) override;
    void Exit() override;
};

// ========== Walk Around State ==========
// 巡游状态 - 怪物在范围内随机移动
class AIState_WalkAround : public AIStateBase {
private:
    float target_x_, target_y_, target_z_;
    bool has_target_;

public:
    AIState_WalkAround(Monster* monster);
    virtual ~AIState_WalkAround() = default;

    void Enter() override;
    AIState Update(float delta_time) override;

private:
    void GenerateRandomTarget();
};

// ========== Search State ==========
// 搜索状态 - 怪物搜索附近的敌人
class AIState_Search : public AIStateBase {
private:
    uint32_t last_search_time_;
    GameObject* found_target_;

public:
    AIState_Search(Monster* monster);
    virtual ~AIState_Search() = default;

    void Enter() override;
    AIState Update(float delta_time) override;

private:
    GameObject* SearchForTarget();
};

// ========== Pursuit State ==========
// 追击状态 - 怪物追击目标
class AIState_Pursuit : public AIStateBase {
public:
    AIState_Pursuit(Monster* monster);
    virtual ~AIState_Pursuit() = default;

    void Enter() override;
    AIState Update(float delta_time) override;
    void Exit() override;
};

// ========== Attack State ==========
// 攻击状态 - 怪物攻击目标
class AIState_Attack : public AIStateBase {
private:
    uint32_t last_attack_time_;
    uint16_t attack_count_;

public:
    AIState_Attack(Monster* monster);
    virtual ~AIState_Attack() = default;

    void Enter() override;
    AIState Update(float delta_time) override;
    void Exit() override;

private:
    bool PerformAttack();
    void SelectNextAttack();
};

// ========== Runaway State ==========
// 逃跑状态 - 怪物血量低时逃跑
class AIState_Runaway : public AIStateBase {
private:
    float runaway_dir_x_, runaway_dir_z_;

public:
    AIState_Runaway(Monster* monster);
    virtual ~AIState_Runaway() = default;

    void Enter() override;
    AIState Update(float delta_time) override;

private:
    void CalculateRunawayDirection();
};

// ========== Return State ==========
// 返回状态 - 怪物返回原位
class AIState_Return : public AIStateBase {
public:
    AIState_Return(Monster* monster);
    virtual ~AIState_Return() = default;

    void Enter() override;
    AIState Update(float delta_time) override;
    void Exit() override;
};

// ========== Die State ==========
// 死亡状态 - 怪物死亡
class AIState_Die : public AIStateBase {
public:
    AIState_Die(Monster* monster);
    virtual ~AIState_Die() = default;

    void Enter() override;
    AIState Update(float delta_time) override;
};

// ========== AI State Machine ==========
// AI状态机 - 管理AI状态的创建和切换
class AIStateMachine {
private:
    Monster* monster_;
    AIStateInfo state_info_;
    AIParameters ai_params_;

    // 状态池
    std::map<AIState, std::unique_ptr<AIStateBase>> states_;
    AIStateBase* current_state_ptr_;

    // 创建所有状态
    void CreateStates();

public:
    AIStateMachine(Monster* monster);
    ~AIStateMachine();

    // 初始化
    void Init();

    // 更新状态机
    void Update(float delta_time);

    // 状态切换
    void ChangeState(AIState new_state);

    // 获取状态信息
    AIState GetState() const { return state_info_.current_state; }
    const AIStateInfo& GetStateInfo() const { return state_info_; }
    AIParameters& GetParameters() { return ai_params_; }
    const AIParameters& GetParameters() const { return ai_params_; }

    // 设置参数
    void SetParameters(const AIParameters& params) { ai_params_ = params; }

    // 设置原始位置
    void SetOriginPosition(float x, float y, float z) {
        state_info_.origin_x = x;
        state_info_.origin_y = y;
        state_info_.origin_z = z;
    }

    // 目标管理
    void SetTarget(GameObject* target);
    void ClearTarget();
    GameObject* GetTarget() const;

    // 辅助函数
    bool HasTarget() const;
    bool IsTargetInRange(float range) const;
    bool IsTargetAlive() const;
    float GetDistanceToTarget() const;
    float GetDistanceToOrigin() const;

private:
    // 创建特定状态
    template<typename StateType>
    void CreateState(AIState state);
};

// ========== Helper Functions ==========

// 获取当前时间(ms)
uint32_t GetCurrentTimeMs();

// 计算两点距离
float CalculateDistance(float x1, float y1, float z1, float x2, float y2, float z2);

// 生成随机数
float RandomRange(float min, float max);
int RandomRange(int min, int max);

// 获取状态名称
const char* GetStateName(AIState state);

} // namespace Game
} // namespace Murim
