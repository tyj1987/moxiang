#include "CharacterManager.hpp"
#include "shared/movement/Movement.hpp"
#include "core/network/NotificationService.hpp"
#include "core/database/CharacterDAO.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <cmath>

using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;
using CharacterDAO = Murim::Core::Database::CharacterDAO;

namespace Murim {
namespace Game {

CharacterManager& CharacterManager::Instance() {
    static CharacterManager instance;
    return instance;
}

void CharacterManager::Initialize() {
    spdlog::info("CharacterManager initialized");
}

void CharacterManager::Update(uint32_t delta_time) {
    // 处理角色生命值、内力恢复等 (暂时为空)
    (void)delta_time;
}

uint64_t CharacterManager::CreateCharacter(uint64_t account_id,
                                        const std::string& name,
                                        uint8_t initial_weapon,
                                        uint8_t gender) {
    // 验证输入
    if (account_id == 0) {
        spdlog::error("Invalid account_id");
        return 0;
    }

    if (!IsValidName(name)) {
        spdlog::error("Invalid character name: {}", name);
        return 0;
    }

    // 验证职业类型（只允许 1-3）
    if (initial_weapon < 1 || initial_weapon > 3) {
        spdlog::error("Invalid job_class: {}", initial_weapon);
        return 0;
    }

    // 检查名称是否已存在
    CharacterDAO dao;
    if (dao.Exists(name)) {
        spdlog::warn("Character name already exists: {}", name);
        return 0;
    }

    // 创建角色对象
    Character character;
    character.account_id = account_id;
    character.name = name;
    character.initial_weapon = initial_weapon;
    character.gender = gender;
    character.level = 1;
    character.exp = 0;

    // 根据 initial_weapon 设置 job_class 和对应的 HP/MP
    // 映射: 1=战士, 2=法师, 3=刺客
    switch (initial_weapon) {
        case 1:  // 战士
            character.job_class = 1;
            character.hp = 150;
            character.max_hp = 150;
            character.mp = 80;
            character.max_mp = 80;
            break;
        case 2:  // 法师
            character.job_class = 2;
            character.hp = 100;
            character.max_hp = 100;
            character.mp = 150;
            character.max_mp = 150;
            break;
        case 3:  // 刺客
            character.job_class = 3;
            character.hp = 120;
            character.max_hp = 120;
            character.mp = 100;
            character.max_mp = 100;
            break;
    }

    character.stamina = 100;
    character.max_stamina = 100;
    character.face_style = 0;
    character.hair_style = 0;
    character.hair_color = 0;
    character.map_id = 1;  // 新手村
    character.x = 100.0f;
    character.y = 100.0f;
    character.z = 0.0f;
    character.direction = 0;
    character.status = Game::CharacterStatus::kAlive;

    // 设置创建时间
    character.create_time = std::chrono::system_clock::now();
    character.last_login_time = character.create_time;

    try {
        uint64_t character_id = dao.Create(character);
        spdlog::info("Character created: id={}, name={}, weapon={}, gender={}",
                    character_id, name, initial_weapon, gender);
        return character_id;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create character: {}", e.what());
        return 0;
    }
}

bool CharacterManager::DeleteCharacter(uint64_t account_id, uint64_t character_id) {
    // 验证所有权
    if (!ValidateOwnership(character_id, account_id)) {
        spdlog::warn("Character {} does not belong to account {}", character_id, account_id);
        return false;
    }

    CharacterDAO dao;
    return dao.Delete(character_id);
}

std::optional<Character> CharacterManager::GetCharacter(uint64_t character_id) {
    CharacterDAO dao;
    return dao.Load(character_id);
}

std::vector<Character> CharacterManager::GetAccountCharacters(uint64_t account_id) {
    CharacterDAO dao;
    return dao.LoadByAccount(account_id);
}

bool CharacterManager::UpdatePosition(uint64_t character_id,
                                   float x, float y, float z,
                                   uint16_t direction) {
    // 对应 legacy: 移动位置更新逻辑
    // legacy中移动验证在MovementManager中进行，这里主要更新数据库
    // 基本范围检查 - 防止越界
    const float MAP_SIZE = 51200.0f;  // 对应 legacy地图大小
    if (x < 0 || x > MAP_SIZE || y < 0 || y > MAP_SIZE || z < 0 || z > MAP_SIZE) {
        spdlog::warn("Invalid position for character {}: ({},{},{})",
                    character_id, x, y, z);
        return false;
    }

    // 更新位置到数据库
    CharacterDAO dao;
    return dao.UpdatePosition(character_id, x, y, z, direction);
}

bool CharacterManager::SaveCharacter(const Character& character) {
    CharacterDAO dao;
    return dao.Save(character);
}

bool CharacterManager::IsNameAvailable(const std::string& name) {
    if (!IsValidName(name)) {
        return false;
    }

    CharacterDAO dao;
    return !dao.Exists(name);
}

bool CharacterManager::IsValidCharacter(uint64_t character_id) {
    auto character = GetCharacter(character_id);
    return character.has_value() && character->IsValid();
}

std::optional<CharacterStats> CharacterManager::GetCharacterStats(uint64_t character_id) {
    CharacterDAO dao;
    return dao.LoadStats(character_id);
}

bool CharacterManager::UpdateCharacterStats(uint64_t character_id,
                                          const CharacterStats& stats) {
    CharacterDAO dao;
    return dao.SaveStats(character_id, stats);
}

bool CharacterManager::GiveExperience(uint64_t character_id, uint64_t exp) {
    // 对应 legacy CPlayer::SetPlayerExpPoint
    try {
        // 获取角色信息
        auto character = GetCharacter(character_id);
        if (!character.has_value()) {
            spdlog::error("Character {} not found for GiveExperience", character_id);
            return false;
        }

        // 检查等级上限 - 对应 legacy MAX_CHARACTER_LEVEL_NUM
        const uint16_t MAX_LEVEL = 120;  // 对应 legacy MAX_CHARACTER_LEVEL_NUM
        if (character->level >= MAX_LEVEL) {
            spdlog::debug("Character {} already at max level {}, cannot add exp",
                          character_id, MAX_LEVEL);
            return false;
        }

        // 添加经验值 - 对应 legacy m_HeroInfo.ExpPoint += exp
        character->exp += exp;

        // 检查是否升级 - 对应 legacy SetPlayerExpPoint 升级逻辑
        // 获取升级所需经验 - 对应 legacy: GAMERESRCMNGR->GetMaxExpPoint(GetLevel())
        // 简化实现，使用内部经验值
        // Legacy中使用配置文件中的经验表,这里使用简化的公式计算
        uint64_t exp_for_level = GetExpForLevel(character->level);

        while (character->exp >= exp_for_level && character->level < MAX_LEVEL) {
            // 升级 - 对应 legacy LevelUp
            character->exp -= exp_for_level;
            character->level++;
            exp_for_level = GetExpForLevel(character->level);

            spdlog::info("Character {} leveled up to {}", character_id, character->level);

            // 发送升级消息到客户端 - 对应 legacy 升级通知
            Core::Network::NotificationService::Instance().NotifyLevelUp(character_id, character->level);

            // 触发升级事件 - 对应 legacy CObjectEvent::ObjectEvent(this, OE_LEVELUP)
            // Legacy中升级事件会触发:
            // 1. 队伍成员等级更新
            // 2. 公会成员等级更新
            // 3. 任务系统检查
            // 4. 其他相关系统通知
            // 这里简化为日志记录,实际实现需要调用PartyManager和GuildManager
            spdlog::debug("Level up event triggered for character {}, new level: {}",
                         character_id, character->level);

            // 更新队伍/公会成员等级信息 - 对应 legacy Party/Guild等级同步
            // Legacy中通过PARTYMGR和GUILDMGR同步成员等级
            // 这里简化为日志记录,实际实现需要调用PartyManager和GuildManager
            spdlog::trace("Syncing level {} to party/guild for character {}",
                         character->level, character_id);
        }

        // 保存角色数据
        bool success = SaveCharacter(character.value());

        if (success) {
            spdlog::info("Character {} gained {} exp, total exp: {}, level: {}",
                        character_id, exp, character->exp, character->level);

            // 发送经验更新消息到客户端 - 对应 legacy 经验通知
            uint64_t exp_for_next_level = GetExpForLevel(character->level);
            Core::Network::NotificationService::Instance().NotifyExpUpdate(
                character_id, character->exp, exp_for_next_level);

            // 记录日志 - 对应 legacy InsertLogExp
            // Legacy中记录经验获取日志到数据库，用于防作弊和统计
            // 这里简化为日志记录
            spdlog::trace("Experience log: character {} gained {} exp, total: {}, level: {}",
                         character_id, exp, character->exp, character->level);
        }

        return success;
    } catch (const std::exception& e) {
        spdlog::error("Failed to give experience to character {}: {}", character_id, e.what());
        return false;
    }
}

bool CharacterManager::GiveMoney(uint64_t character_id, uint64_t amount) {
    // 对应 legacy CPlayer::SetMoney(MONEY_ADDITION)
    try {
        // 获取角色信息
        auto character = GetCharacter(character_id);
        if (!character.has_value()) {
            spdlog::error("Character {} not found for GiveMoney", character_id);
            return false;
        }

        // 检查金钱上限 - 对应 legacy MAXMONEY (0xffffffff)
        const uint64_t MAX_MONEY = 4294967295ULL;
        if (character->money + amount > MAX_MONEY) {
            spdlog::warn("Character {} money would exceed MAX_MONEY, capping at {}",
                        character_id, MAX_MONEY);
            character->money = MAX_MONEY;
        } else {
            // 添加金钱 - 对应 legacy m_HeroInfo.Money += addmoney
            character->money += amount;
        }

        // 保存角色数据
        bool success = SaveCharacter(character.value());

        if (success) {
            spdlog::info("Character {} gained {} money, total money: {}",
                        character_id, amount, character->money);

            // 发送金钱更新消息到客户端 - 对应 legacy MSG_MONEY
            Core::Network::NotificationService::Instance().NotifyMoneyUpdate(character_id, character->money);

            // 记录日志 - 对应 legacy InsertLogMoney
            // Legacy中记录金钱获取日志到数据库，用于防作弊和统计
            // 这里简化为日志记录
            spdlog::trace("Money log: character {} gained {} money, total: {}",
                         character_id, amount, character->money);
        }

        return success;
    } catch (const std::exception& e) {
        spdlog::error("Failed to give money to character {}: {}", character_id, e.what());
        return false;
    }
}

bool CharacterManager::Teleport(uint64_t character_id, uint16_t map_id, float x, float y, float z) {
    // 对应 legacy 传送逻辑
    try {
        // 获取角色信息
        auto character = GetCharacter(character_id);
        if (!character.has_value()) {
            spdlog::error("Character {} not found for Teleport", character_id);
            return false;
        }

        // 更新位置和地图 - 对应 legacy SetPosition / MapMove
        uint16_t old_map_id = character->map_id;
        float old_x = character->x;
        float old_y = character->y;
        float old_z = character->z;

        character->map_id = map_id;
        character->x = x;
        character->y = y;
        character->z = z;

        // 保存角色数据
        bool success = SaveCharacter(character.value());

        if (success) {
            spdlog::info("Character {} teleported from map({},{},{}) to map({},{},{})",
                        character_id, old_map_id, old_x, old_y, map_id, x, y);

            // 发送传送消息到客户端 - 对应 legacy 传送通知
            Core::Network::NotificationService::Instance().NotifyTeleport(character_id, map_id, x, y, z);

            // 如果切换地图,需要通知MapServer - 对应 legacy MapMove
            // Legacy中地图切换会:
            // 1. 从当前MapServer移除角色
            // 2. 通知目标MapServer准备接收
            // 3. 发送地图切换消息给客户端
            // 4. 客户端连接到新MapServer
            // 这里简化为日志记录,实际实现需要MapServer间通信
            if (old_map_id != map_id) {
                spdlog::info("Character {} changing map from {} to {}",
                            character_id, old_map_id, map_id);
                // TODO: 实现MapServer间通信
            }

            // 记录日志 - 对应 legacy传送日志
            spdlog::trace("Teleport log: character {} from ({},{},{}) map{} to ({},{},{}) map{}",
                         character_id, old_x, old_y, old_z, old_map_id, x, y, z, map_id);
        }

        return success;
    } catch (const std::exception& e) {
        spdlog::error("Failed to teleport character {}: {}", character_id, e.what());
        return false;
    }
}

uint64_t CharacterManager::GetExpForLevel(uint16_t level) {
    // 对应 legacy GAMERESRCMNGR->GetMaxExpPoint(level)
    // 简化的经验公式,实际应该从配置读取
    // Legacy经验表中每级所需经验呈指数增长
    if (level >= 120) {
        return UINT64_MAX;  // 满级
    }

    // 简化的经验公式: base * (1.1 ^ (level-1))
    // 实际应该使用legacy的经验表
    const uint64_t base_exp = 1000;
    return static_cast<uint64_t>(base_exp * std::pow(1.1, level - 1));
}

bool CharacterManager::CharacterLogin(uint64_t character_id) {
    if (!IsValidCharacter(character_id)) {
        return false;
    }

    CharacterDAO dao;
    return dao.UpdateLastLoginTime(character_id);
}

bool CharacterManager::CharacterLogout(uint64_t character_id, uint32_t play_time) {
    if (!IsValidCharacter(character_id)) {
        return false;
    }

    CharacterDAO dao;
    return dao.UpdatePlayTime(character_id, play_time);
}

bool CharacterManager::ValidateOwnership(uint64_t character_id, uint64_t account_id) {
    CharacterDAO dao;
    return dao.BelongsToAccount(character_id, account_id);
}

bool CharacterManager::IsValidName(const std::string& name) {
    // 检查长度（最小 3 字符，最大 24 字符）
    if (name.empty() || name.length() < 3 || name.length() > 24) {
        return false;
    }

    // 检查字符（只允许字母、数字、下划线）
    // 简化版本，实际需要更严格的验证（包括中文支持）
    for (char c : name) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }

    return true;
}

CharacterStats CharacterManager::GetDefaultStats(const Character& character) {
    CharacterStats stats;

    // 根据初始武器类型设置默认属性
    switch (character.initial_weapon) {
        case 0:  // 内功 - 智慧和智力为主
            stats.strength = 5;
            stats.vitality = 10;
            stats.dexterity = 8;
            stats.intelligence = 16;
            stats.wisdom = 18;
            break;
        case 1:  // 剑 - 平衡型
            stats.strength = 13;
            stats.vitality = 12;
            stats.dexterity = 13;
            stats.intelligence = 8;
            stats.wisdom = 8;
            break;
        case 2:  // 刀 - 高力量和体力
            stats.strength = 16;
            stats.vitality = 14;
            stats.dexterity = 10;
            stats.intelligence = 5;
            stats.wisdom = 5;
            break;
        case 3:  // 枪 - 高力量和敏捷
            stats.strength = 15;
            stats.vitality = 10;
            stats.dexterity = 15;
            stats.intelligence = 6;
            stats.wisdom = 6;
            break;
        case 4:  // 拳套 - 高敏捷和体力
            stats.strength = 12;
            stats.vitality = 14;
            stats.dexterity = 16;
            stats.intelligence = 5;
            stats.wisdom = 5;
            break;
        case 5:  // 弓 - 高敏捷和智力
            stats.strength = 10;
            stats.vitality = 8;
            stats.dexterity = 16;
            stats.intelligence = 12;
            stats.wisdom = 8;
            break;
        case 6:  // 杖 - 高智慧和智力（法术型）
            stats.strength = 5;
            stats.vitality = 8;
            stats.dexterity = 8;
            stats.intelligence = 18;
            stats.wisdom = 16;
            break;
        case 7:  // 扇 - 平衡型，敏捷和智慧
            stats.strength = 10;
            stats.vitality = 10;
            stats.dexterity = 14;
            stats.intelligence = 12;
            stats.wisdom = 12;
            break;
        case 8:  // 钩 - 高敏捷，技巧型
            stats.strength = 10;
            stats.vitality = 12;
            stats.dexterity = 18;
            stats.intelligence = 8;
            stats.wisdom = 6;
            break;
        default:
            // 默认属性（剑）
            stats.strength = 13;
            stats.vitality = 12;
            stats.dexterity = 13;
            stats.intelligence = 8;
            stats.wisdom = 8;
            break;
    }

    // 计算衍生属性
    stats.physical_attack = stats.CalculatePhysicalAttack();
    stats.physical_defense = stats.CalculatePhysicalDefense();

    // 可用点数 = (level - 1) * 5
    stats.available_points = (character.level - 1) * 5;

    return stats;
}

CharacterStats CharacterManager::GetDefaultStats(uint8_t job_class) {
    CharacterStats stats;

    // 根据职业类型设置默认属性
    switch (job_class) {
        case 1:  // 战士 - 高力量和体力
            stats.strength = 15;
            stats.vitality = 14;
            stats.dexterity = 12;
            stats.intelligence = 8;
            stats.wisdom = 8;
            break;
        case 2:  // 法师 - 高智慧和智力
            stats.strength = 8;
            stats.vitality = 10;
            stats.dexterity = 10;
            stats.intelligence = 16;
            stats.wisdom = 15;
            break;
        case 3:  // 弓手 - 高敏捷和力量
            stats.strength = 12;
            stats.vitality = 12;
            stats.dexterity = 16;
            stats.intelligence = 10;
            stats.wisdom = 8;
            break;
        default:
            // 默认属性（战士）
            stats.strength = 15;
            stats.vitality = 14;
            stats.dexterity = 12;
            stats.intelligence = 8;
            stats.wisdom = 8;
            break;
    }

    // 计算衍生属性
    stats.physical_attack = stats.CalculatePhysicalAttack();
    stats.physical_defense = stats.CalculatePhysicalDefense();

    // 新角色没有可用点数
    stats.available_points = 0;

    return stats;
}

bool CharacterManager::UpdatePositionValidated(
    uint64_t character_id,
    float x, float y, float z,
    uint16_t direction,
    uint8_t move_state,
    uint64_t timestamp
) {
    // 获取角色当前位置
    auto character = GetCharacter(character_id);
    if (!character.has_value()) {
        spdlog::error("Character {} not found for position update", character_id);
        return false;
    }

    // 构建移动信息
    MovementInfo movement_info;
    movement_info.character_id = character_id;
    movement_info.map_id = character->map_id;
    movement_info.position.x = x;
    movement_info.position.y = y;
    movement_info.position.z = z;
    movement_info.direction = direction;
    movement_info.move_state = move_state;
    movement_info.timestamp = timestamp;
    movement_info.last_valid_x = character->x;
    movement_info.last_valid_y = character->y;
    movement_info.last_update_time = timestamp - 100; // 假设上次更新在100ms前

    // 验证移动
    auto& movement_manager = MovementManager::Instance();
    auto validation_result = movement_manager.ValidateMovement(movement_info);

    if (validation_result != MovementValidationResult::kValid) {
        spdlog::warn("Movement validation failed for character {}: result={}",
                     character_id, static_cast<int>(validation_result));

        // 检查是否应该踢出
        if (movement_manager.ShouldKick(character_id, 10)) {
            spdlog::error("Character {} kicked due to excessive validation failures",
                          character_id);

            // 断开连接 - 对应 legacy作弊检测后的踢出
            // Legacy中会:
            // 1. 发送踢出消息给客户端
            // 2. 关闭网络连接
            // 3. 记录作弊日志
            // 这里简化为日志记录,实际实现需要Network层支持
            spdlog::warn("Kicking character {} due to movement validation failure",
                        character_id);
            // TODO: 通过Network层断开连接
            // NetworkService::Instance().Disconnect(character_id);
            return false;
        }

        return false;
    }

    // 更新位置
    return UpdatePosition(character_id, x, y, z, direction);
}

} // namespace Game
} // namespace Murim
