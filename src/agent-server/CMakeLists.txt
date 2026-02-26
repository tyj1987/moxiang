# AgentServer可执行程序
cmake_minimum_required(VERSION 3.15)

set(AGENT_SERVER_SOURCES
    main/main.cpp
    managers/AgentServerManager.cpp
)

add_executable(murim_agentserver ${AGENT_SERVER_SOURCES})

# 设置C++20标准
target_compile_features(murim_agentserver PUBLIC cxx_std_20)

# 包含头文件目录
target_include_directories(murim_agentserver
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# 链接依赖
target_link_libraries(murim_agentserver
    PRIVATE
murim_network
    murim_shared
murim_database
murim_account
murim_character
murim_movement
murim_security
murim_session
)

# Windows特定设置
if(WIN32)
    target_compile_definitions(murim_agentserver PRIVATE
        _WIN32_WINNT=0x0601
    )
endif()

# 安装规则
install(TARGETS murim_agentserver
    RUNTIME DESTINATION bin
)
