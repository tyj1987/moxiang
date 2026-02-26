#include "MonsterAI.hpp"
#include "../entities/Monster.hpp"
#include "../managers/EntityManager.hpp"  // 启用EntityManager集成
#include "shared/character/Player.hpp"    // 启用Player类
#include "shared/character/Character.hpp"
#include "core/spdlog_wrapper.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

namespace Murim {
namespace Game {

MonsterAI::MonsterAI(MapServer::Monster* monster)
    : monster_(monster)
    , search_range_(500)
    , domain_range_(1000)
    , pursuit_forgive_time_(5000)
    , pursuit_forgive_distance_(1500)
    , target_select_(TargetSelectStrategy::CLOSEST)
    , current_target_(nullptr)
    , spawn_point_()
{
    // 在构造函数体中初始化spawn_point_
    if (monster_) {
        spawn_point_ = monster_->GetSpawnPoint();
    } else {
        spawn_point_ = MapServer::Vector3(0.0f, 0.0f, 0.0f);
    }
}

MonsterAI::~MonsterAI() {
    current_target_.reset();
}

std::shared_ptr<Player> MonsterAI::FindTarget() {
    if (!monster_) {
        return nullptr;
    }

    // 获取怪物当前位置
    const MapServer::Vector3& monster_pos = monster_->GetPosition();

    // 转换为Position类型（Murim命名空间，core/spatial/QuadTree.hpp）
    Murim::Position center(monster_pos.x, monster_pos.y, monster_pos.z);

    // 从EntityManager获取附近的玩家
    auto& entity_manager = MapServer::EntityManager::Instance();
    auto nearby_entities = entity_manager.QueryNearbyEntities(
        center,
        static_cast<float>(search_range_),
        MapServer::EntityType::kPlayer
    );

    if (nearby_entities.empty()) {
        spdlog::trace("[MonsterAI] No players in search range ({} units)", search_range_);
        return nullptr;  // 没有找到玩家
    }

    spdlog::debug("[MonsterAI] Found {} nearby player(s)", nearby_entities.size());

    // 根据策略选择目标
    switch (target_select_) {
        case TargetSelectStrategy::CLOSEST:
            return FindClosestTarget(nearby_entities);

        case TargetSelectStrategy::FIRST:
            return FindFirstTarget(nearby_entities);

        case TargetSelectStrategy::LOWEST_HP:
            return FindLowestHPTarget(nearby_entities);

        default:
            spdlog::warn("[MonsterAI] Unknown target selection strategy: {}", static_cast<int>(target_select_));
            return nullptr;
    }

    return nullptr;
}

std::shared_ptr<Player> MonsterAI::FindClosestTarget(
    const std::vector<MapServer::GameEntity*>& nearby_entities
) {
    if (nearby_entities.empty() || !monster_) {
        return nullptr;
    }

    const MapServer::Vector3& monster_pos = monster_->GetPosition();
    float closest_distance = std::numeric_limits<float>::max();
    MapServer::GameEntity* closest_entity = nullptr;

    for (auto entity : nearby_entities) {
        // 计算距离（2D，只考虑x和z）
        float dx = monster_pos.x - entity->position.x;
        float dz = monster_pos.z - entity->position.z;
        float distance = std::sqrt(dx * dx + dz * dz);

        if (distance < closest_distance) {
            closest_distance = distance;
            closest_entity = entity;
        }
    }

    if (closest_entity) {
        spdlog::debug("[MonsterAI] Found closest target (entity_id={}) at distance {:.2f}",
                      closest_entity->entity_id, closest_distance);

        // 从GameEntity创建Player对象
        auto player = Player::FromGameEntity(closest_entity);
        if (player && player->IsValid()) {
            spdlog::debug("[MonsterAI] Selected closest target: {} (HP: {}/{}, Lv: {})",
                          player->GetName(), player->GetHP(), player->GetMaxHP(), player->GetLevel());
            return player;
        } else {
            spdlog::warn("[MonsterAI] Failed to create Player from GameEntity (entity_id={})",
                        closest_entity->entity_id);
        }
    }

    return nullptr;
}

std::shared_ptr<Player> MonsterAI::FindFirstTarget(
    const std::vector<MapServer::GameEntity*>& nearby_entities
) {
    if (nearby_entities.empty()) {
        return nullptr;
    }

    // 返回第一个进入索敌范围的玩家
    auto first_entity = nearby_entities[0];
    spdlog::debug("[MonsterAI] Found first target (entity_id={})", first_entity->entity_id);

    // 从GameEntity创建Player对象
    auto player = Player::FromGameEntity(first_entity);
    if (player && player->IsValid()) {
        spdlog::debug("[MonsterAI] Selected first target: {} (HP: {}/{}, Lv: {})",
                      player->GetName(), player->GetHP(), player->GetMaxHP(), player->GetLevel());
        return player;
    } else {
        spdlog::warn("[MonsterAI] Failed to create Player from GameEntity (entity_id={})",
                    first_entity->entity_id);
    }

    return nullptr;
}

std::shared_ptr<Player> MonsterAI::FindLowestHPTarget(
    const std::vector<MapServer::GameEntity*>& nearby_entities
) {
    if (nearby_entities.empty()) {
        return nullptr;
    }

    uint32_t lowest_hp = std::numeric_limits<uint32_t>::max();
    MapServer::GameEntity* lowest_hp_entity = nullptr;

    for (auto entity : nearby_entities) {
        // 从GameEntity创建Player对象并获取HP
        auto player = Player::FromGameEntity(entity);
        if (player && player->IsValid()) {
            uint32_t hp = player->GetHP();
            if (hp < lowest_hp) {
                lowest_hp = hp;
                lowest_hp_entity = entity;
            }
        }
    }

    if (lowest_hp_entity) {
        spdlog::debug("[MonsterAI] Found lowest HP target (HP: {})", lowest_hp);

        // 从GameEntity创建Player对象
        auto player = Player::FromGameEntity(lowest_hp_entity);
        if (player && player->IsValid()) {
            spdlog::debug("[MonsterAI] Selected lowest HP target: {} (HP: {}/{}, Lv: {})",
                          player->GetName(), player->GetHP(), player->GetMaxHP(), player->GetLevel());
            return player;
        }
    }

    return nullptr;
}

bool MonsterAI::IsInSearchRange(std::shared_ptr<Entity> target) const {
    if (!monster_ || !target) {
        return false;
    }

    float distance = GetDistanceTo(target);
    return distance <= static_cast<float>(search_range_);
}

bool MonsterAI::IsInAttackRange(std::shared_ptr<Entity> target) const {
    if (!monster_ || !target) {
        return false;
    }

    // 攻击范围通常是索敌范围的40%
    float attack_range = search_range_ * 0.4f;
    float distance = GetDistanceTo(target);

    return distance <= attack_range;
}

bool MonsterAI::IsInAttackRange(std::shared_ptr<Player> target) const {
    if (!monster_ || !target) {
        return false;
    }

    // 攻击范围通常是索敌范围的40%
    float attack_range = search_range_ * 0.4f;
    float distance = GetDistanceTo(target);

    return distance <= attack_range;
}

bool MonsterAI::IsInSearchAngle(std::shared_ptr<Entity> target, uint32_t angle) const {
    if (!target || angle >= 360) {
        return true;  // 360度表示全方向索敌
    }

    // TODO: 实现角度检测
    // 需要怪物的朝向和目标位置
    spdlog::debug("[MonsterAI] IsInSearchAngle - TODO: Implement angle check");
    return true;
}

bool MonsterAI::IsOutsideDomain(const MapServer::Vector3& position) const {
    float dx = position.x - spawn_point_.x;
    float dz = position.z - spawn_point_.z;
    float distance = std::sqrt(dx * dx + dz * dz);

    return distance > static_cast<float>(domain_range_);
}

bool MonsterAI::ShouldStopPursuit(uint64_t chase_start_time) const {
    if (!current_target_ || !monster_) {
        return true;
    }

    // 检查追击距离
    float distance = GetDistanceTo(current_target_);
    if (distance > static_cast<float>(pursuit_forgive_distance_)) {
        return true;
    }

    // TODO: 检查追击时间
    // auto now = std::chrono::system_clock::now();
    // auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    //     now - chase_start_time
    // ).count();
    // if (elapsed > pursuit_forgive_time_) {
    //     return true;
    // }

    return false;
}

float MonsterAI::GetDistanceTo(std::shared_ptr<Entity> target) const {
    if (!monster_ || !target) {
        return std::numeric_limits<float>::max();
    }

    // 获取怪物位置
    const MapServer::Vector3& monster_pos = monster_->GetPosition();

    // 从Entity获取目标位置
    float target_x, target_y, target_z;
    target->GetPosition(&target_x, &target_y, &target_z);

    // 计算2D距离（只考虑x和z）
    float dx = monster_pos.x - target_x;
    float dz = monster_pos.z - target_z;
    float distance = std::sqrt(dx * dx + dz * dz);

    return distance;
}

float MonsterAI::GetDistanceTo(std::shared_ptr<Player> target) const {
    if (!monster_ || !target) {
        return std::numeric_limits<float>::max();
    }

    // 获取怪物位置
    const MapServer::Vector3& monster_pos = monster_->GetPosition();

    // 从Player获取目标位置
    float target_x, target_y, target_z;
    target->GetPosition(&target_x, &target_y, &target_z);

    // 计算2D距离（只考虑x和z）
    float dx = monster_pos.x - target_x;
    float dz = monster_pos.z - target_z;
    float distance = std::sqrt(dx * dx + dz * dz);

    return distance;
}

float MonsterAI::GetDistanceToSpawnPoint() const {
    if (!monster_) {
        return std::numeric_limits<float>::max();
    }

    return monster_->GetDistanceToSpawnPoint();
}

} // namespace Game
} // namespace Murim
