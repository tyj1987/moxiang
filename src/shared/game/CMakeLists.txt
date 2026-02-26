# 游戏对象基类库
add_library(murim_game STATIC
    GameObject.cpp
    GameObjectManager.cpp
)

target_include_directories(murim_game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(murim_game
    PUBLIC
        murim_core
        murim_utils
)
