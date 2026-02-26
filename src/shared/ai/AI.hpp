#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <memory>
#include "shared/movement/Movement.hpp"
#include "shared/battle/Battle.hpp"
#include "shared/character/Character.hpp"

namespace Murim {
namespace Game {

/**
 * @brief AI 状态枚举
 */
enum class AIState : uint8_t {
    kNone = 0,
    kIdle = 1,            // 待机
    kPatrol = 2,          // 巡逻
    kChase = 3,           // 追击
    kAttack = 4,          // 攻击
    kReturn = 5,          // 返回
    kFlee = 6,            // 逃跑
    kDead = 7,            // 死亡
    kSpawn = 8,           // 生成
    kDespawn = 9          // 消失
};

/**
 * @brief AI 行为类型
 */
enum class AIBehaviorType : uint8_t {
    kNone = 0,
    kPassive = 1,         // 被动 (不主动攻击)
    kAggressive = 2,      // 主动 (攻击范围内玩家)
    kNeutral = 3,         // 中立 (被攻击后才反击)
    kGuard = 4,           // 守卫 (保护特定区域)
    kBoss = 5             // Boss (特殊AI)
};

/**
 * @brief AI 事件类型 - 对应 legacy eSEVENT_XXX
 */
enum class AIEventType : uint8_t {
    kProcess = 0,         // eSEVENT_Process - 每个tick处理
    kAttack = 1,          // eSEVENT_Attack - 攻击事件
    kDamaged = 2,         // eSEVENT_Damaged - 受伤事件
    kSeeEnemy = 3,        // eSEVENT_SeeEnemy - 发现敌人
    kHear = 4,            // eSEVENT_Hear - 听到声音
    kUserCome = 5,        // eSEVENT_UserCome - 玩家靠近
    kReturn = 6,          // eSEVENT_Return - 返回出生点
    kDead = 7             // eSEVENT_Dead - 死亡事件
};

/**
 * @brief 仇恨数据
 */
struct AggroEntry {
    uint64_t target_id;           // 目标ID
    uint32_t aggro_value;         // 仇恨值
    std::chrono::system_clock::time_point last_update;  // 最后更新时间

    /**
     * @brief 增加仇恨值
     */
    void AddAggro(uint32_t value) {
        aggro_value += value;
        last_update = std::chrono::system_clock::now();
    }

    /**
     * @brief 衰减仇恨值
     */
    void Decay(float decay_rate) {
        aggro_value = static_cast<uint32_t>(aggro_value * decay_rate);
        last_update = std::chrono::system_clock::now();
    }
};

/**
 * @brief AI 配置参数
 */
struct AIConfig {
    // 行为参数
    AIBehaviorType behavior_type = AIBehaviorType::kPassive;
    float detection_range = 10.0f;     // 侦测范围
    float chase_range = 20.0f;          // 追击范围
    float attack_range = 3.0f;          // 攻击范围
    float return_distance = 30.0f;      // 超过此距离返回出生点
    float flee_threshold = 0.2f;        // 逃跑血量阈值

    // 时间参数
    uint32_t patrol_wait_time = 3000;   // 巡逻点停留时间(毫秒)
    uint32_t attack_cooldown = 2000;    // 攻击冷却(毫秒)
    uint32_t aggro_decay_time = 5000;   // 仇恨衰减时间(毫秒)
    float aggro_decay_rate = 0.95f;     // 仇恨衰减率

    // 巡逻参数
    std::vector<Position> patrol_points; // 巡逻点列表
    bool loop_patrol = true;             // 是否循环巡逻

    // 召唤参数
    bool can_call_for_help = true;      // 是否可以呼叫支援
    float help_call_range = 30.0f;      // 呼叫支援范围
    uint32_t help_call_cooldown = 30000; // 呼叫支援冷却(毫秒)
};

/**
 * @brief AI 巡航点
 */
struct PatrolPoint {
    Position position;
    uint32_t wait_time;              // 停留时间(毫秒)
    uint8_t action_type;             // 到达后动作类型 (0=无, 1=说话, 2=使用技能)

    /**
     * @brief 计算到另一点的距离
     */
    float DistanceTo(const PatrolPoint& other) const {
        return position.Distance2D(other.position);
    }
};

/**
 * @brief AI 状态机
 */
class AIStateMachine {
public:
    AIStateMachine(uint64_t owner_id, const AIConfig& config);

    /**
     * @brief 更新 AI (每帧调用)
     */
    void Update(std::chrono::milliseconds delta_time);

    /**
     * @brief 处理AI事件 - 对应 legacy GSTATEMACHINE.Process()
     *
     * Legacy 逻辑:
     * GSTATEMACHINE.Process(obj, eSEVENT_Process, NULL);
     * GSTATEMACHINE.Process(obj, eSEVENT_Attack, pTarget);
     * 等
     */
    void Process(AIEventType event_type, void* event_data = nullptr);

    /**
     * @brief 切换状态
     */
    void ChangeState(AIState new_state);

    /**
     * @brief 获取当前状态
     */
    AIState GetCurrentState() const { return current_state_; }

    /**
     * @brief 获取配置
     */
    const AIConfig& GetConfig() const { return config_; }

    /**
     * @brief 设置当前位置
     */
    void SetCurrentPosition(const Position& pos) { current_position_ = pos; }

    /**
     * @brief 设置出生位置
     */
    void SetSpawnPosition(const Position& pos) { spawn_position_ = pos; }

    // ========== 仇恨系统 ==========

    /**
     * @brief 添加仇恨
     */
    void AddAggro(uint64_t target_id, uint32_t aggro_value);

    /**
     * @brief 移除仇恨
     */
    void RemoveAggro(uint64_t target_id);

    /**
     * @brief 获取最高仇恨目标
     */
    std::optional<uint64_t> GetTopAggroTarget();

    /**
     * @brief 清除所有仇恨
     */
    void ClearAggro();

    /**
     * @brief 衰减所有仇恨
     */
    void DecayAllAggro();

    // ========== 事件处理 ==========

    /**
     * @brief 受到攻击
     */
    void OnAttacked(uint64_t attacker_id, uint32_t damage);

    /**
     * @brief 目标进入范围
     */
    void OnTargetEnterRange(uint64_t target_id);

    /**
     * @brief 目标离开范围
     */
    void OnTargetLeaveRange(uint64_t target_id);

    // ========== 辅助方法 ==========

    /**
     * @brief 获取与目标的距离
     */
    std::optional<float> GetDistanceToTarget(uint64_t target_id);

    /**
     * @brief 检查目标是否在攻击范围
     */
    bool IsTargetInAttackRange(uint64_t target_id);

    /**
     * @brief 检查目标是否在侦测范围
     */
    bool IsTargetInDetectionRange(uint64_t target_id);

private:
    /**
     * @brief 执行状态逻辑
     */
    void ExecuteIdleState();
    void ExecutePatrolState();
    void ExecuteChaseState();
    void ExecuteAttackState();
    void ExecuteReturnState();
    void ExecuteFleeState();
    void ExecuteDeadState();

    /**
     * @brief 检查是否应该切换状态
     */
    void CheckStateTransitions();

    /**
     * @brief 移动到目标
     */
    void MoveTo(const Position& target_pos);

    /**
     * @brief 攻击目标
     */
    void AttackTarget(uint64_t target_id);

    /**
     * @brief 呼叫支援
     */
    void CallForHelp();

private:
    uint64_t owner_id_;
    AIConfig config_;
    AIState current_state_;
    AIState previous_state_;

    // 仇恨列表
    std::unordered_map<uint64_t, AggroEntry> aggro_list_;

    // 巡逻相关
    size_t current_patrol_index_;
    std::chrono::system_clock::time_point patrol_wait_start_;

    // 战斗相关
    std::chrono::system_clock::time_point last_attack_time_;
    uint64_t current_target_id_;

    // 位置相关
    Position current_position_;
    Position spawn_position_;
    bool is_returning_;

    // 状态计时器
    std::chrono::system_clock::time_point state_enter_time_;
};

/**
 * @brief AI 管理器
 *
 * 负责管理所有怪物的 AI
 * 对应 legacy: AISystem.cpp, AIManager.cpp
 */
class AIManager {
public:
    /**
     * @brief 获取单例实例
     */
    static AIManager& Instance();

    /**
     * @brief 初始化 AI 管理器
     */
    void Initialize();

    /**
     * @brief 处理所有 AI (每帧调用) - 严格遵循 legacy AISystem::Process()
     *
     * Legacy 逻辑:
     * 1. 遍历所有怪物,对每个调用 ConstantProcess() 和 PeriodicProcess()
     * 2. 处理消息路由器
     * 3. 每3秒检查一次刷新 (gCurTime - dwRegenCheckTime >= 3000)
     */
    void Process(uint32_t current_time);

    /**
     * @brief 持续处理 - 对应 legacy ConstantProcess()
     *
     * Legacy 逻辑:
     * - Boss怪物跳过 (GetObjectKind() == eObjectKind_BossMonster)
     * - 普通怪物调用状态机 GSTATEMACHINE.Process(obj, eSEVENT_Process, NULL)
     */
    void ConstantProcess(class Monster* monster);

    /**
     * @brief 周期性处理 - 对应 legacy PeriodicProcess()
     *
     * Legacy 逻辑: 空实现
     */
    void PeriodicProcess(class Monster* monster);

    /**
     * @brief 处理刷新 - 对应 legacy GROUPMGR->RegenProcess()
     */
    void ProcessRegen();

    /**
     * @brief 添加怪物对象 - 对应 legacy CAISystem::AddObject()
     *
     * Legacy 逻辑 (第88-105行):
     * - 添加到 m_AISubordinatedObject
     * - 注意: 组处理在加载时完成
     */
    void AddObject(std::shared_ptr<class Monster> monster);

    /**
     * @brief 移除怪物对象 - 对应 legacy CAISystem::RemoveObject()
     *
     * Legacy 逻辑 (第106-126行):
     * - 从 m_AISubordinatedObject 移除
     * - 通知AI组: pGroup->Die() 和 pGroup->RegenCheck()
     */
    std::shared_ptr<class Monster> RemoveObject(uint64_t object_id);

    /**
     * @brief 加载AI组配置 - 对应 legacy CAISystem::LoadAIGroupList()
     *
     * Legacy 逻辑 (第156-514行):
     * - 从 Resource/Server/Monster_XX.bin 加载配置
     * - 为每个频道创建独立的AI组
     * - 解析配置命令: $GROUP, #ADD, $UNIQUE, #UNIQUEADD 等
     */
    bool LoadAIGroups(uint16_t map_num);

    /**
     * @brief 添加 AI
     */
    void AddAI(uint64_t monster_id, const AIConfig& config);

    /**
     * @brief 移除 AI
     */
    void RemoveAI(uint64_t monster_id);

    /**
     * @brief 获取 AI
     */
    AIStateMachine* GetAI(uint64_t monster_id);

    // ========== 区域查询 ==========

    /**
     * @brief 获取范围内的所有怪物
     */
    std::vector<uint64_t> GetMonstersInRange(
        const Position& center,
        float range
    );

    /**
     * @brief 获取范围内的所有玩家
     */
    std::vector<uint64_t> GetPlayersInRange(
        const Position& center,
        float range
    );

    // ========== AI 分组 ==========

    /**
     * @brief 创建 AI 组
     */
    void CreateAIGroup(uint32_t group_id);

    /**
     * @brief 添加怪物到 AI 组
     */
    void AddMonsterToGroup(uint32_t group_id, uint64_t monster_id);

    /**
     * @brief AI 组成员受到攻击时通知其他成员
     */
    void NotifyGroupMembers(uint32_t group_id, uint64_t attacker_id, uint32_t aggro_value);

    // ========== 怪物刷新 ==========

    /**
     * @brief 刷新怪物
     */
    bool SpawnMonster(
        uint32_t monster_template_id,
        const Position& position,
        uint32_t group_id
    );

    /**
     * @brief 怪物死亡
     */
    void OnMonsterDeath(uint64_t monster_id);

private:
    AIManager() = default;
    ~AIManager() = default;
    AIManager(const AIManager&) = delete;
    AIManager& operator=(const AIManager&) = delete;

    // AI 存储 (使用unique_ptr因为AIStateMachine没有默认构造函数)
    std::unordered_map<uint64_t, std::unique_ptr<AIStateMachine>> ais_;

    // AI 组
    std::unordered_map<uint32_t, std::vector<uint64_t>> ai_groups_;

    // 怪物存储 (对应 legacy m_AISubordinatedObject)
    std::unordered_map<uint64_t, std::shared_ptr<class Monster>> monsters_;

    // 3秒刷新检查 (对应 legacy static DWORD dwRegenCheckTime)
    static uint32_t last_regen_check_time_;
};

} // namespace Game
} // namespace Murim
