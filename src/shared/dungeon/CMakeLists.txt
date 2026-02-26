# Dungeon System Library
cmake_minimum_required(VERSION 3.15)

find_package(fmt CONFIG REQUIRED)

# 设置库名称
set(DUNGEON_SOURCES
    DungeonManager.cpp
    # TODO: DungeonHandler.cpp - 需要先添加缺失的 protobuf 消息定义
    # DungeonHandler.cpp
)

set(DUNGEON_HEADERS
    DungeonManager.hpp
    DungeonHandler.hpp
)

# 创建 Dungeon 库
add_library(murim_dungeon STATIC ${DUNGEON_SOURCES} ${DUNGEON_HEADERS})

# 设置C++20标准
target_compile_features(murim_dungeon PUBLIC cxx_std_20)

# 设置 header-only spdlog
target_compile_definitions(murim_dungeon PUBLIC SPDLOG_HEADER_ONLY)

# 包含头文件目录
target_include_directories(murim_dungeon
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        D:/vcpkg/installed/x64-windows/include
)

# 链接依赖库
target_link_libraries(murim_dungeon
    PUBLIC
        murim_protocol
        fmt::fmt
        murim_core
        murim_network
)

# 安装规则
install(TARGETS murim_dungeon
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
