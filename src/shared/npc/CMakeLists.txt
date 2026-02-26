cmake_minimum_required(VERSION 3.15)

# NPC系统库
# find_package(spdlog CONFIG REQUIRED)  # Using header-only from vcpkg

add_library(murim_npc
    NPCManager.cpp
    NPCHandler.cpp
)

target_include_directories(murim_npc
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(murim_npc
    PUBLIC
        murim_database
        murim_core
)

target_compile_features(murim_npc PUBLIC cxx_std_17)
