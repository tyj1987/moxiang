// MurimServer - Game Object Manager
// 游戏对象管理器 - 管理所有游戏对象的创建、销毁、查找
// 对应legacy: GameObjectManager及相关管理系统

#pragma once

#include "GameObject.hpp"
#include <cstdint>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <functional>

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class Player;
class Monster;
class NPC;

// ========== Object Create Info ==========
// 对象创建信息
struct ObjectCreateInfo {
    ObjectType object_type;
    std::string object_name;
    float position_x, position_y, position_z;
    float rotation;
    uint32_t template_id;        // 模板ID
    uint64_t creator_id;         // 创建者ID

    ObjectCreateInfo()
        : object_type(ObjectType::Unknown),
          object_name(""),
          position_x(0.0f),
          position_y(0.0f),
          position_z(0.0f),
          rotation(0.0f),
          template_id(0),
          creator_id(0) {}
};

// ========== Object Filter ==========
// 对象过滤器
struct ObjectFilter {
    ObjectType type;              // 对象类型(Unknown表示所有类型)
    float range;                  // 范围(0表示无限)
    float x, y, z;                // 中心点
    bool only_alive;              // 只包括存活对象
    bool only_visible;            // 只包括可见对象
    bool only_active;             // 只包括活跃对象

    ObjectFilter()
        : type(ObjectType::Unknown),
          range(0.0f),
          x(0.0f), y(0.0f), z(0.0f),
          only_alive(false),
          only_visible(false),
          only_active(false) {}
};

// ========== Object Iterator ==========
// 对象迭代器接口
class IObjectIterator {
public:
    virtual ~IObjectIterator() = default;

    virtual bool HasNext() = 0;
    virtual GameObject* Next() = 0;
    virtual void Reset() = 0;
    virtual size_t GetCount() const = 0;
};

// ========== Game Object Manager ==========
// 游戏对象管理器
class GameObjectManager {
private:
    // 单例
    static GameObjectManager* instance_;

    // 所有对象映射 (object_id -> GameObject*)
    std::map<uint64_t, GameObject*> objects_;

    // 按类型分类的对象
    std::map<ObjectType, std::set<uint64_t>> objects_by_type_;

    // 待销毁对象列表 (延迟销毁)
    std::set<uint64_t> pending_destroy_objects_;

    // 对象创建计数器
    uint64_t next_object_id_;

    // 统计信息
    size_t total_created_count_;
    size_t total_destroyed_count_;

    // ========== 内部辅助函数 ==========

    // 生成新的对象ID
    uint64_t GenerateObjectID();

    // 释放对象内存
    void DestroyObject(GameObject* obj);

    // 从类型索引中移除
    void RemoveFromTypeIndex(GameObject* obj);

    // 添加到类型索引
    void AddToTypeIndex(GameObject* obj);

public:
    GameObjectManager();
    ~GameObjectManager();

    // ========== 单例访问 ==========
    static GameObjectManager& GetInstance();
    static void DestroyInstance();

    // ========== 初始化与清理 ==========
    void Init();
    void Release();

    // ========== 对象创建 ==========

    // 创建对象(使用创建信息)
    GameObject* CreateObject(const ObjectCreateInfo& info);

    // 创建对象(简化版本)
    GameObject* CreateObject(
        ObjectType type,
        const std::string& name,
        float x, float y, float z
    );

    // 创建玩家
    Player* CreatePlayer(
        uint64_t player_id,
        const std::string& player_name,
        float x, float y, float z
    );

    // 创建怪物
    Monster* CreateMonster(
        uint32_t template_id,
        float x, float y, float z
    );

    // 创建NPC
    NPC* CreateNPC(
        uint32_t template_id,
        float x, float y, float z
    );

    // ========== 对象销毁 ==========

    // 销毁对象
    bool DestroyObject(uint64_t object_id);

    // 延迟销毁对象(下一帧销毁)
    void DestroyObjectLater(uint64_t object_id);

    // 销毁所有对象
    void DestroyAllObjects();

    // 销毁指定类型的所有对象
    size_t DestroyObjectsByType(ObjectType type);

    // ========== 对象查询 ==========

    // 通过ID获取对象
    GameObject* GetObject(uint64_t object_id);

    // 通过ID获取对象(模板版本)
    template<typename T>
    T* GetObjectAs(uint64_t object_id) {
        GameObject* obj = GetObject(object_id);
        return dynamic_cast<T*>(obj);
    }

    // ========== 范围查询 ==========

    // 获取指定范围内的所有对象
    std::vector<GameObject*> GetObjectsInRange(
        float x, float y, float z,
        float range,
        ObjectType type = ObjectType::Unknown
    );

    // 获取指定范围内的所有对象(使用过滤器)
    std::vector<GameObject*> GetObjectsInRange(
        const ObjectFilter& filter
    );

    // 获取指定范围内的最近对象
    GameObject* GetNearestObject(
        float x, float y, float z,
        float max_range,
        ObjectType type = ObjectType::Unknown
    );

    // 获取指定范围内的最远对象
    GameObject* GetFarthestObject(
        float x, float y, float z,
        float max_range,
        ObjectType type = ObjectType::Unknown
    );

    // 获取指定范围内的随机对象
    GameObject* GetRandomObject(
        float x, float y, float z,
        float max_range,
        ObjectType type = ObjectType::Unknown
    );

    // ========== 类型查询 ==========

    // 获取指定类型的所有对象
    std::vector<GameObject*> GetObjectsByType(ObjectType type);

    // 获取指定类型的所有对象(在范围内)
    std::vector<GameObject*> GetObjectsByType(
        ObjectType type,
        float x, float y, float z,
        float range
    );

    // 获取所有玩家
    std::vector<Player*> GetAllPlayers();

    // 获取所有怪物
    std::vector<Monster*> GetAllMonsters();

    // 获取所有NPC
    std::vector<NPC*> GetAllNPCs();

    // ========== 条件查询 ==========

    // 使用谓词查找对象
    GameObject* FindObject(std::function<bool(GameObject*)> predicate);

    // 使用谓词查找多个对象
    std::vector<GameObject*> FindObjects(std::function<bool(GameObject*)> predicate);

    // ========== 对象遍历 ==========

    // 创建对象迭代器
    IObjectIterator* CreateIterator(const ObjectFilter& filter = ObjectFilter());

    // 遍历所有对象
    void ForEachObject(std::function<void(GameObject*)> visitor);

    // 遍历指定类型的对象
    void ForEachObject(ObjectType type, std::function<void(GameObject*)> visitor);

    // ========== 对象更新 ==========

    // 更新所有对象
    void UpdateAllObjects(uint32_t delta_time_ms);

    // 更新指定类型的对象
    void UpdateObjectsByType(ObjectType type, uint32_t delta_time_ms);

    // ========== 统计信息 ==========

    // 获取对象数量
    size_t GetObjectCount() const;
    size_t GetObjectCount(ObjectType type) const;

    // 获取统计信息
    void GetStatistics(
        size_t* total_count,
        size_t* player_count,
        size_t* monster_count,
        size_t* npc_count,
        size_t* created_count,
        size_t* destroyed_count
    ) const;

    // ========== 对象验证 ==========

    // 检查对象是否存在
    bool HasObject(uint64_t object_id) const;

    // 检查对象是否有效(存在、活跃、存活)
    bool IsObjectValid(uint64_t object_id) const;

    // ========== 调试与日志 ==========

    // 打印所有对象信息(调试用)
    void PrintAllObjects() const;

    // 打印指定类型的对象
    void PrintObjectsByType(ObjectType type) const;

    // 打印统计信息
    void PrintStatistics() const;
};

// ========== Object Iterator Implementation ==========
// 对象迭代器实现
class FilteredObjectIterator : public IObjectIterator {
private:
    const std::vector<GameObject*>& objects_;
    size_t current_index_;

public:
    FilteredObjectIterator(const std::vector<GameObject*>& objects);
    virtual ~FilteredObjectIterator() = default;

    bool HasNext() override;
    GameObject* Next() override;
    void Reset() override;
    size_t GetCount() const override;
};

// ========== Helper Functions ==========

// 创建对象ID(特定类型)
uint64_t MakeObjectID(ObjectType type, uint32_t index);

// 从对象ID提取类型
ObjectType GetObjectTypeFromID(uint64_t object_id);

// 从对象ID提取索引
uint32_t GetObjectIndexFromID(uint64_t object_id);

} // namespace Game
} // namespace Murim
