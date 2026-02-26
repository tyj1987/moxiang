// MurimServer - Game Object Implementation
// 游戏对象基类实现

#include "GameObject.hpp"
#include "../utils/TimeUtils.hpp"
#include "core/spdlog_wrapper.hpp"
#include <cmath>
#include <sstream>

namespace Murim {
namespace Game {

// ========== GameObject ==========

GameObject::GameObject()
    : object_id_(0),
      object_type_(ObjectType::Unknown),
      object_name_(""),
      position_{0.0f, 0.0f, 0.0f},
      rotation_(0.0f),
      state_(ObjectState::None),
      state_start_time_(0),
      max_hp_(100),
      current_hp_(100),
      max_mp_(100),
      current_mp_(100),
      create_time_(0),
      is_active_(true),
      is_visible_(true),
      // 战斗属性初始化
      attack_power_(100),
      magic_power_(100),
      heal_power_(100),
      critical_rate_(5.0f),
      critical_damage_(1.5f),
      hit_rate_(95.0f),
      armor_penetration_(0.0f),
      magic_penetration_(0.0f),
      armour_(0),
      magic_resist_(0),
      dodge_rate_(5.0f),
      block_rate_(0.0f),
      block_reduction_(0.5f),
      damage_taken_multiplier_(1.0f),
      physical_reduction_(0.0f),
      magic_reduction_(0.0f),
      level_(1) {
}

GameObject::~GameObject() {
    Release();
}

bool GameObject::Init(
    uint64_t object_id,
    ObjectType type,
    const std::string& name,
    float x, float y, float z) {

    object_id_ = object_id;
    object_type_ = type;
    object_name_ = name;
    position_.x = x;
    position_.y = y;
    position_.z = z;
    rotation_ = 0.0f;
    state_ = ObjectState::Idle;
    state_start_time_ = Utils::TimeUtils::GetCurrentTimeMs();
    current_hp_ = max_hp_;
    current_mp_ = max_mp_;
    create_time_ = Utils::TimeUtils::GetCurrentTimeMs();
    is_active_ = true;
    is_visible_ = true;

    spdlog::debug("[GameObject] Created: id={}, type={}, name={}, pos=({},{},{})",
                object_id_, static_cast<int>(type), name, x, y, z);

    return true;
}

void GameObject::Release() {
    spdlog::debug("[GameObject] Released: id={}, name={}", object_id_, object_name_);
}

void GameObject::Update(uint32_t delta_time_ms) {
    // 基础更新逻辑
    // 子类可以重写此方法
}

void GameObject::SetState(ObjectState state) {
    if (state_ != state) {
        spdlog::debug("[GameObject] State changed: id={}, {} -> {}",
                    object_id_,
                    static_cast<int>(state_),
                    static_cast<int>(state));

        state_ = state;
        state_start_time_ = Utils::TimeUtils::GetCurrentTimeMs();
    }
}

uint32_t GameObject::GetStateDuration() const {
    uint64_t current_time = Utils::TimeUtils::GetCurrentTimeMs();
    return static_cast<uint32_t>(current_time - state_start_time_);
}

float GameObject::GetHPPercent() const {
    if (max_hp_ == 0) return 0.0f;
    return static_cast<float>(current_hp_) / static_cast<float>(max_hp_);
}

void GameObject::AddHP(uint32_t amount) {
    uint32_t old_hp = current_hp_;
    current_hp_ = std::min(current_hp_ + amount, max_hp_);

    if (old_hp != current_hp_) {
        spdlog::debug("[GameObject] HP added: id={}, amount={}, old_hp={}, new_hp={}",
                    object_id_, amount, old_hp, current_hp_);

        // 如果从死亡状态复活
        if (old_hp == 0 && current_hp_ > 0) {
            OnRevive();
        }
    }
}

void GameObject::RemoveHP(uint32_t amount) {
    uint32_t old_hp = current_hp_;

    if (amount >= current_hp_) {
        current_hp_ = 0;
    } else {
        current_hp_ -= amount;
    }

    spdlog::debug("[GameObject] HP removed: id={}, amount={}, old_hp={}, new_hp={}",
                object_id_, amount, old_hp, current_hp_);

    // 如果死亡
    if (old_hp > 0 && current_hp_ == 0) {
        OnDie();
    }
}

float GameObject::GetMPPercent() const {
    if (max_mp_ == 0) return 0.0f;
    return static_cast<float>(current_mp_) / static_cast<float>(max_mp_);
}

void GameObject::AddMP(uint32_t amount) {
    uint32_t old_mp = current_mp_;
    current_mp_ = std::min(current_mp_ + amount, max_mp_);

    spdlog::trace("[GameObject] MP added: id={}, amount={}, old_mp={}, new_mp={}",
                object_id_, amount, old_mp, current_mp_);
}

void GameObject::RemoveMP(uint32_t amount) {
    uint32_t old_mp = current_mp_;

    if (amount >= current_mp_) {
        current_mp_ = 0;
    } else {
        current_mp_ -= amount;
    }

    spdlog::trace("[GameObject] MP removed: id={}, amount={}, old_mp={}, new_mp={}",
                object_id_, amount, old_mp, current_mp_);
}

float GameObject::GetDistanceTo(const GameObject* other) const {
    if (!other) return 0.0f;
    return GetDistanceTo(other->position_.x, other->position_.y, other->position_.z);
}

float GameObject::GetDistanceTo(float x, float y, float z) const {
    float dx = x - position_.x;
    float dy = y - position_.y;
    float dz = z - position_.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool GameObject::IsInRange(float x, float y, float z, float range) const {
    return GetDistanceTo(x, y, z) <= range;
}

bool GameObject::IsInRange(const GameObject* other, float range) const {
    if (!other) return false;
    return IsInRange(other->position_.x, other->position_.y, other->position_.z, range);
}

void GameObject::OnStateApplied(uint32_t state_id) {
    spdlog::debug("[GameObject] State applied: id={}, state_id={}", object_id_, state_id);
}

void GameObject::OnStateRemoved(uint32_t state_id) {
    spdlog::debug("[GameObject] State removed: id={}, state_id={}", object_id_, state_id);
}

void GameObject::OnDie() {
    spdlog::info("[GameObject] Died: id={}, name={}", object_id_, object_name_);
    SetState(ObjectState::Dead);
}

void GameObject::OnRevive() {
    spdlog::info("[GameObject] Revived: id={}, name={}", object_id_, object_name_);
    SetState(ObjectState::Idle);
}

void GameObject::OnDamaged(uint32_t damage, uint64_t attacker_id) {
    spdlog::debug("[GameObject] Damaged: id={}, damage={}, attacker={}",
                object_id_, damage, attacker_id);
}

void GameObject::OnHealed(uint32_t amount, uint64_t healer_id) {
    spdlog::debug("[GameObject] Healed: id={}, amount={}, healer={}",
                object_id_, amount, healer_id);
}

std::string GameObject::ToString() const {
    std::ostringstream oss;
    oss << "GameObject[id=" << object_id_
        << ", type=" << static_cast<int>(object_type_)
        << ", name=" << object_name_
        << ", pos=(" << position_.x << "," << position_.y << "," << position_.z << ")"
        << ", hp=" << current_hp_ << "/" << max_hp_
        << ", mp=" << current_mp_ << "/" << max_mp_
        << "]";
    return oss.str();
}

// ========== 属性计算实现 ==========

int32_t GameObject::CalculateTotalAttackPower() const {
    // 基础实现：直接返回基础攻击力
    // 子类（如Player）可以重写此方法，添加装备和Buff加成
    return static_cast<int32_t>(attack_power_);
}

int32_t GameObject::CalculateTotalMagicPower() const {
    // 基础实现：直接返回基础魔法攻击力
    // 子类（如Player）可以重写此方法，添加装备和Buff加成
    return static_cast<int32_t>(magic_power_);
}

int32_t GameObject::CalculateTotalDefense() const {
    // 基础实现：直接返回基础护甲
    // 子类（如Player）可以重写此方法，添加装备和Buff加成
    return static_cast<int32_t>(armour_);
}

int32_t GameObject::CalculateTotalMagicResist() const {
    // 基础实现：直接返回基础魔法抗性
    // 子类（如Player）可以重写此方法，添加装备和Buff加成
    return static_cast<int32_t>(magic_resist_);
}

int32_t GameObject::CalculateTotalMaxHP() const {
    // 基础实现：直接返回基础最大HP
    // 子类（如Player）可以重写此方法，添加装备和Buff加成
    return static_cast<int32_t>(max_hp_);
}

int32_t GameObject::CalculateTotalMaxMP() const {
    // 基础实现：直接返回基础最大MP
    // 子类（如Player）可以重写此方法，添加装备和Buff加成
    return static_cast<int32_t>(max_mp_);
}

float GameObject::CalculateTotalCriticalRate() const {
    // 基础实现：直接返回基础暴击率
    // 子类（如Player）可以重写此方法，添加装备和Buff加成
    return critical_rate_;
}

float GameObject::CalculateTotalDodgeRate() const {
    // 基础实现：直接返回基础闪避率
    // 子类（如Player）可以重写此方法，添加装备和Buff加成
    return dodge_rate_;
}

} // namespace Game
} // namespace Murim
