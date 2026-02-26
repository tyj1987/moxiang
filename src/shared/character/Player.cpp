#include "Player.hpp"
#include "map-server/managers/EntityManager.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

Player::Player(MapServer::GameEntity* game_entity, Character* character)
    : game_entity_(game_entity)
    , character_(character)
{
    if (game_entity_ && game_entity_->user_data) {
        // 如果提供了GameEntity但没提供Character，尝试从user_data提取
        if (!character_) {
            character_ = static_cast<Character*>(game_entity_->user_data);
        }
    }

    if (IsValid()) {
        spdlog::debug("[Player] Created player: entity_id={}, char_id={}, name={}",
                      game_entity_ ? game_entity_->entity_id : 0,
                      character_ ? character_->character_id : 0,
                      character_ ? character_->name : "null");
    }
}

Player::~Player() {
    spdlog::trace("[Player] Destroyed player: {}", GetName());
}

uint64_t Player::GetEntityId() const {
    if (game_entity_) {
        return game_entity_->entity_id;
    }
    if (character_) {
        return character_->character_id;
    }
    return 0;
}

void Player::GetPosition(float* x, float* y, float* z) const {
    if (game_entity_) {
        // 优先从GameEntity获取位置（实时位置）
        if (x) *x = game_entity_->position.x;
        if (y) *y = game_entity_->position.y;
        if (z) *z = game_entity_->position.z;
    } else if (character_) {
        // 备选：从Character获取位置
        if (x) *x = character_->x;
        if (y) *y = character_->y;
        if (z) *z = character_->z;
    } else {
        // 默认值
        if (x) *x = 0.0f;
        if (y) *y = 0.0f;
        if (z) *z = 0.0f;
    }
}

bool Player::IsAlive() const {
    if (character_) {
        return character_->IsAlive();
    }
    return true;  // 默认存活
}

uint32_t Player::GetHP() const {
    if (character_) {
        return character_->hp;
    }
    return 100;  // 默认值
}

uint32_t Player::GetMaxHP() const {
    if (character_) {
        return character_->max_hp;
    }
    return 100;  // 默认值
}

uint16_t Player::GetLevel() const {
    if (character_) {
        return character_->level;
    }
    return 1;  // 默认值
}

uint32_t Player::TakeDamage(uint32_t damage) {
    if (!character_) {
        return 0;  // 无效角色，不受伤害
    }

    uint32_t old_hp = character_->hp;

    // 应用伤害
    if (damage >= character_->hp) {
        character_->hp = 0;
        // TODO: 触发死亡事件
        spdlog::debug("[Player] {} died from damage ({} HP)", GetName(), old_hp);
    } else {
        character_->hp -= damage;
        spdlog::debug("[Player] {} took {} damage ({} -> {} HP)",
                      GetName(), damage, old_hp, character_->hp);
    }

    // 返回实际伤害
    return old_hp - character_->hp;
}

const std::string& Player::GetName() const {
    if (character_) {
        return character_->name;
    }
    static const std::string empty_name = "";
    return empty_name;
}

std::shared_ptr<Player> Player::Create(
    MapServer::GameEntity* game_entity,
    Character* character
) {
    if (!game_entity && !character) {
        return nullptr;
    }

    // 使用shared_ptr构造，但注意这里不管理game_entity和character的生命周期
    // 它们由EntityManager和CharacterManager管理
    return std::shared_ptr<Player>(new Player(game_entity, character));
}

std::shared_ptr<Player> Player::FromGameEntity(MapServer::GameEntity* game_entity) {
    if (!game_entity) {
        return nullptr;
    }

    // 尝试从user_data提取Character
    Character* character = nullptr;
    if (game_entity->user_data) {
        character = static_cast<Character*>(game_entity->user_data);
    }

    return Create(game_entity, character);
}

} // namespace Game
} // namespace Murim
