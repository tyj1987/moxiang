#include "DungeonManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace Murim {
namespace Game {

namespace {
    // 副本配置常量
    constexpr uint32_t DUNGEON_ROOM_EXPIRE_TIME = 300;      // 房间过期时间（秒）
    constexpr uint32_t DUNGEON_TICK_INTERVAL = 1000;        // 更新间隔（毫秒）
    constexpr uint32_t DAILY_RESET_HOUR = 4;                // 每日重置时间（小时）
    constexpr uint32_t WEEKLY_RESET_DAY = 1;                // 每周重置时间（星期几，1=周一）
    constexpr uint32_t WEEKLY_RESET_HOUR = 4;               // 每周重置时间（小时）
}

// ========== 单例实现 ==========

DungeonManager& DungeonManager::Instance() {
    static DungeonManager instance;
    return instance;
}

// ========== 初始化 ==========

bool DungeonManager::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 从数据库加载副本定义
    if (!LoadDungeonDefinitionsFromDB()) {
        spdlog::warn("[DungeonManager] Failed to load dungeon definitions from database");
    }

    next_room_id_ = 1;
    tick_counter_ = 0;
    daily_reset_counter_ = 0;
    weekly_reset_counter_ = 0;

    spdlog::info("[DungeonManager] Initialized with {} dungeon definitions", definitions_.size());
    return true;
}

void DungeonManager::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 清理所有房间
    rooms_.clear();

    // 保存所有进度
    // TODO: 批量保存进度到数据库

    spdlog::info("[DungeonManager] Shutdown complete");
}

// ========== 副本定义管理 ==========

const DungeonDefinition* DungeonManager::GetDungeonDefinition(uint32_t dungeon_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = definitions_.find(dungeon_id);
    if (it != definitions_.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<const DungeonDefinition*> DungeonManager::GetAllDungeonDefinitions() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<const DungeonDefinition*> result;
    result.reserve(definitions_.size());

    for (const auto& pair : definitions_) {
        result.push_back(&(pair.second));
    }

    return result;
}

bool DungeonManager::HasDungeonDefinition(uint32_t dungeon_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return definitions_.find(dungeon_id) != definitions_.end();
}

bool DungeonManager::AddDungeonDefinition(const DungeonDefinition& definition) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (definition.dungeon_id == 0) {
        spdlog::error("[DungeonManager] Invalid dungeon_id: 0");
        return false;
    }

    if (definitions_.find(definition.dungeon_id) != definitions_.end()) {
        spdlog::warn("[DungeonManager] DungeonDefinition {} already exists", definition.dungeon_id);
        return false;
    }

    definitions_[definition.dungeon_id] = definition;
    spdlog::info("[DungeonManager] Added dungeon definition: {} - {}",
        definition.dungeon_id, definition.name);

    return true;
}

bool DungeonManager::RemoveDungeonDefinition(uint32_t dungeon_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = definitions_.find(dungeon_id);
    if (it == definitions_.end()) {
        spdlog::warn("[DungeonManager] DungeonDefinition {} not found", dungeon_id);
        return false;
    }

    definitions_.erase(it);
    spdlog::info("[DungeonManager] Removed dungeon definition: {}", dungeon_id);
    return true;
}

// ========== 副本进度管理 ==========

const DungeonProgress* DungeonManager::GetDungeonProgress(
    uint64_t character_id,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty) const {

    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t key = MakeProgressKey(character_id, dungeon_id, difficulty);
    auto it = progress_.find(key);
    if (it != progress_.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<const DungeonProgress*> DungeonManager::GetAllDungeonProgress(uint64_t character_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<const DungeonProgress*> result;

    for (const auto& pair : progress_) {
        if (pair.second.dungeon_id == character_id) {
            result.push_back(&(pair.second));
        }
    }

    return result;
}

bool DungeonManager::LoadDungeonProgress(uint64_t character_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        auto conn = Core::Database::ConnectionPool::Instance().GetConnection();
        conn->BeginTransaction();

        // 查询玩家副本进度
        auto result = conn->ExecuteParams(
            "SELECT dungeon_id, difficulty, state, clear_time, progress, "
            "best_clear_time, clear_count, today_clear_count, today_first_clear_difficulty, "
            "weekly_clear_count FROM character_dungeon_progress WHERE character_id = $1",
            {std::to_string(character_id)}
        );

        if (result && result->HasData()) {
            for (int row = 0; row < result->RowCount(); ++row) {
                DungeonProgress progress;
                progress.dungeon_id = result->Get<uint32_t>(row, "dungeon_id").value_or(0);
                progress.difficulty = static_cast<murim::DungeonDifficulty>(result->Get<int>(row, "difficulty").value_or(0));
                progress.state = static_cast<murim::DungeonStatus>(result->Get<int>(row, "state").value_or(0));
                progress.clear_time = result->Get<uint32_t>(row, "clear_time").value_or(0);
                progress.progress = result->Get<uint32_t>(row, "progress").value_or(0);
                progress.best_clear_time = result->Get<uint32_t>(row, "best_clear_time").value_or(0);
                progress.clear_count = result->Get<uint32_t>(row, "clear_count").value_or(0);
                progress.today_clear_count = result->Get<uint32_t>(row, "today_clear_count").value_or(0);
                progress.today_first_clear_difficulty = result->Get<uint32_t>(row, "today_first_clear_difficulty").value_or(0);
                progress.weekly_clear_count = result->Get<uint32_t>(row, "weekly_clear_count").value_or(0);

                uint64_t key = MakeProgressKey(character_id, progress.dungeon_id, progress.difficulty);
                progress_[key] = progress;
            }
        }

        conn->Commit();
        spdlog::info("[DungeonManager] Loaded {} dungeon progress records for character {}",
            result ? result->RowCount() : 0, character_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[DungeonManager] Failed to load dungeon progress for character {}: {}",
            character_id, e.what());
        return false;
    }
}

bool DungeonManager::SaveDungeonProgress(uint64_t character_id, const DungeonProgress& progress) {
    try {
        auto conn = Core::Database::ConnectionPool::Instance().GetConnection();
        conn->BeginTransaction();

        // 插入或更新副本进度
        auto result = conn->ExecuteParams(
            "INSERT INTO character_dungeon_progress "
            "(character_id, dungeon_id, difficulty, state, clear_time, progress, "
            "best_clear_time, clear_count, today_clear_count, today_first_clear_difficulty, "
            "weekly_clear_count) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11) "
            "ON CONFLICT (character_id, dungeon_id, difficulty) "
            "DO UPDATE SET state = $4, clear_time = $5, progress = $6, "
            "best_clear_time = $7, clear_count = $8, today_clear_count = $9, "
            "today_first_clear_difficulty = $10, weekly_clear_count = $11",
            {
                std::to_string(character_id),
                std::to_string(progress.dungeon_id),
                std::to_string(static_cast<int>(progress.difficulty)),
                std::to_string(static_cast<int>(progress.state)),
                std::to_string(progress.clear_time),
                std::to_string(progress.progress),
                std::to_string(progress.best_clear_time),
                std::to_string(progress.clear_count),
                std::to_string(progress.today_clear_count),
                std::to_string(progress.today_first_clear_difficulty),
                std::to_string(progress.weekly_clear_count)
            }
        );

        conn->Commit();
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[DungeonManager] Failed to save dungeon progress for character {}: {}",
            character_id, e.what());
        return false;
    }
}

bool DungeonManager::UpdateDungeonProgress(
    uint64_t character_id,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty,
    murim::DungeonStatus state,
    uint32_t clear_time,
    uint32_t progress) {

    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t key = MakeProgressKey(character_id, dungeon_id, difficulty);

    auto it = progress_.find(key);
    if (it == progress_.end()) {
        // 创建新进度
        DungeonProgress new_progress;
        new_progress.dungeon_id = dungeon_id;
        new_progress.difficulty = difficulty;
        new_progress.state = state;
        new_progress.clear_time = clear_time;
        new_progress.progress = progress;

        progress_[key] = new_progress;
    } else {
        // 更新现有进度
        it->second.state = state;
        it->second.clear_time = clear_time;
        it->second.progress = progress;
    }

    return SaveDungeonProgress(character_id, progress_[key]);
}

bool DungeonManager::RecordDungeonClear(
    uint64_t character_id,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty,
    uint32_t clear_time) {

    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t key = MakeProgressKey(character_id, dungeon_id, difficulty);

    bool first_clear = false;
    auto it = progress_.find(key);

    if (it == progress_.end()) {
        // 首次通关
        DungeonProgress new_progress;
        new_progress.dungeon_id = dungeon_id;
        new_progress.difficulty = difficulty;
        new_progress.state = murim::DungeonStatus::DUNGEON_COMPLETED;
        new_progress.clear_time = clear_time;
        new_progress.best_clear_time = clear_time;
        new_progress.clear_count = 1;
        new_progress.today_clear_count = 1;
        new_progress.today_first_clear_difficulty = static_cast<uint32_t>(difficulty);
        new_progress.weekly_clear_count = 1;

        progress_[key] = new_progress;
        first_clear = true;
    } else {
        // 更新进度
        it->second.state = murim::DungeonStatus::DUNGEON_COMPLETED;
        it->second.clear_time = clear_time;
        it->second.clear_count++;
        it->second.today_clear_count++;
        it->second.weekly_clear_count++;

        // 更新最佳时间
        if (it->second.best_clear_time == 0 || clear_time < it->second.best_clear_time) {
            it->second.best_clear_time = clear_time;
        }

        // 检查是否今日首通
        if (it->second.today_clear_count == 1) {
            it->second.today_first_clear_difficulty = static_cast<uint32_t>(difficulty);
            first_clear = true;
        }
    }

    SaveDungeonProgress(character_id, progress_[key]);

    spdlog::info("[DungeonManager] Character {} cleared dungeon {} (difficulty {}) in {}s",
        character_id, dungeon_id, static_cast<int>(difficulty), clear_time);

    return first_clear;
}

bool DungeonManager::CanSweep(
    uint64_t character_id,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty) const {

    std::lock_guard<std::mutex> lock(mutex_);

    // 检查副本定义
    auto def_it = definitions_.find(dungeon_id);
    if (def_it == definitions_.end()) {
        return false;
    }

    const DungeonDefinition& definition = def_it->second;
    if (!definition.can_sweep) {
        return false;
    }

    // 检查是否达到扫荡解锁条件
    uint64_t key = MakeProgressKey(character_id, dungeon_id, difficulty);
    auto progress_it = progress_.find(key);

    if (progress_it == progress_.end()) {
        return false;
    }

    if (progress_it->second.clear_count < definition.sweep_unlock_clear_count) {
        return false;
    }

    return true;
}

uint32_t DungeonManager::GetRemainingSweepCount(
    uint64_t character_id,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty) const {

    std::lock_guard<std::mutex> lock(mutex_);

    // TODO: 根据VIP等级计算剩余扫荡次数
    // 默认为3次
    constexpr uint32_t MAX_SWEEP_COUNT = 3;

    uint64_t key = MakeProgressKey(character_id, dungeon_id, difficulty);
    auto progress_it = progress_.find(key);

    if (progress_it == progress_.end()) {
        return MAX_SWEEP_COUNT;
    }

    uint32_t remaining = MAX_SWEEP_COUNT - progress_it->second.today_clear_count;
    return (remaining > 0) ? remaining : 0;
}

// ========== 副本房间管理 ==========

uint64_t DungeonManager::CreateDungeonRoom(
    uint64_t character_id,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty) {

    std::lock_guard<std::mutex> lock(mutex_);

    // 检查副本定义
    auto def_it = definitions_.find(dungeon_id);
    if (def_it == definitions_.end()) {
        spdlog::warn("[DungeonManager] Dungeon definition {} not found", dungeon_id);
        return 0;
    }

    const DungeonDefinition& definition = def_it->second;

    // 创建房间
    DungeonRoom room;
    room.room_id = GenerateRoomID();
    room.dungeon_id = dungeon_id;
    room.difficulty = difficulty;
    room.created_time = static_cast<uint32_t>(std::time(nullptr));
    room.expire_time = DUNGEON_ROOM_EXPIRE_TIME;
    room.started = false;

    // 创建队伍
    room.team.team_id = room.room_id;
    room.team.leader_id = character_id;
    room.team.member_ids.push_back(character_id);
    room.team.max_members = definition.max_players;
    room.team.ready_count = 0;
    room.team.all_ready = false;

    rooms_[room.room_id] = room;

    spdlog::info("[DungeonManager] Created dungeon room {} for character {}, dungeon {}",
        room.room_id, character_id, dungeon_id);

    return room.room_id;
}

const DungeonRoom* DungeonManager::GetDungeonRoom(uint64_t room_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rooms_.find(room_id);
    if (it != rooms_.end()) {
        return &(it->second);
    }
    return nullptr;
}

bool DungeonManager::JoinDungeonRoom(uint64_t room_id, uint64_t character_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        spdlog::warn("[DungeonManager] Room {} not found", room_id);
        return false;
    }

    DungeonRoom& room = it->second;

    // 检查是否已开始
    if (room.started) {
        spdlog::warn("[DungeonManager] Room {} already started", room_id);
        return false;
    }

    // 检查是否已满员
    if (room.team.member_ids.size() >= room.team.max_members) {
        spdlog::warn("[DungeonManager] Room {} is full", room_id);
        return false;
    }

    // 检查是否已在房间中
    for (uint64_t member_id : room.team.member_ids) {
        if (member_id == character_id) {
            spdlog::warn("[DungeonManager] Character {} already in room {}", character_id, room_id);
            return false;
        }
    }

    // 加入房间
    room.team.member_ids.push_back(character_id);

    spdlog::info("[DungeonManager] Character {} joined dungeon room {}", character_id, room_id);
    return true;
}

bool DungeonManager::LeaveDungeonRoom(uint64_t room_id, uint64_t character_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        spdlog::warn("[DungeonManager] Room {} not found", room_id);
        return false;
    }

    DungeonRoom& room = it->second;

    // 检查是否已开始
    if (room.started) {
        spdlog::warn("[DungeonManager] Cannot leave room {} after start", room_id);
        return false;
    }

    // 查找并移除队员
    auto member_it = std::find(room.team.member_ids.begin(), room.team.member_ids.end(), character_id);
    if (member_it == room.team.member_ids.end()) {
        spdlog::warn("[DungeonManager] Character {} not in room {}", character_id, room_id);
        return false;
    }

    // 如果是队长离开，转移队长或解散房间
    if (room.team.leader_id == character_id) {
        if (room.team.member_ids.size() > 1) {
            // 转移队长给第一个队员
            room.team.leader_id = room.team.member_ids[0];
        } else {
            // 解散房间
            rooms_.erase(it);
            spdlog::info("[DungeonManager] Dissolved dungeon room {} (leader left)", room_id);
            return true;
        }
    }

    room.team.member_ids.erase(member_it);

    // 如果房间为空，删除房间
    if (room.team.member_ids.empty()) {
        rooms_.erase(it);
        spdlog::info("[DungeonManager] Removed empty dungeon room {}", room_id);
    } else {
        // 重置准备状态
        room.team.ready_count = 0;
        room.team.all_ready = false;
    }

    spdlog::info("[DungeonManager] Character {} left dungeon room {}", character_id, room_id);
    return true;
}

bool DungeonManager::RemoveDungeonRoom(uint64_t room_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return false;
    }

    rooms_.erase(it);
    spdlog::info("[DungeonManager] Removed dungeon room {}", room_id);
    return true;
}

// ========== 副本队伍管理 ==========

bool DungeonManager::SetReadyStatus(uint64_t room_id, uint64_t character_id, bool ready) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return false;
    }

    DungeonRoom& room = it->second;

    if (room.started) {
        return false;
    }

    // 检查是否在房间中
    bool in_room = false;
    for (uint64_t member_id : room.team.member_ids) {
        if (member_id == character_id) {
            in_room = true;
            break;
        }
    }

    if (!in_room) {
        return false;
    }

    // 更新准备状态
    if (ready) {
        room.team.ready_count++;
    } else {
        if (room.team.ready_count > 0) {
            room.team.ready_count--;
        }
    }

    // 检查是否全部准备就绪
    room.team.all_ready = (room.team.ready_count == room.team.member_ids.size());

    spdlog::debug("[DungeonManager] Character {} set ready {} in room {} ({}/{})",
        character_id, ready, room_id, room.team.ready_count, room.team.member_ids.size());

    return true;
}

bool DungeonManager::IsAllReady(uint64_t room_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return false;
    }

    return it->second.team.all_ready;
}

uint32_t DungeonManager::GetReadyCount(uint64_t room_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return 0;
    }

    return it->second.team.ready_count;
}

// ========== 副本开始和完成 ==========

bool DungeonManager::StartDungeon(uint64_t room_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        spdlog::warn("[DungeonManager] Room {} not found", room_id);
        return false;
    }

    DungeonRoom& room = it->second;

    if (room.started) {
        spdlog::warn("[DungeonManager] Room {} already started", room_id);
        return false;
    }

    if (!room.team.all_ready) {
        spdlog::warn("[DungeonManager] Room {} not all ready", room_id);
        return false;
    }

    room.started = true;

    spdlog::info("[DungeonManager] Started dungeon room {}", room_id);

    // 异步清理已开始的房间（副本完成后会由CompleteDungeon或FailDungeon处理）
    // CleanupStartedRoom(room_id);

    return true;
}

bool DungeonManager::CompleteDungeon(uint64_t room_id, uint32_t clear_time) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        spdlog::warn("[DungeonManager] Room {} not found", room_id);
        return false;
    }

    DungeonRoom& room = it->second;

    if (!room.started) {
        spdlog::warn("[DungeonManager] Room {} not started", room_id);
        return false;
    }

    // 为所有队员记录通关
    for (uint64_t member_id : room.team.member_ids) {
        RecordDungeonClear(member_id, room.dungeon_id, room.difficulty, clear_time);
    }

    spdlog::info("[DungeonManager] Completed dungeon room {} in {}s", room_id, clear_time);

    // 清理房间
    CleanupStartedRoom(room_id);

    return true;
}

bool DungeonManager::FailDungeon(uint64_t room_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        spdlog::warn("[DungeonManager] Room {} not found", room_id);
        return false;
    }

    DungeonRoom& room = it->second;

    if (!room.started) {
        spdlog::warn("[DungeonManager] Room {} not started", room_id);
        return false;
    }

    // 为所有队员更新失败状态
    for (uint64_t member_id : room.team.member_ids) {
        UpdateDungeonProgress(member_id, room.dungeon_id, room.difficulty,
            murim::DungeonStatus::DUNGEON_FAILED, 0, 0);
    }

    spdlog::info("[DungeonManager] Failed dungeon room {}", room_id);

    // 清理房间
    CleanupStartedRoom(room_id);

    return true;
}

// ========== 副本扫荡 ==========

bool DungeonManager::SweepDungeon(
    uint64_t character_id,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty,
    uint32_t count,
    std::vector<uint32_t>& out_items,
    std::vector<uint32_t>& out_item_counts,
    uint32_t& out_exp,
    uint32_t& out_gold) {

    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否可以扫荡
    if (!CanSweep(character_id, dungeon_id, difficulty)) {
        spdlog::warn("[DungeonManager] Character {} cannot sweep dungeon {}",
            character_id, dungeon_id);
        return false;
    }

    // 检查剩余扫荡次数
    uint32_t remaining_count = GetRemainingSweepCount(character_id, dungeon_id, difficulty);
    if (remaining_count < count) {
        spdlog::warn("[DungeonManager] Not enough sweep count for character {}, dungeon {}",
            character_id, dungeon_id);
        return false;
    }

    // 检查副本定义
    auto def_it = definitions_.find(dungeon_id);
    if (def_it == definitions_.end()) {
        return false;
    }

    const DungeonDefinition& definition = def_it->second;

    // 分配奖励（count次）
    out_items.clear();
    out_item_counts.clear();
    out_exp = 0;
    out_gold = 0;

    std::random_device rd;
    std::mt19937 gen(rd());

    for (uint32_t i = 0; i < count; ++i) {
        // 分配物品奖励
        for (uint32_t item_id : definition.normal_rewards) {
            // 简单随机：50%概率掉落
            std::uniform_int_distribution<> dis(0, 1);
            if (dis(gen) == 1) {
                out_items.push_back(item_id);
                out_item_counts.push_back(1);
            }
        }

        out_exp += definition.exp_reward;
        out_gold += definition.gold_reward;
    }

    // 更新通关次数
    uint64_t key = MakeProgressKey(character_id, dungeon_id, difficulty);
    auto progress_it = progress_.find(key);
    if (progress_it != progress_.end()) {
        progress_it->second.clear_count += count;
        progress_it->second.today_clear_count += count;
        progress_it->second.weekly_clear_count += count;
        SaveDungeonProgress(character_id, progress_it->second);
    }

    spdlog::info("[DungeonManager] Character {} swept dungeon {} {} times",
        character_id, dungeon_id, count);

    return true;
}

// ========== 副本奖励 ==========

bool DungeonManager::DistributeDungeonRewards(
    uint64_t character_id,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty,
    bool first_clear,
    std::vector<uint32_t>& out_items,
    std::vector<uint32_t>& out_item_counts,
    uint32_t& out_exp,
    uint32_t& out_gold) {

    std::lock_guard<std::mutex> lock(mutex_);

    // 检查副本定义
    auto def_it = definitions_.find(dungeon_id);
    if (def_it == definitions_.end()) {
        return false;
    }

    const DungeonDefinition& definition = def_it->second;

    out_items.clear();
    out_item_counts.clear();
    out_exp = definition.exp_reward;
    out_gold = definition.gold_reward;

    // 首通额外奖励
    if (first_clear) {
        out_exp *= 2;  // 首通双倍经验
        out_gold *= 2; // 首通双倍金币

        for (uint32_t item_id : definition.first_clear_rewards) {
            out_items.push_back(item_id);
            out_item_counts.push_back(1);
        }
    }

    // 普通奖励
    for (uint32_t item_id : definition.normal_rewards) {
        out_items.push_back(item_id);
        out_item_counts.push_back(1);
    }

    spdlog::info("[DungeonManager] Distributed rewards for character {}, dungeon {} (first_clear: {})",
        character_id, dungeon_id, first_clear);

    return true;
}

// ========== 定时器 ==========

void DungeonManager::Update(uint32_t delta_time) {
    std::lock_guard<std::mutex> lock(mutex_);

    tick_counter_ += delta_time;

    // 每秒更新一次
    if (tick_counter_ >= DUNGEON_TICK_INTERVAL) {
        tick_counter_ = 0;

        // 清理过期房间
        CleanupExpiredRooms();

        // 检查每日重置
        daily_reset_counter_++;
        if (daily_reset_counter_ >= 86400) {  // 24小时
            daily_reset_counter_ = 0;
            DailyReset();
        }

        // 检查每周重置
        weekly_reset_counter_++;
        if (weekly_reset_counter_ >= 604800) {  // 7天
            weekly_reset_counter_ = 0;
            WeeklyReset();
        }
    }
}

void DungeonManager::DailyReset() {
    spdlog::info("[DungeonManager] Performing daily reset");

    // 重置所有玩家的每日通关次数
    for (auto& pair : progress_) {
        pair.second.today_clear_count = 0;
        pair.second.today_first_clear_difficulty = 0;
    }

    // TODO: 保存到数据库
}

void DungeonManager::WeeklyReset() {
    spdlog::info("[DungeonManager] Performing weekly reset");

    // 重置所有玩家的每周通关次数
    for (auto& pair : progress_) {
        pair.second.weekly_clear_count = 0;
    }

    // TODO: 保存到数据库
}

// ========== 数据库操作 ==========

bool DungeonManager::LoadDungeonDefinitionsFromDB() {
    try {
        auto conn = Core::Database::ConnectionPool::Instance().GetConnection();
        conn->BeginTransaction();

        auto result = conn->Execute("SELECT * FROM dungeon_definitions");

        if (result && result->HasData()) {
            for (int row = 0; row < result->RowCount(); ++row) {
                DungeonDefinition definition;
                definition.dungeon_id = result->Get<uint32_t>(row, "dungeon_id").value_or(0);
                definition.name = result->GetValue(row, "name");
                definition.description = result->GetValue(row, "description");
                definition.type = static_cast<murim::DungeonType>(result->Get<int>(row, "type").value_or(0));
                definition.min_level = result->Get<uint32_t>(row, "min_level").value_or(1);
                definition.max_level = result->Get<uint32_t>(row, "max_level").value_or(0);
                definition.min_players = result->Get<uint32_t>(row, "min_players").value_or(1);
                definition.max_players = result->Get<uint32_t>(row, "max_players").value_or(1);
                definition.recommend_level = result->Get<uint32_t>(row, "recommend_level").value_or(1);
                definition.recommend_item_level = result->Get<uint32_t>(row, "recommend_item_level").value_or(0);
                definition.exp_reward = result->Get<uint32_t>(row, "exp_reward").value_or(0);
                definition.gold_reward = result->Get<uint32_t>(row, "gold_reward").value_or(0);
                definition.can_sweep = result->Get<uint32_t>(row, "can_sweep").value_or(0) != 0;
                definition.sweep_unlock_clear_count = result->Get<uint32_t>(row, "sweep_unlock_clear_count").value_or(0);
                definition.daily_reset_time = result->Get<uint32_t>(row, "daily_reset_time").value_or(0);
                definition.weekly_reset_time = result->Get<uint32_t>(row, "weekly_reset_time").value_or(0);

                definitions_[definition.dungeon_id] = definition;
            }
        }

        conn->Commit();
        spdlog::info("[DungeonManager] Loaded {} dungeon definitions from database",
            definitions_.size());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[DungeonManager] Failed to load dungeon definitions: {}", e.what());
        return false;
    }
}

bool DungeonManager::SaveDungeonDefinitionToDB(const DungeonDefinition& definition) {
    try {
        auto conn = Core::Database::ConnectionPool::Instance().GetConnection();
        conn->BeginTransaction();

        auto result = conn->ExecuteParams(
            "INSERT INTO dungeon_definitions "
            "(dungeon_id, name, description, type, min_level, max_level, min_players, max_players, "
            "recommend_level, recommend_item_level, exp_reward, gold_reward, can_sweep, "
            "sweep_unlock_clear_count, daily_reset_time, weekly_reset_time) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16) "
            "ON CONFLICT (dungeon_id) DO UPDATE SET "
            "name = $2, description = $3, type = $4, min_level = $5, max_level = $6, "
            "min_players = $7, max_players = $8, recommend_level = $9, recommend_item_level = $10, "
            "exp_reward = $11, gold_reward = $12, can_sweep = $13, sweep_unlock_clear_count = $14, "
            "daily_reset_time = $15, weekly_reset_time = $16",
            {
                std::to_string(definition.dungeon_id),
                definition.name,
                definition.description,
                std::to_string(static_cast<int>(definition.type)),
                std::to_string(definition.min_level),
                std::to_string(definition.max_level),
                std::to_string(definition.min_players),
                std::to_string(definition.max_players),
                std::to_string(definition.recommend_level),
                std::to_string(definition.recommend_item_level),
                std::to_string(definition.exp_reward),
                std::to_string(definition.gold_reward),
                std::to_string(definition.can_sweep ? 1 : 0),
                std::to_string(definition.sweep_unlock_clear_count),
                std::to_string(definition.daily_reset_time),
                std::to_string(definition.weekly_reset_time)
            }
        );

        conn->Commit();
        return true;

    } catch (const std::exception& e) {
        spdlog::error("[DungeonManager] Failed to save dungeon definition: {}", e.what());
        return false;
    }
}

// ========== 辅助方法 ==========

uint64_t DungeonManager::MakeProgressKey(
    uint64_t character_id,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty) const {

    // 复合键：character_id (高48位) | dungeon_id (中12位) | difficulty (低4位)
    return (character_id << 16) | (static_cast<uint64_t>(dungeon_id) & 0xFFF) << 4 |
           (static_cast<uint64_t>(difficulty) & 0xF);
}

uint64_t DungeonManager::GenerateRoomID() {
    return next_room_id_++;
}

void DungeonManager::CleanupExpiredRooms() {
    uint32_t current_time = static_cast<uint32_t>(std::time(nullptr));

    auto it = rooms_.begin();
    while (it != rooms_.end()) {
        const DungeonRoom& room = it->second;

        if (room.started) {
            // 已开始的房间由CompleteDungeon/FailDungeon处理
            ++it;
            continue;
        }

        // 检查是否过期
        if (current_time - room.created_time >= room.expire_time) {
            spdlog::info("[DungeonManager] Cleaning up expired room {}", room.room_id);
            it = rooms_.erase(it);
        } else {
            ++it;
        }
    }
}

void DungeonManager::CleanupStartedRoom(uint64_t room_id) {
    auto it = rooms_.find(room_id);
    if (it != rooms_.end()) {
        rooms_.erase(it);
        spdlog::info("[DungeonManager] Cleaned up started room {}", room_id);
    }
}

} // namespace Game
} // namespace Murim
