#include "GuildManager.hpp"
#include "core/database/GuildDAO.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <shared_mutex>

using Murim::Core::Database::GuildDAO;

namespace Murim {
namespace Game {

// ============================================================================
// 单例和初始化
// ============================================================================

GuildManager& GuildManager::Instance() {
    static GuildManager instance;
    return instance;
}

bool GuildManager::Initialize() {
    spdlog::info("[GuildManager] Initializing GuildManager...");

    try {
        // 从数据库加载所有公会
        auto guilds = GuildDAO::LoadAllGuilds();
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            for (const auto& guild : guilds) {
                guilds_[guild.guild_id] = guild;
            }
        }
        spdlog::info("[GuildManager] Loaded {} guildes from database", guilds.size());

        // 从数据库加载所有公会成员
        auto members = GuildDAO::LoadAllMembers();
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            for (const auto& member : members) {
                guild_members_[member.guild_id][member.character_id] = member;
                character_to_guild_[member.character_id] = member.guild_id;
            }
        }
        spdlog::info("[GuildManager] Loaded {} guild members from database", members.size());

        spdlog::info("[GuildManager] Initialization complete");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[GuildManager] Initialization failed: {}", e.what());
        return false;
    }
}

// ============================================================================
// 公会创建和解散
// ============================================================================

uint64_t GuildManager::CreateGuild(const std::string& guild_name, uint64_t leader_id) {
    spdlog::info("[GuildManager] Creating guild: {} (Leader: {})", guild_name, leader_id);

    // 对应 legacy: CGuildManager::CreateGuildSyn() (GuildManager.cpp:1477)
    // legacy中完整的公会创建逻辑包括:
    // 1. 验证角色是否已有公会
    // 2. 验证角色等级是否满足要求 (legacy: 20级)
    // 3. 验证公会名称是否重复
    // 4. 扣除创建费用
    // 5. 创建公会数据
    // 6. 保存到数据库
    // 7. 发送创建成功消息

    // 检查角色是否已在公会中
    if (IsCharacterInGuild(leader_id)) {
        spdlog::warn("[GuildManager] Character {} already in a guild", leader_id);
        return 0;
    }

    // 检查公会名称是否重复
    if (GuildNameExists(guild_name)) {
        spdlog::warn("[GuildManager] Guild name '{}' already exists", guild_name);
        return 0;
    }

    // 使用DAO创建公会
    uint64_t guild_id = GuildDAO::CreateGuild(guild_name, leader_id);
    if (guild_id == 0) {
        spdlog::error("[GuildManager] Failed to create guild in database");
        return 0;
    }

    // 加载完整的公会信息
    auto guild_info_opt = GuildDAO::LoadGuild(guild_id);
    if (!guild_info_opt) {
        spdlog::error("[GuildManager] Failed to load created guild");
        return 0;
    }

    // 添加到缓存
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        guilds_[guild_id] = *guild_info_opt;

        // 添加会长为成员
        GuildMemberInfo member_info;
        member_info.character_id = leader_id;
        member_info.guild_id = guild_id;
        member_info.position = GuildPosition::kGuildMaster;
        member_info.join_time = std::chrono::system_clock::now();
        member_info.contribution = 0;
        member_info.level = 1;
        member_info.character_name = "";

        guild_members_[guild_id][leader_id] = member_info;
        character_to_guild_[leader_id] = guild_id;
    }

    spdlog::info("[GuildManager] Guild created successfully: {} (ID: {}, Leader: {})",
                guild_name, guild_id, leader_id);
    return guild_id;
}

bool GuildManager::DisbandGuild(uint64_t guild_id) {
    spdlog::info("[GuildManager] Disbanding guild: {}", guild_id);

    // 对应 legacy: CGuildManager::BreakUpGuildSyn() (GuildManager.cpp:1560)
    // legacy中完整的解散逻辑包括:
    // 1. 验证请求者是否为公会会长
    // 2. 检查公会是否参加公会战中
    // 3. 移除所有成员
    // 4. 删除公会数据
    // 5. 保存到数据库
    // 6. 发送解散消息

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = guilds_.find(guild_id);
    if (it == guilds_.end()) {
        spdlog::error("[GuildManager] Guild not found: {}", guild_id);
        return false;
    }

    // 移除所有成员的映射
    auto members_it = guild_members_.find(guild_id);
    if (members_it != guild_members_.end()) {
        for (const auto& [character_id, member_info] : members_it->second) {
            character_to_guild_.erase(character_id);
        }
        guild_members_.erase(members_it);
    }

    // 删除公会
    guilds_.erase(it);

    lock.unlock();

    // 从数据库删除
    if (!GuildDAO::DeleteGuild(guild_id)) {
        spdlog::error("[GuildManager] Failed to delete guild from database: {}", guild_id);
        return false;
    }

    spdlog::info("[GuildManager] Guild disbanded successfully: {}", guild_id);
    return true;
}

// ============================================================================
// 成员管理
// ============================================================================

bool GuildManager::AddMember(uint64_t guild_id, uint64_t character_id) {
    spdlog::info("[GuildManager] Adding character {} to guild {}", character_id, guild_id);

    // 对应 legacy: CGuildManager::AddMemberSyn() (GuildManager.cpp:1815)
    // legacy中完整的添加成员逻辑包括:
    // 1. 验证邀请者权限(副会长以上)
    // 2. 验证被邀请者是否已有公会
    // 3. 验证公会人数是否已满
    // 4. 添加成员到公会
    // 5. 保存到数据库
    // 6. 发送添加成功消息

    // 检查角色是否已在公会中
    if (IsCharacterInGuild(character_id)) {
        spdlog::warn("[GuildManager] Character {} already in a guild", character_id);
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);

    // 检查公会是否存在
    auto guild_it = guilds_.find(guild_id);
    if (guild_it == guilds_.end()) {
        spdlog::error("[GuildManager] Guild not found: {}", guild_id);
        return false;
    }

    // 检查公会是否已满
    if (guild_it->second.current_members >= guild_it->second.max_members) {
        spdlog::warn("[GuildManager] Guild {} is full ({} / {})",
                    guild_id, guild_it->second.current_members,
                    guild_it->second.max_members);
        return false;
    }

    lock.unlock();

    // 使用DAO添加成员
    if (!GuildDAO::AddMember(guild_id, character_id, GuildPosition::kMember)) {
        spdlog::error("[GuildManager] Failed to add member to database");
        return false;
    }

    // 更新缓存
    {
        std::unique_lock<std::shared_mutex> lock2(mutex_);

        GuildMemberInfo member_info;
        member_info.character_id = character_id;
        member_info.guild_id = guild_id;
        member_info.position = GuildPosition::kMember;
        member_info.join_time = std::chrono::system_clock::now();
        member_info.contribution = 0;
        member_info.level = 1;
        member_info.character_name = "";

        guild_members_[guild_id][character_id] = member_info;
        character_to_guild_[character_id] = guild_id;
        guilds_[guild_id].current_members++;
    }

    spdlog::info("[GuildManager] Character {} joined guild {} successfully",
                character_id, guild_id);
    return true;
}

bool GuildManager::LeaveGuild(uint64_t guild_id, uint64_t character_id) {
    spdlog::info("[GuildManager] Character {} leaving guild {}", character_id, guild_id);

    // 对应 legacy: CGuildManager::SecedeSyn()
    // 成员主动离开公会

    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto members_it = guild_members_.find(guild_id);
    if (members_it == guild_members_.end()) {
        spdlog::error("[GuildManager] Guild not found: {}", guild_id);
        return false;
    }

    auto member_it = members_it->second.find(character_id);
    if (member_it == members_it->second.end()) {
        spdlog::warn("[GuildManager] Character {} not in guild {}", character_id, guild_id);
        return false;
    }

    // 会长不能离开,只能解散公会或转让会长
    if (member_it->second.position == GuildPosition::kGuildMaster) {
        spdlog::warn("[GuildManager] Guild master cannot leave, must disband or transfer");
        return false;
    }

    lock.unlock();

    return RemoveMember(guild_id, character_id);
}

bool GuildManager::KickMember(uint64_t guild_id, uint64_t character_id) {
    spdlog::info("[GuildManager] Kicking character {} from guild {}", character_id, guild_id);

    // 对应 legacy: CGuildManager::DeleteMemberSyn() (GuildManager.cpp:1690)

    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto members_it = guild_members_.find(guild_id);
    if (members_it == guild_members_.end()) {
        spdlog::error("[GuildManager] Guild not found: {}", guild_id);
        return false;
    }

    auto member_it = members_it->second.find(character_id);
    if (member_it == members_it->second.end()) {
        spdlog::warn("[GuildManager] Character {} not in guild {}", character_id, guild_id);
        return false;
    }

    // 不能踢出会长
    if (member_it->second.position == GuildPosition::kGuildMaster) {
        spdlog::warn("[GuildManager] Cannot kick guild master");
        return false;
    }

    lock.unlock();

    return RemoveMember(guild_id, character_id);
}

bool GuildManager::RemoveMember(uint64_t guild_id, uint64_t character_id) {
    // 内部方法, 不检查权限, 直接移除

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto members_it = guild_members_.find(guild_id);
    if (members_it == guild_members_.end()) {
        spdlog::error("[GuildManager] Guild not found: {}", guild_id);
        return false;
    }

    auto member_it = members_it->second.find(character_id);
    if (member_it == members_it->second.end()) {
        spdlog::warn("[GuildManager] Character {} not in guild {}", character_id, guild_id);
        return false;
    }

    // 从缓存移除
    members_it->second.erase(member_it);
    character_to_guild_.erase(character_id);

    if (guilds_.find(guild_id) != guilds_.end()) {
        guilds_[guild_id].current_members--;
    }

    lock.unlock();

    // 从数据库移除
    if (!GuildDAO::RemoveMember(guild_id, character_id)) {
        spdlog::error("[GuildManager] Failed to remove member from database");
        return false;
    }

    spdlog::info("[GuildManager] Character {} removed from guild {} successfully",
                character_id, guild_id);
    return true;
}

bool GuildManager::SetMemberPosition(uint64_t guild_id, uint64_t character_id,
                                     GuildPosition position) {
    spdlog::info("[GuildManager] Setting character {} position in guild {} to {}",
                character_id, guild_id, static_cast<int>(position));

    // 对应 legacy: CGuildManager::ChangeMemberRank()

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto members_it = guild_members_.find(guild_id);
    if (members_it == guild_members_.end()) {
        spdlog::error("[GuildManager] Guild not found: {}", guild_id);
        return false;
    }

    auto member_it = members_it->second.find(character_id);
    if (member_it == members_it->second.end()) {
        spdlog::error("[GuildManager] Character {} not in guild {}", character_id, guild_id);
        return false;
    }

    lock.unlock();

    // 更新数据库
    if (!GuildDAO::UpdateMemberPosition(guild_id, character_id, position)) {
        spdlog::error("[GuildManager] Failed to update member position in database");
        return false;
    }

    // 更新缓存
    {
        std::unique_lock<std::shared_mutex> lock2(mutex_);
        member_it = members_it->second.find(character_id);
        if (member_it != members_it->second.end()) {
            member_it->second.position = position;

            // 如果是新的会长, 更新公会信息
            if (position == GuildPosition::kGuildMaster) {
                guilds_[guild_id].leader_id = character_id;
                GuildDAO::SaveGuild(guilds_[guild_id]);
            }
        }
    }

    spdlog::info("[GuildManager] Character {} position in guild {} set to {}",
                character_id, guild_id, static_cast<int>(position));
    return true;
}

bool GuildManager::TransferLeadership(uint64_t guild_id, uint64_t new_leader_id) {
    spdlog::info("[GuildManager] Transferring leadership of guild {} to character {}",
                guild_id, new_leader_id);

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto guild_it = guilds_.find(guild_id);
    if (guild_it == guilds_.end()) {
        spdlog::error("[GuildManager] Guild not found: {}", guild_id);
        return false;
    }

    uint64_t old_leader_id = guild_it->second.leader_id;

    auto members_it = guild_members_.find(guild_id);
    if (members_it == guild_members_.end()) {
        spdlog::error("[GuildManager] Members not found for guild: {}", guild_id);
        return false;
    }

    auto new_leader_it = members_it->second.find(new_leader_id);
    if (new_leader_it == members_it->second.end()) {
        spdlog::error("[GuildManager] New leader not found in guild: {}", new_leader_id);
        return false;
    }

    auto old_leader_it = members_it->second.find(old_leader_id);
    if (old_leader_it != members_it->second.end()) {
        // 老会长降为副会长
        old_leader_it->second.position = GuildPosition::kViceMaster;
    }

    // 新会长设为会长
    new_leader_it->second.position = GuildPosition::kGuildMaster;
    guild_it->second.leader_id = new_leader_id;

    lock.unlock();

    // 更新数据库
    if (old_leader_it != members_it->second.end()) {
        GuildDAO::UpdateMemberPosition(guild_id, old_leader_id,
                                       GuildPosition::kViceMaster);
    }
    GuildDAO::UpdateMemberPosition(guild_id, new_leader_id,
                                   GuildPosition::kGuildMaster);
    GuildDAO::SaveGuild(guild_it->second);

    spdlog::info("[GuildManager] Leadership transferred from {} to {} in guild {}",
                old_leader_id, new_leader_id, guild_id);
    return true;
}

// ============================================================================
// 公会信息更新
// ============================================================================

bool GuildManager::UpdateGuildNotice(uint64_t guild_id, const std::string& notice) {
    spdlog::info("[GuildManager] Updating guild {} notice", guild_id);

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = guilds_.find(guild_id);
    if (it == guilds_.end()) {
        spdlog::error("[GuildManager] Guild not found: {}", guild_id);
        return false;
    }

    it->second.notice = notice;

    lock.unlock();

    // 更新数据库
    if (!GuildDAO::UpdateGuildNotice(guild_id, notice)) {
        spdlog::error("[GuildManager] Failed to update notice in database");
        return false;
    }

    spdlog::info("[GuildManager] Guild {} notice updated", guild_id);
    return true;
}

bool GuildManager::AddGuildExp(uint64_t guild_id, uint32_t exp) {
    spdlog::debug("[GuildManager] Adding {} exp to guild {}", exp, guild_id);

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = guilds_.find(guild_id);
    if (it == guilds_.end()) {
        spdlog::error("[GuildManager] Guild not found: {}", guild_id);
        return false;
    }

    it->second.exp += exp;

    // 检查是否升级
    bool leveled_up = CheckLevelUp(guild_id);

    lock.unlock();

    // 保存到数据库
    GuildDAO::SaveGuild(it->second);

    if (leveled_up) {
        spdlog::info("[GuildManager] Guild {} leveled up to level {}!",
                    guild_id, it->second.level);
    }

    return true;
}

bool GuildManager::CheckLevelUp(uint64_t guild_id) {
    // 对应 legacy: CGuild::LevelUp() (Guild.cpp)
    // 简化的升级公式: 每级需要 exp = level * 1000
    auto it = guilds_.find(guild_id);
    if (it == guilds_.end()) {
        return false;
    }

    uint32_t required_exp = it->second.level * 1000;
    if (it->second.exp >= required_exp) {
        it->second.level++;
        it->second.exp -= required_exp;

        // 每级增加5个成员上限
        it->second.max_members = 50 + (it->second.level - 1) * 5;

        spdlog::info("[GuildManager] Guild {} leveled up to {}! Max members: {}",
                    guild_id, it->second.level, it->second.max_members);
        return true;
    }

    return false;
}

// ============================================================================
// 查询操作
// ============================================================================

std::optional<GuildInfo> GuildManager::GetGuildInfo(uint64_t guild_id) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = guilds_.find(guild_id);
    if (it != guilds_.end()) {
        return it->second;
    }

    // 缓存未命中, 从数据库加载
    lock.unlock();

    auto guild_info = GuildDAO::LoadGuild(guild_id);
    if (guild_info) {
        std::unique_lock<std::shared_mutex> write_lock(mutex_);
        guilds_[guild_id] = *guild_info;
    }

    return guild_info;
}

std::vector<GuildMemberInfo> GuildManager::GetGuildMembers(uint64_t guild_id) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<GuildMemberInfo> result;

    auto it = guild_members_.find(guild_id);
    if (it != guild_members_.end()) {
        result.reserve(it->second.size());
        for (const auto& [character_id, member_info] : it->second) {
            result.push_back(member_info);
        }
    }

    spdlog::debug("[GuildManager] Returning {} members for guild {}",
                 result.size(), guild_id);
    return result;
}

std::optional<GuildMemberInfo> GuildManager::GetMemberInfo(uint64_t character_id) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = character_to_guild_.find(character_id);
    if (it == character_to_guild_.end()) {
        return std::nullopt;
    }

    uint64_t guild_id = it->second;
    auto members_it = guild_members_.find(guild_id);
    if (members_it == guild_members_.end()) {
        return std::nullopt;
    }

    auto member_it = members_it->second.find(character_id);
    if (member_it != members_it->second.end()) {
        return member_it->second;
    }

    return std::nullopt;
}

bool GuildManager::IsCharacterInGuild(uint64_t character_id) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    return character_to_guild_.find(character_id) != character_to_guild_.end();
}

uint64_t GuildManager::GetCharacterGuildId(uint64_t character_id) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = character_to_guild_.find(character_id);
    if (it != character_to_guild_.end()) {
        return it->second;
    }

    return 0;
}

bool GuildManager::GuildNameExists(const std::string& guild_name) {
    // 先检查缓存
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [guild_id, guild_info] : guilds_) {
            if (guild_info.guild_name == guild_name) {
                return true;
            }
        }
    }

    // 缓存未命中, 检查数据库
    return GuildDAO::GuildNameExists(guild_name);
}

bool GuildManager::IsGuildFull(uint64_t guild_id) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = guilds_.find(guild_id);
    if (it == guilds_.end()) {
        return true;  // 公会不存在, 视为已满
    }

    return it->second.current_members >= it->second.max_members;
}

bool GuildManager::SaveToDatabase(uint64_t guild_id) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = guilds_.find(guild_id);
    if (it == guilds_.end()) {
        return false;
    }

    lock.unlock();

    return GuildDAO::SaveGuild(it->second);
}

} // namespace Game
} // namespace Murim
