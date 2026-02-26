# find_package(spdlog CONFIG REQUIRED)  # Using header-only from vcpkg
# find_package(fmt CONFIG REQUIRED)  # Using vcpkg paths directly

add_library(murim_resource STATIC
    Resource.cpp
)

target_include_directories(murim_resource
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/src
        D:/vcpkg/installed/x64-windows/include
)

target_compile_features(murim_resource PUBLIC cxx_std_20)

# 链接 fmt（spdlog 依赖）
target_link_libraries(murim_resource
    PUBLIC
        "D:/vcpkg/installed/x64-windows/lib/fmt.lib"
        murim_core
)
