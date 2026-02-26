# AI 系统库
# find_package(spdlog REQUIRED)

add_library(murim_ai STATIC
    AIManager.cpp
)

target_include_directories(murim_ai
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(murim_ai
    PUBLIC
        murim_database
        murim_core
        # spdlog::spdlog  # Using header-only spdlog
)

target_compile_features(murim_ai PUBLIC cxx_std_20)
