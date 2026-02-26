// Murim MMORPG - NPC管理器实现

#include "NPCManager.hpp"
#include "core/database/NPCDAO.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>

namespace Murim {
namespace MapServer {

// ================================
// NPC类实现
// ================================

NPC::NPC(uint64_t object_id, const NPCInfo& info)
    : npc_id_(info.npc_id)
    , name_(info.name)
    , dialogue_id_(info.dialogue_id)
    , shop_id_(info.shop_id)
    , functions_(info.functions)
    , model_id_(info.model_id)
    , scale_(info.scale)
{
    // Initialize GameObject base class
    Game::GameObject::Init(
        object_id,
        Game::ObjectType::NPC,
        info.name,
        info.position_x,
        info.position_y,
        info.position_z
    );

    SetRotation(info.rotation);

    spdlog::debug("NPC created: id={}, name={}, position=({:.1f}, {:.1f}, {:.1f})",
                  npc_id_, name_, GetPositionX(), GetPositionY(), GetPositionZ());
}

bool NPC::CanInteract(uint64_t player_id, float distance) const {
    // 检查距离（默认交互距离700，参考Legacy: NPC_TALKING_DISTANCE）
    const float kInteractDistance = 700.0f;
    return distance <= kInteractDistance;
}

void NPC::Interact(uint64_t player_id) {
    spdlog::debug("Player {} interacting with NPC {}", player_id, name_);

    // 根据NPC功能执行不同操作
    if (IsShop()) {
        // 打开商店
        spdlog::debug("NPC {} is a shop, opening shop dialog", name_);
    } else if (IsWarehouse()) {
        // 打开仓库
        spdlog::debug("NPC {} is a warehouse, opening warehouse dialog", name_);
    } else if (IsQuestGiver()) {
        // 打开任务对话
        spdlog::debug("NPC {} is a quest giver, dialogue_id={}", name_, dialogue_id_);
    } else {
        // 普通对话
        spdlog::debug("NPC {} is a regular NPC, dialogue_id={}", name_, dialogue_id_);
    }

    // TODO: 发送网络消息给客户端，打开相应UI
}

// GameObject 纯虚函数实现

void NPC::OnRevive() {
    spdlog::debug("NPC {} revived", name_);
}

void NPC::OnDamaged(uint32_t damage, uint64_t attacker_id) {
    spdlog::debug("NPC {} damaged: {} damage from attacker {}", name_, damage, attacker_id);
}

void NPC::OnHealed(uint32_t amount, uint64_t healer_id) {
    spdlog::debug("NPC {} healed: {} amount from healer {}", name_, amount, healer_id);
}

void NPC::RemoveMP(uint32_t amount) {
    spdlog::debug("NPC {} MP reduced: {}", name_, amount);
}

void NPC::OnStateApplied(uint32_t state_id) {
    spdlog::debug("NPC {} state applied: {}", name_, state_id);
}

void NPC::OnStateRemoved(uint32_t state_id) {
    spdlog::debug("NPC {} state removed: {}", name_, state_id);
}

void NPC::OnDie() {
    spdlog::debug("NPC {} died", name_);
}

std::string NPC::ToString() const {
    return fmt::format("NPC(id={}, name={}, pos=({:.1f},{:.1f},{:.1f}))",
                        npc_id_, name_, GetPositionX(), GetPositionY(), GetPositionZ());
}

// ================================
// NPC管理器实现
// ================================

NPCManager& NPCManager::Instance() {
    static NPCManager instance;
    return instance;
}

bool NPCManager::Initialize() {
    spdlog::info("Initializing NPCManager...");

    // 从数据库加载NPC数据
    LoadNPCsFromDatabase();

    spdlog::info("NPCManager initialized: {} NPC templates loaded", npc_infos_.size());
    return true;
}

void NPCManager::Shutdown() {
    spdlog::info("Shutting down NPCManager...");
    npcs_by_object_id_.clear();
    npc_infos_.clear();
    npcs_by_map_.clear();
}

void NPCManager::LoadNPCs() {
    // TODO: 从文件加载NPC数据（Legacy兼容）
    spdlog::warn("LoadNPCs() from file not implemented, using database only");
}

void NPCManager::LoadNPCsFromDatabase() {
    spdlog::info("Loading NPCs from database...");

    try {
        // 从数据库加载所有NPC信息
        auto db_npcs = Murim::Core::Database::NPCDAO::LoadAll();

        for (const auto& db_npc : db_npcs) {
            // 转换 Database::NPCInfo → MapServer::NPCInfo
            NPCInfo npc_info;
            npc_info.npc_id = db_npc.npc_id;
            npc_info.name = db_npc.name;
            npc_info.map_id = db_npc.map_id;
            npc_info.position_x = db_npc.position_x;
            npc_info.position_y = db_npc.position_y;
            npc_info.position_z = db_npc.position_z;
            npc_info.rotation = db_npc.rotation;
            npc_info.model_id = db_npc.model_id;
            npc_info.scale = db_npc.scale;
            npc_info.functions = db_npc.functions;
            npc_info.dialogue_id = db_npc.dialogue_id;
            npc_info.shop_id = db_npc.shop_id;

            npc_infos_[npc_info.npc_id] = npc_info;

            // 按地图索引
            npcs_by_map_[npc_info.map_id].push_back(npc_info.npc_id);

            // 创建NPC对象
            CreateNPC(npc_info.npc_id, npc_info.map_id);
        }

        spdlog::info("Loaded {} NPCs from database", npc_infos_.size());

    } catch (const std::exception& e) {
        spdlog::error("Failed to load NPCs from database: {}", e.what());
    }
}

std::shared_ptr<NPC> NPCManager::CreateNPC(uint32_t npc_id, uint16_t map_id) {
    // 查找NPC信息
    auto it = npc_infos_.find(npc_id);
    if (it == npc_infos_.end()) {
        spdlog::error("NPC info not found: npc_id={}", npc_id);
        return nullptr;
    }

    const NPCInfo& info = it->second;

    // 检查地图是否匹配
    if (info.map_id != map_id) {
        spdlog::warn("NPC {} is in map {}, not map {}", npc_id, info.map_id, map_id);
    }

    // 创建NPC对象
    auto npc = std::make_shared<NPC>(next_object_id_++, info);
    npcs_by_object_id_[npc->GetObjectId()] = npc;

    spdlog::debug("Created NPC: npc_id={}, object_id={}, name={}",
                  npc_id, npc->GetObjectId(), info.name);

    return npc;
}

void NPCManager::DestroyNPC(uint64_t object_id) {
    auto it = npcs_by_object_id_.find(object_id);
    if (it != npcs_by_object_id_.end()) {
        spdlog::debug("Destroying NPC: object_id={}", object_id);
        npcs_by_object_id_.erase(it);
    }
}

std::shared_ptr<NPC> NPCManager::GetNPC(uint64_t object_id) {
    auto it = npcs_by_object_id_.find(object_id);
    if (it != npcs_by_object_id_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<NPC> NPCManager::GetNPCById(uint32_t npc_id) {
    for (const auto& pair : npcs_by_object_id_) {
        if (pair.second->GetNPCId() == npc_id) {
            return pair.second;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<NPC>> NPCManager::GetNPCsInMap(uint16_t map_id) {
    std::vector<std::shared_ptr<NPC>> npcs;

    auto it = npcs_by_map_.find(map_id);
    if (it != npcs_by_map_.end()) {
        for (uint32_t npc_id : it->second) {
            auto npc = GetNPCById(npc_id);
            if (npc) {
                npcs.push_back(npc);
            }
        }
    }

    return npcs;
}

std::vector<std::shared_ptr<NPC>> NPCManager::GetNPCsNearPosition(float x, float y, float radius) {
    std::vector<std::shared_ptr<NPC>> npcs;

    for (const auto& pair : npcs_by_object_id_) {
        const auto& npc = pair.second;

        float dx = npc->GetPositionX() - x;
        float dy = npc->GetPositionY() - y;
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance <= radius) {
            npcs.push_back(npc);
        }
    }

    return npcs;
}

const NPCInfo* NPCManager::GetNPCInfo(uint32_t npc_id) const {
    auto it = npc_infos_.find(npc_id);
    if (it != npc_infos_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<NPCInfo> NPCManager::GetAllNPCInfos() const {
    std::vector<NPCInfo> infos;
    for (const auto& pair : npc_infos_) {
        infos.push_back(pair.second);
    }
    return infos;
}

std::string NPCManager::GetDialogueText(uint32_t dialogue_id) {
    // TODO: 从数据库或文件加载对话文本
    // 目前返回默认文本
    return "你好，勇士！有什么我可以帮助你的吗？";
}

bool NPCManager::OpenShop(uint64_t player_id, uint32_t npc_id) {
    auto npc = GetNPCById(npc_id);
    if (!npc || !npc->IsShop()) {
        spdlog::error("NPC {} is not a shop", npc_id);
        return false;
    }

    spdlog::info("Player {} opening shop at NPC {}", player_id, npc_id);

    // TODO: 发送商店数据给客户端
    // TODO: 打开商店UI

    return true;
}

bool NPCManager::CloseShop(uint64_t player_id) {
    spdlog::info("Player {} closing shop", player_id);

    // TODO: 关闭商店UI

    return true;
}

} // namespace MapServer
} // namespace Murim
