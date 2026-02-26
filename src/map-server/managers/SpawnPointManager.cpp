#include "SpawnPointManager.hpp"
#include "MapServerManager.hpp"
#include "core/database/MonsterDAO.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>
#include <random>

namespace Murim {
namespace MapServer {

// ==================== SpawnPoint 实现 ====================

SpawnPoint::SpawnPoint(uint64_t spawn_id, const SpawnPointConfig& config)
    : spawn_id_(spawn_id)
    , config_(config)
    , alive_count_(0)
    , last_respawn_time_(0)
{
}

SpawnPoint::~SpawnPoint() {
    Clear();
}

void SpawnPoint::Initialize() {
    spdlog::debug("[SpawnPoint] Initializing spawn point {}", spawn_id_);

    // 生成初始怪物
    for (uint32_t i = 0; i < config_.initial_spawn; ++i) {
        if (alive_count_ < config_.max_count) {
            SpawnMonster();
        }
    }

    spdlog::debug("[SpawnPoint] Spawn point {} initialized with {} monsters",
                  spawn_id_, alive_count_);
}

void SpawnPoint::Clear() {
    monsters_.clear();
    alive_count_ = 0;
}

void SpawnPoint::Update(uint32_t delta_time) {
    // 检查是否需要复活怪物
    if (alive_count_ < config_.max_count) {
        // 计算距离上次复活的时间
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();

        if (elapsed - last_respawn_time_ >= config_.respawn_delay) {
            // 复活一个怪物
            SpawnMonster();
            last_respawn_time_ = elapsed;
        }
    }

    // 更新所有怪物
    for (auto& monster : monsters_) {
        if (monster && monster->IsAlive()) {
            monster->Update(delta_time);
        }
    }
}

void SpawnPoint::SpawnMonster() {
    // 从MapServerManager获取MonsterTemplate
    auto& map_server_manager = MapServerManager::Instance();
    const Game::MonsterTemplate* tmpl = map_server_manager.GetMonsterTemplate(config_.monster_id);

    if (!tmpl) {
        spdlog::error("[SpawnPoint] Monster template {} not found!", config_.monster_id);
        return;
    }

    // 生成唯一实体ID
    static uint64_t entity_counter = 100000;
    uint64_t entity_id = ++entity_counter;

    // 创建怪物实例
    auto monster = std::make_unique<Monster>(entity_id, config_.monster_id, *tmpl, config_.position);

    // 设置刷怪点ID
    monster->SetSpawnPointID(spawn_id_);
    monster->SetMapID(config_.map_id);

    // 添加到列表
    monsters_.push_back(std::move(monster));
    alive_count_++;

    spdlog::trace("[SpawnPoint] Spawned monster {} (type:{}) at spawn point {} (alive: {}/{})",
                  entity_id, config_.monster_id, spawn_id_, alive_count_, config_.max_count);
}

void SpawnPoint::OnMonsterDeath(uint64_t entity_id) {
    // 从列表中移除死亡的怪物
    auto it = std::remove_if(monsters_.begin(), monsters_.end(),
        [entity_id](const std::unique_ptr<Monster>& m) {
            return m && m->GetEntityID() == entity_id && m->IsDead();
        });

    if (it != monsters_.end()) {
        monsters_.erase(it, monsters_.end());
        alive_count_--;

        spdlog::debug("[SpawnPoint] Monster {} died at spawn point {} (alive: {}/{})",
                      entity_id, spawn_id_, alive_count_, config_.max_count);

        // 安排复活
        ScheduleRespawn();
    }
}

void SpawnPoint::ScheduleRespawn() {
    last_respawn_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}


// ==================== SpawnPointManager 实现 ====================

SpawnPointManager::SpawnPointManager()
    : map_id_(1)
    , initialized_(false)
{
}

SpawnPointManager::~SpawnPointManager() {
    Clear();
}

bool SpawnPointManager::Initialize(uint16_t map_id, const BoundingBox& bounds) {
    if (initialized_) {
        spdlog::warn("[SpawnPointManager] Already initialized");
        return true;
    }

    map_id_ = map_id;
    initialized_ = true;

    spdlog::info("[SpawnPointManager] Initialized for map {}", map_id_);

    return true;
}

void SpawnPointManager::Clear() {
    spawn_points_.clear();
    initialized_ = false;
}

bool SpawnPointManager::LoadSpawnPoints() {
    using namespace Murim::Core::Database;

    spdlog::info("[SpawnPointManager] Loading spawn points for map {} from database...", map_id_);

    try {
        // 从数据库加载刷怪点配置
        auto spawn_point_data = MonsterDAO::LoadSpawnPoints(map_id_);

        if (spawn_point_data.empty()) {
            spdlog::warn("[SpawnPointManager] No spawn points loaded from database, using test data");

            // Fallback: 添加测试刷怪点
            SpawnPointConfig config;
            config.spawn_id = 1;
            config.monster_id = 1001;  // 测试怪物ID
            config.position = Vector3(5000.0f, 0.0f, 5000.0f);
            config.map_id = map_id_;
            config.max_count = 5;
            config.respawn_delay = 30000;  // 30秒
            config.initial_spawn = 3;
            config.group_id = 0;

            AddSpawnPoint(config);
        } else {
            // 将数据库加载的刷怪点转换为配置
            for (const auto& sp_data : spawn_point_data) {
                SpawnPointConfig config;
                config.spawn_id = sp_data.spawn_id;
                config.monster_id = sp_data.monster_id;
                config.position = Vector3(sp_data.x, sp_data.y, sp_data.z);
                config.map_id = sp_data.map_id;
                config.max_count = sp_data.max_count;
                config.respawn_delay = sp_data.respawn_time;
                config.initial_spawn = 1;  // 默认初始生成1个
                config.group_id = 0;

                AddSpawnPoint(config);
            }
        }

        spdlog::info("[SpawnPointManager] Loaded {} spawn points", spawn_points_.size());

        // 初始化所有刷怪点
        for (auto& [spawn_id, spawn_point] : spawn_points_) {
            spawn_point->Initialize();
        }

        return true;

    } catch (const std::exception& e) {
        spdlog::error("[SpawnPointManager] Failed to load spawn points: {}", e.what());
        return false;
    }
}

void SpawnPointManager::AddSpawnPoint(const SpawnPointConfig& config) {
    auto spawn_point = std::make_unique<SpawnPoint>(config.spawn_id, config);
    spawn_points_[config.spawn_id] = std::move(spawn_point);

    spdlog::debug("[SpawnPointManager] Added spawn point {} (monster_id={}, max_count={})",
                  config.spawn_id, config.monster_id, config.max_count);
}

void SpawnPointManager::RemoveSpawnPoint(uint64_t spawn_id) {
    auto it = spawn_points_.find(spawn_id);
    if (it != spawn_points_.end()) {
        it->second->Clear();
        spawn_points_.erase(it);

        spdlog::debug("[SpawnPointManager] Removed spawn point {}", spawn_id);
    }
}

void SpawnPointManager::Update(uint32_t delta_time) {
    if (!initialized_) {
        return;
    }

    // 更新所有刷怪点
    for (auto& [spawn_id, spawn_point] : spawn_points_) {
        spawn_point->Update(delta_time);
    }
}

SpawnPoint* SpawnPointManager::GetSpawnPoint(uint64_t spawn_id) {
    auto it = spawn_points_.find(spawn_id);
    if (it != spawn_points_.end()) {
        return it->second.get();
    }
    return nullptr;
}

uint32_t SpawnPointManager::GetTotalAliveMonsters() const {
    uint32_t total = 0;
    for (const auto& [spawn_id, spawn_point] : spawn_points_) {
        total += spawn_point->GetAliveCount();
    }
    return total;
}

} // namespace MapServer
} // namespace Murim
