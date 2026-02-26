cmake_minimum_required(VERSION 3.15)

# DistributeServer可执行文件
set(DISTRIBUTE_SERVER_SOURCES
    main/main.cpp
    managers/DistributeServerManager.cpp
)

add_executable(murim_distributeserver ${DISTRIBUTE_SERVER_SOURCES})

# 设置C++20标准
target_compile_features(murim_distributeserver PUBLIC cxx_std_20)

# 链接依赖库
target_link_libraries(murim_distributeserver
    PRIVATE
    murim_network
    murim_shared
)

# 包含头文件目录
target_include_directories(murim_distributeserver
    PRIVATE
    ${CMAKE_SOURCE_DIR}
    D:/vcpkg/installed/x64-windows/include
)

# Windows特定设置
if(WIN32)
    target_compile_definitions(murim_distributeserver PRIVATE
        _WIN32_WINNT=0x0601
    )
endif()

# 安装规则
install(TARGETS murim_distributeserver
    RUNTIME DESTINATION bin
)
