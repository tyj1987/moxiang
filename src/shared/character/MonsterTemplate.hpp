#pragma once

#include <string>
#include <cstdint>
#include <optional>

namespace Murim {
namespace Game {

/**
 * @brief 怪物模板结构
 *
 * 从数据库加载的怪物静态数据
 */
struct MonsterTemplate {
    uint32_t monster_id = 0;          // 怪物ID
    std::string monster_name;         // 怪物名称（中文）
    std::string eng_name;             // 英文名称
    uint16_t level = 1;               // 等级

    // 基础属性
    uint32_t hp = 100;                // 生命值
    uint32_t shield = 0;              // 护盾值
    uint32_t exp_reward = 10;         // 经验值奖励

    // 战斗属性
    uint16_t attack_power_min = 5;    // 最小攻击力
    uint16_t attack_power_max = 8;    // 最大攻击力
    uint16_t critical_percent = 5;    // 暴击率（%）
    uint16_t physical_defense = 3;    // 物理防御

    // 攻击系统（P0 - 核心战斗功能）
    uint8_t attack_kind = 0;          // 攻击种类（0=近战，1=远程）
    uint8_t attack_num = 1;           // 攻击数量（1-5种攻击模式）
    uint32_t default_attack_index = 0; // 默认攻击索引（关联skill_id）

    // AI行为参数（P0 - 核心战斗功能）
    uint32_t search_range = 500;      // 索敌范围（像素）
    uint8_t target_select = 0;        // 目标选择策略（0=最近，1=首个，2=血量最少）
    uint32_t domain_range = 1000;     // 活动范围（像素，怪物离开此范围会返回）

    // 追击参数（P0 - 防止无限追击）
    uint32_t pursuit_forgive_time = 5000;  // 追击原谅时间（毫秒，超时后停止追击）
    uint32_t pursuit_forgive_distance = 1500; // 追击原谅距离（像素，超出此距离停止追击）

    // 移动属性
    uint16_t walk_speed = 100;        // 行走速度
    uint16_t run_speed = 200;         // 奔跑速度
    float scale = 1.0f;               // 缩放比例

    // 外观
    std::string model_file;           // 模型文件路径

    /**
     * @brief 检查模板是否有效
     */
    bool IsValid() const {
        return monster_id != 0 && !monster_name.empty();
    }

    /**
     * @brief 获取平均攻击力
     */
    uint16_t GetAverageAttack() const {
        return (attack_power_min + attack_power_max) / 2;
    }

    /**
     * @brief 计算实际攻击力（在min和max之间随机）
     */
    uint16_t CalculateAttackDamage() const {
        if (attack_power_max <= attack_power_min) {
            return attack_power_min;
        }
        return attack_power_min + (rand() % (attack_power_max - attack_power_min + 1));
    }
};

/**
 * @brief 刷怪点结构
 *
 * 定义地图上的怪物生成位置
 */
struct SpawnPoint {
    uint64_t spawn_id = 0;            // 刷怪点ID
    uint16_t map_id = 1;              // 地图ID
    uint32_t monster_id = 0;          // 怪物模板ID
    float x = 0.0f;                   // X坐标
    float y = 0.0f;                   // Y坐标
    float z = 0.0f;                   // Z坐标
    uint16_t max_count = 1;           // 最大同时存在数量
    uint32_t respawn_time = 30000;    // 重生时间（毫秒）

    /**
     * @brief 检查刷怪点是否有效
     */
    bool IsValid() const {
        return monster_id != 0 && max_count > 0;
    }
};

} // namespace Game
} // namespace Murim
