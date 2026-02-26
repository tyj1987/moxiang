cmake_minimum_required(VERSION 3.15)

# 商店系统库
find_package(spdlog CONFIG REQUIRED)

add_library(murim_shop STATIC
    ShopManager.cpp
    ShopHandler.cpp
)

target_include_directories(murim_shop
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party/libpq/include
)

target_link_libraries(murim_shop
    PUBLIC
        murim_database
        murim_core
)

target_compile_features(murim_shop PUBLIC cxx_std_17)
