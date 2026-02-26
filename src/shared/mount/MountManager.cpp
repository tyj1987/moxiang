#include "MountManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <chrono>
#include <algorithm>

namespace Murim {
namespace Game {

// ========== MountDefinition ==========

murim::MountDefinition MountDefinition::ToProto() const {
    murim::MountDefinition proto;
    proto.set_mount_id(mount_id);
    proto.set_name(name);
    proto.set_description(description);
    proto.set_quality(quality);
    proto.set_type(type);

    // 基础属性
    proto.set_base_speed_bonus(base_speed_bonus);
    proto.set_max_star_level(max_star_level);
    proto.set_evolve_mount_id(evolve_mount_id);

    // 星级属性加成
    for (uint32_t bonus : speed_bonus_per_star) {
        proto.add_speed_bonus_per_star(bonus);
    }

    // 技能
    proto.set_skill_id(skill_id);
    proto.set_skill_unlock_star(skill_unlock_star);

    // 外观
    proto.set_model_id(model_id);
    proto.set_icon_id(icon_id);
    proto.set_mesh_name(mesh_name);

    return proto;
}

uint32_t MountDefinition::GetSpeedBonus(uint32_t star_level) const {
    if (star_level == 0 || star_level > speed_bonus_per_star.size()) {
        return base_speed_bonus;
    }
    return base_speed_bonus + speed_bonus_per_star[star_level - 1];
}

// ========== PlayerMount ==========

murim::MountData PlayerMount::ToProto() const {
    murim::MountData proto;
    proto.set_mount_id(mount_id);
    proto.set_star_level(star_level);
    proto.set_state(state);
    proto.set_exp(exp);

    // 装备
    proto.set_saddle_id(saddle_id);
    proto.set_decoration_id(decoration_id);

    // 技能
    proto.set_skill_unlocked(skill_unlocked);
    if (skill_unlocked) {
        proto.set_skill_id(skill_id);
    }

    // 获得时间
    proto.set_obtain_time(obtain_time);

    return proto;
}

uint64_t PlayerMount::GetExpForNextStar() const {
    // 进化经验公式：base * star^2
    uint32_t base_exp = 1000;
    return static_cast<uint64_t>(base_exp * std::pow(star_level + 1, 2));
}

bool PlayerMount::CanEvolve() const {
    // 检查经验是否足够
    return exp >= GetExpForNextStar();
}

// ========== MountManager ==========

MountManager& MountManager::Instance() {
    static MountManager instance;
    return instance;
}

MountManager::MountManager()
    : max_mount_slots_(10)
    , base_evolve_exp_(1000)
    , evolve_material_per_star_(10) {
}

bool MountManager::Initialize() {
    spdlog::info("Initializing MountManager...");

    // TODO: 从数据库或配置文件加载坐骑定义

    // 示例：注册一些基础坐骑
    MountDefinition horse;
    horse.mount_id = 4001;
    horse.name = "白马";
    horse.description = "最常见的坐骑，提供基础的速度加成";
    horse.quality = murim::MountQuality::COMMON;
    horse.type = murim::MountType::LAND;

    horse.base_speed_bonus = 20;  // 20%速度加成
    horse.max_star_level = 5;
    horse.evolve_mount_id = 4002;  // 可进化为战马

    // 每星速度加成：+5%, +10%, +15%, +20%, +25%
    horse.speed_bonus_per_star = {5, 10, 15, 20, 25};

    horse.skill_id = 0;
    horse.skill_unlock_star = 0;

    horse.model_id = 2001;
    horse.icon_id = 3001;
    horse.mesh_name = "Horse_White";

    RegisterMount(horse);

    MountDefinition war_horse;
    war_horse.mount_id = 4002;
    war_horse.name = "战马";
    war_horse.description = "经过训练的战马，速度更快";
    war_horse.quality = murim::MountQuality::RARE;
    war_horse.type = murim::MountType::LAND;

    war_horse.base_speed_bonus = 40;
    war_horse.max_star_level = 5;
    war_horse.evolve_mount_id = 0;  // 不可进化

    war_horse.speed_bonus_per_star = {10, 15, 20, 25, 30};

    war_horse.skill_id = 5001;  // 冲刺技能
    war_horse.skill_unlock_star = 3;  // 3星解锁

    war_horse.model_id = 2002;
    war_horse.icon_id = 3002;
    war_horse.mesh_name = "Horse_War";

    RegisterMount(war_horse);

    MountDefinition flying_carpet;
    flying_carpet.mount_id = 4011;
    flying_carpet.name = "飞毯";
    flying_carpet.description = "神奇的飞毯，可以飞行";
    flying_carpet.quality = murim::MountQuality::EPIC;
    flying_carpet.type = murim::MountType::FLYING;

    flying_carpet.base_speed_bonus = 60;  // 飞行坐骑速度更快
    flying_carpet.max_star_level = 5;
    flying_carpet.evolve_mount_id = 0;

    flying_carpet.speed_bonus_per_star = {15, 20, 25, 30, 35};

    flying_carpet.skill_id = 5002;  // 飞行冲刺
    flying_carpet.skill_unlock_star = 1;

    flying_carpet.model_id = 2011;
    flying_carpet.icon_id = 3011;
    flying_carpet.mesh_name = "FlyingCarpet";

    RegisterMount(flying_carpet);

    spdlog::info("MountManager initialized successfully");
    return true;
}

// ========== 坐骑定义管理 ==========

bool MountManager::RegisterMount(const MountDefinition& definition) {
    if (mount_definitions_.find(definition.mount_id) != mount_definitions_.end()) {
        spdlog::warn("Mount already registered: {}", definition.mount_id);
        return false;
    }

    mount_definitions_[definition.mount_id] = definition;
    spdlog::debug("Registered mount: {} - {}", definition.mount_id, definition.name);
    return true;
}

MountDefinition* MountManager::GetMountDefinition(uint32_t mount_id) {
    auto it = mount_definitions_.find(mount_id);
    if (it != mount_definitions_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<MountDefinition> MountManager::GetAllMountDefinitions() {
    std::vector<MountDefinition> result;
    for (const auto& [id, definition] : mount_definitions_) {
        result.push_back(definition);
    }

    // 按 mount_id 排序
    std::sort(result.begin(), result.end(),
        [](const MountDefinition& a, const MountDefinition& b) {
            return a.mount_id < b.mount_id;
        });

    return result;
}

// ========== 玩家坐骑管理 ==========

std::vector<PlayerMount>* MountManager::GetPlayerMounts(uint64_t character_id) {
    auto it = player_mounts_.find(character_id);
    if (it != player_mounts_.end()) {
        return &it->second;
    }

    // 创建新列表
    player_mounts_[character_id] = std::vector<PlayerMount>();
    return &player_mounts_[character_id];
}

PlayerMount* MountManager::GetMount(uint64_t character_id, uint32_t mount_id) {
    auto* mounts = GetPlayerMounts(character_id);
    if (!mounts) {
        return nullptr;
    }

    for (auto& mount : *mounts) {
        if (mount.mount_id == mount_id) {
            return &mount;
        }
    }

    return nullptr;
}

bool MountManager::HasMount(uint64_t character_id, uint32_t mount_id) {
    return GetMount(character_id, mount_id) != nullptr;
}

bool MountManager::GrantMount(uint64_t character_id, uint32_t mount_id) {
    auto* definition = GetMountDefinition(mount_id);
    if (!definition) {
        spdlog::warn("Mount definition not found: {}", mount_id);
        return false;
    }

    // 检查是否已拥有
    if (HasMount(character_id, mount_id)) {
        spdlog::debug("Character already has mount: character_id={}, mount_id={}",
                     character_id, mount_id);
        return true;
    }

    // 创建新坐骑
    PlayerMount new_mount;
    new_mount.mount_id = mount_id;
    new_mount.star_level = 1;
    new_mount.state = murim::MountState::INACTIVE;
    new_mount.exp = 0;

    new_mount.saddle_id = 0;
    new_mount.decoration_id = 0;

    new_mount.skill_unlocked = false;
    new_mount.skill_id = 0;

    new_mount.obtain_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    // 添加到玩家坐骑列表
    auto* mounts = GetPlayerMounts(character_id);
    mounts->push_back(new_mount);

    spdlog::info("Mount granted: character_id={}, mount_id={}, name={}",
                 character_id, mount_id, definition->name);

    // 发送通知
    NotifyMountObtained(character_id, new_mount);

    return true;
}

// ========== 坐骑骑乘 ==========

bool MountManager::RideMount(uint64_t character_id, uint32_t mount_id) {
    if (!HasMount(character_id, mount_id)) {
        spdlog::warn("Character doesn't have mount: character_id={}, mount_id={}",
                     character_id, mount_id);
        return false;
    }

    // 检查是否已有骑乘的坐骑
    if (ridden_mounts_.find(character_id) != ridden_mounts_.end()) {
        // 先下马
        UnrideMount(character_id);
    }

    // 骑乘新坐骑
    auto* mount = GetMount(character_id, mount_id);
    if (mount) {
        mount->state = murim::MountState::ACTIVE;
    }

    ridden_mounts_[character_id] = mount_id;

    spdlog::info("Mount ridden: character_id={}, mount_id={}", character_id, mount_id);

    return true;
}

bool MountManager::UnrideMount(uint64_t character_id) {
    auto it = ridden_mounts_.find(character_id);
    if (it == ridden_mounts_.end()) {
        return false;
    }

    uint32_t mount_id = it->second;
    auto* mount = GetMount(character_id, mount_id);
    if (mount) {
        mount->state = murim::MountState::INACTIVE;
    }

    ridden_mounts_.erase(it);
    spdlog::info("Mount unriden: character_id={}, mount_id={}", character_id, mount_id);

    return true;
}

PlayerMount* MountManager::GetRiddenMount(uint64_t character_id) {
    auto it = ridden_mounts_.find(character_id);
    if (it == ridden_mounts_.end()) {
        return nullptr;
    }

    return GetMount(character_id, it->second);
}

uint32_t MountManager::GetCurrentSpeedBonus(uint64_t character_id) {
    auto* mount = GetRiddenMount(character_id);
    if (!mount) {
        return 0;  // 未骑乘坐骑，无速度加成
    }

    return CalculateTotalSpeedBonus(character_id, *mount);
}

// ========== 坐骑进化 ==========

bool MountManager::AddExp(uint64_t character_id, uint32_t mount_id, uint64_t exp_amount) {
    auto* mount = GetMount(character_id, mount_id);
    if (!mount) {
        return false;
    }

    mount->exp += exp_amount;

    // 检查是否可以进化
    if (mount->CanEvolve()) {
        // 自动进化
        uint32_t old_star = mount->star_level;
        mount->star_level++;
        mount->exp = 0;

        // 检查技能解锁
        auto* definition = GetMountDefinition(mount_id);
        if (definition && mount->star_level >= definition->skill_unlock_star && definition->skill_id != 0) {
            mount->skill_unlocked = true;
            mount->skill_id = definition->skill_id;
        }

        spdlog::info("Mount evolved: character_id={}, mount_id={}, star: {} -> {}",
                     character_id, mount_id, old_star, mount->star_level);

        NotifyMountEvolved(character_id, *mount, old_star);
    }

    return true;
}

bool MountManager::EvolveMount(uint64_t character_id, uint32_t mount_id, uint32_t material_count) {
    auto* mount = GetMount(character_id, mount_id);
    if (!mount) {
        return false;
    }

    auto* definition = GetMountDefinition(mount_id);
    if (!definition || mount->star_level >= definition->max_star_level) {
        spdlog::warn("Mount cannot evolve: mount_id={}, current_star={}, max_star={}",
                     mount_id, mount->star_level, definition ? definition->max_star_level : 0);
        return false;
    }

    // TODO: 检查进化材料是否足够
    // if (material_count < evolve_material_per_star_) {
    //     return false;
    // }

    // 增加进化经验
    uint64_t exp_gain = material_count * 100;
    return AddExp(character_id, mount_id, exp_gain);
}

// ========== 坐骑技能 ==========

bool MountManager::UnlockSkill(uint64_t character_id, uint32_t mount_id) {
    auto* mount = GetMount(character_id, mount_id);
    if (!mount) {
        return false;
    }

    auto* definition = GetMountDefinition(mount_id);
    if (!definition || definition->skill_id == 0) {
        spdlog::warn("Mount has no skill: mount_id={}", mount_id);
        return false;
    }

    if (mount->star_level < definition->skill_unlock_star) {
        spdlog::warn("Star level insufficient for skill: current={}, require={}",
                     mount->star_level, definition->skill_unlock_star);
        return false;
    }

    mount->skill_unlocked = true;
    mount->skill_id = definition->skill_id;

    spdlog::info("Mount skill unlocked: character_id={}, mount_id={}, skill_id={}",
                 character_id, mount_id, mount->skill_id);

    NotifySkillUnlocked(character_id, *mount);

    return true;
}

bool MountManager::IsSkillUnlocked(uint64_t character_id, uint32_t mount_id) {
    auto* mount = GetMount(character_id, mount_id);
    return mount && mount->skill_unlocked;
}

// ========== 坐骑装备 ==========

bool MountManager::EquipSaddle(uint64_t character_id, uint32_t mount_id, uint32_t saddle_id) {
    auto* mount = GetMount(character_id, mount_id);
    if (!mount) {
        return false;
    }

    // TODO: 检查鞍具是否存在、星级要求等
    mount->saddle_id = saddle_id;

    spdlog::info("Mount saddle equipped: character_id={}, mount_id={}, saddle_id={}",
                 character_id, mount_id, saddle_id);

    return true;
}

bool MountManager::UnequipSaddle(uint64_t character_id, uint32_t mount_id) {
    auto* mount = GetMount(character_id, mount_id);
    if (!mount) {
        return false;
    }

    mount->saddle_id = 0;

    spdlog::info("Mount saddle unequipped: character_id={}, mount_id={}", character_id, mount_id);

    return true;
}

bool MountManager::EquipDecoration(uint64_t character_id, uint32_t mount_id, uint32_t decoration_id) {
    auto* mount = GetMount(character_id, mount_id);
    if (!mount) {
        return false;
    }

    mount->decoration_id = decoration_id;

    spdlog::info("Mount decoration equipped: character_id={}, mount_id={}, decoration_id={}",
                 character_id, mount_id, decoration_id);

    return true;
}

bool MountManager::UnequipDecoration(uint64_t character_id, uint32_t mount_id) {
    auto* mount = GetMount(character_id, mount_id);
    if (!mount) {
        return false;
    }

    mount->decoration_id = 0;

    spdlog::info("Mount decoration unequipped: character_id={}, mount_id={}", character_id, mount_id);

    return true;
}

// ========== 辅助方法 ==========

uint32_t MountManager::CalculateTotalSpeedBonus(uint64_t character_id, const PlayerMount& mount) {
    auto* definition = GetMountDefinition(mount.mount_id);
    if (!definition) {
        return 0;
    }

    // 基础速度加成 + 星级加成
    uint32_t total = definition->GetSpeedBonus(mount.star_level);

    // 鞍具加成
    // TODO: 从鞍具定义获取额外速度加成
    if (mount.saddle_id != 0) {
        total += 5;  // 假设鞍具提供+5%速度
    }

    return total;
}

void MountManager::NotifyMountObtained(uint64_t character_id, const PlayerMount& mount) {
    // TODO: 发送坐骑获得通知给客户端
    spdlog::info("Notification: Mount obtained - character_id={}, mount_id={}",
                character_id, mount.mount_id);
}

void MountManager::NotifyMountEvolved(uint64_t character_id, const PlayerMount& mount, uint32_t old_star) {
    // TODO: 发送坐骑进化通知给客户端
    spdlog::info("Notification: Mount evolved - character_id={}, mount_id={}, star: {} -> {}",
                character_id, mount.mount_id, old_star, mount.star_level);
}

void MountManager::NotifySkillUnlocked(uint64_t character_id, const PlayerMount& mount) {
    // TODO: 发送技能解锁通知给客户端
    spdlog::info("Notification: Mount skill unlocked - character_id={}, mount_id={}, skill_id={}",
                character_id, mount.mount_id, mount.skill_id);
}

} // namespace Game
} // namespace Murim
