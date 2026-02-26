# 移动系统库
# find_package(spdlog REQUIRED)

add_library(murim_movement STATIC
    MovementManager.cpp
    MovementValidator.cpp
    PositionBroadcaster.cpp
)

target_include_directories(murim_movement
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

# 链接依赖
target_link_libraries(murim_movement
    PUBLIC
        murim_database
        murim_core
        # spdlog::spdlog  # Using header-only spdlog
)

target_compile_features(murim_movement PUBLIC cxx_std_20)
