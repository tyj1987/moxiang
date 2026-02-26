// MurimServer - Social System Implementation
// 社交系统实现

#include "SocialSystem.hpp"
#include "../game/Player.hpp"
#include "../game/GameObject.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>

namespace Murim {
namespace Game {

// ========== Helper Functions ==========

static uint32_t HashString(const std::string& str) {
    uint32_t hash = 0;
    for (char c : str) {
        hash = hash * 31 + c;
    }
    return hash;
}

// ========== Guild ==========

Guild::Guild() {
}

Guild::Guild(const GuildInfo& info)
    : guild_info_(info) {
}

Guild::~Guild() {
}

bool Guild::AddMember(const GuildMemberInfo& member) {
    if (members_.size() >= MAX_GUILD_MEMBERS) {
        spdlog::warn("Guild[{}]: Member limit reached", guild_info_.guild_id);
        return false;
    }

    if (members_.find(member.character_id) != members_.end()) {
        spdlog::warn("Guild[{}]: Member {} already exists",
                    guild_info_.guild_id, member.character_id);
        return false;
    }

    members_[member.character_id] = member;
    guild_info_.member_count++;

    spdlog::info("Guild[{}]: Member {} added, total: {}",
                guild_info_.guild_id, member.character_id, guild_info_.member_count);

    return true;
}

bool Guild::RemoveMember(uint64_t character_id) {
    auto it = members_.find(character_id);
    if (it == members_.end()) {
        return false;
    }

    // 如果是会长，需要先转移会长
    if (it->second.position == GuildPosition::Master) {
        spdlog::warn("Guild[{}]: Cannot remove master {}", guild_info_.guild_id, character_id);
        return false;
    }

    members_.erase(it);
    guild_info_.member_count--;

    spdlog::info("Guild[{}]: Member {} removed, remaining: {}",
                guild_info_.guild_id, character_id, guild_info_.member_count);

    return true;
}

GuildMemberInfo* Guild::GetMember(uint64_t character_id) {
    auto it = members_.find(character_id);
    return (it != members_.end()) ? &it->second : nullptr;
}

const GuildMemberInfo* Guild::GetMember(uint64_t character_id) const {
    auto it = members_.find(character_id);
    return (it != members_.end()) ? &it->second : nullptr;
}

std::vector<GuildMemberInfo> Guild::GetAllMembers() const {
    std::vector<GuildMemberInfo> result;
    result.reserve(members_.size());

    for (const auto& pair : members_) {
        result.push_back(pair.second);
    }

    return result;
}

std::vector<GuildMemberInfo> Guild::GetOnlineMembers() const {
    std::vector<GuildMemberInfo> result;

    for (const auto& pair : members_) {
        if (pair.second.is_online) {
            result.push_back(pair.second);
        }
    }

    return result;
}

bool Guild::HasMember(uint64_t character_id) const {
    return members_.find(character_id) != members_.end();
}

bool Guild::SetPosition(uint64_t character_id, GuildPosition position) {
    auto it = members_.find(character_id);
    if (it == members_.end()) {
        return false;
    }

    it->second.position = position;
    return true;
}

bool Guild::CanSetPosition(uint64_t operator_id, GuildPosition target_pos, uint64_t target_id) const {
    auto op_it = members_.find(operator_id);
    if (op_it == members_.end()) {
        return false;
    }

    GuildPosition op_pos = op_it->second.position;

    // 只有会长可以设置任何职位
    if (op_pos == GuildPosition::Master) {
        return true;
    }

    // 副会长可以设置长老及以下
    if (op_pos == GuildPosition::ViceMaster) {
        return target_pos <= GuildPosition::Elder;
    }

    return false;
}

bool Guild::AddContribution(uint64_t character_id, uint32_t amount) {
    auto it = members_.find(character_id);
    if (it == members_.end()) {
        return false;
    }

    it->second.contribution += amount;
    return true;
}

bool Guild::SetContribution(uint64_t character_id, uint32_t amount) {
    auto it = members_.find(character_id);
    if (it == members_.end()) {
        return false;
    }

    it->second.contribution = amount;
    return true;
}

bool Guild::CanInvite(uint64_t character_id) const {
    auto it = members_.find(character_id);
    if (it == members_.end()) {
        return false;
    }

    GuildPosition pos = it->second.position;
    return pos <= GuildPosition::Elder;  // 会长、副会长、长老可以邀请
}

bool Guild::CanKick(uint64_t operator_id, uint64_t target_id) const {
    auto op_it = members_.find(operator_id);
    auto target_it = members_.find(target_id);

    if (op_it == members_.end() || target_it == members_.end()) {
        return false;
    }

    GuildPosition op_pos = op_it->second.position;
    GuildPosition target_pos = target_it->second.position;

    // 会长可以踢任何人（除了自己）
    if (op_pos == GuildPosition::Master) {
        return true;
    }

    // 副会长可以踢长老及以下
    if (op_pos == GuildPosition::ViceMaster) {
        return target_pos <= GuildPosition::Elder && target_pos < GuildPosition::ViceMaster;
    }

    return false;
}

bool Guild::CanModifyNotice(uint64_t character_id) const {
    auto it = members_.find(character_id);
    if (it == members_.end()) {
        return false;
    }

    GuildPosition pos = it->second.position;
    return pos <= GuildPosition::ViceMaster;  // 会长和副会长可以修改公告
}

bool Guild::CanDisband(uint64_t character_id) const {
    auto it = members_.find(character_id);
    if (it == members_.end()) {
        return false;
    }

    return it->second.position == GuildPosition::Master;
}

bool Guild::CanWithdrawGold(uint64_t character_id) const {
    auto it = members_.find(character_id);
    if (it == members_.end()) {
        return false;
    }

    // 只有会长和副会长可以提取金币
    return it->second.position == GuildPosition::Master ||
           it->second.position == GuildPosition::ViceMaster;
}

bool Guild::DepositGold(uint64_t character_id, uint32_t amount) {
    // TODO [需其他系统]: 从玩家对象扣除金币
    // 需要通过Player对象获取character_id对应的玩家实例并检查金币数量
    // Player* player = GetPlayer(character_id);
    // if (!player || player->GetGold() < amount) {
    //     return false;
    // }
    // player->RemoveGold(amount);

    guild_info_.gold += amount;
    spdlog::debug("Guild[{}]: Deposited {} gold, total: {}",
                 guild_info_.guild_id, amount, guild_info_.gold);
    return true;
}

bool Guild::WithdrawGold(uint64_t character_id, uint32_t amount) {
    if (guild_info_.gold < amount) {
        return false;
    }

    // 权限检查 - 只有会长和副会长可以提取金币
    if (!CanWithdrawGold(character_id)) {
        spdlog::warn("Guild[{}]: Character {} has no permission to withdraw gold",
                    guild_info_.guild_id, character_id);
        return false;
    }

    guild_info_.gold -= amount;
    spdlog::debug("Guild[{}]: Withdrew {} gold, remaining: {}",
                 guild_info_.guild_id, amount, guild_info_.gold);
    return true;
}

void Guild::AddExp(uint32_t exp) {
    guild_info_.exp += exp;

    // 检查升级
    uint32_t required_exp = CalculateExpForLevel(guild_info_.level + 1);
    if (guild_info_.exp >= required_exp) {
        guild_info_.exp -= required_exp;
        guild_info_.level++;

        spdlog::info("Guild[{}]: Leveled up to {}",
                    guild_info_.guild_id, guild_info_.level);
    }
}

void Guild::AddWin() {
    guild_info_.total_wins++;
}

void Guild::AddLoss() {
    guild_info_.total_losses++;
}

void Guild::SetMemberOnline(uint64_t character_id, bool online) {
    auto it = members_.find(character_id);
    if (it != members_.end()) {
        it->second.is_online = online;
        if (online) {
            it->second.last_login_time = GetCurrentTimeMs();
        }
    }
}

void Guild::BroadcastMessage(const std::string& message) {
    spdlog::info("Guild[{}]: Broadcast: {}", guild_info_.guild_id, message);

    // TODO [需其他系统]: 发送消息给在线成员
    // 需要网络系统向所有在线成员发送消息包
    // for (auto& pair : members_) {
    //     if (pair.second.is_online) {
    //         Player* player = GetPlayer(pair.first);
    //         if (player) {
    //             player->SendGuildMessage(message);
    //         }
    //     }
    // }
}

// ========== GuildManager ==========

GuildManager::GuildManager()
    : next_guild_id_(1) {
}

GuildManager::~GuildManager() {
}

void GuildManager::Init() {
    spdlog::info("GuildManager: Initialized");
}

bool GuildManager::CreateGuild(Player* creator, const std::string& name) {
    if (!creator) {
        return false;
    }

    // 检查等级和金币
    if (creator->GetLevel() < GUILD_CREATE_LEVEL) {
        return false;
    }

    if (creator->GetGold() < GUILD_CREATE_GOLD) {
        return false;
    }

    // 检查名称
    if (!CheckGuildName(name)) {
        return false;
    }

    // 创建公会
    GuildInfo info;
    info.guild_id = next_guild_id_++;
    info.guild_name = name;
    info.master_id = creator->GetObjectId();
    info.create_time = GetCurrentTimeMs();

    auto guild = std::make_unique<Guild>(info);

    // 添加会长
    GuildMemberInfo member;
    member.character_id = info.master_id;
    member.character_name = creator->GetName();
    member.position = GuildPosition::Master;
    member.level = creator->GetLevel();
    member.join_time = GetCurrentTimeMs();
    member.is_online = true;

    guild->AddMember(member);

    // 消耗金币
    creator->RemoveGold(GUILD_CREATE_GOLD);

    // 保存公会
    guilds_[info.guild_id] = std::move(guild);
    character_to_guild_[info.master_id] = info.guild_id;
    guild_name_to_id_[HashString(name)] = info.guild_id;

    spdlog::info("GuildManager: Created guild '{}' (ID: {}) by player {}",
                name, info.guild_id, info.master_id);

    return true;
}

bool GuildManager::CheckGuildName(const std::string& name) const {
    if (name.empty() || name.length() > MAX_GUILD_NAME_LENGTH) {
        return false;
    }

    // 检查名称是否已被使用
    return !IsNameUsed(name);
}

bool GuildManager::IsNameUsed(const std::string& name) const {
    uint32_t hash = HashString(name);
    return guild_name_to_id_.find(hash) != guild_name_to_id_.end();
}

Guild* GuildManager::GetGuild(uint32_t guild_id) {
    auto it = guilds_.find(guild_id);
    return (it != guilds_.end()) ? it->second.get() : nullptr;
}

const Guild* GuildManager::GetGuild(uint32_t guild_id) const {
    auto it = guilds_.find(guild_id);
    return (it != guilds_.end()) ? it->second.get() : nullptr;
}

Guild* GuildManager::GetGuildByName(const std::string&name) {
    uint32_t hash = HashString(name);
    auto it = guild_name_to_id_.find(hash);
    if (it == guild_name_to_id_.end()) {
        return nullptr;
    }

    return GetGuild(it->second);
}

Guild* GuildManager::GetGuildByMember(uint64_t character_id) {
    auto it = character_to_guild_.find(character_id);
    if (it == character_to_guild_.end()) {
        return nullptr;
    }

    return GetGuild(it->second);
}

bool GuildManager::JoinGuild(uint64_t character_id, uint32_t guild_id) {
    auto guild = GetGuild(guild_id);
    if (!guild) {
        return false;
    }

    // 创建成员信息
    GuildMemberInfo member;
    member.character_id = character_id;
    member.position = GuildPosition::Member;
    member.join_time = GetCurrentTimeMs();
    member.is_online = true;

    return guild->AddMember(member);
}

bool GuildManager::LeaveGuild(uint64_t character_id) {
    auto it = character_to_guild_.find(character_id);
    if (it == character_to_guild_.end()) {
        return false;
    }

    auto guild = GetGuild(it->second);
    if (!guild) {
        return false;
    }

    if (!guild->RemoveMember(character_id)) {
        return false;
    }

    character_to_guild_.erase(it);
    return true;
}

bool GuildManager::KickMember(uint64_t operator_id, uint64_t target_id) {
    auto guild = GetGuildByMember(operator_id);
    if (!guild) {
        return false;
    }

    if (!guild->CanKick(operator_id, target_id)) {
        return false;
    }

    if (!guild->RemoveMember(target_id)) {
        return false;
    }

    character_to_guild_.erase(target_id);
    return true;
}

bool GuildManager::DisbandGuild(uint64_t master_id) {
    auto guild = GetGuildByMember(master_id);
    if (!guild) {
        return false;
    }

    if (!guild->CanDisband(master_id)) {
        return false;
    }

    uint32_t guild_id = guild->GetGuildInfo().guild_id;

    // 移除所有成员
    auto members = guild->GetAllMembers();
    for (const auto& member : members) {
        character_to_guild_.erase(member.character_id);
    }

    // 删除公会
    guilds_.erase(guild_id);

    spdlog::info("GuildManager: Guild {} disbanded by {}", guild_id, master_id);
    return true;
}

bool GuildManager::SetGuildNotice(uint64_t character_id, const std::string& notice) {
    auto guild = GetGuildByMember(character_id);
    if (!guild) {
        return false;
    }

    if (!guild->CanModifyNotice(character_id)) {
        return false;
    }

    // TODO [需其他系统]: 更新数据库
    // 需要将公会公告持久化到数据库
    // Database::UpdateGuildNotice(guild->GetGuildInfo().guild_id, notice);

    return true;
}

bool GuildManager::SetGuildMaster(uint64_t current_master_id, uint64_t new_master_id) {
    auto guild = GetGuildByMember(current_master_id);
    if (!guild) {
        return false;
    }

    // 检查新会长是否存在
    auto new_master = guild->GetMember(new_master_id);
    if (!new_master) {
        return false;
    }

    // 交换职位
    guild->SetPosition(current_master_id, GuildPosition::ViceMaster);
    guild->SetPosition(new_master_id, GuildPosition::Master);

    // 更新公会信息
    GuildInfo info = guild->GetGuildInfo();
    info.master_id = new_master_id;
    guild->SetGuildInfo(info);

    spdlog::info("GuildManager: Guild {} master changed from {} to {}",
                guild->GetGuildInfo().guild_id, current_master_id, new_master_id);

    return true;
}

std::vector<GuildInfo> GuildManager::GetGuildRanking(uint8_t type) const {
    std::vector<GuildInfo> all_guilds;

    for (const auto& pair : guilds_) {
        all_guilds.push_back(pair.second->GetGuildInfo());
    }

    // 根据类型排序
    switch (type) {
        case 1:  // 等级
            std::sort(all_guilds.begin(), all_guilds.end(),
                [](const GuildInfo& a, const GuildInfo& b) {
                    return a.level > b.level;
                });
            break;
        case 2:  // 声望
            std::sort(all_guilds.begin(), all_guilds.end(),
                [](const GuildInfo& a, const GuildInfo& b) {
                    return a.fame > b.fame;
                });
            break;
        case 3:  // 胜率
            std::sort(all_guilds.begin(), all_guilds.end(),
                [](const GuildInfo& a, const GuildInfo& b) {
                    float a_rate = (a.total_wins + a.total_losses) > 0) ?
                        static_cast<float>(a.total_wins) / (a.total_wins + a.total_losses) : 0;
                    float b_rate = (b.total_wins + b.total_losses) > 0) ?
                        static_cast<float>(b.total_wins) / (b.total_wins + b.total_losses) : 0;
                    return a_rate > b_rate;
                });
            break;
    }

    // 只返回前100名
    if (all_guilds.size() > 100) {
        all_guilds.resize(100);
    }

    return all_guilds;
}

// ========== FriendSystem ==========

FriendSystem::FriendSystem() {
}

FriendSystem::~FriendSystem() {
}

bool FriendSystem::SendFriendRequest(uint64_t from_id, uint64_t to_id) {
    if (from_id == to_id) {
        return false;
    }

    // 检查是否已经是好友
    if (HasFriend(from_id, to_id)) {
        return false;
    }

    // 检查请求是否已存在
    auto& pending = pending_requests_[to_id];
    if (pending.find(from_id) != pending.end()) {
        return false;
    }

    // 添加到待处理请求
    pending_requests_[to_id].insert(from_id);
    friend_requests_[from_id].insert(to_id);

    spdlog::debug("FriendSystem: Friend request from {} to {}", from_id, to_id);
    return true;
}

bool FriendSystem::AcceptFriendRequest(uint64_t to_id, uint64_t from_id) {
    // 检查请求是否存在
    auto& pending = pending_requests_[to_id];
    if (pending.find(from_id) == pending.end()) {
        return false;
    }

    // 创建好友关系
    FriendInfo friend1, friend2;

    friend1.friend_id = from_id;
    friend1.add_time = GetCurrentTimeMs();

    friend2.friend_id = to_id;
    friend2.add_time = GetCurrentTimeMs();

    friends_[from_id][to_id] = friend1;
    friends_[to_id][from_id] = friend2;

    // 移除请求
    pending.erase(from_id);
    friend_requests_[from_id].erase(to_id);

    spdlog::info("FriendSystem: {} and {} became friends", from_id, to_id);
    return true;
}

bool FriendSystem::RejectFriendRequest(uint64_t to_id, uint64_t from_id) {
    auto& pending = pending_requests_[to_id];
    if (pending.find(from_id) == pending.end()) {
        return false;
    }

    // 移除请求
    pending.erase(from_id);
    friend_requests_[from_id].erase(to_id);

    spdlog::debug("FriendSystem: Friend request from {} to {} rejected",
                 from_id, to_id);
    return true;
}

bool FriendSystem::CancelFriendRequest(uint64_t from_id, uint64_t to_id) {
    auto& requests = friend_requests_[from_id];
    if (requests.find(to_id) == requests.end()) {
        return false;
    }

    requests.erase(to_id);

    // 移除对方的待处理请求
    auto& pending = pending_requests_[to_id];
    pending.erase(from_id);

    spdlog::debug("FriendSystem: Friend request from {} to {} cancelled",
                 from_id, to_id);
    return true;
}

bool FriendSystem::RemoveFriend(uint64_t character_id, uint64_t friend_id) {
    // 从双方移除
    auto& friends1 = friends_[character_id];
    friends1.erase(friend_id);

    auto& friends2 = friends_[friend_id];
    friends2.erase(character_id);

    spdlog::debug("FriendSystem: {} and {} are no longer friends",
                 character_id, friend_id);
    return true;
}

bool FriendSystem::HasFriend(uint64_t character_id, uint64_t friend_id) const {
    auto it = friends_.find(character_id);
    if (it == friends_.end()) {
        return false;
    }

    return it->second.find(friend_id) != it->second.end();
}

std::vector<FriendInfo> FriendSystem::GetFriendList(uint64_t character_id) const {
    std::vector<FriendInfo> result;

    auto it = friends_.find(character_id);
    if (it != friends_.end()) {
        for (const auto& pair : it->second) {
            result.push_back(pair.second);
        }
    }

    return result;
}

std::vector<FriendInfo> FriendSystem::GetOnlineFriends(uint64_t character_id) const {
    std::vector<FriendInfo> result;

    auto it = friends_.find(character_id);
    if (it != friends_.end()) {
        for (const auto& pair : it->second) {
            if (pair.second.is_online) {
                result.push_back(pair.second);
            }
        }
    }

    return result;
}

size_t FriendSystem::GetFriendCount(uint64_t character_id) const {
    auto it = friends_.find(character_id);
    return (it != friends_.end()) ? it->second.size() : 0;
}

bool FriendSystem::CanAddFriend(uint64_t character_id) const {
    // 好友数量限制检查（默认最多100个好友）
    // TODO [需配置]: 从配置文件读取好友数量上限
    constexpr size_t MAX_FRIENDS = 100;
    return GetFriendCount(character_id) < MAX_FRIENDS;
}

bool FriendSystem::SetFriendMemo(uint64_t character_id, uint64_t friend_id, const std::string& memo) {
    auto it = friends_.find(character_id);
    if (it == friends_.end()) {
        return false;
    }

    auto friend_it = it->second.find(friend_id);
    if (friend_it == it->second.end()) {
        return false;
    }

    friend_it->second.memo = memo;
    return true;
}

// ========== Party ==========

Party::Party(uint32_t party_id, uint64_t leader_id)
    : party_id_(party_id), leader_id_(leader_id),
      max_members_(8), distribute_method_(0), create_time_(GetCurrentTimeMs()) {

    // 添加队长
    PartyMemberInfo leader;
    leader.character_id = leader_id;
    leader.position = PartyPosition::Leader;
    leader.is_online = true;

    members_.push_back(leader);

    spdlog::info("Party[{}]: Created by leader {}", party_id, leader_id);
}

Party::~Party() {
}

bool Party::AddMember(const PartyMemberInfo& member) {
    if (IsFull()) {
        return false;
    }

    members_.push_back(member);
    spdlog::info("Party[{}]: Member {} added, total: {}",
                party_id_, member.character_id, members_.size());
    return true;
}

bool Party::RemoveMember(uint64_t character_id) {
    for (auto it = members_.begin(); it != members_.end(); ++it) {
        if (it->character_id == character_id) {
            members_.erase(it);

            // 如果移除的是队长，转移队长
            if (character_id == leader_id_ && !members_.empty()) {
                leader_id_ = members_[0].character_id;
                members_[0].position = PartyPosition::Leader;
            }

            spdlog::info("Party[{}]: Member {} removed, remaining: {}",
                        party_id_, character_id, members_.size());
            return true;
        }
    }

    return false;
}

PartyMemberInfo* Party::GetMember(uint64_t character_id) {
    for (auto& member : members_) {
        if (member.character_id == character_id) {
            return &member;
        }
    }
    return nullptr;
}

const PartyMemberInfo* Party::GetMember(uint64_t character_id) const {
    for (const auto& member : members_) {
        if (member.character_id == character_id) {
            return &member;
        }
    }
    return nullptr;
}

std::vector<PartyMemberInfo> Party::GetAllMembers() const {
    return members_;
}

bool Party::IsMember(uint64_t character_id) const {
    return GetMember(character_id) != nullptr;
}

bool Party::SetLeader(uint64_t current_leader_id, uint64_t new_leader_id) {
    if (current_leader_id != leader_id_) {
        return false;
    }

    PartyMemberInfo* new_leader = GetMember(new_leader_id);
    if (!new_leader) {
        return false;
    }

    // 转移队长权限
    for (auto& member : members_) {
        if (member.character_id == current_leader_id) {
            member.position = PartyPosition::Member;
        }
        if (member.character_id == new_leader_id) {
            member.position = PartyPosition::Leader;
        }
    }

    leader_id_ = new_leader_id;

    spdlog::info("Party[{}]: Leader changed from {} to {}",
                party_id_, current_leader_id, new_leader_id);

    return true;
}

bool Party::KickMember(uint64_t leader_id, uint64_t target_id) {
    if (leader_id != leader_id_) {
        return false;
    }

    return RemoveMember(target_id);
}

void Party::UpdateMemberPosition(uint64_t character_id, float x, float y, float z) {
    PartyMemberInfo* member = GetMember(character_id);
    if (member) {
        member->x = x;
        member->y = y;
        member->z = z;
    }
}

// ========== PartyManager ==========

PartyManager::PartyManager()
    : next_party_id_(1) {
}

PartyManager::~PartyManager() {
}

Party* PartyManager::CreateParty(uint64_t leader_id) {
    auto party = std::make_unique<Party>(next_party_id_++, leader_id);

    uint32_t party_id = party->GetPartyId();
    parties_[party_id] = std::move(party);
    character_to_party_[leader_id] = party_id;

    return parties_[party_id].get();
}

bool PartyManager::DisbandParty(uint64_t leader_id) {
    auto it = character_to_party_.find(leader_id);
    if (it == character_to_party_.end()) {
        return false;
    }

    uint32_t party_id = it->second;
    auto party = GetParty(party_id);
    if (!party) {
        return false;
    }

    if (!party->IsLeader(leader_id)) {
        return false;
    }

    // 移除所有成员的队伍映射
    for (const auto& member : party->GetAllMembers()) {
        character_to_party_.erase(member.character_id);
    }

    // 删除队伍
    parties_.erase(party_id);

    spdlog::info("PartyManager: Party {} disbanded by {}", party_id, leader_id);
    return true;
}

Party* PartyManager::GetParty(uint32_t party_id) {
    auto it = parties_.find(party_id);
    return (it != parties_.end()) ? it->second.get() : nullptr;
}

const Party* PartyManager::GetParty(uint32_t party_id) const {
    auto it = parties_.find(party_id);
    return (it != parties_.end()) ? it->second.get() : nullptr;
}

Party* PartyManager::GetPartyByMember(uint64_t character_id) {
    auto it = character_to_party_.find(character_id);
    if (it == character_to_party_.end()) {
        return nullptr;
    }

    return GetParty(it->second);
}

bool PartyManager::JoinParty(uint64_t character_id, uint32_t party_id) {
    auto party = GetParty(party_id);
    if (!party || party->IsFull()) {
        return false;
    }

    PartyMemberInfo member;
    member.character_id = character_id;
    member.position = PartyPosition::Member;
    member.is_online = true;

    if (!party->AddMember(member)) {
        return false;
    }

    character_to_party_[character_id] = party_id;

    spdlog::info("PartyManager: {} joined party {}", character_id, party_id);
    return true;
}

bool PartyManager::LeaveParty(uint64_t character_id) {
    auto it = character_to_party_.find(character_id);
    if (it == character_to_party_.end()) {
        return false;
    }

    auto party = GetParty(it->second);
    if (!party) {
        return false;
    }

    if (!party->RemoveMember(character_id)) {
        return false;
    }

    character_to_party_.erase(it);

    spdlog::info("PartyManager: {} left party {}", character_id, it->second);
    return true;
}

std::vector<Party*> PartyManager::GetNearbyParties(float x, float y, float z, float range) {
    std::vector<Party*> result;

    for (auto& pair : parties_) {
        Party* party = pair.second.get();

        // 检查队伍中是否有成员在范围内
        for (const auto& member : party->GetAllMembers()) {
            if (member.is_online) {
                float dist = std::sqrt(
                    std::pow(member.x - x, 2) +
                    std::pow(member.y - y, 2) +
                    std::pow(member.z - z, 2)
                );

                if (dist <= range) {
                    result.push_back(party);
                    break;
                }
            }
        }
    }

    return result;
}

// ========== ChatManager ==========

ChatManager::ChatManager() {
}

ChatManager::~ChatManager() {
}

bool ChatManager::SendChat(const ChatMessage& message) {
    if (message.message.empty()) {
        return false;
    }

    // 检查是否被禁言
    if (IsMuted(message.sender_id)) {
        return false;
    }

    // 检查权限
    if (!CanSendToChannel(message.sender_id, message.channel)) {
        return false;
    }

    // 添加到历史
    chat_history_[message.channel].push_back(message);

    // 限制历史大小
    if (chat_history_[message.channel].size() > 100) {
        chat_history_[message.channel].erase(chat_history_[message.channel].begin());
    }

    spdlog::debug("[Chat-{}] Channel: {}: {}",
                message.sender_id,
                static_cast<int>(message.channel), message.message);

    return true;
}

bool ChatManager::SendWhisper(uint64_t sender_id, uint64_t receiver_id, const std::string& message) {
    ChatMessage msg;
    msg.sender_id = sender_id;
    msg.channel = ChatChannel::Whisper;
    msg.message = message;
    msg.receiver_id = receiver_id;
    msg.send_time = GetCurrentTimeMs();

    // TODO [需其他系统]: 发送给接收者
    // 需要网络系统向接收者发送私聊消息包
    // Player* receiver = GetPlayer(receiver_id);
    // if (receiver && receiver->IsOnline()) {
    //     receiver->SendWhisperMessage(sender_id, message);
    //     return true;
    // }
    // return false;

    spdlog::debug("[Chat-Whisper] {} -> {}: {}",
                sender_id, receiver_id, message);

    return true;
}

bool ChatManager::SendSystemMessage(const std::string& message, ChatChannel channel) {
    ChatMessage msg;
    msg.channel = channel;
    msg.message = message;
    msg.send_time = GetCurrentTimeMs();

    chat_history_[channel].push_back(msg);

    spdlog::info("[Chat-System] Channel: {}: {}", static_cast<int>(channel), message);
    return true;
}

bool ChatManager::SendNotice(const std::string& message) {
    return SendSystemMessage(message, ChatChannel::Notice);
}

bool ChatManager::CanSendToChannel(uint64_t character_id, ChatChannel channel) const {
    // 实现频道权限检查
    // 1. 检查玩家等级
    // 2. 检查VIP等级
    // 3. 检查特殊权限

    // TODO [需其他系统]: 从Player对象获取实际等级和检查道具
    // Player* player = GetPlayer(character_id);
    // if (!player) {
    //     return false;
    // }
    // uint16_t player_level = player->GetLevel();

    // 世界频道需要达到一定等级（例如20级）
    if (channel == ChatChannel::World) {
        // TODO [需其他系统]: 检查玩家等级是否达到要求
        // constexpr uint16_t WORLD_CHAT_MIN_LEVEL = 20;
        // if (player_level < WORLD_CHAT_MIN_LEVEL) {
        //     return false;
        // }
    }

    // 喊话频道需要消耗货币或道具
    if (channel == ChatChannel::Shout) {
        // TODO [需其他系统]: 检查是否有喊话卷轴并消耗
        // if (!player->HasItem(ITEM_SHOUT_SCROLL) || !player->ConsumeItem(ITEM_SHOUT_SCROLL, 1)) {
        //     return false;
        // }
    }

    // GM频道只有GM可以发送
    if (channel == ChatChannel::GM) {
        return IsGM(character_id);
    }

    return true;
}

uint32_t ChatManager::GetChannelCooldown(ChatChannel channel) const {
    switch (channel) {
        case ChatChannel::Local:
            return 1000;  // 1秒
        case ChatChannel::World:
        return 10000;  // 10秒
        case ChatChannel::Shout:
            return 30000;  // 30秒
        default:
            return 0;
    }
}

bool ChatManager::MutePlayer(uint64_t gm_id, uint64_t target_id, uint32_t duration_minutes) {
    // 检查GM权限
    if (!IsGM(gm_id)) {
        spdlog::warn("ChatManager: Non-GM character {} attempted to mute player {}",
                    gm_id, target_id);
        return false;
    }

    mute_end_time_[target_id] = GetCurrentTimeMs() + duration_minutes * 60000;

    spdlog::info("ChatManager: Player {} muted for {} minutes by GM {}",
                target_id, duration_minutes, gm_id);

    return true;
}

bool ChatManager::UnmutePlayer(uint64_t gm_id, uint64_t target_id) {
    // 检查GM权限
    if (!IsGM(gm_id)) {
        spdlog::warn("ChatManager: Non-GM character {} attempted to unmute player {}",
                    gm_id, target_id);
        return false;
    }

    mute_end_time_.erase(target_id);

    spdlog::info("ChatManager: Player {} unmuted by GM {}", target_id, gm_id);
    return true;
}

bool ChatManager::IsMuted(uint64_t character_id) const {
    auto it = mute_end_time_.find(character_id);
    if (it == mute_end_time_.end()) {
        return false;
    }

    return GetCurrentTimeMs() < it->second;
}

void ChatManager::SetGM(uint64_t character_id, bool is_gm) {
    if (is_gm) {
        gm_players_.insert(character_id);
        spdlog::info("ChatManager: Character {} promoted to GM", character_id);
    } else {
        gm_players_.erase(character_id);
        spdlog::info("ChatManager: Character {} demoted from GM", character_id);
    }
}

bool ChatManager::IsGM(uint64_t character_id) const {
    return gm_players_.find(character_id) != gm_players_.end();
}

std::vector<ChatMessage> ChatManager::GetChatHistory(ChatChannel channel, size_t count) const {
    std::vector<ChatMessage> result;

    auto it = chat_history_.find(channel);
    if (it != chat_history_.end()) {
        const auto& history = it->second;

        size_t start = (history.size() > count) ? (history.size() - count) : 0;
        for (size_t i = start; i < history.size(); ++i) {
            result.push_back(history[i]);
        }
    }

    return result;
}

// ========== SocialManager ==========

SocialManager::SocialManager() {
}

SocialManager::~SocialManager() {
}

void SocialManager::Init() {
    guild_manager_.Init();
    spdlog::info("SocialManager: Initialized");
}

bool SocialManager::SendPartyChat(uint64_t sender_id, const std::string& message) {
    auto party = party_manager_.GetPartyByMember(sender_id);
    if (!party) {
        return false;
    }

    ChatMessage msg;
    msg.sender_id = sender_id;
    msg.channel = ChatChannel::Party;
    msg.message = message;
    msg.send_time = GetCurrentTimeMs();

    // TODO [需其他系统]: 发送消息给队伍所有在线成员
    // 需要网络系统向队伍成员发送消息包
    // for (const auto& member : party->GetAllMembers()) {
    //     if (member.is_online) {
    //         Player* player = GetPlayer(member.character_id);
    //         if (player) {
    //             player->SendPartyChatMessage(sender_id, message);
    //         }
    //     }
    // }

    spdlog::debug("[Social] Party chat from {}: {}", sender_id, message);
    return true;
}

bool SocialManager::SendGuildChat(uint64_t sender_id, const std::string& message) {
    auto guild = guild_manager_.GetGuildByMember(sender_id);
    if (!guild) {
        return false;
    }

    ChatMessage msg;
    msg.sender_id = sender_id;
    msg.channel = ChatChannel::Guild;
    msg.message = message;
    msg.send_time = GetCurrentTimeMs();

    // TODO [需其他系统]: 发送消息给公会所有在线成员
    // 需要网络系统向公会成员发送消息包
    // auto members = guild->GetOnlineMembers();
    // for (const auto& member : members) {
    //     Player* player = GetPlayer(member.character_id);
    //     if (player) {
    //         player->SendGuildChatMessage(sender_id, message);
    //     }
    // }

    spdlog::debug("[Social] Guild chat from {}: {}", sender_id, message);
    return true;
}

} // namespace Game
} // namespace Murim
