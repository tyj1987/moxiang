#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// 前向声明
class SkillInfo;
class GameObject;

/**
 * @brief 简化的配置加载器
 * 用于快速加载技能、怪物、物品等配置数据
 */
class SimpleConfigLoader {
public:
    /**
     * @brief 加载技能配置
     * @return 加载的技能数量
     */
    static int LoadSkillConfigs();

    /**
     * @brief 加载怪物配置
     * @return 加载的怪物数量
     */
    static int LoadMonsterConfigs();

    /**
     * @brief 加载物品配置
     * @return 加载的物品数量
     */
    static int LoadItemConfigs();

    /**
     * @brief 获取技能信息
     * @param skill_id 技能ID
     * @return 技能信息指针，不存在返回nullptr
     */
    static SkillInfo* GetSkillInfo(uint32_t skill_id);

private:
    static std::unordered_map<uint32_t SkillInfo*> skill_cache_;
    static bool initialized_;
};
