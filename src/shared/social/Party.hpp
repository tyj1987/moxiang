#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include "shared/character/Character.hpp"

namespace Murim {
namespace Game {

/**
 * @brief 队伍角色状态
 */
enum class PartyMemberStatus : uint8_t {
    kOnline = 0,          // 在线
    kOffline = 1,         // 离线
    kDead = 2             // 死亡
};

/**
 * @brief 经验分配方式
 */
enum class PartyExpDistributionType : uint8_t {
    kLevelBased = 0,      // 基于等级分配
    kEqual = 1,            // 平均分配
    kContribution = 2      // 基于贡献分配
};

/**
 * @brief 战利品分配模式
 */
enum class PartyLootMode : uint8_t {
    kFreeForAll = 0,       // 自由拾取
    kRoundRobin = 1,       // 轮流拾取
    kMasterLooter = 2,     // 队长分配
    kNeedBeforeGreed = 3   // 需求优先
};

/**
 * @brief 队伍成员数据
 */
struct PartyMember {
    uint64_t character_id;          // 角色ID
    std::string character_name;     // 角色名称
    uint16_t level;                 // 等级
    uint8_t job;                    // 职业
    uint32_t hp;                    // 当前生命值
    uint32_t max_hp;                // 最大生命值
    uint32_t mp;                    // 当前内力值
    uint32_t max_mp;                // 最大内力值
    PartyMemberStatus status;      // 状态
    float x, y, z;                 // 位置
    std::chrono::system_clock::time_point join_time;  // 加入时间

    /**
     * @brief 是否在线
     */
    bool IsOnline() const {
        return status == PartyMemberStatus::kOnline;
    }

    /**
     * @brief 是否存活
     */
    bool IsAlive() const {
        return status != PartyMemberStatus::kDead && hp > 0;
    }

    /**
     * @brief 获取距离
     */
    float DistanceTo(const PartyMember& other) const;

    /**
     * @brief 获取等级差
     */
    uint16_t GetLevelDiff(const PartyMember& other) const;
};

/**
 * @brief 队伍设置
 */
struct PartySettings {
    PartyExpDistributionType exp_type;  // 经验分配方式
    PartyLootMode loot_mode;            // 战利品分配模式
    uint8_t auto_loot;                  // 自动拾取 (0=关闭, 1=仅队长, 2=全体)
    uint8_t max_members;                 // 最大成员数
    uint16_t level_diff_limit;           // 等级差限制 (推荐组队)
    uint32_t share_range;                // 共享范围 (经验/物品)
    uint16_t min_loot_level;             // 最低拾取等级
};

/**
 * @brief 队伍数据
 */
struct Party {
    uint64_t party_id;                // 队伍ID
    uint64_t leader_id;               // 队长ID
    std::vector<PartyMember> members;  // 成员列表
    PartySettings settings;           // 队伍设置

    std::chrono::system_clock::time_point create_time;  // 创建时间

    /**
     * @brief 获取成员数量
     */
    size_t GetMemberCount() const {
        return members.size();
    }

    /**
     * @brief 是否已满
     */
    bool IsFull() const {
        return members.size() >= settings.max_members;
    }

    /**
     * @brief 是否是成员
     */
    bool IsMember(uint64_t character_id) const;

    /**
     * @brief 是否是队长
     */
    bool IsLeader(uint64_t character_id) const {
        return leader_id == character_id;
    }

    /**
     * @brief 获取成员
     */
    std::optional<PartyMember*> GetMember(uint64_t character_id);

    /**
     * @brief 添加成员
     */
    bool AddMember(const PartyMember& member);

    /**
     * @brief 移除成员
     */
    bool RemoveMember(uint64_t character_id);

    /**
     * @brief 设置队长
     */
    bool SetLeader(uint64_t character_id);

    /**
     * @brief 更新成员状态
     */
    bool UpdateMemberStatus(uint64_t character_id, PartyMemberStatus status);

    /**
     * @brief 更新成员位置
     */
    bool UpdateMemberPosition(
        uint64_t character_id,
        float x, float y, float z
    );
};

/**
 * @brief 队伍邀请
 */
struct PartyInvite {
    uint64_t invite_id;                                   // 邀请ID
    uint64_t party_id;                                    // 队伍ID
    uint64_t inviter_id;                                  // 邀请者ID
    uint64_t invitee_id;                                  // 被邀请者ID
    std::chrono::system_clock::time_point invite_time;    // 邀请时间
    std::chrono::system_clock::time_point expire_time;    // 过期时间 (5分钟后)

    /**
     * @brief 是否已过期
     */
    bool IsExpired() const {
        auto now = std::chrono::system_clock::now();
        return now >= expire_time;
    }

    /**
     * @brief 是否可以接受
     */
    bool CanAccept() const {
        return !IsExpired();
    }
};

/**
 * @brief 组队管理器
 *
 * 负责队伍的创建、邀请、解散
 * 对应 legacy: Party.cpp
 */
class PartyManager {
public:
    /**
     * @brief 获取单例实例
     */
    static PartyManager& Instance();

    /**
     * @brief 初始化组队管理器
     * @return 是否初始化成功
     */
    bool Initialize();

    /**
     * @brief 处理队伍逻辑
     */
    void Process();

    // ========== 队伍创建/解散 ==========

    /**
     * @brief 创建队伍
     * @param leader_id 队长ID
     * @param max_members 最大成员数 (默认6人)
     * @return 队伍ID,失败返回 0
     */
    uint64_t CreateParty(
        uint64_t leader_id,
        uint8_t max_members = 6
    );

    /**
     * @brief 解散队伍
     * @param party_id 队伍ID
     * @return 是否成功
     */
    bool DisbandParty(uint64_t party_id);

    // ========== 队员操作 ==========

    /**
     * @brief 邀请加入队伍
     * @param party_id 队伍ID
     * @param inviter_id 邀请者ID
     * @param target_id 目标ID
     * @return 是否成功
     */
    bool InviteToParty(
        uint64_t party_id,
        uint64_t inviter_id,
        uint64_t target_id
    );

    /**
     * @brief 接受邀请
     * @param character_id 角色ID
     * @param party_id 队伍ID
     * @return 是否成功
     */
    bool AcceptInvite(
        uint64_t character_id,
        uint64_t party_id
    );

    /**
     * @brief 拒绝邀请
     * @param character_id 角色ID
     * @param party_id 队伍ID
     * @return 是否成功
     */
    bool RejectInvite(
        uint64_t character_id,
        uint64_t party_id
    );

    /**
     * @brief 离开队伍
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool LeaveParty(uint64_t character_id);

    /**
     * @brief 踢出队伍
     * @param party_id 队伍ID
     * @param leader_id 队长ID
     * @param target_id 目标ID
     * @return 是否成功
     */
    bool KickFromParty(
        uint64_t party_id,
        uint64_t leader_id,
        uint64_t target_id
    );

    /**
     * @brief 转让队长
     * @param party_id 队伍ID
     * @param leader_id 当前队长ID
     * @param new_leader_id 新队长ID
     * @return 是否成功
     */
    bool TransferLeadership(
        uint64_t party_id,
        uint64_t leader_id,
        uint64_t new_leader_id
    );

    // ========== 队伍查询 ==========

    /**
     * @brief 获取队伍
     * @param party_id 队伍ID
     * @return 队伍数据
     */
    std::optional<Party*> GetParty(uint64_t party_id);

    /**
     * @brief 获取角色所在队伍
     * @param character_id 角色ID
     * @return 队伍ID
     */
    std::optional<uint64_t> GetPartyID(uint64_t character_id);

    /**
     * @brief 获取角色所在队伍
     * @param character_id 角色ID
     * @return 队伍指针
     */
    Party* GetCharacterParty(uint64_t character_id);

    /**
     * @brief 获取范围内的队伍成员
     * @param character_id 角色ID
     * @param range 范围
     * @return 队伍成员列表
     */
    std::vector<PartyMember> GetPartyMembersInRange(
        uint64_t character_id,
        float range
    );

    // ========== 经验分配 ==========

    /**
     * @brief 分配经验
     * @param party_id 队伍ID
     * @param base_exp 基础经验
     * @param contributor_ids 贡献者ID列表
     * @return 每个角色获得的经验
     */
    std::unordered_map<uint64_t, uint32_t> DistributeExp(
        uint64_t party_id,
        uint32_t base_exp,
        const std::vector<uint64_t>& contributor_ids
    );

private:
    PartyManager() = default;
    ~PartyManager() = default;
    PartyManager(const PartyManager&) = delete;
    PartyManager& operator=(const PartyManager&) = delete;

    // 队伍存储: party_id -> Party
    std::unordered_map<uint64_t, Party> parties_;

    // 角色到队伍的映射: character_id -> party_id
    std::unordered_map<uint64_t, uint64_t> character_to_party_;

    // 队伍邀请: character_id -> (inviter_id -> party_id)
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint64_t>> party_invites_;

    // 队伍ID生成器
    uint64_t next_party_id_ = 1;

    /**
     * @brief 生成队伍ID
     */
    uint64_t GeneratePartyId();

    /**
     * @brief 队伍通知
     */
    void SendPartyNotification(
        uint64_t party_id,
        const std::string& message
    );

    /**
     * @brief 发送队伍邀请
     */
    void SendPartyInvite(
        uint64_t target_id,
        uint64_t inviter_id,
        uint64_t party_id
    );

    /**
     * @brief 计算等级分配经验
     */
    uint32_t CalculateLevelBasedExp(
        uint32_t base_exp,
        uint16_t character_level,
        uint16_t max_level
    );
};

} // namespace Game
} // namespace Murim
