#pragma once

#include "Character.hpp"
#include "core/database/CharacterDAO.hpp"
#include "shared/movement/Movement.hpp"
#include <memory>
#include <vector>
#include <optional>

namespace Murim {
namespace Game {

/**
 * @brief 角色管理器
 *
 * 角色系统的业务逻辑层，对应 legacy 的角色管理功能
 * 参考文件: legacy-source-code/[Server]Map/Player.cpp
 */
class CharacterManager {
public:
    static CharacterManager& Instance();

    /**
     * @brief 初始化管理器
     */
    void Initialize();

    /**
     * @brief 更新角色系统
     * @param delta_time 时间增量(毫秒)
     */
    void Update(uint32_t delta_time);

    /**
     * @brief 创建角色
     * @param account_id 账号 ID
     * @param name 角色名
     * @param initial_weapon 初始武器类型
     * @param gender 性别
     * @return 新创建的角色 ID，失败返回 0
     */
    uint64_t CreateCharacter(uint64_t account_id,
                             const std::string& name,
                             uint8_t initial_weapon,
                             uint8_t gender);

    /**
     * @brief 删除角色
     * @param account_id 账号 ID
     * @param character_id 角色 ID
     * @return 是否成功
     */
    bool DeleteCharacter(uint64_t account_id, uint64_t character_id);

    /**
     * @brief 获取角色信息
     * @param character_id 角色 ID
     * @return 角色信息
     */
    std::optional<Character> GetCharacter(uint64_t character_id);

    /**
     * @brief 获取账号下的所有角色
     * @param account_id 账号 ID
     * @return 角色列表
     */
    std::vector<Character> GetAccountCharacters(uint64_t account_id);

    /**
     * @brief 更新角色位置 (无验证)
     * @param character_id 角色 ID
     * @param x, y, z 新位置
     * @param direction 新朝向
     * @return 是否成功
     */
    bool UpdatePosition(uint64_t character_id,
                       float x, float y, float z,
                       uint16_t direction);

    /**
     * @brief 更新角色位置 (带验证)
     * @param character_id 角色 ID
     * @param x, y, z 新位置
     * @param direction 新朝向
     * @param move_state 移动状态
     * @param timestamp 时间戳
     * @return 是否成功
     */
    bool UpdatePositionValidated(uint64_t character_id,
                                 float x, float y, float z,
                                 uint16_t direction,
                                 uint8_t move_state,
                                 uint64_t timestamp);

    /**
     * @brief 保存角色
     * @param character 角色数据
     * @return 是否成功
     */
    bool SaveCharacter(const Character& character);

    /**
     * @brief 检查角色名是否可用
     * @param name 角色名
     * @return 是否可用
     */
    bool IsNameAvailable(const std::string& name);

    /**
     * @brief 检查角色是否存在且有效
     * @param character_id 角色 ID
     * @return 是否存在
     */
    bool IsValidCharacter(uint64_t character_id);

    /**
     * @brief 获取角色属性
     * @param character_id 角色 ID
     * @return 角色属性
     */
    std::optional<CharacterStats> GetCharacterStats(uint64_t character_id);

    /**
     * @brief 更新角色属性
     * @param character_id 角色 ID
     * @param stats 角色属性
     * @return 是否成功
     */
    bool UpdateCharacterStats(uint64_t character_id, const CharacterStats& stats);

    /**
     * @brief 给予角色经验值 - 对应 legacy SetPlayerExpPoint
     * @param character_id 角色 ID
     * @param exp 经验值
     * @return 是否成功
     */
    bool GiveExperience(uint64_t character_id, uint64_t exp);

    /**
     * @brief 给予角色金钱 - 对应 legacy SetMoney
     * @param character_id 角色 ID
     * @param amount 金钱数量
     * @return 是否成功
     */
    bool GiveMoney(uint64_t character_id, uint64_t amount);

    /**
     * @brief 传送角色到指定位置 - 对应 legacy 传送逻辑
     * @param character_id 角色 ID
     * @param map_id 目标地图 ID
     * @param x 目标 X 坐标
     * @param y 目标 Y 坐标
     * @param z 目标 Z 坐标
     * @return 是否成功
     */
    bool Teleport(uint64_t character_id, uint16_t map_id, float x, float y, float z = 0.0f);

    /**
     * @brief 角色登录
     * @param character_id 角色 ID
     * @return 是否成功
     */
    bool CharacterLogin(uint64_t character_id);

    /**
     * @brief 角色登出
     * @param character_id 角色 ID
     * @param play_time 本次游戏时长（秒）
     * @return 是否成功
     */
    bool CharacterLogout(uint64_t character_id, uint32_t play_time);

    /**
     * @brief 验证角色所有权
     * @param character_id 角色 ID
     * @param account_id 账号 ID
     * @return 是否拥有
     */
    bool ValidateOwnership(uint64_t character_id, uint64_t account_id);

    /**
     * @brief 根据职业类型获取默认属性
     * @param job_class 职业类型 (1=战士, 2=法师, 3=弓手)
     * @return 默认属性值
     */
    CharacterStats GetDefaultStats(uint8_t job_class);

private:
    CharacterManager() = default;
    ~CharacterManager() = default;

    // 禁止拷贝
    CharacterManager(const CharacterManager&) = delete;
    CharacterManager& operator=(const CharacterManager&) = delete;

    /**
     * @brief 验证角色名格式
     * @param name 角色名
     * @return 是否有效
     */
    bool IsValidName(const std::string& name);

    /**
     * @brief 初始化新角色属性
     * @param character 角色数据
     * @return 默认属性值
     */
    CharacterStats GetDefaultStats(const Character& character);

    /**
     * @brief 获取指定等级所需经验值 - 对应 legacy GAMERESRCMNGR->GetMaxExpPoint
     * @param level 等级
     * @return 升级所需经验值
     */
    uint64_t GetExpForLevel(uint16_t level);
};

} // namespace Game
} // namespace Murim
