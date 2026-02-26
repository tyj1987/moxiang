#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <algorithm>
#include <queue>
#include "protocol/leaderboard.pb.h"

namespace Murim {
namespace Game {

/**
 * @brief 排行榜条目（角色排行）
 */
struct LeaderboardEntry {
    uint64_t character_id;
    std::string character_name;
    uint32_t job_class;
    uint32_t level;
    uint64_t score;
    uint32_t rank;
    uint64_t guild_id;
    std::string guild_name;
    uint64_t update_time;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::LeaderboardEntry ToProto() const;
};

/**
 * @brief 排行榜条目（帮派排行）
 */
struct GuildLeaderboardEntry {
    uint64_t guild_id;
    std::string guild_name;
    std::string master_name;
    uint32_t member_count;
    uint64_t total_power;
    uint64_t total_wealth;
    uint32_t level;
    uint32_t rank;
    uint64_t update_time;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::GuildLeaderboardEntry ToProto() const;
};

/**
 * @brief 排行榜条目（副本排行）
 */
struct DungeonLeaderboardEntry {
    uint64_t character_id;
    std::string character_name;
    uint32_t job_class;
    uint32_t level;
    uint32_t dungeon_id;
    std::string dungeon_name;
    uint32_t clear_time;
    uint32_t score;
    uint32_t rank;
    uint64_t clear_timestamp;
    uint64_t update_time;

    /**
     * @brief 转换为 Protobuf 消息
     */
    murim::DungeonLeaderboardEntry ToProto() const;
};

/**
 * @brief 排行榜数据（角色）
 */
struct CharacterLeaderboard {
    murim::LeaderboardType type;
    std::vector<LeaderboardEntry> entries;  // 排序后的条目
    std::unordered_map<uint64_t, uint32_t> character_to_rank;  // character_id -> rank

    /**
     * @brief 更新排名
     */
    void UpdateRanks();

    /**
     * @brief 获取排名
     */
    uint32_t GetRank(uint64_t character_id) const;

    /**
     * @brief 添加或更新条目
     */
    void AddOrUpdateEntry(const LeaderboardEntry& entry);

    /**
     * @brief 移除条目
     */
    void RemoveEntry(uint64_t character_id);

    /**
     * @brief 获取指定范围的条目（用于分页）
     */
    std::vector<LeaderboardEntry> GetEntries(uint32_t page, uint32_t page_size) const;

    /**
     * @brief 获取总条目数
     */
    uint32_t GetTotalEntries() const { return static_cast<uint32_t>(entries.size()); }
};

/**
 * @brief 排行榜管理器
 *
 * 负责管理各种排行榜（等级、财富、战力、帮派、副本等）
 */
class LeaderboardManager {
public:
    static LeaderboardManager& Instance();

    bool Initialize();

    // ========== 排行榜查询 ==========

    /**
     * @brief 获取排行榜（分页）
     */
    std::vector<LeaderboardEntry> GetLeaderboard(
        murim::LeaderboardType type,
        uint32_t page,
        uint32_t page_size
    );

    /**
     * @brief 获取我的排名
     */
    uint32_t GetMyRank(uint64_t character_id, murim::LeaderboardType type);

    /**
     * @brief 获取排行榜条目
     */
    LeaderboardEntry* GetEntry(uint64_t character_id, murim::LeaderboardType type);

    /**
     * @brief 搜索排行
     */
    std::vector<LeaderboardEntry> SearchLeaderboard(
        murim::LeaderboardType type,
        const std::string& search_term
    );

    // ========== 数据更新 ==========

    /**
     * @brief 更新角色等级
     */
    void UpdateLevel(uint64_t character_id, const std::string& name, uint32_t level);

    /**
     * @brief 更新角色财富（金币）
     */
    void UpdateWealth(uint64_t character_id, const std::string& name, uint64_t gold);

    /**
     * @brief 更新角色战力
     */
    void UpdatePower(uint64_t character_id, const std::string& name, uint64_t power);

    /**
     * @brief 更新击杀数
     */
    void UpdateKillCount(uint64_t character_id, const std::string& name, uint32_t kills);

    /**
     * @brief 更新死亡数
     */
    void UpdateDeathCount(uint64_t character_id, const std::string& name, uint32_t deaths);

    /**
     * @brief 添加副本记录
     */
    void AddDungeonRecord(
        uint64_t character_id,
        const std::string& name,
        uint32_t job_class,
        uint32_t level,
        uint32_t dungeon_id,
        const std::string& dungeon_name,
        uint32_t clear_time,
        uint32_t score
    );

    // ========== 帮派排行 ==========

    /**
     * @brief 更新帮派数据
     */
    void UpdateGuild(
        uint64_t guild_id,
        const std::string& guild_name,
        const std::string& master_name,
        uint32_t member_count,
        uint64_t total_power,
        uint64_t total_wealth,
        uint32_t level
    );

    /**
     * @brief 获取帮派排行榜
     */
    std::vector<GuildLeaderboardEntry> GetGuildLeaderboard(uint32_t page, uint32_t page_size);

    // ========== 副本排行 ==========

    /**
     * @brief 获取副本排行榜
     */
    std::vector<DungeonLeaderboardEntry> GetDungeonLeaderboard(
        uint32_t dungeon_id,
        uint32_t page,
        uint32_t page_size
    );

    // ========== 维护操作 ==========

    /**
     * @brief 刷新所有排行榜（定期调用，如每小时）
     */
    void RefreshAll();

    /**
     * @brief 获取排行榜统计信息
     */
    uint32_t GetTotalEntries(murim::LeaderboardType type) const;

    /**
     * @brief 清理过期数据
     */
    void CleanupOldEntries();

private:
    LeaderboardManager();
    ~LeaderboardManager() = default;
    LeaderboardManager(const LeaderboardManager&) = delete;
    LeaderboardManager& operator=(const LeaderboardManager&) = delete;

    // ========== 角色排行榜 ==========
    // type -> CharacterLeaderboard
    std::unordered_map<uint32_t, CharacterLeaderboard> character_leaderboards_;

    // ========== 帮派排行榜 ==========
    std::vector<GuildLeaderboardEntry> guild_leaderboard_;

    // ========== 副本排行榜 ==========
    // dungeon_id -> entries
    std::unordered_map<uint32_t, std::vector<DungeonLeaderboardEntry>> dungeon_leaderboards_;

    // ========== 配置 ==========
    uint32_t max_entries_per_board_ = 1000;  // 每个排行榜最大条目数
    uint32_t refresh_interval_hours_ = 1;    // 刷新间隔（小时）

    // ========== 辅助方法 ==========

    /**
     * @brief 获取或创建排行榜
     */
    CharacterLeaderboard& GetOrCreateCharacterLeaderboard(murim::LeaderboardType type);

    /**
     * @brief 比较函数（用于排序）
     */
    static bool CompareByScore(const LeaderboardEntry& a, const LeaderboardEntry& b);
    static bool CompareByClearTime(const DungeonLeaderboardEntry& a, const DungeonLeaderboardEntry& b);
    static bool CompareByGuildScore(const GuildLeaderboardEntry& a, const GuildLeaderboardEntry& b);

    /**
     * @brief 限制排行榜大小
     */
    void LimitLeaderboardSize(CharacterLeaderboard& board);
    void LimitGuildLeaderboard();
    void LimitDungeonLeaderboard(uint32_t dungeon_id);
};

} // namespace Game
} // namespace Murim
