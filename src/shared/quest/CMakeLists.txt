# 任务系统库
# find_package(spdlog REQUIRED)

add_library(murim_quest STATIC
    QuestManager.cpp
)

target_include_directories(murim_quest
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(murim_quest
    PUBLIC
        murim_database
        murim_core
        # spdlog::spdlog  # Using header-only spdlog
)

target_compile_features(murim_quest PUBLIC cxx_std_20)
