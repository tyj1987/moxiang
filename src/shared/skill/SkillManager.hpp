#pragma once

#include "Skill.hpp"
#include "../battle/DamageCalculator.hpp"
#include "../battle/SpecialStateManager.hpp"
#include "../movement/Movement.hpp"
#include <unordered_map>
#include <memory>
#include <chrono>
#include "core/spdlog_wrapper.hpp"
#include <vector>
#include <string>

namespace Murim {
namespace Game {

/**
 * @brief 技能管理器
 *
 * 派责：
 * - 加载技能数据
 * - 管理技能冷却
 * - 验证技能使用条件
 * - 对应 legacy: CSkillManager::ProcessSkill()
 */
class SkillManager {
public:
    /**
     * @brief 获取单例实例
     */
    static SkillManager& Instance();

    /**
     * @brief 初始化技能管理器
     */
    bool Initialize();

    /**
     * @brief 更新技能系统
     * @param delta_time 时间增量(毫秒)
     */
    void Update(uint32_t delta_time);

    /**
     * @brief 析构函数
     */
    ~SkillManager() = default;

    /**
     * @brief 获取技能数据
     * @param skill_id 技能ID
     * @return 技能数据（如果存在）
     */
    std::shared_ptr<Skill> GetSkill(uint32_t skill_id) const;

    /**
     * @brief 检查技能是否可用
     * @param skill 技能数据
     * @param caster 释放者
     * @param target 目标
     * @return 是否可用
     */
    bool IsSkillReady(const Skill& skill,
                       const GameObject& caster,
                       const GameObject& target) const;

    /**
     * @brief 检查技能冷却
     * @param skill_id 技能ID
     * @param caster_id 释放者ID
     * @return 是否冷却中
     */
    bool IsOnCooldown(uint32_t skill_id, uint64_t caster_id) const;

    /**
     * @brief 添加技能冷却
     * @param skill_id 技能ID
     * @param caster_id 释放者ID
     * @param cooldown_ms 冷却时间（毫秒）
     */
    void AddCooldown(uint32_t skill_id, uint64_t caster_id, uint32_t cooldown_ms);

    /**
     * @brief 减少技能冷却
     * @param skill_id 技能ID
     * @param caster_id 释放者ID
     * @param amount_ms 减少量（毫秒）
     */
    void ReduceCooldown(uint32_t skill_id, uint64_t caster_id, uint32_t amount_ms);

    /**
     * @brief 根据角色获取可用技能列表
     * @param character_id 角色ID
     * @return 技能ID列表
     */
    std::vector<uint32_t> GetCharacterSkills(uint64_t character_id);

    /**
     * @brief 获取技能学习前置要求
     * @param skill_id 技能ID
     * @return 前置技能列表
     */
    std::vector<uint32_t> GetRequiredSkills(uint32_t skill_id);

    /**
     * @brief 学习技能
     * @param character_id 角色ID
     * @param skill_id 技能ID
     * @return 是否成功
     */
    static bool LearnSkill(uint64_t character_id, uint32_t skill_id);

    /**
     * @brief 技能施放结果
     */
    struct CastResult {
        bool success;
        std::vector<BattleEvent> battle_events;
        std::string error_message;
    };

    /**
     * @brief 施放技能
     * @param caster_id 施法者ID
     * @param skill_id 技能ID
     * @param target_ids 目标ID列表
     * @param position 施法位置
     * @param direction 朝向
     * @return 施放结果
     */
    static CastResult CastSkill(
        uint64_t caster_id,
        uint32_t skill_id,
        const std::vector<uint64_t>& target_ids,
        const Position& position,
        uint16_t direction
    );

    /**
     * @brief 检查技能冷却
     * @param caster_id 施法者ID
     * @param skill_id 技能ID
     * @return 剩余冷却时间(秒), 0表示可用
     */
    static uint16_t GetSkillCooldown(uint64_t caster_id, uint32_t skill_id);

    /**
     * @brief 开始技能冷却
     * @param caster_id 施法者ID
     * @param skill_id 技能ID
     */
    static void StartSkillCooldown(uint64_t caster_id, uint32_t skill_id);

    /**
     * @brief 更新冷却状态
     * @param skill_id 技能ID
     * @param caster_id 释放者ID
     */
    void UpdateCooldown(uint32_t skill_id, uint64_t caster_id);

    /**
     * @brief 检查目标是否在技能范围内
     * @param caster_pos 施法者位置
     * @param target_pos 目标位置
     * @param skill 技能数据
     * @return 是否在范围内
     */
    static bool IsTargetInRange(
        const Position& caster_pos,
        const Position& target_pos,
        const Skill& skill
    );

    /**
     * @brief 获取范围内的所有目标
     * @param caster_pos 施法者位置
     * @param skill 技能数据
     * @param all_targets 所有潜在目标
     * @return 在范围内的目标ID列表
     */
    static std::vector<uint64_t> GetTargetsInRange(
        const Position& caster_pos,
        const Skill& skill,
        const std::vector<uint64_t>& all_targets
    );

private:
    // ========== 单例模式 ==========
    SkillManager();
    SkillManager(const SkillManager&) = delete;
    SkillManager& operator=(const SkillManager&) = delete;

    // ========== 内部类型 ==========

    /**
     * @brief 技能冷却记录
     */
    struct SkillCooldownRecord {
        uint64_t caster_id;
        uint32_t skill_id;
        std::chrono::system_clock::time_point start_time;
        uint16_t cooldown_duration;

        /**
         * @brief 获取剩余冷却时间(秒)
         */
        uint16_t GetRemainingCooldown() const {
            auto now = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            if (elapsed >= cooldown_duration) {
                return 0;
            }
            return static_cast<uint16_t>(cooldown_duration - elapsed);
        }

        /**
         * @brief 是否冷却完成
         */
        bool IsReady() const {
            return GetRemainingCooldown() == 0;
        }
    };

    // ========== 数据存储 ==========

    /**
     * @brief 技能数据缓存 (skill_id -> Skill)
     */
    std::unordered_map<uint32_t, std::shared_ptr<Skill>> skills_;

    /**
     * @brief 是否已初始化
     */
    bool initialized_ = false;

    /**
     * @brief 加载技能数据
     */
    void LoadSkills();

    /**
     * @brief 技能冷却状态 (caster_id -> (skill_id -> SkillCooldownRecord))
     */
    std::unordered_map<uint64_t, std::unordered_map<uint32_t, SkillCooldownRecord>> cooldowns_;

    /**
     * @brief 全局冷却记录
     */
    struct GlobalCooldownRecord {
        uint64_t caster_id;
        std::chrono::system_clock::time_point start_time;
        uint16_t cooldown_duration;

        /**
         * @brief 获取剩余冷却时间(秒)
         */
        uint16_t GetRemainingCooldown() const {
            auto now = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            if (elapsed >= cooldown_duration) {
                return 0;
            }
            return static_cast<uint16_t>(cooldown_duration - elapsed);
        }

        /**
         * @brief 是否冷却完成
         */
        bool IsReady() const {
            return GetRemainingCooldown() == 0;
        }
    };

    /**
     * @brief 全局冷却记录 (caster_id -> GlobalCooldownRecord)
     */
    std::unordered_map<uint64_t, GlobalCooldownRecord> global_cooldowns_;

    /**
     * @brief 清理过期冷却记录
     */
    void CleanupExpiredCooldowns();
};

} // namespace Game
} // namespace Murim
