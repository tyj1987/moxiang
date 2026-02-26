// MurimServer - SkillObject Implementation
// 技能对象系统实现 - 对应legacy: skillobject_server.cpp及相关实现

#include "SkillObject.hpp"
#include "SkillInfo.hpp"
#include "../game/GameObject.hpp"
#include "../game/GameObjectManager.hpp"
#include "../battle/SpecialStates.hpp"
#include "../battle/DamageCalculator.hpp"
#include "../battle/HealCalculator.hpp"
#include "../utils/TimeUtils.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <cmath>
#include <algorithm>
#include <chrono>

namespace Murim {
namespace Game {

// ========== Helper Functions ==========

static float DistanceBetween(float x1, float y1, float z1, float x2, float y2, float z2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// ========== SkillObjectTerminator_Time ==========

void SkillObjectTerminator_Time::Init(const SkillInfo* skill_info, uint16_t skill_level) {
    // 从skill_info获取持续时间
    if (skill_info) {
        float duration_sec = skill_info->GetDuration(skill_level);
        duration_ms_ = static_cast<uint32_t>(duration_sec * 1000.0f);
    } else {
        duration_ms_ = 5000;  // 默认5秒
    }

    spdlog::debug("[SkillObject-Time] Duration: {}ms (skill_level={})",
                duration_ms_, skill_level);
}

bool SkillObjectTerminator_Time::CheckTerminate(const SkillObjectInfo& info) {
    uint64_t current_time = TimeUtils::GetCurrentTimeMs();
    return (current_time - info.start_time) >= duration_ms_;
}

// ========== SkillObjectTargetList ==========

SkillObjectTargetList::SkillObjectTargetList()
    : skill_object_(nullptr), position_set_head_(false),
      positive_target_type_(0), negative_target_type_(0),
      skill_range_(0), skill_area_idx_(0),
      current_iterator_(target_table_.end()) {
}

SkillObjectTargetList::~SkillObjectTargetList() {
    Release();
}

uint32_t SkillObjectTargetList::UpdateTargetList(GameObject* center_object) {
    if (!center_object || !skill_object_) {
        return 0;
    }

    // 清空当前目标列表
    target_table_.clear();

    // 获取技能信息
    const SkillInfo* skill_info = skill_object_->GetSkillInfo();
    if (!skill_info) {
        return 0;
    }

    // 获取技能范围
    float range = skill_info->GetRange(skill_object_->GetSkillLevel());
    if (range <= 0.0f) {
        range = skill_range_;  // 使用默认范围
    }

    // 获取中心对象位置
    float x, y, z;
    center_object->GetPosition(x, y, z);

    // 从GameObjectManager获取范围内的所有对象
    std::vector<GameObject*> objects_in_range =
        GameObjectManager::GetObjectsInRange(x, y, z, range, ObjectType::kAll);

    // 根据目标类型过滤并添加到目标列表
    uint32_t count = 0;
    for (GameObject* target : objects_in_range) {
        if (!target) {
            continue;
        }

        // 检查目标类型
        if (!IsValidTarget(target)) {
            continue;
        }

        // 添加到目标列表
        uint64_t target_id = target->GetObjectId();
        target_table_[target_id].target = target;
        count++;

        spdlog::trace("[SkillObject-TargetList] Added target: {}", target_id);
    }

    spdlog::debug("[SkillObject-TargetList] Updated targets: {} (range={:.1f})",
                count, range);

    return count;
}

void SkillObjectTargetList::SetPositionHead() {
    current_iterator_ = target_table_.begin();
    position_set_head_ = true;
}

STList* SkillObjectTargetList::GetNextTargetList() {
    if (current_iterator_ == target_table_.end()) {
        return nullptr;
    }
    STList* result = &current_iterator_->second;
    ++current_iterator_;
    return result;
}

GameObject* SkillObjectTargetList::GetNextTarget() {
    STList* stlist = GetNextTargetList();
    return stlist ? stlist->object : nullptr;
}

bool SkillObjectTargetList::GetNextTarget(SkillResultKind kind,
                                          GameObject** out_object,
                                          SkillResultKind* out_target_kind) {
    while (current_iterator_ != target_table_.end()) {
        STList& stlist = current_iterator_->second;
        ++current_iterator_;

        if (stlist.target_kind == kind) {
            *out_object = stlist.object;
            *out_target_kind = stlist.target_kind;
            return true;
        }
    }
    return false;
}

bool SkillObjectTargetList::GetNextNegativeTarget(GameObject** out_object, SkillResultKind* out_kind) {
    return GetNextTarget(SkillResultKind::Negative, out_object, out_kind);
}

bool SkillObjectTargetList::GetNextPositiveTarget(GameObject** out_object, SkillResultKind* out_kind) {
    return GetNextTarget(SkillResultKind::Positive, out_object, out_kind);
}

bool SkillObjectTargetList::IsInTargetList(GameObject* object) const {
    if (!object) return false;
    return target_table_.find(object->GetObjectId()) != target_table_.end();
}

bool SkillObjectTargetList::IsInTargetList(uint64_t object_id) const {
    return target_table_.find(object_id) != target_table_.end();
}

SkillResultKind SkillObjectTargetList::GetTargetKind(uint64_t object_id) const {
    auto it = target_table_.find(object_id);
    if (it != target_table_.end()) {
        return it->second.target_kind;
    }
    return SkillResultKind::None;
}

void SkillObjectTargetList::InitTargetList(const std::vector<GameObject*>& initial_targets,
                                          GameObject* additional_target,
                                          const MainTargetInfo& main_target) {
    DoInitTargetList();

    // 添加初始目标
    for (GameObject* target : initial_targets) {
        if (target) {
            // 根据技能类型判断目标类型（伤害类为负面，治疗类为正面）
            bool is_negative_skill = skill_object_ && skill_object_->GetSkillInfo() &&
                                     skill_object_->GetSkillInfo()->GetBaseDamage(1) > 0;
            SkillResultKind kind = is_negative_skill ? SkillResultKind::Negative : SkillResultKind::Positive;
            AddTargetObject(target, kind);
        }
    }

    // 添加额外目标
    if (additional_target) {
        AddTargetObject(additional_target, SkillResultKind::Positive);
    }
}

void SkillObjectTargetList::Release() {
    target_table_.clear();
    DoRelease();
}

uint8_t SkillObjectTargetList::AddTargetObject(GameObject* object, SkillResultKind kind) {
    if (!object) return 0;

    STList stlist;
    stlist.object = object;
    stlist.object_id = object->GetObjectId();
    stlist.target_kind = kind;

    target_table_[stlist.object_id] = stlist;
    return 1;  // SOTL_ADDED
}

uint8_t SkillObjectTargetList::RemoveTargetObject(uint64_t object_id) {
    auto it = target_table_.find(object_id);
    if (it != target_table_.end()) {
        target_table_.erase(it);
        return 2;  // SOTL_REMOVED
    }
    return 0;  // SOTL_NOTCHANGED
}

bool SkillObjectTargetList::IsValidTarget(GameObject* object) const {
    // 基础实现：简单的nullptr检查
    if (!object) {
        return false;
    }

    // 检查对象是否存活
    if (!object->IsAlive()) {
        return false;
    }

    // TODO [完整实现]: 可以添加更多目标验证逻辑
    // - 检查目标类型（玩家/怪物/NPC）
    // - 检查阵营关系（敌对/友方）
    // - 检查状态（是否可攻击/可治疗）
    // - 检查距离（虽然在IsInTargetArea中也会检查）

    return true;
}

// ========== SkillObjectTargetList_Area ==========

bool SkillObjectTargetList_Area::IsInTargetArea(GameObject* object) const {
    if (!object) return false;

    float obj_x, obj_y, obj_z;
    object->GetPosition(&obj_x, &obj_y, &obj_z);

    float distance = DistanceBetween(area_x_, area_y_, area_z_, obj_x, obj_y, obj_z);
    return distance <= range_;
}

// ========== SkillObjectTargetList_Circle ==========

bool SkillObjectTargetList_Circle::IsInTargetArea(GameObject* object) const {
    if (!object) return false;

    float obj_x, obj_y, obj_z;
    object->GetPosition(&obj_x, &obj_y, &obj_z);

    float distance = DistanceBetween(center_x_, center_y_, center_z_, obj_x, obj_y, obj_z);
    return distance <= radius_;
}

// ========== SkillObjectTargetList_One ==========

bool SkillObjectTargetList_One::IsInTargetArea(GameObject* object) const {
    if (!object) return false;
    return object->GetObjectId() == target_id_;
}

// ========== SkillObjectFirstUnit ==========

void SkillObjectFirstUnit::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 从skill_info读取数据
    if (skill_info) {
        // 获取技能元素类型
        attrib_ = static_cast<uint8_t>(skill_info->GetElement());
    } else {
        attrib_ = 0;  // 默认无属性
    }

    recover_state_abnormal_ = 0.0f;
    stun_time_ = 0;
    stun_rate_ = 0.0f;
    dispel_attack_feel_rate_ = 0.0f;
}

int SkillObjectFirstUnit::ExecuteFirstUnit(GameObject* operator_obj,
                                          SkillObjectTargetList* target_list,
                                          float skill_tree_amp) {
    // 默认实现：遍历所有目标并应用效果
    if (!target_list) return SO_OK;

    target_list->SetPositionHead();
    STList* target;
    while ((target = target_list->GetNextTargetList()) != nullptr) {
        if (target->object) {
            // 应用默认效果（已由子类实现具体逻辑）
            spdlog::debug("FirstUnit applied to object {}", target->object_id);
        }
    }

    return SO_OK;
}

// ========== SkillObjectFirstUnit_Attack ==========

void SkillObjectFirstUnit_Attack::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    SkillObjectFirstUnit::Init(skill_info, skill_level, oper);

    // 从skill_info读取
    if (skill_info) {
        // 获取基础伤害
        uint32_t base_damage = skill_info->GetBaseDamage(skill_level);
        physical_attack_ = static_cast<float>(base_damage);

        // 获取伤害倍率
        float damage_rate = skill_info->GetDamageRate(skill_level);
        attack_attack_rate_ = damage_rate;

        // 获取元素攻击
        SkillElement element = skill_info->GetElement();
        attrib_attack_ = static_cast<uint8_t>(element);

        spdlog::debug("[SkillObject-Attack] Init: base_dmg={}, rate={}, element={}",
                    base_damage, damage_rate, static_cast<int>(element));
    } else {
        physical_attack_ = 100.0f;
        attrib_ = 1;  // 火属性
        attrib_attack_ = 50;
        attack_attack_rate_ = 1.0f;
    }
}

int SkillObjectFirstUnit_Attack::ExecuteFirstUnit(GameObject* operator_obj,
                                                  SkillObjectTargetList* target_list,
                                                  float skill_tree_amp) {
    if (!operator_obj || !target_list) return SO_OK;

    target_list->SetPositionHead();
    STList* target;
    int hit_count = 0;

    while ((target = target_list->GetNextTargetList()) != nullptr) {
        if (target->object && target->target_kind == SkillResultKind::Negative) {
            // 使用DamageCalculator计算伤害
            AttackParameters atk_params;
            atk_params.attack_power = physical_attack_ * skill_tree_amp * attack_attack_rate_;
            atk_params.skill_damage_rate = attack_attack_rate_;

            // 从target获取防御参数
            DefenseParameters def_params = DamageCalculator::GetDefenseParameters(target->object);

            DamageResult result = DamageCalculator::CalculateNormalAttack(
                operator_obj,
                target->object,
                atk_params,
                def_params
            );

            // 应用伤害到目标
            target->object->RemoveHP(static_cast<uint32_t>(result.damage));

            hit_count++;

            spdlog::debug("[SkillObject-Attack] {:.0f} damage to {} (hit={}, crit={})",
                        result.damage, target->object_id,
                        result.is_hit, result.is_critical);
        }
    }

    return SO_OK;
}

// ========== SkillObjectFirstUnit_Recover ==========

void SkillObjectFirstUnit_Recover::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    SkillObjectFirstUnit::Init(skill_info, skill_level, oper);

    // 从skill_info读取
    if (skill_info) {
        // 获取基础治疗
        uint32_t base_heal = skill_info->GetBaseHeal(skill_level);
        recover_hp_ = static_cast<float>(base_heal);

        // 获取治疗倍率
        float heal_rate = skill_info->GetHealRate(skill_level);
        recover_mp_ = heal_rate * 50.0f;  // MP恢复基于治疗倍率

        spdlog::debug("[SkillObject-Recover] Init: base_heal={}, heal_rate={}",
                    base_heal, heal_rate);
    } else {
        recover_hp_ = 100.0f;
        recover_mp_ = 50.0f;
    }
}

int SkillObjectFirstUnit_Recover::ExecuteFirstUnit(GameObject* operator_obj,
                                                   SkillObjectTargetList* target_list,
                                                   float skill_tree_amp) {
    if (!target_list) return SO_OK;

    target_list->SetPositionHead();
    STList* target;
    int heal_count = 0;

    while ((target = target_list->GetNextTargetList()) != nullptr) {
        if (target->object && target->target_kind == SkillResultKind::Positive) {
            // 使用HealCalculator计算治疗
            HealParameters heal_params;
            heal_params.heal_power = recover_hp_ * skill_tree_amp;
            heal_params.skill_heal_rate = recover_mp_ / 50.0f;

            HealResult result = HealCalculator::CalculateSkillHeal(
                operator_obj,
                target->object,
                nullptr,  // skill_info - 可选
                1,  // skill_level
                heal_params
            );

            // 应用治疗到目标
            target->object->AddHP(static_cast<uint32_t>(result.actual_heal));

            heal_count++;

            spdlog::debug("[SkillObject-Recover] HP+{:.0f} to {} (overheal={})",
                        result.actual_heal, target->object_id, result.over_heal);
        }
    }

    return SO_OK;
}

// ========== SkillObjectFirstUnit_Job ==========

void SkillObjectFirstUnit_Job::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    SkillObjectFirstUnit::Init(skill_info, skill_level, oper);
    // TODO: 实现职业特定效果
}

// ========== SkillObjectFirstUnit_SingleSpecialState ==========

void SkillObjectFirstUnit_SingleSpecialState::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    SkillObjectFirstUnit::Init(skill_info, skill_level, oper);

    // 从skill_info读取
    if (skill_info) {
        // 获取目标状态ID
        state_id_ = skill_info->GetStateIDTarget();

        // 获取持续时间
        float duration_sec = skill_info->GetDuration(skill_level);
        duration_ = duration_sec;

        // 确定状态类型(根据state_id)
        if (state_id_ >= 1 && state_id_ <= 10) {
            state_type_ = SpecialStateType::kStun;  // 眩晕类
        } else if (state_id_ >= 11 && state_id_ <= 20) {
            state_type_ = SpecialStateType::kBuff;  // Buff类
        } else if (state_id_ >= 21 && state_id_ <= 30) {
            state_type_ = SpecialStateType::kDebuff;  // Debuff类
        } else {
            state_type_ = SpecialStateType::kStun;  // 默认眩晕
        }

        spdlog::debug("[SkillObject-State] Init: state_id={}, duration={:.1f}s",
                    state_id_, duration_);
    } else {
        state_id_ = 1;  // 眩晕
        duration_ = 2.0f;
        state_type_ = SpecialStateType::kStun;
    }
}

int SkillObjectFirstUnit_SingleSpecialState::ExecuteFirstUnit(GameObject* operator_obj,
                                                              SkillObjectTargetList* target_list,
                                                              float skill_tree_amp) {
    if (!target_list) return SO_OK;

    target_list->SetPositionHead();
    STList* target;
    int apply_count = 0;

    while ((target = target_list->GetNextTargetList()) != nullptr) {
        if (target->object && target->target_kind == SkillResultKind::Negative) {
            // 应用状态效果到目标
            uint32_t duration_ms = static_cast<uint32_t>(duration_ * 1000.0f);

            SpecialStateManager::ApplyState(
                target->object->GetObjectId(),
                state_type_,
                operator_obj->GetObjectId(),
                0,  // skill_id
                1,  // skill_level
                duration_ms
            );

            apply_count++;

            spdlog::debug("[SkillObject-State] Applied state {} to {} (duration={:.1f}s)",
                        state_id_, target->object_id, duration_);
        }
    }

    return SO_OK;
}

// ========== SkillObjectSingleUnit ==========

void SkillObjectSingleUnit::Init(const SkillInfo* skill_info, uint16_t skill_level) {
    DoInit(skill_info, skill_level);
}

void SkillObjectSingleUnit::Update(SkillObjectInfo* info,
                                   SkillObjectTargetList* target_list,
                                   float skill_tree_amp) {
    if (!info || !target_list) return;

    uint64_t current_time = TimeUtils::GetCurrentTimeMs();
    uint32_t elapsed = static_cast<uint32_t>(current_time - info->start_time);

    if (interval_ > 0 && elapsed >= interval_ * (operate_count_ + 1)) {
        Operate(info, target_list, skill_tree_amp);
        operate_count_++;
    }
}

// ========== SkillObjectSingleUnit_Attack ==========

void SkillObjectSingleUnit_Attack::DoInit(const SkillInfo* skill_info, uint16_t skill_level) {
    // 从skill_info读取
    if (skill_info) {
        uint32_t base_damage = skill_info->GetBaseDamage(skill_level);
        physical_attack_ = static_cast<float>(base_damage) * 0.5f;  // 持续伤害是基础的50%

        float damage_rate = skill_info->GetDamageRate(skill_level);
        attack_attack_rate_ = damage_rate * 0.5f;

        SkillElement element = skill_info->GetElement();
        attrib_ = static_cast<uint8_t>(element);
        attrib_attack_ = static_cast<uint8_t>(element) * 25;

        spdlog::debug("[SkillObject-SingleAttack] Init: base_dmg={}, rate={}",
                    physical_attack_, attack_attack_rate_);
    } else {
        physical_attack_ = 50.0f;
        attrib_ = 1;
        attrib_attack_ = 25;
        attack_attack_rate_ = 0.5f;
    }
}

void SkillObjectSingleUnit_Attack::Operate(SkillObjectInfo* info,
                                          SkillObjectTargetList* target_list,
                                          float skill_tree_amp) {
    if (!info || !target_list) return;

    target_list->SetPositionHead();
    STList* target;
    while ((target = target_list->GetNextTargetList()) != nullptr) {
        if (target->object && target->target_kind == SkillResultKind::Negative) {
            float damage = physical_attack_ * skill_tree_amp;

            // 应用伤害到GameObject
            GameObject* obj = target->object;
            uint32_t damage_int = static_cast<uint32_t>(damage);

            if (obj->IsAlive()) {
                obj->RemoveHP(damage_int);

                spdlog::debug("SingleUnit_Attack[{}]: {:.0f} damage to object {}, HP now {}/{}",
                             single_unit_num_, damage, target->object_id,
                             obj->GetCurrentHP(), obj->GetMaxHP());
            }
        }
    }
}

// ========== SkillObjectSingleUnit_Heal ==========

void SkillObjectSingleUnit_Heal::Init(const SkillInfo* skill_info, uint16_t skill_level) {
    // 从skill_info读取数据
    if (skill_info) {
        uint32_t base_heal = skill_info->GetBaseHeal(skill_level);
        heal_amount_ = static_cast<float>(base_heal) * 0.3f;  // 每次治疗为基础值的30%

        float duration = skill_info->GetDuration(skill_level);
        interval_ = static_cast<uint32_t>(duration * 1000.0f / 5.0f);  // 持续时间内触发5次
        if (interval_ < 1000) interval_ = 1000;  // 最小1秒间隔

        spdlog::debug("[SkillObject-Heal] Init: heal_per_tick={}, interval={}ms",
                     heal_amount_, interval_);
    } else {
        heal_amount_ = 20.0f;
        interval_ = 1000;  // 1秒
    }
}

void SkillObjectSingleUnit_Heal::Operate(SkillObjectInfo* info,
                                        SkillObjectTargetList* target_list,
                                        float skill_tree_amp) {
    if (!info || !target_list) return;

    float heal = heal_amount_ * skill_tree_amp;

    target_list->SetPositionHead();
    STList* target;
    while ((target = target_list->GetNextTargetList()) != nullptr) {
        if (target->object && target->target_kind == SkillResultKind::Positive) {
            // 应用治疗到GameObject
            GameObject* obj = target->object;
            uint32_t heal_int = static_cast<uint32_t>(heal);

            if (obj->IsAlive()) {
                uint32_t old_hp = obj->GetCurrentHP();
                obj->AddHP(heal_int);
                uint32_t new_hp = obj->GetCurrentHP();

                spdlog::debug("SingleUnit_Heal[{}]: +{:.0f} HP to object {}, HP: {} -> {}/{}",
                             single_unit_num_, heal, target->object_id,
                             old_hp, new_hp, obj->GetMaxHP());
            }
        }
    }
}

// ========== SkillObjectSingleUnit_Recharge ==========

void SkillObjectSingleUnit_Recharge::Init(const SkillInfo* skill_info, uint16_t skill_level) {
    // 从skill_info读取数据
    if (skill_info) {
        uint32_t base_heal = skill_info->GetBaseHeal(skill_level);
        recharge_mp_ = static_cast<float>(base_heal) * 0.2f;  // MP恢复为治疗值的20%

        float duration = skill_info->GetDuration(skill_level);
        interval_ = static_cast<uint32_t>(duration * 1000.0f / 3.0f);  // 持续时间内触发3次
        if (interval_ < 2000) interval_ = 2000;  // 最小2秒间隔

        spdlog::debug("[SkillObject-Recharge] Init: mp_per_tick={}, interval={}ms",
                     recharge_mp_, interval_);
    } else {
        recharge_mp_ = 10.0f;
        interval_ = 2000;  // 2秒
    }
}

void SkillObjectSingleUnit_Recharge::Operate(SkillObjectInfo* info,
                                            SkillObjectTargetList* target_list,
                                            float skill_tree_amp) {
    if (!info || !target_list) return;

    float recharge = recharge_mp_ * skill_tree_amp;

    target_list->SetPositionHead();
    STList* target;
    while ((target = target_list->GetNextTargetList()) != nullptr) {
        if (target->object && target->target_kind == SkillResultKind::Positive) {
            // 恢复MP到GameObject
            GameObject* obj = target->object;
            uint32_t recharge_int = static_cast<uint32_t>(recharge);

            if (obj->IsAlive()) {
                uint32_t old_mp = obj->GetCurrentMP();
                obj->AddMP(recharge_int);
                uint32_t new_mp = obj->GetCurrentMP();

                spdlog::debug("SingleUnit_Recharge[{}]: +{:.0f} MP to object {}, MP: {} -> {}/{}",
                             single_unit_num_, recharge, target->object_id,
                             old_mp, new_mp, obj->GetMaxMP());
            }
        }
    }
}

// ========== SkillObjectStateUnit ==========

void SkillObjectStateUnit::StartState(SkillObjectTargetList* target_list) {
    if (!target_list) return;

    target_list->SetPositionHead();
    STList* target;
    while ((target = target_list->GetNextTargetList()) != nullptr) {
        if (target->object) {
            DoStartState(target->object, nullptr);
        }
    }
}

void SkillObjectStateUnit::EndState(SkillObjectTargetList* target_list) {
    if (!target_list) return;

    target_list->SetPositionHead();
    STList* target;
    while ((target = target_list->GetNextTargetList()) != nullptr) {
        if (target->object) {
            DoEndState(target->object);
        }
    }
}

void SkillObjectStateUnit::StartState(GameObject* object, SkillResultKind kind) {
    if (!object) return;
    DoStartState(object, nullptr);
}

void SkillObjectStateUnit::EndState(GameObject* object, SkillResultKind kind) {
    if (!object) return;
    DoEndState(object);
}

// ========== SkillObjectStateUnit_StatusAttach ==========

void SkillObjectStateUnit_StatusAttach::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 从skill_info读取数据
    if (skill_info) {
        state_id_ = skill_info->GetStateIDTarget();
        float duration_sec = skill_info->GetDuration(skill_level);
        duration_ = duration_sec;

        spdlog::debug("[SkillObject-StatusAttach] Init: state_id={}, duration={:.1f}s",
                     state_id_, duration_);
    } else {
        state_id_ = 1;
        duration_ = 5.0f;
    }
}

void SkillObjectStateUnit_StatusAttach::DoStartState(GameObject* object, GameObject* skill_operator) {
    if (!object) return;

    SpecialStateInfo state;
    state.state_id = state_id_;
    state.state_type = SpecialStateType::kStun;  // 根据state_id确定类型
    state.duration = duration_;
    state.time_remaining = duration_;
    state.level = 1;

    // 应用状态效果
    SpecialStateManager::ApplyState(object->GetObjectId(), state.state_type,
                                    skill_operator ? skill_operator->GetObjectId() : 0,
                                    0, state_id_,
                                    static_cast<uint32_t>(duration_ * 1000));

    spdlog::debug("StateUnit_StatusAttach: Apply state {} to object {}", state_id_, object->GetObjectId());
}

void SkillObjectStateUnit_StatusAttach::DoEndState(GameObject* object) {
    if (!object) return;

    // 移除状态效果
    SpecialStateManager::RemoveState(object->GetObjectId(), state_id_);

    spdlog::debug("StateUnit_StatusAttach: Remove state {} from object {}", state_id_, object->GetObjectId());
}

// ========== SkillObjectStateUnit_TieUp ==========

void SkillObjectStateUnit_TieUp::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 从skill_info读取数据
    if (skill_info) {
        float duration_sec = skill_info->GetDuration(skill_level);
        duration_ = duration_sec;

        spdlog::debug("[SkillObject-TieUp] Init: duration={:.1f}s", duration_);
    } else {
        duration_ = 3.0f;
    }
}

void SkillObjectStateUnit_TieUp::DoStartState(GameObject* object, GameObject* skill_operator) {
    if (!object) return;

    SpecialStateInfo state;
    state.state_id = 100;
    state.state_type = SpecialStateType::kRoot;
    state.duration = duration_;
    state.time_remaining = duration_;

    // 应用定身状态
    SpecialStateManager::ApplyState(object->GetObjectId(), state.state_type,
                                    skill_operator ? skill_operator->GetObjectId() : 0,
                                    0, 100,
                                    static_cast<uint32_t>(duration_ * 1000));

    spdlog::debug("StateUnit_TieUp: Apply root to object {}", object->GetObjectId());
}

void SkillObjectStateUnit_TieUp::DoEndState(GameObject* object) {
    if (!object) return;

    // 移除定身状态
    SpecialStateManager::RemoveState(object->GetObjectId(), 100);

    spdlog::debug("StateUnit_TieUp: Remove root from object {}", object->GetObjectId());
}

// ========== SkillObjectStateUnit_DummyState ==========

void SkillObjectStateUnit_DummyState::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 空状态
}

void SkillObjectStateUnit_DummyState::DoStartState(GameObject* object, GameObject* skill_operator) {
    // 什么都不做
}

void SkillObjectStateUnit_DummyState::DoEndState(GameObject* object) {
    // 什么都不做
}

// ========== SkillObjectStateUnit_AmplifiedPower ==========

void SkillObjectStateUnit_AmplifiedPower::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 从skill_info读取数据
    if (skill_info) {
        float damage_rate = skill_info->GetDamageRate(skill_level);
        power_rate_ = 1.0f + damage_rate;  // 伤害倍率转换为加成比例

        spdlog::debug("[SkillObject-AmplifiedPower] Init: power_rate={:.2f}", power_rate_);
    } else {
        power_rate_ = 1.5f;  // 50%伤害加成
    }
}

void SkillObjectStateUnit_AmplifiedPower::DoStartState(GameObject* object, GameObject* skill_operator) {
    if (!object) return;

    // 添加伤害加成标记（通过设置属性或状态）
    // 实际实现需要与GameObject的属性系统集成
    spdlog::debug("StateUnit_AmplifiedPower: +{:.0f}% damage to object {}",
                 (power_rate_ - 1.0f) * 100, object->GetObjectId());
}

void SkillObjectStateUnit_AmplifiedPower::DoEndState(GameObject* object) {
    if (!object) return;

    // 移除伤害加成标记（恢复原始属性）
    // 实际实现需要与GameObject的属性系统集成
    spdlog::debug("StateUnit_AmplifiedPower: Remove bonus from object {}", object->GetObjectId());
}

// ========== SkillObjectAttachUnit Implementations ==========

void SkillObjectAttachUnit_AttackUp::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 从skill_info读取数据
    if (skill_info) {
        float damage_rate = skill_info->GetDamageRate(skill_level);
        phy_attack_up_ = damage_rate * 0.5f;  // 攻击提升为伤害倍率的50%

        spdlog::debug("[SkillObject-AttackUp] Init: phy_attack_up={:.2f}", phy_attack_up_);
    } else {
        phy_attack_up_ = 0.2f;  // +20%物理攻击
    }
}

void SkillObjectAttachUnit_AttackDown::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 从skill_info读取数据
    if (skill_info) {
        float damage_rate = skill_info->GetDamageRate(skill_level);
        attack_attack_down_ = -damage_rate * 0.3f;  // 攻击降低为伤害倍率的30%
        phy_attack_down_ = -damage_rate * 0.3f;

        spdlog::debug("[SkillObject-AttackDown] Init: attack_down={:.2f}", attack_attack_down_);
    } else {
        attack_attack_down_ = -0.15f;  // -15%攻击
        phy_attack_down_ = -0.15f;
    }
}

void SkillObjectAttachUnit_Defence::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 从skill_info读取数据
    if (skill_info) {
        float heal_rate = skill_info->GetHealRate(skill_level);
        defence_up_ = heal_rate * 0.6f;  // 防御提升为治疗倍率的60%

        spdlog::debug("[SkillObject-Defence] Init: defence_up={:.2f}", defence_up_);
    } else {
        defence_up_ = 0.3f;  // +30%防御
    }
}

void SkillObjectAttachUnit_MoveSpeed::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 从skill_info读取数据
    if (skill_info) {
        float range = skill_info->GetRange(skill_level);
        move_speed_rate_ = 1.0f + (range / 100.0f);  // 移动速度基于范围
        if (move_speed_rate_ > 2.0f) move_speed_rate_ = 2.0f;  // 最大200%

        spdlog::debug("[SkillObject-MoveSpeed] Init: move_speed_rate={:.2f}", move_speed_rate_);
    } else {
        move_speed_rate_ = 1.2f;  // +20%移动速度
    }
}

void SkillObjectAttachUnit_Vampiric::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 从skill_info读取数据
    if (skill_info) {
        float damage_rate = skill_info->GetDamageRate(skill_level);
        vampiric_rate_ = damage_rate * 0.15f;  // 生命偷取为伤害倍率的15%
        if (vampiric_rate_ > 0.5f) vampiric_rate_ = 0.5f;  // 最大50%

        spdlog::debug("[SkillObject-Vampiric] Init: vampiric_rate={:.2f}", vampiric_rate_);
    } else {
        vampiric_rate_ = 0.1f;  // 10%生命偷取
    }
}

void SkillObjectAttachUnit_Immune::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 免疫状态，无需额外参数
}

void SkillObjectAttachUnit_CounterAttack::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 从skill_info读取数据
    if (skill_info) {
        float damage_rate = skill_info->GetDamageRate(skill_level);
        counter_rate_ = damage_rate * 0.4f;  // 反击率为伤害倍率的40%
        if (counter_rate_ > 0.8f) counter_rate_ = 0.8f;  // 最大80%

        spdlog::debug("[SkillObject-CounterAttack] Init: counter_rate={:.2f}", counter_rate_);
    } else {
        counter_rate_ = 0.3f;  // 30%反击概率
    }
}

void SkillObjectAttachUnit_MaxLife::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 从skill_info读取数据
    if (skill_info) {
        float heal_rate = skill_info->GetHealRate(skill_level);
        max_life_up_ = heal_rate * 0.4f;  // 最大生命提升为治疗倍率的40%
        if (max_life_up_ > 0.5f) max_life_up_ = 0.5f;  // 最大50%

        spdlog::debug("[SkillObject-MaxLife] Init: max_life_up={:.2f}", max_life_up_);
    } else {
        max_life_up_ = 0.25f;  // +25%最大生命
    }
}

void SkillObjectAttachUnit_Dodge1::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 从skill_info读取数据
    if (skill_info) {
        float range = skill_info->GetRange(skill_level);
        dodge_rate_ = range / 500.0f;  // 闪避率基于范围
        if (dodge_rate_ > 0.6f) dodge_rate_ = 0.6f;  // 最大60%

        spdlog::debug("[SkillObject-Dodge] Init: dodge_rate={:.2f}", dodge_rate_);
    } else {
        dodge_rate_ = 0.2f;  // 20%闪避率
    }
}

void SkillObjectAttachUnit_SkipEffect::Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper) {
    // 跳过特效，无需额外参数
}

// ========== SkillObject ==========

SkillObject::SkillObject(const SkillInfo* skill_info,
                        std::unique_ptr<SkillObjectTerminator> terminator,
                        std::unique_ptr<SkillObjectTargetList> target_list,
                        std::unique_ptr<SkillObjectFirstUnit> first_unit)
    : skill_info_(skill_info)
    , terminator_(std::move(terminator))
    , target_list_(std::move(target_list))
    , first_unit_(std::move(first_unit))
    , positive_target_type_(0)
    , negative_target_type_(0)
    , operator_id_(0)
    , die_flag_(false)
    , max_life_(0)
    , cur_life_(0)
    , skill_tree_amp_(1.0f)
    , option_index_(0)
    , release_kind(0) {

    if (target_list_) {
        target_list_->SetSkillObject(this);
    }
}

SkillObject::~SkillObject() {
    Release();
}

void SkillObject::Init(const SkillObjectInfo* info,
                      const std::vector<GameObject*>& initial_targets,
                      float skill_tree_amp,
                      GameObject* main_target) {
    if (!info) return;

    skill_object_info_ = *info;
    skill_tree_amp_ = skill_tree_amp;

    // 初始化目标列表
    if (target_list_) {
        target_list_->InitTargetList(initial_targets, main_target, info->main_target);
    }

    // 执行首次效果
    if (first_unit_) {
        first_unit_->ExecuteFirstUnit(GetOperator(), target_list_.get(), skill_tree_amp_);
        SkillObjectFirstUnitResult();
    }

    // 启动状态单元
    for (auto& state_unit : state_unit_list_) {
        if (state_unit) {
            state_unit->StartState(target_list_.get());
        }
    }

    spdlog::debug("SkillObject[{}] initialized: Skill={}, Level={}, Operator={}",
                 skill_object_info_.skill_object_idx,
                 skill_object_info_.skill_idx,
                 skill_object_info_.skill_level,
                 skill_object_info_.operator_id);
}

void SkillObject::Release() {
    // 结束所有状态单元
    for (auto& state_unit : state_unit_list_) {
        if (state_unit && target_list_) {
            state_unit->EndState(target_list_.get());
        }
    }

    // 释放目标列表
    if (target_list_) {
        target_list_->Release();
    }

    single_unit_list_.clear();
    state_unit_list_.clear();
    attach_unit_list_.clear();

    spdlog::debug("SkillObject[{}] released", skill_object_info_.skill_object_idx);
}

uint32_t SkillObject::Update() {
    if (die_flag_) {
        return SO_DESTROYOBJECT;
    }

    // 检查终止条件
    if (terminator_ && terminator_->CheckTerminate(skill_object_info_)) {
        return SO_DESTROYOBJECT;
    }

    // 更新目标列表
    if (target_list_ && GetOperator()) {
        target_list_->UpdateTargetList(GetOperator());
    }

    // 更新周期性单元
    for (auto& single_unit : single_unit_list_) {
        if (single_unit) {
            single_unit->Update(&skill_object_info_, target_list_.get(), skill_tree_amp_);
        }
    }

    return SO_OK;
}

void SkillObject::UpdateTargetList(GameObject* object) {
    if (target_list_) {
        target_list_->UpdateTargetList(object);
    }
}

void SkillObject::AddTargetObject(GameObject* object) {
    if (target_list_ && object) {
        // 判断是正面还是负面目标（根据技能类型）
        bool is_negative_skill = skill_info_ && skill_info_->GetBaseDamage(1) > 0;
        SkillResultKind kind = is_negative_skill ? SkillResultKind::Negative : SkillResultKind::Positive;
        target_list_->AddTargetObject(object, kind);
    }
}

void SkillObject::RemoveTargetObject(uint64_t object_id) {
    if (target_list_) {
        target_list_->RemoveTargetObject(object_id);
    }
}

void SkillObject::SetAddMsg(char* add_msg, uint16_t* msg_len, uint64_t receiver_id, bool is_login) {
    // 构建添加消息（需要协议定义）
    // 此处留空，等待网络协议实现
    if (msg_len) *msg_len = 0;
}

void SkillObject::SetRemoveMsg(char* remove_msg, uint16_t* msg_len, uint64_t receiver_id) {
    // 构建移除消息（需要协议定义）
    // 此处留空，等待网络协议实现
    if (msg_len) *msg_len = 0;
}

void SkillObject::GetPosition(float* x, float* y, float* z) const {
    if (x) *x = skill_object_info_.position.x;
    if (y) *y = skill_object_info_.position.y;
    if (z) *z = skill_object_info_.position.z;
}

bool SkillObject::IsNegativeTarget(GameObject* object) const {
    if (!object || !target_list_) return false;
    return target_list_->GetTargetKind(object->GetObjectId()) == SkillResultKind::Negative;
}

bool SkillObject::IsPositiveTarget(GameObject* object) const {
    if (!object || !target_list_) return false;
    return target_list_->GetTargetKind(object->GetObjectId()) == SkillResultKind::Positive;
}

bool SkillObject::TargetTypeCheck(uint16_t target_type, GameObject* object) const {
    // 根据target_type检查目标类型
    if (!object) return false;

    // 简化实现：目前总是返回true
    // 实际应该检查target_type与object的类型是否匹配
    // 例如：target_type=0表示敌人，target_type=1表示友军
    return true;
}

GameObject* SkillObject::GetOperator() {
    // 从GameObjectManager获取operator_id_对应的对象
    if (operator_id_ == 0) return nullptr;
    return GameObjectManager::GetObjectById(operator_id_);
}

void SkillObject::SkillObjectFirstUnitResult() {
    if (first_unit_) {
        first_unit_->FirstUnitResult();
    }
}

void SkillObject::DoDie(GameObject* attacker) {
    die_flag_ = true;
    spdlog::debug("SkillObject[{}] died", skill_object_info_.skill_object_idx);
}

uint32_t SkillObject::GetLife() {
    return cur_life_;
}

void SkillObject::SetLife(uint32_t life, bool send_msg) {
    cur_life_ = life;
    if (cur_life_ == 0) {
        DoDie(nullptr);
    }
}

uint32_t SkillObject::DoGetMaxLife() {
    return max_life_;
}

void SkillObject::AddSingleUnit(std::unique_ptr<SkillObjectSingleUnit> unit) {
    if (unit) {
        unit->SetSkillObject(this);
        single_unit_list_.push_back(std::move(unit));
    }
}

void SkillObject::AddStateUnit(std::unique_ptr<SkillObjectStateUnit> unit) {
    if (unit) {
        unit->SetSkillObject(this);
        state_unit_list_.push_back(std::move(unit));
    }
}

void SkillObject::AddAttachUnit(std::unique_ptr<SkillObjectAttachUnit> unit) {
    if (unit) {
        attach_unit_list_.push_back(std::move(unit));
    }
}

bool SkillObject::IsSame(SkillObject* other) {
    if (!other) return false;
    return skill_object_info_.skill_idx == other->skill_object_info_.skill_idx;
}

// ========== SkillObjectFactory ==========

std::unique_ptr<SkillObject> SkillObjectFactory::MakeNewSkillObject(const SkillInfo* skill_info) {
    // TODO: 根据skill_info的类型创建相应的SkillObject子类
    // 这里需要实现工厂模式来创建不同类型的技能对象

    if (!skill_info) {
        spdlog::error("SkillObjectFactory: skill_info is null");
        return nullptr;
    }

    // 示例：创建一个基础的SkillObject
    auto terminator = std::make_unique<SkillObjectTerminator_Time>();
    auto target_list = std::make_unique<SkillObjectTargetList_Circle>();
    auto first_unit = std::make_unique<SkillObjectFirstUnit_Attack>();

    auto skill_object = std::make_unique<SkillObject>(
        skill_info,
        std::move(terminator),
        std::move(target_list),
        std::move(first_unit)
    );

    spdlog::info("SkillObjectFactory: Created skill object for skill {}", skill_info->GetSkillId());

    return skill_object;
}

// ========== Helper Function Implementation ==========

uint32_t GetCurrentTimeMs() {
    // 实现获取当前毫秒级时间戳的函数
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return static_cast<uint32_t>(millis);
}

} // namespace Game
} // namespace Murim
