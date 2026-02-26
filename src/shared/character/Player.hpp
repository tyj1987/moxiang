#pragma once

#include <memory>
#include "Character.hpp"

namespace Murim {

namespace MapServer {
struct GameEntity;
} // namespace MapServer

namespace Game {

/**
 * @brief Entity基类（GameObject的简化版本）
 *
 * 用于MonsterAI的通用接口，避免直接依赖GameObject
 */
class Entity {
public:
    virtual ~Entity() = default;

    // 获取实体ID
    virtual uint64_t GetEntityId() const = 0;

    // 获取位置
    virtual void GetPosition(float* x, float* y, float* z) const = 0;

    // 检查是否存活
    virtual bool IsAlive() const = 0;

    // 获取HP
    virtual uint32_t GetHP() const = 0;
    virtual uint32_t GetMaxHP() const = 0;

    // 获取等级
    virtual uint16_t GetLevel() const = 0;

    // ========== 伤害管理 ==========

    /**
     * @brief 受到伤害
     * @param damage 伤害值
     * @return 实际受到的伤害
     */
    virtual uint32_t TakeDamage(uint32_t damage) = 0;
};

/**
 * @brief Player类 - 玩家对象
 *
 * 包装GameEntity和Character，提供统一的玩家访问接口
 * 对应legacy: CPlayerCharacter
 */
class Player : public Entity {
public:
    /**
     * @brief 构造函数
     * @param game_entity MapServer的GameEntity指针（可以为null）
     * @param character 角色数据指针（可以为null）
     */
    Player(MapServer::GameEntity* game_entity, Character* character);

    /**
     * @brief 析构函数
     */
    ~Player() override;

    // ========== Entity接口实现 ==========

    uint64_t GetEntityId() const override;
    void GetPosition(float* x, float* y, float* z) const override;
    bool IsAlive() const override;
    uint32_t GetHP() const override;
    uint32_t GetMaxHP() const override;
    uint16_t GetLevel() const override;

    // ========== Player特有方法 ==========

    /**
     * @brief 获取角色名称
     */
    const std::string& GetName() const;

    /**
     * @brief 获取角色数据
     */
    const Character* GetCharacter() const { return character_; }
    Character* GetCharacter() { return character_; }

    /**
     * @brief 获取GameEntity
     */
    MapServer::GameEntity* GetGameEntity() const { return game_entity_; }

    /**
     * @brief 检查是否有效（非null且数据有效）
     */
    bool IsValid() const { return game_entity_ != nullptr || character_ != nullptr; }

    /**
     * @brief 受到伤害
     * @param damage 伤害值
     * @return 实际受到的伤害（如果HP不足则受到实际HP的伤害）
     */
    uint32_t TakeDamage(uint32_t damage) override;

    /**
     * @brief 创建Player的shared_ptr
     * @param game_entity GameEntity指针
     * @param character Character指针
     * @return Player的shared_ptr，如果输入都为null则返回nullptr
     */
    static std::shared_ptr<Player> Create(
        MapServer::GameEntity* game_entity,
        Character* character
    );

    /**
     * @brief 从GameEntity创建Player（假设user_data指向Character）
     * @param game_entity GameEntity指针
     * @return Player的shared_ptr，如果输入为null或user_data无效则返回nullptr
     */
    static std::shared_ptr<Player> FromGameEntity(MapServer::GameEntity* game_entity);

private:
    MapServer::GameEntity* game_entity_;  // GameEntity指针（不管理生命周期）
    Character* character_;                 // Character指针（不管理生命周期）
};

} // namespace Game
} // namespace Murim
