# find_package(spdlog CONFIG REQUIRED)  # Using header-only from vcpkg

# Pet System Library
# 宠物系统 - 宠物捕捉、养成、进化、战斗

cmake_minimum_required(VERSION 3.15)

# 创建静态库
add_library(murim_pet STATIC
    PetManager.cpp
    PetHandler.cpp
)

# 设置 C++17 标准
target_compile_features(murim_pet PUBLIC cxx_std_17)

# 包含目录
target_include_directories(murim_pet PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# 链接库
target_link_libraries(murim_pet PUBLIC
    murim_protocol
    murim_core
    murim_database
    # spdlog::spdlog_header_only  # Using header-only from vcpkg
)

# 设置输出目录
set_target_properties(murim_pet PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 编译选项
if(MSVC)
    target_compile_options(murim_pet PRIVATE /W4)
else()
    target_compile_options(murim_pet PRIVATE -Wall -Wextra -Wpedantic)
endif()
