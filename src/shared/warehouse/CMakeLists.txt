cmake_minimum_required(VERSION 3.15)

# 仓库系统库
# find_package(spdlog CONFIG REQUIRED)  # Using header-only from vcpkg

add_library(murim_warehouse STATIC
    WarehouseManager.cpp
    WarehouseHandler.cpp
)

target_include_directories(murim_warehouse
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party/libpq/include
)

target_link_libraries(murim_warehouse
    PUBLIC
        murim_database
        murim_core
)

target_compile_features(murim_warehouse PUBLIC cxx_std_17)
