// Murim MMORPG - NPC管理器
// 负责管理所有NPC的创建、销毁、对话和商店功能

#pragma once

#include <unordered_map>
#include <memory>
#include "shared/game/GameObject.hpp"
#include "../../core/spdlog_wrapper.hpp"

namespace Murim {
namespace MapServer {

// NPC功能位掩码
enum class NPCFunction : uint32_t {
    kNone = 0,
    kShop = 1 << 0,          // 商店
    kWarehouse = 1 << 1,     // 仓库
    kRepair = 1 << 2,        // 修理
    kTeleport = 1 << 3,      // 传送
    kQuest = 1 << 4,         // 任务
    kGuild = 1 << 5,         // 公会
};

// NPC数据结构
struct NPCInfo {
    uint32_t npc_id;
    std::string name;
    uint16_t map_id;
    float position_x;
    float position_y;
    float position_z;
    float rotation;
    std::string model_id;
    float scale;
    uint32_t functions;  // NPCFunction位掩码
    uint32_t dialogue_id;
    uint32_t shop_id;

    // 检查是否有某个功能
    bool HasFunction(NPCFunction func) const {
        return (functions & static_cast<uint32_t>(func)) != 0;
    }
};

// NPC对象
class NPC : public Game::GameObject {
public:
    NPC(uint64_t object_id, const NPCInfo& info);
    virtual ~NPC() = default;

    // Getter
    uint32_t GetNPCId() const { return npc_id_; }
    const std::string& GetName() const { return name_; }
    uint32_t GetDialogueId() const { return dialogue_id_; }
    uint32_t GetShopId() const { return shop_id_; }

    // 功能检查
    bool IsShop() const { return HasFunction(NPCFunction::kShop); }
    bool IsWarehouse() const { return HasFunction(NPCFunction::kWarehouse); }
    bool IsRepair() const { return HasFunction(NPCFunction::kRepair); }
    bool IsTeleport() const { return HasFunction(NPCFunction::kTeleport); }
    bool IsQuestGiver() const { return HasFunction(NPCFunction::kQuest); }
    bool IsGuild() const { return HasFunction(NPCFunction::kGuild); }

    // 交互
    bool CanInteract(uint64_t player_id, float distance) const;
    void Interact(uint64_t player_id);

    // GameObject 纯虚函数实现
    void OnRevive() override;
    void OnDamaged(uint32_t damage, uint64_t attacker_id) override;
    void OnHealed(uint32_t amount, uint64_t healer_id) override;
    void RemoveMP(uint32_t amount) override;
    void OnStateApplied(uint32_t state_id) override;
    void OnStateRemoved(uint32_t state_id) override;
    void OnDie() override;
    std::string ToString() const override;

private:
    uint32_t npc_id_;
    std::string name_;
    uint32_t dialogue_id_;
    uint32_t shop_id_;
    uint32_t functions_;
    std::string model_id_;
    float scale_;

    bool HasFunction(NPCFunction func) const {
        return (functions_ & static_cast<uint32_t>(func)) != 0;
    }
};

// NPC管理器
class NPCManager {
public:
    static NPCManager& Instance();

    bool Initialize();
    void Shutdown();

    // 加载NPC数据
    void LoadNPCs();
    void LoadNPCsFromDatabase();

    // 创建/销毁NPC
    std::shared_ptr<NPC> CreateNPC(uint32_t npc_id, uint16_t map_id);
    void DestroyNPC(uint64_t object_id);

    // 查询
    std::shared_ptr<NPC> GetNPC(uint64_t object_id);
    std::shared_ptr<NPC> GetNPCById(uint32_t npc_id);
    std::vector<std::shared_ptr<NPC>> GetNPCsInMap(uint16_t map_id);
    std::vector<std::shared_ptr<NPC>> GetNPCsNearPosition(float x, float y, float radius);

    // NPC信息
    const NPCInfo* GetNPCInfo(uint32_t npc_id) const;
    std::vector<NPCInfo> GetAllNPCInfos() const;

    // 对话
    std::string GetDialogueText(uint32_t dialogue_id);

    // 商店
    bool OpenShop(uint64_t player_id, uint32_t npc_id);
    bool CloseShop(uint64_t player_id);

private:
    NPCManager() = default;
    ~NPCManager() = default;

    std::unordered_map<uint64_t, std::shared_ptr<NPC>> npcs_by_object_id_;
    std::unordered_map<uint32_t, NPCInfo> npc_infos_;
    std::unordered_map<uint16_t, std::vector<uint32_t>> npcs_by_map_;  // map_id -> npc_ids

    uint64_t next_object_id_ = 100000;  // NPC对象ID起始值
};

} // namespace MapServer
} // namespace Murim
