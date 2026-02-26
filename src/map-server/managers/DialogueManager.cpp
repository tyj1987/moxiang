// Murim MMORPG - 对话系统实现

#include "DialogueManager.hpp"
#include "NPCManager.hpp"
#include "ShopManager.hpp"
#include "core/database/DialogueDAO.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <nlohmann/json.hpp>

namespace Murim {
namespace MapServer {

using json = nlohmann::json;

DialogueManager& DialogueManager::Instance() {
    static DialogueManager instance;
    return instance;
}

bool DialogueManager::Initialize() {
    spdlog::info("Initializing DialogueManager...");

    LoadDialogues();

    spdlog::info("DialogueManager initialized: {} dialogues loaded", dialogues_.size());
    return true;
}

void DialogueManager::Shutdown() {
    spdlog::info("Shutting down DialogueManager...");
    dialogues_.clear();
    player_states_.clear();
}

void DialogueManager::LoadDialogues() {
    spdlog::info("Loading dialogues from database...");

    try {
        // Use DAO to load all dialogues
        auto dialogue_infos = Murim::Core::Database::DialogueDAO::LoadAll();

        for (const auto& info : dialogue_infos) {
            DialogueNode node;
            node.dialogue_id = info.dialogue_id;
            node.npc_text = info.npc_text;
            node.npc_id = info.npc_id;
            node.voice_id = info.voice_id;
            node.animation_id = info.animation_id;

            // Convert options
            for (const auto& option_info : info.options) {
                DialogueOption option;
                option.option_id = option_info.option_id;
                option.text = option_info.text;
                option.next_dialogue_id = option_info.next_dialogue_id;
                option.quest_id = option_info.quest_id;
                option.action_type = option_info.action_type;
                option.action_data = option_info.action_data;

                node.options.push_back(option);
            }

            dialogues_[node.dialogue_id] = node;
        }

        spdlog::info("Loaded {} dialogues from database", dialogues_.size());

    } catch (const std::exception& e) {
        spdlog::error("Failed to load dialogues: {}", e.what());
    }
}

bool DialogueManager::StartDialogue(uint64_t player_id, uint32_t npc_id) {
    spdlog::info("Player {} starting dialogue with NPC {}", player_id, npc_id);

    // 获取NPC的默认对话ID
    // TODO: 从NPC数据中读取dialogue_id
    uint32_t dialogue_id = 1;  // 默认对话

    // 检查对话是否存在
    auto dialogue = GetDialogue(dialogue_id);
    if (!dialogue) {
        spdlog::error("Dialogue {} not found", dialogue_id);
        return false;
    }

    // 记录玩家对话状态
    PlayerDialogueState state;
    state.player_id = player_id;
    state.current_dialogue_id = dialogue_id;
    state.npc_id = npc_id;
    player_states_[player_id] = state;

    // TODO: 发送网络消息给客户端，显示对话UI
    spdlog::info("Sent dialogue {} to player {}", dialogue_id, player_id);

    return true;
}

bool DialogueManager::SelectOption(uint64_t player_id, uint32_t option_id) {
    auto state_it = player_states_.find(player_id);
    if (state_it == player_states_.end()) {
        spdlog::error("Player {} is not in dialogue", player_id);
        return false;
    }

    PlayerDialogueState& state = state_it->second;
    auto dialogue = GetDialogue(state.current_dialogue_id);
    if (!dialogue) {
        spdlog::error("Dialogue {} not found", state.current_dialogue_id);
        return false;
    }

    // 查找选项
    const DialogueOption* selected_option = nullptr;
    for (const auto& option : dialogue->options) {
        if (option.option_id == option_id) {
            selected_option = &option;
            break;
        }
    }

    if (!selected_option) {
        spdlog::error("Invalid option {} for dialogue {}", option_id, state.current_dialogue_id);
        return false;
    }

    spdlog::info("Player {} selected option {}: {}", player_id, option_id, selected_option->text);

    // 处理对话动作
    return ProcessDialogueAction(player_id, *selected_option);
}

bool DialogueManager::ProcessDialogueAction(uint64_t player_id, const DialogueOption& option) {
    switch (option.action_type) {
        case 0:  // 继续对话
            {
                if (option.next_dialogue_id != 0) {
                    // 跳转到下一个对话
                    auto state_it = player_states_.find(player_id);
                    if (state_it != player_states_.end()) {
                        state_it->second.current_dialogue_id = option.next_dialogue_id;

                        auto next_dialogue = GetDialogue(option.next_dialogue_id);
                        if (next_dialogue) {
                            // TODO: 发送新对话给客户端
                            spdlog::info("Player {} advanced to dialogue {}", player_id, option.next_dialogue_id);
                        }
                    }
                } else {
                    // 结束对话
                    player_states_.erase(player_id);
                    spdlog::info("Player {} ended dialogue", player_id);
                }
            }
            break;

        case 1:  // 接受任务
            {
                if (option.quest_id != 0) {
                    // TODO: 调用QuestManager接受任务
                    spdlog::info("Player {} accepted quest {}", player_id, option.quest_id);
                }
            }
            break;

        case 2:  // 打开商店
            {
                // 获取玩家对话状态以获取 NPC ID
                auto state_it = player_states_.find(player_id);
                if (state_it != player_states_.end()) {
                    // 获取NPC对应的商店
                    auto& shop_manager = ShopManager::Instance();
                    auto shop_ids = shop_manager.GetShopIdsByNPC(state_it->second.npc_id);
                    if (!shop_ids.empty()) {
                        shop_manager.OpenShop(player_id, shop_ids[0]);
                        spdlog::info("Opened shop {} for player {}", shop_ids[0], player_id);
                    }
                }
            }
            break;

        case 3:  // 传送
            {
                // TODO: 解析action_data（JSON格式），执行传送
                spdlog::info("Player {} teleporting (data: {})", player_id, option.action_data);
            }
            break;

        default:
            spdlog::warn("Unknown action type: {}", option.action_type);
            return false;
    }

    return true;
}

const DialogueNode* DialogueManager::GetDialogue(uint32_t dialogue_id) const {
    auto it = dialogues_.find(dialogue_id);
    if (it != dialogues_.end()) {
        return &it->second;
    }
    return nullptr;
}

DialogueNode* DialogueManager::GetDialogue(uint32_t dialogue_id) {
    auto it = dialogues_.find(dialogue_id);
    if (it != dialogues_.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace MapServer
} // namespace Murim
