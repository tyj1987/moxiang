#pragma once

#include <memory>
#include <cstdint>
#include <chrono>
#include "shared/ai/AI.hpp"
#include "../entities/Monster.hpp"  // 获取Vector3定义

namespace Murim {

namespace MapServer {
class Monster;
} // namespace MapServer

namespace Game {

class Player;
class MonsterAI;

/**
 * @brief 怪物状态机
 *
 * 管理怪物的AI行为状态转换：
 * - Idle（待机）: 定时索敌
 * - Patrol（巡逻）: 随机移动
 * - Chase（追击）: 追踪目标
 * - Attack（攻击）: 攻击目标
 * - Return（返回）: 返回出生点
 */
class MonsterStateMachine {
public:
    /**
     * @brief 构造函数
     * @param monster 拥有此状态机的怪物
     */
    explicit MonsterStateMachine(MapServer::Monster* monster);

    /**
     * @brief 析构函数
     */
    ~MonsterStateMachine();

    /**
     * @brief 更新状态机
     * @param delta_time 时间增量（毫秒）
     */
    void Update(uint32_t delta_time);

    /**
     * @brief 强制切换状态
     * @param new_state 新状态
     */
    void ChangeState(AIState new_state);

    /**
     * @brief 获取当前状态
     */
    AIState GetCurrentState() const { return current_state_; }

    /**
     * @brief 获取状态持续时间（毫秒）
     */
    uint64_t GetStateDuration() const;

    /**
     * @brief 检查是否可以索敌
     */
    bool CanSearchForTarget() const;

    /**
     * @brief 检查是否需要返回出生点
     */
    bool ShouldReturnToSpawn() const;

private:
    // ========== 状态处理方法 ==========

    /**
     * @brief 处理Idle状态
     */
    void HandleIdle(uint32_t delta_time);

    /**
     * @brief 处理Patrol状态
     */
    void HandlePatrol(uint32_t delta_time);

    /**
     * @brief 处理Chase状态
     */
    void HandleChase(uint32_t delta_time);

    /**
     * @brief 处理Attack状态
     */
    void HandleAttack(uint32_t delta_time);

    /**
     * @brief 处理Return状态
     */
    void HandleReturn(uint32_t delta_time);

    // ========== 辅助方法 ==========

    /**
     * @brief 尝试索敌
     * @return 如果找到目标返回true
     */
    bool TryFindTarget();

    /**
     * @brief 选择随机巡逻点
     */
    void SelectRandomPatrolPoint();

    /**
     * @brief 检查目标是否有效
     */
    bool IsTargetValid() const;

    /**
     * @brief 检查目标是否在攻击范围
     */
    bool IsTargetInAttackRange() const;

    /**
     * @brief 检查是否超出活动范围
     */
    bool IsOutsideDomain() const;

    /**
     * @brief 检查是否到达出生点
     */
    bool IsAtSpawnPoint() const;

    /**
     * @brief 清除目标
     */
    void ClearTarget();

    // ========== 成员变量 ==========

    MapServer::Monster* monster_;           // 拥有此状态机的怪物
    std::shared_ptr<Player> chase_target_;  // 当前追击目标
    AIState current_state_;                 // 当前状态
    std::chrono::system_clock::time_point state_enter_time_;  // 状态进入时间
    uint64_t state_duration_;               // 状态持续时间（毫秒）

    // 巡逻相关
    MapServer::Vector3 patrol_point_;       // 当前巡逻目标点
    bool has_patrol_point_;                 // 是否有巡逻目标点

    // 索敌相关
    uint64_t last_search_time_;             // 上次索敌时间
    static constexpr uint64_t SEARCH_INTERVAL_MS = 1000;  // 索敌间隔（1秒）

    // 攻击相关
    uint64_t last_attack_time_;             // 上次攻击时间
    static constexpr uint64_t ATTACK_INTERVAL_MS = 2000;  // 攻击间隔（2秒）
};

} // namespace Game
} // namespace Murim
