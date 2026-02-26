#include "NPCManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <fstream>
#include <sstream>

namespace Murim {
namespace Game {

NPCManager& NPCManager::Instance() {
    static NPCManager instance;
    return instance;
}

bool NPCManager::Initialize() {
    if (initialized_) {
        spdlog::warn("NPCManager already initialized");
        return true;
    }

    spdlog::info("Initializing NPCManager...");

    LoadNPCs();
    LoadDialogues();

    initialized_ = true;

    spdlog::info("NPCManager initialized: {} NPCs, {} dialogues loaded",
                 npcs_.size(), dialogues_.size());

    return true;
}

void NPCManager::Update(uint32_t delta_time) {
    // NPC系统暂时不需要每帧更新
    // 未来可以添加NPC AI、巡逻等逻辑
}

std::shared_ptr<NPCInfo> NPCManager::GetNPC(uint32_t npc_id) {
    auto it = npcs_.find(npc_id);
    if (it != npcs_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<NPCInfo>> NPCManager::GetMapNPCs(uint32_t map_id) {
    std::vector<std::shared_ptr<NPCInfo>> result;

    auto it = map_npcs_.find(map_id);
    if (it != map_npcs_.end()) {
        for (uint32_t npc_id : it->second) {
            auto npc = GetNPC(npc_id);
            if (npc) {
                result.push_back(npc);
            }
        }
    }

    return result;
}

std::shared_ptr<DialogueNode> NPCManager::GetDialogue(uint32_t dialogue_id) {
    auto it = dialogues_.find(dialogue_id);
    if (it != dialogues_.end()) {
        return it->second;
    }
    return nullptr;
}

bool NPCManager::CanInteract(uint64_t player_id, uint32_t npc_id) {
    // 检查NPC是否存在
    auto npc = GetNPC(npc_id);
    if (!npc) {
        spdlog::warn("NPC {} not found", npc_id);
        return false;
    }

    // TODO: 检查玩家是否在NPC附近
    // TODO: 检查玩家是否正在与其他NPC交互
    // TODO: 检查NPC是否处于可交互状态

    return true;
}

NPCManager::InteractResult NPCManager::HandleInteraction(uint64_t player_id, uint32_t npc_id) {
    InteractResult result;
    result.success = false;

    // 检查是否可以交互
    if (!CanInteract(player_id, npc_id)) {
        result.error_message = "Cannot interact with NPC";
        return result;
    }

    // 获取NPC信息
    auto npc = GetNPC(npc_id);
    if (!npc) {
        result.error_message = "NPC not found";
        return result;
    }

    result.npc = npc;
    result.dialogue_id = npc->default_dialogue_id;
    result.success = true;

    spdlog::info("Player {} interacted with NPC {} ({})",
                 player_id, npc_id, npc->npc_name);

    return result;
}

void NPCManager::LoadNPCs() {
    spdlog::info("Loading NPC data...");

    // TODO: 从数据库加载NPC数据
    // 现在先添加一些测试NPC

    // NPC 1: 新手村武器商人
    auto npc1 = std::make_shared<NPCInfo>();
    npc1->npc_id = 1001;
    npc1->npc_name = "铁匠张三";
    npc1->map_id = 1;
    npc1->position_x = 100.0f;
    npc1->position_y = 200.0f;
    npc1->position_z = 0.0f;
    npc1->functions = static_cast<uint32_t>(NPCFunction::kShop) |
                       static_cast<uint32_t>(NPCFunction::kRepair);
    npc1->default_dialogue_id = 1001;
    npcs_[npc1->npc_id] = npc1;
    map_npcs_[npc1->map_id].push_back(npc1->npc_id);

    // NPC 2: 新手村仓库管理员
    auto npc2 = std::make_shared<NPCInfo>();
    npc2->npc_id = 1002;
    npc2->npc_name = "仓库管理员李四";
    npc2->map_id = 1;
    npc2->position_x = 150.0f;
    npc2->position_y = 200.0f;
    npc2->position_z = 0.0f;
    npc2->functions = static_cast<uint32_t>(NPCFunction::kWarehouse);
    npc2->default_dialogue_id = 1002;
    npcs_[npc2->npc_id] = npc2;
    map_npcs_[npc2->map_id].push_back(npc2->npc_id);

    // NPC 3: 任务发布员
    auto npc3 = std::make_shared<NPCInfo>();
    npc3->npc_id = 1003;
    npc3->npc_name = "村长";
    npc3->map_id = 1;
    npc3->position_x = 200.0f;
    npc3->position_y = 200.0f;
    npc3->position_z = 0.0f;
    npc3->functions = static_cast<uint32_t>(NPCFunction::kQuest);
    npc3->default_dialogue_id = 1003;
    npcs_[npc3->npc_id] = npc3;
    map_npcs_[npc3->map_id].push_back(npc3->npc_id);

    spdlog::info("Loaded {} test NPCs", npcs_.size());
}

void NPCManager::LoadDialogues() {
    spdlog::info("Loading dialogue data...");

    // TODO: 从数据库加载对话数据
    // 现在先添加一些测试对话

    // 对话 1001: 武器商人
    auto dialogue1 = std::make_shared<DialogueNode>();
    dialogue1->dialogue_id = 1001;
    dialogue1->npc_id = 1001;
    dialogue1->npc_text = "欢迎光临！需要买武器还是修理装备？";

    DialogueNode::DialogueOption option1;
    option1.option_id = 1;
    option1.text = "我想看看武器";
    option1.next_dialogue_id = 0;
    option1.action_type = 2;  // 打开商店
    option1.action_data = "{\"shop_id\": 1001}";
    dialogue1->options.push_back(option1);

    DialogueNode::DialogueOption option2;
    option2.option_id = 2;
    option2.text = "我要修理装备";
    option2.next_dialogue_id = 0;
    option2.action_type = 0;
    option2.action_data = "";
    dialogue1->options.push_back(option2);

    dialogues_[dialogue1->dialogue_id] = dialogue1;

    // 对话 1002: 仓库管理员
    auto dialogue2 = std::make_shared<DialogueNode>();
    dialogue2->dialogue_id = 1002;
    dialogue2->npc_id = 1002;
    dialogue2->npc_text = "需要存取物品吗？";

    DialogueNode::DialogueOption option3;
    option3.option_id = 1;
    option3.text = "打开仓库";
    option3.next_dialogue_id = 0;
    option3.action_type = 0;
    option3.action_data = "";
    dialogue2->options.push_back(option3);

    dialogues_[dialogue2->dialogue_id] = dialogue2;

    // 对话 1003: 村长
    auto dialogue3 = std::make_shared<DialogueNode>();
    dialogue3->dialogue_id = 1003;
    dialogue3->npc_id = 1003;
    dialogue3->npc_text = "年轻人，我们村子最近有些麻烦，你能帮忙吗？";

    DialogueNode::DialogueOption option4;
    option4.option_id = 1;
    option4.text = "我愿意帮忙";
    option4.next_dialogue_id = 0;
    option4.quest_id = 1001;
    option4.action_type = 1;  // 接受任务
    option4.action_data = "{\"quest_id\": 1001}";
    dialogue3->options.push_back(option4);

    DialogueNode::DialogueOption option5;
    option5.option_id = 2;
    option5.text = "我还没准备好";
    option5.next_dialogue_id = 0;
    option5.action_type = 0;
    option5.action_data = "";
    dialogue3->options.push_back(option5);

    dialogues_[dialogue3->dialogue_id] = dialogue3;

    spdlog::info("Loaded {} test dialogues", dialogues_.size());
}

} // namespace Game
} // namespace Murim
