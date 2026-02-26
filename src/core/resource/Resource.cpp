// Murim MMORPG - Resource Manager Implementation

#include "Resource.hpp"
#include "core/spdlog_wrapper.hpp"
#include <fstream>
#include <cstring>

namespace Murim {
namespace Core {
namespace Resource {

ResourceManager& ResourceManager::Instance() {
    static ResourceManager instance;
    return instance;
}

ResourceManager::ResourceManager() {
    base_path_ = "Resource";
    server_path_ = "Resource/Server";
}

bool ResourceManager::Initialize(const std::string& base_path) {
    if (initialized_) {
        spdlog::warn("ResourceManager already initialized");
        return true;
    }

    spdlog::info("Initializing ResourceManager...");
    spdlog::info("Resource base path: {}", base_path);
    SetResourcePath(base_path);

    // 验证关键目录是否存在
    if (!FileExists("Monster_01.bin") && !FileExists("Server/Monster_01.bin")) {
        spdlog::warn("Monster resource files not found in resource path");
    }

    initialized_ = true;
    spdlog::info("ResourceManager initialized successfully");
    return true;
}

void ResourceManager::Shutdown() {
    if (!initialized_) {
        return;
    }

    spdlog::info("Shutting down ResourceManager...");
    monster_data_map_.clear();
    boss_data_map_.clear();
    item_change_map_.clear();
    initialized_ = false;
    spdlog::info("ResourceManager shut down");
}

void ResourceManager::SetResourcePath(const std::string& path) {
    base_path_ = path;
    server_path_ = path + "/Server";
    spdlog::debug("Resource path set to: {}", base_path_);
    spdlog::debug("Server resource path set to: {}", server_path_);
}

void ResourceManager::SetServerResourcePath(const std::string& path) {
    server_path_ = path;
    spdlog::debug("Server resource path set to: {}", server_path_);
}

std::string ResourceManager::GetFullPath(const std::string& filename) const {
    // 先尝试 Server 路径
    std::string full_path = server_path_ + "/" + filename;

    // 如果文件不存在，尝试基础路径
    std::ifstream test_file(full_path, std::ios::binary);
    if (!test_file.good()) {
        full_path = base_path_ + "/" + filename;
    }

    return full_path;
}

bool ResourceManager::FileExists(const std::string& filename) const {
    std::string full_path = GetFullPath(filename);
    std::ifstream file(full_path, std::ios::binary);
    return file.good();
}

bool ResourceManager::ReadBinaryFile(const std::string& filename, std::vector<uint8_t>& buffer) const {
    std::string full_path = GetFullPath(filename);

    std::ifstream file(full_path, std::ios::binary | std::ios::ate);
    if (!file.good()) {
        spdlog::error("Failed to open file: {}", full_path);
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer.resize(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        spdlog::error("Failed to read file: {}", full_path);
        return false;
    }

    spdlog::debug("Read {} bytes from {}", size, filename);
    return true;
}

// ========== 怪物数据加载 ==========

bool ResourceManager::LoadMonsterData(uint16_t map_id) {
    char filename[64];
    snprintf(filename, sizeof(filename), "Monster_%02d.bin", map_id);

    std::vector<uint8_t> buffer;
    if (!ReadBinaryFile(filename, buffer)) {
        spdlog::warn("Failed to load monster data for map {}", map_id);
        return false;
    }

    // 解析怪物数据
    // 注意：这里需要根据实际的 .bin 文件格式进行解析
    // 以下是示例解析逻辑，实际格式可能不同

    size_t monster_count = buffer.size() / sizeof(MonsterData);
    spdlog::info("Parsed {} monsters from {}", monster_count, filename);

    // TODO: 实际的二进制格式解析
    // 这需要参考 Legacy 代码中的数据结构

    return true;
}

size_t ResourceManager::LoadAllMonsterData() {
    spdlog::info("Loading all monster data from Resource files...");

    size_t loaded_count = 0;

    // 加载地图 1-20 的怪物数据
    for (uint16_t map_id = 1; map_id <= 20; ++map_id) {
        if (LoadMonsterData(map_id)) {
            loaded_count++;
        }
    }

    // 加载其他地图
    std::vector<uint16_t> additional_maps = {59, 60, 61, 65, 66, 67, 68, 69, 70, 72, 75};
    for (uint16_t map_id : additional_maps) {
        if (LoadMonsterData(map_id)) {
            loaded_count++;
        }
    }

    spdlog::info("Loaded monster data for {} maps", loaded_count);
    return loaded_count;
}

const MonsterData* ResourceManager::GetMonsterData(uint32_t monster_id) const {
    auto it = monster_data_map_.find(monster_id);
    if (it != monster_data_map_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<MonsterData> ResourceManager::GetMapMonsters(uint16_t map_id) const {
    std::vector<MonsterData> monsters;

    // TODO: 根据地图ID筛选怪物
    // 这需要先实现地图->怪物映射

    return monsters;
}

// ========== Boss 怪物加载 ==========

bool ResourceManager::LoadBossData() {
    spdlog::info("Loading Boss monster data...");

    std::vector<uint8_t> buffer;
    if (!ReadBinaryFile("BossMonsterfileList.bin", buffer)) {
        return false;
    }

    // TODO: 解析 Boss 怪物列表
    size_t boss_count = buffer.size() / sizeof(BossMonsterData);
    spdlog::info("Loaded {} boss monsters", boss_count);

    return true;
}

const BossMonsterData* ResourceManager::GetBossData(uint32_t boss_id) const {
    auto it = boss_data_map_.find(boss_id);
    if (it != boss_data_map_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<BossMonsterData> ResourceManager::GetAllBossData() const {
    std::vector<BossMonsterData> bosses;
    bosses.reserve(boss_data_map_.size());

    for (const auto& pair : boss_data_map_) {
        bosses.push_back(pair.second);
    }

    return bosses;
}

// ========== 物品兑换加载 ==========

bool ResourceManager::LoadItemChangeData() {
    spdlog::info("Loading item change data...");

    std::vector<uint8_t> buffer;
    if (!ReadBinaryFile("ItemChangeList.bin", buffer)) {
        return false;
    }

    // TODO: 解析物品兑换数据
    size_t change_count = buffer.size() / sizeof(ItemChangeData);
    spdlog::info("Loaded {} item changes", change_count);

    return true;
}

std::vector<ItemChangeData> ResourceManager::GetItemChanges(uint32_t item_id) const {
    auto it = item_change_map_.find(item_id);
    if (it != item_change_map_.end()) {
        return it->second;
    }
    return {};
}

} // namespace Resource
} // namespace Core
} // namespace Murim
