# VIP System Library
# VIP系统 - VIP等级、特权、礼包

cmake_minimum_required(VERSION 3.15)

# find_package(fmt CONFIG REQUIRED)  # Using vcpkg paths directly

# 创建静态库
add_library(murim_vip STATIC
    VIPManager.cpp
    VIPHandler.cpp
)

# 设置 C++20 标准
target_compile_features(murim_vip PUBLIC cxx_std_20)

# 设置 header-only spdlog
target_compile_definitions(murim_vip PUBLIC SPDLOG_HEADER_ONLY)

# 包含目录
target_include_directories(murim_vip PUBLIC
    ${CMAKE_SOURCE_DIR}
    D:/vcpkg/installed/x64-windows/include
)

# 链接库
target_link_libraries(murim_vip PUBLIC
    "D:/vcpkg/installed/x64-windows/lib/libprotobuf.lib"
    "D:/vcpkg/installed/x64-windows/lib/fmt.lib"
    murim_core
    murim_shared
)

# 设置输出目录
set_target_properties(murim_vip PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 编译选项
if(MSVC)
    target_compile_options(murim_vip PRIVATE /W4)
else()
    target_compile_options(murim_vip PRIVATE -Wall -Wextra -Wpedantic)
endif()
