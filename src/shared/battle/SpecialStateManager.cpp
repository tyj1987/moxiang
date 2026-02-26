// MurimServer - Special State Manager Implementation
// 特殊状态管理器实现

#include "SpecialStateManager.hpp"
#include "../utils/TimeUtils.hpp"
#include "../game/GameObject.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>

namespace Murim {
namespace Game {

// ========== 静态成员初始化 ==========

SpecialStateManager* SpecialStateManager::instance_ = nullptr;

// ========== SpecialStateManager ==========

SpecialStateManager::SpecialStateManager() {
    spdlog::info("SpecialStateManager: Constructor called");
}

SpecialStateManager::~SpecialStateManager() {
    Release();
    spdlog::info("SpecialStateManager: Destructor called");
}

SpecialStateManager& SpecialStateManager::GetInstance() {
    if (!instance_) {
        instance_ = new SpecialStateManager();
        instance_->Init();
    }
    return *instance_;
}

void SpecialStateManager::DestroyInstance() {
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
}

void SpecialStateManager::Init() {
    spdlog::info("SpecialStateManager: Initialized");
}

void SpecialStateManager::Release() {
    object_states_.clear();
    callbacks_.clear();
    spdlog::info("SpecialStateManager: Released");
}

// ========== 内部辅助函数 ==========

bool SpecialStateManager::CanApplyState(uint64_t target_id, const StateInstance& state) {
    // 检查目标是否已存在相同状态
    auto it = object_states_.find(target_id);
    if (it != object_states_.end()) {
        auto state_it = it->second.find(state.state_id);
        if (state_it != it->second.end()) {
            // 状态已存在,检查是否可以刷新
            if (state_it->second.is_refreshable) {
                return true;  // 可以刷新
            }
            return false;  // 不可刷新,应用失败
        }
    }

    // 检查互斥状态
    SpecialStateType state_type = state.state_type;

    // 例如: 眩晕和睡眠不能同时存在
    if (HasStateType(target_id, SpecialStateType::kStun) &&
        state_type == SpecialStateType::kSleep) {
        return false;
    }

    if (HasStateType(target_id, SpecialStateType::kSleep) &&
        state_type == SpecialStateType::kStun) {
        return false;
    }

    return true;
}

void SpecialStateManager::ApplyStateEffect(uint64_t target_id, StateInstance& state) {
    // 应用状态的具体效果
    // 这里需要调用GameObject/Player的相关方法

    spdlog::debug("[SpecialStateManager] Applying state effect: type={}, value={}, target={}",
                static_cast<int>(state.state_type), state.value, target_id);

    // 根据状态类型应用效果
    switch (state.state_type) {
        case SpecialStateType::kStun:
            // 禁止移动和攻击
            break;

        case SpecialStateType::kSilence:
            // 禁止使用技能
            break;

        case SpecialStateType::kPoison:
        case SpecialStateType::kBurn:
        case SpecialStateType::kBleed:
            // DoT效果,在Update中处理
            break;

        case SpecialStateType::kRegeneration:
        case SpecialStateType::kManaShield:
            // HoT效果,在Update中处理
            break;

        default:
            break;
    }

    // 调用GameObject接口应用状态效果
    GameObject* obj = GameObjectManager::GetObject(target_id);
    if (obj) {
        obj->OnStateApplied(static_cast<uint32_t>(state.state_type));
        spdlog::debug("[SpecialStateManager] Called OnStateApplied for object {}", target_id);
    }
}

void SpecialStateManager::RemoveStateEffect(uint64_t target_id, const StateInstance& state) {
    // 移除状态的效果

    spdlog::debug("[SpecialStateManager] Removing state effect: type={}, target={}",
                static_cast<int>(state.state_type), target_id);

    // 调用GameObject接口移除状态效果
    GameObject* obj = GameObjectManager::GetObject(target_id);
    if (obj) {
        obj->OnStateRemoved(static_cast<uint32_t>(state.state_type));
        spdlog::debug("[SpecialStateManager] Called OnStateRemoved for object {}", target_id);
    }
}

void SpecialStateManager::TriggerCallbacks(
    uint64_t target_id,
    const StateInstance& state,
    bool is_added) {

    for (auto& callback : callbacks_) {
        callback(target_id, state, is_added);
    }
}

// ========== 状态应用 ==========

StateApplyResult SpecialStateManager::ApplyState(
    uint64_t target_id,
    SpecialStateType state_type,
    uint64_t caster_id,
    uint32_t skill_id,
    uint16_t skill_level,
    uint32_t duration_ms,
    int32_t value,
    int32_t extra_value) {

    StateInstance state;
    state.state_id = skill_id;  // 使用skill_id作为state_id
    state.state_type = state_type;
    state.caster_id = caster_id;
    state.skill_id = skill_id;
    state.skill_level = skill_level;
    state.duration_ms = duration_ms;
    state.value = value;
    state.extra_value = extra_value;

    uint64_t current_time = Utils::GetCurrentTimeMs();
    state.start_time = current_time;
    state.end_time = current_time + duration_ms;
    state.is_permanent = (duration_ms == 0);

    return ApplyState(target_id, state);
}

StateApplyResult SpecialStateManager::ApplyState(
    uint64_t target_id,
    const StateInstance& new_state) {

    // 检查是否可以应用
    if (!CanApplyState(target_id, new_state)) {
        spdlog::warn("[SpecialStateManager] Cannot apply state: type={}, target={}",
                    static_cast<int>(new_state.state_type), target_id);
        return StateApplyResult::Failed;
    }

    auto& states = object_states_[target_id];
    auto it = states.find(new_state.state_id);

    if (it != states.end()) {
        // 状态已存在,尝试刷新或叠加
        StateInstance& existing_state = it->second;

        if (existing_state.is_refreshable) {
            // 刷新持续时间
            uint64_t current_time = Utils::GetCurrentTimeMs();
            existing_state.Refresh(current_time, new_state.duration_ms);

            spdlog::debug("[SpecialStateManager] State refreshed: id={}, target={}",
                        new_state.state_id, target_id);

            return StateApplyResult::Success;
        } else if (existing_state.stack_count < existing_state.max_stack_count) {
            // 增加叠加层数
            if (existing_state.AddStack()) {
                spdlog::debug("[SpecialStateManager] State stacked: id={}, stacks={}, target={}",
                            new_state.state_id, existing_state.stack_count, target_id);
                return StateApplyResult::Stacked;
            }
        }

        return StateApplyResult::AlreadyExists;
    }

    // 添加新状态
    StateInstance state = new_state;
    ApplyStateEffect(target_id, state);

    states[new_state.state_id] = state;

    spdlog::info("[SpecialStateManager] State applied: type={}, id={}, target={}, duration={}ms",
                static_cast<int>(state.state_type), state.state_id, target_id, state.duration_ms);

    // 触发回调
    TriggerCallbacks(target_id, state, true);

    return StateApplyResult::Success;
}

bool SpecialStateManager::RemoveState(uint64_t target_id, uint32_t state_id) {
    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return false;
    }

    auto state_it = it->second.find(state_id);
    if (state_it == it->second.end()) {
        return false;
    }

    // 移除状态效果
    RemoveStateEffect(target_id, state_it->second);

    // 触发回调
    TriggerCallbacks(target_id, state_it->second, false);

    it->second.erase(state_it);

    spdlog::debug("[SpecialStateManager] State removed: id={}, target={}", state_id, target_id);

    return true;
}

size_t SpecialStateManager::RemoveStatesByType(uint64_t target_id, SpecialStateType state_type) {
    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return 0;
    }

    size_t removed_count = 0;
    auto state_it = it->second.begin();

    while (state_it != it->second.end()) {
        if (state_it->second.state_type == state_type) {
            // 移除状态效果
            RemoveStateEffect(target_id, state_it->second);

            // 触发回调
            TriggerCallbacks(target_id, state_it->second, false);

            state_it = it->second.erase(state_it);
            removed_count++;
        } else {
            ++state_it;
        }
    }

    spdlog::debug("[SpecialStateManager] Removed {} states of type {} from target {}",
                removed_count, static_cast<int>(state_type), target_id);

    return removed_count;
}

size_t SpecialStateManager::RemoveAllDebuffs(uint64_t target_id) {
    size_t removed_count = 0;

    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return 0;
    }

    auto state_it = it->second.begin();
    while (state_it != it->second.end()) {
        if (IsDebuffState(state_it->second.state_type)) {
            // 移除状态效果
            RemoveStateEffect(target_id, state_it->second);

            // 触发回调
            TriggerCallbacks(target_id, state_it->second, false);

            state_it = it->second.erase(state_it);
            removed_count++;
        } else {
            ++state_it;
        }
    }

    spdlog::info("[SpecialStateManager] Removed {} debuffs from target {}", removed_count, target_id);

    return removed_count;
}

size_t SpecialStateManager::RemoveAllBuffs(uint64_t target_id) {
    size_t removed_count = 0;

    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return 0;
    }

    auto state_it = it->second.begin();
    while (state_it != it->second.end()) {
        if (IsBuffState(state_it->second.state_type)) {
            // 移除状态效果
            RemoveStateEffect(target_id, state_it->second);

            // 触发回调
            TriggerCallbacks(target_id, state_it->second, false);

            state_it = it->second.erase(state_it);
            removed_count++;
        } else {
            ++state_it;
        }
    }

    spdlog::info("[SpecialStateManager] Removed {} buffs from target {}", removed_count, target_id);

    return removed_count;
}

void SpecialStateManager::ClearAllStates(uint64_t target_id) {
    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return;
    }

    size_t count = it->second.size();

    // 移除所有状态效果
    for (auto& state_pair : it->second) {
        RemoveStateEffect(target_id, state_pair.second);
        TriggerCallbacks(target_id, state_pair.second, false);
    }

    object_states_.erase(it);

    spdlog::info("[SpecialStateManager] Cleared all states from target {} (count: {})",
                target_id, count);
}

// ========== 状态查询 ==========

bool SpecialStateManager::HasState(uint64_t target_id, uint32_t state_id) const {
    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return false;
    }

    return it->second.find(state_id) != it->second.end();
}

bool SpecialStateManager::HasStateType(uint64_t target_id, SpecialStateType state_type) const {
    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return false;
    }

    for (const auto& state_pair : it->second) {
        if (state_pair.second.state_type == state_type) {
            return true;
        }
    }

    return false;
}

const StateInstance* SpecialStateManager::GetState(uint64_t target_id, uint32_t state_id) const {
    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return nullptr;
    }

    auto state_it = it->second.find(state_id);
    if (state_it == it->second.end()) {
        return nullptr;
    }

    return &state_it->second;
}

std::vector<const StateInstance*> SpecialStateManager::GetStatesByType(
    uint64_t target_id,
    SpecialStateType state_type) const {

    std::vector<const StateInstance*> result;

    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return result;
    }

    for (const auto& state_pair : it->second) {
        if (state_pair.second.state_type == state_type) {
            result.push_back(&state_pair.second);
        }
    }

    return result;
}

std::vector<const StateInstance*> SpecialStateManager::GetAllStates(uint64_t target_id) const {
    std::vector<const StateInstance*> result;

    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return result;
    }

    for (const auto& state_pair : it->second) {
        result.push_back(&state_pair.second);
    }

    return result;
}

size_t SpecialStateManager::GetStateCount(uint64_t target_id) const {
    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return 0;
    }

    return it->second.size();
}

// ========== 状态操作 ==========

bool SpecialStateManager::RefreshState(
    uint64_t target_id,
    uint32_t state_id,
    uint32_t new_duration_ms) {

    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return false;
    }

    auto state_it = it->second.find(state_id);
    if (state_it == it->second.end()) {
        return false;
    }

    uint64_t current_time = Utils::GetCurrentTimeMs();
    state_it->second.Refresh(current_time, new_duration_ms);

    spdlog::debug("[SpecialStateManager] State refreshed: id={}, target={}, new_duration={}ms",
                state_id, target_id, new_duration_ms);

    return true;
}

bool SpecialStateManager::AddStateStack(
    uint64_t target_id,
    uint32_t state_id,
    uint8_t count) {

    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return false;
    }

    auto state_it = it->second.find(state_id);
    if (state_it == it->second.end()) {
        return false;
    }

    return state_it->second.AddStack(count);
}

bool SpecialStateManager::RemoveStateStack(
    uint64_t target_id,
    uint32_t state_id,
    uint8_t count) {

    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return false;
    }

    auto state_it = it->second.find(state_id);
    if (state_it == it->second.end()) {
        return false;
    }

    bool still_has_stacks = state_it->second.RemoveStack(count);

    if (!still_has_stacks) {
        // 叠加层数归零,移除状态
        RemoveState(target_id, state_id);
    }

    return still_has_stacks;
}

size_t SpecialStateManager::DispelStates(uint64_t target_id, uint32_t count) {
    size_t dispelled = 0;

    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return 0;
    }

    auto state_it = it->second.begin();
    while (state_it != it->second.end() && dispelled < count) {
        if (state_it->second.is_dispellable) {
            // 移除状态效果
            RemoveStateEffect(target_id, state_it->second);

            // 触发回调
            TriggerCallbacks(target_id, state_it->second, false);

            state_it = it->second.erase(state_it);
            dispelled++;
        } else {
            ++state_it;
        }
    }

    spdlog::info("[SpecialStateManager] Dispelled {} states from target {}", dispelled, target_id);

    return dispelled;
}

size_t SpecialStateManager::PurgeDebuffs(uint64_t target_id, uint32_t count) {
    size_t purged = 0;

    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return 0;
    }

    auto state_it = it->second.begin();
    while (state_it != it->second.end() && purged < count) {
        if (IsDebuffState(state_it->second.state_type)) {
            // 移除状态效果
            RemoveStateEffect(target_id, state_it->second);

            // 触发回调
            TriggerCallbacks(target_id, state_it->second, false);

            state_it = it->second.erase(state_it);
            purged++;
        } else {
            ++state_it;
        }
    }

    spdlog::info("[SpecialStateManager] Purged {} debuffs from target {}", purged, target_id);

    return purged;
}

// ========== 状态更新 ==========

void SpecialStateManager::UpdateAllStates(uint64_t current_time) {
    // 更新所有对象的状态
    for (auto& object_pair : object_states_) {
        UpdateObjectStates(object_pair.first, current_time);
    }
}

void SpecialStateManager::UpdateObjectStates(uint64_t target_id, uint64_t current_time) {
    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return;
    }

    auto state_it = it->second.begin();
    while (state_it != it->second.end()) {
        StateInstance& state = state_it->second;

        // 检查是否过期
        if (state.IsExpired(current_time)) {
            spdlog::debug("[SpecialStateManager] State expired: id={}, target={}",
                        state.state_id, target_id);

            // 移除状态效果
            RemoveStateEffect(target_id, state);

            // 触发回调
            TriggerCallbacks(target_id, state, false);

            state_it = it->second.erase(state_it);
        } else {
            // 处理Tick效果(DoT/HoT)
            if (state.state_type == SpecialStateType::kPoison ||
                state.state_type == SpecialStateType::kBurn ||
                state.state_type == SpecialStateType::kBleed) {
                // 应用DoT伤害
                GameObject* obj = GameObjectManager::GetObject(target_id);
                if (obj && obj->IsAlive()) {
                    uint32_t damage = static_cast<uint32_t>(std::abs(state.value));
                    obj->RemoveHP(damage);
                    spdlog::debug("[SpecialStateManager] DoT tick: type={}, target={}, damage={}",
                                static_cast<int>(state.state_type), target_id, damage);
                }
            } else if (state.state_type == SpecialStateType::kRegeneration) {
                // 应用HoT治疗
                GameObject* obj = GameObjectManager::GetObject(target_id);
                if (obj && obj->IsAlive()) {
                    uint32_t heal = static_cast<uint32_t>(std::abs(state.value));
                    obj->AddHP(heal);
                    spdlog::debug("[SpecialStateManager] HoT tick: type={}, target={}, heal={}",
                                static_cast<int>(state.state_type), target_id, heal);
                }
            }

            ++state_it;
        }
    }
}

size_t SpecialStateManager::CleanExpiredStates() {
    uint64_t current_time = Utils::GetCurrentTimeMs();
    size_t cleaned_count = 0;

    auto object_it = object_states_.begin();
    while (object_it != object_states_.end()) {
        auto state_it = object_it->second.begin();
        while (state_it != object_it->second.end()) {
            if (state_it->second.IsExpired(current_time)) {
                state_it = object_it->second.erase(state_it);
                cleaned_count++;
            } else {
                ++state_it;
            }
        }

        // 如果对象没有状态了,移除对象条目
        if (object_it->second.empty()) {
            object_it = object_states_.erase(object_it);
        } else {
            ++object_it;
        }
    }

    if (cleaned_count > 0) {
        spdlog::debug("[SpecialStateManager] Cleaned {} expired states", cleaned_count);
    }

    return cleaned_count;
}

// ========== 回调管理 ==========

void SpecialStateManager::RegisterCallback(StateChangeCallback callback) {
    callbacks_.push_back(callback);
    spdlog::debug("[SpecialStateManager] Callback registered, total callbacks: {}",
                callbacks_.size());
}

void SpecialStateManager::UnregisterCallback(StateChangeCallback callback) {
    auto it = std::find(callbacks_.begin(), callbacks_.end(), callback);
    if (it != callbacks_.end()) {
        callbacks_.erase(it);
        spdlog::debug("[SpecialStateManager] Callback unregistered, remaining callbacks: {}",
                    callbacks_.size());
    }
}

// ========== 状态效果计算 ==========

float SpecialStateManager::CalculateStateEffectOnAttribute(
    uint64_t target_id,
    const std::string& attribute_name,
    float base_value) const {

    float modifier = 0.0f;

    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return base_value;
    }

    for (const auto& state_pair : it->second) {
        const StateInstance& state = state_pair.second;

        // 根据状态类型计算属性修正
        if (attribute_name == "move_speed") {
            if (state.state_type == SpecialStateType::kSlow) {
                modifier -= static_cast<float>(state.value) / 100.0f;  // 百分比减速
            } else if (state.state_type == SpecialStateType::kHaste) {
                modifier += static_cast<float>(state.value) / 100.0f;  // 百分比加速
            } else if (state.state_type == SpecialStateType::kRoot ||
                       state.state_type == SpecialStateType::kStun ||
                       state.state_type == SpecialStateType::kFreeze) {
                return 0.0f;  // 无法移动
            }
        } else if (attribute_name == "attack_power") {
            if (state.state_type == SpecialStateType::kWeaken) {
                modifier -= static_cast<float>(state.value) / 100.0f;
            } else if (state.state_type == SpecialStateType::kStrength) {
                modifier += static_cast<float>(state.value) / 100.0f;
            }
        }
    }

    float result = base_value * (1.0f + modifier);

    spdlog::trace("[SpecialStateManager] Attribute modified: attr={}, base={}, modifier={}, result={}",
                attribute_name, base_value, modifier, result);

    return result;
}

bool SpecialStateManager::IsActionBlocked(
    uint64_t target_id,
    const std::string& action_name) const {

    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        return false;
    }

    for (const auto& state_pair : it->second) {
        SpecialStateType state_type = state_pair.second.state_type;

        if (action_name == "move" || action_name == "walk" || action_name == "run") {
            if (state_type == SpecialStateType::kStun ||
                state_type == SpecialStateType::kFreeze ||
                state_type == SpecialStateType::kRoot ||
                state_type == SpecialStateType::kSleep) {
                return true;
            }
        } else if (action_name == "attack" || action_name == "skill") {
            if (state_type == SpecialStateType::kStun ||
                state_type == SpecialStateType::kSilence ||
                state_type == SpecialStateType::kDisarm ||
                state_type == SpecialStateType::kSleep) {
                return true;
            }
        } else if (action_name == "use_item") {
            if (state_type == SpecialStateType::kStun ||
                state_type == SpecialStateType::kSleep) {
                return true;
            }
        }
    }

    return false;
}

// ========== 调试与日志 ==========

void SpecialStateManager::PrintObjectStates(uint64_t target_id) const {
    auto it = object_states_.find(target_id);
    if (it == object_states_.end()) {
        spdlog::info("[SpecialStateManager] Target {} has no states", target_id);
        return;
    }

    spdlog::info("[SpecialStateManager] States for target {} (count: {}):",
                target_id, it->second.size());

    for (const auto& state_pair : it->second) {
        const StateInstance& state = state_pair.second;
        spdlog::info("  - State: id={}, type={}, stacks={}, duration={}ms, remaining={}ms",
                    state.state_id,
                    static_cast<int>(state.state_type),
                    state.stack_count,
                    state.duration_ms,
                    state.GetRemainingTime(Utils::GetCurrentTimeMs()));
    }
}

void SpecialStateManager::GetStatistics(
    size_t* total_objects,
    size_t* total_states,
    size_t* expired_states) const {

    if (total_objects) {
        *total_objects = object_states_.size();
    }

    if (total_states) {
        size_t count = 0;
        for (const auto& object_pair : object_states_) {
            count += object_pair.second.size();
        }
        *total_states = count;
    }

    if (expired_states) {
        uint64_t current_time = Utils::GetCurrentTimeMs();
        size_t count = 0;
        for (const auto& object_pair : object_states_) {
            for (const auto& state_pair : object_pair.second) {
                if (state_pair.second.IsExpired(current_time)) {
                    count++;
                }
            }
        }
        *expired_states = count;
    }
}

// ========== Helper Functions ==========

StateInstance CreateStateInstance(
    SpecialStateType state_type,
    uint64_t caster_id,
    uint32_t skill_id,
    uint16_t skill_level,
    uint32_t duration_ms,
    int32_t value) {

    StateInstance state;
    state.state_id = skill_id;
    state.state_type = state_type;
    state.caster_id = caster_id;
    state.skill_id = skill_id;
    state.skill_level = skill_level;
    state.duration_ms = duration_ms;

    uint64_t current_time = Utils::GetCurrentTimeMs();
    state.start_time = current_time;
    state.end_time = current_time + duration_ms;
    state.is_permanent = (duration_ms == 0);

    state.value = value;

    return state;
}

bool IsBuffState(SpecialStateType state_type) {
    // 正面效果
    return (state_type == SpecialStateType::kStrength ||
            state_type == SpecialStateType::kHaste ||
            state_type == SpecialStateType::kRegeneration ||
            state_type == SpecialStateType::kShield ||
            state_type == SpecialStateType::kInvincible ||
            state_type == SpecialStateType::kStealth ||
            state_type == SpecialStateType::kManaShield);
}

bool IsDebuffState(SpecialStateType state_type) {
    // 负面效果
    return (state_type == SpecialStateType::kStun ||
            state_type == SpecialStateType::kFreeze ||
            state_type == SpecialStateType::kSleep ||
            state_type == SpecialStateType::kSilence ||
            state_type == SpecialStateType::kDisarm ||
            state_type == SpecialStateType::kRoot ||
            state_type == SpecialStateType::kTaunt ||
            state_type == SpecialStateType::kConfusion ||
            state_type == SpecialStateType::kFear ||
            state_type == SpecialStateType::kPoison ||
            state_type == SpecialStateType::kBurn ||
            state_type == SpecialStateType::kBleed ||
            state_type == SpecialStateType::kSlow ||
            state_type == SpecialStateType::kWeaken);
}

bool IsControlState(SpecialStateType state_type) {
    // 控制效果(阻止行动)
    return (state_type == SpecialStateType::kStun ||
            state_type == SpecialStateType::kFreeze ||
            state_type == SpecialStateType::kSleep ||
            state_type == SpecialStateType::kSilence ||
            state_type == SpecialStateType::kDisarm ||
            state_type == SpecialStateType::kRoot);
}

bool IsMovementState(SpecialStateType state_type) {
    // 移动效果(影响移动)
    return (state_type == SpecialStateType::kStun ||
            state_type == SpecialStateType::kFreeze ||
            state_type == SpecialStateType::kRoot ||
            state_type == SpecialStateType::kSlow ||
            state_type == SpecialStateType::kHaste ||
            state_type == SpecialStateType::kFear);
}

const char* GetStateTypeString(SpecialStateType state_type) {
    switch (state_type) {
        case SpecialStateType::kNone: return "None";
        case SpecialStateType::kStun: return "Stun";
        case SpecialStateType::kFreeze: return "Freeze";
        case SpecialStateType::kSleep: return "Sleep";
        case SpecialStateType::kSilence: return "Silence";
        case SpecialStateType::kDisarm: return "Disarm";
        case SpecialStateType::kRoot: return "Root";
        case SpecialStateType::kTaunt: return "Taunt";
        case SpecialStateType::kConfusion: return "Confusion";
        case SpecialStateType::kFear: return "Fear";
        case SpecialStateType::kPoison: return "Poison";
        case SpecialStateType::kBurn: return "Burn";
        case SpecialStateType::kBleed: return "Bleed";
        case SpecialStateType::kSlow: return "Slow";
        case SpecialStateType::kHaste: return "Haste";
        case SpecialStateType::kStrength: return "Strength";
        case SpecialStateType::kWeaken: return "Weaken";
        case SpecialStateType::kRegeneration: return "Regeneration";
        case SpecialStateType::kShield: return "Shield";
        case SpecialStateType::kInvincible: return "Invincible";
        case SpecialStateType::kStealth: return "Stealth";
        case SpecialStateType::kManaShield: return "ManaShield";
        case SpecialStateType::kKnockback: return "Knockback";
        case SpecialStateType::kPull: return "Pull";
        case SpecialStateType::kKnockup: return "Knockup";
        default: return "Unknown";
    }
}

} // namespace Game
} // namespace Murim
