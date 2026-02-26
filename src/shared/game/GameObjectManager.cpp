// MurimServer - Game Object Manager Implementation
// 游戏对象管理器实现

#include "GameObjectManager.hpp"
#include "../utils/TimeUtils.hpp"
#include "core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

// 取消 Windows API 的 GetObject 宏定义，避免与我们的函数冲突
#ifdef GetObject
#undef GetObject
#endif

namespace Murim {
namespace Game {

// ========== Player/Monster/NPC 前向声明(临时) ==========
// 实际实现时应该在各自的头文件中
class Player : public GameObject {
public:
    Player() : GameObject() { SetObjectType(ObjectType::Player); }
    bool IsPlayer() const override { return true; }
};

class Monster : public GameObject {
public:
    Monster() : GameObject() { SetObjectType(ObjectType::Monster); }
    bool IsMonster() const override { return true; }
};

class NPC : public GameObject {
public:
    NPC() : GameObject() { SetObjectType(ObjectType::NPC); }
    bool IsNPC() const override { return true; }
};

// ========== 静态成员初始化 ==========

GameObjectManager* GameObjectManager::instance_ = nullptr;

// ========== GameObjectManager ==========

GameObjectManager::GameObjectManager()
    : next_object_id_(1),
      total_created_count_(0),
      total_destroyed_count_(0) {
    spdlog::info("GameObjectManager: Constructor called");
}

GameObjectManager::~GameObjectManager() {
    Release();
    spdlog::info("GameObjectManager: Destructor called");
}

GameObjectManager& GameObjectManager::GetInstance() {
    if (!instance_) {
        instance_ = new GameObjectManager();
        instance_->Init();
    }
    return *instance_;
}

void GameObjectManager::DestroyInstance() {
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
}

void GameObjectManager::Init() {
    spdlog::info("GameObjectManager: Initialized");
}

void GameObjectManager::Release() {
    // 销毁所有对象
    DestroyAllObjects();

    spdlog::info("GameObjectManager: Released - Created: {}, Destroyed: {}",
                total_created_count_, total_destroyed_count_);
}

// ========== 内部辅助函数 ==========

uint64_t GameObjectManager::GenerateObjectID() {
    uint64_t object_id = next_object_id_++;

    // 确保ID不重复
    while (objects_.find(object_id) != objects_.end()) {
        object_id = next_object_id_++;
    }

    return object_id;
}

void GameObjectManager::DestroyObject(GameObject* obj) {
    if (!obj) return;

    spdlog::debug("[GameObjectManager] Destroying object: id={}, type={}, name={}",
                obj->GetObjectId(),
                static_cast<int>(obj->GetObjectType()),
                obj->GetObjectName());

    delete obj;
    total_destroyed_count_++;
}

void GameObjectManager::RemoveFromTypeIndex(GameObject* obj) {
    if (!obj) return;

    ObjectType type = obj->GetObjectType();
    auto it = objects_by_type_.find(type);
    if (it != objects_by_type_.end()) {
        it->second.erase(obj->GetObjectId());

        // 如果该类型没有对象了,移除类型条目
        if (it->second.empty()) {
            objects_by_type_.erase(it);
        }
    }
}

void GameObjectManager::AddToTypeIndex(GameObject* obj) {
    if (!obj) return;

    ObjectType type = obj->GetObjectType();
    objects_by_type_[type].insert(obj->GetObjectId());
}

// ========== 对象创建 ==========

GameObject* GameObjectManager::CreateObject(const ObjectCreateInfo& info) {
    uint64_t object_id = GenerateObjectID();
    GameObject* obj = nullptr;

    // 根据类型创建对象
    switch (info.object_type) {
        case ObjectType::Player:
            obj = new Player();
            break;
        case ObjectType::Monster:
            obj = new Monster();
            break;
        case ObjectType::NPC:
            obj = new NPC();
            break;
        default:
            obj = new GameObject();
            break;
    }

    if (!obj) {
        spdlog::error("[GameObjectManager] Failed to create object: type={}",
                    static_cast<int>(info.object_type));
        return nullptr;
    }

    // 初始化对象
    if (!obj->Init(object_id, info.object_type, info.object_name,
                   info.position_x, info.position_y, info.position_z)) {
        DestroyObject(obj);
        return nullptr;
    }

    obj->SetRotation(info.rotation);

    // 添加到管理器
    objects_[object_id] = obj;
    AddToTypeIndex(obj);

    total_created_count_++;

    spdlog::info("[GameObjectManager] Object created: id={}, type={}, name={}, pos=({},{},{})",
                object_id,
                static_cast<int>(info.object_type),
                info.object_name,
                info.position_x,
                info.position_y,
                info.position_z);

    return obj;
}

GameObject* GameObjectManager::CreateObject(
    ObjectType type,
    const std::string& name,
    float x, float y, float z) {

    ObjectCreateInfo info;
    info.object_type = type;
    info.object_name = name;
    info.position_x = x;
    info.position_y = y;
    info.position_z = z;

    return CreateObject(info);
}

Player* GameObjectManager::CreatePlayer(
    uint64_t player_id,
    const std::string& player_name,
    float x, float y, float z) {

    // 使用指定的player_id作为object_id
    GameObject* existing_obj = GetObject(player_id);
    if (existing_obj) {
        spdlog::warn("[GameObjectManager] Player already exists: id={}", player_id);
        return nullptr;
    }

    ObjectCreateInfo info;
    info.object_type = ObjectType::Player;
    info.object_name = player_name;
    info.position_x = x;
    info.position_y = y;
    info.position_z = z;

    Player* player = new Player();
    if (!player->Init(player_id, ObjectType::Player, player_name, x, y, z)) {
        delete player;
        return nullptr;
    }

    objects_[player_id] = player;
    AddToTypeIndex(player);

    total_created_count_++;

    spdlog::info("[GameObjectManager] Player created: id={}, name={}, pos=({},{},{})",
                player_id, player_name, x, y, z);

    return player;
}

Monster* GameObjectManager::CreateMonster(
    uint32_t template_id,
    float x, float y, float z) {

    ObjectCreateInfo info;
    info.object_type = ObjectType::Monster;
    info.object_name = "Monster_" + std::to_string(template_id);
    info.position_x = x;
    info.position_y = y;
    info.position_z = z;
    info.template_id = template_id;

    GameObject* obj = CreateObject(info);
    return dynamic_cast<Monster*>(obj);
}

NPC* GameObjectManager::CreateNPC(
    uint32_t template_id,
    float x, float y, float z) {

    ObjectCreateInfo info;
    info.object_type = ObjectType::NPC;
    info.object_name = "NPC_" + std::to_string(template_id);
    info.position_x = x;
    info.position_y = y;
    info.position_z = z;
    info.template_id = template_id;

    GameObject* obj = CreateObject(info);
    return dynamic_cast<NPC*>(obj);
}

// ========== 对象销毁 ==========

bool GameObjectManager::DestroyObject(uint64_t object_id) {
    auto it = objects_.find(object_id);
    if (it == objects_.end()) {
        spdlog::warn("[GameObjectManager] Object not found for destruction: id={}", object_id);
        return false;
    }

    GameObject* obj = it->second;

    // 从类型索引中移除
    RemoveFromTypeIndex(obj);

    // 从主映射中移除
    objects_.erase(it);

    // 销毁对象
    DestroyObject(obj);

    spdlog::debug("[GameObjectManager] Object destroyed: id={}", object_id);

    return true;
}

void GameObjectManager::DestroyObjectLater(uint64_t object_id) {
    // 添加到待销毁列表
    pending_destroy_objects_.insert(object_id);

    spdlog::debug("[GameObjectManager] Object {} scheduled for delayed destruction", object_id);
}

void GameObjectManager::DestroyAllObjects() {
    spdlog::info("[GameObjectManager] Destroying all objects (count: {})", objects_.size());

    // 先从类型索引中移除
    objects_by_type_.clear();

    // 销毁所有对象
    for (auto& pair : objects_) {
        DestroyObject(pair.second);
    }

    objects_.clear();
}

size_t GameObjectManager::DestroyObjectsByType(ObjectType type) {
    size_t destroyed_count = 0;

    auto it = objects_.begin();
    while (it != objects_.end()) {
        if (it->second->GetObjectType() == type) {
            uint64_t object_id = it->first;
            ++it;  // 先递增迭代器
            DestroyObject(object_id);
            destroyed_count++;
        } else {
            ++it;
        }
    }

    spdlog::info("[GameObjectManager] Destroyed {} objects of type {}",
                destroyed_count, static_cast<int>(type));

    return destroyed_count;
}

// ========== 对象查询 ==========

GameObject* GameObjectManager::GetObject(uint64_t object_id) {
    auto it = objects_.find(object_id);
    if (it != objects_.end()) {
        return it->second;
    }
    return nullptr;
}

// ========== 范围查询 ==========

std::vector<GameObject*> GameObjectManager::GetObjectsInRange(
    float x, float y, float z,
    float range,
    ObjectType type) {

    std::vector<GameObject*> result;
    float range_squared = range * range;

    for (auto& pair : objects_) {
        GameObject* obj = pair.second;

        // 检查类型
        if (type != ObjectType::Unknown && obj->GetObjectType() != type) {
            continue;
        }

        // 检查范围
        float dx = obj->GetPositionX() - x;
        float dy = obj->GetPositionY() - y;
        float dz = obj->GetPositionZ() - z;
        float dist_squared = dx * dx + dy * dy + dz * dz;

        if (dist_squared <= range_squared) {
            result.push_back(obj);
        }
    }

    return result;
}

std::vector<GameObject*> GameObjectManager::GetObjectsInRange(const ObjectFilter& filter) {
    if (filter.range <= 0.0f) {
        // 无范围限制,返回所有符合类型的对象
        return GetObjectsByType(filter.type);
    }

    return GetObjectsInRange(filter.x, filter.y, filter.z, filter.range, filter.type);
}

GameObject* GameObjectManager::GetNearestObject(
    float x, float y, float z,
    float max_range,
    ObjectType type) {

    GameObject* nearest = nullptr;
    float nearest_dist_squared = max_range * max_range;

    for (auto& pair : objects_) {
        GameObject* obj = pair.second;

        // 检查类型
        if (type != ObjectType::Unknown && obj->GetObjectType() != type) {
            continue;
        }

        // 计算距离
        float dx = obj->GetPositionX() - x;
        float dy = obj->GetPositionY() - y;
        float dz = obj->GetPositionZ() - z;
        float dist_squared = dx * dx + dy * dy + dz * dz;

        if (dist_squared <= nearest_dist_squared) {
            nearest_dist_squared = dist_squared;
            nearest = obj;
        }
    }

    return nearest;
}

GameObject* GameObjectManager::GetFarthestObject(
    float x, float y, float z,
    float max_range,
    ObjectType type) {

    GameObject* farthest = nullptr;
    float farthest_dist_squared = 0.0f;
    float max_range_squared = max_range * max_range;

    for (auto& pair : objects_) {
        GameObject* obj = pair.second;

        // 检查类型
        if (type != ObjectType::Unknown && obj->GetObjectType() != type) {
            continue;
        }

        // 计算距离
        float dx = obj->GetPositionX() - x;
        float dy = obj->GetPositionY() - y;
        float dz = obj->GetPositionZ() - z;
        float dist_squared = dx * dx + dy * dy + dz * dz;

        if (dist_squared <= max_range_squared && dist_squared >= farthest_dist_squared) {
            farthest_dist_squared = dist_squared;
            farthest = obj;
        }
    }

    return farthest;
}

GameObject* GameObjectManager::GetRandomObject(
    float x, float y, float z,
    float max_range,
    ObjectType type) {

    std::vector<GameObject*> objects = GetObjectsInRange(x, y, z, max_range, type);

    if (objects.empty()) {
        return nullptr;
    }

    // 简单随机选择(实际应该使用更好的随机数生成器)
    size_t index = Utils::TimeUtils::GetCurrentTimeMs() % objects.size();
    return objects[index];
}

// ========== 类型查询 ==========

std::vector<GameObject*> GameObjectManager::GetObjectsByType(ObjectType type) {
    std::vector<GameObject*> result;

    if (type == ObjectType::Unknown) {
        // 返回所有对象
        result.reserve(objects_.size());
        for (auto& pair : objects_) {
            result.push_back(pair.second);
        }
    } else {
        // 返回指定类型的对象
        auto it = objects_by_type_.find(type);
        if (it != objects_by_type_.end()) {
            result.reserve(it->second.size());
            for (uint64_t object_id : it->second) {
                auto obj_it = objects_.find(object_id);
                if (obj_it != objects_.end()) {
                    result.push_back(obj_it->second);
                }
            }
        }
    }

    return result;
}

std::vector<GameObject*> GameObjectManager::GetObjectsByType(
    ObjectType type,
    float x, float y, float z,
    float range) {

    return GetObjectsInRange(x, y, z, range, type);
}

std::vector<Player*> GameObjectManager::GetAllPlayers() {
    std::vector<Player*> result;

    auto objects = GetObjectsByType(ObjectType::Player);
    result.reserve(objects.size());

    for (GameObject* obj : objects) {
        Player* player = dynamic_cast<Player*>(obj);
        if (player) {
            result.push_back(player);
        }
    }

    return result;
}

std::vector<Monster*> GameObjectManager::GetAllMonsters() {
    std::vector<Monster*> result;

    auto objects = GetObjectsByType(ObjectType::Monster);
    result.reserve(objects.size());

    for (GameObject* obj : objects) {
        Monster* monster = dynamic_cast<Monster*>(obj);
        if (monster) {
            result.push_back(monster);
        }
    }

    return result;
}

std::vector<NPC*> GameObjectManager::GetAllNPCs() {
    std::vector<NPC*> result;

    auto objects = GetObjectsByType(ObjectType::NPC);
    result.reserve(objects.size());

    for (GameObject* obj : objects) {
        NPC* npc = dynamic_cast<NPC*>(obj);
        if (npc) {
            result.push_back(npc);
        }
    }

    return result;
}

// ========== 条件查询 ==========

GameObject* GameObjectManager::FindObject(std::function<bool(GameObject*)> predicate) {
    for (auto& pair : objects_) {
        if (predicate(pair.second)) {
            return pair.second;
        }
    }
    return nullptr;
}

std::vector<GameObject*> GameObjectManager::FindObjects(std::function<bool(GameObject*)> predicate) {
    std::vector<GameObject*> result;

    for (auto& pair : objects_) {
        if (predicate(pair.second)) {
            result.push_back(pair.second);
        }
    }

    return result;
}

// ========== 对象遍历 ==========

IObjectIterator* GameObjectManager::CreateIterator(const ObjectFilter& filter) {
    std::vector<GameObject*> objects = GetObjectsInRange(filter);
    return new FilteredObjectIterator(objects);
}

void GameObjectManager::ForEachObject(std::function<void(GameObject*)> visitor) {
    for (auto& pair : objects_) {
        visitor(pair.second);
    }
}

void GameObjectManager::ForEachObject(ObjectType type, std::function<void(GameObject*)> visitor) {
    auto objects = GetObjectsByType(type);
    for (GameObject* obj : objects) {
        visitor(obj);
    }
}

// ========== 对象更新 ==========

void GameObjectManager::UpdateAllObjects(uint32_t delta_time_ms) {
    // 更新所有对象
    for (auto& pair : objects_) {
        if (pair.second->IsActive()) {
            pair.second->Update(delta_time_ms);
        }
    }

    // 处理待销毁列表
    if (!pending_destroy_objects_.empty()) {
        spdlog::debug("[GameObjectManager] Processing {} pending destructions",
                     pending_destroy_objects_.size());

        // 创建副本以避免在迭代时修改集合
        std::set<uint64_t> to_destroy = pending_destroy_objects_;
        pending_destroy_objects_.clear();

        for (uint64_t object_id : to_destroy) {
            DestroyObject(object_id);
        }
    }
}

void GameObjectManager::UpdateObjectsByType(ObjectType type, uint32_t delta_time_ms) {
    ForEachObject(type, [delta_time_ms](GameObject* obj) {
        if (obj->IsActive()) {
            obj->Update(delta_time_ms);
        }
    });
}

// ========== 统计信息 ==========

size_t GameObjectManager::GetObjectCount() const {
    return objects_.size();
}

size_t GameObjectManager::GetObjectCount(ObjectType type) const {
    auto it = objects_by_type_.find(type);
    if (it != objects_by_type_.end()) {
        return it->second.size();
    }
    return 0;
}

void GameObjectManager::GetStatistics(
    size_t* total_count,
    size_t* player_count,
    size_t* monster_count,
    size_t* npc_count,
    size_t* created_count,
    size_t* destroyed_count) const {

    if (total_count) *total_count = objects_.size();
    if (player_count) *player_count = GetObjectCount(ObjectType::Player);
    if (monster_count) *monster_count = GetObjectCount(ObjectType::Monster);
    if (npc_count) *npc_count = GetObjectCount(ObjectType::NPC);
    if (created_count) *created_count = total_created_count_;
    if (destroyed_count) *destroyed_count = total_destroyed_count_;
}

// ========== 对象验证 ==========

bool GameObjectManager::HasObject(uint64_t object_id) const {
    return objects_.find(object_id) != objects_.end();
}

bool GameObjectManager::IsObjectValid(uint64_t object_id) const {
    auto it = objects_.find(object_id);
    if (it == objects_.end()) {
        return false;
    }

    GameObject* obj = it->second;
    return obj->IsActive() && obj->IsAlive();
}

// ========== 调试与日志 ==========

void GameObjectManager::PrintAllObjects() const {
    spdlog::info("=== GameObjectManager: All Objects (count: {}) ===", objects_.size());

    for (const auto& pair : objects_) {
        const GameObject* obj = pair.second;
        spdlog::info("  [{}] id={} type={} name=({},{},{}) hp={}/{}",
                    static_cast<int>(obj->GetObjectType()),
                    obj->GetObjectId(),
                    obj->GetObjectName(),
                    obj->GetPositionX(),
                    obj->GetPositionY(),
                    obj->GetPositionZ(),
                    obj->GetCurrentHP(),
                    obj->GetMaxHP());
    }
}

void GameObjectManager::PrintObjectsByType(ObjectType type) const {
    auto objects = const_cast<GameObjectManager*>(this)->GetObjectsByType(type);

    spdlog::info("=== GameObjectManager: Objects of type {} (count: {}) ===",
                static_cast<int>(type), objects.size());

    for (const GameObject* obj : objects) {
        spdlog::info("  [{}] id={}, name={}, pos=({},{},{})",
                    static_cast<int>(obj->GetObjectType()),
                    obj->GetObjectId(),
                    obj->GetObjectName(),
                    obj->GetPositionX(),
                    obj->GetPositionY(),
                    obj->GetPositionZ());
    }
}

void GameObjectManager::PrintStatistics() const {
    size_t total, players, monsters, npcs, created, destroyed;
    GetStatistics(&total, &players, &monsters, &npcs, &created, &destroyed);

    spdlog::info("=== GameObjectManager Statistics ===");
    spdlog::info("Total Objects: {}", total);
    spdlog::info("Players: {}", players);
    spdlog::info("Monsters: {}", monsters);
    spdlog::info("NPCs: {}", npcs);
    spdlog::info("Total Created: {}", created);
    spdlog::info("Total Destroyed: {}", destroyed);
    spdlog::info("=====================================");
}

// ========== FilteredObjectIterator ==========

FilteredObjectIterator::FilteredObjectIterator(const std::vector<GameObject*>& objects)
    : objects_(objects), current_index_(0) {
}

bool FilteredObjectIterator::HasNext() {
    return current_index_ < objects_.size();
}

GameObject* FilteredObjectIterator::Next() {
    if (!HasNext()) {
        return nullptr;
    }
    return objects_[current_index_++];
}

void FilteredObjectIterator::Reset() {
    current_index_ = 0;
}

size_t FilteredObjectIterator::GetCount() const {
    return objects_.size();
}

// ========== Helper Functions ==========

uint64_t MakeObjectID(ObjectType type, uint32_t index) {
    return (static_cast<uint64_t>(type) << 32) | index;
}

ObjectType GetObjectTypeFromID(uint64_t object_id) {
    return static_cast<ObjectType>((object_id >> 32) & 0xFF);
}

uint32_t GetObjectIndexFromID(uint64_t object_id) {
    return static_cast<uint32_t>(object_id & 0xFFFFFFFF);
}

} // namespace Game
} // namespace Murim
