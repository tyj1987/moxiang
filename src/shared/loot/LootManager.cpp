#include "Loot.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>
#include <cmath>

namespace Murim {
namespace Game {

LootManager& LootManager::Instance() {
    static LootManager instance;
    return instance;
}

uint64_t LootManager::GenerateLootId() {
    return next_loot_id_.fetch_add(1);
}

uint64_t LootManager::CreateLootDrop(
    uint64_t source_id,
    float x, float y, float z,
    uint32_t map_id,
    const std::vector<LootItem>& items,
    uint64_t owner_id,
    uint32_t protect_duration,
    uint32_t duration
) {
    if (items.empty()) {
        spdlog::warn("Cannot create loot drop: no items");
        return 0;
    }

    LootDrop loot;
    loot.loot_id = GenerateLootId();
    loot.source_id = source_id;
    loot.x = x;
    loot.y = y;
    loot.z = z;
    loot.map_id = map_id;
    loot.items = items;
    loot.drop_time = std::chrono::system_clock::now();
    loot.duration = duration;
    loot.owner_id = owner_id;
    loot.protect_duration = protect_duration;

    loot_drops_[loot.loot_id] = loot;

    spdlog::info("Created loot drop: loot_id={}, source_id=({}, {}, {}), items={}, owner={}, protect={}s, duration={}s",
                 loot.loot_id, loot.x, loot.y, loot.z, items.size(), owner_id, protect_duration, duration);

    return loot.loot_id;
}

bool LootManager::RemoveLootDrop(uint64_t loot_id) {
    auto it = loot_drops_.find(loot_id);
    if (it == loot_drops_.end()) {
        return false;
    }

    spdlog::info("Removed loot drop: loot_id={}", loot_id);
    loot_drops_.erase(it);
    return true;
}

std::optional<LootDrop> LootManager::GetLootDrop(uint64_t loot_id) {
    auto it = loot_drops_.find(loot_id);
    if (it == loot_drops_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<LootDrop> LootManager::QueryNearbyLoot(
    float x, float y, float z,
    float radius,
    uint32_t map_id
) {
    std::vector<LootDrop> nearby_loot;

    float radius_sq = radius * radius;

    for (const auto& [loot_id, loot] : loot_drops_) {
        // 检查地图ID
        if (loot.map_id != map_id) {
            continue;
        }

        // 检查距离（2D）
        float dx = loot.x - x;
        float dy = loot.y - y;
        float dist_sq = dx * dx + dy * dy;

        if (dist_sq <= radius_sq) {
            nearby_loot.push_back(loot);
        }
    }

    spdlog::debug("Queried nearby loot: center=({:.1f}, {:.1f}, {:.1f}), radius={:.1f}, found={}",
                 x, y, z, radius, nearby_loot.size());

    return nearby_loot;
}

size_t LootManager::CleanupExpiredLoot() {
    size_t cleaned_count = 0;

    for (auto it = loot_drops_.begin(); it != loot_drops_.end(); ) {
        if (it->second.IsExpired()) {
            spdlog::info("Cleaning up expired loot: loot_id={}", it->first);
            it = loot_drops_.erase(it);
            ++cleaned_count;
        } else {
            ++it;
        }
    }

    if (cleaned_count > 0) {
        spdlog::info("Cleaned up {} expired loot drops", cleaned_count);
    }

    return cleaned_count;
}

} // namespace Game
} // namespace Murim
