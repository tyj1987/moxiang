#include "MovementValidator.hpp"
#include "core/spdlog_wrapper.hpp"
#include <cmath>

namespace Murim {
namespace Shared {
namespace Movement {

MovementValidator::MovementValidator(const MovementValidationConfig& config)
    : config_(config) {
    spdlog::info("MovementValidator initialized with map bounds: ({}, {}), ({}, {}), ({}, {})",
                 config_.map_min_x, config_.map_max_x,
                 config_.map_min_y, config_.map_max_y,
                 config_.map_min_z, config_.map_max_z);
}

MovementValidationResult MovementValidator::ValidateMovement(
    const MovementInfo& info,
    const MovementValidationConfig& config) {

    spdlog::debug("Validating movement for character {}: pos=({}, {}, {}), dir={}, speed={}",
                 info.character_id, info.x, info.y, info.z,
                 info.direction, info.speed);

    // 1. 验证位置有效性
    if (!IsValidPosition(info, config_)) {
        spdlog::warn("Movement validation failed: invalid position");
        return MovementValidationResult::kInvalidPosition;
    }

    // 2. 验证方向有效性
    if (!IsValidDirection(info, config_)) {
        spdlog::warn("Movement validation failed: invalid direction");
        return MovementValidationResult::kInvalidDirection;
    }

    // 3. 验证速度合理性
    if (info.speed < config.min_speed) {
        spdlog::warn("Speed {} is below minimum {}", info.speed, config.min_speed);
        return MovementValidationResult::kSpeedTooLow;
    }
    float max_allowed = config.max_run_speed * (1.0f + config.speed_tolerance);
    if (info.speed > max_allowed) {
        spdlog::warn("Speed {} exceeds maximum {} (max_run={}, tolerance={})",
                    info.speed, max_allowed, config.max_run_speed, config.speed_tolerance);
        return MovementValidationResult::kSpeedTooHigh;
    }

    // 4. 验证时间戳（如果有上次有效时间）
    // 注意：首次移动时没有上次时间，可以通过验证
    // 这里暂时返回有效，实际验证应该使用会话信息

    // 5. 验证地图边界
    if (!IsWithinMapBounds(info, config_)) {
        spdlog::warn("Movement validation failed: out of map bounds");
        return MovementValidationResult::kOutOfMap;
    }

    // 所有验证通过
    spdlog::info("Movement validation successful for character {}", info.character_id);
    return MovementValidationResult::kValid;
}

bool MovementValidator::IsValidPosition(const MovementInfo& info, const MovementValidationConfig& config) {
    return info.IsValidX() && info.IsValidY() && info.IsValidZ();
}

bool MovementValidator::IsValidDirection(const MovementInfo& info, const MovementValidationConfig& config) {
    // 方向范围: 0-359度
    return info.IsValidDirection();
}

bool MovementValidator::IsValidSpeed(const MovementInfo& info, const MovementValidationConfig& config) {
    // 速度必须大于最小值
    if (info.speed < config_.min_speed) {
        spdlog::warn("Speed {} is below minimum {}", info.speed, config_.min_speed);
        return false;
    }

    // 根据移动类型判断最大速度
    float max_speed = (info.speed >= config_.max_run_speed * 0.8f) ?
                      config_.max_run_speed : config_.max_walk_speed;

    // 检查是否超过最大速度（考虑容差）
    if (info.speed > max_speed * (1.0f + config_.speed_tolerance)) {
        spdlog::warn("Speed {} exceeds maximum {}", info.speed, max_speed);
        return false;
    }

    return true;
}

bool MovementValidator::IsValidTimestamp(const MovementInfo& info, uint64_t last_valid_time, const MovementValidationConfig& config) {
    // 检查时间戳是否倒流（当前时间小于上次时间）
    if (info.timestamp < last_valid_time) {
        spdlog::warn("Timestamp {} is earlier than last valid {}", info.timestamp, last_valid_time);
        return false;
    }

    // 检查时间差是否过大
    uint64_t time_delta = (info.timestamp >= last_valid_time) ?
        (info.timestamp - last_valid_time) : (last_valid_time - info.timestamp);

    if (time_delta > config_.max_timestamp_delta) {
        spdlog::warn("Timestamp delta {} exceeds maximum {}", time_delta, config_.max_timestamp_delta);
        return false;
    }

    // 检查时间差是否过小
    if (time_delta < config_.min_timestamp_delta) {
        spdlog::warn("Timestamp delta {} is below minimum {}", time_delta, config_.min_timestamp_delta);
        return false;
    }

    return true;
}

bool MovementValidator::IsWithinMapBounds(const MovementInfo& info, const MovementValidationConfig& config) {
    // 检查X坐标
    if (info.x < config_.map_min_x || info.x > config_.map_max_x) {
        return false;
    }

    // 检查Y坐标
    if (info.y < config_.map_min_y || info.y > config_.map_max_y) {
        return false;
    }

    // 检查Z坐标
    if (info.z < config_.map_min_z || info.z > config_.map_max_z) {
        return false;
    }

    return true;
}

} // namespace Movement
} // namespace Shared
} // namespace Murim
