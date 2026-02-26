#pragma once

#include <vector>
#include <optional>
#include "shared/guild/GuildManager.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"

namespace Murim {
namespace Core {
namespace Database {

/**
 * @brief 公会数据访问对象 (DAO)
 *
 * 负责公会的数据库操作
 * 对应 legacy: GuildManager + Guild database operations
 */
class GuildDAO {
public:
    /**
     * @brief 创建公会
     * @param guild_name 公会名称
     * @param leader_id 会长角色ID
     * @return 新创建的公会 ID (失败返回 0)
     */
    static uint64_t CreateGuild(const std::string& guild_name, uint64_t leader_id);

    /**
     * @brief 加载公会信息
     * @param guild_id 公会 ID
     * @return 公会信息
     */
    static std::optional<Game::GuildInfo> LoadGuild(uint64_t guild_id);

    /**
     * @brief 根据名称加载公会
     * @param guild_name 公会名称
     * @return 公会信息
     */
    static std::optional<Game::GuildInfo> LoadGuildByName(const std::string& guild_name);

    /**
     * @brief 保存公会信息
     * @param guild_info 公会信息
     * @return 是否成功
     */
    static bool SaveGuild(const Game::GuildInfo& guild_info);

    /**
     * @brief 删除公会
     * @param guild_id 公会 ID
     * @return 是否成功
     */
    static bool DeleteGuild(uint64_t guild_id);

    /**
     * @brief 添加公会成员
     * @param guild_id 公会 ID
     * @param character_id 角色 ID
     * @param position 职位 (默认: Member)
     * @return 是否成功
     */
    static bool AddMember(uint64_t guild_id, uint64_t character_id,
                         Game::GuildPosition position = Game::GuildPosition::kMember);

    /**
     * @brief 移除公会成员
     * @param guild_id 公会 ID
     * @param character_id 角色 ID
     * @return 是否成功
     */
    static bool RemoveMember(uint64_t guild_id, uint64_t character_id);

    /**
     * @brief 更新成员职位
     * @param guild_id 公会 ID
     * @param character_id 角色 ID
     * @param position 新职位
     * @return 是否成功
     */
    static bool UpdateMemberPosition(uint64_t guild_id, uint64_t character_id,
                                    Game::GuildPosition position);

    /**
     * @brief 加载公会成员列表
     * @param guild_id 公会 ID
     * @return 成员列表
     */
    static std::vector<Game::GuildMemberInfo> LoadMembers(uint64_t guild_id);

    /**
     * @brief 加载成员的公会信息
     * @param character_id 角色 ID
     * @return 成员的公会信息
     */
    static std::optional<Game::GuildMemberInfo> LoadMemberInfo(uint64_t character_id);

    /**
     * @brief 获取公会成员数量
     * @param guild_id 公会 ID
     * @return 成员数量
     */
    static uint32_t GetMemberCount(uint64_t guild_id);

    /**
     * @brief 检查公会是否存在
     * @param guild_id 公会 ID
     * @return 是否存在
     */
    static bool GuildExists(uint64_t guild_id);

    /**
     * @brief 检查公会名称是否已存在
     * @param guild_name 公会名称
     * @return 是否存在
     */
    static bool GuildNameExists(const std::string& guild_name);

    /**
     * @brief 检查角色是否已在公会中
     * @param character_id 角色 ID
     * @return 是否在公会中
     */
    static bool IsCharacterInGuild(uint64_t character_id);

    /**
     * @brief 更新公会等级
     * @param guild_id 公会 ID
     * @param level 新等级
     * @return 是否成功
     */
    static bool UpdateGuildLevel(uint64_t guild_id, uint8_t level);

    /**
     * @brief 更新公会经验
     * @param guild_id 公会 ID
     * @param exp 经验值
     * @return 是否成功
     */
    static bool UpdateGuildExp(uint64_t guild_id, uint32_t exp);

    /**
     * @brief 更新公会公告
     * @param guild_id 公会 ID
     * @param notice 公告内容
     * @return 是否成功
     */
    static bool UpdateGuildNotice(uint64_t guild_id, const std::string& notice);

    /**
     * @brief 加载所有公会 (服务器启动时)
     * @return 所有公会信息
     */
    static std::vector<Game::GuildInfo> LoadAllGuilds();

    /**
     * @brief 加载所有公会成员 (服务器启动时)
     * @return 所有公会成员信息
     */
    static std::vector<Game::GuildMemberInfo> LoadAllMembers();

private:
    /**
     * @brief 从数据库行构建公会信息对象
     */
    static std::optional<Game::GuildInfo> RowToGuildInfo(Result& result, int row);

    /**
     * @brief 从数据库行构建公会成员信息对象
     */
    static std::optional<Game::GuildMemberInfo> RowToMemberInfo(Result& result, int row);
};

} // namespace Database
} // namespace Core
} // namespace Murim
