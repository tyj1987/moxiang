# 角色系统库
# find_package(spdlog REQUIRED)

add_library(murim_character STATIC
    CharacterManager.cpp
    Player.cpp
)

target_include_directories(murim_character
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(murim_character
    PUBLIC
        murim_database
        murim_core
        # spdlog::spdlog  # Using header-only spdlog
)

target_compile_features(murim_character PUBLIC cxx_std_20)
