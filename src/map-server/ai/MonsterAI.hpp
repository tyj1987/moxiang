#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "../entities/Monster.hpp"  // 获取Vector3定义
#include "../managers/EntityManager.hpp"  // 获取GameEntity定义

namespace Murim {

namespace MapServer {
class Monster;
struct GameEntity;
struct Vector3;
} // namespace MapServer

namespace Game {

class Entity;
class Player;

/**
 * @brief 怪物AI系统
 *
 * 负责怪物的智能行为：
 * - 索敌（Search）
 * - 目标选择（Target Selection）
 * - 距离检测（Range Check）
 * - 角度检测（Angle Check）
 */
class MonsterAI {
public:
    /**
     * @brief 目标选择策略
     */
    enum class TargetSelectStrategy : uint8_t {
        CLOSEST = 0,    // 选择最近的玩家
        FIRST = 1,      // 选择第一个进入索敌范围的玩家
        LOWEST_HP = 2   // 选择血量最少的玩家
    };

    /**
     * @brief 构造函数
     * @param monster 拥有此AI的怪物
     */
    explicit MonsterAI(MapServer::Monster* monster);

    /**
     * @brief 析构函数
     */
    ~MonsterAI();

    /**
     * @brief 索敌：在索敌范围内查找目标玩家
     * @return 找到的目标玩家，如果没有则返回nullptr
     */
    std::shared_ptr<Player> FindTarget();

    /**
     * @brief 查找最近的玩家（CLOSEST策略）
     */
    std::shared_ptr<Player> FindClosestTarget(
        const std::vector<MapServer::GameEntity*>& nearby_entities
    );

    /**
     * @brief 查找第一个玩家（FIRST策略）
     */
    std::shared_ptr<Player> FindFirstTarget(
        const std::vector<MapServer::GameEntity*>& nearby_entities
    );

    /**
     * @brief 查找血量最少的玩家（LOWEST_HP策略）
     */
    std::shared_ptr<Player> FindLowestHPTarget(
        const std::vector<MapServer::GameEntity*>& nearby_entities
    );

    /**
     * @brief 检查目标是否在索敌范围内
     * @param target 要检查的目标
     * @return true如果目标在索敌范围内
     */
    bool IsInSearchRange(std::shared_ptr<Entity> target) const;

    /**
     * @brief 检查目标是否在攻击范围内
     * @param target 要检查的目标
     * @return true如果目标在攻击范围内
     */
    bool IsInAttackRange(std::shared_ptr<Entity> target) const;

    /**
     * @brief 检查目标是否在攻击范围内（Player版本）
     * @param target 要检查的目标玩家
     * @return true如果目标在攻击范围内
     */
    bool IsInAttackRange(std::shared_ptr<Player> target) const;

    /**
     * @brief 检查目标是否在索敌角度内（前方扇形）
     * @param target 要检查的目标
     * @param angle 索敌角度（0-360度）
     * @return true如果目标在索敌角度内
     */
    bool IsInSearchAngle(std::shared_ptr<Entity> target, uint32_t angle) const;

    /**
     * @brief 检查是否超出活动范围
     * @param position 当前位置
     * @return true如果超出活动范围
     */
    bool IsOutsideDomain(const MapServer::Vector3& position) const;

    /**
     * @brief 检查是否应该停止追击（超出追击原谅范围或时间）
     * @param chase_start_time 开始追击的时间戳
     * @return true如果应该停止追击
     */
    bool ShouldStopPursuit(uint64_t chase_start_time) const;

    /**
     * @brief 计算到目标的距离
     * @param target 目标实体
     * @return 距离（像素）
     */
    float GetDistanceTo(std::shared_ptr<Entity> target) const;

    /**
     * @brief 计算到玩家目标的距离
     * @param target 目标玩家
     * @return 距离（像素）
     */
    float GetDistanceTo(std::shared_ptr<Player> target) const;

    /**
     * @brief 计算到出生点的距离
     * @return 距离（像素）
     */
    float GetDistanceToSpawnPoint() const;

    /**
     * @brief 获取当前目标
     * @return 当前目标玩家，如果没有则返回nullptr
     */
    std::shared_ptr<Player> GetCurrentTarget() const { return current_target_; }

    /**
     * @brief 设置当前目标
     * @param target 新的目标玩家
     */
    void SetCurrentTarget(std::shared_ptr<Player> target) { current_target_ = target; }

    /**
     * @brief 清除当前目标
     */
    void ClearTarget() { current_target_.reset(); }

    /**
     * @brief 获取索敌范围
     * @return 索敌范围（像素）
     */
    uint32_t GetSearchRange() const { return search_range_; }

    /**
     * @brief 设置索敌范围
     * @param range 新的索敌范围
     */
    void SetSearchRange(uint32_t range) { search_range_ = range; }

    /**
     * @brief 获取活动范围
     * @return 活动范围（像素）
     */
    uint32_t GetDomainRange() const { return domain_range_; }

    /**
     * @brief 设置活动范围
     * @param range 新的活动范围
     */
    void SetDomainRange(uint32_t range) { domain_range_ = range; }

    /**
     * @brief 获取目标选择策略
     * @return 目标选择策略
     */
    TargetSelectStrategy GetTargetSelectStrategy() const { return target_select_; }

    /**
     * @brief 设置目标选择策略
     * @param strategy 新的目标选择策略
     */
    void SetTargetSelectStrategy(TargetSelectStrategy strategy) { target_select_ = strategy; }

private:
    // 拥有此AI的怪物
    MapServer::Monster* monster_;

    // AI参数
    uint32_t search_range_;          // 索敌范围（像素）
    uint32_t domain_range_;          // 活动范围（像素）
    uint32_t pursuit_forgive_time_;  // 追击原谅时间（毫秒）
    uint32_t pursuit_forgive_distance_; // 追击原谅距离（像素）
    TargetSelectStrategy target_select_; // 目标选择策略

    // 当前状态
    std::shared_ptr<Player> current_target_;  // 当前目标
    MapServer::Vector3 spawn_point_;          // 出生点（用于检测活动范围）
};

} // namespace Game
} // namespace Murim
