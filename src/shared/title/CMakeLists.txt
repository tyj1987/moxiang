# Title System Library
# 称号系统 - 玩家称号获取、装备、属性加成

cmake_minimum_required(VERSION 3.15)

# 创建静态库
add_library(murim_title STATIC
    TitleManager.cpp
    TitleHandler.cpp
)

# 设置 C++17 标准
target_compile_features(murim_title PUBLIC cxx_std_17)

# 包含目录
target_include_directories(murim_title PUBLIC
    ${CMAKE_SOURCE_DIR}
    D:/vcpkg/installed/x64-windows/include
)

# 链接库
target_link_libraries(murim_title PUBLIC
    "D:/vcpkg/installed/x64-windows/lib/libprotobuf.lib"
    "D:/vcpkg/installed/x64-windows/lib/fmt.lib"
    murim_core
    murim_shared
)

# 设置输出目录
set_target_properties(murim_title PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 编译选项
if(MSVC)
    target_compile_options(murim_title PRIVATE /W4)
else()
    target_compile_options(murim_title PRIVATE -Wall -Wextra -Wpedantic)
endif()
