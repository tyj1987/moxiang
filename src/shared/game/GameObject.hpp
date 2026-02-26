// MurimServer - Game Object Base Class
// 游戏对象基类 - 所有游戏对象的基类
// 对应legacy: GameObject及相关派生类

#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <memory>
#include <vector>

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class GameObject;

// ========== Object Type ==========
// 对象类型枚举
enum class ObjectType : uint8_t {
    Unknown = 0,        // 未知
    Player = 1,         // 玩家
    Monster = 2,        // 怪物
    NPC = 3,            // NPC
    Pet = 4,            // 宠物
    Item = 5,           // 物品(地面掉落)
    Door = 6,           // 门
    Trigger = 7,        // 触发器
    Vehicle = 8,        // 载具
    Titan = 9,          // 泰坦
    Mount = 10          // 坐骑
};

// ========== Object State ==========
// 对象状态
enum class ObjectState : uint8_t {
    None = 0,           // 无状态
    Idle = 1,           // 空闲
    Moving = 2,         // 移动中
    Attacking = 3,      // 攻击中
    Casting = 4,        // 施法中
    Dead = 5,           // 死亡
    Dying = 6,          // 濒死
    Stunned = 7,        // 眩晕
    Sleeping = 8,       // 睡眠
    Frozen = 9,         // 冰冻
    Hidden = 10         // 隐身
};

// ========== GameObject Base Class ==========
// 游戏对象基类
class GameObject {
protected:
    // 基础信息
    uint64_t object_id_;              // 对象唯一ID
    ObjectType object_type_;          // 对象类型
    std::string object_name_;         // 对象名称

    // 位置信息
    struct {
        float x, y, z;
    } position_;

    // 旋转信息
    float rotation_;                  // 旋转角度(度)

    // 状态信息
    ObjectState state_;               // 当前状态
    uint32_t state_start_time_;       // 状态开始时间

    // 生命值
    uint32_t max_hp_;                 // 最大生命值
    uint32_t current_hp_;             // 当前生命值

    // 魔法值
    uint32_t max_mp_;                 // 最大魔法值
    uint32_t current_mp_;             // 当前魔法值

    // 创建时间
    uint64_t create_time_;            // 创建时间

    // 是否有效
    bool is_active_;                  // 是否活跃
    bool is_visible_;                 // 是否可见

    // ========== 战斗属性 (新增) ==========
    // 攻击属性
    uint32_t attack_power_;           // 攻击力
    uint32_t magic_power_;            // 魔法攻击力
    uint32_t heal_power_;             // 治疗强度
    float critical_rate_;             // 暴击率(0-100)
    float critical_damage_;           // 暴击伤害倍率
    float hit_rate_;                  // 命中率(0-100)
    float armor_penetration_;         // 护甲穿透(0-100)
    float magic_penetration_;         // 魔法穿透(0-100)

    // 防御属性
    uint32_t armour_;                 // 护甲(物理防御)
    uint32_t magic_resist_;           // 魔法抗性
    float dodge_rate_;                // 闪避率(0-100)
    float block_rate_;                // 格挡率(0-100)
    float block_reduction_;           // 格挡减伤比例
    float damage_taken_multiplier_;   // 承受伤害倍率
    float physical_reduction_;        // 物理伤害减免
    float magic_reduction_;           // 魔法伤害减免

    // 角色属性
    uint16_t level_;                  // 等级

public:
    GameObject();
    virtual ~GameObject();

    // ========== 初始化 ==========

    // 初始化对象
    virtual bool Init(
        uint64_t object_id,
        ObjectType type,
        const std::string& name,
        float x, float y, float z
    );

    // 释放对象
    virtual void Release();

    // ========== 更新 ==========

    // 每帧更新
    virtual void Update(uint32_t delta_time_ms);

    // ========== 信息访问 ==========

    // 对象ID
    uint64_t GetObjectId() const { return object_id_; }
    void SetObjectId(uint64_t id) { object_id_ = id; }

    // 对象类型
    ObjectType GetObjectType() const { return object_type_; }
    void SetObjectType(ObjectType type) { object_type_ = type; }

    // 对象名称
    const std::string& GetObjectName() const { return object_name_; }
    void SetObjectName(const std::string& name) { object_name_ = name; }

    // ========== 位置管理 ==========

    // 获取位置
    void GetPosition(float* x, float* y, float* z) const {
        if (x) *x = position_.x;
        if (y) *y = position_.y;
        if (z) *z = position_.z;
    }

    float GetPositionX() const { return position_.x; }
    float GetPositionY() const { return position_.y; }
    float GetPositionZ() const { return position_.z; }

    // 设置位置
    void SetPosition(float x, float y, float z) {
        position_.x = x;
        position_.y = y;
        position_.z = z;
    }

    void SetPositionX(float x) { position_.x = x; }
    void SetPositionY(float y) { position_.y = y; }
    void SetPositionZ(float z) { position_.z = z; }

    // 旋转
    float GetRotation() const { return rotation_; }
    void SetRotation(float rotation) { rotation_ = rotation; }

    // ========== 状态管理 ==========

    ObjectState GetState() const { return state_; }
    void SetState(ObjectState state);

    uint32_t GetStateDuration() const;
    bool IsInState(ObjectState state) const { return state_ == state; }

    // ========== 生命值管理 ==========

    uint32_t GetMaxHP() const { return max_hp_; }
    void SetMaxHP(uint32_t max_hp) { max_hp_ = max_hp; }

    uint32_t GetCurrentHP() const { return current_hp_; }
    void SetCurrentHP(uint32_t hp) { current_hp_ = hp; }

    float GetHPPercent() const;
    bool IsAlive() const { return current_hp_ > 0; }
    bool IsDead() const { return current_hp_ == 0; }

    // 增加生命值
    virtual void AddHP(uint32_t amount);

    // 减少生命值(伤害)
    virtual void RemoveHP(uint32_t amount);

    // ========== 魔法值管理 ==========

    uint32_t GetMaxMP() const { return max_mp_; }
    void SetMaxMP(uint32_t max_mp) { max_mp_ = max_mp; }

    uint32_t GetCurrentMP() const { return current_mp_; }
    void SetCurrentMP(uint32_t mp) { current_mp_ = mp; }

    float GetMPPercent() const;

    // 增加魔法值
    virtual void AddMP(uint32_t amount);

    // 减少魔法值(消耗)
    virtual void RemoveMP(uint32_t amount);

    // ========== 活跃状态 ==========

    bool IsActive() const { return is_active_; }
    void SetActive(bool active) { is_active_ = active; }

    bool IsVisible() const { return is_visible_; }
    void SetVisible(bool visible) { is_visible_ = visible; }

    // ========== 战斗属性管理 (新增) ==========

    // 攻击属性
    uint32_t GetAttackPower() const { return attack_power_; }
    void SetAttackPower(uint32_t power) { attack_power_ = power; }

    uint32_t GetMagicPower() const { return magic_power_; }
    void SetMagicPower(uint32_t power) { magic_power_ = power; }

    uint32_t GetHealPower() const { return heal_power_; }
    void SetHealPower(uint32_t power) { heal_power_ = power; }

    float GetCriticalRate() const { return critical_rate_; }
    void SetCriticalRate(float rate) { critical_rate_ = rate; }

    float GetCriticalDamage() const { return critical_damage_; }
    void SetCriticalDamage(float damage) { critical_damage_ = damage; }

    float GetHitRate() const { return hit_rate_; }
    void SetHitRate(float rate) { hit_rate_ = rate; }

    float GetArmorPenetration() const { return armor_penetration_; }
    void SetArmorPenetration(float penetration) { armor_penetration_ = penetration; }

    float GetMagicPenetration() const { return magic_penetration_; }
    void SetMagicPenetration(float penetration) { magic_penetration_ = penetration; }

    // 防御属性
    uint32_t GetArmour() const { return armour_; }
    void SetArmour(uint32_t armour) { armour_ = armour; }

    uint32_t GetMagicResist() const { return magic_resist_; }
    void SetMagicResist(uint32_t resist) { magic_resist_ = resist; }

    float GetDodgeRate() const { return dodge_rate_; }
    void SetDodgeRate(float rate) { dodge_rate_ = rate; }

    float GetBlockRate() const { return block_rate_; }
    void SetBlockRate(float rate) { block_rate_ = rate; }

    float GetBlockReduction() const { return block_reduction_; }
    void SetBlockReduction(float reduction) { block_reduction_ = reduction; }

    float GetDamageTakenMultiplier() const { return damage_taken_multiplier_; }
    void SetDamageTakenMultiplier(float multiplier) { damage_taken_multiplier_ = multiplier; }

    float GetPhysicalReduction() const { return physical_reduction_; }
    void SetPhysicalReduction(float reduction) { physical_reduction_ = reduction; }

    float GetMagicReduction() const { return magic_reduction_; }
    void SetMagicReduction(float reduction) { magic_reduction_ = reduction; }

    // 角色属性
    uint16_t GetLevel() const { return level_; }
    void SetLevel(uint16_t level) { level_ = level; }

    // ========== 属性计算（含装备、Buff加成） ==========

    /**
     * @brief 计算总攻击力（基础 + 装备 + Buff）
     * @return 总攻击力
     */
    virtual int32_t CalculateTotalAttackPower() const;

    /**
     * @brief 计算总魔法攻击力（基础 + 装备 + Buff）
     * @return 总魔法攻击力
     */
    virtual int32_t CalculateTotalMagicPower() const;

    /**
     * @brief 计算总防御力（基础 + 装备 + Buff）
     * @return 总防御力
     */
    virtual int32_t CalculateTotalDefense() const;

    /**
     * @brief 计算总魔法抗性（基础 + 装备 + Buff）
     * @return 总魔法抗性
     */
    virtual int32_t CalculateTotalMagicResist() const;

    /**
     * @brief 计算最大生命值（基础 + 装备 + Buff）
     * @return 最大生命值
     */
    virtual int32_t CalculateTotalMaxHP() const;

    /**
     * @brief 计算最大魔法值（基础 + 装备 + Buff）
     * @return 最大魔法值
     */
    virtual int32_t CalculateTotalMaxMP() const;

    /**
     * @brief 计算总暴击率（基础 + 装备 + Buff）
     * @return 总暴击率(0-100)
     */
    virtual float CalculateTotalCriticalRate() const;

    /**
     * @brief 计算总闪避率（基础 + 装备 + Buff）
     * @return 总闪避率(0-100)
     */
    virtual float CalculateTotalDodgeRate() const;

    // ========== 距离计算 ==========

    // 计算与另一个对象的距离
    float GetDistanceTo(const GameObject* other) const;

    // 计算到指定点的距离
    float GetDistanceTo(float x, float y, float z) const;

    // 检查是否在指定范围内
    bool IsInRange(float x, float y, float z, float range) const;
    bool IsInRange(const GameObject* other, float range) const;

    // ========== 虚函数接口 ==========

    // 状态应用回调
    virtual void OnStateApplied(uint32_t state_id);
    virtual void OnStateRemoved(uint32_t state_id);

    // 死亡回调
    virtual void OnDie();

    // 复活回调
    virtual void OnRevive();

    // 受伤回调
    virtual void OnDamaged(uint32_t damage, uint64_t attacker_id);

    // 治疗回调
    virtual void OnHealed(uint32_t amount, uint64_t healer_id);

    // ========== 类型检查 ==========

    virtual bool IsPlayer() const { return false; }
    virtual bool IsMonster() const { return false; }
    virtual bool IsNPC() const { return false; }
    virtual bool IsPet() const { return false; }

    // ========== 调试 ==========

    virtual std::string ToString() const;
};

// ========== Forward Declarations ==========
// GameObjectManager 完整定义在 GameObjectManager.hpp

} // namespace Game
} // namespace Murim
