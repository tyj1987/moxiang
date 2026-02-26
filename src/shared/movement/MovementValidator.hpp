#pragma once

#include <cstdint>
#include <string>
#include "shared/game/GameObject.hpp"

namespace Murim {
namespace Shared {
namespace Movement {

/**
 * @brief 移动验证结果
 */
enum class MovementValidationResult {
    kValid = 0,              // 移动有效
    kInvalidPosition = 1,      // 位置无效
    kInvalidDirection = 2,     // 方向无效
    kSpeedTooHigh = 3,         // 速度过快
    kSpeedTooLow = 4,          // 速度过慢
    kTimestampError = 5,       // 时间戳错误
    kOutOfMap = 6              // 超出地图边界
};

/**
 * @brief 移动验证配置
 */
struct MovementValidationConfig {
    // 地图边界（单位：游戏坐标）
    float map_min_x = -5000.0f;
    float map_max_x = 5000.0f;
    float map_min_y = -5000.0f;
    float map_max_y = 5000.0f;
    float map_min_z = -500.0f;
    float map_max_z = 5000.0f;

    // 移动速度限制（单位：游戏单位/秒）
    float max_walk_speed = 300.0f;      // 最大走路速度
    float max_run_speed = 600.0f;        // 最大跑步速度
    float min_speed = 50.0f;             // 最小速度（防止过慢）
    float speed_tolerance = 0.05f;        // 速度容差（5%）

    // 时间戳验证（毫秒）
    uint64_t max_timestamp_delta = 5000;  // 最大时间差（5秒）
    uint64_t min_timestamp_delta = 10;      // 最小时间差（10ms）
};

/**
 * @brief 移动信息
 */
struct MovementInfo {
    uint64_t character_id;        // 角色ID
    float x, y, z;              // 当前位置
    float target_x, target_y, target_z;  // 目标位置
    uint16_t direction;          // 朝向（0-359度，0=北）
    float speed;                 // 移动速度
    uint64_t timestamp;          // 时间戳

    // 地图边界
    static constexpr float map_min_x = -5000.0f;
    static constexpr float map_max_x = 5000.0f;
    static constexpr float map_min_y = -5000.0f;
    static constexpr float map_max_y = 5000.0f;
    static constexpr float map_min_z = -5000.0f;
    static constexpr float map_max_z = 5000.0f;

    // 便捷方法
    bool IsValidX() const { return x >= map_min_x && x <= map_max_x; }
    bool IsValidY() const { return y >= map_min_y && y <= map_max_y; }
    bool IsValidZ() const { return z >= map_min_z && z <= map_max_z; }
    bool IsValidDirection() const { return direction < 360; }
};

/**
 * @brief 移动验证器
 *
 * 职责：
 * 1. 验证移动位置有效性
 * 2. 验证移动方向有效性
 * 3. 验证移动速度合理性
 * 4. 验证时间戳有效性
 * 5. 验证地图边界
 *
 * 对应 legacy: MapServer中的移动验证逻辑
 */
class MovementValidator {
public:
    MovementValidator(const MovementValidationConfig& config);
    ~MovementValidator() = default;

    /**
     * @brief 验证移动请求
     * @param info 移动信息
     * @param config 验证配置
     * @return 验证结果
     */
    MovementValidationResult ValidateMovement(const MovementInfo& info, const MovementValidationConfig& config);

    /**
     * @brief 验证位置有效性
     * @param info 移动信息
     * @param config 验证配置
     * @return 是否有效
     */
    bool IsValidPosition(const MovementInfo& info, const MovementValidationConfig& config);

    /**
     * @brief 验证方向有效性
     * @param info 移动信息
     * @param config 验证配置
     * @return 是否有效
     */
    bool IsValidDirection(const MovementInfo& info, const MovementValidationConfig& config);

    /**
     * @brief 验证速度合理性
     * @param info 移动信息
     * @param config 验证配置
     * @return 是否有效
     */
    bool IsValidSpeed(const MovementInfo& info, const MovementValidationConfig& config);

    /**
     * @brief 验证时间戳
     * @param info 移动信息
     * @param last_valid_time 上次有效时间戳
     * @param config 验证配置
     * @return 是否有效
     */
    bool IsValidTimestamp(const MovementInfo& info, uint64_t last_valid_time, const MovementValidationConfig& config);

    /**
     * @brief 验证地图边界
     * @param info 移动信息
     * @param config 验证配置
     * @return 是否在地图内
     */
    bool IsWithinMapBounds(const MovementInfo& info, const MovementValidationConfig& config);

private:
    const MovementValidationConfig& config_;
};

} // namespace Movement
} // namespace Shared
} // namespace Murim
