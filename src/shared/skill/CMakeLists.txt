# 技能系统库
# spdlog is used in header-only mode (SPDLOG_HEADER_ONLY)
# No need to find_package for header-only usage

add_library(murim_skill STATIC
    SkillManager.cpp
)

target_include_directories(murim_skill
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(murim_skill
    PUBLIC
        murim_database
        murim_core
)

# Use header-only spdlog to avoid linker errors
target_compile_definitions(murim_skill PRIVATE SPDLOG_HEADER_ONLY)

target_compile_features(murim_skill PUBLIC cxx_std_20)
