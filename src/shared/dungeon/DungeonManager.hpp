#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <mutex>
#include "protocol/dungeon.pb.h"
#include "core/database/DatabasePool.hpp"
#include "core/database/libpq_wrapper.hpp"

namespace Murim {
namespace Game {

/**
 * @brief 副本定义
 * 副本的基础配置数据
 */
struct DungeonDefinition {
    uint32_t dungeon_id = 0;             // 副本ID
    std::string name;                    // 副本名称
    std::string description;             // 描述
    murim::DungeonType type;             // 副本类型
    uint32_t min_level = 1;              // 最低等级要求
    uint32_t max_level = 0;              // 最高等级要求（0=无限制）
    uint32_t min_players = 1;            // 最少玩家数
    uint32_t max_players = 1;            // 最多玩家数

    // 推荐等级
    uint32_t recommend_level = 1;        // 推荐等级
    uint32_t recommend_item_level = 0;   // 推荐装备等级

    // 副本奖励
    std::vector<uint32_t> normal_rewards;      // 普通奖励物品ID
    std::vector<uint32_t> first_clear_rewards; // 首通奖励物品ID
    uint32_t exp_reward = 0;              // 经验奖励
    uint32_t gold_reward = 0;              // 金币奖励

    // 副本扫荡
    bool can_sweep = false;              // 是否可扫荡
    uint32_t sweep_unlock_clear_count = 0; // 扫荡解锁所需通关次数

    // 刷新时间
    uint32_t daily_reset_time = 0;       // 每日重置时间（小时）
    uint32_t weekly_reset_time = 0;      // 每周重置时间（小时）
};

/**
 * @brief 副本进度
 * 玩家的副本通关记录
 */
struct DungeonProgress {
    uint32_t dungeon_id = 0;             // 副本ID
    murim::DungeonDifficulty difficulty; // 难度
    murim::DungeonStatus state;          // 状态
    uint32_t clear_time = 0;             // 通关用时（秒）
    uint32_t progress = 0;               // 进度（0-100）
    uint32_t best_clear_time = 0;        // 最佳通关时间
    uint32_t clear_count = 0;            // 通关次数
    uint32_t today_clear_count = 0;      // 今日通关次数
    uint32_t today_first_clear_difficulty = 0; // 今日首通难度
    uint32_t weekly_clear_count = 0;     // 本周通关次数
};

/**
 * @brief 副本队伍
 * 副本队伍信息
 */
struct DungeonTeam {
    uint64_t team_id = 0;                // 队伍ID
    uint64_t leader_id = 0;              // 队长ID
    std::vector<uint64_t> member_ids;    // 队员ID列表
    uint32_t max_members = 1;            // 最大队员数
    uint32_t ready_count = 0;            // 准备就绪人数
    bool all_ready = false;              // 是否全部准备就绪
};

/**
 * @brief 副本房间
 * 副本房间信息
 */
struct DungeonRoom {
    uint64_t room_id = 0;                // 房间ID
    uint32_t dungeon_id = 0;             // 副本ID
    murim::DungeonDifficulty difficulty; // 难度
    DungeonTeam team;                    // 队伍信息
    uint32_t created_time = 0;           // 创建时间
    uint32_t expire_time = 0;            // 过期时间（秒）
    bool started = false;                // 是否已开始
};

/**
 * @brief 副本管理器
 *
 * 负责副本系统的管理，包括：
 * - 副本定义管理
 * - 副本进度跟踪
 * - 副本房间管理
 * - 副本队伍管理
 * - 副本扫荡
 * - 副本奖励
 *
 * 对应 legacy: DungeonManager / GameDungeon
 */
class DungeonManager {
public:
    /**
     * @brief 获取单例实例
     */
    static DungeonManager& Instance();

    // ========== 初始化 ==========

    /**
     * @brief 初始化副本管理器
     * @return 是否初始化成功
     */
    bool Initialize();

    /**
     * @brief 关闭副本管理器
     */
    void Shutdown();

    // ========== 副本定义管理 ==========

    /**
     * @brief 获取副本定义
     * @param dungeon_id 副本ID
     * @return 副本定义（如果存在）
     */
    const DungeonDefinition* GetDungeonDefinition(uint32_t dungeon_id) const;

    /**
     * @brief 获取所有副本定义
     * @return 副本定义列表
     */
    std::vector<const DungeonDefinition*> GetAllDungeonDefinitions() const;

    /**
     * @brief 检查副本定义是否存在
     * @param dungeon_id 副本ID
     * @return 是否存在
     */
    bool HasDungeonDefinition(uint32_t dungeon_id) const;

    /**
     * @brief 添加副本定义
     * @param definition 副本定义
     * @return 是否成功
     */
    bool AddDungeonDefinition(const DungeonDefinition& definition);

    /**
     * @brief 移除副本定义
     * @param dungeon_id 副本ID
     * @return 是否成功
     */
    bool RemoveDungeonDefinition(uint32_t dungeon_id);

    // ========== 副本进度管理 ==========

    /**
     * @brief 获取玩家副本进度
     * @param character_id 角色ID
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @return 副本进度（如果存在）
     */
    const DungeonProgress* GetDungeonProgress(
        uint64_t character_id,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty
    ) const;

    /**
     * @brief 获取玩家所有副本进度
     * @param character_id 角色ID
     * @return 副本进度列表
     */
    std::vector<const DungeonProgress*> GetAllDungeonProgress(uint64_t character_id) const;

    /**
     * @brief 加载玩家副本进度
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool LoadDungeonProgress(uint64_t character_id);

    /**
     * @brief 保存玩家副本进度
     * @param character_id 角色ID
     * @param progress 副本进度
     * @return 是否成功
     */
    bool SaveDungeonProgress(uint64_t character_id, const DungeonProgress& progress);

    /**
     * @brief 更新副本进度
     * @param character_id 角色ID
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @param state 状态
     * @param clear_time 通关用时
     * @param progress 进度
     * @return 是否成功
     */
    bool UpdateDungeonProgress(
        uint64_t character_id,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty,
        murim::DungeonStatus state,
        uint32_t clear_time,
        uint32_t progress
    );

    /**
     * @brief 记录副本通关
     * @param character_id 角色ID
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @param clear_time 通关用时
     * @return 是否首通
     */
    bool RecordDungeonClear(
        uint64_t character_id,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty,
        uint32_t clear_time
    );

    /**
     * @brief 检查是否可以扫荡
     * @param character_id 角色ID
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @return 是否可以扫荡
     */
    bool CanSweep(
        uint64_t character_id,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty
    ) const;

    /**
     * @brief 获取剩余扫荡次数
     * @param character_id 角色ID
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @return 剩余扫荡次数
     */
    uint32_t GetRemainingSweepCount(
        uint64_t character_id,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty
    ) const;

    // ========== 副本房间管理 ==========

    /**
     * @brief 创建副本房间
     * @param character_id 创建者角色ID
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @return 房间ID（0表示失败）
     */
    uint64_t CreateDungeonRoom(
        uint64_t character_id,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty
    );

    /**
     * @brief 获取副本房间
     * @param room_id 房间ID
     * @return 副本房间（如果存在）
     */
    const DungeonRoom* GetDungeonRoom(uint64_t room_id) const;

    /**
     * @brief 加入副本房间
     * @param room_id 房间ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool JoinDungeonRoom(uint64_t room_id, uint64_t character_id);

    /**
     * @brief 离开副本房间
     * @param room_id 房间ID
     * @param character_id 角色ID
     * @return 是否成功
     */
    bool LeaveDungeonRoom(uint64_t room_id, uint64_t character_id);

    /**
     * @brief 删除副本房间
     * @param room_id 房间ID
     * @return 是否成功
     */
    bool RemoveDungeonRoom(uint64_t room_id);

    // ========== 副本队伍管理 ==========

    /**
     * @brief 设置准备状态
     * @param room_id 房间ID
     * @param character_id 角色ID
     * @param ready 是否准备就绪
     * @return 是否成功
     */
    bool SetReadyStatus(uint64_t room_id, uint64_t character_id, bool ready);

    /**
     * @brief 检查是否所有队员都准备就绪
     * @param room_id 房间ID
     * @return 是否全部准备就绪
     */
    bool IsAllReady(uint64_t room_id) const;

    /**
     * @brief 获取准备就绪人数
     * @param room_id 房间ID
     * @return 准备就绪人数
     */
    uint32_t GetReadyCount(uint64_t room_id) const;

    // ========== 副本开始和完成 ==========

    /**
     * @brief 开始副本
     * @param room_id 房间ID
     * @return 是否成功
     */
    bool StartDungeon(uint64_t room_id);

    /**
     * @brief 完成副本
     * @param room_id 房间ID
     * @param clear_time 通关用时
     * @return 是否成功
     */
    bool CompleteDungeon(uint64_t room_id, uint32_t clear_time);

    /**
     * @brief 副本失败
     * @param room_id 房间ID
     * @return 是否成功
     */
    bool FailDungeon(uint64_t room_id);

    // ========== 副本扫荡 ==========

    /**
     * @brief 扫荡副本
     * @param character_id 角色ID
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @param count 扫荡次数
     * @param out_items 输出：获得的物品ID列表
     * @param out_item_counts 输出：物品数量列表
     * @param out_exp 输出：获得的经验
     * @param out_gold 输出：获得的金币
     * @return 是否成功
     */
    bool SweepDungeon(
        uint64_t character_id,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty,
        uint32_t count,
        std::vector<uint32_t>& out_items,
        std::vector<uint32_t>& out_item_counts,
        uint32_t& out_exp,
        uint32_t& out_gold
    );

    // ========== 副本奖励 ==========

    /**
     * @brief 分配副本奖励
     * @param character_id 角色ID
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @param first_clear 是否首通
     * @param out_items 输出：奖励物品ID列表
     * @param out_item_counts 输出：物品数量列表
     * @param out_exp 输出：经验奖励
     * @param out_gold 输出：金币奖励
     * @return 是否成功
     */
    bool DistributeDungeonRewards(
        uint64_t character_id,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty,
        bool first_clear,
        std::vector<uint32_t>& out_items,
        std::vector<uint32_t>& out_item_counts,
        uint32_t& out_exp,
        uint32_t& out_gold
    );

    // ========== 定时器 ==========

    /**
     * @brief 更新副本系统（定时器）
     * @param delta_time 时间增量（毫秒）
     */
    void Update(uint32_t delta_time);

    /**
     * @brief 每日重置
     */
    void DailyReset();

    /**
     * @brief 每周重置
     */
    void WeeklyReset();

    // ========== 数据库操作 ==========

    /**
     * @brief 从数据库加载副本定义
     * @return 是否成功
     */
    bool LoadDungeonDefinitionsFromDB();

    /**
     * @brief 保存副本定义到数据库
     * @param definition 副本定义
     * @return 是否成功
     */
    bool SaveDungeonDefinitionToDB(const DungeonDefinition& definition);

private:
    DungeonManager() = default;
    ~DungeonManager() = default;
    DungeonManager(const DungeonManager&) = delete;
    DungeonManager& operator=(const DungeonManager&) = delete;

    // ========== 数据成员 ==========

    mutable std::mutex mutex_;                                      // 互斥锁

    std::unordered_map<uint32_t, DungeonDefinition> definitions_;  // 副本定义 (dungeon_id -> definition)
    std::unordered_map<uint64_t, DungeonProgress> progress_;        // 副本进度 (composite_key -> progress)
    std::unordered_map<uint64_t, DungeonRoom> rooms_;              // 副本房间 (room_id -> room)

    uint64_t next_room_id_ = 1;                                     // 下一个房间ID
    uint32_t tick_counter_ = 0;                                     // tick计数器
    uint32_t daily_reset_counter_ = 0;                              // 每日重置计数器
    uint32_t weekly_reset_counter_ = 0;                             // 每周重置计数器

    // ========== 辅助方法 ==========

    /**
     * @brief 生成进度复合键
     * @param character_id 角色ID
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @return 复合键
     */
    uint64_t MakeProgressKey(
        uint64_t character_id,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty
    ) const;

    /**
     * @brief 生成房间ID
     * @return 房间ID
     */
    uint64_t GenerateRoomID();

    /**
     * @brief 清理过期房间
     */
    void CleanupExpiredRooms();

    /**
     * @brief 清理已开始的房间
     * @param room_id 房间ID
     */
    void CleanupStartedRoom(uint64_t room_id);
};

} // namespace Game
} // namespace Murim
