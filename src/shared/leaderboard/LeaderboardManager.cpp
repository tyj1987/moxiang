#include "LeaderboardManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <chrono>

namespace Murim {
namespace Game {

// ========== LeaderboardEntry ==========

murim::LeaderboardEntry LeaderboardEntry::ToProto() const {
    murim::LeaderboardEntry proto;
    proto.set_character_id(character_id);
    proto.set_character_name(character_name);
    proto.set_job_class(job_class);
    proto.set_level(level);
    proto.set_score(score);
    proto.set_rank(rank);
    proto.set_guild_id(guild_id);
    proto.set_guild_name(guild_name);
    proto.set_update_time(update_time);
    return proto;
}

// ========== GuildLeaderboardEntry ==========

murim::GuildLeaderboardEntry GuildLeaderboardEntry::ToProto() const {
    murim::GuildLeaderboardEntry proto;
    proto.set_guild_id(guild_id);
    proto.set_guild_name(guild_name);
    proto.set_master_name(master_name);
    proto.set_member_count(member_count);
    proto.set_total_power(total_power);
    proto.set_total_wealth(total_wealth);
    proto.set_level(level);
    proto.set_rank(rank);
    proto.set_update_time(update_time);
    return proto;
}

// ========== DungeonLeaderboardEntry ==========

murim::DungeonLeaderboardEntry DungeonLeaderboardEntry::ToProto() const {
    murim::DungeonLeaderboardEntry proto;
    proto.set_character_id(character_id);
    proto.set_character_name(character_name);
    proto.set_job_class(job_class);
    proto.set_level(level);
    proto.set_dungeon_id(dungeon_id);
    proto.set_dungeon_name(dungeon_name);
    proto.set_clear_time(clear_time);
    proto.set_score(score);
    proto.set_rank(rank);
    proto.set_clear_timestamp(clear_timestamp);
    proto.set_update_time(update_time);
    return proto;
}

// ========== CharacterLeaderboard ==========

void CharacterLeaderboard::UpdateRanks() {
    // 按分数降序排序
    std::sort(entries.begin(), entries.end(),
        [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
            return a.score > b.score;
        });

    // 更新排名
    character_to_rank.clear();
    for (size_t i = 0; i < entries.size(); ++i) {
        entries[i].rank = static_cast<uint32_t>(i) + 1;
        character_to_rank[entries[i].character_id] = entries[i].rank;
    }
}

uint32_t CharacterLeaderboard::GetRank(uint64_t character_id) const {
    auto it = character_to_rank.find(character_id);
    if (it != character_to_rank.end()) {
        return it->second;
    }
    return 0;  // 未上榜
}

void CharacterLeaderboard::AddOrUpdateEntry(const LeaderboardEntry& entry) {
    // 查找是否已存在
    auto it = std::find_if(entries.begin(), entries.end(),
        [&entry](const LeaderboardEntry& e) {
            return e.character_id == entry.character_id;
        });

    if (it != entries.end()) {
        // 更新现有条目
        *it = entry;
    } else {
        // 添加新条目
        entries.push_back(entry);
    }

    UpdateRanks();
}

void CharacterLeaderboard::RemoveEntry(uint64_t character_id) {
    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
            [character_id](const LeaderboardEntry& e) {
                return e.character_id == character_id;
            }),
        entries.end()
    );

    UpdateRanks();
}

std::vector<LeaderboardEntry> CharacterLeaderboard::GetEntries(
    uint32_t page,
    uint32_t page_size) const {

    std::vector<LeaderboardEntry> result;

    uint32_t start = (page - 1) * page_size;
    if (start >= entries.size()) {
        return result;
    }

    uint32_t end = std::min(start + page_size, static_cast<uint32_t>(entries.size()));

    for (uint32_t i = start; i < end; ++i) {
        result.push_back(entries[i]);
    }

    return result;
}

// ========== LeaderboardManager ==========

LeaderboardManager& LeaderboardManager::Instance() {
    static LeaderboardManager instance;
    return instance;
}

LeaderboardManager::LeaderboardManager() {
    // 预创建排行榜
    character_leaderboards_[murim::LeaderboardType::LEADERBOARD_LEVEL] = {
        murim::LeaderboardType::LEADERBOARD_LEVEL, {}, {}
    };
    character_leaderboards_[murim::LeaderboardType::LEADERBOARD_WEALTH] = {
        murim::LeaderboardType::LEADERBOARD_WEALTH, {}, {}
    };
    character_leaderboards_[murim::LeaderboardType::LEADERBOARD_POWER] = {
        murim::LeaderboardType::LEADERBOARD_POWER, {}, {}
    };
    character_leaderboards_[murim::LeaderboardType::LEADERBOARD_KILL_COUNT] = {
        murim::LeaderboardType::LEADERBOARD_KILL_COUNT, {}, {}
    };
    character_leaderboards_[murim::LeaderboardType::LEADERBOARD_DEATH_COUNT] = {
        murim::LeaderboardType::LEADERBOARD_DEATH_COUNT, {}, {}
    };
    character_leaderboards_[murim::LeaderboardType::LEADERBOARD_ACHIEVEMENT] = {
        murim::LeaderboardType::LEADERBOARD_ACHIEVEMENT, {}, {}
    };
}

bool LeaderboardManager::Initialize() {
    spdlog::info("Initializing LeaderboardManager...");
    spdlog::info("LeaderboardManager initialized successfully");
    return true;
}

// ========== 排行榜查询 ==========

std::vector<LeaderboardEntry> LeaderboardManager::GetLeaderboard(
    murim::LeaderboardType type,
    uint32_t page,
    uint32_t page_size) {

    auto it = character_leaderboards_.find(static_cast<uint32_t>(type));
    if (it == character_leaderboards_.end()) {
        spdlog::warn("Leaderboard type not found: {}", static_cast<uint32_t>(type));
        return {};
    }

    return it->second.GetEntries(page, page_size);
}

uint32_t LeaderboardManager::GetMyRank(uint64_t character_id, murim::LeaderboardType type) {
    auto it = character_leaderboards_.find(static_cast<uint32_t>(type));
    if (it == character_leaderboards_.end()) {
        return 0;
    }

    return it->second.GetRank(character_id);
}

LeaderboardEntry* LeaderboardManager::GetEntry(
    uint64_t character_id,
    murim::LeaderboardType type) {

    auto it = character_leaderboards_.find(static_cast<uint32_t>(type));
    if (it == character_leaderboards_.end()) {
        return nullptr;
    }

    auto& entries = it->second.entries;
    auto entry_it = std::find_if(entries.begin(), entries.end(),
        [character_id](const LeaderboardEntry& e) {
            return e.character_id == character_id;
        });

    if (entry_it != entries.end()) {
        return &(*entry_it);
    }

    return nullptr;
}

std::vector<LeaderboardEntry> LeaderboardManager::SearchLeaderboard(
    murim::LeaderboardType type,
    const std::string& search_term) {

    std::vector<LeaderboardEntry> results;

    auto it = character_leaderboards_.find(static_cast<uint32_t>(type));
    if (it == character_leaderboards_.end()) {
        return results;
    }

    for (const auto& entry : it->second.entries) {
        if (entry.character_name.find(search_term) != std::string::npos) {
            results.push_back(entry);
            if (results.size() >= 10) {
                break;  // 最多返回10条
            }
        }
    }

    return results;
}

// ========== 数据更新 ==========

void LeaderboardManager::UpdateLevel(
    uint64_t character_id,
    const std::string& name,
    uint32_t level) {

    auto& board = GetOrCreateCharacterLeaderboard(murim::LeaderboardType::LEADERBOARD_LEVEL);

    LeaderboardEntry entry;
    entry.character_id = character_id;
    entry.character_name = name;
    entry.level = level;
    entry.score = level;
    entry.update_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    board.AddOrUpdateEntry(entry);
    LimitLeaderboardSize(board);

    spdlog::debug("Updated level leaderboard: character_id={}, level={}", character_id, level);
}

void LeaderboardManager::UpdateWealth(
    uint64_t character_id,
    const std::string& name,
    uint64_t gold) {

    auto& board = GetOrCreateCharacterLeaderboard(murim::LeaderboardType::LEADERBOARD_WEALTH);

    LeaderboardEntry entry;
    entry.character_id = character_id;
    entry.character_name = name;
    entry.score = gold;
    entry.update_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    board.AddOrUpdateEntry(entry);
    LimitLeaderboardSize(board);

    spdlog::debug("Updated wealth leaderboard: character_id={}, gold={}", character_id, gold);
}

void LeaderboardManager::UpdatePower(
    uint64_t character_id,
    const std::string& name,
    uint64_t power) {

    auto& board = GetOrCreateCharacterLeaderboard(murim::LeaderboardType::LEADERBOARD_POWER);

    LeaderboardEntry entry;
    entry.character_id = character_id;
    entry.character_name = name;
    entry.score = power;
    entry.update_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    board.AddOrUpdateEntry(entry);
    LimitLeaderboardSize(board);

    spdlog::debug("Updated power leaderboard: character_id={}, power={}", character_id, power);
}

void LeaderboardManager::UpdateKillCount(
    uint64_t character_id,
    const std::string& name,
    uint32_t kills) {

    auto& board = GetOrCreateCharacterLeaderboard(murim::LeaderboardType::LEADERBOARD_KILL_COUNT);

    LeaderboardEntry entry;
    entry.character_id = character_id;
    entry.character_name = name;
    entry.score = kills;
    entry.update_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    board.AddOrUpdateEntry(entry);
    LimitLeaderboardSize(board);

    spdlog::debug("Updated kill count leaderboard: character_id={}, kills={}", character_id, kills);
}

void LeaderboardManager::UpdateDeathCount(
    uint64_t character_id,
    const std::string& name,
    uint32_t deaths) {

    auto& board = GetOrCreateCharacterLeaderboard(murim::LeaderboardType::LEADERBOARD_DEATH_COUNT);

    LeaderboardEntry entry;
    entry.character_id = character_id;
    entry.character_name = name;
    entry.score = deaths;
    entry.update_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    board.AddOrUpdateEntry(entry);
    LimitLeaderboardSize(board);

    spdlog::debug("Updated death count leaderboard: character_id={}, deaths={}", character_id, deaths);
}

void LeaderboardManager::AddDungeonRecord(
    uint64_t character_id,
    const std::string& name,
    uint32_t job_class,
    uint32_t level,
    uint32_t dungeon_id,
    const std::string& dungeon_name,
    uint32_t clear_time,
    uint32_t score) {

    auto& entries = dungeon_leaderboards_[dungeon_id];

    // 检查是否已有记录
    auto it = std::find_if(entries.begin(), entries.end(),
        [character_id](const DungeonLeaderboardEntry& e) {
            return e.character_id == character_id;
        });

    if (it != entries.end()) {
        // 如果新记录更好（时间更短或分数更高），则更新
        if (clear_time < it->clear_time || score > it->score) {
            it->clear_time = clear_time;
            it->score = score;
            it->update_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            it->clear_timestamp = it->update_time;
        }
    } else {
        // 添加新记录
        DungeonLeaderboardEntry entry;
        entry.character_id = character_id;
        entry.character_name = name;
        entry.job_class = job_class;
        entry.level = level;
        entry.dungeon_id = dungeon_id;
        entry.dungeon_name = dungeon_name;
        entry.clear_time = clear_time;
        entry.score = score;
        entry.clear_timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        entry.update_time = entry.clear_timestamp;

        entries.push_back(entry);
    }

    // 重新排序
    std::sort(entries.begin(), entries.end(), CompareByClearTime);

    // 更新排名
    for (size_t i = 0; i < entries.size(); ++i) {
        entries[i].rank = static_cast<uint32_t>(i) + 1;
    }

    LimitDungeonLeaderboard(dungeon_id);

    spdlog::debug("Added dungeon record: character_id={}, dungeon_id={}, clear_time={}s",
                  character_id, dungeon_id, clear_time);
}

// ========== 帮派排行 ==========

void LeaderboardManager::UpdateGuild(
    uint64_t guild_id,
    const std::string& guild_name,
    const std::string& master_name,
    uint32_t member_count,
    uint64_t total_power,
    uint64_t total_wealth,
    uint32_t level) {

    // 查找是否已存在
    auto it = std::find_if(guild_leaderboard_.begin(), guild_leaderboard_.end(),
        [guild_id](const GuildLeaderboardEntry& e) {
            return e.guild_id == guild_id;
        });

    if (it != guild_leaderboard_.end()) {
        // 更新
        it->guild_name = guild_name;
        it->master_name = master_name;
        it->member_count = member_count;
        it->total_power = total_power;
        it->total_wealth = total_wealth;
        it->level = level;
        it->update_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    } else {
        // 添加
        GuildLeaderboardEntry entry;
        entry.guild_id = guild_id;
        entry.guild_name = guild_name;
        entry.master_name = master_name;
        entry.member_count = member_count;
        entry.total_power = total_power;
        entry.total_wealth = total_wealth;
        entry.level = level;
        entry.update_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        guild_leaderboard_.push_back(entry);
    }

    // 重新排序（按总战力）
    std::sort(guild_leaderboard_.begin(), guild_leaderboard_.end(), CompareByGuildScore);

    // 更新排名
    for (size_t i = 0; i < guild_leaderboard_.size(); ++i) {
        guild_leaderboard_[i].rank = static_cast<uint32_t>(i) + 1;
    }

    LimitGuildLeaderboard();

    spdlog::debug("Updated guild leaderboard: guild_id={}, total_power={}", guild_id, total_power);
}

std::vector<GuildLeaderboardEntry> LeaderboardManager::GetGuildLeaderboard(
    uint32_t page,
    uint32_t page_size) {

    std::vector<GuildLeaderboardEntry> result;

    uint32_t start = (page - 1) * page_size;
    if (start >= guild_leaderboard_.size()) {
        return result;
    }

    uint32_t end = std::min(start + page_size, static_cast<uint32_t>(guild_leaderboard_.size()));

    for (uint32_t i = start; i < end; ++i) {
        result.push_back(guild_leaderboard_[i]);
    }

    return result;
}

// ========== 副本排行 ==========

std::vector<DungeonLeaderboardEntry> LeaderboardManager::GetDungeonLeaderboard(
    uint32_t dungeon_id,
    uint32_t page,
    uint32_t page_size) {

    std::vector<DungeonLeaderboardEntry> result;

    auto it = dungeon_leaderboards_.find(dungeon_id);
    if (it == dungeon_leaderboards_.end()) {
        return result;
    }

    const auto& entries = it->second;

    uint32_t start = (page - 1) * page_size;
    if (start >= entries.size()) {
        return result;
    }

    uint32_t end = std::min(start + page_size, static_cast<uint32_t>(entries.size()));

    for (uint32_t i = start; i < end; ++i) {
        result.push_back(entries[i]);
    }

    return result;
}

// ========== 维护操作 ==========

void LeaderboardManager::RefreshAll() {
    spdlog::info("Refreshing all leaderboards...");

    // TODO: 从数据库重新加载排行榜数据

    for (auto& [type, board] : character_leaderboards_) {
        board.UpdateRanks();
    }

    std::sort(guild_leaderboard_.begin(), guild_leaderboard_.end(), CompareByGuildScore);
    for (size_t i = 0; i < guild_leaderboard_.size(); ++i) {
        guild_leaderboard_[i].rank = static_cast<uint32_t>(i) + 1;
    }

    for (auto& [dungeon_id, entries] : dungeon_leaderboards_) {
        std::sort(entries.begin(), entries.end(), CompareByClearTime);
        for (size_t i = 0; i < entries.size(); ++i) {
            entries[i].rank = static_cast<uint32_t>(i) + 1;
        }
    }

    spdlog::info("All leaderboards refreshed");
}

uint32_t LeaderboardManager::GetTotalEntries(murim::LeaderboardType type) const {
    auto it = character_leaderboards_.find(static_cast<uint32_t>(type));
    if (it != character_leaderboards_.end()) {
        return it->second.GetTotalEntries();
    }
    return 0;
}

void LeaderboardManager::CleanupOldEntries() {
    // TODO: 清理超过30天未更新的条目
    spdlog::debug("Cleaning up old leaderboard entries...");
}

// ========== 辅助方法 ==========

CharacterLeaderboard& LeaderboardManager::GetOrCreateCharacterLeaderboard(
    murim::LeaderboardType type) {

    auto it = character_leaderboards_.find(static_cast<uint32_t>(type));
    if (it == character_leaderboards_.end()) {
        CharacterLeaderboard board;
        board.type = type;
        character_leaderboards_[static_cast<uint32_t>(type)] = board;
        return character_leaderboards_[static_cast<uint32_t>(type)];
    }

    return it->second;
}

bool LeaderboardManager::CompareByScore(
    const LeaderboardEntry& a,
    const LeaderboardEntry& b) {
    return a.score > b.score;
}

bool LeaderboardManager::CompareByClearTime(
    const DungeonLeaderboardEntry& a,
    const DungeonLeaderboardEntry& b) {
    if (a.clear_time != b.clear_time) {
        return a.clear_time < b.clear_time;  // 时间越短越好
    }
    return a.score > b.score;  // 时间相同时，分数高的优先
}

bool LeaderboardManager::CompareByGuildScore(
    const GuildLeaderboardEntry& a,
    const GuildLeaderboardEntry& b) {
    return a.total_power > b.total_power;
}

void LeaderboardManager::LimitLeaderboardSize(CharacterLeaderboard& board) {
    if (board.entries.size() > max_entries_per_board_) {
        board.entries.resize(max_entries_per_board_);
        board.UpdateRanks();
    }
}

void LeaderboardManager::LimitGuildLeaderboard() {
    if (guild_leaderboard_.size() > max_entries_per_board_) {
        guild_leaderboard_.resize(max_entries_per_board_);
        for (size_t i = 0; i < guild_leaderboard_.size(); ++i) {
            guild_leaderboard_[i].rank = static_cast<uint32_t>(i) + 1;
        }
    }
}

void LeaderboardManager::LimitDungeonLeaderboard(uint32_t dungeon_id) {
    auto& entries = dungeon_leaderboards_[dungeon_id];
    if (entries.size() > max_entries_per_board_) {
        entries.resize(max_entries_per_board_);
        for (size_t i = 0; i < entries.size(); ++i) {
            entries[i].rank = static_cast<uint32_t>(i) + 1;
        }
    }
}

} // namespace Game
} // namespace Murim
