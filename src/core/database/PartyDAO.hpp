#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "shared/social/Party.hpp"

namespace Murim {
namespace Core {
namespace Database {

/**
 * @brief Party DAO (Data Access Object)
 *
 * 负责队伍系统的数据库操作
 */
class PartyDAO {
public:
    // ========== 队伍操作 ==========

    /**
     * @brief 创建队伍
     * @param leader_id 队长角色ID
     * @param max_members 最大成员数
     * @return 队伍ID，失败返回0
     */
    static uint64_t CreateParty(uint64_t leader_id, uint32_t max_members);

    /**
     * @brief 解散队伍
     * @param party_id 队伍ID
     * @return 是否成功
     */
    static bool DisbandParty(uint64_t party_id);

    /**
     * @brief 添加队伍成员
     * @param party_id 队伍ID
     * @param character_id 角色ID
     * @param character_name 角色名称
     * @param level 等级
     * @param job 职业
     * @return 是否成功
     */
    static bool AddPartyMember(
        uint64_t party_id,
        uint64_t character_id,
        const std::string& character_name,
        uint16_t level,
        uint8_t job
    );

    /**
     * @brief 移除队伍成员
     * @param party_id 队伍ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    static bool RemovePartyMember(uint64_t party_id, uint64_t character_id);

    /**
     * @brief 转让队长
     * @param party_id 队伍ID
     * @param new_leader_id 新队长角色ID
     * @return 是否成功
     */
    static bool TransferLeadership(uint64_t party_id, uint64_t new_leader_id);

    /**
     * @brief 更新成员状态
     * @param party_id 队伍ID
     * @param character_id 角色ID
     * @param status 状态
     * @return 是否成功
     */
    static bool UpdateMemberStatus(
        uint64_t party_id,
        uint64_t character_id,
        Game::PartyMemberStatus status
    );

    /**
     * @brief 更新成员位置
     * @param party_id 队伍ID
     * @param character_id 角色ID
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @return 是否成功
     */
    static bool UpdateMemberPosition(
        uint64_t party_id,
        uint64_t character_id,
        float x, float y, float z
    );

    // ========== 队伍查询 ==========

    /**
     * @brief 获取队伍信息
     * @param party_id 队伍ID
     * @return 队伍对象，失败返回nullopt
     */
    static std::optional<Game::Party> GetParty(uint64_t party_id);

    /**
     * @brief 获取角色所在队伍ID
     * @param character_id 角色ID
     * @return 队伍ID，不在队伍中返回0
     */
    static uint64_t GetPartyID(uint64_t character_id);

    /**
     * @brief 获取队伍成员列表
     * @param party_id 队伍ID
     * @return 成员列表
     */
    static std::vector<Game::PartyMember> GetPartyMembers(uint64_t party_id);

    /**
     * @brief 获取队伍成员数量
     * @param party_id 队伍ID
     * @return 成员数量
     */
    static size_t GetPartyMemberCount(uint64_t party_id);

    // ========== 队伍设置 ==========

    /**
     * @brief 更新经验分配类型
     * @param party_id 队伍ID
     * @param exp_type 经验分配类型
     * @return 是否成功
     */
    static bool UpdateExpDistribution(
        uint64_t party_id,
        Game::PartyExpDistributionType exp_type
    );

    /**
     * @brief 更新物品拾取模式
     * @param party_id 队伍ID
     * @param loot_mode 拾取模式
     * @return 是否成功
     */
    static bool UpdateLootMode(
        uint64_t party_id,
        Game::PartyLootMode loot_mode
    );

    /**
     * @brief 更新最低拾取等级
     * @param party_id 队伍ID
     * @param min_level 最低等级
     * @return 是否成功
     */
    static bool UpdateMinLootLevel(
        uint64_t party_id,
        uint16_t min_level
    );

    // ========== 队伍邀请 ==========

    /**
     * @brief 创建队伍邀请
     * @param party_id 队伍ID
     * @param inviter_id 邀请者ID
     * @param invitee_id 被邀请者ID
     * @return 邀请ID，失败返回0
     */
    static uint64_t CreatePartyInvite(
        uint64_t party_id,
        uint64_t inviter_id,
        uint64_t invitee_id
    );

    /**
     * @brief 获取待处理邀请
     * @param character_id 角色ID
     * @return 邀请列表
     */
    static std::vector<Game::PartyInvite> GetPendingInvites(uint64_t character_id);

    /**
     * @brief 删除邀请
     * @param invite_id 邀请ID
     * @return 是否成功
     */
    static bool DeleteInvite(uint64_t invite_id);

    /**
     * @brief 清理过期邀请
     * @param expire_seconds 过期时间（秒）
     * @return 清理数量
     */
    static size_t CleanupExpiredInvites(uint32_t expire_seconds);

    // ========== 队伍统计 ==========

    /**
     * @brief 检查队伍是否已满
     * @param party_id 队伍ID
     * @return 是否已满
     */
    static bool IsPartyFull(uint64_t party_id);

    /**
     * @brief 检查角色是否在队伍中
     * @param character_id 角色ID
     * @return 是否在队伍中
     */
    static bool IsInParty(uint64_t character_id);

    /**
     * @brief 检查角色是否是队长
     * @param party_id 队伍ID
     * @param character_id 角色ID
     * @return 是否是队长
     */
    static bool IsPartyLeader(uint64_t party_id, uint64_t character_id);

private:
    PartyDAO() = default;
};

} // namespace Database
} // namespace Core
} // namespace Murim
