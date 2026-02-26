cmake_minimum_required(VERSION 3.15)

# 排行榜系统库
find_package(spdlog CONFIG REQUIRED)

add_library(murim_leaderboard STATIC
    LeaderboardManager.cpp
    LeaderboardHandler.cpp
)

target_include_directories(murim_leaderboard
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(murim_leaderboard
    PUBLIC
        murim_database
        murim_core
        spdlog::spdlog_header_only
)

target_compile_features(murim_leaderboard PUBLIC cxx_std_17)

# ============== 安装 ==============
install(TARGETS murim_leaderboard
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
