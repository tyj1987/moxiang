// MurimServer - SkillArea System
// 技能范围系统 - 支持各种形状的技能范围判定
// 对应legacy: SkillArea.h, SkillAreaData.h, SkillAreaManager.h

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <map>
#include <string>

namespace Murim {
namespace Game {

// ========== Constants ==========
constexpr float TILE_SIZE = 50.0f;
constexpr int MAX_DIRECTION_INDEX = 8;

// ========== Area Tile Attributes ==========
enum class AreaTileAttribute : uint8_t {
    None = 0,
    Damage = 0x01,       // 伤害区域
    Block = 0x02,        // 阻挡区域
    SafetyZone = 0x03,   // 安全区
    OtherEffect = 0x04   // 其他效果
};

// ========== Direction Index ==========
enum class DirectionIndex : uint8_t {
    North = 0,
    NorthEast = 1,
    East = 2,
    SouthEast = 3,
    South = 4,
    SouthWest = 5,
    West = 6,
    NorthWest = 7
};

// ========== SkillAreaData ==========
// 技能范围数据 - 存储范围的tile信息
class SkillAreaData {
private:
    int area_radius_;           // 范围半径（tile数）
    int area_size_;             // 范围大小（2*radius+1）
    int max_area_radius_;       // 最大范围半径（用于计算包围盒）

    std::vector<uint8_t> area_tiles_;  // 范围tile数据

    void CalcMaxAreaRadius();
    void SetAreaAttrib(int index, uint8_t attr);
    void InitAreaData(int radius);
    void ClearAreaData();

public:
    SkillAreaData();
    virtual ~SkillAreaData();

    // 从文件加载范围数据
    bool LoadAreaData(const std::string& file_path);

    // 获取旋转后的范围数据（用于不同朝向的技能）
    void GetRotatedAreaData(SkillAreaData* rotated_area, DirectionIndex dir_idx) const;

    // 获取指定位置的tile属性
    uint8_t GetAreaAttrib(int index) const;
    uint8_t GetAreaAttrib(int x, int z) const;

    // 坐标转索引
    // x, z: 相对中心坐标 [-radius, radius]
    inline int GetAreaIndex(int x, int z) const {
        int rx = x + area_radius_;
        int rz = z + area_radius_;
        return rz * area_size_ + rx;
    }

    // 浮点坐标转索引
    inline int GetAreaIndex(float local_x, float local_z) const {
        int nx = static_cast<int>(local_x / TILE_SIZE);
        if ((local_x - nx * TILE_SIZE) > (TILE_SIZE * 0.5f)) nx += 1;
        if ((local_x - nx * TILE_SIZE) < -(TILE_SIZE * 0.5f)) nx -= 1;

        int nz = static_cast<int>(local_z / TILE_SIZE);
        if ((local_z - nz * TILE_SIZE) > (TILE_SIZE * 0.5f)) nz += 1;
        if ((local_z - nz * TILE_SIZE) < -(TILE_SIZE * 0.5f)) nz -= 1;

        return GetAreaIndex(nx, nz);
    }

    int GetAreaRadius() const { return area_radius_; }
    int GetAreaSize() const { return area_size_; }

    const std::vector<uint8_t>& GetAreaTiles() const { return area_tiles_; }
};

// ========== SkillAreaDataSet ==========
// 技能范围数据集 - 包含所有方向的范围数据
struct SkillAreaDataSet {
private:
    std::vector<SkillAreaData> skill_area_data_array_;

public:
    SkillAreaDataSet() {
        skill_area_data_array_.resize(MAX_DIRECTION_INDEX);
    }

    void SetAreaData(DirectionIndex dir, const SkillAreaData& data) {
        skill_area_data_array_[static_cast<int>(dir)] = data;
    }

    SkillAreaData* GetAreaData(DirectionIndex dir) {
        int idx = static_cast<int>(dir);
        if (idx >= 0 && idx < static_cast<int>(skill_area_data_array_.size())) {
            return &skill_area_data_array_[idx];
        }
        return nullptr;
    }

    const SkillAreaData* GetAreaData(DirectionIndex dir) const {
        int idx = static_cast<int>(dir);
        if (idx >= 0 && idx < static_cast<int>(skill_area_data_array_.size())) {
            return &skill_area_data_array_[idx];
        }
        return nullptr;
    }
};

// ========== SkillArea ==========
// 技能范围实例 - 用于判断对象是否在技能范围内
class GameObject;  // Forward declaration

class SkillArea {
private:
    struct {
        float x, y, z;
    } center_pos_;

    const SkillAreaData* skill_area_data_;
    uint64_t area_id_;  // 唯一的区域ID，用于标识和日志记录

public:
    SkillArea();
    SkillArea(const SkillAreaData* area_data);
    virtual ~SkillArea();

    // 设置/获取中心位置
    void SetCenterPos(float x, float y, float z) {
        center_pos_.x = x;
        center_pos_.y = y;
        center_pos_.z = z;
    }

    void GetCenterPos(float* x, float* y, float* z) const {
        if (x) *x = center_pos_.x;
        if (y) *y = center_pos_.y;
        if (z) *z = center_pos_.z;
    }

    // 判断位置是否在范围内
    bool IsInArea(float x, float y, float z) const;

    // 判断位置是否在伤害tile中
    bool IsInDamageTile(float x, float y, float z) const;

    // 根据范围数据添加/移除tile属性
    void AddTileAttrByAreaData(GameObject* object);
    void RemoveTileAttrByAreaData(GameObject* object);

    const SkillAreaData* GetSkillAreaData() const { return skill_area_data_; }
};

// ========== SkillAreaManager ==========
// 技能范围管理器 - 加载和管理所有技能范围数据
class SkillAreaManager {
private:
    int max_skill_area_;
    std::map<uint32_t, SkillAreaDataSet> skill_area_data_table_;

    // 缓存已创建的SkillArea实例
    std::map<uint64_t, std::unique_ptr<SkillArea>> skill_area_cache_;
    uint64_t next_area_id_;

public:
    SkillAreaManager();
    virtual ~SkillAreaManager();

    // 单例模式（可选）
    // static SkillAreaManager& GetInstance();

    // 加载技能范围列表
    bool LoadSkillAreaList(const std::string& data_dir);

    // 释放所有资源
    void Release();

    // 获取技能范围实例
    // 返回的SkillArea由Manager管理，使用ReleaseSkillArea释放
    SkillArea* GetSkillArea(uint32_t skill_area_idx, DirectionIndex dir_idx);

    // 释放技能范围实例
    void ReleaseSkillArea(SkillArea* skill_area);

private:
    // 生成唯一的area ID
    uint64_t GenerateAreaId() {
        return next_area_id_++;
    }
};

// ========== Helper Functions ==========

// 计算两点之间的tile距离
int GetTileDistance(float x1, float z1, float x2, float z2);

// 将世界坐标转换为相对于中心的局部坐标
void WorldToLocal(float center_x, float center_z, float world_x, float world_z, float* local_x, float* local_z);

// 将局部坐标转换为世界坐标
void LocalToWorld(float center_x, float center_z, float local_x, float local_z, float* world_x, float* world_z);

} // namespace Game
} // namespace Murim
