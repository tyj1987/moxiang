// MurimServer - Special State Manager
// 特殊状态管理器 - 管理所有Buff/Debuff/特殊效果
// 对应legacy: SpecialState相关系统的管理器

#pragma once

#include "SpecialStates.hpp"
#include <cstdint>
#include <memory>
#include <vector>
#include <map>
#include <functional>

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class GameObject;
class Player;
class Monster;

// ========== State Apply Result ==========
// 状态应用结果
enum class StateApplyResult : uint8_t {
    Success = 0,           // 成功应用
    Failed = 1,            // 应用失败
    AlreadyExists = 2,     // 状态已存在
    Immune = 3,            // 免疫
    Replaced = 4,          // 被新状态替换
    Stacked = 5            // 叠加
};

// ========== State Instance ==========
// 状态实例 - 记录单个状态的所有信息
struct StateInstance {
    uint32_t state_id;                 // 状态ID
    SpecialStateType state_type;       // 状态类型
    uint64_t caster_id;                // 施法者ID
    uint32_t skill_id;                 // 来源技能ID
    uint16_t skill_level;              // 技能等级

    uint64_t start_time;               // 开始时间(毫秒)
    uint64_t end_time;                 // 结束时间(毫秒)
    uint32_t duration_ms;              // 持续时间(毫秒)

    int32_t value;                     // 效果值(可正可负)
    int32_t extra_value;               // 额外值

    uint8_t stack_count;               // 叠加层数
    uint8_t max_stack_count;           // 最大叠加层数

    bool is_permanent;                 // 是否永久
    bool is_dispellable;               // 是否可驱散
    bool is_refreshable;               // 是否可刷新

    StateInstance()
        : state_id(0),
          state_type(SpecialStateType::kNone),
          caster_id(0),
          skill_id(0),
          skill_level(0),
          start_time(0),
          end_time(0),
          duration_ms(0),
          value(0),
          extra_value(0),
          stack_count(1),
          max_stack_count(1),
          is_permanent(false),
          is_dispellable(true),
          is_refreshable(true) {}

    // 检查状态是否过期
    bool IsExpired(uint64_t current_time) const {
        if (is_permanent) return false;
        return current_time >= end_time;
    }

    // 获取剩余时间(毫秒)
    uint64_t GetRemainingTime(uint64_t current_time) const {
        if (is_permanent) return UINT64_MAX;
        if (current_time >= end_time) return 0;
        return end_time - current_time;
    }

    // 刷新持续时间
    void Refresh(uint64_t current_time, uint32_t new_duration_ms) {
        if (!is_refreshable) return;
        start_time = current_time;
        duration_ms = new_duration_ms;
        end_time = current_time + new_duration_ms;
    }

    // 增加叠加层数
    bool AddStack(uint8_t count = 1) {
        if (stack_count >= max_stack_count) {
            return false;
        }
        uint16_t new_count = static_cast<uint16_t>(stack_count) + static_cast<uint16_t>(count);
        stack_count = static_cast<uint8_t>(std::min(new_count, static_cast<uint16_t>(max_stack_count)));
        return true;
    }

    // 减少叠加层数
    bool RemoveStack(uint8_t count = 1) {
        if (stack_count <= count) {
            stack_count = 0;
            return false;  // 状态应该被移除
        }
        stack_count -= count;
        return true;
    }
};

// ========== State Change Callback ==========
// 状态变化回调函数类型
using StateChangeCallback = std::function<void(
    uint64_t target_id,
    const StateInstance& state,
    bool is_added
)>;

// ========== Special State Manager ==========
// 特殊状态管理器
class SpecialStateManager {
private:
    // 单例
    static SpecialStateManager* instance_;

    // 所有对象的状态
    // object_id -> state_id -> StateInstance
    std::map<uint64_t, std::map<uint32_t, StateInstance>> object_states_;

    // 状态变化回调
    std::vector<StateChangeCallback> callbacks_;

    // ========== 内部辅助函数 ==========

    // 检查状态是否可以应用
    bool CanApplyState(uint64_t target_id, const StateInstance& state);

    // 应用状态效果
    void ApplyStateEffect(uint64_t target_id, StateInstance& state);

    // 移除状态效果
    void RemoveStateEffect(uint64_t target_id, const StateInstance& state);

    // 触发状态变化回调
    void TriggerCallbacks(uint64_t target_id, const StateInstance& state, bool is_added);

public:
    SpecialStateManager();
    ~SpecialStateManager();

    // ========== 单例访问 ==========
    static SpecialStateManager& GetInstance();
    static void DestroyInstance();

    // ========== 初始化与清理 ==========
    void Init();
    void Release();

    // ========== 状态应用 ==========

    // 应用状态到目标
    StateApplyResult ApplyState(
        uint64_t target_id,
        SpecialStateType state_type,
        uint64_t caster_id,
        uint32_t skill_id,
        uint16_t skill_level,
        uint32_t duration_ms,
        int32_t value = 0,
        int32_t extra_value = 0
    );

    // 应用状态(使用StateInstance)
    StateApplyResult ApplyState(uint64_t target_id, const StateInstance& state);

    // 移除指定状态
    bool RemoveState(uint64_t target_id, uint32_t state_id);

    // 移除指定类型的所有状态
    size_t RemoveStatesByType(uint64_t target_id, SpecialStateType state_type);

    // 移除所有负面状态(Debuff)
    size_t RemoveAllDebuffs(uint64_t target_id);

    // 移除所有正面状态(Buff)
    size_t RemoveAllBuffs(uint64_t target_id);

    // 移除所有状态
    void ClearAllStates(uint64_t target_id);

    // ========== 状态查询 ==========

    // 检查是否拥有指定状态
    bool HasState(uint64_t target_id, uint32_t state_id) const;

    // 检查是否拥有指定类型的状态
    bool HasStateType(uint64_t target_id, SpecialStateType state_type) const;

    // 获取指定状态
    const StateInstance* GetState(uint64_t target_id, uint32_t state_id) const;

    // 获取指定类型的所有状态
    std::vector<const StateInstance*> GetStatesByType(
        uint64_t target_id,
        SpecialStateType state_type
    ) const;

    // 获取所有状态
    std::vector<const StateInstance*> GetAllStates(uint64_t target_id) const;

    // 获取状态数量
    size_t GetStateCount(uint64_t target_id) const;

    // ========== 状态操作 ==========

    // 刷新状态持续时间
    bool RefreshState(uint64_t target_id, uint32_t state_id, uint32_t new_duration_ms);

    // 增加状态叠加层数
    bool AddStateStack(uint64_t target_id, uint32_t state_id, uint8_t count = 1);

    // 减少状态叠加层数
    bool RemoveStateStack(uint64_t target_id, uint32_t state_id, uint8_t count = 1);

    // 驱散状态(移除可驱散的状态)
    size_t DispelStates(uint64_t target_id, uint32_t count);

    // 净化状态(移除Debuff)
    size_t PurgeDebuffs(uint64_t target_id, uint32_t count);

    // ========== 状态更新 ==========

    // 更新所有对象的状态(每帧调用)
    void UpdateAllStates(uint64_t current_time);

    // 更新指定对象的状态
    void UpdateObjectStates(uint64_t target_id, uint64_t current_time);

    // 清理所有过期状态
    size_t CleanExpiredStates();

    // ========== 回调管理 ==========

    // 注册状态变化回调
    void RegisterCallback(StateChangeCallback callback);

    // 取消注册回调
    void UnregisterCallback(StateChangeCallback callback);

    // ========== 状态效果计算 ==========

    // 计算状态对属性的影响
    float CalculateStateEffectOnAttribute(
        uint64_t target_id,
        const std::string& attribute_name,
        float base_value
    ) const;

    // 检查状态是否阻止指定动作
    bool IsActionBlocked(
        uint64_t target_id,
        const std::string& action_name
    ) const;

    // ========== 调试与日志 ==========

    // 打印对象的所有状态(调试用)
    void PrintObjectStates(uint64_t target_id) const;

    // 获取状态统计信息
    void GetStatistics(
        size_t* total_objects,
        size_t* total_states,
        size_t* expired_states
    ) const;
};

// ========== Helper Functions ==========

// 创建状态实例
StateInstance CreateStateInstance(
    SpecialStateType state_type,
    uint64_t caster_id,
    uint32_t skill_id,
    uint16_t skill_level,
    uint32_t duration_ms,
    int32_t value = 0
);

// 检查状态类型是Buff还是Debuff
bool IsBuffState(SpecialStateType state_type);
bool IsDebuffState(SpecialStateType state_type);
bool IsControlState(SpecialStateType state_type);
bool IsMovementState(SpecialStateType state_type);

// 获取状态类型的描述
const char* GetStateTypeString(SpecialStateType state_type);

} // namespace Game
} // namespace Murim
