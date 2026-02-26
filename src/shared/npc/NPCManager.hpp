#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>
#include "../game/GameObject.hpp"

namespace Murim {
namespace Game {

/**
 * @brief NPC功能类型
 */
enum class NPCFunction : uint32_t {
    kNone = 0,
    kShop = 1,          // 商店
    kWarehouse = 2,     // 仓库
    kRepair = 4,        // 修理
    kTeleport = 8,      // 传送
    kQuest = 16,        // 任务
    kGuild = 32         // 公会
};

/**
 * @brief NPC信息
 */
struct NPCInfo {
    uint32_t npc_id;
    std::string npc_name;
    uint32_t map_id;
    float position_x;
    float position_y;
    float position_z;
    uint32_t functions;           // 功能位掩码
    uint32_t default_dialogue_id; // 默认对话ID

    /**
     * @brief 检查是否有指定功能
     */
    bool HasFunction(NPCFunction func) const {
        return (functions & static_cast<uint32_t>(func)) != 0;
    }
};

/**
 * @brief NPC对话节点
 */
struct DialogueNode {
    uint32_t dialogue_id;
    uint32_t npc_id;
    std::string npc_text;
    std::string voice_id;
    std::string animation_id;

    struct DialogueOption {
        uint32_t option_id;
        std::string text;
        uint32_t next_dialogue_id;
        uint32_t quest_id;
        uint32_t action_type;       // 0:继续, 1:接受任务, 2:打开商店, 3:传送
        std::string action_data;    // JSON格式的动作参数
    };

    std::vector<DialogueOption> options;
};

/**
 * @brief NPC管理器
 *
 * 职责：
 * - 加载NPC数据
 * - 管理NPC实例
 * - 处理NPC交互请求
 * - 提供对话数据
 */
class NPCManager {
public:
    /**
     * @brief 获取单例实例
     */
    static NPCManager& Instance();

    /**
     * @brief 初始化NPC管理器
     */
    bool Initialize();

    /**
     * @brief 更新NPC系统
     */
    void Update(uint32_t delta_time);

    /**
     * @brief 获取NPC信息
     */
    std::shared_ptr<NPCInfo> GetNPC(uint32_t npc_id);

    /**
     * @brief 获取指定地图的所有NPC
     */
    std::vector<std::shared_ptr<NPCInfo>> GetMapNPCs(uint32_t map_id);

    /**
     * @brief 获取对话节点
     */
    std::shared_ptr<DialogueNode> GetDialogue(uint32_t dialogue_id);

    /**
     * @brief 检查玩家是否可以与NPC交互
     * @param player_id 玩家ID
     * @param npc_id NPC ID
     * @return 是否可以交互
     */
    bool CanInteract(uint64_t player_id, uint32_t npc_id);

    /**
     * @brief 处理NPC交互请求
     */
    struct InteractResult {
        bool success;
        std::shared_ptr<NPCInfo> npc;
        uint32_t dialogue_id;
        std::string error_message;
    };

    InteractResult HandleInteraction(uint64_t player_id, uint32_t npc_id);

private:
    NPCManager() = default;
    ~NPCManager() = default;
    NPCManager(const NPCManager&) = delete;
    NPCManager& operator=(const NPCManager&) = delete;

    /**
     * @brief 加载NPC数据
     */
    void LoadNPCs();

    /**
     * @brief 加载对话数据
     */
    void LoadDialogues();

    // ========== 数据存储 ==========

    /**
     * @brief NPC数据缓存 (npc_id -> NPCInfo)
     */
    std::unordered_map<uint32_t, std::shared_ptr<NPCInfo>> npcs_;

    /**
     * @brief 地图NPC索引 (map_id -> vector of npc_ids)
     */
    std::unordered_map<uint32_t, std::vector<uint32_t>> map_npcs_;

    /**
     * @brief 对话数据缓存 (dialogue_id -> DialogueNode)
     */
    std::unordered_map<uint32_t, std::shared_ptr<DialogueNode>> dialogues_;

    /**
     * @brief 是否已初始化
     */
    bool initialized_ = false;
};

} // namespace Game
} // namespace Murim
