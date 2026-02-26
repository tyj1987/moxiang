# Mount System Library
# 坐骑系统 - 坐骑获取、骑乘、进化、装备

cmake_minimum_required(VERSION 3.15)

# 创建静态库
add_library(murim_mount STATIC
    MountManager.cpp
    MountHandler.cpp
)

# 设置 C++17 标准
target_compile_features(murim_mount PUBLIC cxx_std_17)

# 包含目录
target_include_directories(murim_mount PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/protobuf-29.5/include
    ${CMAKE_SOURCE_DIR}/third_party/spdlog-1.14.1/include
)

# 链接库
target_link_libraries(murim_mount PUBLIC
    murim_protocol
    libprotobuf
    spdlog_header_only
)

# 设置输出目录
set_target_properties(murim_mount PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 编译选项
if(MSVC)
    target_compile_options(murim_mount PRIVATE /W4)
else()
    target_compile_options(murim_mount PRIVATE -Wall -Wextra -Wpedantic)
endif()
