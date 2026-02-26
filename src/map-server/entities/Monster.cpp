#include "Monster.hpp"
#include "map-server/ai/MonsterAI.hpp"
#include "map-server/ai/MonsterStateMachine.hpp"  // 状态机
#include "map-server/managers/EntityManager.hpp"
#include "shared/character/Character.hpp"
#include "shared/character/Player.hpp"  // 需要完整定义以调用方法
#include "core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>

namespace Murim {
namespace MapServer {

Monster::Monster(
    uint64_t entity_id,
    uint32_t monster_id,
    const Game::MonsterTemplate& template_data,
    const Vector3& spawn_point
)
    : entity_id_(entity_id)
    , monster_id_(monster_id)
    , template_(template_data)
    , spawn_point_id_(0)
    , map_id_(1)
    , current_hp_(template_data.hp)
    , position_(spawn_point)
    , spawn_point_(spawn_point)
    , ai_(nullptr)
    , state_machine_(nullptr)
    , current_state_(Game::AIState::kSpawn)
    , chase_target_(nullptr)
{
    // 创建AI系统
    ai_ = std::make_unique<Game::MonsterAI>(this);

    // 初始化AI参数
    ai_->SetSearchRange(template_data.search_range);
    ai_->SetDomainRange(template_data.domain_range);
    ai_->SetTargetSelectStrategy(static_cast<Game::MonsterAI::TargetSelectStrategy>(
        template_data.target_select
    ));

    // 初始化状态机
    state_machine_ = std::make_unique<Game::MonsterStateMachine>(this);

    spdlog::debug("[Monster] Created monster {} (Lv.{}) at ({}, {}, {})",
                  template_.monster_name, template_.level,
                  position_.x, position_.y, position_.z);
}

Monster::~Monster() {
    spdlog::debug("[Monster] Destroyed monster {}", template_.monster_name);
}

Game::MonsterAI& Monster::GetAI() {
    return *ai_;
}

const Game::MonsterAI& Monster::GetAI() const {
    return *ai_;
}

void Monster::SetState(Game::AIState new_state) {
    if (current_state_ == new_state) {
        return;  // 状态未改变
    }

    Game::AIState old_state = current_state_;
    current_state_ = new_state;

    spdlog::debug("[Monster] {} state transition: {} -> {}",
                  template_.monster_name,
                  static_cast<int>(old_state),
                  static_cast<int>(new_state));

    // 通过状态机处理状态转换
    if (state_machine_) {
        state_machine_->ChangeState(new_state);
    }
}

void Monster::Update(uint32_t delta_time) {
    if (!IsAlive()) {
        return;  // 死亡的怪物不更新
    }

    // 使用状态机更新AI行为
    if (state_machine_) {
        state_machine_->Update(delta_time);

        // 同步状态到current_state_
        current_state_ = state_machine_->GetCurrentState();
    } else {
        // 备选：如果没有状态机，使用简单的状态切换
        // 这种情况不应该发生，但作为防御性编程
        spdlog::warn("[Monster] {} state_machine is null!", template_.monster_name);
    }
}

bool Monster::Attack(std::shared_ptr<Game::Entity> target) {
    if (!target || !IsAlive()) {
        return false;
    }

    // 1. 计算伤害
    uint16_t damage = CalculateAttackDamage();

    // 2. 应用伤害到目标
    uint32_t actual_damage = target->TakeDamage(damage);

    // 3. 记录日志
    spdlog::debug("[Monster] {} attacks target (entity_id={}) for {} actual damage",
                  template_.monster_name, target->GetEntityId(), actual_damage);

    // TODO: 4. 发送攻击消息到客户端
    // TODO: 5. 如果目标死亡，触发死亡事件

    return true;
}

bool Monster::Attack(std::shared_ptr<Game::Player> target) {
    if (!target || !IsAlive()) {
        return false;
    }

    // 1. 计算伤害
    uint16_t damage = CalculateAttackDamage();

    // 2. 应用伤害到目标玩家
    uint32_t actual_damage = target->TakeDamage(damage);

    // 3. 记录日志
    spdlog::debug("[Monster] {} attacks player {} for {} damage (HP: {}/{})",
                  template_.monster_name, target->GetName(),
                  actual_damage, target->GetHP(), target->GetMaxHP());

    // TODO: 4. 发送攻击消息到客户端
    // TODO: 5. 如果目标死亡，触发死亡事件和掉落

    return true;
}

void Monster::MoveTo(const Vector3& target, uint32_t delta_time) {
    if (!IsAlive()) {
        return;
    }

    // 计算移动方向
    float dx = target.x - position_.x;
    float dz = target.z - position_.z;
    float distance = std::sqrt(dx * dx + dz * dz);

    if (distance < 1.0f) {
        return;  // 已到达目标
    }

    // 归一化方向
    float dir_x = dx / distance;
    float dir_z = dz / distance;

    // 计算移动距离（根据速度和时间）
    float speed = static_cast<float>(template_.run_speed);
    float move_distance = speed * (delta_time / 1000.0f);  // 转换为秒

    // 限制移动距离不超过目标距离
    move_distance = std::min(move_distance, distance);

    // 更新位置
    position_.x += dir_x * move_distance;
    position_.z += dir_z * move_distance;

    spdlog::trace("[Monster] {} moved to ({}, {}, {})",
                  template_.monster_name, position_.x, position_.y, position_.z);
}

void Monster::MoveToEntity(std::shared_ptr<Game::Entity> target, uint32_t delta_time) {
    if (!target) {
        return;
    }

    // 从Entity获取目标位置
    float target_x, target_y, target_z;
    target->GetPosition(&target_x, &target_y, &target_z);

    Vector3 target_pos(target_x, target_y, target_z);
    MoveTo(target_pos, delta_time);
}

void Monster::MoveToEntity(std::shared_ptr<Game::Player> target, uint32_t delta_time) {
    if (!target) {
        return;
    }

    // 从Player获取目标位置
    float target_x, target_y, target_z;
    target->GetPosition(&target_x, &target_y, &target_z);

    Vector3 target_pos(target_x, target_y, target_z);
    MoveTo(target_pos, delta_time);
}

void Monster::ReturnToSpawnPoint() {
    MoveTo(spawn_point_, 100);  // 假设100ms一帧
}

bool Monster::ShouldStopPursuit() const {
    if (!chase_target_) {
        return true;
    }

    // 检查距离
    float distance = GetDistanceToSpawnPoint();
    if (distance > static_cast<float>(template_.domain_range)) {
        spdlog::debug("[Monster] {} stopping pursuit: outside domain range ({} > {})",
                      template_.monster_name,
                      distance,
                      template_.domain_range);
        return true;
    }

    // TODO: 检查时间
    // auto now = std::chrono::system_clock::now();
    // auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    //     now - chase_start_time_
    // );
    // if (elapsed.count() > template_.pursuit_forgive_time_) {
    //     spdlog::debug("[Monster] {} stopping pursuit: time expired",
    //                   template_.monster_name);
    //     return true;
    // }

    return false;
}

void Monster::OnDeath() {
    spdlog::info("[Monster] {} died", template_.monster_name);

    // 切换到死亡状态
    SetState(Game::AIState::kDead);

    // 清除目标
    chase_target_.reset();
    ai_->ClearTarget();

    // TODO: 通知刷怪点管理器重生
    // TODO: 掉落物品和经验
}

void Monster::OnRevive() {
    spdlog::info("[Monster] {} revived", template_.monster_name);

    // 恢复HP
    current_hp_ = template_.hp;

    // 返回出生点
    position_ = spawn_point_;

    // 切换到待机状态
    SetState(Game::AIState::kIdle);
}

std::string Monster::GetDebugInfo() const {
    std::ostringstream oss;
    oss << "Monster[" << entity_id_ << "] " << template_.monster_name
        << " (Lv." << template_.level << ") "
        << "HP:" << current_hp_ << "/" << template_.hp
        << " State:" << static_cast<int>(current_state_)
        << " Pos:(" << position_.x << "," << position_.y << "," << position_.z << ")";
    return oss.str();
}

} // namespace MapServer
} // namespace Murim
