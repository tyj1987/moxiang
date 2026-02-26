#include "PlayerxMonsterPointTable.hpp"
#include <iostream>
#include <filesystem>

namespace Murim {
namespace Game {

bool PlayerxMonsterPointTable::LoadFromFile(const std::string& json_file_path) {
    // 检查文件是否存在
    if (!std::filesystem::exists(json_file_path)) {
        std::cerr << "[ERROR] File not found: " << json_file_path << std::endl;
        return false;
    }

    // 读取 JSON 文件
    std::ifstream json_file(json_file_path);
    if (!json_file.is_open()) {
        std::cerr << "[ERROR] Failed to open file: " << json_file_path << std::endl;
        return false;
    }

    try {
        // 解析 JSON
        nlohmann::json json_data;
        json_file >> json_data;

        // 读取元数据
        if (json_data.contains("meta")) {
            auto& meta = json_data["meta"];
            std::cout << "[INFO] Loading PlayerxMonsterPoint table:" << std::endl;
            std::cout << "  Source: " << meta.value("source", "") << std::endl;
            std::cout << "  Max Level: " << meta.value("max_level", 0) << std::endl;
            std::cout << "  Level Gap Range: "
                      << meta.value("level_gap_min", 0) << " to "
                      << meta.value("level_gap_max", 0) << std::endl;
        }

        // 读取数据
        if (!json_data.contains("data")) {
            std::cerr << "[ERROR] JSON file missing 'data' section" << std::endl;
            return false;
        }

        auto& data_section = json_data["data"];

        // 遍历所有等级
        for (auto& level_entry : data_section.items()) {
            uint16_t level = std::stoi(level_entry.key());  // 等级
            auto& level_gap_data = level_entry.value();     // 等级差数据

            // 遍历所有等级差
            for (auto& gap_entry : level_gap_data.items()) {
                int level_gap = std::stoi(gap_entry.key());  // 等级差
                uint32_t point = gap_entry.value();           // 点数

                // 存储到数据表
                data_[level][level_gap] = point;
            }
        }

        loaded_ = true;

        std::cout << "[OK] PlayerxMonsterPoint table loaded successfully" << std::endl;
        std::cout << "  Total levels: " << data_.size() << std::endl;
        std::cout << "  Total data points: " << GetTotalDataPoints() << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to parse JSON: " << e.what() << std::endl;
        return false;
    }
}

uint32_t PlayerxMonsterPointTable::GetPoint(uint16_t level, int level_gap) const {
    // 检查数据是否已加载
    if (!loaded_) {
        std::cerr << "[WARNING] Data table not loaded" << std::endl;
        return 0;
    }

    // 检查等级范围
    if (level < 1 || level >= MAX_PLAYERLEVEL_NUM) {
        return 0;  // 满级或无效等级
    }

    // 限制等级差范围
    if (level_gap < -MONSTERLEVELRESTRICT_LOWSTARTNUM) {
        level_gap = -MONSTERLEVELRESTRICT_LOWSTARTNUM;
    } else if (level_gap > MAX_MONSTERLEVELPOINTRESTRICT_NUM) {
        level_gap = MAX_MONSTERLEVELPOINTRESTRICT_NUM;
    }

    // 查找数据
    auto level_it = data_.find(level);
    if (level_it == data_.end()) {
        return 0;  // 等级数据不存在
    }

    auto gap_it = level_it->second.find(level_gap);
    if (gap_it == level_it->second.end()) {
        return 0;  // 等级差数据不存在
    }

    return gap_it->second;
}

std::string PlayerxMonsterPointTable::GetInfo() const {
    std::ostringstream oss;
    oss << "PlayerxMonsterPointTable:\n";
    oss << "  Loaded: " << (loaded_ ? "Yes" : "No") << "\n";
    oss << "  Levels: " << data_.size() << "\n";
    oss << "  Total data points: " << GetTotalDataPoints() << "\n";
    return oss.str();
}

size_t PlayerxMonsterPointTable::GetTotalDataPoints() const {
    size_t count = 0;
    for (const auto& level_entry : data_) {
        count += level_entry.second.size();
    }
    return count;
}

} // namespace Game
} // namespace Murim
