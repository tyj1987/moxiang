#pragma once

#include <memory>
#include <cstdint>
#include <string>
#include <sstream>
#include "shared/character/Character.hpp"
#include "shared/character/MonsterTemplate.hpp"
#include "shared/ai/AI.hpp"

namespace Murim {
namespace Game {

// 前向声明
class Player;
class Entity;
class MonsterAI;      // MonsterAI在Game命名空间
class MonsterStateMachine;  // 状态机

} // namespace Game

namespace MapServer {

/**
 * @brief 向量3结构
 */
struct Vector3 {
    float x, y, z;

    Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    float DistanceTo(const Vector3& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    bool operator==(const Vector3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

/**
 * @brief 怪物实例类
 *
 * 运行时怪物对象，包含：
 * - 模板数据引用
 * - 运行时状态（HP、位置等）
 * - AI系统
 * - 状态机
 */
class Monster {
public:
    /**
     * @brief 构造函数
     * @param entity_id 实体唯一ID
     * @param monster_id 怪物模板ID
     * @param template_data 怪物模板数据
     * @param spawn_point 出生点位置
     */
    Monster(
        uint64_t entity_id,
        uint32_t monster_id,
        const Game::MonsterTemplate& template_data,
        const Vector3& spawn_point
    );

    /**
     * @brief 析构函数
     */
    ~Monster();

    // ==================== 基础信息 ====================

    /**
     * @brief 获取实体ID
     */
    uint64_t GetEntityID() const { return entity_id_; }

    /**
     * @brief 获取怪物ID（模板ID）
     */
    uint32_t GetMonsterID() const { return monster_id_; }

    /**
     * @brief 获取怪物名称
     */
    const std::string& GetName() const { return template_.monster_name; }

    /**
     * @brief 获取怪物模板数据
     */
    const Game::MonsterTemplate& GetTemplate() const { return template_; }

    /**
     * @brief 检查是否有效
     */
    bool IsValid() const { return entity_id_ != 0 && template_.IsValid(); }

    // ==================== 状态管理 ====================

    /**
     * @brief 是否存活
     */
    bool IsAlive() const { return current_hp_ > 0; }

    /**
     * @brief 是否死亡
     */
    bool IsDead() const { return current_hp_ == 0; }

    /**
     * @brief 获取当前HP
     */
    uint32_t GetCurrentHP() const { return current_hp_; }

    /**
     * @brief 设置当前HP
     */
    void SetCurrentHP(uint32_t hp) { current_hp_ = std::min(hp, template_.hp); }

    /**
     * @brief 造成伤害
     * @param damage 伤害值
     * @return 实际造成的伤害
     */
    uint32_t TakeDamage(uint32_t damage) {
        uint32_t actual_damage = std::min(damage, current_hp_);
        current_hp_ -= actual_damage;
        if (current_hp_ == 0) {
            OnDeath();
        }
        return actual_damage;
    }

    /**
     * @brief 治疗
     * @param heal 治疗量
     * @return 实际治疗量
     */
    uint32_t Heal(uint32_t heal) {
        uint32_t old_hp = current_hp_;
        current_hp_ = std::min(current_hp_ + heal, template_.hp);
        return current_hp_ - old_hp;
    }

    // ==================== 位置管理 ====================

    /**
     * @brief 获取当前位置
     */
    const Vector3& GetPosition() const { return position_; }

    /**
     * @brief 设置位置
     */
    void SetPosition(const Vector3& pos) { position_ = pos; }

    /**
     * @brief 获取出生点
     */
    const Vector3& GetSpawnPoint() const { return spawn_point_; }

    /**
     * @brief 计算到出生点的距离
     */
    float GetDistanceToSpawnPoint() const {
        return position_.DistanceTo(spawn_point_);
    }

    /**
     * @brief 计算到目标位置的距离
     */
    float GetDistanceTo(const Vector3& target) const {
        return position_.DistanceTo(target);
    }

    // ==================== AI系统 ====================

    /**
     * @brief 获取AI系统
     */
    Game::MonsterAI& GetAI();

    /**
     * @brief 获取AI系统（const）
     */
    const Game::MonsterAI& GetAI() const;

    // ==================== 状态机 ====================

    /**
     * @brief 获取当前AI状态
     */
    Game::AIState GetCurrentState() const { return current_state_; }

    /**
     * @brief 设置AI状态
     */
    void SetState(Game::AIState new_state);

    /**
     * @brief 更新怪物（每帧调用）
     * @param delta_time 距离上一帧的时间（毫秒）
     */
    void Update(uint32_t delta_time);

    // ==================== 战斗相关 ====================

    /**
     * @brief 攻击目标
     * @param target 目标实体
     * @return 是否攻击成功
     */
    bool Attack(std::shared_ptr<Game::Entity> target);

    /**
     * @brief 攻击目标（Player版本）
     * @param target 目标玩家
     * @return 是否攻击成功
     */
    bool Attack(std::shared_ptr<Game::Player> target);

    /**
     * @brief 计算攻击伤害
     */
    uint16_t CalculateAttackDamage() const {
        return template_.CalculateAttackDamage();
    }

    // ==================== 移动相关 ====================

    /**
     * @brief 移动到目标位置
     * @param target 目标位置
     * @param delta_time 时间增量（毫秒）
     */
    void MoveTo(const Vector3& target, uint32_t delta_time);

    /**
     * @brief 移动到目标实体
     * @param target 目标实体
     * @param delta_time 时间增量（毫秒）
     */
    void MoveToEntity(std::shared_ptr<Game::Entity> target, uint32_t delta_time);

    /**
     * @brief 移动到目标实体（Player版本）
     * @param target 目标玩家
     * @param delta_time 时间增量（毫秒）
     */
    void MoveToEntity(std::shared_ptr<Game::Player> target, uint32_t delta_time);

    /**
     * @brief 返回出生点
     */
    void ReturnToSpawnPoint();

    // ==================== 追击管理 ====================

    /**
     * @brief 开始追击
     */
    void StartChase(std::shared_ptr<Game::Player> target) {
        chase_target_ = target;
        chase_start_time_ = std::chrono::system_clock::now();
        SetState(Game::AIState::kChase);
    }

    /**
     * @brief 停止追击
     */
    void StopChase() {
        chase_target_.reset();
        SetState(Game::AIState::kIdle);
    }

    /**
     * @brief 检查是否应该停止追击
     */
    bool ShouldStopPursuit() const;

    // ==================== 刷怪点管理 ====================

    /**
     * @brief 获取刷怪点ID
     */
    uint64_t GetSpawnPointID() const { return spawn_point_id_; }

    /**
     * @brief 设置刷怪点ID
     */
    void SetSpawnPointID(uint64_t spawn_id) { spawn_point_id_ = spawn_id; }

    /**
     * @brief 获取地图ID
     */
    uint16_t GetMapID() const { return map_id_; }

    /**
     * @brief 设置地图ID
     */
    void SetMapID(uint16_t map_id) { map_id_ = map_id; }

    // ==================== 调试信息 ====================

    /**
     * @brief 获取调试信息
     */
    std::string GetDebugInfo() const;

private:
    /**
     * @brief 死亡处理
     */
    void OnDeath();

    /**
     * @brief 复活处理
     */
    void OnRevive();

    // ==================== 基础字段 ====================
    uint64_t entity_id_;                    // 实体唯一ID
    uint32_t monster_id_;                   // 怪物模板ID
    const Game::MonsterTemplate& template_; // 怪物模板引用
    uint64_t spawn_point_id_;               // 刷怪点ID
    uint16_t map_id_;                       // 地图ID

    // ==================== 运行时状态 ====================
    uint32_t current_hp_;                   // 当前HP
    Vector3 position_;                      // 当前位置
    Vector3 spawn_point_;                   // 出生点

    // ==================== AI系统 ====================
    std::unique_ptr<Game::MonsterAI> ai_;   // AI系统（Pimpl模式）

    // ==================== 状态机 ====================
    std::unique_ptr<Game::MonsterStateMachine> state_machine_;  // AI状态机
    Game::AIState current_state_;           // 当前AI状态（用于快速访问）

    // ==================== 追击信息 ====================
    std::shared_ptr<Game::Player> chase_target_;         // 追击目标
    std::chrono::system_clock::time_point chase_start_time_; // 追击开始时间
};

} // namespace MapServer
} // namespace Murim
