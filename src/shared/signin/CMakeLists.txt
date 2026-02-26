# Sign-in System Library
cmake_minimum_required(VERSION 3.15)

find_package(fmt CONFIG REQUIRED)

# 设置库名称
set(SIGNIN_SOURCES
    SignInManager.cpp
    SignInHandler.cpp
)

set(SIGNIN_HEADERS
    SignInManager.hpp
    SignInHandler.hpp
)

# 创建 SignIn 库
add_library(murim_signin STATIC ${SIGNIN_SOURCES} ${SIGNIN_HEADERS})

# 设置C++20标准
target_compile_features(murim_signin PUBLIC cxx_std_20)

# 设置 header-only spdlog
target_compile_definitions(murim_signin PUBLIC SPDLOG_HEADER_ONLY)

# 包含头文件目录
target_include_directories(murim_signin
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        D:/vcpkg/installed/x64-windows/include
)

# 链接依赖库
target_link_libraries(murim_signin
    PUBLIC
        murim_protocol
        fmt::fmt
        murim_core
        murim_network
)

# 安装规则
install(TARGETS murim_signin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
