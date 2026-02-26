# ============== 共享游戏逻辑库 ==============
add_library(murim_shared INTERFACE)

target_include_directories(murim_shared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============== Utils ==============
add_subdirectory(utils)

# ============== Game Object ==============
add_subdirectory(game)

# ============== Account Manager ==============
add_subdirectory(account)

# ============== Character Manager ==============
add_subdirectory(character)

# ============== Skill Manager ==============
add_subdirectory(skill)

# Skill Manager 源文件
set(SKILL_MANAGER_SOURCES
    SkillManager.cpp
    )

# ============== Battle Manager ==============
add_subdirectory(battle)

# ============== Loot Manager ==============
add_subdirectory(loot)

# ============== Movement Manager ==============
add_subdirectory(movement)

# Movement Manager 源文件
set(MOVEMENT_MANAGER_SOURCES
    MovementManager.cpp
)

# ============== NPC Manager ==============
add_subdirectory(npc)

# ============== Item Manager ==============
add_subdirectory(item)

# ============== Shop Manager ==============
add_subdirectory(shop)

# ============== Warehouse Manager ==============
add_subdirectory(warehouse)

# ============== Quest Manager ==============
add_subdirectory(quest)

# ============== AI Manager ==============
add_subdirectory(ai)

# ============== Social Manager ==============
add_subdirectory(social)

# ============== Guild Manager ==============
add_subdirectory(guild)

# ============== Mail Manager ==============
add_subdirectory(mail)

# ============== Market Manager ==============
add_subdirectory(market)

# ============== Trade Manager ==============
add_subdirectory(trade)

# ============== Leaderboard Manager ==============
add_subdirectory(leaderboard)

# ============== Achievement Manager ==============
add_subdirectory(achievement)

# ============== Title Manager ==============
add_subdirectory(title)

# ============== Pet Manager ==============
# TODO: 暂时注释，API 不匹配需要重构
# add_subdirectory(pet)

# ============== Mount Manager ==============
# TODO: 暂时注释，需要修复网络 API
# add_subdirectory(mount)

# ============== VIP Manager ==============
add_subdirectory(vip)

# ============== Dungeon Manager ==============
add_subdirectory(dungeon)

# ============== Sign-in Manager ==============
add_subdirectory(signin)

# ============== Marriage Manager ==============
add_subdirectory(marriage)
