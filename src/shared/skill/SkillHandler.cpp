#include "SkillHandler.hpp"
#include "core/network/PacketSerializer.hpp"
#include "character/CharacterManager.hpp"
#include "skill/SkillManager.hpp"
#include "item/ItemManager.hpp"
#include "battle/DamageCalculator.hpp"
#include "battle/specialStateManager.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Shared {
namespace Battle {

SkillHandler::SkillHandler(
    std::shared_ptr<Game::Character::Manager> char_manager,
    std::shared_ptr<Game::Skill::Manager> skill_manager,
    std::shared_ptr<Game::Item::Manager> item_manager,
    std::shared_ptr<DamageCalculator> damage_calculator,
    std::shared_ptr<SpecialStateManager> buff_manager)
    : character_manager_(char_manager),
      skill_manager_(skill_manager),
      item_manager_(item_manager),
      damage_calculator_(damage_calculator),
      buff_manager_(buff_manager) {

    spdlog::info("[SkillHandler] Initialized");
}

SkillHandler::~SkillHandler() {
    spdlog::info("[SkillHandler] Destroyed");
}

SkillCastResponse SkillHandler::HandleSkillCast(const SkillCastRequest& request) {
    spdlog::info("[SkillHandler] Processing skill cast request:");
    spdlog::info("  Character ID: {}", request.character_id);
    spdlog::info("  Skill ID: {}", request.skill_id);
    spdlog::info("  Target ID: {}", request.target_id);
    spdlog::info("  Target Type: {}", request.target_type);

    SkillCastResponse response;
    response.result_code = 0;  // 默认成功
    response.skill_id = request.skill_id;
    response.damage = 0;

    // 1. 验证角色
    auto caster = character_manager_->GetCharacter(request.character_id);
    if (!caster.has_value()) {
        spdlog::error("[SkillHandler] Caster not found: {}", request.character_id);
        response.result_code = 4;  // 目标无效
        return response;
    }

    // 2. 验证技能
    auto skill = skill_manager_->GetSkill(request.skill_id);
    if (!skill.has_value()) {
        spdlog::error("[SkillHandler] Skill not found: {}", request.skill_id);
        response.result_code = 3;  // 技能不存在
        return response;
    }

    // 3. 验证技能是否可用
    if (!ValidateSkill(*skill, *caster)) {
        spdlog::warn("[SkillHandler] Skill not ready: cooldown or no MP");
        response.result_code = 1;  // 冷却中
        return response;
    }

    // 4. 验证目标
    auto target = character_manager_->GetCharacter(request.target_id);
    if (!target.has_value()) {
        spdlog::warn("[SkillHandler] Target not found: {}", request.target_id);
        response.result_code = 4;  // 目标无效
        return response;
    }

    // 5. 计算伤害
    uint32_t damage = CalculateDamage(request, *caster, *skill, *target);
    response.damage = damage;

    // 6. 应用技能效果
    ApplySkillEffect(request, *caster, *target, damage);

    // 7. 广播给周围玩家
    BroadcastSkillCast(request, response);

    spdlog::info("[SkillHandler] Skill cast processed:");
    spdlog::info("  Result Code: {}", response.result_code);
    spdlog::info("  Damage: {}", response.damage);

    return response;
}

bool SkillHandler::ValidateSkill(const Game::Skill::Skill& skill,
                                     const Game::GameObject& caster) const {

    // 检查冷却时间
    if (!skill_manager_->IsSkillReady(skill.id_, caster.id_)) {
        spdlog::debug("[SkillHandler] Skill on cooldown: {}", skill.name_);
        return false;
    }

    // 检查MP（技能消耗）
    if (caster.current_mp_ < skill.mp_cost_) {
        spdlog::debug("[SkillHandler] Not enough MP: {} < {}",
                     caster.current_mp_, skill.mp_cost_);
        return false;
    }

    // 检查距离（如果是范围技能）
    if (skill.cast_range_ > 0) {
        // 需要获取目标位置，这里简化处理
        // 实际应用中应该从 CharacterManager 获取
        float distance = 100.0f;  // 假设100米
        if (caster.current_mp_ >= skill.mp_cost_ &&
            distance > skill.cast_range_) {
            spdlog::debug("[SkillHandler] Target out of range: {}m", distance);
            return false;
        }
    }

    spdlog::debug("[SkillHandler] Skill validated: {}", skill.name_);
    return true;
}

uint32_t SkillHandler::CalculateDamage(const SkillCastRequest& request,
                                        const Game::GameObject& caster,
                                        const Game::GameObject::Skill& skill,
                                        const Game::GameObject& target) const {

    // 使用 DamageCalculator 计算伤害
    DamageCalculator::DamageParams params;
    params.attacker_id_ = caster.id_;
    params.target_id_ = target.id_;
    params.skill_id_ = skill.id_;
    params.is_critical_ = false;  // 默认普通攻击

    // 设置伤害计算参数
    damage_calculator_->SetDamageParams(params);

    // 计算伤害
    auto damage_result = damage_calculator_->CalculateDamage();

    if (!damage_result.has_value()) {
        spdlog::error("[SkillHandler] Damage calculation failed");
        return 0;
    }

    return damage_result.value();
}

void SkillHandler::ApplySkillEffect(const SkillCastRequest& request,
                                     const Game::GameObject& caster,
                                     const Game::GameObject& target,
                                     uint32_t damage) {

    spdlog::info("[SkillHandler] Applying skill effect: {} to {} (damage: {})",
                 request.skill_id, request.target_id, damage);

    // 根据技能ID应用不同效果
    switch (request.skill_id) {
        case 1:  // 普通攻击
            // 立即伤害
            if (target.has_value()) {
                auto target_char = std::dynamic_pointer_cast<Game::Character::Character*>(target.shared_from_this());
                if (target_char) {
                    target_char->current_hp_ -= damage;
                    spdlog::info("[SkillHandler] Applied damage: {} to character {} (HP: {}/{})",
                             damage, target.id_, target_char->current_hp_, target_char->max_hp_);
                }
            }
            break;

        case 2:  // 火射性技能
            // 需要实现飞行轨迹
            spdlog::info("[SkillHandler] Projectile skill not fully implemented");
            break;

        case 3:  // 治疗技能
            if (target.has_value()) {
                auto target_char = std::dynamic_pointer_cast<Game::Character::Character*>(target.shared_from_this());
                if (target_char) {
                    uint32_t heal_amount = static_cast<uint32_t>(damage * 0.5f);  // 伤害值的50%作为治疗
                    uint32_t new_hp = std::min(target_char->current_hp_ + heal_amount,
                                               target_char->max_hp_);
                    target_char->current_hp_ = new_hp;
                    spdlog::info("[SkillHandler] Healed character {} ({} + {} = {}/{})",
                             target.id_, damage, heal_amount, target_char->current_hp_, new_hp);
                }
            }
            break;

        default:
            spdlog::warn("[SkillHandler] Unimplemented skill ID: {}", request.skill_id);
            break;
    }
}

void SkillHandler::BroadcastSkillCast(const SkillCastRequest& request,
                                     const SkillCastResponse& response) {

    spdlog::info("[SkillHandler] Broadcasting skill cast:");

    // 获取施法者周围的玩家
    // 简化：从 AOIManager 获取150米内的玩家
    std::vector<uint64_t> nearby_characters;
    // TODO: 实现 AOI 查询
    // nearby_characters = aoi_manager_->GetAOICharacters(caster->x, caster->y, 150.0f);

    // 序列化响应
    std::vector<uint8_t> data;
    data.push_back(0x02);  // SkillCastResponse
    data.push_back(response.result_code);
    data.push_back(response.skill_id);
    data.push_back(static_cast<uint32_t>(response.damage));
    data.push_back(response.remaining_mp);
    data.push_back(static_cast<uint32_t>(response.result_code == 0 ? 1 : 0));  // 是否成功

    // 广播给周围玩家
    for (uint64_t char_id : nearby_characters) {
        auto conn = character_manager_->GetConnectionByCharacterId(char_id);
        if (conn) {
            auto buffer = std::make_shared<Core::Network::ByteBuffer>(data.data(), data.size());
            conn->AsyncSend(buffer, [](const boost::system::error_code& ec, size_t) {
                if (ec) {
                    spdlog::error("[SkillHandler] Failed to broadcast: {}", ec.message());
                }
            });
        }
    }

    spdlog::info("[SkillHandler] Broadcasted to {} characters", nearby_characters.size());
}

} // namespace Battle
} // namespace Shared
} // namespace Murim
