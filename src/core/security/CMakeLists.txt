# ============== 安全层 ==============
# find_package(spdlog CONFIG REQUIRED)  # Using header-only from vcpkg

add_library(murim_security STATIC
    RateLimiter.cpp
)

target_link_libraries(murim_security
    PUBLIC
        murim_core
)

target_include_directories(murim_security
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
        D:/vcpkg/installed/x64-windows/include
)

target_compile_features(murim_security PUBLIC cxx_std_20)
