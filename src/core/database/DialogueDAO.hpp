// Murim MMORPG - Dialogue DAO
// Responsible for dialogue data database access

#pragma once

#include <string>
#include <vector>
#include <optional>

namespace Murim {
namespace Core {
namespace Database {

// Dialogue option structure (corresponds to dialogue_options table)
struct DialogueOptionInfo {
    uint32_t option_id;
    uint32_t dialogue_id;
    std::string text;
    uint32_t next_dialogue_id;
    uint32_t quest_id;
    uint32_t action_type;
    std::string action_data;  // JSON format
};

// Dialogue node structure (corresponds to dialogues table)
struct DialogueInfo {
    uint32_t dialogue_id;
    std::string npc_text;
    uint32_t npc_id;
    std::string voice_id;
    std::string animation_id;
    std::vector<DialogueOptionInfo> options;
};

// Dialogue DAO class
class DialogueDAO {
public:
    // Load all dialogues
    static std::vector<DialogueInfo> LoadAll();

    // Load dialogue by ID
    static std::optional<DialogueInfo> LoadById(uint32_t dialogue_id);

    // Load dialogues by NPC ID
    static std::vector<DialogueInfo> LoadByNPC(uint32_t npc_id);

    // Create dialogue
    static bool Create(const DialogueInfo& dialogue_info);

    // Update dialogue
    static bool Update(const DialogueInfo& dialogue_info);

    // Delete dialogue
    static bool Delete(uint32_t dialogue_id);

    // Create dialogue option
    static bool CreateOption(const DialogueOptionInfo& option_info);

    // Update dialogue option
    static bool UpdateOption(const DialogueOptionInfo& option_info);

    // Delete dialogue option
    static bool DeleteOption(uint32_t option_id);
};

} // namespace Database
} // namespace Core
} // namespace Murim
