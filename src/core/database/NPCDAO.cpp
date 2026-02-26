// Murim MMORPG - NPC DAO implementation

#include "NPCDAO.hpp"
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"
#include "core/spdlog_wrapper.hpp"
#include <sstream>

namespace Murim {
namespace Core {
namespace Database {

// No need for using declarations - we're already in the correct namespace

// ================================
// NPC DAO Implementation
// ================================

std::vector<NPCInfo> NPCDAO::LoadAll() {
    std::vector<NPCInfo> npcs;

    try {
        auto conn = ConnectionPool::Instance().GetConnection();
        if (!conn) {
            spdlog::error("Failed to get database connection");
            return npcs;
        }

        QueryExecutor executor(conn);

        // Query all NPCs
        std::string sql =
            "SELECT npc_id, npc_name, map_id, position_x, position_y, position_z, "
            "rotation, model_id, scale, functions, dialogue_id, shop_id "
            "FROM npcs "
            "ORDER BY npc_id";

        auto result = executor.Query(sql);
        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return npcs;
        }

        for (int i = 0; i < result->RowCount(); ++i) {
            NPCInfo info;
            info.npc_id = result->Get<uint32_t>(i, "npc_id").value_or(0);
            info.name = result->Get<std::string>(i, "npc_name").value_or("");
            info.map_id = result->Get<uint16_t>(i, "map_id").value_or(0);
            info.position_x = result->Get<float>(i, "position_x").value_or(0.0f);
            info.position_y = result->Get<float>(i, "position_y").value_or(0.0f);
            info.position_z = result->Get<float>(i, "position_z").value_or(0.0f);
            info.rotation = result->Get<float>(i, "rotation").value_or(0.0f);
            info.model_id = result->Get<std::string>(i, "model_id").value_or("");
            info.scale = result->Get<float>(i, "scale").value_or(1.0f);
            info.functions = result->Get<uint32_t>(i, "functions").value_or(0);
            info.dialogue_id = result->Get<uint32_t>(i, "dialogue_id").value_or(0);
            info.shop_id = result->Get<uint32_t>(i, "shop_id").value_or(0);

            npcs.push_back(info);
        }

        spdlog::info("Loaded {} NPCs from database", npcs.size());
        ConnectionPool::Instance().ReturnConnection(conn);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load NPCs: {}", e.what());
    }

    return npcs;
}

std::optional<NPCInfo> NPCDAO::LoadById(uint32_t npc_id) {
    try {
        auto conn = ConnectionPool::Instance().GetConnection();
        if (!conn) {
            spdlog::error("Failed to get database connection");
            return std::nullopt;
        }

        QueryExecutor executor(conn);

        std::string sql =
            "SELECT npc_id, npc_name, map_id, position_x, position_y, position_z, "
            "rotation, model_id, scale, functions, dialogue_id, shop_id "
            "FROM npcs "
            "WHERE npc_id = $1";

        auto result = executor.QueryParams(sql, {std::to_string(npc_id)});
        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return std::nullopt;
        }

        NPCInfo info;
        info.npc_id = result->Get<uint32_t>(0, "npc_id").value_or(0);
        info.name = result->Get<std::string>(0, "npc_name").value_or("");
        info.map_id = result->Get<uint16_t>(0, "map_id").value_or(0);
        info.position_x = result->Get<float>(0, "position_x").value_or(0.0f);
        info.position_y = result->Get<float>(0, "position_y").value_or(0.0f);
        info.position_z = result->Get<float>(0, "position_z").value_or(0.0f);
        info.rotation = result->Get<float>(0, "rotation").value_or(0.0f);
        info.model_id = result->Get<std::string>(0, "model_id").value_or("");
        info.scale = result->Get<float>(0, "scale").value_or(1.0f);
        info.functions = result->Get<uint32_t>(0, "functions").value_or(0);
        info.dialogue_id = result->Get<uint32_t>(0, "dialogue_id").value_or(0);
        info.shop_id = result->Get<uint32_t>(0, "shop_id").value_or(0);

        ConnectionPool::Instance().ReturnConnection(conn);
        return info;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load NPC {}: {}", npc_id, e.what());
        return std::nullopt;
    }
}

std::vector<NPCInfo> NPCDAO::LoadByMap(uint16_t map_id) {
    std::vector<NPCInfo> npcs;

    try {
        auto conn = ConnectionPool::Instance().GetConnection();
        if (!conn) {
            spdlog::error("Failed to get database connection");
            return npcs;
        }

        QueryExecutor executor(conn);

        std::string sql =
            "SELECT npc_id, npc_name, map_id, position_x, position_y, position_z, "
            "rotation, model_id, scale, functions, dialogue_id, shop_id "
            "FROM npcs "
            "WHERE map_id = $1 "
            "ORDER BY npc_id";

        auto result = executor.QueryParams(sql, {std::to_string(map_id)});
        if (!result || result->RowCount() == 0) {
            ConnectionPool::Instance().ReturnConnection(conn);
            return npcs;
        }

        for (int i = 0; i < result->RowCount(); ++i) {
            NPCInfo info;
            info.npc_id = result->Get<uint32_t>(i, "npc_id").value_or(0);
            info.name = result->Get<std::string>(i, "npc_name").value_or("");
            info.map_id = result->Get<uint16_t>(i, "map_id").value_or(0);
            info.position_x = result->Get<float>(i, "position_x").value_or(0.0f);
            info.position_y = result->Get<float>(i, "position_y").value_or(0.0f);
            info.position_z = result->Get<float>(i, "position_z").value_or(0.0f);
            info.rotation = result->Get<float>(i, "rotation").value_or(0.0f);
            info.model_id = result->Get<std::string>(i, "model_id").value_or("");
            info.scale = result->Get<float>(i, "scale").value_or(1.0f);
            info.functions = result->Get<uint32_t>(i, "functions").value_or(0);
            info.dialogue_id = result->Get<uint32_t>(i, "dialogue_id").value_or(0);
            info.shop_id = result->Get<uint32_t>(i, "shop_id").value_or(0);

            npcs.push_back(info);
        }

        spdlog::info("Loaded {} NPCs for map {}", npcs.size(), map_id);
        ConnectionPool::Instance().ReturnConnection(conn);

    } catch (const std::exception& e) {
        spdlog::error("Failed to load NPCs for map {}: {}", map_id, e.what());
    }

    return npcs;
}

bool NPCDAO::Create(const NPCInfo& npc) {
    try {
        auto conn = ConnectionPool::Instance().GetConnection();
        if (!conn) {
            spdlog::error("Failed to get database connection");
            return false;
        }

        QueryExecutor executor(conn);

        std::string sql =
            "INSERT INTO npcs (npc_id, npc_name, map_id, position_x, position_y, position_z, "
            "rotation, model_id, scale, functions, dialogue_id, shop_id) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12) "
            "ON CONFLICT (npc_id) DO UPDATE SET "
            "npc_name = EXCLUDED.npc_name, "
            "map_id = EXCLUDED.map_id, "
            "position_x = EXCLUDED.position_x, "
            "position_y = EXCLUDED.position_y, "
            "position_z = EXCLUDED.position_z, "
            "rotation = EXCLUDED.rotation, "
            "model_id = EXCLUDED.model_id, "
            "scale = EXCLUDED.scale, "
            "functions = EXCLUDED.functions, "
            "dialogue_id = EXCLUDED.dialogue_id, "
            "shop_id = EXCLUDED.shop_id";

        std::vector<std::string> params = {
            std::to_string(npc.npc_id),
            npc.name,
            std::to_string(npc.map_id),
            std::to_string(npc.position_x),
            std::to_string(npc.position_y),
            std::to_string(npc.position_z),
            std::to_string(npc.rotation),
            npc.model_id,
            std::to_string(npc.scale),
            std::to_string(npc.functions),
            std::to_string(npc.dialogue_id),
            std::to_string(npc.shop_id)
        };

        executor.ExecuteParams(sql, params);
        ConnectionPool::Instance().ReturnConnection(conn);

        spdlog::info("Created/Updated NPC {}", npc.npc_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create NPC {}: {}", npc.npc_id, e.what());
        return false;
    }
}

bool NPCDAO::Update(const NPCInfo& npc) {
    return Create(npc);  // UPSERT handled in Create
}

bool NPCDAO::Delete(uint32_t npc_id) {
    try {
        auto conn = ConnectionPool::Instance().GetConnection();
        if (!conn) {
            spdlog::error("Failed to get database connection");
            return false;
        }

        QueryExecutor executor(conn);

        std::string sql = "DELETE FROM npcs WHERE npc_id = $1";
        executor.ExecuteParams(sql, {std::to_string(npc_id)});

        ConnectionPool::Instance().ReturnConnection(conn);
        spdlog::info("Deleted NPC {}", npc_id);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to delete NPC {}: {}", npc_id, e.what());
        return false;
    }
}

} // namespace Database
} // namespace Core
} // namespace Murim
