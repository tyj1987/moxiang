# Marriage System Library
cmake_minimum_required(VERSION 3.15)

find_package(fmt CONFIG REQUIRED)

set(MARRIAGE_SOURCES
    MarriageManager.cpp
    MarriageHandler.cpp
)

set(MARRIAGE_HEADERS
    MarriageManager.hpp
    MarriageHandler.hpp
)

# 创建 Marriage 库
add_library(murim_marriage STATIC ${MARRIAGE_SOURCES} ${MARRIAGE_HEADERS})

# 设置C++20标准
target_compile_features(murim_marriage PUBLIC cxx_std_20)

# 设置 header-only spdlog
target_compile_definitions(murim_marriage PUBLIC SPDLOG_HEADER_ONLY)

# 包含头文件目录
target_include_directories(murim_marriage
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        D:/vcpkg/installed/x64-windows/include
)

# 链接依赖库
target_link_libraries(murim_marriage
    PUBLIC
        murim_protocol
        fmt::fmt
        murim_core
        murim_network
)

# 安装规则
install(TARGETS murim_marriage
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
