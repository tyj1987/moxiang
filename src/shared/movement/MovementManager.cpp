#include "Movement.hpp"
#include "shared/character/CharacterManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <cmath>
#include <unordered_map>
#include <algorithm>

// using Database = Murim::Core::Database;  // Removed - causes parsing errors
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Game {

MovementManager& MovementManager::Instance() {
    static MovementManager instance;
    return instance;
}

// ========== 移动验证 ==========

MovementValidationResult MovementManager::ValidateMovement(
    const MovementInfo& current_info,
    const MovementValidationConfig& config
) {
    // 1. 验证位置有效性
    if (!current_info.position.IsValid()) {
        spdlog::warn("Character {} has invalid position: ({}, {}, {})",
                     current_info.character_id,
                     current_info.position.x,
                     current_info.position.y,
                     current_info.position.z);
        return MovementValidationResult::kInvalidPosition;
    }

    // 2. 验证地图边界
    if (current_info.position.x < config.map_min_x ||
        current_info.position.x > config.map_max_x ||
        current_info.position.y < config.map_min_y ||
        current_info.position.y > config.map_max_y ||
        current_info.position.z < config.map_min_z ||
        current_info.position.z > config.map_max_z) {

        spdlog::warn("Character {} position out of bounds: ({}, {}, {})",
                     current_info.character_id,
                     current_info.position.x,
                     current_info.position.y,
                     current_info.position.z);
        return MovementValidationResult::kInvalidPosition;
    }

    // 3. 验证朝向
    if (current_info.direction > 360) {
        spdlog::warn("Character {} has invalid direction: {}",
                     current_info.character_id,
                     current_info.direction);
        return MovementValidationResult::kWrongDirection;
    }

    // 4. 验证时间戳
    uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    if (current_info.timestamp > current_time + config.max_timestamp_delta) {
        spdlog::warn("Character {} has future timestamp: {} (current: {})",
                     current_info.character_id,
                     current_info.timestamp,
                     current_time);
        return MovementValidationResult::kTimestampError;
    }

    // 5. 如果是首次移动,只验证基本条件
    if (current_info.last_update_time == 0) {
        return MovementValidationResult::kValid;
    }

    // 6. 计算移动速度
    float delta_time = (current_info.timestamp - current_info.last_update_time) / 1000.0f; // 转换为秒

    if (delta_time <= 0.0f) {
        spdlog::warn("Character {} has invalid delta time: {}",
                     current_info.character_id,
                     delta_time);
        return MovementValidationResult::kTimestampError;
    }

    Position old_pos(current_info.last_valid_x, current_info.last_valid_y, 0.0f);
    float calculated_speed = CalculateSpeed(old_pos, current_info.position, delta_time);

    // 7. 验证速度
    float expected_speed = current_info.GetSpeed();
    float max_allowed_speed = config.max_run_speed;

    if (IsSpeedAbnormal(calculated_speed, max_allowed_speed, 1.5f)) {
        spdlog::warn("Character {} speed abnormal: calculated={}, max_allowed={}",
                     current_info.character_id,
                     calculated_speed,
                     max_allowed_speed);

        RecordValidationFailure(current_info.character_id, MovementValidationResult::kTooFast);
        return MovementValidationResult::kTooFast;
    }

    // 8. 验证距离 (检测瞬移)
    float distance = old_pos.Distance2D(current_info.position);

    if (IsTeleport(distance, config.max_teleport_distance)) {
        spdlog::warn("Character {} possible teleport: distance={}, max_allowed={}",
                     current_info.character_id,
                     distance,
                     config.max_teleport_distance);

        RecordValidationFailure(current_info.character_id, MovementValidationResult::kTooFar);
        return MovementValidationResult::kTooFar;
    }

    return MovementValidationResult::kValid;
}

MovementValidationResult MovementManager::ValidatePositionUpdate(
    uint64_t character_id,
    const Position& old_position,
    const Position& new_position,
    float delta_time,
    float speed,
    const MovementValidationConfig& config
) {
    // 1. 验证新位置有效性
    if (!new_position.IsValid()) {
        return MovementValidationResult::kInvalidPosition;
    }

    // 2. 验证地图边界
    if (new_position.x < config.map_min_x ||
        new_position.x > config.map_max_x ||
        new_position.y < config.map_min_y ||
        new_position.y > config.map_max_y ||
        new_position.z < config.map_min_z ||
        new_position.z > config.map_max_z) {
        return MovementValidationResult::kInvalidPosition;
    }

    // 3. 计算移动速度
    float calculated_speed = this->CalculateSpeed(old_position, new_position, delta_time);

    // 4. 验证速度
    float max_allowed_speed = config.max_run_speed;

    if (calculated_speed > max_allowed_speed * 1.5f) {
        spdlog::warn("Character {} speed violation: calculated={}, max_allowed={}",
                     character_id,
                     calculated_speed,
                     max_allowed_speed);
        return MovementValidationResult::kTooFast;
    }

    // 5. 验证距离
    float distance = old_position.Distance2D(new_position);
    float max_distance = speed * delta_time + config.position_tolerance;

    if (distance > max_distance) {
        spdlog::warn("Character {} distance violation: distance={}, max_allowed={}",
                     character_id,
                     distance,
                     max_distance);
        return MovementValidationResult::kTooFar;
    }

    return MovementValidationResult::kValid;
}

// ========== 移动计算 ==========

float MovementManager::CalculateSpeed(
    const Position& from,
    const Position& to,
    float delta_time
) {
    if (delta_time <= 0.0f) {
        return 0.0f;
    }

    float distance = from.Distance2D(to);
    return distance / delta_time;
}

float MovementManager::CalculateMoveTime(
    const Position& from,
    const Position& to,
    float speed
) {
    if (speed <= 0.0f) {
        return 0.0f;
    }

    float distance = from.Distance2D(to);
    return distance / speed;
}

Position MovementManager::PredictPosition(
    const Position& from,
    float direction,
    float speed,
    float delta_time
) {
    float distance = speed * delta_time;

    Position result;
    result.x = from.x + std::cos(direction) * distance;
    result.y = from.y + std::sin(direction) * distance;
    result.z = from.z;

    return result;
}

// ========== 作弊检测 ==========

bool MovementManager::IsSpeedAbnormal(
    float calculated_speed,
    float expected_speed,
    float tolerance
) {
    if (expected_speed <= 0.0f) {
        return calculated_speed > 300.0f;  // 绝对最大速度
    }

    return calculated_speed > expected_speed * tolerance;
}

bool MovementManager::IsTeleport(
    float distance,
    float max_allowed_distance
) {
    return distance > max_allowed_distance;
}

// ========== 配置管理 ==========

const MovementValidationConfig& MovementManager::GetDefaultConfig() {
    static MovementValidationConfig config;
    return config;
}

void MovementManager::SetValidationConfig(const MovementValidationConfig& config) {
    config_ = config;
    spdlog::info("Movement validation config updated");
}

// ========== 统计信息 ==========

MovementStats MovementManager::GetStats(uint64_t character_id) {
    auto it = character_stats_.find(character_id);
    if (it != character_stats_.end()) {
        return it->second;
    }

    // 如果没有统计信息,返回空统计
    MovementStats stats;
    return stats;
}

void MovementManager::ResetStats(uint64_t character_id) {
    auto it = character_stats_.find(character_id);
    if (it != character_stats_.end()) {
        it->second.Reset();
        spdlog::debug("Reset movement stats for character {}", character_id);
    } else {
        spdlog::warn("No stats found for character {} to reset", character_id);
    }
}

MovementStats MovementManager::GetGlobalStats() {
    return global_stats_;
}

// ========== 反作弊 ==========

void MovementManager::RecordValidationFailure(uint64_t character_id, MovementValidationResult result) {
    // 更新全局统计
    global_stats_.validation_failed++;

    switch (result) {
        case MovementValidationResult::kTooFar:
            global_stats_.teleport_detected++;
            break;
        case MovementValidationResult::kTooFast:
            global_stats_.speed_violation++;
            break;
        default:
            break;
    }

    // 更新角色统计
    character_stats_[character_id].validation_failed++;

    switch (result) {
        case MovementValidationResult::kTooFar:
            character_stats_[character_id].teleport_detected++;
            break;
        case MovementValidationResult::kTooFast:
            character_stats_[character_id].speed_violation++;
            break;
        default:
            break;
    }

    spdlog::debug("Recorded validation failure for character {}: type={}",
                  character_id,
                  static_cast<int>(result));
}

bool MovementManager::ShouldKick(uint64_t character_id, uint32_t threshold) {
    auto stats = GetStats(character_id);
    return stats.validation_failed >= threshold;
}

// ========== 角色速度查询 ==========

float MovementManager::GetCharacterSpeed(uint64_t character_id, uint8_t move_state) {
    // 获取角色属性中的基础速度
    try {
        auto character = CharacterManager::Instance().GetCharacter(character_id);
        if (character.has_value()) {
            // 从角色属性中获取移动速度
            // 这里需要获取 CharacterStats, 目前Character结构中没有包含stats
            // 作为简化实现,我们使用基础速度

            // 基础速度: 行走 100, 奔跑 200
            float base_speed = 0.0f;
            switch (move_state) {
                case 1: base_speed = 100.0f; break;  // 行走
                case 2: base_speed = 200.0f; break;  // 奔跑
                default: base_speed = 0.0f; break;   // 停止
            }

            // TODO: 从 CharacterStats 获取实际移动速度
            // 实际实现需要:
            // 1. 获取角色的 CharacterStats (包含 move_speed 属性)
            // 2. 计算装备加成
            // 3. 计算buff/debuff影响
            // 4. 返回最终速度: base_speed * (1 + stats.move_speed / 100.0f)

            // 简化实现: 只返回基础速度
            return base_speed;
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to get character speed for {}: {}", character_id, e.what());
    }

    // 默认返回基础速度
    switch (move_state) {
        case 1: return 100.0f;  // 行走
        case 2: return 200.0f;  // 奔跑
        default: return 0.0f;   // 停止
    }
}

} // namespace Game
} // namespace Murim
