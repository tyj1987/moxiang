#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>

#include "nlohmann/json.hpp"

namespace Murim {
namespace Game {

/**
 * @brief 玩家点数数据表
 *
 * 对应 Legacy: m_PLAYERxMONSTER_POINT[level-1][levelgap+MONSTERLEVELRESTRICT_LOWSTARTNUM]
 * 数据来源: legacy-client/Resource/Server/PlayerxMonsterPoint.bin
 * JSON 文件: new-server/config/PlayerxMonsterPoint.json
 */
class PlayerxMonsterPointTable {
public:
    /**
     * @brief 从 JSON 文件加载数据
     *
     * @param json_file_path JSON 文件路径
     * @return true 加载成功，false 加载失败
     */
    bool LoadFromFile(const std::string& json_file_path);

    /**
     * @brief 获取玩家点数
     *
     * @param level 玩家等级 (1-120)
     * @param level_gap 等级差 (-6 到 +9)
     * @return 玩家点数，如果数据不存在返回 0
     */
    uint32_t GetPoint(uint16_t level, int level_gap) const;

    /**
     * @brief 检查数据是否已加载
     */
    bool IsLoaded() const { return loaded_; }

    /**
     * @brief 获取数据表信息
     */
    std::string GetInfo() const;

    /**
     * @brief 获取数据点总数
     */
    size_t GetTotalDataPoints() const;

private:
    bool loaded_ = false;

    // 常量定义（来自 Legacy CommonGameDefine.h）
    static constexpr int MAX_PLAYERLEVEL_NUM = 121;          // 最大等级
    static constexpr int MONSTERLEVELRESTRICT_LOWSTARTNUM = 6;
    static constexpr int MAX_MONSTERLEVELPOINTRESTRICT_NUM = 9;

    // 数据表: data[level][level_gap] = point
    // level: 1-120 (索引 0-119)
    // level_gap: -6 到 +9 (索引 0-15)
    std::unordered_map<uint16_t, std::unordered_map<int, uint32_t>> data_;
};

} // namespace Game
} // namespace Murim
