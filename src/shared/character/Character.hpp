#pragma once

#include <string>
#include <cstdint>
#include <chrono>
#include <optional>

namespace Murim {
namespace Game {

/**
 * @brief 角色状态枚举
 */
enum class CharacterStatus : uint8_t {
    kAlive = 0,      // 存活状态
    kDead = 1,       // 死亡状态
    kGhost = 2       // 亡魂状态（可选）
};

struct Character {
    uint64_t character_id = 0;
    uint64_t account_id = 0;
    std::string name;
    uint8_t job_class = 0;  // 职业类型: 0=未知, 1=战士, 2=法师, 3=弓手
    uint8_t initial_weapon = 1;
    uint8_t gender = 0;

    uint16_t level = 1;
    uint64_t exp = 0;

    uint64_t money = 0;

    uint32_t hp = 100;
    uint32_t max_hp = 100;
    uint32_t mp = 50;
    uint32_t max_mp = 50;
    uint32_t stamina = 100;
    uint32_t max_stamina = 100;

    uint16_t face_style = 0;
    uint16_t hair_style = 0;
    uint32_t hair_color = 0;

    uint16_t map_id = 1;
    float x = 100.0f;
    float y = 100.0f;
    float z = 0.0f;
    uint16_t direction = 0;

    std::chrono::system_clock::time_point create_time;
    std::chrono::system_clock::time_point last_login_time;
    uint32_t total_play_time = 0;

    CharacterStatus status = CharacterStatus::kAlive;

    // 死亡时间（用于复活计时）
    std::chrono::system_clock::time_point death_time;

    // 最后复活时间（用于复活冷却）
    std::chrono::system_clock::time_point last_revive_time;

    // 复活保护时间（复活后无敌时间）
    std::chrono::system_clock::time_point revive_protection_time;

    bool IsValid() const {
        return character_id != 0;
    }

    bool IsOnline() const {
        return true;  // 在线状态由session管理，不使用status字段
    }

    bool IsAlive() const {
        return status == CharacterStatus::kAlive;
    }

    bool IsDead() const {
        return status == CharacterStatus::kDead;
    }

    /**
     * @brief 检查是否有复活保护
     * @param protection_seconds 保护时间（秒），默认3秒
     * @return true表示在保护期内，false表示无保护
     */
    bool IsReviveProtected(uint32_t protection_seconds = 3) const {
        if (revive_protection_time.time_since_epoch().count() == 0) {
            return false;  // 从未复活过，无保护
        }

        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - revive_protection_time);
        return elapsed.count() < protection_seconds;
    }

    /**
     * @brief 获取复活保护剩余时间（秒）
     * @param protection_seconds 保护时间（秒），默认3秒
     * @return 剩余秒数，0表示无保护
     */
    uint32_t GetReviveProtectionRemaining(uint32_t protection_seconds = 3) const {
        if (revive_protection_time.time_since_epoch().count() == 0) {
            return 0;  // 从未复活过，无保护
        }

        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - revive_protection_time);

        if (elapsed.count() >= protection_seconds) {
            return 0;  // 保护已结束
        }

        return protection_seconds - static_cast<uint32_t>(elapsed.count());
    }

    /**
     * @brief 检查复活冷却是否完成
     * @param cooldown_seconds 冷却时间（秒）
     * @return true表示冷却中，false表示可以复活
     */
    bool IsReviveCoolingDown(uint32_t cooldown_seconds = 60) const {
        if (last_revive_time.time_since_epoch().count() == 0) {
            return false;  // 从未复活过，无冷却
        }

        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_revive_time);
        return elapsed.count() < cooldown_seconds;
    }

    /**
     * @brief 获取复活冷却剩余时间（秒）
     * @param cooldown_seconds 冷却时间（秒）
     * @return 剩余秒数，0表示无冷却
     */
    uint32_t GetReviveCooldownRemaining(uint32_t cooldown_seconds = 60) const {
        if (last_revive_time.time_since_epoch().count() == 0) {
            return 0;  // 从未复活过，无冷却
        }

        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_revive_time);

        if (elapsed.count() >= cooldown_seconds) {
            return 0;  // 冷却已完成
        }

        return cooldown_seconds - static_cast<uint32_t>(elapsed.count());
    }
};

struct CharacterStats {
    uint16_t strength = 10;
    uint16_t intelligence = 10;
    uint16_t dexterity = 10;
    uint16_t vitality = 10;
    uint16_t wisdom = 10;

    uint32_t physical_attack = 0;
    uint32_t physical_defense = 0;
    uint32_t magic_attack = 0;
    uint32_t magic_defense = 0;

    uint16_t attack_speed = 100;
    uint16_t move_speed = 100;
    uint16_t critical_rate = 5;
    uint16_t dodge_rate = 5;
    uint16_t hit_rate = 90;

    uint16_t available_points = 0;

    uint32_t CalculatePhysicalAttack() const {
        return physical_attack + (strength * 2);
    }

    uint32_t CalculatePhysicalDefense() const {
        return physical_defense + (vitality * 2) + (dexterity / 2);
    }

    bool AddPoints(uint16_t strength_delta, uint16_t intelligence_delta,
                   uint16_t dexterity_delta, uint16_t vitality_delta, uint16_t wisdom_delta) {
        uint16_t total = strength_delta + intelligence_delta + dexterity_delta + vitality_delta + wisdom_delta;
        if (total > available_points) {
            return false;
        }

        this->strength += strength_delta;
        this->intelligence += intelligence_delta;
        this->dexterity += dexterity_delta;
        this->vitality += vitality_delta;
        this->wisdom += wisdom_delta;
        this->available_points -= total;

        return true;
    }
};

} // namespace Game
} // namespace Murim
