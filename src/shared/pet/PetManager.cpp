#include "PetManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <chrono>
#include <random>
#include <algorithm>

namespace Murim {
namespace Game {

// ========== PetDefinition ==========

murim::PetDefinition PetDefinition::ToProto() const {
    murim::PetDefinition proto;
    proto.set_pet_id(pet_id);
    proto.set_name(name);
    proto.set_description(description);
    proto.set_quality(quality);
    proto.set_growth_type(growth_type);

    // 基础属性
    proto.set_base_hp(base_hp);
    proto.set_base_mp(base_mp);
    proto.set_base_attack(base_attack);
    proto.set_base_defense(base_defense);
    proto.set_base_magic_attack(base_magic_attack);
    proto.set_base_magic_defense(base_magic_defense);
    proto.set_base_speed(base_speed);

    // 成长率
    proto.set_hp_growth(hp_growth);
    proto.set_mp_growth(mp_growth);
    proto.set_attack_growth(attack_growth);
    proto.set_defense_growth(defense_growth);
    proto.set_magic_attack_growth(magic_attack_growth);
    proto.set_magic_defense_growth(magic_defense_growth);
    proto.set_speed_growth(speed_growth);

    // 其他属性
    proto.set_capture_rate(capture_rate);
    proto.set_max_level(max_level);
    proto.set_evolve_level(evolve_level);
    proto.set_evolve_to(evolve_to);

    // 战斗属性
    proto.set_attack_range(attack_range);
    proto.set_skill_id(skill_id);
    for (uint32_t skill_id : skill_ids) {
        proto.add_skill_ids(skill_id);
    }

    // 外观
    proto.set_model_id(model_id);
    proto.set_icon_id(icon_id);

    return proto;
}

// ========== PetData ==========

murim::PetData PetData::ToProto() const {
    murim::PetData proto;
    proto.set_pet_unique_id(pet_unique_id);
    proto.set_pet_id(pet_id);
    proto.set_name(name);
    proto.set_level(level);
    proto.set_exp(exp);
    proto.set_state(state);

    // 当前属性
    proto.set_hp(hp);
    proto.set_mp(mp);
    proto.set_max_hp(max_hp);
    proto.set_max_mp(max_mp);
    proto.set_attack(attack);
    proto.set_defense(defense);
    proto.set_magic_attack(magic_attack);
    proto.set_magic_defense(magic_defense);
    proto.set_speed(speed);

    // 成长属性
    proto.set_potential(potential);
    proto.set_loyalty(loyalty);
    proto.set_intimacy(intimacy);

    // 装备
    proto.set_equipment_id(equipment_id);

    // 技能
    for (uint32_t skill_id : learned_skills) {
        proto.add_learned_skills(skill_id);
    }

    // 时间戳
    proto.set_birth_time(birth_time);
    proto.set_last_feed_time(last_feed_time);

    return proto;
}

uint64_t PetData::GetExpForNextLevel() const {
    // 经验公式：base * level^1.5
    uint32_t base_exp = 100;
    return static_cast<uint64_t>(base_exp * std::pow(level + 1, 1.5));
}

bool PetData::CanEvolve() const {
    // 检查等级是否满足进化条件
    // TODO: 从PetDefinition中获取evolve_level进行比较
    return false;
}

// ========== PetManager ==========

PetManager& PetManager::Instance() {
    static PetManager instance;
    return instance;
}

PetManager::PetManager()
    : next_unique_pet_id_(1)
    , max_pet_slots_(5)
    , base_exp_gain_(100)
    , intimacy_decay_rate_(1) {
}

bool PetManager::Initialize() {
    spdlog::info("Initializing PetManager...");

    // TODO: 从数据库或配置文件加载宠物定义

    // 示例：注册一些基础宠物
    PetDefinition slime;
    slime.pet_id = 3001;
    slime.name = "史莱姆";
    slime.description = "最基础的宠物，虽然弱小但很可爱";
    slime.quality = murim::PetQuality::COMMON;
    slime.growth_type = murim::PetGrowthType::BALANCED;

    slime.base_hp = 100;
    slime.base_mp = 50;
    slime.base_attack = 10;
    slime.base_defense = 10;
    slime.base_magic_attack = 10;
    slime.base_magic_defense = 10;
    slime.base_speed = 10;

    slime.hp_growth = 10.0f;
    slime.mp_growth = 5.0f;
    slime.attack_growth = 1.0f;
    slime.defense_growth = 1.0f;
    slime.magic_attack_growth = 1.0f;
    slime.magic_defense_growth = 1.0f;
    slime.speed_growth = 1.0f;

    slime.capture_rate = 5000;  // 50%捕捉率
    slime.max_level = 50;
    slime.evolve_level = 10;
    slime.evolve_to = 3002;

    slime.attack_range = 2;
    slime.skill_id = 0;
    slime.model_id = 1001;
    slime.icon_id = 2001;

    RegisterPet(slime);

    PetDefinition wolf;
    wolf.pet_id = 3011;
    wolf.name = "森林狼";
    wolf.description = "凶猛的野兽，忠诚的伙伴";
    wolf.quality = murim::PetQuality::GOOD;
    wolf.growth_type = murim::PetGrowthType::AGILITY;

    wolf.base_hp = 200;
    wolf.base_mp = 30;
    wolf.base_attack = 25;
    wolf.base_defense = 15;
    wolf.base_magic_attack = 5;
    wolf.base_magic_defense = 10;
    wolf.base_speed = 30;

    wolf.hp_growth = 15.0f;
    wolf.mp_growth = 2.0f;
    wolf.attack_growth = 3.0f;
    wolf.defense_growth = 1.5f;
    wolf.magic_attack_growth = 0.5f;
    wolf.magic_defense_growth = 1.0f;
    wolf.speed_growth = 4.0f;

    wolf.capture_rate = 7000;  // 70%捕捉率
    wolf.max_level = 60;
    wolf.evolve_level = 0;  // 不可进化
    wolf.evolve_to = 0;

    wolf.attack_range = 3;
    wolf.skill_id = 1001;
    wolf.model_id = 1011;
    wolf.icon_id = 2011;

    RegisterPet(wolf);

    spdlog::info("PetManager initialized successfully");
    return true;
}

// ========== 宠物定义管理 ==========

bool PetManager::RegisterPet(const PetDefinition& definition) {
    if (pet_definitions_.find(definition.pet_id) != pet_definitions_.end()) {
        spdlog::warn("Pet already registered: {}", definition.pet_id);
        return false;
    }

    pet_definitions_[definition.pet_id] = definition;
    spdlog::debug("Registered pet: {} - {}", definition.pet_id, definition.name);
    return true;
}

PetDefinition* PetManager::GetPetDefinition(uint32_t pet_id) {
    auto it = pet_definitions_.find(pet_id);
    if (it != pet_definitions_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<PetDefinition> PetManager::GetAllPetDefinitions() {
    std::vector<PetDefinition> result;
    for (const auto& [id, definition] : pet_definitions_) {
        result.push_back(definition);
    }

    // 按 pet_id 排序
    std::sort(result.begin(), result.end(),
        [](const PetDefinition& a, const PetDefinition& b) {
            return a.pet_id < b.pet_id;
        });

    return result;
}

// ========== 玩家宠物管理 ==========

std::vector<PetData>* PetManager::GetPlayerPets(uint64_t character_id) {
    auto it = player_pets_.find(character_id);
    if (it != player_pets_.end()) {
        return &it->second;
    }

    // 创建新列表
    player_pets_[character_id] = std::vector<PetData>();
    return &player_pets_[character_id];
}

PetData* PetManager::GetPet(uint64_t character_id, uint64_t pet_unique_id) {
    auto* pets = GetPlayerPets(character_id);
    if (!pets) {
        return nullptr;
    }

    for (auto& pet : *pets) {
        if (pet.pet_unique_id == pet_unique_id) {
            return &pet;
        }
    }

    return nullptr;
}

PetData* PetManager::CapturePet(uint64_t character_id, uint32_t pet_id, uint32_t monster_level) {
    auto* definition = GetPetDefinition(pet_id);
    if (!definition) {
        spdlog::warn("Pet definition not found: {}", pet_id);
        return nullptr;
    }

    // 检查宠物栏是否已满
    if (IsPetSlotFull(character_id)) {
        spdlog::warn("Pet slot full: character_id={}", character_id);
        return nullptr;
    }

    // 创建新宠物
    PetData new_pet;
    new_pet.pet_unique_id = GenerateUniquePetId();
    new_pet.pet_id = pet_id;
    new_pet.name = definition->name;
    new_pet.level = monster_level;
    new_pet.exp = 0;
    new_pet.state = murim::PetState::INACTIVE;

    // 计算初始属性
    CalculatePetStats(new_pet, *definition);

    // 成长属性
    new_pet.potential = 10;  // 初始10点潜能
    new_pet.loyalty = 50;    // 初始忠诚度
    new_pet.intimacy = 30;   // 初始亲密度

    new_pet.equipment_id = 0;

    // 学习初始技能
    if (definition->skill_id != 0) {
        new_pet.learned_skills.push_back(definition->skill_id);
    }

    new_pet.birth_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    new_pet.last_feed_time = 0;

    // 添加到玩家宠物列表
    auto* pets = GetPlayerPets(character_id);
    pets->push_back(new_pet);

    spdlog::info("Pet captured: character_id={}, pet_id={}, unique_id={}, name={}",
                 character_id, pet_id, new_pet.pet_unique_id, new_pet.name);

    // 发送通知
    NotifyPetObtained(character_id, new_pet);

    return GetPet(character_id, new_pet.pet_unique_id);
}

bool PetManager::ReleasePet(uint64_t character_id, uint64_t pet_unique_id) {
    auto* pets = GetPlayerPets(character_id);
    if (!pets) {
        return false;
    }

    auto it = std::find_if(pets->begin(), pets->end(),
        [pet_unique_id](const PetData& pet) {
            return pet.pet_unique_id == pet_unique_id;
        });

    if (it != pets->end()) {
        // 如果是当前召唤的宠物，先召回
        if (summoned_pets_[character_id] == pet_unique_id) {
            UnsummonPet(character_id);
        }

        spdlog::info("Pet released: character_id={}, pet_unique_id={}", character_id, pet_unique_id);
        pets->erase(it);
        return true;
    }

    return false;
}

bool PetManager::RenamePet(uint64_t character_id, uint64_t pet_unique_id, const std::string& new_name) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        return false;
    }

    pet->name = new_name;
    spdlog::info("Pet renamed: character_id={}, pet_unique_id={}, new_name={}",
                 character_id, pet_unique_id, new_name);
    return true;
}

// ========== 宠物召唤 ==========

bool PetManager::SummonPet(uint64_t character_id, uint64_t pet_unique_id) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        spdlog::warn("Pet not found: character_id={}, pet_unique_id={}", character_id, pet_unique_id);
        return false;
    }

    if (pet->state == murim::PetState::DEAD) {
        spdlog::warn("Cannot summon dead pet: pet_unique_id={}", pet_unique_id);
        return false;
    }

    // 检查是否已有召唤的宠物
    if (summoned_pets_.find(character_id) != summoned_pets_.end()) {
        // 先召回当前宠物
        UnsummonPet(character_id);
    }

    // 召唤新宠物
    pet->state = murim::PetState::ACTIVE;
    summoned_pets_[character_id] = pet_unique_id;

    spdlog::info("Pet summoned: character_id={}, pet_unique_id={}, name={}",
                 character_id, pet_unique_id, pet->name);

    return true;
}

bool PetManager::UnsummonPet(uint64_t character_id) {
    auto it = summoned_pets_.find(character_id);
    if (it == summoned_pets_.end()) {
        return false;
    }

    uint64_t pet_unique_id = it->second;
    auto* pet = GetPet(character_id, pet_unique_id);
    if (pet) {
        pet->state = murim::PetState::INACTIVE;
    }

    summoned_pets_.erase(it);
    spdlog::info("Pet unsummoned: character_id={}, pet_unique_id={}", character_id, pet_unique_id);

    return true;
}

PetData* PetManager::GetSummonedPet(uint64_t character_id) {
    auto it = summoned_pets_.find(character_id);
    if (it == summoned_pets_.end()) {
        return nullptr;
    }

    return GetPet(character_id, it->second);
}

// ========== 宠物养成 ==========

bool PetManager::AddPetExp(uint64_t character_id, uint64_t pet_unique_id, uint64_t exp_amount) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        return false;
    }

    uint32_t old_level = pet->level;
    pet->exp += exp_amount;

    // 检查是否升级
    while (pet->exp >= pet->GetExpForNextLevel() && pet->level < 50) {
        pet->exp -= pet->GetExpForNextLevel();
        pet->level++;

        // 升级时获得潜能点
        pet->potential += 2;

        // 重新计算属性
        auto* definition = GetPetDefinition(pet->pet_id);
        if (definition) {
            CalculatePetStats(*pet, *definition);
        }

        spdlog::info("Pet leveled up: character_id={}, pet_unique_id={}, level={}",
                     character_id, pet_unique_id, pet->level);
    }

    if (pet->level > old_level) {
        NotifyPetLevelUp(character_id, *pet, old_level);
    }

    return true;
}

bool PetManager::LevelUpPet(uint64_t character_id, uint64_t pet_unique_id, uint64_t exp_amount) {
    return AddPetExp(character_id, pet_unique_id, exp_amount);
}

bool PetManager::TrainPet(uint64_t character_id, uint64_t pet_unique_id, uint32_t stat_type, uint32_t points) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        return false;
    }

    if (pet->potential < points) {
        spdlog::warn("Not enough potential points: have={}, need={}", pet->potential, points);
        return false;
    }

    // 应用培养点数
    switch (stat_type) {
        case 1:  // HP
            pet->max_hp += points * 5;
            pet->hp = pet->max_hp;
            break;
        case 2:  // MP
            pet->max_mp += points * 5;
            pet->mp = pet->max_mp;
            break;
        case 3:  // 攻击
            pet->attack += points * 2;
            break;
        case 4:  // 防御
            pet->defense += points * 2;
            break;
        case 5:  // 速度
            pet->speed += points * 2;
            break;
        default:
            spdlog::warn("Invalid stat type: {}", stat_type);
            return false;
    }

    pet->potential -= points;

    spdlog::info("Pet trained: character_id={}, pet_unique_id={}, stat_type={}, points={}, remaining={}",
                 character_id, pet_unique_id, stat_type, points, pet->potential);

    return true;
}

bool PetManager::EvolvePet(uint64_t character_id, uint64_t pet_unique_id) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        return false;
    }

    auto* current_def = GetPetDefinition(pet->pet_id);
    if (!current_def || current_def->evolve_level == 0 || current_def->evolve_to == 0) {
        spdlog::warn("Pet cannot evolve: pet_id={}", pet->pet_id);
        return false;
    }

    if (pet->level < current_def->evolve_level) {
        spdlog::warn("Pet level insufficient for evolution: level={}, require={}",
                     pet->level, current_def->evolve_level);
        return false;
    }

    auto* new_def = GetPetDefinition(current_def->evolve_to);
    if (!new_def) {
        spdlog::warn("Evolved pet definition not found: pet_id={}", current_def->evolve_to);
        return false;
    }

    uint32_t old_pet_id = pet->pet_id;
    pet->pet_id = current_def->evolve_to;
    pet->name = new_def->name;

    // 重新计算属性
    CalculatePetStats(*pet, *new_def);

    spdlog::info("Pet evolved: character_id={}, pet_unique_id={}, {} -> {}",
                 character_id, pet_unique_id, old_pet_id, pet->pet_id);

    return true;
}

bool PetManager::FeedPet(uint64_t character_id, uint64_t pet_unique_id, uint32_t food_id, uint32_t count) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        return false;
    }

    // TODO: 从ItemManager检查食物ID是否有效
    uint32_t exp_per_food = 100;
    uint32_t intimacy_per_food = 5;

    // 增加经验和亲密度
    AddPetExp(character_id, pet_unique_id, exp_per_food * count);
    pet->intimacy = std::min(static_cast<int32_t>(100), pet->intimacy + static_cast<int32_t>(intimacy_per_food * count));

    pet->last_feed_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    spdlog::info("Pet fed: character_id={}, pet_unique_id={}, food_id={}, count={}",
                 character_id, pet_unique_id, food_id, count);

    return true;
}

// ========== 宠物技能 ==========

bool PetManager::LearnSkill(uint64_t character_id, uint64_t pet_unique_id, uint32_t skill_id) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        return false;
    }

    // 检查是否已学习
    if (HasLearnedSkill(character_id, pet_unique_id, skill_id)) {
        spdlog::debug("Pet already learned skill: pet_unique_id={}, skill_id={}", pet_unique_id, skill_id);
        return true;
    }

    pet->learned_skills.push_back(skill_id);

    spdlog::info("Pet learned skill: character_id={}, pet_unique_id={}, skill_id={}",
                 character_id, pet_unique_id, skill_id);

    return true;
}

bool PetManager::HasLearnedSkill(uint64_t character_id, uint64_t pet_unique_id, uint32_t skill_id) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        return false;
    }

    for (uint32_t learned_id : pet->learned_skills) {
        if (learned_id == skill_id) {
            return true;
        }
    }

    return false;
}

// ========== 宠物装备 ==========

bool PetManager::EquipItem(uint64_t character_id, uint64_t pet_unique_id, uint32_t equipment_id) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        return false;
    }

    pet->equipment_id = equipment_id;

    spdlog::info("Pet item equipped: character_id={}, pet_unique_id={}, equipment_id={}",
                 character_id, pet_unique_id, equipment_id);

    return true;
}

bool PetManager::UnequipItem(uint64_t character_id, uint64_t pet_unique_id) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        return false;
    }

    pet->equipment_id = 0;

    spdlog::info("Pet item unequipped: character_id={}, pet_unique_id={}", character_id, pet_unique_id);

    return true;
}

// ========== 宠物战斗 ==========

bool PetManager::TakeDamage(uint64_t character_id, uint64_t pet_unique_id, uint32_t damage) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        return false;
    }

    if (pet->state != murim::PetState::ACTIVE) {
        return false;
    }

    if (damage >= pet->hp) {
        pet->hp = 0;
        OnPetDead(character_id, pet_unique_id);
    } else {
        pet->hp -= damage;
    }

    return true;
}

bool PetManager::OnPetDead(uint64_t character_id, uint64_t pet_unique_id) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        return false;
    }

    pet->state = murim::PetState::DEAD;
    UnsummonPet(character_id);

    spdlog::info("Pet dead: character_id={}, pet_unique_id={}", character_id, pet_unique_id);

    // TODO: 发送宠物死亡通知
    return true;
}

bool PetManager::RevivePet(uint64_t character_id, uint64_t pet_unique_id) {
    auto* pet = GetPet(character_id, pet_unique_id);
    if (!pet) {
        return false;
    }

    if (pet->state != murim::PetState::DEAD) {
        return false;
    }

    pet->hp = pet->max_hp / 2;  // 复活后恢复50% HP
    pet->mp = pet->max_mp / 2;  // 复活后恢复50% MP
    pet->state = murim::PetState::INACTIVE;

    spdlog::info("Pet revived: character_id={}, pet_unique_id={}", character_id, pet_unique_id);

    // TODO: 发送宠物复活通知
    return true;
}

// ========== 辅助方法 ==========

uint64_t PetManager::GenerateUniquePetId() {
    return next_unique_pet_id_++;
}

void PetManager::CalculatePetStats(PetData& pet, const PetDefinition& definition) {
    // 计算等级属性
    uint32_t level_bonus = pet.level - 1;

    pet.max_hp = definition.base_hp + static_cast<uint32_t>(definition.hp_growth * level_bonus);
    pet.max_mp = definition.base_mp + static_cast<uint32_t>(definition.mp_growth * level_bonus);
    pet.attack = definition.base_attack + static_cast<uint32_t>(definition.attack_growth * level_bonus);
    pet.defense = definition.base_defense + static_cast<uint32_t>(definition.defense_growth * level_bonus);
    pet.magic_attack = definition.base_magic_attack + static_cast<uint32_t>(definition.magic_attack_growth * level_bonus);
    pet.magic_defense = definition.base_magic_defense + static_cast<uint32_t>(definition.magic_defense_growth * level_bonus);
    pet.speed = definition.base_speed + static_cast<uint32_t>(definition.speed_growth * level_bonus);

    // 初始化HP/MP
    if (pet.hp == 0 || pet.hp > pet.max_hp) {
        pet.hp = pet.max_hp;
    }
    if (pet.mp == 0 || pet.mp > pet.max_mp) {
        pet.mp = pet.max_mp;
    }
}

bool PetManager::IsPetSlotFull(uint64_t character_id) {
    auto* pets = GetPlayerPets(character_id);
    return pets && pets->size() >= max_pet_slots_;
}

void PetManager::NotifyPetObtained(uint64_t character_id, const PetData& pet) {
    // TODO: 发送宠物获取通知给客户端
    spdlog::info("Notification: Pet obtained - character_id={}, name={}", character_id, pet.name);
}

void PetManager::NotifyPetLevelUp(uint64_t character_id, const PetData& pet, uint32_t old_level) {
    // TODO: 发送宠物升级通知给客户端
    spdlog::info("Notification: Pet leveled up - character_id={}, name={}, {} -> {}",
                character_id, pet.name, old_level, pet.level);
}

} // namespace Game
} // namespace Murim
