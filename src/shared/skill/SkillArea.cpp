// MurimServer - SkillArea Implementation
// 技能范围系统实现

#include "SkillArea.hpp"
#include "../game/GameObject.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace Murim {
namespace Game {

// ========== SkillAreaData ==========

SkillAreaData::SkillAreaData()
    : area_radius_(0), area_size_(0), max_area_radius_(0) {
}

SkillAreaData::~SkillAreaData() {
    ClearAreaData();
}

void SkillAreaData::InitAreaData(int radius) {
    area_radius_ = radius;
    area_size_ = 2 * radius + 1;

    // 分配tile数据数组
    size_t total_tiles = area_size_ * area_size_;
    area_tiles_.resize(total_tiles, 0);

    // 计算最大半径
    CalcMaxAreaRadius();

    spdlog::debug("SkillAreaData initialized: radius={}, size={}, total_tiles={}",
                 area_radius_, area_size_, total_tiles);
}

void SkillAreaData::ClearAreaData() {
    area_tiles_.clear();
    area_radius_ = 0;
    area_size_ = 0;
    max_area_radius_ = 0;
}

void SkillAreaData::CalcMaxAreaRadius() {
    // 计算实际包含tile的最大半径（从边缘向内搜索）
    max_area_radius_ = area_radius_;

    // 优化：从边缘开始向内搜索，找到最远的非零tile
    bool found_tiles = false;
    for (int r = area_radius_; r > 0; --r) {
        // 检查半径为r的圆环上是否有tile
        for (int angle = 0; angle < 360; angle += 10) {
            float rad = angle * 3.14159f / 180.0f;
            int x = static_cast<int>(r * std::cos(rad));
            int z = static_cast<int>(r * std::sin(rad));

            if (GetAreaAttrib(x, z) != 0) {
                max_area_radius_ = r;
                found_tiles = true;
                break;
            }
        }
        if (found_tiles) break;
    }

    spdlog::debug("CalcMaxAreaRadius: optimized radius {} -> {}", area_radius_, max_area_radius_);
}

void SkillAreaData::SetAreaAttrib(int index, uint8_t attr) {
    if (index >= 0 && index < static_cast<int>(area_tiles_.size())) {
        area_tiles_[index] = attr;
    }
}

uint8_t SkillAreaData::GetAreaAttrib(int index) const {
    if (index >= 0 && index < static_cast<int>(area_tiles_.size())) {
        return area_tiles_[index];
    }
    return 0;
}

uint8_t SkillAreaData::GetAreaAttrib(int x, int z) const {
    int index = GetAreaIndex(x, z);
    return GetAreaAttrib(index);
}

bool SkillAreaData::LoadAreaData(const std::string& file_path) {
    // 实现从二进制文件加载（legacy使用CMHFile读取二进制格式）
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open skill area file: {}", file_path);
        return false;
    }

    // 读取半径
    file.read(reinterpret_cast<char*>(&area_radius_), sizeof(int));
    if (file.fail() || area_radius_ <= 0) {
        spdlog::error("Invalid area radius in file: {}", file_path);
        return false;
    }

    // 初始化数据
    InitAreaData(area_radius_);

    // 读取tile数据
    size_t total_tiles = area_size_ * area_size_;
    file.read(reinterpret_cast<char*>(area_tiles_.data()), total_tiles * sizeof(uint8_t));

    if (file.fail()) {
        spdlog::error("Failed to read tile data from file: {}", file_path);
        ClearAreaData();
        return false;
    }

    file.close();

    spdlog::info("Loaded skill area data from {}: radius={}, tiles={}",
                file_path, area_radius_, total_tiles);

    return true;
}

void SkillAreaData::GetRotatedAreaData(SkillAreaData* rotated_area, DirectionIndex dir_idx) const {
    if (!rotated_area || area_radius_ == 0) {
        return;
    }

    // 初始化输出范围
    rotated_area->InitAreaData(area_radius_);

    int dir = static_cast<int>(dir_idx);
    int rotations = dir / 2;  // 每个方向45度，90度为一次旋转

    for (int z = -area_radius_; z <= area_radius_; ++z) {
        for (int x = -area_radius_; x <= area_radius_; ++x) {
            // 获取原始tile属性
            uint8_t attr = GetAreaAttrib(x, z);

            if (attr != 0) {
                // 计算旋转后的坐标
                int rx = x, rz = z;

                // 顺时针旋转
                for (int r = 0; r < rotations; ++r) {
                    int temp = rx;
                    rx = -rz;
                    rz = temp;
                }

                // 设置旋转后的属性
                rotated_area->SetAreaAttrib(rotated_area->GetAreaIndex(rx, rz), attr);
            }
        }
    }

    spdlog::debug("Rotated area data: dir={}, radius={}", dir, area_radius_);
}

// ========== SkillArea ==========

SkillArea::SkillArea()
    : center_pos_{0, 0, 0}, skill_area_data_(nullptr), area_id_(0) {
    spdlog::debug("SkillArea: Created default area (area_id=0)");
}

SkillArea::SkillArea(const SkillAreaData* area_data)
    : center_pos_{0, 0, 0}, skill_area_data_(area_data), area_id_(0) {
    spdlog::debug("SkillArea: Created area from data (area_id=0)");
}

SkillArea::~SkillArea() {
}

bool SkillArea::IsInArea(float x, float y, float z) const {
    if (!skill_area_data_) {
        return false;
    }

    // 转换为局部坐标
    float local_x = x - center_pos_.x;
    float local_z = z - center_pos_.z;

    // 获取tile索引
    int tile_idx = skill_area_data_->GetAreaIndex(local_x, local_z);

    // 检查是否在范围内
    uint8_t attr = skill_area_data_->GetAreaAttrib(tile_idx);
    return attr != 0;
}

bool SkillArea::IsInDamageTile(float x, float y, float z) const {
    if (!skill_area_data_) {
        return false;
    }

    float local_x = x - center_pos_.x;
    float local_z = z - center_pos_.z;

    int tile_idx = skill_area_data_->GetAreaIndex(local_x, local_z);
    uint8_t attr = skill_area_data_->GetAreaAttrib(tile_idx);

    return (attr & static_cast<uint8_t>(AreaTileAttribute::Damage)) != 0;
}

void SkillArea::AddTileAttrByAreaData(GameObject* object) {
    if (!object || !skill_area_data_) {
        return;
    }

    // 根据范围数据给对象添加tile属性
    // 实现遍历范围内的所有tile并设置属性

    spdlog::debug("SkillArea::AddTileAttrByAreaData: object={}, area_id={}",
                object->GetObjectId(), area_id_);

    // TODO [需其他系统]: 在GameObject中添加tile_attribute_成员和相关方法
    // 需要GameObject系统支持：SetTileAttribute(x, z, attrib), GetTileAttribute(x, z)
    // 当前版本只记录日志
    /*
    for (int z = -skill_area_data_->GetAreaRadius(); z <= skill_area_data_->GetAreaRadius(); ++z) {
        for (int x = -skill_area_data_->GetAreaRadius(); x <= skill_area_data_->GetAreaRadius(); ++x) {
            uint8_t attrib = skill_area_data_->GetAreaAttrib(x, z);
            if (attrib != 0) {
                // 设置对象的tile属性
                object->SetTileAttribute(x, z, attrib);
            }
        }
    }
    */
}

void SkillArea::RemoveTileAttrByAreaData(GameObject* object) {
    if (!object || !skill_area_data_) {
        return;
    }

    // 移除对象的tile属性
    // 实现遍历范围内的所有tile并清除属性

    spdlog::debug("SkillArea::RemoveTileAttrByAreaData: object={}, area_id={}",
                object->GetObjectId(), area_id_);

    // TODO [需其他系统]: 在GameObject中添加tile_attribute_成员和相关方法
    // 需要GameObject系统支持：ClearTileAttribute(x, z)
    // 当前版本只记录日志
    /*
    for (int z = -skill_area_data_->GetAreaRadius(); z <= skill_area_data_->GetAreaRadius(); ++z) {
        for (int x = -skill_area_data_->GetAreaRadius(); x <= skill_area_data_->GetAreaRadius(); ++x) {
            // 清除对象的tile属性
            object->ClearTileAttribute(x, z);
        }
    }
    */
}

// ========== SkillAreaManager ==========

SkillAreaManager::SkillAreaManager()
    : max_skill_area_(0), next_area_id_(1) {
}

SkillAreaManager::~SkillAreaManager() {
    Release();
}

bool SkillAreaManager::LoadSkillAreaList(const std::string& data_dir) {
    spdlog::info("SkillAreaManager: Loading skill area data from: {}", data_dir);

    // 首先尝试从配置文件加载
    std::string config_file = data_dir + "/skill_areas.json";
    std::ifstream config(config_file);

    if (config.is_open()) {
        spdlog::info("SkillAreaManager: Loading from config file: {}", config_file);
        // TODO [需配置系统]: 解析JSON配置文件
        // 需要JSON解析库（如nlohmann/json）或配置系统
        // JSON格式示例：
        // {
        //   "skill_areas": [
        //     {
        //       "area_id": 1,
        //       "type": "circle",
        //       "radius": 3,
        //       "tiles": [...]
        //     }
        //   ]
        // }
        // 当前使用硬编码示例数据
        config.close();
    } else {
        spdlog::warn("SkillAreaManager: Config file not found: {}, using hardcoded areas", config_file);
    }

    // 加载示例范围数据

    // 1. 圆形范围（半径3）
    {
        SkillAreaDataSet area_set;
        for (int dir = 0; dir < MAX_DIRECTION_INDEX; ++dir) {
            SkillAreaData data;
            data.InitAreaData(3);

            // 创建圆形tile
            int radius = 3;
            for (int z = -radius; z <= radius; ++z) {
                for (int x = -radius; x <= radius; ++x) {
                    float dist = std::sqrt(static_cast<float>(x * x + z * z));
                    if (dist <= radius) {
                        int idx = data.GetAreaIndex(x, z);
                        data.SetAreaAttrib(idx, static_cast<uint8_t>(AreaTileAttribute::Damage));
                    }
                }
            }

            // 旋转数据
            SkillAreaData rotated;
            data.GetRotatedAreaData(&rotated, static_cast<DirectionIndex>(dir));
            area_set.SetAreaData(static_cast<DirectionIndex>(dir), rotated);
        }

        skill_area_data_table_[1] = area_set;  // 范围ID 1
        spdlog::info("Loaded skill area 1: Circle radius 3");
    }

    // 2. 扇形范围（半径5，90度）
    {
        SkillAreaDataSet area_set;
        for (int dir = 0; dir < MAX_DIRECTION_INDEX; ++dir) {
            SkillAreaData data;
            data.InitAreaData(5);

            int radius = 5;
            for (int z = -radius; z <= radius; ++z) {
                for (int x = -radius; x <= radius; ++x) {
                    float dist = std::sqrt(static_cast<float>(x * x + z * z));
                    if (dist <= radius && dist > 0) {
                        // 计算角度，创建扇形
                        float angle = std::atan2(z, x) * 180.0f / 3.14159f;
                        if (std::abs(angle) <= 45.0f) {  // 90度扇形
                            int idx = data.GetAreaIndex(x, z);
                            data.SetAreaAttrib(idx, static_cast<uint8_t>(AreaTileAttribute::Damage));
                        }
                    }
                }
            }

            SkillAreaData rotated;
            data.GetRotatedAreaData(&rotated, static_cast<DirectionIndex>(dir));
            area_set.SetAreaData(static_cast<DirectionIndex>(dir), rotated);
        }

        skill_area_data_table_[2] = area_set;  // 范围ID 2
        spdlog::info("Loaded skill area 2: Fan radius 5, angle 90");
    }

    // 3. 矩形范围（5x3）
    {
        SkillAreaDataSet area_set;
        for (int dir = 0; dir < MAX_DIRECTION_INDEX; ++dir) {
            SkillAreaData data;
            data.InitAreaData(5);

            int width = 5;  // x方向
            int depth = 3;  // z方向

            for (int z = -depth; z <= depth; ++z) {
                for (int x = 0; x <= width; ++x) {  // 只在正x方向
                    int idx = data.GetAreaIndex(x, z);
                    data.SetAreaAttrib(idx, static_cast<uint8_t>(AreaTileAttribute::Damage));
                }
            }

            SkillAreaData rotated;
            data.GetRotatedAreaData(&rotated, static_cast<DirectionIndex>(dir));
            area_set.SetAreaData(static_cast<DirectionIndex>(dir), rotated);
        }

        skill_area_data_table_[3] = area_set;  // 范围ID 3
        spdlog::info("Loaded skill area 3: Rectangle 5x3");
    }

    max_skill_area_ = static_cast<int>(skill_area_data_table_.size());

    spdlog::info("SkillAreaManager loaded {} area types", max_skill_area_);

    return true;
}

void SkillAreaManager::Release() {
    skill_area_cache_.clear();
    skill_area_data_table_.clear();
    max_skill_area_ = 0;

    spdlog::info("SkillAreaManager released");
}

SkillArea* SkillAreaManager::GetSkillArea(uint32_t skill_area_idx, DirectionIndex dir_idx) {
    auto it = skill_area_data_table_.find(skill_area_idx);
    if (it == skill_area_data_table_.end()) {
        spdlog::warn("SkillAreaManager: Area index {} not found", skill_area_idx);
        return nullptr;
    }

    const SkillAreaData* area_data = it->second.GetAreaData(dir_idx);
    if (!area_data) {
        spdlog::warn("SkillAreaManager: Direction {} not found for area {}",
                    static_cast<int>(dir_idx), skill_area_idx);
        return nullptr;
    }

    // 创建新的SkillArea实例
    auto skill_area = std::make_unique<SkillArea>(area_data);
    SkillArea* ptr = skill_area.get();

    // 生成并设置area_id
    uint64_t area_id = GenerateAreaId();
    skill_area->area_id_ = area_id;  // 设置area_id成员

    // 缓存实例
    skill_area_cache_[area_id] = std::move(skill_area);

    spdlog::debug("SkillAreaManager: Created area {} for idx={}, dir={}",
                area_id, skill_area_idx, static_cast<int>(dir_idx));

    return ptr;
}

void SkillAreaManager::ReleaseSkillArea(SkillArea* skill_area) {
    if (!skill_area) {
        return;
    }

    // 从缓存中移除
    for (auto it = skill_area_cache_.begin(); it != skill_area_cache_.end(); ++it) {
        if (it->second.get() == skill_area) {
            spdlog::debug("SkillAreaManager: Released area {}", it->first);
            skill_area_cache_.erase(it);
            return;
        }
    }

    spdlog::warn("SkillAreaManager: Attempted to release unknown area");
}

// ========== Helper Functions ==========

int GetTileDistance(float x1, float z1, float x2, float z2) {
    float dx = x2 - x1;
    float dz = z2 - z1;
    float dist = std::sqrt(dx * dx + dz * dz);
    return static_cast<int>(dist / TILE_SIZE);
}

void WorldToLocal(float center_x, float center_z, float world_x, float world_z,
                 float* local_x, float* local_z) {
    if (local_x) *local_x = world_x - center_x;
    if (local_z) *local_z = world_z - center_z;
}

void LocalToWorld(float center_x, float center_z, float local_x, float local_z,
                 float* world_x, float* world_z) {
    if (world_x) *world_x = center_x + local_x;
    if (world_z) *world_z = center_z + local_z;
}

} // namespace Game
} // namespace Murim
