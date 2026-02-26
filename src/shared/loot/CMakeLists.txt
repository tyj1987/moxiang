# Loot Manager
# find_package(spdlog REQUIRED)

add_library(murim_loot STATIC
    LootManager.cpp
)

target_include_directories(murim_loot
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(murim_loot
    PUBLIC
        murim_core
        # spdlog::spdlog  # Using header-only spdlog
)

target_compile_features(murim_loot PUBLIC cxx_std_20)
