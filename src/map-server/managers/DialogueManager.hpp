// Murim MMORPG - 对话系统
// 负责NPC与玩家之间的对话功能

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace Murim {
namespace MapServer {

// 对话选项
struct DialogueOption {
    uint32_t option_id;
    std::string text;
    uint32_t next_dialogue_id;    // 选择后跳转的对话ID
    uint32_t quest_id;          // 关联的任务ID（0=无）
    uint32_t action_type;       // 动作类型: 0=继续对话, 1=接受任务, 2=打开商店, 3=传送
    std::string action_data;   // 动作参数（JSON格式）
};

// 对话节点
struct DialogueNode {
    uint32_t dialogue_id;
    std::string npc_text;      // NPC说的话
    std::vector<DialogueOption> options;  // 玩家可选的选项
    uint32_t npc_id;           // 所属NPC
    std::string voice_id;      // 语音文件ID
    std::string animation_id;  // 动画ID
};

// 对话管理器
class DialogueManager {
public:
    static DialogueManager& Instance();

    bool Initialize();
    void Shutdown();

    // 加载对话数据
    void LoadDialogues();
    void LoadDialogueFromDatabase(uint32_t dialogue_id);

    // 对话操作
    bool StartDialogue(uint64_t player_id, uint32_t npc_id);
    bool SelectOption(uint64_t player_id, uint32_t option_id);

    // 查询
    const DialogueNode* GetDialogue(uint32_t dialogue_id) const;
    DialogueNode* GetDialogue(uint32_t dialogue_id);

    // 对话处理
    bool ProcessDialogueAction(uint64_t player_id, const DialogueOption& option);

private:
    DialogueManager() = default;
    ~DialogueManager() = default;

    std::unordered_map<uint32_t, DialogueNode> dialogues_;

    // 玩家当前对话状态
    struct PlayerDialogueState {
        uint64_t player_id;
        uint32_t current_dialogue_id;
        uint32_t npc_id;
    };
    std::unordered_map<uint64_t, PlayerDialogueState> player_states_;
};

} // namespace MapServer
} // namespace Murim
