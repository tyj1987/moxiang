# 战斗系统库
# nlohmann_json and spdlog are provided by FetchContent in root CMakeLists.txt

add_library(murim_battle STATIC
    AttackCalculator.cpp
    BattleManager.cpp
    BuffManager.cpp
    DamageCalculator.cpp
    PlayerxMonsterPointTable.cpp
)

target_include_directories(murim_battle
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

# 链接依赖（移除 nlohmann_json 依赖，使用简单 JSON 处理或自定义方案）
target_link_libraries(murim_battle
    PUBLIC
        murim_database
        murim_skill
        # nlohmann_json::nlohmann_json  # 暂时移除，后续可添加轻量级 JSON 库
        # spdlog::spdlog  # Using mock_spdlog.hpp
)

target_compile_features(murim_battle PUBLIC cxx_std_20)

# ============== 安装 ==============
install(TARGETS murim_battle
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
