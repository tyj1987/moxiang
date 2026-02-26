# 物品系统库
# find_package(spdlog REQUIRED)

add_library(murim_item STATIC
    ItemManager.cpp
)

target_include_directories(murim_item
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(murim_item
    PUBLIC
        murim_database
        murim_core
        # spdlog::spdlog  # Using header-only spdlog
)

target_compile_features(murim_item PUBLIC cxx_std_20)
