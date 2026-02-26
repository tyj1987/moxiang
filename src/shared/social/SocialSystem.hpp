// MurimServer - Social System
// 社交系统 - 公会、好友、聊天、组队
// 对应legacy: Guild, Party, Chat相关系统

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class Player;
class GameObject;

// ========== Guild Constants ==========
constexpr uint32_t MAX_GUILD_MEMBERS = 100;
constexpr uint32_t MAX_GUILD_NAME_LENGTH = 20;
constexpr uint32_t MAX_GUILD_NOTICE_LENGTH = 200;
constexpr uint32_t GUILD_CREATE_LEVEL = 20;
constexpr uint32_t GUILD_CREATE_GOLD = 1000000;

// ========== Guild Member Position ==========
enum class GuildPosition : uint8_t {
    None = 0,
    Master = 1,        // 公会长
    ViceMaster = 2,     // 副会长
    Elder = 3,          // 长老
    Member = 4,         // 成员
    Trainee = 5         // 学徒
};

// ========== Guild Info ==========
struct GuildInfo {
    uint32_t guild_id;              // 公会ID
    std::string guild_name;         // 公会名称
    uint32_t master_id;             // 公会长ID
    uint16_t level;                 // 公会等级
    uint32_t exp;                   // 公会经验
    uint32_t gold;                  // 公会金库
    uint32_t fame;                  // 公会声望
    std::string notice;             // 公会公告
    uint32_t create_time;           // 创建时间

    // 统计数据
    uint16_t member_count;          // 成员数量
    uint32_t total_wins;            // 总胜利次数
    uint32_t total_losses;          // 总失败次数

    GuildInfo()
        : guild_id(0), master_id(0), level(1), exp(0),
          gold(0), fame(0), create_time(0),
          member_count(0), total_wins(0), total_losses(0) {}
};

// ========== Guild Member Info ==========
struct GuildMemberInfo {
    uint64_t character_id;          // 角色ID
    std::string character_name;     // 角色名称
    GuildPosition position;         // 职位
    uint16_t level;                 // 等级
    uint32_t contribution;          // 贡献度
    uint32_t join_time;             // 加入时间
    uint32_t last_login_time;       // 最后登录时间
    bool is_online;                // 是否在线

    GuildMemberInfo()
        : character_id(0), position(GuildPosition::Member),
          level(1), contribution(0), join_time(0),
          last_login_time(0), is_online(false) {}
};

// ========== Guild ==========
class Guild {
private:
    GuildInfo guild_info_;
    std::map<uint64_t, GuildMemberInfo> members_;  // character_id -> member

public:
    Guild();
    explicit Guild(const GuildInfo& info);
    virtual ~Guild();

    // ========== 信息管理 ==========
    const GuildInfo& GetGuildInfo() const { return guild_info_; }
    void SetGuildInfo(const GuildInfo& info) { guild_info_ = info; }

    // ========== 成员管理 ==========
    bool AddMember(const GuildMemberInfo& member);
    bool RemoveMember(uint64_t character_id);
    GuildMemberInfo* GetMember(uint64_t character_id);
    const GuildMemberInfo* GetMember(uint64_t character_id) const;

    std::vector<GuildMemberInfo> GetAllMembers() const;
    std::vector<GuildMemberInfo> GetOnlineMembers() const;

    uint16_t GetMemberCount() const { return guild_info_.member_count; }
    bool HasMember(uint64_t character_id) const;

    // ========== 职位管理 ==========
    bool SetPosition(uint64_t character_id, GuildPosition position);
    bool CanSetPosition(uint64_t operator_id, GuildPosition target_pos, uint64_t target_id) const;

    // ========== 贡献度管理 ==========
    bool AddContribution(uint64_t character_id, uint32_t amount);
    bool SetContribution(uint64_t character_id, uint32_t amount);

    // ========== 权限检查 ==========
    bool CanInvite(uint64_t character_id) const;
    bool CanKick(uint64_t operator_id, uint64_t target_id) const;
    bool CanModifyNotice(uint64_t character_id) const;
    bool CanDisband(uint64_t character_id) const;
    bool CanWithdrawGold(uint64_t character_id) const;

    // ========== 金库管理 ==========
    bool DepositGold(uint64_t character_id, uint32_t amount);
    bool WithdrawGold(uint64_t character_id, uint32_t amount);
    uint32_t GetGold() const { return guild_info_.gold; }

    // ========== 经验和等级 ==========
    void AddExp(uint32_t exp);
    void AddWin();
    void AddLoss();

    // ========== 在线管理 ==========
    void SetMemberOnline(uint64_t character_id, bool online);
    void BroadcastMessage(const std::string& message);
};

// ========== Guild Manager ==========
// 公会管理器
class GuildManager {
private:
    std::map<uint32_t, std::unique_ptr<Guild>> guilds_;  // guild_id -> guild
    std::map<uint64_t, uint32_t> character_to_guild_;   // character_id -> guild_id
    std::map<uint32_t, uint64_t> guild_name_to_id_;      // guild_name_hash -> guild_id

    // 公会名称ID计数器
    uint32_t next_guild_id_;

public:
    GuildManager();
    ~GuildManager();

    void Init();

    // ========== 公会创建 ==========
    bool CreateGuild(Player* creator, const std::string& name);
    bool CheckGuildName(const std::string& name) const;
    bool IsNameUsed(const std::string& name) const;

    // ========== 公会查询 ==========
    Guild* GetGuild(uint32_t guild_id);
    const Guild* GetGuild(uint32_t guild_id) const;
    Guild* GetGuildByName(const std::string& name);
    Guild* GetGuildByMember(uint64_t character_id);

    // ========== 成员操作 ==========
    bool JoinGuild(uint64_t character_id, uint32_t guild_id);
    bool LeaveGuild(uint64_t character_id);
    bool KickMember(uint64_t operator_id, uint64_t target_id);

    // ========== 公会管理 ==========
    bool DisbandGuild(uint64_t master_id);
    bool SetGuildNotice(uint64_t character_id, const std::string& notice);
    bool SetGuildMaster(uint64_t current_master_id, uint64_t new_master_id);

    // ========== 排行榜 ==========
    std::vector<GuildInfo> GetGuildRanking(uint8_t type) const;  // 1=等级, 2=声望, 3=胜率

    // ========== 统计 ==========
    size_t GetGuildCount() const { return guilds_.size(); }
};

// ========== Friend System ==========
// 好友系统
struct FriendInfo {
    uint64_t friend_id;             // 好友ID
    std::string friend_name;        // 好友名称
    uint16_t level;                 // 等级
    bool is_online;                // 是否在线
    uint32_t add_time;              // 添加时间
    std::string memo;               // 备注

    FriendInfo()
        : friend_id(0), level(1), is_online(false),
          add_time(0) {}
};

class FriendSystem {
private:
    std::map<uint64_t, std::map<uint64_t, FriendInfo>> friends_;  // character_id -> friends
    std::map<uint64_t, std::set<uint64_t>> friend_requests_;     // 发出的好友请求
    std::map<uint64_t, std::set<uint64_t>> pending_requests_;   // 收到的好友请求

public:
    FriendSystem();
    ~FriendSystem();

    // ========== 好友请求 ==========
    bool SendFriendRequest(uint64_t from_id, uint64_t to_id);
    bool AcceptFriendRequest(uint64_t to_id, uint64_t from_id);
    bool RejectFriendRequest(uint64_t to_id, uint64_t from_id);
    bool CancelFriendRequest(uint64_t from_id, uint64_t to_id);

    // ========== 好友管理 ==========
    bool RemoveFriend(uint64_t character_id, uint64_t friend_id);
    bool HasFriend(uint64_t character_id, uint64_t friend_id) const;

    // ========== 好友列表 ==========
    std::vector<FriendInfo> GetFriendList(uint64_t character_id) const;
    std::vector<FriendInfo> GetOnlineFriends(uint64_t character_id) const;

    // ========== 好友数量限制 ==========
    size_t GetFriendCount(uint64_t character_id) const;
    bool CanAddFriend(uint64_t character_id) const;

    // ========== 备注管理 ==========
    bool SetFriendMemo(uint64_t character_id, uint64_t friend_id, const std::string& memo);
};

// ========== Party System ==========
// 组队系统
enum class PartyPosition : uint8_t {
    None = 0,
    Leader = 1,       // 队长
    Member = 2        // 队员
};

struct PartyMemberInfo {
    uint64_t character_id;
    std::string character_name;
    PartyPosition position;
    uint16_t level;
    uint32_t current_hp;
    uint32_t max_hp;
    float x, y, z;               // 位置
    bool is_online;

    PartyMemberInfo()
        : character_id(0), position(PartyPosition::Member),
          level(1), current_hp(0), max_hp(0),
          x(0), y(0), z(0), is_online(false) {}
};

class Party {
private:
    uint32_t party_id_;
    uint64_t leader_id_;
    std::vector<PartyMemberInfo> members_;
    uint8_t max_members_;
    uint32_t distribute_method_;  // 0=自由分配 1=顺序分配 2=队长分配
    uint32_t create_time_;

public:
    Party(uint32_t party_id, uint64_t leader_id);
    virtual ~Party();

    // ========== 信息 ==========
    uint32_t GetPartyId() const { return party_id_; }
    uint64_t GetLeaderId() const { return leader_id_; }
    uint8_t GetMemberCount() const { return static_cast<uint8_t>(members_.size()); }
    uint8_t GetMaxMembers() const { return max_members_; }

    // ========== 成员管理 ==========
    bool AddMember(const PartyMemberInfo& member);
    bool RemoveMember(uint64_t character_id);
    PartyMemberInfo* GetMember(uint64_t character_id);
    const PartyMemberInfo* GetMember(uint64_t character_id) const;
    std::vector<PartyMemberInfo> GetAllMembers() const;

    bool IsFull() const { return members_.size() >= max_members_; }
    bool IsMember(uint64_t character_id) const;
    bool IsLeader(uint64_t character_id) const { return character_id == leader_id_; }

    // ========== 队长权限 ==========
    bool SetLeader(uint64_t current_leader_id, uint64_t new_leader_id);
    bool KickMember(uint64_t leader_id, uint64_t target_id);

    // ========== 分配方式 ==========
    void SetDistributeMethod(uint32_t method) { distribute_method_ = method; }
    uint32_t GetDistributeMethod() const { return distribute_method_; }

    // ========== 位置同步 ==========
    void UpdateMemberPosition(uint64_t character_id, float x, float y, float z);
};

class PartyManager {
private:
    std::map<uint32_t, std::unique_ptr<Party>> parties_;
    std::map<uint64_t, uint32_t> character_to_party_;  // character_id -> party_id
    uint32_t next_party_id_;

public:
    PartyManager();
    ~PartyManager();

    // ========== 组队创建 ==========
    Party* CreateParty(uint64_t leader_id);
    bool DisbandParty(uint64_t leader_id);

    // ========== 成员操作 ==========
    bool JoinParty(uint64_t character_id, uint32_t party_id);
    bool LeaveParty(uint64_t character_id);
    bool InviteToParty(uint64_t inviter_id, uint64_t invitee_id);
    bool AcceptPartyInvite(uint64_t invitee_id, uint32_t party_id);
    bool RejectPartyInvite(uint64_t invitee_id, uint32_t party_id);

    // ========== 队伍查询 ==========
    Party* GetParty(uint32_t party_id);
    const Party* GetParty(uint32_t party_id) const;
    Party* GetPartyByMember(uint64_t character_id);

    // ========== 附近队伍 ==========
    std::vector<Party*> GetNearbyParties(float x, float y, float z, float range);
};

// ========== Chat System ==========
// 聊天系统
enum class ChatChannel : uint8_t {
    None = 0,
    Local = 1,         // 附近频道
    World = 2,         // 世界频道
    Guild = 3,         // 公会频道
    Party = 4,         // 队伍频道
    Whisper = 5,        // 私聊
    Shout = 6,          // 喊喊频道
    System = 7,         // 系统消息
    Notice = 8          // 公告
};

struct ChatMessage {
    uint64_t sender_id;
    std::string sender_name;
    ChatChannel channel;
    std::string message;
    uint32_t send_time;

    // 私聊专用
    uint64_t receiver_id;
    std::string receiver_name;

    ChatMessage()
        : sender_id(0), channel(ChatChannel::Local),
          send_time(0), receiver_id(0) {}
};

class ChatManager {
private:
    // 聊天历史（暂存最近消息）
    std::map<ChatChannel, std::vector<ChatMessage>> chat_history_;

    // 禁言列表
    std::set<uint64_t> muted_players_;
    std::map<uint64_t, uint32_t> mute_end_time_;  // player_id -> end_time

    // GM权限
    std::set<uint64_t> gm_players_;

public:
    ChatManager();
    ~ChatManager();

    // ========== 消息发送 ==========
    bool SendChat(const ChatMessage& message);
    bool SendWhisper(uint64_t sender_id, uint64_t receiver_id, const std::string& message);
    bool SendSystemMessage(const std::string& message, ChatChannel channel = ChatChannel::System);
    bool SendNotice(const std::string& message);

    // ========== 频道权限 ==========
    bool CanSendToChannel(uint64_t character_id, ChatChannel channel) const;
    uint32_t GetChannelCooldown(ChatChannel channel) const;

    // ========== 禁言管理 ==========
    bool MutePlayer(uint64_t gm_id, uint64_t target_id, uint32_t duration_minutes);
    bool UnmutePlayer(uint64_t gm_id, uint64_t target_id);
    bool IsMuted(uint64_t character_id) const;

    // ========== GM管理 ==========
    void SetGM(uint64_t character_id, bool is_gm);
    bool IsGM(uint64_t character_id) const;

    // ========== 消息历史 ==========
    std::vector<ChatMessage> GetChatHistory(ChatChannel channel, size_t count) const;
};

// ========== Social Manager ==========
// 社交系统统一管理器
class SocialManager {
private:
    GuildManager guild_manager_;
    FriendSystem friend_system_;
    PartyManager party_manager_;
    ChatManager chat_manager_;

public:
    SocialManager();
    ~SocialManager();

    void Init();

    // ========== 子系统访问 ==========
    GuildManager* GetGuildManager() { return &guild_manager_; }
    FriendSystem* GetFriendSystem() { return &friend_system_; }
    PartyManager* GetPartyManager() { return &party_manager_; }
    ChatManager* GetChatManager() { return &chat_manager_; }

    const GuildManager* GetGuildManager() const { return &guild_manager_; }
    const FriendSystem* GetFriendSystem() const { return &friend_system_; }
    const PartyManager* GetPartyManager() const { return &party_manager_; }
    const ChatManager* GetChatManager() const { return &chat_manager_; }

    // ========== 跨系统集成 ==========
    // 例如：队伍聊天需要Party和Chat系统协作
    bool SendPartyChat(uint64_t sender_id, const std::string& message);
    bool SendGuildChat(uint64_t sender_id, const std::string& message);
};

} // namespace Game
} // namespace Murim
