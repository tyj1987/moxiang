#include "SimpleConfigLoader.hpp"
#include "../skill/SkillInfo.hpp"
#include "../skill/SkillDatabase.hpp"
#include "../game/GameObject.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <memory>

// 静态成员初始化
std::unordered_map<uint32_t SkillInfo*> SimpleConfigLoader::skill_cache_;
bool SimpleConfigLoader::initialized_ = false;

int SimpleConfigLoader::LoadSkillConfigs() {
    spdlog::info("[ConfigLoader] 开始加载技能配置...");

    int loaded_count = 0;

    // 示例1: 创建基础攻击技能
    {
        SkillInfo* basic_attack = new SkillInfo();
        basic_attack->SetSkillID(1);
        basic_attack->SetSkillName("普通攻击");
        basic_attack->SetSkillType(SkillType::Active);
        basic_attack->SetTargetType(SkillTargetType::Single);
        basic_attack->SetDamageType(SkillDamageType::Physical);
        basic_attack->SetBaseDamage(1, 50);      // 1级50伤害
        basic_attack->SetManaCost(1, 0);
        basic_attack->SetCooldown(1, 1000);       // 1秒冷却
        basic_attack->SetRequiredLevel(1);

        skill_cache_[1] = basic_attack;
        loaded_count++;
    }

    // 示例2: 创建火球术
    {
        SkillInfo* fireball = new SkillInfo();
        fireball->SetSkillID(100);
        fireball->SetSkillName("火球术");
        fireball->SetSkillType(SkillType::Active);
        fireball->SetTargetType(SkillTargetType::Single);
        fireball->SetDamageType(SkillDamageType::Magic);
        fireball->SetBaseDamage(1, 120);     // 1级120伤害
        fireball->SetManaCost(1, 20);
        fireball->SetCooldown(1, 3000);      // 3秒冷却
        fireball->SetRequiredLevel(5);
        fireball->SetRange(15.0f);

        skill_cache_[100] = fireball;
        loaded_count++;
    }

    // 示例3: 创建治疗术
    {
        SkillInfo* heal = new SkillInfo();
        heal->SetSkillID(200);
        heal->SetSkillName("治疗术");
        heal->SetSkillType(SkillType::Active);
        heal->SetTargetType(SkillTargetType::Single);
        heal->SetDamageType(SkillDamageType::Heal);
        heal->SetBaseHeal(1, 100);        // 1级100治疗
        heal->SetManaCost(1, 15);
        heal->SetCooldown(1, 2000);       // 2秒冷却
        heal->SetRequiredLevel(3);

        skill_cache_[200] = heal;
        loaded_count++;
    }

    // 示例4: 创建范围伤害技能 - 雷暴
    {
        SkillInfo* thunderstorm = new SkillInfo();
        thunderstorm->SetSkillID(300);
        thunderstorm->SetSkillName("雷暴");
        thunderstorm->SetSkillType(SkillType::Active);
        thunderstorm->SetTargetType(SkillTargetType::Area);
        thunderstorm->SetDamageType(SkillDamageType::Magic);
        thunderstorm->SetBaseDamage(1, 80);      // 1级80伤害
        thunderstorm->SetManaCost(1, 50);
        thunderstorm->SetCooldown(1, 10000);    // 10秒冷却
        thunderstorm->SetRequiredLevel(10);
        thunderstorm->SetRadius(10.0f);
        thunderstorm->SetAreaShape(SkillAreaShape::Circle);

        skill_cache_[300] = thunderstorm;
        loaded_count++;
    }

    // 示例5: 创建Buff技能 - 力量提升
    {
        SkillInfo* power_buff = new SkillInfo();
        power_buff->SetSkillID(400);
        power_buff->SetSkillName("力量提升");
        power_buff->SetSkillType(SkillType::Buff);
        power_buff->SetTargetType(SkillTargetType::Single);
        power_buff->SetDuration(1, 60000);     // 60秒
        power_buff->SetManaCost(1, 10);
        power_buff->SetCooldown(1, 60000);    // 60秒冷却
        power_buff->SetRequiredLevel(1);

        skill_cache_[400] = power_buff;
        loaded_count++;
    }

    // 添加到数据库
    for (auto& pair : skill_cache_) {
        SkillDatabase::GetInstance().AddSkill(pair.second);
    }

    spdlog::info("[ConfigLoader] 技能配置加载完成: {} 个技能", loaded_count);
    initialized_ = true;

    return loaded_count;
}

int SimpleConfigLoader::LoadMonsterConfigs() {
    spdlog::info("[ConfigLoader] 开始加载怪物配置...");

    // TODO: 实现怪物配置加载
    spdlog::info("[ConfigLoader] 怪物配置加载完成: 0 个怪物 (待实现)");
    return 0;
}

int SimpleConfigLoader::LoadItemConfigs() {
    spdlog::info("[ConfigLoader] 开始加载物品配置...");

    // TODO: 实现物品配置加载
    spdlog::info("[ConfigLoader] 物品配置加载完成: 0 个物品 (待实现)");
    return 0;
}

SkillInfo* SimpleConfigLoader::GetSkillInfo(uint32_t skill_id) {
    if (!initialized_) {
        LoadSkillConfigs();
    }

    auto it = skill_cache_.find(skill_id);
    if (it != skill_cache_.end()) {
        return it->second;
    }

    return nullptr;
}
