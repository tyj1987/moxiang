cmake_minimum_required(VERSION 3.15)

# find_package(fmt CONFIG REQUIRED)  # Using vcpkg paths directly

# 成就系统库
add_library(murim_achievement STATIC
    AchievementManager.cpp
    AchievementHandler.cpp
)

target_compile_features(murim_achievement PUBLIC cxx_std_20)

# 设置 header-only spdlog
target_compile_definitions(murim_achievement PUBLIC SPDLOG_HEADER_ONLY)

target_include_directories(murim_achievement
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
        D:/vcpkg/installed/x64-windows/include
)

target_link_libraries(murim_achievement
    PUBLIC
        "D:/vcpkg/installed/x64-windows/lib/libprotobuf.lib"
        "D:/vcpkg/installed/x64-windows/lib/fmt.lib"
        murim_core
        murim_shared
)

# ============== 安装 ==============
install(TARGETS murim_achievement
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
