cmake_minimum_required(VERSION 3.15)

# MurimNetServer可执行文件
set(MURIMNET_SERVER_SOURCES
    main/main.cpp
    managers/MurimNetServerManager.cpp
)

add_executable(murim_murimnetserver ${MURIMNET_SERVER_SOURCES})

# 设置C++20标准
target_compile_features(murim_murimnetserver PUBLIC cxx_std_20)

# 链接依赖库
target_link_libraries(murim_murimnetserver
    PRIVATE
    murim_network
    murim_shared
    murim_social
    murim_guild
    murim_character
    murim_movement
)

# 包含头文件目录
target_include_directories(murim_murimnetserver
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Windows特定设置
if(WIN32)
    target_compile_definitions(murim_murimnetserver PRIVATE
        _WIN32_WINNT=0x0601
    )
endif()

# 安装规则
install(TARGETS murim_murimnetserver
    RUNTIME DESTINATION bin
)
