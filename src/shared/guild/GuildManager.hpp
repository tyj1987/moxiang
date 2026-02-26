#pragma once

#include <memory>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <shared_mutex>

namespace Murim {
namespace Game {

/**
 * @brief 公会职位枚举
 * 对应 legacy: GUILD_RANK (Guild.h)
 */
enum class GuildPosition : uint8_t {
    kGuildMaster = 1,    // 会长 (GUILD_MASTER)
    kViceMaster = 2,     // 副会长 (GUILD_VICE_MASTER)
    kMember = 3          // 成员 (GUILD_MEMBER)
};

/**
 * @brief 公会信息
 * 对应 legacy: GUILDINFO (Guild.h)
 */
struct GuildInfo {
    uint64_t guild_id;
    std::string guild_name;
    uint64_t leader_id;
    uint8_t level;
    uint32_t exp;
    uint32_t max_members;
    uint32_t current_members;
    std::string notice;  // 公会公告
    std::chrono::system_clock::time_point created_time;
};

/**
 * @brief 公会成员信息
 * 对应 legacy: GUILDMEMBERINFO (Guild.h)
 */
struct GuildMemberInfo {
    uint64_t character_id;
    std::string character_name;
    GuildPosition position;  // 职位
    uint16_t level;
    uint64_t contribution;
    uint64_t guild_id;
    std::chrono::system_clock::time_point join_time;
};

/**
 * @brief 公会管理器
 *
 * 对应 legacy: CGuildManager (GuildManager.h)
 *
 * 核心功能:
 * - 公会创建/解散
 * - 成员管理 (添加/移除/职位设置)
 * - 公会信息查询
 * - 数据库持久化
 */
class GuildManager {
public:
    /**
     * @brief 获取单例实例
     */
    static GuildManager& Instance();

    /**
     * @brief 初始化管理器 (从数据库加载所有公会)
     * @return 是否成功
     */
    bool Initialize();

    /**
     * @brief 创建公会
     * 对应 legacy: CGuildManager::CreateGuildSyn() (GuildManager.cpp:1477)
     *
     * 验证规则:
     * - 角色不能已在公会中
     * - 公会名称不能重复
     * - 需要满足最低等级要求 (legacy: 20级)
     *
     * @param guild_name 公会名称
     * @param leader_id 会长角色ID
     * @return 公会ID (失败返回0)
     */
    uint64_t CreateGuild(const std::string& guild_name, uint64_t leader_id);

    /**
     * @brief 解散公会
     * 对应 legacy: CGuildManager::BreakUpGuildSyn() (GuildManager.cpp:1560)
     *
     * 验证规则:
     * - 只有会长可以解散
     * - 公会不能参加公会战中
     *
     * @param guild_id 公会ID
     * @return 是否成功
     */
    bool DisbandGuild(uint64_t guild_id);

    /**
     * @brief 加入公会
     * 对应 legacy: CGuildManager::AddMemberSyn() (GuildManager.cpp:1815)
     *
     * 验证规则:
     * - 角色不能已在公会中
     * - 公会人数未满
     * - 邀请者有权限 (副会长以上)
     *
     * @param guild_id 公会ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool AddMember(uint64_t guild_id, uint64_t character_id);

    /**
     * @brief 离开公会
     * 对应 legacy: CGuildManager::SecedeSyn() (GuildManager.cpp)
     *
     * @param guild_id 公会ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool LeaveGuild(uint64_t guild_id, uint64_t character_id);

    /**
     * @brief 踢出成员
     * 对应 legacy: CGuildManager::DeleteMemberSyn() (GuildManager.cpp:1690)
     *
     * 验证规则:
     * - 操作者有权限 (副会长以上)
     * - 不能踢出会长
     *
     * @param guild_id 公会ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool KickMember(uint64_t guild_id, uint64_t character_id);

    /**
     * @brief 移除成员 (内部方法, 用于踢出和离开)
     * @param guild_id 公会ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool RemoveMember(uint64_t guild_id, uint64_t character_id);

    /**
     * @brief 设置成员职位
     * 对应 legacy: CGuildManager::ChangeMemberRank() (GuildManager.cpp)
     *
     * @param guild_id 公会ID
     * @param character_id 角色ID
     * @param position 职位
     * @return 是否成功
     */
    bool SetMemberPosition(uint64_t guild_id, uint64_t character_id, GuildPosition position);

    /**
     * @brief 转让会长
     * @param guild_id 公会ID
     * @param new_leader_id 新会长角色ID
     * @return 是否成功
     */
    bool TransferLeadership(uint64_t guild_id, uint64_t new_leader_id);

    /**
     * @brief 更新公会信息
     * @param guild_id 公会ID
     * @param notice 公告内容
     * @return 是否成功
     */
    bool UpdateGuildNotice(uint64_t guild_id, const std::string& notice);

    /**
     * @brief 增加公会经验
     * @param guild_id 公会ID
     * @param exp 经验值
     * @return 是否成功 (可能升级)
     */
    bool AddGuildExp(uint64_t guild_id, uint32_t exp);

    /**
     * @brief 获取公会信息
     * @param guild_id 公会ID
     * @return 公会信息
     */
    std::optional<GuildInfo> GetGuildInfo(uint64_t guild_id);

    /**
     * @brief 获取公会成员列表
     * @param guild_id 公会ID
     * @return 成员列表
     */
    std::vector<GuildMemberInfo> GetGuildMembers(uint64_t guild_id);

    /**
     * @brief 获取成员的公会信息
     * @param character_id 角色ID
     * @return 成员信息
     */
    std::optional<GuildMemberInfo> GetMemberInfo(uint64_t character_id);

    /**
     * @brief 检查角色是否在公会中
     * @param character_id 角色ID
     * @return 是否在公会中
     */
    bool IsCharacterInGuild(uint64_t character_id);

    /**
     * @brief 获取角色所在的公会ID
     * @param character_id 角色ID
     * @return 公会ID (0表示不在公会中)
     */
    uint64_t GetCharacterGuildId(uint64_t character_id);

    /**
     * @brief 检查公会名称是否已存在
     * @param guild_name 公会名称
     * @return 是否存在
     */
    bool GuildNameExists(const std::string& guild_name);

    /**
     * @brief 刷新公会数据到数据库
     * @param guild_id 公会ID
     * @return 是否成功
     */
    bool SaveToDatabase(uint64_t guild_id);

private:
    GuildManager() = default;
    ~GuildManager() = default;
    GuildManager(const GuildManager&) = delete;
    GuildManager& operator=(const GuildManager&) = delete;

    /**
     * @brief 检查公会是否已满
     */
    bool IsGuildFull(uint64_t guild_id);

    /**
     * @brief 检查公会升级
     */
    bool CheckLevelUp(uint64_t guild_id);

    // 线程安全缓存
    mutable std::shared_mutex mutex_;
    std::unordered_map<uint64_t, GuildInfo> guilds_;
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, GuildMemberInfo>> guild_members_;  // guild_id -> (character_id -> member_info)
    std::unordered_map<uint64_t, uint64_t> character_to_guild_;  // character_id -> guild_id
};

} // namespace Game
} // namespace Murim
