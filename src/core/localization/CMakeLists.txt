# Localization library
cmake_minimum_required(VERSION 3.15)

find_package(spdlog CONFIG REQUIRED)

set(LOCALIZATION_SOURCES
    LocalizationManager.cpp
)

set(LOCALIZATION_HEADERS
    Language.hpp
    LocalizationManager.hpp
)

add_library(murim_localization STATIC ${LOCALIZATION_SOURCES})

target_compile_features(murim_localization PUBLIC cxx_std_20)

target_link_libraries(murim_localization
    PUBLIC
        spdlog::spdlog_header_only
)

target_include_directories(murim_localization
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)
