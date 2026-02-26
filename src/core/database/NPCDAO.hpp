// Murim MMORPG - NPC DAO
// Responsible for NPC data access

#pragma once

#include <string>
#include <vector>
#include <optional>

namespace Murim {
namespace Core {
namespace Database {

// NPC Info structure (database layer)
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
    uint32_t functions;
    uint32_t dialogue_id;
    uint32_t shop_id;
};

// NPC DAO class
class NPCDAO {
public:
    // Load all NPCs
    static std::vector<NPCInfo> LoadAll();

    // Load NPCs by map ID
    static std::vector<NPCInfo> LoadByMap(uint16_t map_id);

    // Load single NPC by NPC ID
    static std::optional<NPCInfo> LoadById(uint32_t npc_id);

    // Create NPC
    static bool Create(const NPCInfo& npc_info);

    // Update NPC
    static bool Update(const NPCInfo& npc_info);

    // Delete NPC
    static bool Delete(uint32_t npc_id);
};

} // namespace Database
} // namespace Core
} // namespace Murim
