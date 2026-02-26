#pragma once

#include <cstdint>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>

namespace Murim {
namespace Game {

/**
 * @brief 3D 位置坐标
 */
struct Position {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Position() = default;
    Position(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    /**
     * @brief 计算到另一个位置的距离 (2D)
     */
    float Distance2D(const Position& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    /**
     * @brief 计算到另一个位置的距离 (3D)
     */
    float Distance3D(const Position& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    /**
     * @brief 判断是否在指定范围内
     */
    bool IsInRange(const Position& other, float range) const {
        return Distance2D(other) <= range;
    }

    /**
     * @brief 位置是否有效
     */
    bool IsValid() const {
        return !std::isnan(x) && !std::isnan(y) && !std::isnan(z) &&
               std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    /**
     * @brief 重置为原点
     */
    void Reset() {
        x = y = z = 0.0f;
    }
};

/**
 * @brief 移动信息
 */
struct MovementInfo {
    uint64_t character_id;         // 角色ID
    uint16_t map_id;               // 地图ID
    Position position;             // 当前位置
    uint16_t direction;            // 朝向 (0-360度)
    uint8_t move_state;            // 移动状态 (0=停止, 1=行走, 2=奔跑)

    uint64_t timestamp;            // 时间戳 (milliseconds)

    // 验证数据
    float last_valid_x;            // 上次有效位置 X
    float last_valid_y;            // 上次有效位置 Y
    uint64_t last_update_time;     // 上次更新时间

    MovementInfo() {
        character_id = 0;
        map_id = 0;
        direction = 0;
        move_state = 0;
        timestamp = 0;
        last_valid_x = 0.0f;
        last_valid_y = 0.0f;
        last_update_time = 0;
    }

    /**
     * @brief 获取移动速度 (单位/秒)
     * 根据移动状态返回对应速度
     * 注意: 这是一个简单的基础速度实现
     * 实际游戏中应该通过 MovementManager::GetCharacterSpeed() 获取从角色属性计算的速度
     */
    float GetSpeed() const {
        // 基础速度: 行走 100, 奔跑 200
        // 实际速度会根据角色属性(装备、buff、职业等)进行调整
        switch (move_state) {
            case 1: return 100.0f;  // 行走
            case 2: return 200.0f;  // 奔跑
            default: return 0.0f;   // 停止
        }
    }

    /**
     * @brief 是否正在移动
     */
    bool IsMoving() const {
        return move_state > 0;
    }
};

/**
 * @brief 移动验证结果
 */
enum class MovementValidationResult : uint8_t {
    kValid = 0,              // 有效
    kInvalidPosition = 1,    // 无效位置 (NaN, Inf, 超出边界)
    kTooFast = 2,            // 速度过快
    kTooFar = 3,             // 距离过远 (瞬移)
    kWrongMap = 4,           // 错误的地图
    kWrongDirection = 5,     // 无效的朝向
    kTimestampError = 6,     // 时间戳错误
    kUnknown = 99            // 未知错误
};

/**
 * @brief 移动验证配置
 */
struct MovementValidationConfig {
    float max_walk_speed = 150.0f;      // 最大行走速度 (单位/秒)
    float max_run_speed = 300.0f;       // 最大奔跑速度 (单位/秒)
    float max_teleport_distance = 50.0f; // 最大瞬移距离 (超过此距离判定为瞬移)
    float position_tolerance = 5.0f;    // 位置容差 (允许的误差)
    uint64_t max_timestamp_delta = 5000; // 最大时间戳差 (毫秒)
    bool enable_strict_validation = true; // 是否启用严格验证

    // 地图边界 (默认值,实际应该从地图配置读取)
    float map_min_x = -5000.0f;
    float map_max_x = 5000.0f;
    float map_min_y = -5000.0f;
    float map_max_y = 5000.0f;
    float map_min_z = -1000.0f;
    float map_max_z = 1000.0f;
};

/**
 * @brief 移动统计信息
 */
struct MovementStats {
    uint64_t total_updates = 0;         // 总更新次数
    uint64_t validation_failed = 0;     // 验证失败次数
    uint64_t teleport_detected = 0;     // 检测到瞬移次数
    uint64_t speed_violation = 0;       // 速度违规次数

    float average_speed = 0.0f;         // 平均速度
    float max_observed_speed = 0.0f;    // 观察到的最大速度

    void Reset() {
        total_updates = 0;
        validation_failed = 0;
        teleport_detected = 0;
        speed_violation = 0;
        average_speed = 0.0f;
        max_observed_speed = 0.0f;
    }
};

/**
 * @brief 移动管理器
 *
 * 负责角色移动的验证和同步
 * 对应 legacy: Player 移动相关逻辑
 */
class MovementManager {
public:
    /**
     * @brief 获取单例实例
     */
    static MovementManager& Instance();

    // ========== 移动验证 ==========

    /**
     * @brief 验证移动
     * @param current_info 当前移动信息
     * @param config 验证配置
     * @return 验证结果
     */
    MovementValidationResult ValidateMovement(
        const MovementInfo& current_info,
        const MovementValidationConfig& config = GetDefaultConfig()
    );

    /**
     * @brief 验证位置更新
     * @param character_id 角色ID
     * @param old_position 旧位置
     * @param new_position 新位置
     * @param delta_time 时间差 (秒)
     * @param speed 角色速度
     * @param config 验证配置
     * @return 验证结果
     */
    MovementValidationResult ValidatePositionUpdate(
        uint64_t character_id,
        const Position& old_position,
        const Position& new_position,
        float delta_time,
        float speed,
        const MovementValidationConfig& config = GetDefaultConfig()
    );

    // ========== 移动计算 ==========

    /**
     * @brief 计算移动速度
     * @param from 起始位置
     * @param to 目标位置
     * @param delta_time 时间差 (秒)
     * @return 速度 (单位/秒)
     */
    float CalculateSpeed(
        const Position& from,
        const Position& to,
        float delta_time
    );

    /**
     * @brief 计算移动时间
     * @param from 起始位置
     * @param to 目标位置
     * @param speed 速度 (单位/秒)
     * @return 所需时间 (秒)
     */
    float CalculateMoveTime(
        const Position& from,
        const Position& to,
        float speed
    );

    /**
     * @brief 预测位置 (基于速度和时间)
     * @param from 起始位置
     * @param direction 方向 (弧度)
     * @param speed 速度
     * @param delta_time 时间 (秒)
     * @return 预测位置
     */
    Position PredictPosition(
        const Position& from,
        float direction,
        float speed,
        float delta_time
    );

    // ========== 作弊检测 ==========

    /**
     * @brief 检测速度异常
     * @param calculated_speed 计算出的速度
     * @param expected_speed 预期速度
     * @param tolerance 容差
     * @return 是否异常
     */
    bool IsSpeedAbnormal(
        float calculated_speed,
        float expected_speed,
        float tolerance = 1.5f
    );

    /**
     * @brief 检测瞬移
     * @param distance 移动距离
     * @param max_allowed_distance 最大允许距离
     * @return 是否瞬移
     */
    bool IsTeleport(
        float distance,
        float max_allowed_distance
    );

    // ========== 配置管理 ==========

    /**
     * @brief 获取默认验证配置
     */
    static const MovementValidationConfig& GetDefaultConfig();

    /**
     * @brief 设置验证配置
     */
    void SetValidationConfig(const MovementValidationConfig& config);

    // ========== 统计信息 ==========

    /**
     * @brief 获取角色移动统计
     * @param character_id 角色ID
     */
    MovementStats GetStats(uint64_t character_id);

    /**
     * @brief 重置角色移动统计
     * @param character_id 角色ID
     */
    void ResetStats(uint64_t character_id);

    /**
     * @brief 获取全局统计
     */
    MovementStats GetGlobalStats();

    // ========== 反作弊 ==========

    /**
     * @brief 记录验证失败
     * @param character_id 角色ID
     * @param result 验证结果
     */
    void RecordValidationFailure(uint64_t character_id, MovementValidationResult result);

    /**
     * @brief 检查角色是否应该被踢出 (验证失败过多)
     * @param character_id 角色ID
     * @param threshold 失败次数阈值
     * @return 是否应该踢出
     */
    bool ShouldKick(uint64_t character_id, uint32_t threshold = 10);

private:
    MovementManager() = default;
    ~MovementManager() = default;
    MovementManager(const MovementManager&) = delete;
    MovementManager& operator=(const MovementManager&) = delete;

    MovementValidationConfig config_;
    MovementStats global_stats_;

    // 角色统计信息 (character_id -> stats)
    std::unordered_map<uint64_t, MovementStats> character_stats_;

    /**
     * @brief 从角色属性获取实际移动速度
     * @param character_id 角色ID
     * @param move_state 移动状态 (0=停止, 1=行走, 2=奔跑)
     * @return 实际速度 (单位/秒)
     */
    float GetCharacterSpeed(uint64_t character_id, uint8_t move_state);
};

} // namespace Game
} // namespace Murim
